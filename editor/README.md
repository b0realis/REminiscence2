<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# REminiscence2 Editor — learn from Flashback

This visual editor is a homage to **Delphine Software** and the artists,
animators, designers and programmers who made Flashback. Its purpose is to let
game designers learn from an earlier generation's craft: how a room teaches a
movement, how animation controls timing, and how a few connected objects create
a memorable puzzle. It also provides a workshop for testing new ideas.

This is a community tool, not an official Delphine authoring tool. The native
views expose the behavior the engine interprets. Automatically generated puzzle
names and groupings are analytical starting points, not claims about the
original designers' intentions. Follow the connections, watch execution, and
record your own observations in module/node comments.

## Separate editor, shared engine

- `reminiscence2-editor` is the desktop editing application.
- `reminiscence2` is the game player. Players need the player, campaign files and
  their required game assets; they do not need the editor.
- Both share engine, resource-decoding and simulation code. Custom room tests
  run inside the editor; native campaign tests use a separate player window.
- Editing custom projects needs no original game files. Gameplay and native
  studies use your extracted Amiga, DOS/GOG or Sega CD assets. Macintosh editor
  playtests remain unverified/unsupported. Source assets are never overwritten.

## Start

See the [standalone build guide](../docs/BUILDING.md) for complete, player-only
and editor-only builds on Windows, Linux and both Mac architectures. Native
campaign and whole-campaign playtests require the player beside the editor;
custom-room playtests run inside the editor.

From the development checkout, after building locally:

```sh
./edit-local.sh              # Room/ledge Workshop
./edit-local.sh --modules    # Connected-puzzle Module Lab
./edit-local.sh --lab        # Five-room Conrad Systems Lab
./edit-local.sh --campaign   # Import/open all seven original levels
./edit-local.sh --campaign-editor # Levels, branches, movies and story screens
```

For another desktop checkout, install the SDL2/zlib development dependencies:

```sh
cmake -S . -B build/editor -DREMINISCENCE2_USE_MODPLUG=OFF
cmake --build build/editor --target reminiscence2-editor reminiscence2 --parallel
build/editor/reminiscence2-editor --assets=/path/to/extracted/assets
```

Use **Game assets…** to choose files, **Settings** for play-start preferences,
and **Help / F1** for the grid, layers, controls and workflow. The first native
import creates local working documents; later launches reopen your edits.

The [Systems Lab example](../examples/editor/systems-lab/README.md) includes its
room artwork, animation strips, synthesized sounds and complete module/campaign
graphs. It demonstrates proximity doors, an interlocked airlock, droid repair,
moving-target hit feedback and optional computer interactions. `--lab` creates
an editable local copy on first use and keeps subsequent edits. The example
requires your own Amiga, DOS/GOG or Sega CD resources for playtesting.

## Four workspaces

| Workspace | What the designer does |
| --- | --- |
| Level Editing | Import/slice Back, Playfield and Front art; paint collision and ledges separately; connect rooms; place the player start |
| Module Placement | Place, move and copy complete connected puzzles; inspect original actors and hidden helpers |
| Module Editing | Connect custom nodes or inspect original puzzle dependencies and state-machine rules; monitor signals and annotate findings |
| Campaign | Connect levels, movies, illustrated text screens, branches and endings |

New/Open, Save, Undo/Redo, Validate and Play follow the active document. The
first three workspaces share a level; Campaign has a separate document/history.
Art and audio are imported files; the editor is not a bitmap or audio editor.

## A first study: follow an original switch and door

1. Open the original campaign and select a level/room. In Module Placement,
   find a door or activator in the native puzzle palette. Hidden helpers are
   included even when they do not appear in the room.
2. Open **Edit puzzle nodes**. Inspect the typed actor connections. A wire may
   identify a message recipient while a rule specifies the message value;
   both matter. Shared player/inventory services are shown separately.
