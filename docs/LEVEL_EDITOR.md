<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# REminiscence2 visual level editor

Version 1.41 adds Editor Settings for native campaign setup and the Play/F5 start mode.
The editor includes native campaign live updates, group selection/clipboard,
layer locks, graph navigation and clickable validation. Import artwork,
slice rooms, paint collision/ledges, wire puzzles and test with the original
movement engine. **Guidelines (F1)** explains the pixel grid, artwork sizes,
visual style and controls directly in the editor.

The palette has legacy door pairs, animated props, health pickups and the native
crawler, plus editable graph templates for doors/buttons, keycards, NPC retrieval
quests, mission computers and area triggers. See the [module guide and original
campaign coverage audit](EDITOR_MODULES.md). Native campaign mode retains every
original actor, puzzle and script across all seven levels. Custom module mode
remains separate: arbitrary native dependency bundles are not reusable prefabs.

![Native editor showing two independent door/activator pairs](screenshots/editor-workspace-1.36.png)

## Four-part workspace (1.45)

Use the top tabs to choose your task:

1. **Level Editing:** art layers, slicing, collision, ledges, exits and player start.
2. **Module Placement:** complete puzzles from recipes or the local module library;
   move/copy instances and place their parts around the level.
3. **Module Editing:** create a named puzzle, import its assets, add individual nodes
   and wire their actions. Dialogue, animation, pickup-effect and timer nodes can
   join existing buttons, doors, NPCs, quest items and mission computers.
4. **Campaign:** connect imported levels, movies, text/photo screens, choices and endings.

**Help** explains all four workflows and the original pixel grid. F1 opens the
same guide during a playtest. See [module authoring](EDITOR_MODULES.md) for dialogue
choices, trigger/stop semantics and reusable local templates.


## Start locally

From this checkout on the development Mac:

```sh
./edit-local.sh
# Or open the three-room graph demonstration:
./edit-local.sh --modules
```

The script opens a private working copy of the supplied Workshop example under
`.local/editor-projects/Workshop`. It finds prepared editions under `.local/assets`.
The example in `examples/editor/workshop` remains a clean template. It contains
only newly drawn sample art and project data, with no original game files.

To build in another desktop checkout, use the normal SDL2/zlib development setup:

```sh
cmake -S . -B build/editor -DREMINISCENCE2_USE_MODPLUG=OFF
cmake --build build/editor --target reminiscence2-editor reminiscence2 -j 8
build/editor/reminiscence2-editor examples/editor/workshop/project.r2project.json
```

The editor is an SDL2/Dear ImGui desktop application. Editing needs no game
assets. **Choose asset folder** selects your prepared files for Play. Amiga,
classic DOS/GOG and Sega CD were tested; Macintosh editor playtests are not
supported yet. Select an edition from the list if the folder contains several.
The editor itself is currently built and tested locally on Apple Silicon.

## Guidelines and navigation

Click **Guidelines (F1)** for four tabs: Pixels & artwork, Visual style, Workflow
& controls, and Original level. The text is original project documentation.
Opening it during a playtest pauses the simulation; close it and explicitly
Resume when ready. Escape dismisses the guidelines without stopping that test.

A room is **256×224 = 57,344 logical pixels**. Horizontal room spacing is 256;
vertical spacing is 216, leaving eight overlapping rows. Collision is a separate
16×7 grid with 16×36-pixel cells, and normal floor edges are Y=72, 144 and 216.
The artwork and collision systems have different coordinates; see the in-app
Workflow tab for the collision-row offset and player feet convention.

The palette's **Search** filters object names, graph templates and collision
brushes. **Fit** shows the whole room and resets scrolling; **1x / 2x** choose exact
pixel multiples. Undo/redo retain the selected graph and node when they still
exist. Drop a project JSON to open it, or an image to slice it. File drops are
blocked while a playtest, reference view or modal operation is active.

![Built-in pixel and artwork guidelines](screenshots/editor-guidelines-1.38.png)

## Editor Settings and native campaign mode

