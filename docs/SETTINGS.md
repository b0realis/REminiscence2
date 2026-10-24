<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# In-game settings

Press **Escape** or **F10**, press **Start** on an SDL controller, or click
the **Menu [F10]** shortcut at the bottom right during gameplay to open the
paused REminiscence2 overlay. Mouse, keyboard and SDL gamepad navigation are supported. The overlay
uses its own MIT-licensed UI font; it does not require a font or illustration
copied from the commercial manual.

Gameplay stops while the overlay is open. Music can continue so you can hear
volume adjustments. On resume, held menu inputs are cleared and simulation /
rewind timers restart from the current time. There is no catch-up simulation.

See the [screenshot gallery](screenshots/README.md) for the 1920×1080 desktop
menu, save browser, graphics settings and original-resolution 256×224 layout.

## Pages

| Page | Available controls |
| --- | --- |
| Session | Game edition selection; default-off automatic resume of the latest compatible save; Continue latest autosave, disk autosave settings and optional contextual hints; health, score, room and rewind count; rewind; clean screenshot capture; restart level; return to title; preference save/undo/defaults; focus pause, FPS and shortcut visibility. |
| Rewind | Captured-frame timeline with active age, room and health; restores the selected moment after confirmation. |
| Save / Load | Manual slots 1–99, occupied-slot browser with room and modification time, checkpoint loading, refresh, overwrite and load confirmation. |
| Graphics | Fullscreen, VSync, point/linear/ScaleNx sampling, window scale, widescreen side content, compact layout and menu text scale. |
| Audio | Independent soundtrack selection with native fallback; master, music and effects volume, mute, save-confirmation sound, current output rate and PRF fallback information. |
| Enhancements | Conrad’s Original/Red-Pink/White T-shirt, rewind, inventory insertion order, transition style, save sound, additional MEMO content, text/SEQ choices and optional cutscene switches. |
| Controls | Live keyboard diagram with colored action keys and arrow callouts; rebind the nine primary actions; view fixed shortcuts, menu navigation and controller bindings. |
| Cheats | All three gameplay cheat flags, unlimited life, fast mode, diagnostic rectangles, DOS combat zoom, one-shot health restoration and clear-all. |
| Joypad | Persistent controller/raw-button remapping, optional save/load/rewind shortcuts, 5–50% stick dead zone and right-stick movement. Start always opens settings. |
| Journal | Optional original companion prose; only reached chapters/completed memory scenes appear. Notifications and reading state follow saved progress. |
| Levels | Select any of the seven available levels and Easy/Normal/Expert difficulty; confirm before starting. |


The search field finds controls across pages. Some choices affect the next
relevant event: new-item placement does not reorder existing inventory, and
cutscene options apply to future scenes with the required resources. Turning
rewind off discards its in-memory history.

Display changes replace game textures while retaining the window and renderer.
The menu automatically uses a compact single-column layout in small windows,
including the original **256×224** resolution, without resizing the window.
A page dropdown and left/right page buttons replace the desktop tabs. Content
scrolls while **Resume** and a turquoise **EXIT GAME** button stay visible.
Graphics → Menu layout can also select Compact on a larger display. Native-size
windows use a separately rasterized 11-pixel MIT font; text scaling is capped at
125% in those tiny windows so the footer remains usable. Larger windows retain
the full 75–175% range. Scroll or use keyboard/gamepad focus to reach more rows. A driver can reject a VSync request; the previous
settings are retained and an error is shown. Widescreen changes rebuild side
backgrounds while preserving the paused frame. They do not enlarge the world
or change collisions.

Volume changes scale music and effects separately. Mute and zero volume advance
the audio players silently, so unmuting does not replay stale action.

## Navigation and rebinding

- **Mouse:** click tabs, checkboxes and buttons; drag sliders and use the wheel.
- **Keyboard:** Tab / Shift+Tab changes focus, arrows adjust controls, and
  Enter / Space activates a focused control. Escape or F10 resumes.
- **Gamepad:** D-pad/stick navigates, A confirms, B closes, and the shoulders
  switch pages. A held opening button is not treated as a request to close.

On Controls, select an action's key button, then press a key. Escape cancels
capture. Duplicate keys and reserved engine shortcuts are rejected with an
explanation. Bindings use SDL physical scancodes: changing keyboard layout can
alter the character printed on a key without moving the binding.
Changed bindings have a Reset button, including restoring Pause to Escape.

### Illustrated keyboard guide