3. Open **Actor state-machine rules**. Conditions 1 and 2 must pass before
   the action executes. Inspect the next state, fallback rows, movement and
   flags. **Operand help / raw value** shows source values and restrictions.
   **Actor data / protected operands** explains all four data slots, including
   numeric and mixed-use fields that have no editable wire.
4. Launch **PLAY HERE**, perform the interaction, then **Pause native**.
   Watch the live message/rule trace and use **Step native** to advance one
   simulation frame. A step is not one script instruction.
5. Before changing a shared rule, check the owner count and use **Make behavior
   private** if the experiment should affect one actor. Change one certified
   condition/action or typed port, then **Apply live**. Step to observe the
   result, or use **Apply + resume** to continue immediately.
6. Record what you observed in the module comments. Undo edits in the document
   and apply again to restore the code. Undoing code does not rewind effects
   already produced in the running puzzle; restart for a clean comparison.

The editor validates the entire candidate before changing the running world.
Compatible updates preserve actor positions, animation cursors, inventory,
counters, queued messages and unchanged extension-graph progress. Previously
queued messages are delivered to their old recipients; new evaluations use
new wiring. Live updates clear rewind history. A failed **Apply + resume**
leaves the game paused and explains the conflict.

## Intentional limits

- Original actor/resource capacities stay in place. **New Washington is full**:
  edit or move its existing actors; adding copies is unavailable. Removing an
  actor reserves its original slot for references and Undo.
- Native puzzle copies stay within the **same level and asset edition** and
  include the complete connected dependency bundle. They are visual study and
  editing modules, not resource-independent custom prefabs.
- Protected operands are exposed for understanding. Unreviewed resource values
  and mixed-use actor data are not unlocked for arbitrary writes.
- Compatible native rules, typed ports, appended branches and private behavior
  copies can apply live. Initial health edits require unchanged positive live
  health. Actor movement/facing, slot changes, removal/restoration, incompatible
  row layouts and authored extension-graph changes require a restart.
- Custom scenes currently use **Stop → edit → Play**. Native updates wait for
  a simulation boundary; menus/dialogue/cinematics may need to finish or be
  skipped first. No automatic background application is performed.

See the [native coverage, capacity table and live-update matrix](../docs/NATIVE_CAMPAIGN_MODULES.md)
for precise constraints and validation coverage. Keep imported native campaign
studies and original artwork private with your assets. The engine/tool license
is GPL-3.0-or-later; it does not relicense original game content.

## Further guides

- [Editor controls, room geometry and artwork sizes](../docs/LEVEL_EDITOR.md)
- [Custom modules, node semantics and animation/audio](../docs/EDITOR_MODULES.md)
- [Campaigns, routes and story screens](../docs/CAMPAIGN_EDITOR.md)
- [Project format for developers](../docs/formats/CUSTOM_PROJECTS.md)

The editor has been built and tested locally on Apple Silicon. Native campaign
checks cover Amiga, DOS/GOG and Sega CD fixtures across seven levels; this is not
an exhaustive human playthrough of every modified puzzle or a claim that every
proposed design feature has been implemented.

Custom playtests show a nearby **Enter** prompt for the closest enabled control.
Use repeatable Button pairs for machinery and lighting switches, and Dialogue
choices plus Timers for bounded tests. These patterns are described in
[the module guide](../docs/EDITOR_MODULES.md#nearby-controls-in-playtests).

## Authored player controls

When launched with `--project` or `--campaign`, Esc/F10 opens the game's QoL
menu. Resume returns to the same room; EXIT GAME closes the player. Embedded
editor playtests still use Escape/Stop to return to editing. Missing room links
are sealed automatically, so the player cannot walk into undesigned screens.
Story passages use native fonts above the action; Tab/Backspace or Enter advances
text, and Up/Down chooses responses. See [the level editor guide](../docs/LEVEL_EDITOR.md)
for checkpoint and settings storage.
