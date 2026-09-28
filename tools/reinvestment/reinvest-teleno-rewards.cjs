#!/usr/bin/env node
'use strict';

const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');
const { spawn, execFileSync } = require('node:child_process');
const CHAIN = 'EiBZK_GGVP0H_fXVAM3j6EAuz3-B-l3ejxRSewi7qIBfSA==';
const KOIN = '19GYjDBVXU7keLbYvMLazsGQn3GTWHjHkK';
const POB = '159myq5YUhhoVWu3wsHKHiJYKPKGUrGiyv';
const SCALE = 100000000n;

function units(value) {
  if (!/^\d+(\.\d{1,8})?$/.test(value || '')) throw new Error('Use a nonnegative decimal with at most 8 decimals');
  const [whole, fraction = ''] = value.split('.');
  return BigInt(whole) * SCALE + BigInt(fraction.padEnd(8, '0'));
}
function decimal(value) {
  return `${value / SCALE}.${(value % SCALE).toString().padStart(8, '0')}`;
}
function budget(rewards, burned, balance, reserve, cap) {
  const due = rewards * 95n / 100n - burned;
  return [due, balance - reserve, cap].reduce((a, b) => a < b ? a : b) > 0n
    ? [due, balance - reserve, cap].reduce((a, b) => a < b ? a : b) : 0n;
}
// Current kcli parses --amount through Number; fail closed unless it preserves atomic units.
function cliExact(value) {
  for (let i = 0; i < 100 && value > 0n; i++, value--) {
    if (BigInt(Math.floor(Number(decimal(value)) * 1e8)) === value) return value;
  }
  throw new Error('Cannot represent this burn exactly in the installed kcli');
}
function args(argv) {
  const out = {};
  for (let i = 0; i < argv.length; i++) {
    const key = argv[i];
    if (['--execute', '--init', '--help', '--observe-only'].includes(key)) out[key.slice(2)] = true;
    else if (['--account', '--reserve', '--cap', '--min', '--start-height', '--max-blocks', '--state', '--rpc', '--kcli', '--password-file', '--keychain-helper', '--kcli-root'].includes(key)) {
      if (!argv[i + 1] || argv[i + 1].startsWith('--')) throw new Error(`Missing value: ${key}`);
      out[key.slice(2)] = argv[++i];
    } else throw new Error(`Unknown argument: ${key}`);
  }
  return out;
}
function protectedFile(file) {
  const stat = fs.lstatSync(file);
  if (!stat.isFile() || stat.uid !== process.getuid() || (stat.mode & 0o077)) {
    throw new Error(`Expected an owned regular private file (0600): ${file}`);
  }
}
function save(file, state) {
  const tmp = `${file}.tmp`;
  const fd = fs.openSync(tmp, fs.constants.O_WRONLY | fs.constants.O_CREAT | fs.constants.O_EXCL, 0o600);
  try { fs.writeFileSync(fd, JSON.stringify(state, null, 2) + '\n'); fs.fsyncSync(fd); }
  finally { fs.closeSync(fd); }
  fs.renameSync(tmp, file);
  const dir = fs.openSync(path.dirname(file), 'r');
  try { fs.fsyncSync(dir); } finally { fs.closeSync(dir); }
}
async function main(argv) {
  const opt = args(argv);
  if (opt['password-file'] && opt['keychain-helper']) throw new Error('Choose password-file or keychain-helper, not both');
  if (opt.execute && opt['observe-only']) throw new Error('Observation cannot execute');
  if (opt.help) {
    console.log('All operations require --account ADDRESS.\nInitialize: --init [--start-height N]\nPreview: --reserve KOIN --cap KOIN\nBurn: same options plus --execute and --password-file PATH or --keychain-helper PATH\nSee docs/operations/reward-reinvestment-macos.md');
    return;
  }
  const ACCOUNT = opt.account;
  if (!ACCOUNT) throw new Error('Explicit --account is required');
  const root = opt['kcli-root'] || path.join(os.homedir(), 'code/kcli');
  const { Provider, Contract, Serializer, utils } = require(path.join(root, 'node_modules/koilib'));
  const decoded = utils.decodeBase58(ACCOUNT);
  const crypto = require('node:crypto');
  const checksum = crypto.createHash('sha256').update(crypto.createHash('sha256').update(decoded.slice(0, 21)).digest()).digest().subarray(0, 4);
  if (decoded.length !== 25 || decoded[0] !== 0 || !Buffer.from(decoded.slice(21)).equals(checksum)) throw new Error('Invalid producer address');
  const provider = new Provider([opt.rpc || 'https://api.koinos.io']);
  const file = path.resolve(opt.state || path.join(os.homedir(), '.kcli/teleno-reinvest/state.json'));
  fs.mkdirSync(path.dirname(file), { recursive: true, mode: 0o700 });
  const dir = fs.lstatSync(path.dirname(file));
  if (!dir.isDirectory() || dir.uid !== process.getuid() || (dir.mode & 0o077)) throw new Error('State directory must be private (0700)');
  const lock = `${file}.lock`;
  const lockFd = fs.openSync(lock, fs.constants.O_WRONLY | fs.constants.O_CREAT | fs.constants.O_EXCL, 0o600);
  fs.writeFileSync(lockFd, `${process.pid}\n`);
  try {
    if (await provider.getChainId() !== CHAIN) throw new Error('Wrong chain: mainnet required');
    const head = await provider.getHeadInfo();
    const lib = Number(head.last_irreversible_block);
    if (opt.init) {
      if (opt.execute || fs.existsSync(file)) throw new Error('Initialization cannot execute or overwrite state');
      const start = opt['start-height'] ? Number(opt['start-height']) : lib + 1;
      if (!Number.isSafeInteger(start) || start < 1 || start > lib + 1) throw new Error('Invalid start height');
      const previous = await provider.getBlock(start - 1);
      save(file, { version: 1, account: ACCOUNT, chain: CHAIN, cursor: start - 1, blockId: previous.block_id,
        rewards: '0', burned: '0', pending: null, history: [] });
      console.log(`Initialized for ${ACCOUNT}; rewards counted from height ${start}. No burn.`);
      return;
    }
    if (!fs.existsSync(file)) throw new Error('Initialize state explicitly with --init first');
    protectedFile(file);
    const state = JSON.parse(fs.readFileSync(file, 'utf8'));
    if (state.version !== 1 || state.account !== ACCOUNT || state.chain !== CHAIN) throw new Error('Wrong state identity/version');
    if (state.cursor > lib || (await provider.getBlock(state.cursor)).block_id !== state.blockId) throw new Error('RPC finality/checkpoint mismatch');
    const mint = new Serializer({ nested: { reward: { fields: {
      to: { type: 'bytes', id: 1, options: { '(koinos.btype)': 'ADDRESS' } },
      value: { type: 'uint64', id: 2, options: { jstype: 'JS_STRING' } }
    } } } });
    const pobAbi = JSON.parse(fs.readFileSync(path.join(root, 'src/abis/pob.json'), 'utf8'));
    const pobSerializer = new Serializer(pobAbi.koilib_types);
    const max = Number(opt['max-blocks'] || 1000);
    if (!Number.isSafeInteger(max) || max < 1 || max > 10000) throw new Error('max-blocks must be 1..10000');
    const end = Math.min(lib, state.cursor + max);
    for (let height = state.cursor + 1; height <= end; height++) {
      const b = await provider.getBlock(height);
      if (!b?.receipt || !b.block || Number(b.block_height) !== height || b.receipt.id !== b.block_id || b.block.header.previous !== state.blockId) {
        throw new Error(`Missing/inconsistent block or receipt at ${height}`);
      }
      if (b.block.header.signer === ACCOUNT) {
        let found = 0;
        for (const event of b.receipt.events || []) {
          if (event.source !== KOIN || event.name !== 'token.mint_event') continue;
          const reward = await mint.deserialize(event.data, 'reward');
          if (reward.to === ACCOUNT) { state.rewards = (BigInt(state.rewards) + BigInt(reward.value)).toString(); found++; }
        }
        if (found !== 1) throw new Error(`Expected one producer reward at ${height}, found ${found}`);
      }
      if (state.pending?.txid) {
        const tx = (b.block.transactions || []).find(t => t.id === state.pending.txid);
        if (tx) {
          const receipt = (b.receipt.transaction_receipts || []).find(r => r.id === tx.id);
          if (!receipt || receipt.reverted) throw new Error('Pending transaction missing receipt or reverted; manual review required');
          const burns = (tx.operations || []).filter(o => o.call_contract?.contract_id === POB && o.call_contract.entry_point === 2241834181);
          if (burns.length !== 1) throw new Error('Unexpected pending transaction operations');
          const burn = await pobSerializer.deserialize(burns[0].call_contract.args, 'pob.burn_arguments');
          if (burn.burn_address !== ACCOUNT || burn.vhp_address !== ACCOUNT || burn.token_amount !== state.pending.amount || tx.header.payer !== ACCOUNT) throw new Error('Pending burn identity/amount mismatch');
          state.burned = (BigInt(state.burned) + BigInt(burn.token_amount)).toString();
          state.history.push({ ...state.pending, height, blockId: b.block_id });
          state.pending = null;
          console.log(`Burn verified irreversible at ${height}: ${decimal(BigInt(burn.token_amount))} KOIN`);
        }
      }
      state.cursor = height; state.blockId = b.block_id;
      save(file, state);
    }
    if (state.pending) throw new Error(`Unresolved burn attempt (${state.pending.txid || 'transaction ID unknown'}); no automatic resubmission. Review state and chain.`);
    if (state.cursor < lib) { console.log('Scan incomplete; run again. No burn until caught up.'); return; }
    if (opt['observe-only']) {
      console.log(JSON.stringify({ account: ACCOUNT, irreversibleThrough: state.cursor,
        rewards: decimal(BigInt(state.rewards)), burned: decimal(BigInt(state.burned)), mode: 'observe-only' }));
      return;
    }
    if (!opt.reserve || !opt.cap) throw new Error('Set explicit --reserve and --cap (KOIN)');
    const reserve = units(opt.reserve), cap = units(opt.cap), minimum = units(opt.min || '1');
    if (reserve <= 0n || cap <= 0n || minimum <= 0n) throw new Error('Reserve, cap and minimum must be positive');
    const koin = new Contract({ id: KOIN, abi: utils.tokenAbi, provider });
    const { result } = await koin.functions.balance_of({ owner: ACCOUNT });
    if (!result?.value) throw new Error('Missing balance response');
    const balance = BigInt(result.value);
    const allowed = budget(BigInt(state.rewards), BigInt(state.burned), balance, reserve, cap);
    const amount = allowed > 0n ? cliExact(allowed) : 0n;
    console.log(JSON.stringify({ account: ACCOUNT, irreversibleThrough: state.cursor, rewards: decimal(BigInt(state.rewards)), burned: decimal(BigInt(state.burned)), balance: decimal(balance), reserve: decimal(reserve), proposedBurn: decimal(amount), execute: !!opt.execute }, null, 2));
    if (!allowed || amount < minimum) { console.log('Below minimum; no burn.'); return; }
    if (!opt.execute) { console.log('Preview only; no transaction signed or submitted.'); return; }
    if (!opt['password-file'] && !opt['keychain-helper']) throw new Error('Execution requires password-file or keychain-helper');
    if (opt['password-file']) protectedFile(path.resolve(opt['password-file']));
    const walletFile = path.join(os.homedir(), '.kcli/wallet.json');
    protectedFile(walletFile);
    if (JSON.parse(fs.readFileSync(walletFile, 'utf8')).address !== ACCOUNT) throw new Error('kcli wallet address mismatch');
    if (BigInt(await provider.getAccountRc(ACCOUNT)) < 50000000n) throw new Error('Insufficient available Mana; try later');
    let secretDir;
    let passwordPath = opt['password-file'] && path.resolve(opt['password-file']);
    try {
    if (opt['keychain-helper']) {
      // kcli currently requires a regular password file. Keep the bridge private and transient.
      const password = execFileSync(path.resolve(opt['keychain-helper']), ['read', ACCOUNT], { timeout: 15000, maxBuffer: 16384, stdio: ['ignore', 'pipe', 'ignore'] });
      try {
        if (!password.length) throw new Error('Keychain returned an empty password');
        secretDir = fs.mkdtempSync(path.join(os.tmpdir(), 'koinos-reinvest-secret-'));
        fs.chmodSync(secretDir, 0o700);
        passwordPath = path.join(secretDir, 'password');
        fs.writeFileSync(passwordPath, password, { mode: 0o600, flag: 'wx' });
      } finally { password.fill(0); }
    }
    state.pending = { amount: amount.toString(), txid: null, createdAt: new Date().toISOString(), scanFrom: state.cursor + 1 };
    save(file, state);
    const cli = opt.kcli || 'kcli';
    const cmd = ['-n', 'mainnet', '-r', opt.rpc || 'https://api.koinos.io', 'burn', '--amount', decimal(amount), '--password-file', passwordPath, '--yes'];
    console.log('Burn attempt recorded before invoking kcli. Next run verifies finality.');
    await new Promise((resolve, reject) => {
      const child = spawn(cli, cmd, { stdio: ['ignore', 'pipe', 'pipe'] });
      let output = '';
      const timeout = setTimeout(() => child.kill('SIGTERM'), 120000);
      child.stdout.on('data', chunk => {
        output = (output + chunk.toString()).slice(-32768);
        const match = output.match(/Transaction ID:\s*(0x[0-9a-f]{68})/i);
        if (match && !state.pending.txid) { state.pending.txid = match[1]; save(file, state); }
      });
      // Do not echo wallet/CLI output into unattended logs.
      child.stderr.on('data', () => {});
      child.on('error', error => { clearTimeout(timeout); reject(error); });
      child.on('close', code => {
        clearTimeout(timeout);
        console.log(`kcli exit=${code}; transaction=${state.pending.txid || 'unknown'}. Pending until irreversible verification.`);
        if (code !== 0 || !state.pending.txid) reject(new Error('Ambiguous/failed kcli attempt; manual review required, never retry blindly'));
        else resolve();
      });
    });
    } finally {
      if (secretDir) {
        fs.unlinkSync(path.join(secretDir, 'password'));
        fs.rmdirSync(secretDir);
      }
    }
  } finally { fs.closeSync(lockFd); fs.unlinkSync(lock); }
}
module.exports = { units, decimal, budget, cliExact, args };
if (require.main === module) main(process.argv.slice(2)).catch(error => { console.error(error.message); process.exitCode = 1; });
