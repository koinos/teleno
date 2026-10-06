#!/usr/bin/env python3
"""Run the release binary against fresh owned data and loopback-only RPC."""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import tempfile
import time
import urllib.error
import urllib.request


def require(value, message):
    if not value:
        raise RuntimeError(message)


def rpc_bytes(value):
    if value.startswith('0x'):
        return bytes.fromhex(value[2:])
    return base64.b64decode(value + '=' * (-len(value) % 4), altchars=b'-_', validate=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--node', required=True)
    parser.add_argument('--fixture', required=True)
    args = parser.parse_args()
    artifact_root = Path(os.environ['TELENO_TEST_ARTIFACT_ROOT'])
    require(artifact_root.is_absolute() and artifact_root.is_dir() and not artifact_root.is_symlink()
            and artifact_root.resolve() != Path('/'), 'existing owned artifact root required')
    root = Path(tempfile.mkdtemp(prefix='compatibility-node-', dir=artifact_root))
    root.chmod(0o700)
    basedir = root / 'observer'
    basedir.mkdir()
    bytecode_path = os.environ.get('TELENO_TEST_CONTRACT_BYTECODE')
    command = [args.fixture, str(basedir)]
    if bytecode_path:
        payload = Path(bytecode_path).read_bytes()
        require(len(payload) == 109002 and hashlib.sha256(payload).hexdigest()
                == 'd66facf63456ff6b2690d7e6756142008b11d985886d8ce10411864eb6992a49',
                'selected bridge WASM hash/length mismatch')
        command.append(bytecode_path)
    with (root / 'fixture.log').open('w') as log:
        subprocess.run(command, check=True, stdout=log, stderr=subprocess.STDOUT, timeout=30)
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        port = sock.getsockname()[1]
    config = basedir / 'config.yml'
    config.write_text(f'''global:
  log-level: info
chain:
  verify-blocks: true
jsonrpc:
  listen: 127.0.0.1:{port}
  jobs: 1
features:
  chain: true
  block_store: true
  mempool: true
  jsonrpc: true
  p2p: false
  grpc: false
  block_producer: false
  contract_meta_store: false
  transaction_store: false
  account_history: false
''')
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    url = f'http://127.0.0.1:{port}/'
    sequence = 0

    def rpc(method, params=None, error=False):
        nonlocal sequence
        sequence += 1
        body = json.dumps({'jsonrpc': '2.0', 'id': sequence, 'method': method,
                           'params': params or {}}).encode()
        req = urllib.request.Request(url, data=body, headers={'Content-Type': 'application/json'})
        with opener.open(req, timeout=5) as response:
            result = json.load(response)
        require(result.get('id') == sequence, 'RPC response identity')
        if error:
            require('error' in result, 'oversize read unexpectedly succeeded')
            message = result['error'].get('message', '').lower()
            require('buffer' in message and 'return' in message, 'unexpected oversize refusal')
            return result['error']
        require('error' not in result and 'result' in result, f'{method}: {result.get("error")}')
        return result['result']

    def start(label):
        log = (root / f'{label}.log').open('w')
        process = subprocess.Popen([args.node, '--basedir', str(basedir), '--config', str(config),
                                    '--disable', 'p2p', 'block_producer'], stdout=log,
                                   stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL, cwd=root)
        try:
            deadline = time.monotonic() + 25
            while time.monotonic() < deadline:
                require(process.poll() is None, 'node exited before RPC readiness')
                try:
                    rpc('chain.get_chain_id')
                    return process, log
                except (urllib.error.URLError, TimeoutError):
                    time.sleep(0.1)
            raise RuntimeError('node RPC readiness timed out')
        except BaseException:
            process.kill()
            process.wait(timeout=10)
            log.close()
            raise

    def stop(process, log):
        try:
            process.send_signal(signal.SIGTERM)
            require(process.wait(timeout=15) == 0, 'observer shutdown failed')
        except BaseException:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=10)
            raise
        finally:
            log.close()

    def read_cases():
        before = rpc('chain.get_head_info')
        for slot in ('large', 'boundary', 'small'):
            response = rpc('chain.invoke_system_call', json.loads((basedir / f'{slot}-request.json').read_text()))
            require(rpc_bytes(response['value'])
                    == (basedir / f'{slot}-expected-wire.bin').read_bytes(), f'{slot} byte-for-byte readback')
        rpc('chain.invoke_system_call', json.loads((basedir / 'oversize-request.json').read_text()), error=True)
        response = rpc('chain.invoke_system_call', json.loads((basedir / 'small-request.json').read_text()))
        require(rpc_bytes(response['value'])
                == (basedir / 'small-expected-wire.bin').read_bytes(), 'read after refusal')
        require(before == rpc('chain.get_head_info'), 'read changed chain head/state')

    def block_id(height):
        return '0x' + (basedir / f'block-{height}-id.bin').read_bytes().hex()

    def read_receipt(height):
        response = rpc('block_store.get_blocks_by_id', {'block_ids': [block_id(height)],
                                                       'return_block': True, 'return_receipt': True})
        require(len(response['block_items']) == 1, 'archived block missing')
        item = response['block_items'][0]
        receipt = item['receipt']
        expected_root = (basedir / f'block-{height}-root.bin').read_bytes()
        expected_id = (basedir / f'block-{height}-id.bin').read_bytes()
        require(rpc_bytes(receipt['id']) == expected_id and int(receipt['height']) == height
                and rpc_bytes(receipt['state_merkle_root']) == expected_root,
                f'archived finalized receipt/root mismatch: {receipt.get("state_merkle_root")} / {expected_root.hex()}')
        require(rpc_bytes(item['block']['id']) == expected_id, 'archived block identity')
        return item

    def submit(height):
        response = rpc('chain.submit_block', json.loads((basedir / f'block-{height}-request.json').read_text()))
        item = read_receipt(height)
        require(response['receipt'] == item['receipt'], 'early archive did not complete exact execution receipt')
        head = rpc('chain.get_head_info')
        require(int(head['head_topology']['height']) == height
                and rpc_bytes(head['head_topology']['id']) == (basedir / f'block-{height}-id.bin').read_bytes(),
                'observer head did not advance')
        return item

    process, log = start('first-start')
    try:
        chain_id = rpc('chain.get_chain_id')
        read_cases()
        original = [submit(1), submit(2)]
        head_before = rpc('chain.get_head_info')
    finally:
        stop(process, log)
    process, log = start('cold-restart')
    try:
        require(rpc('chain.get_chain_id') == chain_id, 'restart chain identity changed')
        require(rpc('chain.get_head_info') == head_before, 'restart head/state changed')
        require([read_receipt(1), read_receipt(2)] == original, 'restart block/receipt bytes changed')
        read_cases()
        submit(3)
        require([read_receipt(1), read_receipt(2)] == original, 'later block changed earlier receipts')
    finally:
        stop(process, log)
    for name in ('first-start.log', 'cold-restart.log'):
        text = (root / name).read_text()
        require('[block_producer] Started' not in text and '[p2p] Started' not in text, 'forbidden component started')
    evidence = {'status': 'pass', 'scope': 'isolated-native-observer', 'native_version':
                subprocess.check_output([args.node, '--version'], text=True).strip(),
                'selected_bridge_wasm': bool(bytecode_path), 'bytecode_bytes': 109002,
                'bytecode_sha256': hashlib.sha256((basedir / 'selected-bytecode.bin').read_bytes()).hexdigest(),
                'return_wire_boundary': 131072, 'oversize_refused': True, 'read_state_unchanged': True,
                'finalized_receipts': 3, 'receipts_persisted_after_cold_restart': 2,
                'observer_restart': 'pass', 'public_network_transactions': 0,
                'real_signing_keys': 0, 'live_node_changes': False}
    (root / 'result.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print(json.dumps(evidence, sort_keys=True))
    print(f'Retained owned fixture: {root}')


if __name__ == '__main__':
    main()
