#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Package tracked engine sources plus the exact SDL/zlib sources used to build."""
import argparse
import io
from pathlib import Path
import subprocess
import tarfile
from build_release import ROOT, VERSION, prepare_dependency


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/dist')
    args = parser.parse_args(); args.output.mkdir(parents=True, exist_ok=True)
    # git archive enforces the public-source boundary: no ignored game files,
    # local scans, credentials, build outputs, or private historical branches.
    source = subprocess.check_output(['git', 'archive', '--format=tar', 'HEAD'], cwd=ROOT)
    prefix = f'REminiscence2-{VERSION}-source'
    path = args.output / f'{prefix}.tar.gz'
    with tarfile.open(path, 'w:gz') as output:
        with tarfile.open(fileobj=io.BytesIO(source)) as tracked:
            for member in tracked.getmembers():
                member.name = prefix + '/' + member.name
                output.addfile(member, tracked.extractfile(member) if member.isfile() else None)
        for name in ('SDL2', 'zlib'):
            output.add(prepare_dependency(name), arcname=f'{prefix}/third_party_src/{name}')
    print(path)

if __name__ == '__main__': main()
