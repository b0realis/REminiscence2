#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise real editor widgets with SDL's offscreen driver (test builds only)."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("example", type=Path)
    parser.add_argument("--assets", type=Path)
    args = parser.parse_args()
    env = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy",
               SDL_RENDER_DRIVER="software")
    with tempfile.TemporaryDirectory(prefix="reminiscence2-editor-test-") as output:
        command = [str(args.binary.resolve()), str(args.example.resolve()),
                   f"--smoke-test={output}"]
        if args.assets:
            command.append(f"--assets={args.assets.resolve()}")
        result = subprocess.run(command, env=env, timeout=120, check=False)
        if result.returncode < 0:
            print(f"Editor terminated by signal {-result.returncode}")
        return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
