#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
editor="$project_dir/build/package-arm64/reminiscence2-editor"
if [ ! -x "$editor" ]; then
    printf 'Build the editor first:\n  cmake --build "%s/build/package-arm64" --target reminiscence2-editor -j 8\n' "$project_dir" >&2
    exit 1
fi
# Alien-starship continuation; keep the earlier Relay Station demonstration too.
if [ "${1-}" = "--starship" ]; then
    shift
    demo="$project_dir/.local/editor-projects/AfterTheDrift-Classic"
    if [ ! -f "$demo/campaign.r2campaign.json" ]; then
        if [ ! -f "$project_dir/examples/editor/alien-starship/campaign.r2campaign.json" ]; then
            printf 'This private demo is not included. Open your own project in the editor.\n' >&2
            exit 1
        fi
        mkdir -p "$demo"
        cp -R "$project_dir/examples/editor/alien-starship/." "$demo/"
    fi
    exec "$editor" "$demo/project.r2project.json" --assets="$project_dir/.local/assets" --campaign="$demo/campaign.r2campaign.json" "$@"
fi
# Work on a private copy of the new demonstration campaign.
if [ "${1-}" = "--demo" ]; then
    shift
    demo="$project_dir/.local/editor-projects/RelayStation"
    if [ ! -f "$demo/campaign.r2campaign.json" ]; then
        if [ ! -f "$project_dir/examples/editor/relay-station/campaign.r2campaign.json" ]; then
            printf 'This private demo is not included. Open your own project in the editor.\n' >&2
            exit 1
        fi
        mkdir -p "$demo"
        cp -R "$project_dir/examples/editor/relay-station/." "$demo/"
    fi
    exec "$editor" "$demo/levels/dock/project.r2project.json" --assets="$project_dir/.local/assets" --campaign="$demo/campaign.r2campaign.json" "$@"
fi
# A private example for the top-level campaign editor.
if [ "${1-}" = "--campaign-editor" ]; then
    shift
    campaign="$project_dir/.local/editor-projects/CampaignRoutes"
    if [ ! -f "$campaign/campaign.r2campaign.json" ]; then
        mkdir -p "$campaign"
        cp -R "$project_dir/examples/editor/campaign/." "$campaign/"
    fi
    exec "$editor" --assets="$project_dir/.local/assets" --campaign="$campaign/campaign.r2campaign.json" "$@"
fi
# A complete native campaign is prepared only once; future runs keep edits.
if [ "${1-}" = "--campaign" ]; then
    shift
    campaign="$project_dir/.local/editor-projects/NativeCampaign"
    if [ ! -f "$campaign/level-1.r2project.json" ]; then
        "$editor" --assets="$project_dir/.local/assets" --export-native-campaign="$campaign"
    fi
    exec "$editor" --assets="$project_dir/.local/assets" "$campaign/level-1.r2project.json" "$@"
fi
# Work on a private copy; the redistributable example remains a clean template.
template="workshop"
name="Workshop"
if [ "${1-}" = "--modules" ]; then
    template="module-lab"
    name="ModuleLab"
    shift
fi
workshop="$project_dir/.local/editor-projects/$name"
if [ ! -f "$workshop/project.r2project.json" ]; then
    mkdir -p "$workshop"
    cp -R "$project_dir/examples/editor/$template/." "$workshop/"
fi
# The editor accepts a later positional project as an override; option-only
# invocations still open the Workshop, even when launched from another folder.
set -- "$workshop/project.r2project.json" "$@"
exec "$editor" --assets="$project_dir/.local/assets" "$@"
