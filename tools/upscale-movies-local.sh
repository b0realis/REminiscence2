#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Local Sega CD restoration runner; defaults are anchored to this checkout.
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
movie_python="$project_dir/.local/movie-tools/venv/bin/python"
if [ ! -x "$movie_python" ]; then
    printf '%s\n' 'Set up .local/movie-tools/venv as described in tools/MOVIE_UPSCALING.md.' >&2
    exit 1
fi
if [ "$#" -eq 0 ]; then set -- --list; fi
exec "$movie_python" "$project_dir/tools/upscale_movies.py" \
    "$project_dir/.local/assets/segacd" "$@"
