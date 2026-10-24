#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Launch the local Apple Silicon build with private Amiga or extracted GOG assets.
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
engine="$project_dir/build/package-arm64/reminiscence2"
data_dir="$project_dir/.local/assets"
asset_choice=""
case "${1-}" in
    --gog)
        asset_choice="--asset-set=dos"
        shift
        ;;
    --amiga) asset_choice="--asset-set=amiga:amiga-unpacked"; shift ;;
esac

if [ ! -x "$engine" ]; then
    printf 'Local build missing. Build it with:\n  cmake --build "%s/build/package-arm64" --target reminiscence2\n' "$project_dir" >&2
    exit 1
fi

[ -z "$asset_choice" ] || set -- "$asset_choice" "$@"

# Absolute defaults work from any directory; later arguments can override them.
# Saves and preferences use the engine's normal user-data location.
exec "$engine" \
    --config="$project_dir/fb.cfg" \
    --datapath="$data_dir" \
    --window "$@"