Click **Settings** in the main toolbar. **Create native campaign project...**
opens the folder picker directly and creates all seven native level documents
from the asset edition selected in the palette. It opens the new campaign with
**Native campaign / authored level start** selected. **Open existing project...**
opens a saved custom or native project. The current project mode is displayed;
changing playback preferences does not convert a custom document into a native
campaign. Stop an active native preview before switching project folders.

Under **Play button and F5**, choose:

| Setting | Play button | Result |
| --- | --- | --- |
| Current room / TEST marker | PLAY HERE | Start at this room or the selected graph node's TEST position |
| Native campaign / authored level start | PLAY CAMPAIGN | Start at the selected native level's authored beginning, with its arrival sequence and later campaign progression |
| Authored project start (custom projects) | PLAY FROM START | Start at the custom project's authored START |

**F5** follows that preference. **Shift+F5** always forces the authored start;
**Ctrl/Cmd+F5** always forces the current room. Right-click the Play button for
both explicit choices. Native campaign start means the selected level, so select
Level 1 to play from the campaign beginning.

**Show native actor markers** toggles the numbered room overlay without hiding
native artwork. Hidden markers cannot be dragged accidentally; records remain
selectable in the palette. These two preferences are saved in
`.editor-settings.json` beside the project, shared by native sibling levels, and
loaded on reopening. They do not change project JSON, undo history, original
assets or ordinary player preferences. Malformed settings fall back to defaults
with a status message. The sidecar is excluded from Git.

