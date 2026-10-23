# REminiscence2 modernization roadmap

REminiscence2 should make Flashback easy to play on current computers while
preserving its animation, collision, pacing, puzzles, music and atmosphere.
The original experience remains the reference. Improvements that change play
or presentation should be explicit options.

## Foundation

The initial development pass imports the selected repository with its history,
adds CMake and asset-free regression tests, fixes confirmed startup defects,
and records architecture, credits and license provenance. Local Apple Silicon
Release and UBSan Debug tests pass. Linux and macOS CI is configured; hosted
results remain to be collected.

The [0.5.6 integration ledger](UPSTREAM_056.md) records the approved enhancement
implementation. Amiga, GOG DOS/Windows and Macintosh CD are asset targets;
new decoders, music backends and synthetic Macintosh menu checks are present.
The private Amiga regression covers seven levels and all 419 valid rooms.

Before declaring edition validation complete, add a repeatable gameplay reference
using original DOS data: introduction, title menu, one level transition, a
manual save and restore, death and restart, and rewind. Record the edition,
language, file hashes and settings, without committing commercial game files.

## Reliable startup and progress

Add a first-launch game-folder picker and a clear description of missing or
unsupported data. Put saves, configuration and screenshots in standard user
locations, with an explicit portable mode and migration from legacy folders.

Harden the file, archive and memory stream boundaries. Add atomic manual saves,
validated loading, persistent autosave, and recovery after an interrupted
write. Keep the current rewind ring as a separate feature. Preserve existing
save compatibility where it can be demonstrated; provide import and backup
behavior before changing a format.

Acceptance: startup succeeds from any working directory, unwritable paths
produce useful errors, corrupt saves leave the current game intact, and a
restart can recover the last valid persistent autosave.

## Input and accessibility

Introduce named actions and persistent keyboard/controller bindings. Support
controller hotplug, dead-zone adjustment, consistent menu navigation, and
input reset after focus loss or device disconnect. Add readable help and
configurable audio levels before considering features that alter game rules.

Acceptance: a keyboard or common controller can complete startup, gameplay,
inventory, saving, loading and quitting. Reconnecting a controller or switching
applications cannot leave an action held. Original bindings remain available.

## Current platforms and presentation

Port the SDL boundary to SDL3 after repeatable behavior checks exist. Add
high-DPI output, predictable resizing and integer scaling, display selection,
renderer fallback, and controlled audio latency. Preserve simulation timing
independently of 60, 120 or 144 Hz display refresh.

Ship an Apple Silicon macOS application, then validate Intel macOS, Windows
x64 and Linux x64/arm64 builds. Add Windows ARM and Steam Deck validation as
hardware becomes available. Choose and document the minimum OS versions only
after compiling and running on representative targets.

Acceptance: installable packages find writable paths, retain sharp original
pixels as an option, handle sleep and display changes, and pass the same
recorded gameplay scenarios across tested systems. Sign or notarize release
packages when the required signing identity is available.

## Optional quality of life

Prioritize a visible save browser, continue-last-game, configurable rewind,
clear screenshot controls, remembered display settings and useful in-game
help. Keep restored cutscenes and visual filters optional. Consider additional
languages, assist options and user-supplied music only with clear provenance
and tests for the affected edition.

Acceptance: settings persist and can be reset, save slots are identifiable,
and each enhancement has a documented default and compatibility impact.

## Release requirements

- Preserve credits, copyright notices, source provenance and dependency licenses.
- Confirm the later-version permission before labeling the whole inherited
  distribution `GPL-3.0-or-later`; new contributions already use that grant.
- Distribute engine code and permitted assets with corresponding source and
  build instructions. Keep original commercial game files separate.
- Pass automated checks and the manual playthrough matrix for every platform
  and edition claimed in the release notes.
- Publish precise support and save-compatibility statements based on those runs.

Milestones describe order and acceptance criteria, not delivery dates. Extend
platform claims only when the corresponding build and runtime checks exist.
