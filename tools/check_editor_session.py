#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Test the actual native player process and atomic editor command protocol."""
import argparse
import copy
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('fixtures', type=Path)
    parser.add_argument('assets', type=Path)
    args = parser.parse_args()
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', SDL_RENDER_DRIVER='software')
    with tempfile.TemporaryDirectory(prefix='native editor session ') as temp:
        root = Path(temp)
        for source in args.fixtures.glob('level-*.r2project.json'):
            shutil.copy2(source, root / source.name)
        project = json.loads((root / 'level-1.r2project.json').read_text())
        control = root / 'control.json'
        sequence = 0
        def publish(action, document=None, start=None):
            nonlocal sequence
            sequence += 1
            command = {'id': str(sequence), 'action': action}
            if document is not None:
                command['project'] = json.dumps(document)
            if start is not None:
                command['start'] = start
            temporary = control.with_suffix('.tmp')
            temporary.write_text(json.dumps(command))
            temporary.replace(control)
            return str(sequence)
        marker = json.loads((args.fixtures / 'test-start.json').read_text())
        initial = publish('play-here', project, marker)
        with (root / 'player.log').open('w+') as log:
            child = subprocess.Popen([str(args.binary.resolve()), '--no-preferences',
                                      f'--datapath={args.assets.resolve()}',
                                      f'--project={root / "level-1.r2project.json"}',
                                      f'--editor-session={control}'], env=env, stdout=log, stderr=log)
            def wait_ack(identity):
                deadline = time.monotonic() + 10
                while time.monotonic() < deadline:
                    if child.poll() is not None:
                        raise AssertionError(f'Native player exited early: {child.returncode}')
                    try:
                        status = json.loads(Path(str(control) + '.status').read_text())
                        if status['ack'] == identity:
                            return status
                    except (FileNotFoundError, json.JSONDecodeError):
                        pass
                    time.sleep(.02)
                raise AssertionError(f'Native command {identity} was not acknowledged')
            try:
                status = wait_ack(initial)
                assert not status['paused'] and status['level'] == 0, status
                assert status['room'] == int(marker['room'].removeprefix('original-room-')), status
                assert status['x'] == marker['x'] and status['y'] == marker['y'], status
                status = wait_ack(publish('pause'))
                assert status['paused'], status
                for value in (2**32 + marker['x'], 2**64 - 1, -1, 1.5):
                    invalid = dict(marker, x=value)
                    status = wait_ack(publish('play-here', project, invalid))
                    assert status['paused'] and 'coordinate' in status['message'], status
                applied = copy.deepcopy(project)
                applied['name'] = 'Native session regression'
                status = wait_ack(publish('apply', applied))
                assert status['paused'] and 'Applied' in status['message'], status
                broken = copy.deepcopy(applied)
                broken['native']['actors'][0]['data'][0] ^= 1
                status = wait_ack(publish('apply', broken))
                assert 'ports may change' in status['message'], status
                moved = copy.deepcopy(applied)
                moved['native']['actors'][0]['x'] += 8
                moved['spawn']['x'] += 8
                status = wait_ack(publish('apply', moved))
                assert status['restartRequired'] and status['paused'], status
                second = json.loads((root / 'level-2.r2project.json').read_text())
                status = wait_ack(publish('restart', second))
                assert status['level'] == 1 and status['paused'] and 'Restarted' in status['message'], status
                status = wait_ack(publish('play-here', project, marker))
                assert not status['paused'] and status['level'] == 0, status
                assert status['room'] == int(marker['room'].removeprefix('original-room-')), status
                assert status['x'] == marker['x'] and status['y'] == marker['y'], status
                assert json.loads((root / 'level-1.r2project.json').read_text()) == project
                status = wait_ack(publish('pause'))
                assert status['paused']
                status = wait_ack(publish('step'))
                deadline = time.monotonic() + 5
                while not status['paused'] and time.monotonic() < deadline:
                    time.sleep(.02)
                    status = json.loads(Path(str(control) + '.status').read_text())
                assert status['paused'] and 'Stepped' in status['message'], status
                snapshot = status['actors']
                time.sleep(.1)
                assert json.loads(Path(str(control) + '.status').read_text())['actors'] == snapshot
                status = wait_ack(publish('resume'))
                assert not status['paused'], status
                status = wait_ack(publish('play-start', project))
                assert not status['paused'] and 'authored level start' in status['message'], status
                # Stop must also exit a native cinematic/dialogue/QoL event loop,
                # where pause/apply correctly wait for a simulation boundary.
                time.sleep(.15)
                publish('stop')
                assert child.wait(timeout=10) == 0
                assert (root / '.native-playtest-saves').is_dir()
            except Exception:
                log.flush(); log.seek(0)
                print(log.read()[-6000:])
                raise
            finally:
                if child.poll() is None:
                    child.terminate()
                    try:
                        child.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        child.kill(); child.wait()
    print('Native player process: pause, atomic apply, rejection, level restart, resume and stop passed')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
