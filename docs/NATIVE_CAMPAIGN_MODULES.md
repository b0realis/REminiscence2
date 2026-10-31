<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Studying and editing the native campaign

All seven original levels can be opened as editable native puzzle modules. This
keeps the original interpreter, actor templates, animations, messages, inventory,
text and cinematics. It does not approximate original scripts with new dialogue
or guessed high-level actions. Authored State/Dialogue/Timer/Actor graphs can
extend these native modules.

## Open and play

From a locally built checkout:

```sh
./edit-local.sh --campaign
```

The first run imports the first detected full-game edition into
`.local/editor-projects/NativeCampaign`; subsequent runs reopen those edits.
The folder contains seven `level-N.r2project.json` files and `CAMPAIGN.md`.
Choose **Campaign level** to switch levels. Save before switching. **PLAY HERE**
tests the TEST marker; **Editor Settings → authored start** follows the original
level entry. Existing projects are never overwritten by the importer.

For an explicit edition, pass its extracted assets folder to the editor:

```sh
build/package-arm64/reminiscence2-editor \
  --assets=/path/to/extracted/edition \
  --export-native-campaign=/path/to/NEW/campaign-folder
```

The export command creates all seven levels without opening a window. It writes
to a staging directory and publishes the completed folder together. The GUI's
**Edit full native campaign** uses the same importer. Source game files remain
untouched. Imported projects contain decoded original script tables and should
stay private with your game assets.

## The three workspaces

1. **Level Editing:** room art, Back/Playfield/Front, collision and ledges, exits,
   and the player start. Native room slots remain stable.
2. **Module Placement:** a palette of the native puzzle groups, every original
   actor and hidden helper, plus authored extension modules. Select a puzzle and
   choose **Place complete copy / move**.
3. **Module Editing:** **Puzzle connections / ports** displays actors and their
   dependencies. Select an actor to reconnect typed **Target** ports or open its
   original state-machine rules. **Make behavior private** isolates a shared
   rule table before editing it.

Generated names describe detected behavior families (door/collision control,
transport, dialogue/terminal, pickup, credits/mission, lift, enemy/NPC, etc.). They
are starting labels, not verified narrative puzzle names. Rename or regroup them
in the rule workspace. Some groups are single independent actors, not puzzles.

## What a native module owns

Every actor has exactly one ownership group. Fixed connections are derived from
reviewed interpreter operands, not from any number that happens to resemble an
actor ID. Connected groups span rooms and include holding-list helpers.

The player, pistol/projectile/casing, global weapon controller and edition-specific
engine-owned actors remain shared services. Dynamic collision searches, inventory
ownership, replies to message senders and global campaign state remain real native
lookups. They are listed separately from fixed ports. A door may use its switch's
actor ID *and* require a particular message value: inspect both its ports and rules.

**Move/copy complete puzzle** follows incoming and outgoing fixed connections
beyond a manually split group. The dialog shows the resulting actor count and a
source-to-destination room map. Copies get new actor slots, private behavior tables
and remapped internal references. External shared-service links stay attached to
the existing service. Room-valued operands follow the room map; counters, object
classes, sound IDs and other numeric fields are never blindly relocated.

Hidden helpers retain their holding-room/initial coordinate setup. Their scripts
still decide when to enter the world or an inventory. The full original InitPGE
template is copied, including sprite/icon, flags, skill, collision span and text
metadata. Native resource identity stays separate from the private rule table:
monster graphics and transport state copying still use the correct original identity.

Authored graphs whose Actor bindings all belong to the bundle travel with it;
bindings and message senders inside the copy are remapped. A graph binding actors
both inside and outside the bundle must be split before copying/moving. Assets
remain referenced within the same private project. All operations validate a
candidate before replacing the document and support undo/redo.

Room art, painted collision and screen exits remain level layers; moving a module
does not copy a room. Review the destination geometry before playing. Native door
scripts update their own collision spans at runtime.

## Watch the real puzzle execute

