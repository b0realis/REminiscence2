#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Install the flat-panel APK and deploy prepared game files using Linux adb."""
import argparse
import hashlib
from pathlib import Path
import shlex
import shutil
import subprocess
import time
import uuid


def asset_files(root):
    root = root.resolve()
    if not root.is_dir(): raise ValueError('Assets must be a prepared directory, not a WHDLoad archive')
    result = []
    for file in sorted(root.rglob('*')):
        if file.is_symlink(): raise ValueError('Asset symlinks are not deployed')
        if file.is_file(): result.append((file, file.relative_to(root).as_posix()))
    markers = {'level1.lev', 'demo.lev', 'level1.map', 'level1.bnq', 'glob_fr.aba', 'demo_uk.aba', 'intro.seq', 'menu1ssi.map', 'flashback.bin', 'flashback.rsrc', 'global.paq', 'present.lev'}
    if not any(file.name.lower() in markers for file, _ in result):
        raise ValueError('No supported prepared game files found in assets; copy your game data first')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apk', type=Path, required=True)
    parser.add_argument('--assets', type=Path, required=True)
    parser.add_argument('--serial', help='Select an adb device when several are connected')
    parser.add_argument('--experimental', action='store_true', help='Deploy to the separate debug app ID')
    parser.add_argument('--dry-run', action='store_true', help='Validate files and print deployment scope without connecting')
    args = parser.parse_args()
    if not args.apk.is_file(): parser.error('APK does not exist')
    try: files = asset_files(args.assets)
    except ValueError as error: parser.error(str(error))
    package = 'org.b0realis.reminiscence2' + ('.experimental' if args.experimental else '')
    if args.dry_run:
        print(f'Install {args.apk}; deploy {len(files)} files to {package}/files/assets; preserve SAVE and TUNES.'); return
    if not shutil.which('adb'): parser.error('Install Linux adb tools first: sudo apt install adb')
    base = ['adb'] + (['-s', args.serial] if args.serial else [])
    def adb(*arguments): return subprocess.check_output(base + list(arguments), text=True).strip()
    if not args.serial:
        devices = [line.split()[0] for line in adb('devices').splitlines()[1:] if line.endswith('\tdevice')]
        if len(devices) != 1: parser.error('Connect/authorize exactly one headset, or select --serial')
        base += ['-s', devices[0]]
    def shell(command): return adb('shell', command).replace('\r', '')
    quote = shlex.quote
    root = f'/sdcard/Android/data/{package}/files'
    component = f'{package}/org.b0realis.reminiscence2.LauncherActivity'
    print(adb('install', '-r', str(args.apk.resolve())))
    shell(f'am start -n {quote(component)}')
    # The app must create its external directories so headset ownership is right.
    for attempt in range(20):
        try:
            if shell(f'test -d {quote(root + "/assets")} && echo ready') == 'ready': break
        except subprocess.CalledProcessError:
            pass
        time.sleep(.5)
    else: raise RuntimeError('Open the app once on the headset to create its asset folder, then retry')
    shell(f'am force-stop {quote(package)}')
    nonce = uuid.uuid4().hex
    download = f'/sdcard/Download/REminiscence2-{nonce}'
    incoming = f'{root}/assets.incoming-{nonce}'
    previous = f'{root}/assets.previous-{nonce}'
    total = sum(file.stat().st_size for file, _ in files)
    free = shell(f'df -k {quote(root)}').splitlines()[-1].split()
    if len(free) < 4 or not free[3].isdigit() or int(free[3]) * 1024 < total + max(f.stat().st_size for f, _ in files) + 8 * 1024 * 1024:
        raise RuntimeError('Headset has insufficient free space for a complete staged asset copy')
    shell(f'mkdir -p {quote(download)} {quote(incoming)}')
    promoted = False
    try:
        for local, relative in files:
            digest = hashlib.sha256(local.read_bytes()).hexdigest()
            stage = f'{download}/{digest}.part'; destination = f'{incoming}/{relative}'
            adb('push', str(local), stage)
            if shell(f'sha256sum {quote(stage)}').split()[0] != digest: raise RuntimeError(f'Transfer checksum failed: {relative}')
            parent = destination.rsplit('/', 1)[0]
            # Copy on the device: avoids adb push's ownership/chown failures in
            # Android/data. No all-files storage permission or root is needed.
            shell(f'mkdir -p {quote(parent)} && cp {quote(stage)} {quote(destination)}')
            if shell(f'sha256sum {quote(destination)}').split()[0] != digest: raise RuntimeError(f'Installed checksum failed: {relative}')
            shell(f'rm -f {quote(stage)}')
            print(relative)
        assets = root + '/assets'
        shell(f'mv {quote(assets)} {quote(previous)} && (mv {quote(incoming)} {quote(assets)} || (mv {quote(previous)} {quote(assets)}; exit 1))')
        promoted = True
        shell(f'rm -rf {quote(previous)}')
    finally:
        shell(f'rm -rf {quote(download)}')
        if not promoted: shell(f'rm -rf {quote(incoming)}')
    shell(f'am start -n {quote(component)}')
    print('Assets verified and installed. Saves/settings were preserved. Use a Bluetooth gamepad or keyboard.')

if __name__ == '__main__': main()
