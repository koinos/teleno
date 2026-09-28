#!/usr/bin/env python3
"""Install an hourly per-user observation job. This never enables burns."""
import argparse
import json
import os
from pathlib import Path
import plistlib
import shutil
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--account', required=True, help='Mainnet producer/custody account')
    parser.add_argument('--password-file', required=True, type=Path, help='Existing private wallet password file')
    parser.add_argument('--kcli-root', required=True, type=Path, help='kcli source checkout with node_modules installed')
    parser.add_argument('--node', type=Path, default=shutil.which('node'))
    parser.add_argument('--kcli', type=Path, default=shutil.which('kcli'))
    opt = parser.parse_args()
    if sys.platform != 'darwin':
        parser.error('This installer requires macOS')
    if not opt.node or not opt.kcli:
        parser.error('Pass --node and --kcli or put both on PATH')
    os.umask(0o077)
    home = Path.home()
    source = Path(__file__).resolve().parent
    base = home / 'Library/Application Support/KoinosReinvest'
    release = base / 'releases/keychain-v1'
    logs = home / 'Library/Logs/KoinosReinvest'
    config_path = base / 'config.json'
    state = home / '.kcli/teleno-reinvest/state.json'
    node, kcli, root = opt.node.resolve(), opt.kcli.resolve(), opt.kcli_root.resolve()
    label = 'org.koinos.reinvest'
    plist_path = home / f'Library/LaunchAgents/{label}.plist'
    if config_path.exists() or plist_path.exists():
        raise SystemExit('Existing installation found; refusing to overwrite it')
    for required in [node, kcli, root / 'node_modules/koilib', root / 'src/abis/pob.json']:
        if not required.exists():
            raise SystemExit(f'Missing dependency: {required}')
    wallet = home / '.kcli/wallet.json'
    if json.loads(wallet.read_text()).get('address') != opt.account:
        raise SystemExit('kcli custody wallet does not match --account')
    for folder in [base, release, logs, state.parent, plist_path.parent]:
        folder.mkdir(parents=True, exist_ok=True)
    for folder in [base, release, logs, state.parent]:
        folder.chmod(0o700)
    for name in ['reinvest-teleno-rewards.cjs', 'reinvest-macos-runner.cjs', 'reinvest-keychain.swift']:
        shutil.copyfile(source / name, release / name)
        (release / name).chmod(0o600)
    helper = release / 'reinvest-keychain'
    subprocess.run(['/usr/bin/swiftc', str(release / 'reinvest-keychain.swift'), '-o', str(helper)], check=True)
    helper.chmod(0o700)
    subprocess.run([str(helper), 'import', opt.account, str(opt.password_file.resolve())], check=True)
    subprocess.run([str(helper), 'check', opt.account], check=True)
    if not state.exists():
        subprocess.run([str(node), str(release / 'reinvest-teleno-rewards.cjs'), '--init',
                        '--account', opt.account, '--kcli-root', str(root), '--state', str(state)], check=True)
    else:
        existing = json.loads(state.read_text())
        if existing.get('account') != opt.account or existing.get('pending'):
            raise SystemExit('Existing state has a different account or unresolved burn; review it manually')
    config = dict(execute=False, account=opt.account, reserve=None, cap=None, minimum='1',
                  state=str(state), kcli=str(kcli), kcliRoot=str(root), rpc='https://api.koinos.io')
    config_path.write_text(json.dumps(config, indent=2) + '\n')
    config_path.chmod(0o600)
    job = dict(Label=label, ProgramArguments=[str(node), str(release / 'reinvest-macos-runner.cjs'), str(config_path)],
               StartInterval=3600, RunAtLoad=True, ProcessType='Background',
               EnvironmentVariables=dict(PATH=f'{node.parent}:/usr/bin:/bin', HOME=str(home)),
               StandardOutPath=str(logs / 'stdout.log'), StandardErrorPath=str(logs / 'stderr.log'), Umask=0o077)
    for name in ['stdout.log', 'stderr.log']:
        (logs / name).touch(exist_ok=True)
        (logs / name).chmod(0o600)
    with plist_path.open('wb') as stream:
        plistlib.dump(job, stream)
    plist_path.chmod(0o600)
    subprocess.run(['/bin/launchctl', 'bootstrap', f'gui/{os.getuid()}', str(plist_path)], check=True)
    print(f'Installed {label}: hourly observation, burns disabled. Configuration: {config_path}')


if __name__ == '__main__':
    main()
