<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Editor projects (schema 9) and portable modules (schema 7)

Implemented by `editor/content.*`, `graph.*`, `graph_runtime.*`, `image.*` and
`runtime.*`, `native.*` and `editing.cpp` in engine 1.47.
The [editor guide](../LEVEL_EDITOR.md) describes authoring. This format is separate
from original Flashback PGE/OBJ/CT files and from ordinary campaign save files.
The human-readable JSON uses UTF-8. Paths are relative to the JSON's folder.

## Project document

See `examples/editor/workshop/project.r2project.json` for a complete executable
example. Fields emitted by the serializer are required; legacy schema-1 modules may omit
`nodes` and `wires`, which default to empty. Project schemas 1, 2, 3 and 4 migrate on load. Missing `assets` defaults to an empty
library; new node fields `speaker`, `choices`, `duration` default to empty, two
empty strings, and 30 ticks respectively.

| Field | Contract |
| --- | --- |
| `schema` | Integer 9; schemas 1..8 migrate to 9; other versions rejected |
| `id` | Stable campaign ID; letters, digits, underscore/hyphen, 1–80 bytes |
| `name` | Display name, at most 160 bytes |
| `serial` | Counter for locally unique room/module IDs; bounded signed-32-bit positive range |
| `rooms` | 1–64 room records |
| `modules` | 0–64 module records |
| `spawn` | `{room, x, y}`; room ID plus logical player anchor |
| `spawnLeft` | Boolean; otherwise the player starts facing right |
| `native` | Native campaign descriptor below; omitted in older custom projects |

Room/module IDs must be unique within the project. Custom project identity does not depend
on vector position. Native campaign slots are the exception described below. Compilation assigns room indices, so reordering serialized
rooms does not redirect links or module anchors.

Each room has `id`, `name`, `column`, `row`, `art`, `collision`, `ledges`, `exits`.
World column/row are integers in −128…128 and serve layout/slicing. Two rooms cannot occupy the same world coordinate. Runtime links
come from `exits`, an array of exactly four room IDs in **up, down, right, left**
order. An empty string means no neighbor. One-way links are valid in the file;
the initial inspector authors reciprocal links.

`art` has exactly three records ordered **Back, Playfield, Front**:

```json
{"file":"art/whole-level.png", "x":256, "y":216, "w":256, "h":224}
```

An empty filename means no image. Coordinates and dimensions refer to the source
image, not collision cells. Crops map to the 256×224 logical room. Out-of-image
crop areas remain transparent. The file is retained once regardless of crop count.
PNG alpha is composited in layer order; JPEG/BMP are also decoded. Partial alpha
uses straight-alpha source-over with integer rounding. Runtime actor coverage is
an explicit mask written by sprite rasterizers; no background color is reserved
as an implicit transparency key.

`collision` contains exactly 112 byte-valued integers in row-major order, 16×7.
Zero is empty, one is ordinary blocking, two preserves the original special
collision bit semantics. The initial brush authors only zero and one. Ledges are
`{column, row, facing}`; columns 1…14, rows 2/4/6, facing −1/+1 for climbing
left/right. A ledge stamp supplies a solid floor cell and clears the cell above
it, plus the corresponding two cells on its approach side. It does not erase
the authored base grid; compilation applies it to a copy. Ordinary cells remain
responsible for the rest of a platform.

## Module records

| Field | Contract |
| --- | --- |
| `id`, `name` | Same identifier/name limits as rooms |
| `kind` | 0 door+activator; 1 prop; 2 health pickup; 3 crawler; 4 graph |
| `a`, `b` | `{room,x,y}` anchors. Door owns both; other kinds use A |
| `initiallyOpen`, `toggle` | Door state and direct activation mode |
| `facingLeft` | Crawler facing |
| `health` | Pickup restoration amount; crawler keeps original scripted vulnerability/death behavior |
| `width`, `height` | Logical display dimensions; door also uses these for blocking |
| `sprite` | Art record; horizontal animation strip for a prop |
| `frames`, `fps` | Horizontal frame count and integer rate, max 128 and 30 |
| `layer` | 0 Back, 1 Playfield, 2 Front |
| `outputs` | Legacy IDs of doors or graphs containing doors; graph targets open all their doors |
| `assets` | Up to 64 owned Art records, including images not yet assigned to a node |
| `nodes`, `wires` | Graph node and event-wire arrays, empty for kinds 0–3 |