Launch from the desired room, return to the editor, then use **Pause native**,
**Step native**, or **Resume native**. Step advances one native simulation frame
and pauses at the next boundary; it is not a single opcode instruction. A native
cutscene can defer boundary commands until it ends or is skipped.

The connections view shows:

- Current state, life/counter value, room/position and active/sleeping status.
- Fixed dependency wires and observed runtime message links.
- The latest matching delivered message highlighted in gold.
- A bounded trace of sender, recipient and message value; select one to inspect
  an actor. Only messages actually accepted into the native message queue appear.

The rule view retains the executed-row/state trace. Authored extension graphs
have their own node-state/signal monitor. Monitoring requires matching native project content. After **Apply live**, the
monitor follows the new revision; old trace highlights are cleared, while the
actual game's queued messages remain intact.

## Live editing: what changes without restarting

Use **Pause native → edit → Apply live → Step native / Resume native** to inspect
one change carefully. **Apply + resume** performs the same checks and continues
play automatically on success. A rejected update leaves the world unchanged and
paused, with a specific reason. Apply is explicit: typing in the editor does not
silently change a running puzzle.

| Edit | Live behavior |
| --- | --- |
| Room art and compatible collision/exits | Applied as authored differences; preserves puzzle-modified cells and rejects new collision overlapping live actors |
| Rule conditions/actions, certified operands, transitions, movement/flags, enabled state | Applied without resetting animation cursors, counters, inventory or actor activity; existing row indices must retain their animation-state types |
| Append a rule branch | Supported when existing rows remain stable and all destinations validate |
| Make behavior private | Supported when the copy retains the source resource identity and row layout |
| Rewire a certified actor/room dependency | Supported; new evaluations use the new connection; already queued messages keep their original destination |
| Initial health | Supported only for a living actor whose current and initial health still match the last applied document; does not overwrite damage or resurrect actors |
| Move actor, change facing/initial state, remove/restore/copy actors | Restart required: room lists, collision, inventory and activation may already differ from initial setup |
| Change existing row state types/remove rows, change resource identity, switch level | Restart required |
| Add/change authored extension modules | Restart required; running dialogue, timers and graph state are not migrated to changed definitions |

Updates run at a complete simulation boundary. All validation, image checks and
replacement table allocations finish before committing changes. Compatible
extension graphs keep their progress and receive the updated native definition.
Rewind history is cleared after application. Cutscenes, native dialogues and menus
can defer the boundary until they finish or are skipped. **Stop native** remains
available. Custom standalone scenes still use Stop → edit → Play.

## Understand protected operands

In **Module Editing → Actor state-machine rules**, select a rule to inspect its
two conditions, action, transition, movement and flags. Operands show a reviewed
meaning and either an editable range or a **Source values** selector. Expand
**Operand help / raw value** for the opcode, decimal/hex value, original values
and the reason for a restriction. Unknown semantics are described as unreviewed,
not assigned guessed meanings. The editor and validator share the range policy.

Expand **Actor data / protected operands** in either the rule or connection
inspector to see every `data[0..3]` slot, including numeric fields with no wire.
Each lists its reviewed uses and why it is protected or edited through a typed
port. Disabled rules still count when determining mixed use. Changing a field
that serves both an actor reference and a counter could break a puzzle, so these
fields remain visible for study without unrestricted writes. Full validation
still checks source constraints, ownership and resource compatibility.

Shared behaviors affect every owner. The rule inspector shows the owner count;
use **Make behavior private** before experimenting on one actor. Module and node
comments are a place for the designer's observations, separate from source data.

## Campaign coverage and limits

The checked Amiga, DOS/GOG and Sega CD editions each contain 1,097 actors:

| Level | Actors | Capacity for additional actor slots* |
| --- | ---: | ---: |
| 1 | 107 | 148 |
| 2 (New Washington) | 256 | 0 |
| 3 | 176 | 79 |
| 4 | 190 | 65 |
| 5 | 90 | 165 |
| 6 | 109 | 146 |
| 7 | 169 | 86 |