The Controls page shows a keyboard with a different color for each primary
action. Unassigned keys stay dark. Matching arrows lead from the colored keys
to their action labels. Click a colored key or its callout, then press the new
key; the existing duplicate/reserved-key checks still apply. Escape cancels.
The highlights and arrows follow changes immediately, including loaded profiles,
Reset, Undo and restored defaults. On the full keyboard, held keys receive a
bright outline.

The diagram uses physical key positions and the same SDL scancode names as the
binding list. The extra ISO key is marked **ISO**; bound keys outside the main
layout, including additional function, international and media keys, appear in
an extra row. The diagram highlights primary bindings; fixed shortcuts and
legacy aliases are described below it.

Compact windows and large menu text use colored key caps with arrows to one
action per row, so keys remain readable instead of shrinking an entire keyboard.
Scroll to reach the remaining actions. The native-resolution footer stays in
place. See the [keyboard screenshots](screenshots/README.md#keyboard--action-guide).

F10 remains fixed as a recovery key. Outside the overlay, Ctrl+S/L/R and Alt
shortcuts remain engine shortcuts; inside it, use the Save / Load page's actions.
Default keypad movement, right-Shift and Backspace aliases remain
available where their corresponding primary binding has not been replaced.
Controller gameplay remapping is on the Joypad page.

Gamepad navigation is enabled through Dear ImGui's SDL backend. Automated input
checks cover keyboard paths; physical-controller testing remains outstanding.

## Apply, save, undo and reset

Controls apply immediately to the current session. **Save settings** writes
`preferences.cfg` in `--savepath` (default `SAVE`). The write uses a temporary
file and atomic replacement; failure leaves the old preferences file intact
and displays an error.

**Undo settings changes** restores settings from when this overlay opened. **Restore default settings**
restores built-in choices after confirmation. These change the current session;
neither replaces the profile until **Save settings** is selected. The bundled
example `fb.cfg` intentionally differs from some built-in defaults.

**Save game** writes the current level and selected manual slot and leaves the
overlay open. Existing saves ask before replacement. **Load game**, **Load
checkpoint** or **Rewind** resumes after success. The browser spans all seven levels and switches levels after save/asset preflight. Slot 00
is the game's checkpoint and can be loaded but not overwritten by the manual
save button. Keep different asset editions in separate save directories.

The browser shows room/time, version support and new-format previews, names and protection. Refresh picks up external changes. Loading
preflights supported versions (2, 3 and 4), serialized counts, links and truncation
before changing the running state. Version 4 additionally checks edition, level,
entity count and gameplay fingerprint. Legacy saves cannot verify asset identity.


Return to title, level restart/selection and **EXIT GAME** ask before discarding
unsaved progress. Escape/B cancels a confirmation; Escape/F10/Start otherwise
resumes gameplay. The preferences file contains settings, not a game save,
cheats, selected level or rewind history.

## Cutscene skip controls

Under **Enhancements → Cutscene presentation**, two options are enabled by default:

- **Key/click skips developer logos** covers opening developer/publisher scenes,
  including the Macintosh title cards.
- **Key/click skips other cutscenes** covers the intro and subsequent polygon,
  SET, text-summary and PC CD SEQ scenes.

Press Space, any letter or the left mouse button once to skip. A held key or
mouse button skips only the current scene; release and press again for the next
scene. These shortcuts are independent of gameplay bindings and are consumed
before returning to play. A skip click cannot activate the next menu or HUD. Enter and arrows retain
their interactive-panel functions. Backspace remains available when the options
are off. **Save settings** persists `skip_logos_with_keyboard` and
`skip_cutscenes_with_keyboard` for subsequent launches. The standalone cutscene
player uses the same preferences; Escape also exits that player.

## Experimental pistol styles

**Enhancements → Pistol style** selects Original (default), Heavy / red, or
Laser. Save settings persists `pistol_style=0`, `1` or `2` respectively.
Invalid values retain the previous setting. These preferences do not change the
save-state format. These experiments support Amiga, GOG/DOS, Macintosh and Sega CD assets,
using each edition's own shot position. Original remains the default. PC-98 and
experimental Sega cartridge assets retain their original pistol presentation.

With Laser selected, **Laser colour** offers **Orange (default), Red, Green, Blue
and Purple**. The flash, beam and pistol light use the same hue, with a darker
halo and lighter core. Save settings stores `laser_color=0` through `4` in that
order. Colour changes clear old effects; they never change sound or gameplay.

Heavy adds red flame/casings and drawing-only recoil; a strong 240 Hz low shelf makes
Conrad's original pistol sample bassier without lowering its pitch or changing
its duration, with soft peak limiting for loud source samples. Laser uses the
loaded edition's original laser sound (`laser205.spl` on Amiga) with a dedicated
deep-bass shelf and soft peak limiting, a coloured muzzle
flash, a moving glowing segment and a small pistol-side light, with no casings.
A short synthesized electrical pulse with the same bass processing covers asset
sets missing the laser sample. Both styles preserve sound pitch and duration.
Enemy firing, hits, damage and player movement use the original logic.

`game_weapon.cpp` follows Conrad → pistol → shot-effect script links. It starts
replacement effects on the native flash frame and reads the native sprite's rear
edge and centerline for their position. The second flash frame follows its native
position too; the laser travels horizontally at the initial origin's height.
Native render-pixel coordinates preserve odd Macintosh rows at 512×448; they
are never rounded down to the 256×224 playfield. Conrad keeps his original
animation. Heavy adds a drawing-only recoil offset without shifting the shot origin.
`weapon_fx.cpp` owns bounded, deterministic particles and fallback PCM. Effects
pause with simulation and clear on restore, level/room transitions and style changes.
Game palette entries 0x60–0x65 are private effect colors. Sprite/resource buffers
and imported files are never edited. Missing or invalid geometry leaves the
native flash visible. Amiga, GOG/DOS and Sega CD have real-asset firing and render
regressions; Macintosh has synthetic coverage and still needs actual assets.
Sega CD reads its separate object MBK using column-major tile decoding, then
restores the active bank after inspecting the flash.

## Conrad's T-shirt

Open **Enhancements → Conrad's T-shirt** and choose **Original / asset colours**,
**Red / Pink** or **White**. The choice updates the paused gameplay frame
immediately. Select **Save settings** to keep it as `conrad_shirt=0`, `1` or `2`
in `preferences.cfg`. Undo/default preferences include this choice.

Original preserves the exact colours supplied by your asset edition. The two
alternate styles recolour **only the T-shirt inside Conrad's jacket**, with
brightness adjusted for the room's native sprite palette. His jacket keeps its
original brown tones. Skin, hair, trousers, shoes, equipment, scenery, other
characters, polygon cutscenes and prerecorded movies retain their artwork.

Amiga and Sega CD sprites share their light T-shirt shades with shoe soles and equipment.
The engine identifies light fabric components bordered by the jacket in each
decoded pose, before clipping or mirroring, and routes those pixels to private
colours. It never recolours the jacket's brown pixels. DOS-style sprites use
their separate two-tone T-shirt ramp.

The legacy `fb.cfg` switch `use_white_tshirt=true` imports the White preset when
no saved preference overrides it. A saved Original or Red/Pink choice takes
precedence over that boolean. Game saves keep gameplay state; this cosmetic
choice belongs to the player profile and applies after loading a save.

## Cheats, levels and extra tools

Cheats are explicit session controls. All existing engine cheats are available:

| Control | Effect |
| --- | --- |
| One-hit enemy kills | Apply the existing `--cheats=1` flag after a successful enemy hit. |
| Ignore projectile / gun hits | Apply `--cheats=2`; other hazards and scripted deaths can remain. |
| Keep life counter | Apply `--cheats=4` to cancel normal life decrements. |
| Unlimited life | Maintain the original large-life counter, also toggled by Ctrl+I. |
| Fast mode | Toggle the engine's faster pacing, also Ctrl+F. |
| Collision / dirty rectangles | Toggle diagnostic rectangles, also Ctrl+B. |
| Automatic combat zoom | Toggle the DOS-only feature, also Ctrl+G; disabled for other editions. |
| Restore health once | Restore at least the level's initial health while Conrad is alive. |

**Turn off all cheats / debug** clears every listed toggle. Saving preferences
does not save cheats; `--cheats` can still set the initial three-bit mask at
launch. Gameplay effects such as modified health can naturally become part of a
game save. Undo/default preferences do not undo gameplay actions or cheat flags.

Levels uses the normal resource loader, clears rewind and death state, starts
with a fresh score and inventory, and applies the selected difficulty. Existing
save files remain. Demo assets expose only their current level; missing level
object resources disable their entries. A complete matching asset set is still
required. Restart current level uses the current difficulty, regardless of a
previous unconfirmed selection on Levels.

**Capture clean screenshot** writes an original-resolution BMP to `--savepath`
with a unique `flashback-...bmp` name. It captures the paused game frame without
the QoL overlay. The shortcut button is hidden outside active gameplay; its
visibility can be changed on Session and persisted with Save settings. Move the
mouse to reveal its cursor; the gameplay cursor hides again after inactivity.

## Startup precedence and recovery

Startup chooses values in this order:

1. Built-in defaults, then compatibility switches in `fb.cfg` / `--config`.
2. Saved `preferences.cfg` values for settings exposed by the overlay.
3. Explicit display/rewind switches such as `--window`, `--maximized`,
   `--scaler`, `--widescreen`, `--autosave`, `--disk-autosave` or `--no-disk-autosave`.

Paths, output sample rate, game language, MIDI driver, ROMs and soundfonts remain
startup options. The overlay offers working engine controls; it does not expose
unsupported shaders, MSAA, renderer APIs or simulation-rate changes. External
scaler plugins use `--scaler` at launch; the overlay offers the three built-in
filter choices.

To ignore saved preferences for one launch:

```sh
./build/release/reminiscence2 --no-preferences \
  --datapath=/path/to/assets --tunepath=/path/to/assets --window
```

This retains the old file. Saving from the overlay can still replace it.
Settings remain reachable with F10 after the Pause action has been rebound.

## Implementation

The model and validation are in `settings.h` / `settings.cpp`; atomic storage
is in `settings_store.cpp`; save browsing and preflight validation are in
`save_catalog.cpp`; Dear ImGui presentation is in
`settings_overlay.cpp`. `keyboard_diagram.cpp` owns the physical key layout,
live action lookup, colored caps and callouts; it delegates rebinding to the
existing overlay validation and capture path. `game_settings.cpp` connects the model to the engine.
The SDL boundary handles input capture, presentation and texture replacement.
The UI does not advance the game simulation. `appearance.cpp` contains the
T-shirt colour rules and shared-shade fabric segmentation; sprite drawing routes
only Conrad’s T-shirt pixels to two
private palette entries (0x6C/0x6D), leaving the shared source palette unchanged.

Dear ImGui core and SDL backends are pinned to v1.91.9b, commit
`f5befd2d29e66809cd1110a152e375a7f1981f06`, under `third_party/imgui`.
Its MIT license, embedded font/stb notices and source hashes are retained.
No reference screenshot or other game's menu assets are bundled.

Tests cover profile round trips, invalid values and duplicate bindings, atomic
write failure, independent audio gains, silent playback advancement, paused
preference application, SDL input remapping and complete overlay open/close.
Software-rendered captures exercise all eleven pages at desktop and 256×224
resolutions, compact / large-text layouts, the turquoise exit footer and exit
confirmation/cancellation. Private asset tests cover saves, level jumps and
cheat actions against the running engine.
Private Amiga tests check that changing side content preserves the composed
frame and the player's position, life and random state. Shirt tests cover all
three choices, profile persistence and legacy import, reopening the menu,
character orientations and foreground masking, and synthetic Macintosh sprites.
All seven private Amiga levels verify isolated palette changes without moving
the player or changing simulation state; room artwork never uses the two
reserved clothing entries. Actual DOS/GOG and Macintosh data still need their
own visual validation.

## Joypad and journal additions

Gameplay mappings are independent of menu navigation. D-pad and left stick always
move; right-stick movement is opt-in. Mapped pads use SDL button names, while
unmapped joysticks have a separate numbered profile. The keyboard and every pad
source retain their own held state, so neutral stick events cannot cancel held
keys or D-pad input. Stick release hysteresis limits jitter around the dead zone.
Quick save, quick load and rewind have no default button: assign them explicitly.

The Journal page contains project-written companion passages. It reveals seven
chapter entries during active play and a memory entry after that scene completes.
Future entries stay hidden. Opening an entry marks it read; a small unread notice
can be disabled. Save version 4 preserves the random seed and journal bits, with
edition/level/gameplay-fingerprint preflight. Old version 2/3 saves load without
those fields and cannot verify their edition.

The joypad capture accepts physical button presses, rejects conflicting bindings
and uses Start to cancel capture (except when assigning Pause). F10 resumes from
capture, while Escape cancels it. Save settings retains the binding profiles.

New rendered examples: [desktop joypad](screenshots/qol-joypad-1.07.png),
[desktop journal](screenshots/qol-journal-1.07.png),
[native joypad](screenshots/qol-native-joypad-1.07.png) and
[native journal](screenshots/qol-native-journal-1.07.png). These captures use
synthetic session data and the production overlay, without commercial artwork.

## Persistent recovery, previews and reminders

See the [player guide](../manual.md#autosaves-and-continue) for the complete flow.
Normal launches play the developer logos and introduction, then show the title
menu. Autosaves never replace this flow automatically. Space/A–Z skips each
scene by default; Backspace remains available. Esc/F10/controller Start leaves
the opening and opens all QoL pages immediately, or opens them from the title.
Closing settings returns to the title. Levels can start any available level;
Session offers Continue and preferences; Save / Load selects saved progress,
including older autosave generations. Save and rewind require active gameplay.
Rewind has its own timeline page. Contextual hints are opt-in on Session.
The explicit `--continue` CLI option still bypasses openings and the title.

`progress_store.cpp` owns the bounded, CRC32-protected autosave envelope, preview
bytes, three-generation catalog and atomic replacement. `game_progress.cpp`
serializes the existing version 4 state, probes immutable PGE/object/collision
resources in a separate Resource instance, then commits the load only after asset
identity matches. Save format 4 remains unchanged. A preview uses 128×112 RGBA;
the overlay owns a renderer-reset-aware texture cache. Rewind retains immutable
preview owners and metadata alongside its existing memory ring. Its maximum
preview memory is about 6.6 MiB for 120 snapshots.

Disk autosaves default on at 60 active seconds. The profile fields are
`disk_autosave=1`, `disk_autosave_seconds=60` (15–600), and
`contextual_hints=0`. Clean exit can save only a living, coherent frame; forced
termination recovers the last completed write. Rotation never uses manual or
checkpoint filenames. Errors are logged and disk retries back off for 30 active
seconds. Disabling writes retains all files. Continue tries candidates newest
first; explicitly selecting one older generation does not silently choose another.

`context_hints.cpp` owns the small reminder policy and current-binding text.
Timers use simulation ticks, expire after eight active seconds, wait ten seconds
between reminders, and suppress already seen reminders through rewind. Hints do
not change collision, movement or puzzle state. The profile stores the enable
choice; hint suppression lasts for the running game, separately from save data.

## Campaign fixes

The [1.09 campaign](CAMPAIGN_1.09.md) checks actor/script references and inventory
cycles before loading, removes the fixed inventory capacity, and preserves
checkpoint validity when making a manual save. Inventory can hand control to the
QoL menu with Escape/F10/Start. Rewind scheduling follows active simulation time,
so pauses no longer consume its interval. Existing manual and autosave schemas
remain unchanged.

## Recovery, profiles and assists (1.10)

See the [complete recovery/accessibility guide](QOL_1.10.md) for death recovery,
new `.r2m` files, named/protected milestones, HUD notices and Diagnostics.

**Joypad** captures buttons or LT/L2/RT/R2. Save settings persists the connected
SDL GUID's pad/stick profile; reconnecting loads it without changing keyboard,
display or volume settings. Disconnect during play opens a pause; reconnection
requires Resume. PlayStation face-button labels follow the detected controller.

**Enhancements** includes Sticky Action, 100 ms Up/Use/Weapon tap buffering,
reduced shake, a steady shield-hit sprite and 50–100% gameplay speed. All assists
are off by default; speed starts at 100%. **Objectives** shows reached chapter
goals and folded hints. Its knowledge follows the current save and rewind.

New preference keys: `sticky_action`, `input_buffer`, `reduce_shake`, `reduce_blink`
(default 0), `game_speed` (50–100, default 100) and `objectives_notebook` (default 1).
Mapped trigger bindings use codes 32/33; raw joystick bindings still use 0–31.
A disconnected audio device uses a silent mixer clock until output reconnects.

## Startup and independent soundtracks

**Session → Quicksave skips title sequence** is off by default. Enable it and
save settings to resume the newest compatible modern quick/manual/checkpoint
save or autosave on launch. Damaged or incompatible saves are skipped; if none
works, the developer logos, introduction and title menu appear. Legacy `.state`
files still need manual loading. `--no-preferences` uses defaults for one launch.

Keep multiple editions in separate subfolders under `assets`. The first launch
with several available sets shows an edition picker. Its choice is remembered.
**Session → Game edition → Use selected edition…** confirms before restarting
the session with the chosen assets. Save first; modern save compatibility checks
still apply. `--list-assets` prints IDs for `--asset-set=ID` and `--music-source=ID`.

**Audio → Soundtrack** switches immediately and **Save settings** remembers it.
Choose discovered Amiga MOD, DOS PRF, extracted Sega CD audio or an Ogg/WAV pack under
`assets/music/<pack>/`; **This edition / supplied tunes** restores native music.
Missing tracks fall back to native music. The override does not change sound
effects, voices or embedded SEQ/MCD audio. See the [music mapping](../manual.md#music-sources)
for names, looping and preparation of Genesis/Sega CD recordings. Raw ROM/VGM
playback is not implemented. [The Sega CD tool](SEGA_CD.md) extracts supported
CUE/BIN files and preserves CD audio as lossless PCM WAV.
