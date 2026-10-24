#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Advance the single player-facing version source, once per delivered patch."""

import argparse
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parent.parent
VERSION = re.compile(r'^(#define REMINISCENCE2_VERSION ")([0-9]+\.[0-9]{2,})(")$', re.MULTILINE)


def next_version(current: str) -> str:
    if not re.fullmatch(r"[0-9]+\.[0-9]{2,}", current):
        raise ValueError("Expected a version such as 1.01")
    major, patch = current.split(".")
    return f"{int(major)}.{int(patch) + 1:02d}"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dry-run", action="store_true", help="Print the next version without editing files")
    args = parser.parse_args()
    path = ROOT / "version.h"
    source = path.read_text(encoding="utf-8")
    matches = list(VERSION.finditer(source))
    if len(matches) != 1:
        parser.error("version.h must contain exactly one canonical version definition")
    match = matches[0]
    current = match.group(2)
    following = next_version(current)
    if not args.dry_run:
        path.write_text(source[:match.start(2)] + following + source[match.end(2):], encoding="utf-8")
    print(f"{current} -> {following}" + (" (dry run)" if args.dry_run else ""))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