\* A copy requires space for its complete connected bundle. New modules cannot
allocate slot 255 because native inventory uses that value as its sentinel.
Original actor 255 is retained where present. The editor checks capacity before
editing anything; New Washington supports moving, reconnecting and modifying its
existing modules, but cannot add actor copies.

These capacity limits are intentional and remain part of the native format.

Other concrete boundaries:

- Copies stay in the same level and edition. Cross-level/resource-bank imports
  and portable native prefab files are not supported by this patch.
- There are at most 255 behavior slots. Private tables use currently unowned
  slots; actor provenance records the original template. Exhaustion is reported.
- Newly mixing different native enemy sprite/palette banks in one room is
  rejected. Use separate rooms for those enemy families; existing original
  scripted arrangements are retained. Arbitrary per-actor palettes are future work.
- Mixed-use data operands remain protected. Unreviewed opcode operands retain
  the original behavior's vocabulary. Original animation state entries cannot
  be removed; disable unwanted rows instead.
- Dynamic lookups and global story effects can intentionally couple modules.
  A copy is an independent set of actors/rules, not a separate copy of the player,
  inventory, cinematic manager or campaign progression.
- Native authored-graph progress is preserved across compatible live updates.
  It is outside ordinary native save/rewind state; authored campaign snapshots
  have their own progress format. Native base actors still use native saves.
- Macintosh assets were unavailable; this workflow is validated with the three
  installed editions above. Tests cover original-state parity, every copyable
  bundle's references, representative execution, and editor interactions—not a
  human playthrough of every possible campaign branch.

## Developer contract

Project schema 6 adds `source` to NativeActor and NativeProgram. `-1` means the
original slot; a nonnegative source identifies the pristine actor or behavior
in the same source level. Original actor IDs cannot change. New actors append;
all other InitPGE fields come from a pristine template snapshot taken before
any installation writes. Program memory is compiled transactionally into
Resource-owned allocations.

| Native operation | Reference interpretation |
| --- | --- |
| `23..26`, `38..3b` | data[0..3] message recipient / required sender |
| `41`, `42` | data[operand] wake/sleep target |
| `49` | data[0] actor excluded from a collision search |
| `65`, `66` | data[operand] credit holder; next data slot is a numeric amount |
| `82` | data[operand] transported actor; next slot is destination actor |
| `30`, `31`, `4a`, `58`, `59` | literal actor operand (inventory, position or life) |
| `57` | data[operand] room destination |
| `44`, `48`, `6c`, `58`, `5c`, `5f`, `87` | numeric data reads: counters/classes/life/delay/distance/sound |

Uses are combined across the behavior's full rule table, including disabled
rules. A data field used as both an actor reference and a scalar is not freely
rewireable. Validation also compares against the source behavior's interpretation;
deleting a condition cannot turn an original numeric field into an unchecked link.

`editor/native_modules.cpp` owns port analysis, closure, isolation, placement and
installation; `editor/native_logic.cpp` retains rule capture/validation/compilation.
`editor/native_live.cpp` checks compatibility with the actual running actors
before a live transaction. NativeSession installs replacement tables between
frames, preserving interpreter row indices and InitPGE addresses; it adopts the
new definition in an unchanged extension Scene without resetting its progress.
`AuthoringHooks::behaviorIdentity` preserves sprite-bank and transport semantics.
`messageSent` observes accepted deliveries without changing queue ordering.
Interpreter guards bound actor/data indices, inventory ancestry/traversal and
collision writes when authored rewiring exposes unusual branches.

`tests/editor_native_live_checks.h` exercises rule replacement, private behaviors,
appended branches, typed rewiring, health conflicts and transaction rejection
against each installed edition's seven levels. It checks that live actor memory,
inventory/counters and queued messages survive compatible updates. The separate
`tools/check_editor_session.py` drives the real player command protocol, including
**Apply + resume** and rejected edits that must remain paused. Synthetic operand
and module tests run in public CI without distributing original game scripts.
