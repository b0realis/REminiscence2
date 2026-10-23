#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Reject known private documentation assets in the public source tree.

This checks an explicit release boundary, not copyright status or legal clearance.
It works in an extracted source tree too; Git-specific checks run when available.
"""

from pathlib import Path
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
    if (root / ".git").exists():
        result = subprocess.run(["git", "ls-files", "-z"], cwd=root, capture_output=True, check=False)
        if result.returncode:
            problems.append("Unable to inspect the tracked release paths")
        else:
            for name in result.stdout.decode("utf-8", errors="replace").split("\0"):
                first = name.split("/", 1)[0]
                if first in (".local", "build") or first.startswith("build-"):
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
