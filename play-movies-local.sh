#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Browse private, extracted Sega CD movies using the current local build.
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec python3 "$project_dir/tools/play_cutscenes.py" \
    "$project_dir/.local/assets/segacd" \
    --engine "$project_dir/build/package-arm64/reminiscence2" "$@"
