#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Verify same-commit CI/artifacts, then tag and publish a GitHub preview release."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tarfile
import tempfile
import zipfile
from build_release import ROOT, VERSION

REPOSITORY = 'b0realis/REminiscence2'


def output(*command):
    return subprocess.check_output([str(x) for x in command], cwd=ROOT, text=True).strip()


def verify_files(directory):
    expected = {
        f'REminiscence2-{VERSION}-win64.zip', f'REminiscence2-{VERSION}-linux64.tar.gz',
        f'REminiscence2-{VERSION}-steamdeck.tar.gz', f'REminiscence2-{VERSION}-macos-arm64.zip',
        f'REminiscence2-{VERSION}-macos-x86_64.zip', f'REminiscence2-{VERSION}-quest-arm64.apk',
        f'REminiscence2-{VERSION}-quest-arm64-bundle.zip', f'REminiscence2-{VERSION}-source.tar.gz'}
    found = {p.name for p in directory.iterdir() if p.is_file()}
    if found != expected: raise RuntimeError(f'Unexpected artifact set. Missing: {expected-found}; extra: {found-expected}')
    for path in sorted(directory.iterdir()):
        if path.suffix in ('.zip', '.apk'):
            with zipfile.ZipFile(path) as archive:
                bad = archive.testzip()
                if bad: raise RuntimeError(f'Corrupt ZIP member: {bad}')
                names = archive.namelist()
                if path.suffix == '.apk':
                    libraries = [n for n in names if n.startswith('lib/') and n.endswith('.so')]
                    if len(libraries) < 2 or any(not n.startswith('lib/arm64-v8a/') for n in libraries):
                        raise RuntimeError('Unexpected APK native architecture')
                elif not any(n.endswith('/assets/README.txt') for n in names):
                    raise RuntimeError('Package lacks adjacent precreated assets folder')
        else:
            with tarfile.open(path) as archive:
                names = archive.getnames()
                if 'source' in path.name:
                    header = next(n for n in names if n.endswith('/version.h') and '/third_party' not in n)
                    if f'"{VERSION}"' not in archive.extractfile(header).read().decode(): raise RuntimeError('Source version mismatch')
                    if not any('/third_party_src/SDL2/' in n for n in names) or not any('/third_party_src/zlib/' in n for n in names): raise RuntimeError('Missing dependency source')
                elif not any(n.endswith('/assets/README.txt') for n in names): raise RuntimeError('Missing assets folder')
        if path.suffix != '.apk' and 'source' not in path.name:
            if any('/assets/' in n and not n.endswith(('/assets/', '/assets/README.txt')) for n in names):
                raise RuntimeError('Original game data must not be bundled in assets')
        if any('/.local/' in n.lower() or n.lower().endswith(('.lha', '.state', '.p12', '.jks', '.pem')) for n in names):
            raise RuntimeError('Private game data or signing material in archive')
    return sorted(directory.iterdir())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run-id', required=True)
    parser.add_argument('--notes', type=Path, required=True)
    parser.add_argument('--publish', action='store_true', help='Create/push the version tag and publish; otherwise only verify/download')
    args = parser.parse_args()
    if not args.notes.is_file(): parser.error('Write the reviewed release notes first')
    if output('git', 'status', '--porcelain'): parser.error('Commit all release changes before tagging')
    revision = output('git', 'rev-parse', 'HEAD')
    run = json.loads(output('gh', 'run', 'view', args.run_id, '--repo', REPOSITORY, '--json', 'headSha,conclusion,workflowName,url'))
    if run['conclusion'] != 'success' or run['headSha'] != revision or run['workflowName'] != 'Release packages': raise RuntimeError('Release package CI must pass for exactly HEAD')
    tests = json.loads(output('gh', 'run', 'list', '--repo', REPOSITORY, '--workflow', 'build.yml', '--commit', revision, '--limit', '1', '--json', 'conclusion'))
    if not tests or tests[0]['conclusion'] != 'success': raise RuntimeError('Build/test CI must pass for exactly HEAD')
    folder = Path(tempfile.mkdtemp(prefix=f'verified-release-{VERSION}-{args.run_id}-', dir=ROOT / 'build'))
    subprocess.run(['gh','run','download',args.run_id,'--repo',REPOSITORY,'--dir',str(folder),'--pattern','*'],cwd=ROOT,check=True)
    flat = folder / 'assets'; flat.mkdir()
    for artifact in sorted(folder.iterdir()):
        if artifact == flat: continue
        for file in artifact.iterdir():
            destination = flat / file.name
            if destination.exists(): raise RuntimeError('Duplicate artifact filename')
            file.rename(destination)
    files = verify_files(flat)
    sums = flat / 'SHA256SUMS'
    sums.write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest() + '  ' + p.name + '\n' for p in files))
    print('Verified', len(files), 'release assets for', revision, 'from', run['url'])
    if not args.publish:
        print('Verification only. Run again with --publish to create the tag/release.'); return
    tag = 'v' + VERSION
    existing = subprocess.run(['git','rev-parse','--verify','refs/tags/'+tag],cwd=ROOT,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode == 0
    if existing:
        if output('git','rev-list','-n','1',tag) != revision: raise RuntimeError('Existing tag points to a different commit')
    else: subprocess.run(['git','tag','-a',tag,'-m',f'REminiscence2 {VERSION} preview release',revision],cwd=ROOT,check=True)
    subprocess.run(['git','push','origin','refs/tags/'+tag],cwd=ROOT,check=True)
    subprocess.run(['gh','release','create',tag,'--repo',REPOSITORY,'--verify-tag','--title',f'REminiscence2 {VERSION} — first multiplatform preview','--notes-file',str(args.notes.resolve()),'--prerelease',*[str(p) for p in files],str(sums)],cwd=ROOT,check=True)
    print('Published https://github.com/'+REPOSITORY+'/releases/tag/'+tag)

if __name__ == '__main__': main()