Anchors use X=0…248, Y=0…216. Player spawn Y must be below 216. Dimensions are
bounded to 256×216; health to 1…100. Doors additionally require X and width
multiples of 16, Y/height multiples of 72, the whole panel inside the room and
height ≤ anchor Y. UI defaults use a 16×72 panel and an activator 48 pixels away.
Props may extend above a room and are clipped during drawing.

An activation carries a bounded visited set. The originating toggle door may
toggle; downstream targets only open. A cyclic graph therefore terminates after
visiting each module once. Closing is refused while Conrad or a live native enemy overlaps the panel.
Holding Enter does not continually reactivate: the runner records the previous
input state and reacts to a fresh press. Pickups emit activation when consumed.
Props/crawlers do not currently emit gameplay events.

A portable `.r2module.json` contains `schema: 7` (schemas 1..6 remain readable) and one `module` record. Saving
clears outgoing connections and substitutes local room placeholders. Loading
allocates a new ID, translates anchors and graph parts by the same delta and
assigns them to the chosen room. Validation precedes dependency copying; the
project ID counter commits only after import succeeds. All node sprite states and owned library images
and audio files are included in dependency copies. Scoped node IDs and internal wires survive.
The editor may subsequently move either endpoint into a different room. A
module's embedded pair does not depend on the `outputs` list.

## Graph records and execution

A kind-4 module has up to 32 `nodes` and 128 `wires`. There are at most 256 graph
nodes across a project. Node IDs are unique **inside the module**; separate
module instances can use the same node IDs without sharing state. A duplicate
retains node IDs, internal wires and item requirements, with a new module ID.

Node fields:

| Field | Contract |
| --- | --- |
| `id`, `name` | Same ID/name rules as module records, locally scoped |
| `kind` | 0 Button, 1 Door, 2 NPC, 3 Item, 4 Computer, 5 Zone, 6 Relay, 7 Prop, 8 Dialogue, 9 Animation, 10 PickupAnimation, 11 Timer, 12 State, 13 Actor |
| `at` | Room ID and X/feet-Y anchor; node width must fit the room |
| `graphX`, `graphY` | Integer diagram position 0…8192; separate from in-game position |
| `width`, `height`, `layer` | Display bounds/layer; door alignment also defines collision |
| `visuals` | Exactly two Animation records, idle then active/completed |
| `enabled`, `initiallyActive` | Initial interaction gate and door/open state |
| `once` | Button, relay, zone and action-node one-shot flag |
| `requiredItem` | Empty, or a scoped Item node ID required by this NPC |
| `consumeItem` | Clear that item's collected flag after successful NPC hand-in |
| `text`, `doneText` | Before/after dialogue, max 1024 UTF-8 bytes each; native font displays ASCII |
| `goal` | Computer objective count 1…32 |
| `speaker` | Dialogue speaker, up to 28 bytes |
| `choices` | Exactly two strings, each up to 26 bytes; both empty or both populated |
| `duration` | Timer delay 1…18000 ticks (30 Hz) |

An Animation is `{art, frames, fps, loop}`. `art` is the same source crop record
used elsewhere. Frames advance horizontally. Frame count 1…128, FPS 1…30.
Non-looping clips hold their last frame; door active clips default to non-looping.
Animation age starts at the corresponding state transition. Empty active artwork
falls back to idle. Images are preflighted, including all frames and states,
before creating a playtest. Editor-only invalid intermediate edits stay undoable.

