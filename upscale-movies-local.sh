#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Convenience entry point; reusable tooling lives in tools/.
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec "$project_dir/tools/upscale-movies-local.sh" "$@"
