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

## Pages

| Page | Available controls |
| --- | --- |
| Session | Health, score, room and rewind count; rewind; clean screenshot capture; restart level; return to title; preference save/undo/defaults; focus pause, FPS and shortcut visibility. |
| Save / Load | Manual slots 1–99, occupied-slot browser with room and modification time, checkpoint loading, refresh, overwrite and load confirmation. |
| Graphics | Fullscreen, VSync, point/linear/ScaleNx sampling, window scale, widescreen side content, compact layout and menu text scale. |
| Audio | Master, music and effects volume, mute, save-confirmation sound, current output rate and PRF fallback information. |
| Enhancements | Conrad’s Original/Red-Pink/White T-shirt, rewind, inventory insertion order, transition style, save sound, additional MEMO content, text/SEQ choices and optional cutscene switches. |
| Controls | Rebind the nine primary keyboard actions; view fixed shortcuts, menu navigation and controller bindings. |
| Cheats | All three gameplay cheat flags, unlimited life, fast mode, diagnostic rectangles, DOS combat zoom, one-shot health restoration and clear-all. |
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

F10 remains fixed as a recovery key. Outside the overlay, Ctrl+S/L/R and Alt
shortcuts remain engine shortcuts; inside it, use the Save / Load page's actions.
Default keypad movement, right-Shift and Backspace aliases remain
available where their corresponding primary binding has not been replaced.
Controller gameplay remapping is not part of this pass.

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
checkpoint** or **Rewind** resumes after success. The browser is scoped to the
current level; use Levels first when loading a save from another level. Slot 00
is the game's checkpoint and can be loaded but not overwritten by the manual
save button. Keep different asset editions in separate save directories.

The browser reads the existing save header and shows its room, version support
and filesystem modification time. Refresh picks up external changes. Loading
preflights supported versions (2 and 3), serialized counts, links and truncation
before changing the running state. This does not certify that a save belongs to
the correct asset edition: the inherited format has no edition identifier.

Return to title, level restart/selection and **EXIT GAME** ask before discarding
unsaved progress. Escape/B cancels a confirmation; Escape/F10/Start otherwise
resumes gameplay. The preferences file contains settings, not a game save,
cheats, selected level or rewind history.

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

Amiga sprites share their light T-shirt shades with shoe soles and equipment.
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
   `--scaler`, `--widescreen` or `--autosave`.

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
`settings_overlay.cpp`. `game_settings.cpp` connects the model to the engine.
The SDL boundary handles input capture, presentation and texture replacement.
The UI does not advance the game simulation. `appearance.cpp` contains the
T-shirt colour rules and Amiga fabric segmentation; sprite drawing routes
only Conrad’s T-shirt pixels to two
private palette entries (0x6C/0x6D), leaving the shared source palette unchanged.

Dear ImGui core and SDL backends are pinned to v1.91.9b, commit
`f5befd2d29e66809cd1110a152e375a7f1981f06`, under `third_party/imgui`.
Its MIT license, embedded font/stb notices and source hashes are retained.
No reference screenshot or other game's menu assets are bundled.

Tests cover profile round trips, invalid values and duplicate bindings, atomic
write failure, independent audio gains, silent playback advancement, paused
preference application, SDL input remapping and complete overlay open/close.
Software-rendered captures exercise all eight pages at desktop and 256×224
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
