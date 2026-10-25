<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

REminiscence2 1.30 adds live display controls, built-in xBRZ and original demo
playback from the QoL menu. This preview combines the changes since 1.25 with
the previous release's Sega CD support and fixes.

## New since 1.25

- **Graphics:** choose windowed, borderless windowed or exclusive fullscreen;
  select a window size or a fullscreen resolution reported by the monitor.
  Unsupported fullscreen changes retain the previous settings. Old fullscreen
  profiles remain borderless, and obsolete startup resolutions fall back safely.
- **Pixel presentation:** Point, Linear, ScaleNx and now xBRZ at 2×–6×.
  Integer scaling fits whole multiples of the original pixels with borders;
  select Point for unaltered pixels. Fixed window dimensions survive filter
  changes. Point sampling remains sharp after renderer resets.
- **Session → Gameplay demos:** watch available Jungle, Alien World and Death
  Tower recordings from the selected DOS/GOG or Macintosh edition. These are
  original attract-mode replays, not additional levels. Esc returns to the title;
  F10 opens QoL. Confirm before replacing a running game; demos cannot save or
  overwrite progress. Editions without compatible recordings explain why.
- **Demo robustness:** cover the historical missing-map starting-room case,
  return recorded death to the title, and reject oversized or incomplete loose
  recordings without losing the previous session.
- **Offline movie restoration tools:** export Sega CD movies through the engine
  decoder, upscale with Real-CUGAN Pro, preserve original frame holds and audio,
  and validate the result with source/model hashes. The tooling guide includes
  the 3× / denoise 3 workflow and optional tracked-region sign repair. Models,
  restored movies and commercial assets are supplied separately. The game still
  plays the original MCD movies; this is an offline viewing/export workflow.

Open QoL with **Esc or F10** during gameplay. Both desktop and original-resolution
layouts include the new controls. **Ctrl+S / Ctrl+L** quick-save/load the selected
manual slot in the current level; controller bindings are configurable in Joypad.
Save settings in the menu to retain preferences between launches.

## Retained from 1.25: Sega CD and original assets

- Play the supported US Sega CD assets: seven level resource sets, title
  graphics, 70 MCD movies with embedded audio, VCE voice dialogue and eight
  lossless music tracks.
- Extract your own CUE/BIN with the standard-library Python tool. Browse and
  play movies with `tools/play_cutscenes.py`. Asset and binary-format references
  document required files, extraction and engine support.
- Select the CD soundtrack independently of Amiga or GOG/DOS graphics in QoL
  Audio. Imported game files stay untouched.
- Sega CD supports Conrad's undershirt choices and alternative pistol styles
  in Enhancements. The jacket retains its native colours. Effects follow the
  original muzzle position and firing height; gameplay remains unchanged.
- Title navigation fixes prevent excessive Amiga repeats and missed DOS taps.
  Mouse wheel/click navigation and left-click cutscene skipping are available.
- Movie playback handles focus/background pauses; WAV failure handling,
  extraction bounds and CD object-bank/muzzle lookup fixes are retained.

All 70 extracted movies matched an independent disc extraction, and all 17,754
indexed frames matched the original game's 68000 decompressor in the 1.25 audit.
The checkerboard texture belongs to the original 256×128, 16-colour dithering.

## Downloads and validation

Packages cover **Windows x64, Linux x64, Steam Deck, macOS Apple Silicon,
macOS Intel and Quest arm64**. Quest includes a signed APK and Linux/ADB
deployment bundle. Desktop packages provide launchers and a precreated `assets`
folder; supply your own original game files. The corresponding-source archive
includes matching SDL2/zlib sources. Packages retain xBRZ and other dependency
notices. `SHA256SUMS` covers the eight package/source downloads.

All **31 local Release suites**, **31 Debug/UBSan suites** and **11 packaging-tool
tests** passed. Private tests use supplied Amiga, GOG/DOS and Sega CD assets;
public regressions run without original game files. Publication requires the
normal Build and test workflow and all six package builds to pass for the same
tagged source commit.

This remains a preview. Full playthroughs, physical-display mode switching,
physical Deck/Quest testing, console A/V timing comparisons and other Sega CD
pressings remain outstanding. Macintosh compatibility needs real assets.
macOS apps are ad-hoc signed, not notarized. Quest runs as a flat Android panel;
its host controls window size and mode.

## Screenshots

The new Graphics captures use the production overlay with synthetic session
data: the first at 1280×800, the second at the native 256×224 size.

![Graphics with borderless mode and xBRZ](https://raw.githubusercontent.com/b0realis/REminiscence2/v1.30/docs/screenshots/qol-graphics-1.30.png)

![Original-resolution Graphics menu](https://raw.githubusercontent.com/b0realis/REminiscence2/v1.30/docs/screenshots/qol-native-graphics-1.30.png)

<details>
<summary>Sega CD gameplay and the keyboard guide retained from 1.25</summary>

Real Sega CD gameplay enlarged with nearest-neighbour sampling, and the
production keyboard action guide. These captures retain their original version.
Original game artwork belongs to its respective rights holders.

![Sega CD gameplay](https://raw.githubusercontent.com/b0realis/REminiscence2/v1.30/docs/screenshots/segacd-gameplay-1.25.png)

![Interactive keyboard action guide](https://raw.githubusercontent.com/b0realis/REminiscence2/v1.30/docs/screenshots/qol-keyboard-1.25.png)

</details>
