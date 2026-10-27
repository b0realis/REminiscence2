#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Launch the local Apple Silicon build with private Amiga, GOG or Sega CD assets.
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
    --segacd) asset_choice="--asset-set=sega:segacd"; shift ;;
esac

if [ ! -x "$engine" ]; then
    printf 'Local build missing. Build it with:\n  cmake --build "%s/build/package-arm64" --target reminiscence2\n' "$project_dir" >&2
    exit 1
fi

if [ "${1-}" = "--starship" ]; then
    shift
    demo="$project_dir/.local/editor-projects/AfterTheDrift-Classic"
    if [ ! -f "$demo/campaign.r2campaign.json" ]; then
        demo="$project_dir/examples/editor/alien-starship"
    fi
    if [ ! -f "$demo/campaign.r2campaign.json" ]; then
        printf 'This private demo is not included. Use --campaign=/path/to/your/campaign.r2campaign.json.\n' >&2
        exit 1
    fi
    set -- "--campaign=$demo/campaign.r2campaign.json" "$@"
fi

if [ "${1-}" = "--demo" ]; then
    shift
    demo="$project_dir/.local/editor-projects/RelayStation"
    if [ ! -f "$demo/campaign.r2campaign.json" ]; then
        demo="$project_dir/examples/editor/relay-station"
    fi
    if [ ! -f "$demo/campaign.r2campaign.json" ]; then
        printf 'This private demo is not included. Use --campaign=/path/to/your/campaign.r2campaign.json.\n' >&2
        exit 1
    fi
    set -- "--campaign=$demo/campaign.r2campaign.json" "$@"
fi
[ -z "$asset_choice" ] || set -- "$asset_choice" "$@"

# Absolute defaults work from any directory; later arguments can override them.
# Saves and preferences use the engine's normal user-data location.
exec "$engine" \
    --config="$project_dir/fb.cfg" \
    --datapath="$data_dir" \
    --window "$@"
