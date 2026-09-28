'use strict';
const { test } = require('node:test');
const assert = require('node:assert/strict');
const { createReadRpc, canonicalBlocks } = require('./reinvest-rpc.cjs');
function client(fetchImpl, extra = {}) {
  return createReadRpc({ endpoint: 'https://rpc.example.invalid', fetchImpl,
    sleep: async () => {}, intervalMs: 0, random: () => 0, warn: () => {}, ...extra });
}
function success(request, result = { value: 'ok' }) {
  return new Response(JSON.stringify({ jsonrpc: '2.0', id: JSON.parse(request.body).id, result }));
}
test('retries HTML responses and sends explicit JSON headers', async () => {
  let attempts = 0;
  const call = client(async (_, request) => {
    assert.equal(request.headers['Content-Type'], 'application/json');
    assert.equal(request.headers.Accept, 'application/json');
    assert.equal(request.redirect, 'error');
    return ++attempts === 1 ? new Response('<html>temporary gateway</html>') : success(request);
  });
  assert.deepEqual(await call('chain.get_head_info', {}), { value: 'ok' });
  assert.equal(attempts, 2);
});
test('429 respects bounded Retry-After and exponential backoff', async () => {
  let attempts = 0;
  const waits = [];
  const call = client(async (_, request) => ++attempts === 1
    ? new Response('', { status: 429, headers: { 'Retry-After': '600' } }) : success(request),
  { sleep: async ms => waits.push(ms) });
  await call('chain.get_head_info', {});
  assert.ok(waits.includes(30000));
  assert.equal(attempts, 2);
});
test('transient gateway failures exhaust a finite retry budget', async () => {
  let attempts = 0;
  const call = client(async () => { attempts++; return new Response('<html>', { status: 503 }); }, { attempts: 3 });
  await assert.rejects(call('chain.get_chain_id', {}), /HTTP 503; stopped after 3/);
  assert.equal(attempts, 3);
});
test('transport errors retry and recover', async () => {
  let attempts = 0;
  const call = client(async (_, request) => {
    if (++attempts === 1) throw new Error('connection reset');
    return success(request);
  });
  await call('chain.get_head_info', {});
  assert.equal(attempts, 2);
});
test('timeout aborts the request and exhausts bounded retries', async () => {
  let attempts = 0;
  const call = client(async (_, request) => {
    attempts++;
    return new Promise((_, reject) => request.signal.addEventListener('abort', () => reject(new Error('aborted')), { once: true }));
  }, { timeoutMs: 5, attempts: 2 });
  await assert.rejects(call('chain.get_head_info', {}), /timeout; stopped after 2/);
  assert.equal(attempts, 2);
});
test('authorization errors are not retried', async () => {
  let attempts = 0;
  const call = client(async () => { attempts++; return new Response('', { status: 403 }); });
  await assert.rejects(call('chain.read_contract', {}), /HTTP 403; stopped after 1/);
  assert.equal(attempts, 1);
});
test('JSON-RPC application errors are not retried or leaked into logs', async () => {
  let attempts = 0;
  const call = client(async (_, request) => {
    attempts++;
    return new Response(JSON.stringify({ jsonrpc: '2.0', id: JSON.parse(request.body).id,
      error: { code: -32602, message: 'sensitive-detail' } }));
  });
  await assert.rejects(call('chain.read_contract', {}), error =>
    error.message.includes('-32602') && !error.message.includes('sensitive-detail'));
  assert.equal(attempts, 1);
});
test('wrong envelope/id fails closed', async () => {
  const call = client(async () => new Response(JSON.stringify({ jsonrpc: '2.0', id: 'wrong', result: {} })));
  await assert.rejects(call('chain.get_head_info', {}), /invalid JSON-RPC envelope/);
});
test('submission methods are rejected without a network request', async () => {
  let attempts = 0;
  const call = client(async () => { attempts++; throw new Error('must not fetch'); });
  for (const method of ['chain.submit_transaction', 'chain.submit_block', 'unknown.method']) {
    await assert.rejects(call(method, {}), /Read-only RPC transport refuses/);
  }
  assert.equal(attempts, 0);
});
test('block batching uses one pinned head and validates exact sorted coverage', async () => {
  const calls = [];
  const provider = { getBlocks: async (start, count, head) => {
    calls.push([start, count, head]);
    return Array.from({ length: count }, (_, i) => ({ block_height: String(start + i) })).reverse();
  } };
  const found = [];
  for await (const b of canonicalBlocks(provider, 'pinned-head', 10, 14, 3)) found.push(Number(b.block_height));
  assert.deepEqual(found, [10, 11, 12, 13, 14]);
  assert.deepEqual(calls, [[10, 3, 'pinned-head'], [13, 2, 'pinned-head']]);
});
test('missing or duplicate blocks never yield partially validated batches', async () => {
  for (const blocks of [[{ block_height: '10' }], [{ block_height: '10' }, { block_height: '10' }]]) {
    let yielded = 0;
    await assert.rejects(async () => {
      for await (const _ of canonicalBlocks({ getBlocks: async () => blocks }, 'head', 10, 11)) yielded++;
    }, /block batch/);
    assert.equal(yielded, 0);
  }
});
test('bundled kcli token ABI constructs and decodes balance responses', { skip: !process.env.KCLI_ROOT }, async () => {
  const fs = require('node:fs'), path = require('node:path');
  const root = process.env.KCLI_ROOT;
  const { Contract, Provider, Serializer, utils } = require(path.join(root, 'node_modules/koilib'));
  const abi = JSON.parse(fs.readFileSync(path.join(root, 'src/abis/token.json'), 'utf8'));
  const serializer = new Serializer(abi.koilib_types);
  const encoded = await serializer.serialize({ value: '123456789' }, 'koinos.contracts.token.balance_of_result');
  const provider = new Provider(['https://rpc.example.invalid']);
  provider.call = async (method, params) => {
    assert.equal(method, 'chain.read_contract');
    assert.equal(params.entry_point, 1550980247);
    return { result: utils.encodeBase64url(encoded) };
  };
  const koin = new Contract({ id: '19GYjDBVXU7keLbYvMLazsGQn3GTWHjHkK', abi, provider });
  const { result } = await koin.functions.balance_of({ owner: '19GYjDBVXU7keLbYvMLazsGQn3GTWHjHkK' });
  assert.equal(result.value, '123456789');
});
