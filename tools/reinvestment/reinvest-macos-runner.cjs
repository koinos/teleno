'use strict';
const fs = require('node:fs');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const configPath = process.argv[2];
if (!configPath) throw new Error('Expected configuration path');
const stat = fs.lstatSync(configPath);
if (!stat.isFile() || stat.uid !== process.getuid() || (stat.mode & 0o077)) throw new Error('Configuration must be private');
const config = JSON.parse(fs.readFileSync(configPath, 'utf8'));
const flags = ['--account', config.account, '--state', config.state, '--kcli', config.kcli, '--kcli-root', config.kcliRoot,
  '--rpc', config.rpc, '--max-blocks', '10000'];
if (config.execute === true) {
  if (!config.reserve || !config.cap) throw new Error('Explicit reserve and cap required for execution');
  flags.push('--execute', '--reserve', config.reserve, '--cap', config.cap,
    '--min', config.minimum || '1', '--keychain-helper', path.join(__dirname, 'reinvest-keychain'));
} else if (config.reserve && config.cap) {
  flags.push('--reserve', config.reserve, '--cap', config.cap, '--min', config.minimum || '1');
} else flags.push('--observe-only');
console.log(`${new Date().toISOString()} mode=${config.execute === true ? 'execute' : 'observe'} start`);
const result = spawnSync(process.execPath, [path.join(__dirname, 'reinvest-teleno-rewards.cjs'), ...flags],
  { stdio: 'inherit', timeout: 3000000 });
if (result.error) console.error(result.error.message);
console.log(`${new Date().toISOString()} exit=${result.status}`);
process.exitCode = result.status === 0 ? 0 : 1;
