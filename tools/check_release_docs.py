#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Reject known private documentation assets in the public source tree.

This checks an explicit release boundary, not copyright status or legal clearance.
It works in an extracted source tree too; Git-specific checks run when available.
"""

from pathlib import Path
import json
import re
import subprocess
import sys


ROOT = Path(__file__).resolve().parent.parent
REMOVED_PATHS = (
    "docs/manual/images",
    "docs/manual/images.json",
)
PRIVATE_FORMATS = {
    ".png", ".jpg", ".jpeg", ".gif", ".webp", ".avif", ".bmp", ".svg",
    ".tif", ".tiff", ".ico", ".pdf", ".lha",
}
PRIVATE_CAMPAIGNS = (
    "examples/editor/alien-starship/",
    "examples/editor/relay-station/",
    "tools/create_alien_starship.py",
    "tools/create_relay_station.py",
    "tests/alien_starship_tests.cpp",
    "tests/relay_station_tests.cpp",
    "docs/screenshots/editor-relay-station-1.46.png",
)


def check(root: Path) -> list[str]:
    problems = []
    for relative in REMOVED_PATHS:
        if (root / relative).exists():
            problems.append(f"Removed manual asset path is present: {relative}")
    # Match suffixes without case assumptions, including nested accidental copies.
    for folder in ("docs/manual", "docs/reference"):
        for path in (root / folder).rglob("*"):
            if path.is_file() and path.suffix.lower() in PRIVATE_FORMATS:
                problems.append(f"Private binary reference in public documentation: {path.relative_to(root)}")
    manual = root / "manual.md"
    if not manual.is_file():
        problems.append("manual.md is missing")
    else:
        text = manual.read_text(encoding="utf-8")
        if re.search(r"!\[|<(?:img|image|svg|object|embed|iframe)\b|data:image/", text, re.IGNORECASE):
            problems.append("The player guide must remain text-only; found an embedded-image/object marker")
        if "SPDX-License-Identifier: GPL-3.0-or-later" not in text:
            problems.append("The original guide's license identifier is missing")
    manifest = root / "packaging/public-docs.json"
    public_docs = []
    try:
        public_docs = json.loads(manifest.read_text(encoding="utf-8"))
        if not isinstance(public_docs, list) or not all(isinstance(name, str) for name in public_docs):
            raise ValueError("Expected a list of public documentation paths")
        for name in public_docs:
            path = Path(name)
            if path.is_absolute() or ".." in path.parts or not path.parts or path.parts[0] != "docs" or not (root / path).is_file():
                problems.append(f"Invalid public documentation manifest entry: {name}")
    except (OSError, ValueError) as error:
        problems.append(f"Cannot read public documentation manifest: {error}")
    if (root / ".git").exists():
        result = subprocess.run(["git", "ls-files", "-z"], cwd=root, capture_output=True, check=False)
        if result.returncode:
            problems.append("Unable to inspect the tracked release paths")
        else:
            tracked = result.stdout.decode("utf-8", errors="replace").split("\0")
            if sorted(name for name in tracked if name.startswith("docs/")) != public_docs:
                problems.append("packaging/public-docs.json must match tracked docs paths")
            for name in tracked:
                if any(name.startswith(path) if path.endswith("/") else name == path
                       for path in PRIVATE_CAMPAIGNS):
                    problems.append(f"Private authored campaign content is tracked: {name}")
                first = name.split("/", 1)[0]
                if first in (".local", "build", "upscaled") or first.startswith("build-") or name.lower().endswith((".state", ".r2s", ".r2m")):
                    problems.append(f"Private/generated path is tracked: {name}")
    return problems


def main() -> int:
    problems = check(ROOT)
    if problems:
        for problem in problems:
            print(f"ERROR: {problem}", file=sys.stderr)
        return 1
    print("Documentation boundary checked: original text-only guide; known private asset locations excluded.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