A wire is `{from, to, signal, input}`, referencing scoped node IDs. Signal numbers:
0 Used, 1 Completed, 2 Opened, 3 Closed, 4 Entered, 5 Started, 6 Stopped, 7 Choice1, 8 Choice2, 9 Exited, 10 Hurt, 11 Died, 12 Reached, 13 StateChanged. Input numbers:
0 Activate, 1 Open, 2 Close, 3 Toggle, 4 Progress, 5 Reset, 6 Enable, 7 Disable, 8 Stop, 9 Friendly, 10 Hostile, 11 Patrol, 12 Hold.
The [node/port table](../EDITOR_MODULES.md#node-behaviors-and-ports) defines allowed
combinations. Invalid ports, duplicate wires and unresolved IDs are rejected.
NPC `requiredItem` appears as a separate gold connection, not an event wire.
Deleting its target leaves an invalid requirement until deliberately repaired.

`GraphMachine` uses a FIFO queue with a per-transaction set of delivered wire
indices. A wire is enqueued at most once per transaction, bounding cyclic graphs
without recursion. Mission progress stores a set of contributing node indices;
repeated events from one node cannot impersonate several objectives. Item
collection is one-shot; consuming an item does not respawn it. Reset clears a computer or rearms an action node. Enabling/disabling gates behavior without changing collision.
Each playtest has separate state and no author JSON is mutated during play.

NPC use checks the module's item state, optionally consumes it, emits Completed
once, and displays paginated text. Mission acceptance is explicit; previously
completed objectives are retained until Reset. Dialogue pauses actor simulation
and requires fresh Enter presses. Pickup notices expire after 120 ticks. Native
controller/keyboard input uses the same interaction edge in the command-line
player. Editor keyboard controls remain fixed. Graph output door transitions
also preserve legacy `outputs` when converting an existing paired door.

## Runtime and isolation

The runtime compiles 7424 CT bytes: 256 neighbor bytes (four groups of 64), then
64×112 collision bytes. Missing rooms/exits use −1. Closed door panels overlay
blocking cells onto that compiled base. Opening restores the base collision,
including any independently authored wall that still occupies the opening.
Disabled doors retain both their visible panel and collision. Custom scenes also
sweep actor cell centers across closed gates after native simulation, preventing
animation strides from crossing a narrow panel. A blocked step retains its prior
horizontal coordinate rather than snapping to a different stride phase. This
guard runs before node teleports and does not alter native-campaign physics.

For the supported private Amiga/DOS/Sega CD editions, runtime loads level one's
original resource library, then replaces its actors with a reviewed player
bundle and the selected crawler instances. The bundle includes the player,
inventory gun, projectile, casing, and invisible Space-key latch controller.
The reviewed reference fields are relocated: player `data[0]` → gun,
gun `data[0]` → projectile, projectile `data[3]` → casing. Other integers remain
scalars. The controller uses node 228/229, type 316; player/gun/projectile/casing
nodes are 1/4/32/37. The crawler uses node 161, type 939. Unknown bundle signatures
are rejected. Adding another prefab requires reviewing its full dependency
closure, graphics banks and message references; cloning arbitrary actors is
not supported by the custom-scene palette. Native campaigns now have a separate
[typed bundle relocation path](../NATIVE_CAMPAIGN_MODULES.md).

Custom scenes call the same `Game::simulateActors()` as normal gameplay at
30 ticks/second. They do not run original story/level-change orchestration or
write ordinary saves. Source game files are read-only. Frame pacing shares the
native fractional accumulator and compensates for scheduler overshoot. Controller
disconnect pauses actors and graph clocks until a fresh confirm gesture, which
is consumed without activating a puzzle. Scene restart restores the most recent
standalone checkpoint's actors, modules and local quest values, or re-creates
the project state when no checkpoint exists. A custom deleter stops
and joins mixer audio before the resource member can free samples, including
initialization failures. Only one scene is active in the native editor.

Renderer order is Back art/modules, Playfield art/modules, original actors,
Front art/modules. Module wires and editor collision/ledge markers never reach
the gameplay compositor. Source artwork is currently sampled at native room
resolution; retained source resolution does not change physics or room limits.
Each of the four original sprite queues now holds up to 255 states, with a
guard before insertion. This retains the byte-sized empty sentinel and protects
crowded authored scenes instead of relying on the original rooms' small queues.

## Bounds, persistence and dependencies

- Project/module JSON: at most 4 MiB, nesting depth 32; floating-point numbers
  and integers outside signed 32-bit range are rejected before narrowing.
- Image files: at most 64 MiB, width/height at most 16384, at most 32 megapixels
  decoded per image. PNG/JPEG/BMP only. Cache: 256 files, 256 MiB decoded total.
- Relative image paths cannot escape via `..`, absolute paths or symlinks.
  Backslashes, colons and embedded NULs are rejected for portable path handling.
- Image imports use a CRC/size content name with full byte comparison on an
  existing destination. CRC is a deduplication aid, not an authenticity claim.
- JSON and imported files use the existing atomic-file writer. Save As copies
  referenced images to the new root; no absolute machine-specific asset path
  is serialized into a project.
- nlohmann JSON 3.11.3 and pinned stb_image sources/notices are under
  `third_party/editor/`, with download URLs and SHA-256 in `source-manifest.json`.
- The public Workshop images are generated by `tools/create_editor_example.py`;
  Module Lab by `tools/create_module_workshop.py`.
  Original sprites, scripts, sounds, manuals and extracted game data are absent.

## Original-level scenery studies

`editor/original.*` uses the same native initialization and decoders as embedded
playtests to inspect any of the seven levels. Reference mode keeps the original PGE records and
room-slot IDs intact, renders initial poses without simulating scripts, and
never writes source assets. It has a separate silent platform instance.

Scenery export writes a regular schema-6 custom project into a new directory only:

- 64 rooms in original slot order, named `original-room-N`; an 8×8 slot shelf
  supplies unique editor world coordinates without guessing geographical layout.
- All 7,168 raw collision cells retained exactly (supported values 0..2), and
  valid 0..63 room exits retained; other exit sentinels become sealed links.
- The original player position/facing; no imported modules or inferred ledges.
- Flattened scenery on Back and native bit-7 priority pixels on Front, using
  exact 8-bit RGBA PNGs; Playfield remains empty. Priority is an approximation
  for the custom renderer, whose actors all render before Front.

The exporter refuses unsupported collision values rather than normalizing them.
Each PNG and the final project file use atomic writes. Dependencies and a local
README are completed before publishing the project JSON. A failed export may
leave a new, incomplete directory; it never replaces an existing project.
Older schema-2 studies migrate when opened; newly saved studies need a schema-6
reader. Scenery studies contain proprietary art and are not public examples. They omit native script,
actor, palette-animation and campaign state; they are not an alternate save
format or a lossless campaign import.

## Native campaign descriptor and live session

Schema 6 emits `native: {enabled, level, edition, fingerprint, actors, programs, puzzles}`. Custom
projects use `enabled=false` and no native actors. Native projects require:

- `level`: 0–6; `edition`: the engine ResourceType enum for the chosen assets.
- `fingerprint`: eight lowercase hexadecimal digits from `gameplayFingerprint`
  of the **original** loaded level. This checks compatibility, not authenticity.
- Exactly 64 rooms in original order, IDs `original-room-0`…`original-room-63`.
  Original out-of-range exit sentinels survive unless that exit is edited.
- Extension graph modules may bind native actors; collision Door nodes require a
  custom scene. `actors` contains every original slot in order, at most 256.
  Each record has `{id,room,x,y,life,facingLeft,type,behavior,objectKind,data,source}`.
  `data` is exactly four original signed operands; these mix IDs and counters.
- Original `id`/`objectKind` retain their source identity. New actors append, with
  `source` identifying the pristine actor template. `-1` means its original slot.
  Unexported InitPGE fields come from that template. New copies cannot use actor
  255, the inventory sentinel; original 256-actor levels remain supported.
- Behavior tables likewise carry `source` (`-1` = original slot). A private copy
  uses a free behavior slot, preserves the source animation vocabulary, and
  retains resource identity for sprite-bank/transport checks.
- `data` edits are allowed only for unambiguous actor/room ports certified against
  both current and source behavior usage. Scalars and mixed uses stay protected.
  Literal actor operands are relocated in private behavior tables.
- Room 0–63 actors may have edited placement, facing and initial life; actor 0
  matches `spawn`/`spawnLeft`. Holding-list helpers retain initial placement/life,
  while their typed links can be remapped with the complete bundle.

See the [native module contract](../NATIVE_CAMPAIGN_MODULES.md#developer-contract)
for opcode/reference semantics, copy closure, authored-graph ownership and limits.

`level-1.r2project.json`…`level-7.r2project.json` form a campaign in one folder.
The normal Game loop loads the next sibling on progression, retaining that
level's complete native script dependencies. A missing sibling uses original data.
Scenery is decoded from the selected edition unless a room art field overrides it.
Import writes editable JSON including decoded original behavior tables. Source
asset files stay untouched; imported native campaigns are private asset-derived
projects, not redistributable clean-room examples.

`--project=FILE --editor-session=CONTROL` launches a controlled native player.
The editor starts this process with exact argv, without a shell. The control file
is atomically replaced JSON: `{id, action, project?, start?}`. Actions are `pause`,
`resume`, `apply`, `restart`, `play-here`, `play-start`, `stop`. `project` is a
serialized project string, required for apply/restart/play-here/play-start.
`play-here` additionally requires `start: {room, x, y, facingLeft}`: room ID,
integer X 0..240, integer Y 0..214 and boolean facing. Coordinates outside these
bounds, floating point values and oversized integers are rejected before casting. The envelope is limited to 4 MiB and depth 8; project parsing has
its own size/depth limits. The sidecar `CONTROL.status` returns `ack`, zero-based
`level`, `room`, `x`, `y`, `paused`, `restartRequired`, `message`. Wait for matching ack
before publishing another command; stop may supersede pending work.

Commands execute at the start of a complete simulation frame. Blocking native
cinematics, dialogue and menus finish before reaching this boundary; the SDL
input pump separately accepts stop. Apply preflights metadata, images and the
entire collision transaction before mutation. Only authored CT differences are
written. Runtime cells differing from the previous authored values indicate a
script conflict and reject the transaction; new solid cells over a live active
actor also reject it. Actor setup changes require explicit restart. Runtime actor
lists, pointers, messages, inventory and unrelated puzzle collision stay intact
on live apply. Rewind is cleared after a successful edit. Restart reloads the
native level and initializes its complete actor table, losing in-level progress.

`play-here` validates authored collision, a mapped room and the original standing
player/pistol dependency before restarting. A transient launch override changes
only the runtime player InitPGE position, facing and standing type; project actor
0 and its authored spawn stay unchanged. All native IDs, OBJ/ANI resources and
helpers survive. The skipped arrival script's pistol attachment is performed
using the original inventory helper and gun ID. The cinematic is cleared and
play starts immediately. Score, journal, play time and random seed reset for
repeatable runs. Earlier quest progress is not synthesized. `play-start` restarts
at the authored start with the normal arrival/cinematic and resumes immediately.

`editor/playtest.*` computes automatic starts near the authored spawn (when in
that room), a selected graph node, or a room default. It searches the normal
floor heights for body clearance and support, including initially closed custom
doors. Explicit workspace TEST markers require clearance but may intentionally
start above a gap. Markers are per-room workspace state, absent from project JSON
and undo history. Custom tests override only their disposable Scene snapshot's
spawn; native tests use the command's `start` envelope.

The compositor wraps native drawing only in rooms with authored art, preserving
sprite coverage, native foreground priority (unless replaced), HUD and modal
indexed draws. Rooms without overrides use the original renderer. The editor's
reference rendering is an initial-state preview; scripts execute in the separate
player window. Session controls and logs remain beside the private project;
preview saves use `.native-playtest-saves` rather than ordinary campaign saves.

## Editor preferences (1.41)

`.editor-settings.json` is folder-local UI state, separate from the project schema:
`{schema: 1, authoredStart: boolean, nativeMarkers: boolean}`. It contains no asset
paths or game state. Reads require a supported integer schema, boolean fields,
maximum 1 KiB and bounded nesting; failure restores UI defaults with a diagnostic.
Writes are atomic. Save As carries these preferences to the destination; Open
loads the destination folder's preferences. Sibling native level documents share
the sidecar. The normal game ignores it and Git excludes it.

## Asynchronous actions (1.42)

Dialogue, Animation, PickupAnimation and Timer start only through Activate;
`initiallyActive` is rejected for these kinds. Runtime state stores an activation
`generation`, start tick, active/completed flags and the ordinary enabled gate.
Activate while active is ignored. A one-shot completed action needs Reset before
another activation. Stop/Disable cancels an active action and emits Stopped;
Reset also clears completion. None of these cancellation inputs emits Completed.

Dialogue requests retain module index, scoped node index and generation. Scene
queues requests in trigger order and removes canceled/stale generations before
showing them. Finishing the last page emits the chosen branch (if present) and
Completed. Stale callbacks cannot finish a restarted action. Legacy inline NPC
text uses the same queue without a node-completion callback. Player simulation
pauses while reading; graph action clocks keep advancing at 30 Hz. Input edges
prevent held Enter from dismissing multiple conversations or triggering a nearby
button, and held Up/Down from repeatedly changing the selected choice.

For a non-looping animation, completion occurs after
`ceil(frames * 30 / fps)` ticks. The active strip is used if supplied; otherwise
idle supplies both image and timing. Timers use `duration` directly. Due actions
are snapshotted by generation before dispatch, so a wire that restarts another
node cannot complete its new run using an old deadline. A looping animation
never completes automatically. PickupAnimation hides when inactive; Animation
holds its completed final frame. The regular wire-delivery budget still applies
to Started/Stopped feedback cycles.

The GUI library scans at most 128 `.r2module.json` files directly inside the
project's `modules/` folder. Templates and image dependencies use the existing
bounded/validated prefab loader and content-addressed image importer. Export
preserves unused owned images as well as images referenced by node clips.

## State machines, actors and native programs (1.43)

Project schema 5 and prefab schema 4 add NodeKind 12 State and 13 Actor.
Older enum numbers remain unchanged. Signals append Exited=9, Hurt=10, Died=11,
Reached=12 and StateChanged=13. Inputs append Friendly=9, Hostile=10, Patrol=11,
Hold=12; Activate on a native Actor binding sends its configured message.

New node fields/defaults: `stateGroup="main"`, `friendly=false`, `patrol=false`,
`patrolLeft=32`, `patrolRight=208`, `speed=1`, `health=2`, `nativeActor=-1`,
`nativeRule=-1`, `message=1`, `messageFrom=0`. State groups are 1–80 bytes with
at most one enabled `initiallyActive` State each. Patrol bounds are 0–240,
at least 16px apart; patrol spawns lie within them. Speed is 1–4, health 1–100.
Native bindings require a unique non-player actor and a valid message sender.

Native `programs` retains original behavior-slot order. Each entry is either
`{rules:[...]}` or an integer backward reference to an identical earlier table.
Aliases expand into independent value records in memory. Limits: 255 programs,
24,000 expanded rules, 4 MiB serialized project, 4096 editable rows per behavior.
The native compiler allocates a complete replacement before handing malloc-owned
ObjectNode arrays to Resource; it never installs pointers into editor vectors.

A rule is `{type,nextType,next,dx,dy,flags,opcode,argument,enabled}`. Opcode and
argument arrays have exactly three elements (condition 1, condition 2, action).
Types must use existing animation states from that behavior. `next` is a valid
row whose `type` matches `nextType`. `dx/dy` are signed bytes; flags are a byte;
opcodes are 0–139 and operands signed 16-bit. Opaque opcode/operand pairs must
occur in the original behavior. Validated scalar exceptions are no-op, counter,
life changes and message numbers for message opcodes already used there.
Disabled rules retain their index and are filtered at execution; iteration is
bounded even if no remaining rule matches.

`puzzles:[{name,actors:[...]}]` is editor ownership metadata. Every native actor
belongs to exactly one group when groups are present. Grouping does not change
engine identity or resource tables. Old projects lacking programs/groups are
upgraded from the matching installed edition. They cannot introduce unchecked
initial state IDs through that legacy path.

Graph runtime queues, active state, trace and dialogue generations are never
serialized into authoring JSON. Signal delivery uses a bounded FIFO and a
per-transaction visited-wire set. The debugger and game use GraphMachine with
identical transitions; debug mode retains the queue for single-wire stepping.
Native status carries at most 128 native events and 128 graph trace records
(up to 16 per module), plus current node flags. Monitor snapshots include a
stable content revision and are rejected for a changed graph. Status remains
bounded to 64 KiB. Graph progress is not serialized into native savegames.


### Native monitor additions (1.44)

Status adds `nativeRevision` (decimal string of the canonical native descriptor's
64-bit FNV-1a content hash), `messages` (at most 128 rows of
`[source,target,value,serial]`) and `actors` (at most 256 rows of
`[id,room,x,y,state,life,counter,active]`). Hashes reject stale display data, not
malicious content. Actor and message monitoring require the current descriptor's
revision. Existing rule events remain bounded to 128. Shared serial numbers
order rule executions and accepted message deliveries. The `step` command runs
one native simulation frame before pausing at the next boundary.

## Schema 7 additions

Module, node, native actor, native rule and native puzzle `comments` are optional
on migration and default to empty (maximum 2047 bytes). They are preserved in
exports/duplicates and excluded from runtime identity hashes. Native actor
`removed` defaults to false; true suppresses that actor while preserving its slot.
Shared player/weapon/service actors cannot be removed. Original actor slots are
never compacted.

Node kind IDs 0..13 retain their existing meaning. New IDs: HealthPickup=14,
Damage=15, Counter=16, ItemCondition=17, Checkpoint=18, Teleport=19, LevelExit=20,
Start=21. Counter uses `goal`; health/damage uses `health`; Has item? uses
`requiredItem` and `consumeItem`; Teleport uses `at`; LevelExit uses `text` as a
route name (1..80 bytes). Existing Input/Signal numeric IDs are unchanged. New
portable module exports use schema 5; readers still migrate module schemas 1..4.

See [campaign schema 2](../CAMPAIGN_EDITOR.md#developer-contract) for separate
`.r2campaign.json` route and presentation documents.

## Schema 8 additions (1.46)

Node `campaignKey` defaults to empty. Keys use 1..64 ASCII letters, digits, dot,
underscore or hyphen. Items with a key write a shared held-item value; NPC and
ItemCondition nodes with that key test/consume it instead of their local reference.
Keys on a locally referenced Item also work. Unkeyed items preserve old behavior.

CampaignSet=22 and CampaignTest=23 extend the palette without renumbering existing
nodes. Set uses `initiallyActive` as its true/false value, Activate writes it and
emits Completed, Reset clears it. Test emits Choice1/Choice2 for true/false on
Activate. Flags are separate from held-item keys. Standalone scenes share values
within that scene; the graph debugger keeps isolated values for its simulation.

Portable module schema 6 stores these fields; module schemas 1..5 still import.
Campaign progress has a separate schema 1, documented in CAMPAIGN_EDITOR.md.


## Imported audio (project 9 / module 7)

NodeKind 24 Music and 25 Sound append to the existing enum. All node records gain
`audioFile` (safe project-relative path, default empty), `audioVolume` (integer
0..100, default 70), `audioLoop` (boolean, default false). `duration` stores the
ceiling of decoded frame count × 30 / sample rate. Import computes it; validation
requires it to match the file. Old records receive default fields on load.
Music/Sound must start through Activate, never `initiallyActive`.

Both use the common action state machine. Looping audio never emits Completed;
Stop and Disable emit Stopped, Reset also clears completion. Active state and
generation are saved with the graph; the PCM cursor is reconstructed from saved
age at 30 Hz. Audio positions are not sample-accurate save fields. The graph
simulator uses duration/loop flags but does not open an audio device.

`editor/audio.*` decodes bounded PCM16 RIFF WAV or Ogg Vorbis at scene load into
interleaved stereo. Source limits: 1–2 channels, 8000–192000 Hz, 600 seconds,
64 MiB file and 64 MiB decoded PCM per clip. Whole-level decoded PCM is bounded
at 128 MiB with at most 32 audio nodes. Imports use content-derived filenames
under `audio/`, reject path escapes and preserve colliding existing files.
Save As, campaign level import, prefab export/import and clipboard transfers copy
these dependencies. Missing/corrupt audio blocks validation/play before callbacks
are installed.

The independent mixer content hook adds authored PCM after native effects and
before master/mute, with separate music/effects gains. Voice data is immutable
except cursors and state protected by the SDL audio lock. Linear interpolation
resamples to output rate. No allocation, graph callbacks or file I/O happens in
the audio callback. Destruction removes only the owning hook before freeing PCM.