The panel also states current native event support. Actor markers are not an
exhaustive event overlay: hidden helpers may have no spatial location, and
conditions/messages can connect actors across rooms. Native rule editing and
execution traces are now available in Module Editing. Complete native dependency-bundle relocation is available within the source
level/edition; see [native campaign modules](NATIVE_CAMPAIGN_MODULES.md). The custom
**Puzzle Modules** palette supports connected editable puzzles today; see the
[module coverage audit](EDITOR_MODULES.md#original-flashback-campaign-coverage-audit).

![Editor Settings](screenshots/editor-settings-1.41.png)

## Playtest where you are working

With Current room selected in Settings, click **PLAY HERE** or press **F5**
(**Ctrl/Cmd+F5** in either mode) to launch the latest edits in the selected
room. In the module graph, it uses the selected node's room and looks for a
standing position near that node. This works for custom projects, scenery
studies and full native campaign projects.

The yellow **TEST** crosshair shows the room's launch position. **Place TEST
marker** followed by a click in the room pins a position, snapped to the native
floor heights. **Auto** returns to automatic placement. Automatic placement
checks floor support, headroom and initially closed custom doors; an invalid
explicit marker reports a problem instead of silently moving Conrad. Facing
uses the authored player-start direction when a marker is placed. A native slot
without map data cannot be used.

Markers belong to the editor session and are remembered per room until another
project is opened. They do not alter the authored **START**, the saved document
or undo history. **Shift+F5**, or right-click **PLAY HERE → Play from authored
start**, runs the normal authored beginning. Custom playtests run inside the
editor; Escape or Stop returns to the same editing location and selection.
Restart repeats the start chosen for that playtest.

Native tests run in a separate game window. PLAY HERE loads fresh level state,
keeps the complete original actor/script table and pistol dependencies, and
skips the scripted arrival and opening cinematic. It does not manufacture quest
items or completed objectives from earlier rooms. To test an edit with earned
progress, use **Pause native → Apply live → Resume native**. PLAY HERE starts a
fresh test; use **Stop native** to close its window.

![PLAY HERE and the TEST marker](screenshots/editor-play-here-1.40.png)

## Open the original campaign

```sh
./edit-local.sh --original-level
```

Or choose an assets folder and edition in the palette, then click **Original campaign**.
This opens a reference viewer, retaining your current project. Choose any of
the seven levels with **Reference level**. The left list
selects native room slots; **Empty slots** includes slots without map data. The
central image uses the engine's actual decoder. Toggle initial actors and
collision, or follow the **Up / Down / Right / Left** link buttons. The right
column lists initial actor positions, native types and behavior-node numbers.
**All actors / helpers** includes offscreen holding-list records.

The local editions have 38 available map slots for Amiga, 26 for DOS/GOG and
38 for Sega CD; all have 107 initial actor records. These counts are for the
supplied editions, and map availability does not mean every slot is a normally
reachable gameplay room. The native starting room is slot 27, not slot 0.
Initial-state actors may be hidden until scripts activate or animate them.

### Make an editable scenery study

Click **Make scenery copy...**, choose a parent folder (preferably under
`.local/editor-projects`), then **Use folder**. The editor creates and opens a
new `Level1-study-...` subfolder. It does not overwrite the original files or an
existing project; unsaved work follows the normal recovery-file behavior.

The copy preserves all 64 native room indices, raw collision cells, valid room
links and Conrad's starting position. Available maps become lossless PNG Back
art; native foreground-priority pixels become an alpha PNG on Front. You can
change collision, replace artwork, place modules and **PLAY HERE** with the native
player. Room World coordinates form an indexed 8×8 shelf, **not** a geographic
reconstruction; native exit links remain independent of that shelf.

This is **not a full campaign conversion**. Native actors, enemies, puzzles,
inventory, story scripts and palette animation are not imported. The native
foreground mask approximates occlusion in the custom renderer; it is not the
original tile-authoring structure. There are no inferred ledge-marker objects:
the native collision cells already encode the climb geometry. Decoded artwork
remains original game content, so keep scenery-study projects private.

### Edit the full native campaign

Click **Edit full native campaign...**, choose a parent folder, then **Use folder**.
A new `NativeCampaign-...` directory holds seven `level-N.r2project.json` files.
No original asset files are changed or duplicated. The editor reads original art
from the selected edition and stores your overrides separately. All 64 room slots
per level and every native actor ID are preserved, including hidden helpers.
Native exports now include decoded original behavior tables as editable JSON;
keep these asset-derived campaign copies with your private project data.
The World view is an indexed 8×8 shelf; exit links define actual connectivity.

Select an actor marker or its palette record to edit its initial position,
room, facing or health. The inspector shows its original type, behavior index
and four script operands. Puzzle Connections exposes typed actor/room ports,
and Make behavior private isolates shared rules before editing.
The Module Editing workspace now exposes the complete native rule tables,
conditions, actions and next-state connections. Offscreen
holding-list helpers are protected: their scripts decide when they spawn or
enter an inventory. Move/copy complete puzzle relocates the full connected bundle, including helpers,
within its source level/edition. Original actor IDs remain stable; native deletion
and cross-level imports are unavailable. Capacity and sprite-bank limits are checked. Authored event modules
can now bind existing actors, observe their rules, send native messages and run
new dialogues and state machines. Use **Create actor event module** in the actor
inspector, or **New event module** in the native palette.

**PLAY HERE** saves the current project when launching a new player window and
starts immediately at the TEST position. Subsequent tests send the latest edits
to that same player. It runs the normal engine loop, with original enemies, puzzles,
items, inventory, dialogue, cinematics and level progression. The sibling project
for each level loads when the campaign reaches that level; missing siblings use
original data. **Campaign level** saves the current document before opening the
chosen sibling. The running level changes on progression, explicit restart or PLAY HERE / F5.

| Control | Result |
| --- | --- |
| PLAY HERE / Ctrl/Cmd+F5 | Start fresh at the current room or selected graph node's TEST position |
| F5 | Use the start mode chosen in Editor Settings |
| Shift+F5 | Start fresh at the authored beginning, including its arrival sequence |
| Pause native | Wait for a complete simulation boundary, then freeze the game |
| Apply live | Validate and apply art, collision/ledge and exit edits; stay paused and redraw |
| Resume native | Continue the preserved running state |
| Restart at edited level | Rebuild that level from its edited initial actors and geometry |
| Stop native | Close the game preview, including a blocked cinematic/dialogue |

Live apply merges only cells/links that changed in the editor. A door or puzzle
may have independently changed the running collision map; an overlapping edit
is rejected instead of overwriting that state. Adding collision over an active
actor is also rejected. Actor setup, native rule edits, authored module edits and switching levels require a fresh
test with **PLAY HERE** or **Restart at edited level**. Rejected edits leave the live game unchanged and explain the
reason in the status line. Cutscenes, dialogue and native menus must finish or be
closed/skipped before Pause/Apply can reach a simulation boundary; Stop remains
available. Wait for a command's acknowledgement before issuing another.

Room art overrides retain native sprite rendering, palette-based actors, inventory
and dialogue. Empty art fields use original art; replacing Front uses PNG alpha
for actor occlusion. Live edits clear rewind history so it cannot restore stale
geometry. Native preview saves go in the project's `.native-playtest-saves`;
normal campaign saves and preferences are not written. Stop the preview before
opening a different project folder or using Save As into another folder. Save As
copies the current level and its artwork, not the other six campaign documents.

### Faster authoring

- **Multi-select:** Ctrl-click on Windows/Linux, Cmd-click on Mac, or Shift-click
  toggles whole modules. Drag empty room space for box selection. Dragging a group
  retains part spacing and clamps the group together; doors keep their grid.
- **Copy/paste:** Ctrl/Cmd+C and Ctrl/Cmd+V, or palette buttons. Paste assigns fresh
  instance IDs, retains scoped graph wires and links within the copied group,
  and drops links to modules outside it. Undo restores the previous document.
  The clipboard is local to this editor session. Cross-project paste copies art;
  remote-room parts still need matching destination room IDs.
- **Locks:** each visual layer has its own Lock checkbox. A module with any part
  on a locked layer cannot move, change, paste or delete. Lock collision protects
  painting and ledge stamps. Visibility is independent. Locks are workspace state.
- **Graph navigation:** middle-drag pans, wheel zooms around the pointer, **Frame
  all** fits the entire graph and **100%** restores the default view. Dragging and
  wiring use the transformed coordinates. Graph layout does not move room objects.
- **Validation:** Validate collects structural and image issues. Click **Focus**
  to select the offending room, module, graph node or native actor. Refresh after
  a repair. Unreachable-room notes may be intentional for native scripted routes.

![Graph pan/zoom and Frame all](screenshots/editor-graph-navigation-1.39.png)

Custom playtests still use **Stop → edit → PLAY HERE**; controlled live apply belongs
to native campaign mode. Automatic native bundle extraction, custom savegames
and custom moving-platform behavior remain separate future work.

## Try the example

1. Click **PLAY HERE** (F5). Use Right to walk behind the foreground railing.
2. Stop beside the orange activator and press Enter. Its paired door opens.
3. Walk through the right edge into the climb chamber.
4. Approach the raised platform and use Up to climb its ledge.
5. Press Escape or **STOP / return to editor**. Editing resumes from the same
   document; playtest positions and door states do not alter it.

Play uses arrows for movement/climbing, Space to draw/holster, Shift for actions
and firing, and Enter for activators. **Pause**, **One tick** and **Restart** help
inspect behavior. Losing window focus pauses the embedded playtest. These
editor playtest keys are fixed in this first version.

## Whole-level artwork and screen slicing

Use **Slice image...**, or drop an image onto the editor. PNG supports
transparency; JPEG and BMP also work. Choose the visual layer and source block
size. The source image is copied once into the project's `art/` directory; rooms
store crop rectangles referencing that same file. No repeated room image exports
are needed, and the source file is unchanged.

| Setting | Meaning |
| --- | --- |
| Block width / height | Size of one room crop in source-image pixels |
| Horizontal / vertical stride | Distance between the starts of successive crops |
| Classic overlap | 256×224 crops, 256×216 stride; neighboring vertical rooms share eight image rows |
| Sprite slicing: no overlap | Strides equal block dimensions |
| Rebuild rooms and modules | Replace the room layout, collision, ledges and modules with a newly linked grid; Undo restores the previous document |
| Rebuild unchecked | Replace only the selected art layer; the image must match the existing room grid |

For example, a 1024×440 image produces four columns and two rows with the classic
overlap preset. For artwork painted at twice the logical resolution, use 512×448
blocks and a 512×432 stride. The first renderer downsamples each crop into a
256×224 logical room. Higher-resolution sources are preserved on disk, but this
version does not retain their extra detail on screen.

Incomplete crops at the right/bottom are transparently padded. Prefer image
dimensions chosen for your block/stride geometry. Generated neighboring rooms
have reciprocal screen links; you still author the openings and platforms that
make those transitions playable. Each new room starts with a bottom floor.

## Layers and collision

Rendering order is **Back → Playfield art/decorations → original actors → Front**.
Select a room with no module selected to import or clear its individual layer
images. Back is normally opaque. Use transparent PNGs for other layers.

Collision, ledges and virtual wires are separate overlays. A railing in Front
can hide Conrad while he walks through its image; it blocks him only if you paint
collision. Replacing a visual layer leaves collision and ledges unchanged.

The first profile deliberately follows the original movement geometry:

- Rooms are 256×224 logical pixels. Vertical movement between rooms advances
  by 216 pixels.
- The collision grid has 16 columns and 7 rows. X cells are 16 pixels wide;
  Y sampling uses 36-pixel rows. Supported standing surfaces are at Y=72, 144
  and 216, with Conrad's actor anchor two pixels above the surface.
- **Paint blocking cells** and **Erase collision** edit the grid. Amber lines
  mark standing surfaces. Artwork does not generate collision automatically.
- **Ledge: climb right/left** stamps a supported edge and its required adjacent
  clearance into the compiled grid. It remains a separate authoring marker.
  Right-click beside a marker to remove it. Extend the platform with collision
  cells; a ledge marker alone does not create an entire platform.
- **Player start** places Conrad. The room inspector sets his starting facing.
  The player starts on the nearest supported floor height, not in the original
  arrival animation.

Paint walls or connect exits intentionally. An unlinked edge does not invent a
wall: walking/falling out of the graph ends the playtest. **Restart** repeats
the chosen playtest start. Slopes, arbitrary polygon collision and freeform ledge
physics are not part of this profile.

## Self-contained modules

Choose a palette entry and click the room. **Select / move** lets you select a
visible part and drag it. Most gameplay objects snap to the original movement
grid. Animated props can be placed vertically between floor heights.

### Door + activator

A placement creates one module with two parts: a door panel and its activator.
The cyan line shows their relationship only while editing. There is no switch
number to enter or global ID to wire manually.

Select **Door** or **Activator** in the inspector to move that part or change its
room. During play, Enter near the activator opens its own door and removes the
panel's collision. **Initially open** and **Toggle** change its starting/activation
behavior. A closing door remains open while Conrad occupies its opening.

**Duplicate module** creates a new paired instance. Its own activator continues
to operate its own door. Explicit outgoing puzzle connections are cleared on
duplication so a copy does not silently control the original puzzle.

Under **Chain outputs**, select other doors to open when this module activates.
This connects whole modules. Cycles terminate, and one activation cannot toggle
a target back and forth. Chained targets receive an open request; Toggle applies
to the directly operated module. Cross-room owned pairs are also shown in the
world overview.

**Save module...** writes a `.r2module.json` plus any owned imported image.
**Load module...** places a fresh instance with its relative part positions
preserved. It starts in the selected room; move either part to another room
after loading. Loading rejects placements that put a part outside supported
bounds. External puzzle links are not saved into a portable module file.

### Other initial palette entries

| Module | Current behavior |
| --- | --- |
| Animated prop | Built-in blinking marker, or an imported horizontal sprite strip. Set frame width/height, frame count, rate, display size and Back/Playfield/Front layer. Animation bounds are validated before play. No collision is inferred from sprite pixels. |
| Health pickup | Walk nearby to restore the chosen amount, up to the original five-point shield maximum. Consumed only when healing is needed. |
| Alien crawler | Original crawler animation, movement and combat script, with independently allocated actor state. Choose its facing. Its original vulnerable phase responds to crouched pistol fire; this is not a generic configurable-health enemy. |

The editor uses a colored schematic for the crawler while editing. Play displays
the actual edition's artwork and behavior. The player bundle owns its gun,
projectile, casing and invisible draw/holster controller; these dependencies
cannot accidentally be shared by separately placed modules.

## Node and connection editor

The **Puzzle Modules** palette adds editable button/door, keycard, retrieval quest,
mission computer and area-trigger templates. Select one and choose **EDIT MODULE
GRAPH**; legacy door pairs offer **Convert to node graph**. Nodes have independent
sprites/animation, room placement and behavior. Turquoise wires carry events;
gold wires define an NPC's required item. See the [complete walkthrough](EDITOR_MODULES.md).

## Rooms, saving and sharing

**World view** shows room thumbnails and their explicit links. Click a thumbnail
to edit it. **Add room** adds a room; use the inspector's Up/Down/Right/Left fields
to connect it. Changing a link updates the reciprocal link and clears a displaced
reciprocal connection. Image placement alone does not control room transitions.

**Save** writes a readable `.r2project.json` atomically. **Save as** into another
folder copies referenced artwork as well. Asset copies are deduplicated, with
byte comparison to prevent overwriting a different file after a hash collision.
Save As across folders starts a fresh undo history because asset paths change.

Use toolbar Undo/Redo, or Cmd/Ctrl+Z and Cmd/Ctrl+Shift+Z. Cmd/Ctrl+S saves.
Canvas painting and dragging are grouped by mouse gesture. History is bounded
to 100 entries and roughly 16 MiB. Before New/Open replaces unsaved work, a
`recovery.r2project.json` is written beside the current project. Closing with
unsaved changes offers Save, Discard or Keep editing.

Share the JSON **and its `art/` folder**. Keep original game files separate;
recipients supply their own supported edition. The sample artwork is newly
authored for this project and carries GPL-3.0-or-later in its accompanying README.

The game binary can run a project directly:

```sh
build/package-arm64/reminiscence2 \
  --datapath=.local/assets --asset-set=amiga \
  --project=.local/editor-projects/Workshop/project.r2project.json
```

This opens a 3× window. Escape exits; Enter restarts after death. The custom
runner has no campaign menus, journal, inventory menu, quicksave/rewind or normal
QoL overlay yet. Original saves and preferences are not written by a custom
playtest. Original sound effects use the selected edition; Music/Sound nodes import WAV
or Ogg clips, with preview, looping and wired start/stop controls. Authored graph dialogue and mission text work in both the
editor and command-line player; see the [module guide](EDITOR_MODULES.md).

## Developer notes

The [schema and runtime contract](formats/CUSTOM_PROJECTS.md) describes files,
bounds, collision compilation, bundle relocation and rendering. `editor/` shares
the engine library; `Game::simulateActors()` is also used by normal gameplay.
The editor owns its SDL window, while the embedded test owns one disposable
game scene. It is not a second implementation of Conrad's movement.

Run `ctest --test-dir build/package-arm64 --output-on-failure`. The editor
regressions cover schema bounds, module persistence/duplication, art slicing,
original movement, transitions, ledges, crouched gun hits, activation edges,
cyclic wiring and foreground occlusion. Real-asset tests run only when their
private CMake data paths are configured. UI tests drive the actual widgets
through ImGui input events with SDL's dummy video/audio drivers.

For a workspace capture without an interactive desktop:

```sh
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SDL_RENDER_DRIVER=software \
  build/package-arm64/reminiscence2-editor \
  examples/editor/workshop/project.r2project.json --screenshot=workspace.bmp
```

`tools/create_editor_example.py` regenerates the redistributable sample; use a
new output folder if you want to preserve an edited version.

### Campaign authoring and expanded modules

The editor now has four authoring workspaces and 26 node types, with animated
signals, help and designer notes. Run `./edit-local.sh --campaign-editor` to try
the level-route editor, movies and text/photo screens. See the [campaign guide](CAMPAIGN_EDITOR.md).
Campaign progress now saves actors, puzzles, dialogue and carried items together.
Returning levels preserve their world. Ctrl+S/Ctrl+L save/load during gameplay;
the campaign launcher offers Resume/New. The repository includes small training
fixtures for learning and regression tests. Full authored story demos stay private.
