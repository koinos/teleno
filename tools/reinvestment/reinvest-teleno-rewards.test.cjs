'use strict';
const { test } = require('node:test');
const assert = require('node:assert/strict');
const { units, decimal, budget, cliExact, args } = require('./reinvest-teleno-rewards.cjs');
test('decimal conversion never rounds atomic units', () => {
  for (const text of ['0', '1', '0.00000001', '12345.12345678']) {
    assert.equal(units(decimal(units(text))), units(text));
  }
  for (const bad of ['-1', '1e3', '1.000000001', 'NaN']) assert.throws(() => units(bad));
});
test('95 percent applies to rewards, not wallet balance', () => {
  assert.equal(budget(units('100'), 0n, units('10000'), units('10'), units('1000')), units('95'));
  assert.equal(budget(units('200'), units('95'), units('10000'), units('10'), units('1000')), units('95'));
});
test('reserve, cap, partial reinvestment and exhaustion', () => {
  assert.equal(budget(units('100'), 0n, units('30'), units('10'), units('100')), units('20'));
  assert.equal(budget(units('100'), 0n, units('200'), units('10'), units('5')), units('5'));
  assert.equal(budget(units('100'), units('95'), units('200'), units('10'), units('100')), 0n);
  assert.equal(budget(units('100'), 0n, units('5'), units('10'), units('100')), 0n);
});
test('kcli Number workaround never exceeds intended amount', () => {
  for (const text of ['1.00000001', '2.12034567', '100.12345678']) {
    const requested = units(text), actual = cliExact(requested);
    assert.ok(actual <= requested);
    assert.equal(BigInt(Math.floor(Number(decimal(actual)) * 1e8)), actual);
  }
});
test('execution is opt-in and unknown flags are rejected', () => {
  assert.equal(args([]).execute, undefined);
  assert.equal(args(['--execute', '--reserve', '10']).execute, true);
  assert.throws(() => args(['--reserve']));
  assert.throws(() => args(['--percent', '95']));
  assert.equal(args(['--observe-only'])['observe-only'], true);
  assert.equal(args(['--keychain-helper', '/private/helper'])['keychain-helper'], '/private/helper');
});
test('installed runner refuses execution without explicit reserve and cap', () => {
  const fs = require('node:fs'), os = require('node:os'), path = require('node:path');
  const { spawnSync } = require('node:child_process');
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'reinvest-runner-test-'));
  try {
    const config = path.join(dir, 'config.json');
    fs.writeFileSync(config, JSON.stringify({ execute: true, state: '/unused', kcli: '/unused', kcliRoot: '/unused', rpc: 'https://api.koinos.io' }), { mode: 0o600 });
    const result = spawnSync(process.execPath, [path.join(__dirname, 'reinvest-macos-runner.cjs'), config], { encoding: 'utf8' });
    assert.notEqual(result.status, 0);
    assert.match(result.stderr, /Explicit reserve and cap required/);
  } finally { fs.rmSync(dir, { recursive: true }); }
});
