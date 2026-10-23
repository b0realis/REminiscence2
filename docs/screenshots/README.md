<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# REminiscence2 screenshots

Gameplay and menu captures from REminiscence2 `0.1.0-dev` with player-supplied
Amiga assets. Select an image to view its full resolution.

## High-resolution QoL menu

The production SDL renderer and Dear ImGui overlay draw these **1920×1080**
captures over a paused game. The save browser shows two actual manual saves.
The images are lossless PNG conversions of captured renderer output.

### Enhancements

Conrad's T-shirt choices, rewind, inventory behavior and cutscene presentation.
**Resume game**, **Save settings** and the turquoise **EXIT GAME** button remain
at the bottom of the overlay.

![Enhancements page at 1920×1080](qol-highres.png)

### Save / Load

Manual slots, room and modification time, checkpoint loading and stored saves.

![Save and load page at 1920×1080 with two saved slots](qol-save-load.png)

### Graphics

Display controls, pixel filtering, window scale, side content and menu text size.

![Graphics page at 1920×1080](qol-graphics.png)

## Original-resolution QoL menu

At **256×224**, a page selector replaces the desktop tabs. The contents scroll
while **Resume** and **EXIT GAME** stay visible. This PNG retains its native
resolution.

![Compact QoL menu at the original 256×224 resolution](qol-native.png)

## Keyboard / action guide

Controls shows a full keyboard with colored primary action keys, darker unused
keys and arrows pointing to the corresponding actions. Select a colored key or
callout to rebind it. Highlights follow the actual preferences, including custom
profiles, resets and changes made during play.

![Live keyboard diagram with the default controls at 1920×1080](qol-keyboard.png)

<details>
<summary>Compare a custom WASD layout</summary>

This capture applies WASD movement, K for actions, E to use, R for the weapon,
I for inventory and P for Pause. The old arrow, Shift, Space, Tab and Escape
primary keys become dark; the colored keys and arrows move to the new bindings.
The fixed F10 recovery shortcut remains available.

![The same keyboard diagram after changing to WASD and custom action keys](qol-keyboard-wasd.png)

</details>

At the original resolution, the guide keeps each key and its action readable
in scrollable rows instead of shrinking the entire keyboard.

![Colored key caps and action callouts at 256×224](qol-keyboard-native.png)

## Amiga gameplay

The earlier seven-level captures are preserved below at **512×448**, a 2×
nearest-neighbor presentation of the original **256×224** game frame. These
are Amiga captures; they do not demonstrate DOS/GOG or Macintosh CD assets.

<details>
<summary>Show the seven-level overview</summary>

![Contact sheet of all seven Amiga levels](amiga-contact-sheet.png)

</details>

| Level 1 | Level 2 |
| --- | --- |
| ![Amiga level 1](amiga-level1.png) | ![Amiga level 2](amiga-level2.png) |

| Level 3 | Level 4 |
| --- | --- |
| ![Amiga level 3](amiga-level3.png) | ![Amiga level 4](amiga-level4.png) |

| Level 5 | Level 6 |
| --- | --- |
| ![Amiga level 5](amiga-level5.png) | ![Amiga level 6](amiga-level6.png) |

![Amiga level 7](amiga-level7.png)

## Image credits

Flashback was created by Delphine Software. Its original artwork and other game
content remain copyrighted by their respective rights holders. These screenshots
document the engine's gameplay and interface; the engine's GPL license does not
grant rights in the game artwork shown. The SPDX notice above covers this
project-written gallery text, not the PNGs.

The QoL interface is REminiscence2 work using the MIT-licensed Dear ImGui UI
and font. Dependency notices are retained in
[third_party/imgui](../../third_party/imgui/README.md).
The supplied publisher-manual illustrations, comic, protection sheets and cover
scans remain private references and are not part of this gallery. Players still
supply their own original game files.
