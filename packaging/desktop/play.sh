#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
PACKAGE_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec "$PACKAGE_DIR/reminiscence2" --datapath="$PACKAGE_DIR/assets" "$@"
