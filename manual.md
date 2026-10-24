<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# REminiscence2 — player field guide

```text
  R E m i n i s c e n c e 2
  =======================
  ORIGINAL DATA / MODERN ENGINE / ONE MORE TRY
```

**Guide:** current engine · **Interface:** command line + game window

Welcome back. REminiscence2 runs Flashback with the game files you supply.
This guide gets you from an asset folder to a first session, then puts the
controls, saves and presentation settings within reach.

This is original REminiscence2 documentation. It contains no reproduced game
art, comic pages, screenshots, dialogue or publisher-manual passages.

[Start playing](#start-playing) · [Prepare your data](#prepare-your-data) ·
[Controls](#controls) · [Movement practice](#movement-practice) ·
[In-game settings](#in-game-settings) ·
[Story journal](#story-journal) · [Keep your progress](#keep-your-progress) · [Picture and sound](#picture-and-sound) ·
[Cutscene player](#cutscene-player) · [Build](#build) ·
[Command reference](#command-reference) · [Configuration](#configuration) ·
[Troubleshooting](#troubleshooting) · [Credits and license](#credits-and-license)

## Start playing

You need a compiled `reminiscence2` executable and one edition's extracted
Flashback data. The engine and its tools do not include the commercial game.
For build instructions, jump to [Build](#build).

From the project directory, check the executable:

```sh
./build/release/reminiscence2 --version
./build/release/reminiscence2 -h
```

Both `-h` and `--help` show the complete built-in guide. They work before you
have prepared any assets and do not open graphics or audio devices.

Start with a window and keep your saves separate from the game files:

```sh
./build/release/reminiscence2 \
  --datapath="/path/to/Flashback/assets" \
  --tunepath="/path/to/Flashback/assets" \
  --savepath=SAVE --window
```

The music path in this example also searches your asset folder's subdirectories.
If your music lives elsewhere, point `--tunepath` to that folder instead.
Use the title menu to start a game. Arrow keys move through menus; Enter confirms.

> **First-session kit:** arrows to move, Shift for actions, Space for the gun,
> Enter to use an item, Tab for inventory, Ctrl+S to save, Esc or F10 for settings.

For rewind during a session, add `--autosave`. It keeps temporary history in
memory; [manual saves](#keep-your-progress) preserve progress after you quit.

## Prepare your data

Keep each edition in its own folder. REminiscence2 searches below the selected
folder and detects the edition from the files it recognizes.

| Data set | What to supply | Current evidence |
| --- | --- | --- |
| Amiga | Unpacked game files, including cutscenes and fonts; use the WHDLoad preparation tool below when needed. | Supplied assets pass seven-level rendering/save checks and all-parts cutscene playback. |
| DOS / classic DOS files from GOG | The original data directory, with its resource files or supported archives. | Loaders are present; real GOG data validation is pending. |
| Macintosh / Macintosh CD target | A supported `FLASHBACK.BIN` or `FLASHBACK.RSRC` containing the resource fork. | Synthetic resource tests pass; original Macintosh CD data still needs validation. |
| PC-98 | The edition's supported PAQ data. | Format support is present; real-data runtime checks remain. |
| Sega Mega Drive | Extracted engine resources. | Experimental; ROM extraction and Sega audio are not implemented. |
| 3DO | No supported data path in this build. | Research target only. |

An installer, WHDLoad slave, disk image, ROM image or original program executable
is not a ready-to-use asset directory. Selecting a language does not install
missing translations. Supply the relevant data yourself.

### Amiga WHDLoad preparation

`tools/extract_amiga.py` opens a WHDLoad LHA package and unpacks its inner RNC
files. It leaves the input archive untouched and refuses an existing output
folder. Python 3.9+, `lhafile` and the Ancient decompressor are required.

On macOS, set up the tool once:

```sh
python3 -m venv .local/extract-venv
.local/extract-venv/bin/python -m pip install -r tools/requirements-amiga.txt
brew install ancient
```

Then prepare an archive you own into a new directory:

```sh
.local/extract-venv/bin/python tools/extract_amiga.py \
  "/path/to/Flashback_v3.4_1163.lha" .local/assets/amiga-ready

./build/release/reminiscence2 \
  --datapath=.local/assets/amiga-ready \
  --tunepath=.local/assets/amiga-ready --window
```

The output preserves the useful directory hierarchy, omits the Amiga executable
and WHDLoad support files, and records file sizes and hashes in
`reminiscence2-extraction.json`. LHA and RNC checksums are checked before a
complete output directory is published.

Use `--ancient=PATH` to select a decompressor explicitly, `--verbose` for
per-file progress, or `--help` for all setup details. No separate `lha` command
is necessary. The supplied archive produces 128 game files; this count is
specific to that package, not a requirement for every edition.

### Folders and configuration

Relative paths start at your terminal's current directory. Quote paths with
spaces. Normal game launches use these defaults:

| Purpose | Default | Change it with |
| --- | --- | --- |
| Game resources | `assets` beside the executable (legacy `DATA` fallback) | `--datapath=PATH` |
| Music resources | Game folder or discovered `TUNES` | `--tunepath=PATH` |
| Persistent saves | User-data `REminiscence2/SAVE` | `--savepath=PATH` |
| Compatibility settings | `fb.cfg` | `--config=PATH` |

The game creates the save directory and its missing parents. Optional tune
folders are read-only. Saves default to `%APPDATA%/REminiscence2/SAVE` on Windows,
`~/Library/Application Support/REminiscence2/SAVE` on macOS, and
`$XDG_DATA_HOME/REminiscence2/SAVE` (or `~/.local/share/REminiscence2/SAVE`) on Linux.
Use `--portable` for `assets`, `TUNES` and `SAVE` beside the executable; explicit
path switches take priority. Keep saves writable. The default `fb.cfg` is optional;
an explicitly requested configuration file must exist and be readable.

A single existing folder passed as the **only argument** is a convenience mode
that uses the folder for data and music, retaining the normal save location. Use the named switches when you
want to separate these locations. A launch from outside the project might be:

```sh
/path/to/reminiscence2 \
  --config="/path/to/REminiscence2/fb.cfg" \
  --datapath="/games/Flashback/Amiga" \
  --tunepath="/games/Flashback/Amiga" \
  --savepath="/games/Flashback/Saves" --window
```

## Controls

These are the default bindings; the overlay can rebind the primary keyboard
actions. Actions depend on whether Conrad
is standing, crouching, holding an item or carrying the weapon.

```text
                   [ Up ]                  [ Shift ] action / run / fire
         [ Left ] [ Down ] [ Right ]       [ Space ] draw / holster
                                           [ Enter ] use / confirm
                                           [ Tab ]   inventory
```

| Key | During play or in menus |
| --- | --- |
| Arrow keys | Movement and menu navigation; keypad 4/6/8/2 also give directions. |
| Shift | Context action; interact nearby, run with a direction, or fire when armed. |
| Space | Draw or put away the weapon. |
| Enter / Return | Use the active inventory item; confirm a menu choice. |
| Tab / Backspace | Open or close inventory. Highlight an item before closing to select it. |
| Enter in inventory | Switch between the inventory and score/difficulty information. |
| Escape | Open the in-game settings overlay or leave a menu. |
| F10 | Fixed QoL settings shortcut, including after rebinding Pause. |
| Menu [F10] button | Click the bottom-right shortcut during gameplay to open QoL settings. |
| Ctrl+S / Ctrl+L | Save / load the current manual slot. |
| Ctrl+PageUp / Ctrl+PageDown | Next / previous manual slot, from 1 to 99. Ctrl+keypad Plus/Minus are alternatives. |
| Ctrl+R | Step back through available rewind history. |
| Alt+Enter | Switch between fullscreen and a window. |
| Alt+PageUp / Alt+PageDown | Increase / decrease a supported scale factor. Alt+keypad Plus/Minus also work. |
| F1 / F2 / F3 | Choose point / linear / ScaleNx display sampling. |
| Alt+S | Write a BMP screenshot in the working directory. |
| Alt+X | Exit the engine. |

The settings overlay supports mouse controls, Tab / Shift+Tab focus, arrow-key
adjustment and Enter / Space activation. On the Continue screen, choose with
Up/Down and confirm with Enter.

For SDL-recognized controllers, A uses/confirms, B draws the weapon, X performs
actions and Y opens inventory. The D-pad and left stick provide directions;
Start always opens settings (Back does too unless assigned to another action).
Open **QoL → Joypad** to rebind gameplay buttons, assign optional quick save,
quick load and rewind, adjust the stick dead zone (5–50%), or enable right-stick
movement. Start remains a settings shortcut after remapping. Quick save/load/
rewind are unassigned by default to avoid accidental actions.

Keyboard, D-pad, hats and sticks can be used together. Releasing one source does
not cancel another held source. Reconnect and focus changes release stale input.
The default dead zone is 15%, with a small release margin to reduce stick jitter.
Unmapped USB joysticks have a separate raw-button profile on the same page;
button numbers in that profile start at 1. In menus, raw button 3 confirms,
buttons 1/10 go back, and 5/6 change pages. Mapped controller menus use A/B and
shoulders, independently of gameplay remapping. Physical Deck/Quest/pad checks
remain pending.

## In-game settings

**[MENU · F10 — open the QoL guide](docs/SETTINGS.md)**

Press **Escape**, **F10** or controller **Start** while playing, or click the
bottom-right **Menu [F10]** button. The overlay pauses gameplay while music can
continue for volume adjustments. Move the mouse to reveal the cursor.

| Page | What you can change or do |
| --- | --- |
| Session | Continue latest autosave, set disk autosave interval and optional contextual hints; view health/score/rewind count, rewind, capture a clean screenshot, restart, return to title, save/undo/reset preferences, and show or hide the shortcut. |
| Rewind | Preview captured moments, their age, room and health, then restore the selected point. |
| Save / Load | Browse all levels, previews and names; protect milestones, save/load manual slots 1–99 and restore checkpoint 00. |
| Graphics | Fullscreen, VSync, point/linear/ScaleNx filtering, window scale, side content, compact layout and menu text size. |
| Audio | Separate master/music/effects levels, mute and save-confirmation sound. |
| Enhancements | Conrad’s Original/Red-Pink/White T-shirt, rewind, inventory ordering, transitions and optional cutscene choices. |
| Controls | Rebind the nine primary keyboard actions and review fixed shortcuts. |
| Cheats | All three gameplay cheat flags, unlimited life, fast mode, diagnostic rectangles, DOS zoom, one-shot healing and clear-all. |
| Joypad | Remap controller/raw buttons and mapped triggers, configure sticks, save a device profile and assign save/load/rewind shortcuts. |
| Journal | Read optional companion passages for reached chapters and completed memory scenes; control notifications. |
| Objectives | Read reached-chapter goals and open gentle hints; future chapters stay hidden. |
| Diagnostics | Inspect asset presence, save location, controller profile, audio status and measured disk-write durations. |
| Levels | Choose any available level and Easy/Normal/Expert difficulty. Starting resets current progress, score, inventory and rewind after confirmation. |

Mouse, Tab/Shift+Tab, arrows and Enter/Space navigate the overlay. On a gamepad,
use D-pad/stick, A to confirm, B to close/cancel and shoulders to change pages.
Escape/F10/Start resumes play. On the death recovery screen, choose a recovery point or restart instead. A prominent turquoise **EXIT GAME** button is
always visible and asks for confirmation; Escape/B cancels that confirmation.

At the original **256×224** resolution the menu uses a single-column layout with
a page dropdown, previous/next page buttons, scrolling content and a fixed
Resume/EXIT GAME footer. It keeps your gameplay window size. Graphics → Menu
layout also enables Compact on larger displays. The tiny-window font is drawn
at 11 pixels, with text enlargement capped at 125% to keep navigation visible.

**Save settings** stores `preferences.cfg` under `--savepath`, separately from
**Save game**. On Session, **Undo settings changes** returns preferences to their
opening values; **Restore default settings** resets built-in choices. The saved
profile changes only when you choose Save settings. Cheats, selected levels and
gameplay actions are session controls and are not stored in that profile.

Save / Load browses all seven levels. Loading validates the saved state and actual
level assets before switching. The checkpoint (00) is load-only here. Manual
slots are 1–99; saving requires the current playing level and a living Conrad.
Expand Name / milestone protection to name a save or lock it against overwrite.
Existing saves require Update to apply changed metadata. Unlock and Update
before replacing a protected milestone; quicksave respects this protection too.
Successful loading or rewind resumes play; saving leaves the overlay open.

Capture clean screenshot writes the game frame without the overlay, at its
original resolution, as a uniquely named BMP in the save directory. Rewind count
shows how many session snapshots remain. One-shot healing restores the level's
initial health while Conrad is alive. Cheats do not prevent every scripted death.

To choose Conrad's appearance, open **Enhancements → Conrad's T-shirt**. Select
**Original / asset colours**, **Red / Pink** or **White**. It changes immediately
while paused; choose **Save settings** to retain your choice. Original uses the
exact colours from your game edition. **Only the T-shirt inside the jacket
changes.** The jacket retains its original brown colours; shoes, equipment,
other characters, scenery, skin, hair and cutscene/movie artwork keep theirs. The old `use_white_tshirt=true` config
option selects White unless a saved player preference overrides it.

At startup, saved menu preferences override their corresponding `fb.cfg`
settings. Explicit display/rewind command-line options override the saved
profile. Use `--no-preferences` to ignore that profile for one launch. Paths,
language, output rate and synthesizer selection remain startup options.

The Controls page includes an illustrated keyboard: action keys are colored,
other keys stay dark, and matching arrows point to each action's description.
The highlights follow your current bindings, including saved profiles and
changes made with Reset, Undo or Restore defaults. At small resolutions, colored
key caps and arrows form a scrollable action guide.

To rebind a key, open Controls, select a colored key, its callout or its binding
button and press the new key. Escape
cancels capture; duplicate and reserved keys are rejected. F10 remains a fixed
recovery shortcut. Menu inputs are cleared before resuming so confirming a
setting does not fire or move Conrad. Gamepad shoulders switch pages, A
confirms and B closes. See the [settings guide](docs/SETTINGS.md).

### Recovery and optional assists

After a death scene, recovery offers living rewind moments, checkpoint, retained
autosaves and manual saves together. High resolution shows preview cards; the
native menu places the three main recovery buttons before expandable previews.
Escape/F10/Start keep this screen open. Select a recovery point, Restart level,
Return to title or the turquoise EXIT GAME button. Failed loads keep progress.

Enhancements provides Sticky Action (also affects firing), a 100 ms Up/Use/Weapon
tap buffer, reduced camera shake, a steady shield-hit sprite and slower gameplay
at 50–100%. Assists default off and speed defaults to 100%. The shield-hit option
keeps original invulnerability timing; it does not filter movie/cutscene artwork.
Menu text scaling remains under Graphics. Pause, focus loss and disconnect clear
sticky/buffered input. Save settings keeps these preferences.

Unplugging a pad during play opens a blocking pause. Reconnect or use the keyboard,
then Resume explicitly. Save settings stores pad/stick bindings for that SDL GUID.
Missing audio output uses a silent mixer clock so voiced dialogue still finishes;
reconnecting retries sound without changing the mixer rate.

The optional Objectives page is a companion guide, with reached-chapter goals
and hints you open deliberately. Its knowledge follows the journal in saves and
rewind. It does not mark individual puzzles complete automatically. See the
[complete feature guide](docs/QOL_1.10.md) for profiles, formats and diagnostics.

## Movement practice

Find a quiet stretch of floor and learn how one action leads into the next.
A tap starts an action; a held key can maintain it across several animation
frames. Position and facing matter as much as the key combination.

| Try this | Input pattern |
| --- | --- |
| Walk, stop and turn | Use Left/Right without Shift; release the direction before changing your plan. |
| Run | Hold Shift with Left or Right. |
| Jump from rest | With the weapon away, hold Shift and tap Up. |
| Jump out of a run | Begin running, then tap Up while maintaining Shift. |
| Reach a ledge above | Position beneath it and hold Up; finish the pull-up before starting another action. |
| Descend at an edge | Put the weapon away, face the edge and use Shift+Down. |
| Use an elevator | Stand on its platform and hold Shift with Up or Down. |
| Roll from a crouch | Hold Down, add Left or Right, then release Down while keeping the direction. |
| Fire | Draw with Space, face the target, then use Shift. |

A movement that works from standing may behave differently with the weapon
drawn or while an animation is in progress. Practice away from drops and
hazards, and use a manual save before experimenting.

### Inventory and interactions

Open inventory with Tab or Backspace, move the highlight, then close inventory
to make that item active. Enter attempts to use it in the current situation.
Shift handles nearby context actions such as collecting or operating things.
If an action does nothing, check your position, facing and selected object.

Read the messages your own game data presents. This guide leaves story events,
puzzle solutions and secret routes for you to discover.

## Story journal

Open **QoL → Journal** to read short, original companion passages. Seven chapter
entries appear only when you play those chapters. A further entry follows a
completed memory sequence. Skipping to a level does not reveal the chapters
between. Future titles and passages remain hidden.

A small **Journal: new entry** notice appears while an entry is unread. Opening
an entry marks it read. The journal never interrupts a jump, fight or cutscene;
its text can be read in the paused overlay. The page works at 256×224 and larger
resolutions. Both the journal display and notification can be disabled, and the
choices can be saved as preferences. Save/load and rewind restore the journal's
unlocked/read state along with your progress. This is project-written companion
prose, with no copied dialogue, comic panels or manual passages.

## Keep your progress

There are several kinds of progress recovery. Choose one deliberately:

| Mechanism | What it remembers | Survives quitting? |
| --- | --- | --- |
| Manual save, slots 1–99 | A game state for the current level and slot. | Yes, when the save succeeds. |
| In-game checkpoint, slot 0 | A checkpoint state for its level. | Stored in the save directory. |
| Persistent autosaves | Three recent generations per edition, including a frame preview. | Yes, when a write succeeds. |
| Rewind through `--autosave` | Recent snapshots from this running session. | No. |

Manual slot 1 is selected initially. Ctrl+S writes it; Ctrl+L loads it. Check the
slot shown on the Save / Load page before overwriting progress you want to keep.
New save filenames identify edition, level and slot, such as
`reminiscence2-0-level1-manual-1.r2m` (edition 0 is Amiga; 1 DOS, 2 Macintosh, 3 PC-98, 4 Sega).
Version 4 saves also validate the edition, level, entity count and a gameplay-data
fingerprint before restoring. They retain the random seed and journal state.
Existing `.state` files (v2/v3/v4) remain readable as a fallback. V2/v3 produce a warning because
those formats cannot identify their source assets. Keep legacy saves with their
matching edition. A manual load clears the old rewind branch.

After restarting, open Save / Load, browse the saved level and select its slot;
loading switches levels automatically. New `.r2m` bundles atomically include a
preview, optional ASCII name and overwrite protection. A modern bundle takes
precedence over a legacy file in the same level/slot. Manual milestones remain
outside the three-file automatic rotation. A password starts a level rather
than restoring a saved position. Keep editions in separate save directories.

With `--autosave`, the engine records roughly one in-memory snapshot every five
seconds while Conrad is alive, up to 120 entries. Ctrl+R restores an older one.
History resets on a level change and disappears on exit. Recovery after death
offers available rewind history, checkpoints, manual saves, autosaves and a level restart. Keep manual saves for milestones you want to preserve.

### Autosaves and Continue

Persistent autosaves are enabled by default. They keep **three generations** per
asset edition in the save directory, named `reminiscence2-0-auto-0.r2s` through
`reminiscence2-0-auto-2.r2s` for Amiga (other editions use their numeric ID).
These files are independent of checkpoint slot 00 and manual slots 01–99.

The engine saves the first living gameplay frame, then every **60 seconds of
active play**, after a checkpoint/level change, and on clean exit when progress
changed and the state can be saved. Pauses, inventory screens and cutscenes do
not consume the interval. Dead or transitioning frames wait until gameplay is
coherent. A living frame can still put Conrad near danger; manual saves remain
useful. Forced termination can lose progress since the last successful write.

On the next launch, the welcome screen offers **Continue latest autosave**,
**New game** and **EXIT GAME**. Continue restores the newest compatible
candidate, changing levels automatically; it skips a damaged file or a different
asset revision and tries an older generation. The command-line `--continue`
restores directly and skips opening/title screens. It reports failure if no
candidate can be restored. New game opens the original title/level menu.

During play, **Session → Continue latest autosave** asks before replacing the
current game. **Save / Load → Persistent autosaves** has a generation selector,
preview, level, room, health and UTC date; generation 0 is the newest retained
candidate. Loading a selected generation checks its state and matching assets
before changing the game. A checksum detects damaged files; atomic replacement
keeps other generations available if a write fails. Failures are reported in the
HUD and terminal and do not touch manual saves.

Set **Session → Persistent autosaves / Autosave interval**, or launch with
`--disk-autosave=SECONDS` (15–600). `--no-disk-autosave` disables writes while
retained autosaves remain available to Continue. These command-line switches
override the saved profile for this launch; choose **Save settings** to retain
the new choice. The separate `--autosave` flag still enables in-memory rewind.

### Preview a rewind point

Enable **Session → Rewind history** or launch with `--autosave`. The **Rewind**
page lists up to 120 moments, captured about every five seconds while alive.
Select a point to see its captured frame, age in active seconds, room and health,
then choose **Restore selected moment** and confirm. The engine restores the
state, random seed and journal together. The selected moment and all newer
snapshots are removed; earlier history remains. Ctrl+R immediately restores the
latest available point. A manual/autosave load or level change clears history;
rewind never survives an exit. Rewind intervals count active gameplay; inventory and dialogue pauses do not
consume the five-second interval. Escape, F10 or Start can leave inventory for
the QoL overlay. Original-resolution menus use a smaller preview
and scrolling content, with the Resume/EXIT GAME footer always available.

### Contextual control reminders

Enable **Session → Contextual control hints** to see brief reminders about
running, nearby objects, inventory and low shield. They are off by default,
contain original project-written text and reveal no puzzle solutions. Key labels
and pad button names (numbered buttons for raw joysticks) follow your bindings, including changes made during
play. Each reminder appears once per new game, remains for eight active seconds,
and has a ten-second gap before another reminder. Pauses stop those timers.
Rewinding does not repeatedly show reminders already seen. Toggle hints off and
on to reset the reminders in the current session.

## Picture and sound

### Pick a display style

Without saved preferences the default launch is fullscreen. `--window` opens a resizable window;
`--maximized` starts one maximized. The default scaler is `scale@3`.

| Choice | Result |
| --- | --- |
| `--scaler=point@3` | Nearest-neighbor sampling with sharp pixel edges. |
| `--scaler=linear@3` | Smooth sampling. |
| `--scaler=scale@3` | ScaleNx edge-aware enlargement; supported factors are 2–4. |

The suffix controls the initial size multiplier. Without a suffix the default
factor is 3. Macintosh rendering keeps its native 512×448 resolution; the
base playfield for other supported editions is 256×224. External scaler names
require matching plugins or a build that includes them; default CMake builds
do not include xBR.

### Fill a wide display

`--widescreen` adds side content while retaining the game's room boundaries
and collisions:

| Value | Side content |
| --- | --- |
| `none` | No added side content; default. |
| `adjacent` | Backgrounds from neighboring rooms. |
| `adjacent-blur` | Blurred neighboring backgrounds. |
| `mirror` | Darkened mirrored edges of the current room. |
| `blur` | A blurred backdrop from the current room. |
| `cdi` | Optional decorative borders supplied by the player as `flashp1.bob` through `flashp7.bob`. Missing borders leave empty margins. |

### Music sources

Original or optional MOD/OGG music belongs in the directory selected by
`--tunepath`. DOS PRF/MIDI/INS files are also searched for in the game-data
folder. Available music formats and their priority depend on the edition and
files present.

`--mididriver` chooses the **PRF fallback** synthesizer:

| Driver | Requirements |
| --- | --- |
| `adlib` | Built-in OPL3 synthesis; default. No soundfont or ROM is needed. |
| `fluidsynth` | A build with FluidSynth enabled and your compatible General MIDI soundfont through `--soundfont=PATH`. MT-32 programs are mapped to GM. |
| `mt32` | A build with mt32emu enabled and compatible ROMs through `--rompath=PATH`. |
| `none` | Turns off PRF synthesis; other music and effects can still play. |

For MT-32, supply either `MT32_CONTROL.ROM` + `MT32_PCM.ROM` or
`CM32L_CONTROL.ROM` + `CM32L_PCM.ROM`. These files and soundfonts are not bundled.
`--version` lists the PRF backends enabled in your executable. Existing music
formats can take priority over the fallback.

`--outputrate=48000` requests 48 kHz audio. Supported requests range from 8,000
to 192,000 Hz; the device can negotiate another rate. This setting does not
change game speed.

## Cutscene player

The Python mini player turns your asset folder into a selectable catalog:

```sh
python3 tools/play_cutscenes.py /path/to/assets
python3 tools/play_cutscenes.py /path/to/assets --list
python3 tools/play_cutscenes.py /path/to/assets --play intro
python3 tools/play_cutscenes.py /path/to/assets --play chute:1
```

Python 3.9+ uses its standard library. A compiled REminiscence2 provides the
renderer, subtitles and audio. Select a number or name, then a part if asked.
Part numbering begins at zero. A finished part returns to the menu; `--play`
plays once and exits. Type `q` at the terminal menu to quit.

Backspace, Tab or Escape skips playback. Enter, Space and directions advance
interactive panels. Closing the playback window also returns to the menu.
Short or static selections are normal: the catalog includes original gameplay
panels as well as story sequences. The supplied Amiga set has 27 files and
68 selectable parts.

Music defaults to the chosen asset directory. `--tunepath`, `--language`,
`--fullscreen`, `--scale` and `--engine` customize the launch. `--json` provides
a machine-readable catalog; `--filter TEXT` searches names and descriptions.
Run the tool with `--help` for complete usage.

The player selects polygon resources. It does not list SEQ replacements,
standalone SET animations or 3DO video, and it does not assemble a complete
ending-credit sequence. See [the cutscene guide](docs/CUTSCENE_PLAYER.md).

## Build

Requirements: a C/C++17 toolchain, CMake 3.20+, SDL2 2.0.18+ development files, zlib and
libmodplug. Run the following commands from the repository root.

On macOS with Homebrew:

```sh
brew install cmake pkgconf sdl2 libmodplug
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --parallel
ctest --test-dir build/release --output-on-failure
```

On Debian or Ubuntu:

```sh
sudo apt-get install build-essential cmake pkg-config libsdl2-dev zlib1g-dev libmodplug-dev
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --parallel
ctest --test-dir build/release --output-on-failure
```

The main executable is `build/release/reminiscence2`. Development builds can
still depend on installed shared libraries; one executable does not mean all
libraries and game assets are embedded in it.

To use the internal MOD player, configure with
`-DREMINISCENCE2_USE_MODPLUG=OFF`. FluidSynth and MT-32 support are optional:
install their development libraries and configure with
`-DREMINISCENCE2_FLUIDSYNTH=ON` and/or `-DREMINISCENCE2_MT32EMU=ON`.

macOS and Linux build jobs are configured. Windows packaging and a native
SDL3 backend remain future work. See the [build notes](README.md#build) and
[validation record](docs/VALIDATION.md) for tested configurations.

## Command reference

Long options accept `--name=VALUE` or `--name VALUE`. For detailed explanations
and examples at any time, run `reminiscence2 -h`.

| Switch | Effect and default |
| --- | --- |
| `-h`, `--help` | Print the full guide and exit without requiring assets. |
| `--version` | Print version, credits and enabled PRF backends, then exit. |
| `--datapath=PATH` | Game folder; default `assets` beside the executable. |
| `--tunepath=PATH` | Music folder; defaults to the selected game folder. |
| `--savepath=PATH` | Persistent save folder; default platform user-data location. |
| `--portable` | Keep default assets/music/saves beside the executable. |
| `--config=PATH` | Compatibility configuration; default working-directory `fb.cfg`. |
| `--no-preferences` | Ignore the saved menu profile for this launch; retain it on disk. |
| `--window` | Start in a resizable window; default launch is fullscreen. |
| `--maximized` | Start maximized; implies a window. |
| `--scaler=NAME@X` | Display sampling and size multiplier; default `scale@3`. |
| `--widescreen=MODE` | Side content; default `none`. See [Picture and sound](#picture-and-sound). |
| `--levelnum=NUM` | Initial level, 0–6; default 0. Menus can change it, and opening sequences still play. |
| `--language=LANG` | `fr`, `en`, `de`, `sp`, `it`, `jp` or `ru`, ignoring case. Default: file detection, then English. |
| `--autosave` | Enable session-only rewind; default off. |
| `--continue` | Restore the latest compatible autosave directly, or report failure. |
| `--disk-autosave=SECONDS` | Enable disk autosaves every 15–600 active seconds; default 60. |
| `--no-disk-autosave` | Disable automatic disk writes; retained autosaves remain loadable. |
| `--cheats=MASK` | Gameplay flags described below; default 0. |
| `--outputrate=HZ` | Request 8,000–192,000 Hz audio; default 22050. |
| `--mididriver=NAME` | `adlib`, `fluidsynth`, `mt32` or `none`; default `adlib`. |
| `--soundfont=PATH` | General MIDI soundfont for FluidSynth; default none. |
| `--rompath=PATH` | MT-32 ROM directory; default working directory. |
| `--debug=MASK` | Diagnostic categories described below; default 1. |
| `--list-cutscenes` | Print a JSON catalog without the banner or SDL initialization. Creates no tune/save directories. |
| `--play-cutscene=NAME[:PART]` | Play one polygon part and exit; default part 0. Uses normal display, language and music settings. |

Listing and playback switches cannot be combined. A normal game launch prints
the REminiscence2 banner and version. Successful help, version, listing and
normal exits return status 0; startup or runtime errors return a nonzero status.

### Gameplay flags

`--cheats` adds these values: **1** for one-hit enemy kills, **2** to ignore
ordinary enemy hit messages to Conrad, and **4** to prevent ordinary life-counter
decreases. For example, `--cheats=3` combines the first two. Scripted deaths
and environmental hazards can still apply.

### Diagnostic flags

Add categories to combine them. `--debug=0` disables category logging, while
warnings, errors and normal startup text remain. `--debug=0x1040` is useful
when reporting resource selection or gameplay-loading problems.

| Value | Category | Value | Category |
| --- | --- | --- | --- |
| 1 | General | 2 | Resources |
| 4 | Menus | 8 | Unpacking |
| 16 | Object scripting | 32 | Video |
| 64 | Gameplay | 128 | Collision |
| 256 | Sound | 512 | Cutscenes |
| 1024 | MOD music | 2048 | Sound effects |
| 4096 | File access | 8192 | Demo replay |
| 16384 | PC-98 archives | | |

Both masks accept unsigned decimal or `0x` hexadecimal values up to 4294967295.
Unlisted bits have no effect. `--debug=32767` selects every category in the table.

## Configuration

`fb.cfg` uses one case-sensitive `name=value` setting per line. Boolean values
accept `true`/`false` or `1`/`0`, ignoring value case and surrounding whitespace.
Use a separate line starting with `#` for a comment. Unknown names or invalid
values produce a warning and leave the previous value in place.

The bundled example makes different choices from the built-in defaults:

| Setting | Built-in | Example `fb.cfg` | Purpose |
| --- | --- | --- | --- |
| `bypass_protection` | `true` | `true` | Skip the original protection prompt. |
| `enable_password_menu` | `true` | `false` | Choose the password interface instead of the level selector. |
| `enable_language_selection` | `false` | `true` | Allow title-menu language changes with Left/Right. |
| `fade_out_palette` | `true` | `true` | Fade transitions to black; false uses palette blending. |
| `use_tile_data` | `false` | `false` | Prefer DOS BNQ/LEV backgrounds over MAP data. |
| `use_text_cutscenes` | `false` | `false` | Substitute available text summaries for polygon scenes. |
| `use_seq_cutscenes` | `true` | `true` | Prefer available PC CD SEQ movies. |
| `use_words_protection` | `false` | `false` | Select the DOS word challenge when protection is enabled. |
| `use_white_tshirt` | `false` | `false` | Import the White gameplay-shirt preset; a saved QoL shirt choice overrides it. |
| `play_asc_cutscene` | `false` | `false` | Enable the optional ASC scene. |
| `play_caillou_cutscene` | `false` | `true` | Enable the optional CAILLOU-F.SET scene. |
| `play_carte_cutscene` | `false` | `true` | Enable the optional CARTE scene. |
| `play_metro_cutscene` | `false` | `true` | Enable the optional METRO scene. |
| `play_serrure_cutscene` | `false` | `true` | Enable the optional SERRURE scene. |
| `play_gamesaved_sound` | `false` | `true` | Play the save-confirmation sample. |
| `restore_memo_cutscene` | `true` | `true` | Restore additional MEMO content. |
| `order_inventory_original` | `false` | `false` | Put newly collected items at the bottom of inventory. |
| `fix_fmopl_e0_reg` | `false` | `false` | Correct the original AdLib waveform-register quirk. |
| `use_prf_music` | `true` | `true` | Permit PRF music as a fallback. |

A switch that needs edition-specific resources cannot supply missing assets.
To reproduce the original protection prompts, set `bypass_protection=false`
and use your own edition's documentation. This project does not distribute
challenge sheets or answers. `use_words_protection` selects the DOS word
variant; shape challenges use `false`.

## Troubleshooting

| Symptom | What to check |
| --- | --- |
| No supported data found | Point `--datapath` at extracted resources, not the installer, slave or archive. Check edition filenames and resource-fork preservation. |
| Amiga loading fails after LHA extraction | Inner files may still be RNC-packed. Run `tools/extract_amiga.py` into a new folder. |
| Help does not show cutscene options | Rebuild this tree; an older executable may be on PATH. Use the Python player's `--engine` override. |
| Configuration cannot be read | Check the explicit path and working directory. The filename is `fb.cfg` by default. |
| A save seems missing | Match the edition, save directory, current level and slot. A password and rewind history are different mechanisms. |
| Music is silent | Check `--tunepath`, the available music files and PRF fallback settings. Extracted Amiga music can remain below the asset folder. |
| A MIDI driver is unavailable | Read `--version`; optional synthesizers must be enabled at build time. |
| Soundfont or MT-32 startup fails | Check backend selection and the external file requirements in [Picture and sound](#picture-and-sound). |
| A cutscene seems to repeat | Some gameplay panels loop or wait for input. Try Enter/Space/directions, or Backspace/Tab/Escape in the mini player. |
| Screenshot output fails | Alt+S writes to the working directory; it must be writable. |
| Controller input differs | Try the keyboard bindings; adjust bindings/dead zones in QoL → Joypad; physical-device validation is pending. |

For a useful bug report, include the executable's `--version` output, asset
edition and language, launch command, relevant configuration, and a short
sequence that reproduces the issue. Include terminal diagnostics. Do not attach
commercial game files to a public issue.

## Credits and license

Thank you to **Delphine Software** for Flashback, **Gregory Montoir** for
REminiscence, **Alex Chermenin** for the selected fork, and the contributors
who kept the engine running. REminiscence2 is an independent community project;
references to the game and its creators identify compatibility and credit.
They do not assert affiliation or endorsement.

This original guide is distributed under **GNU GPL version 3 or any later
version**; see [LICENSE](LICENSE). This grant covers the new guide, not
Flashback's commercial assets, third-party documents, or other authors' work.
The engine and dependencies retain their own notices and license terms.

- [Project overview](README.md) and [credits](CREDITS.md).
- [Cutscene player guide](docs/CUTSCENE_PLAYER.md).
- [In-game settings guide](docs/SETTINGS.md).
- [Validation record](docs/VALIDATION.md) and [roadmap](docs/ROADMAP.md).
- [Documentation provenance](docs/manual/README.md).
- [Licensing and provenance](docs/LICENSING.md).
