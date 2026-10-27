// SPDX-License-Identifier: GPL-3.0-or-later
// Included inside Workspace; all guidance is original project documentation.
void guidelinesUI() {
	if (!guidelines)
		return;
	ImGui::OpenPopup("Level design guidelines");
	const auto screen = ImGui::GetIO().DisplaySize;
	ImGui::SetNextWindowSize({std::min(850.f, screen.x - 30), std::min(690.f, screen.y - 30)},
	                         ImGuiCond_Appearing);
	ImGui::SetNextWindowBgAlpha(1);
	if (ImGui::BeginPopupModal("Level design guidelines", nullptr, ImGuiWindowFlags_NoCollapse)) {
		helpWorkspace();
		if (ImGui::Button("Close guidelines") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
			guidelines = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

void helpWorkspace() {
	ImGui::TextColored({0.35f, 0.9f, 0.82f, 1}, "DESIGN ON THE ORIGINAL PIXEL GRID");
	ImGui::TextWrapped(
	    "The editor uses the engine's logical pixel grid. A larger display scales that grid; "
	    "it does not change jumps, ledges or room boundaries.");
	ImGui::BeginChild("guideline_text", {0, -35});
	if (ImGui::BeginTabBar("guide_sections")) {
		if (ImGui::BeginTabItem("Four workflows & nodes")) {
			ImGui::SeparatorText("1. LEVEL EDITING");
			ImGui::TextWrapped(
			    "Create rooms, import or slice artwork into Back / Playfield / Front layers, paint "
			    "collision and ledges, set exits and the authored player start. Placed modules "
			    "remain visible as references.");
			ImGui::SeparatorText("2. MODULE PLACEMENT");
			ImGui::TextWrapped(
			    "Pick an entire connected puzzle from recipes or your local module library, then "
			    "click its destination. Select and move its parts, including across rooms. "
			    "Copy/paste preserves internal wiring and gives each instance independent state. "
			    "The library is stored in the project's modules folder; Refresh scans it again. "
			    "Import module accepts a shared .r2module.json with its art folder.");
			ImGui::SeparatorText("3. MODULE EDITING");
			ImGui::TextWrapped(
			    "Create a named module, import its image assets, then add behavior nodes. Connect "
			    "outputs to inputs: Button Used -> Dialogue Activate -> Choice 1 -> Play animation "
			    "Activate -> Completed -> Door Open. Save to library exports the whole puzzle and "
			    "its images. Existing placed instances do not change when a library template is "
			    "updated.");
			ImGui::SeparatorText("4. CAMPAIGN");
			ImGui::TextWrapped("A separate .r2campaign.json links reusable levels, movies, text/photo screens, route choices and endings. Import levels into the campaign folder, connect OUT to IN, then name the routes. Level exit nodes emit names; native completion emits complete. Matching routes can offer a choice. Story screens use a 640x360 canvas with positioned text/photos, color, typewriter and fade effects. Play campaign or Play selected stage uses the actual player. Campaign progress autosaves and can resume. Revisited levels keep puzzle state unless Remember is disabled. Items can use shared campaign keys; Set/Test campaign flag nodes connect objectives across levels. Ctrl+S/Ctrl+L save/load during gameplay.");
			ImGui::SeparatorText("PICKUPS / GAMEPLAY / NOTES");
			ImGui::TextWrapped("The palette has 26 node kinds. Item / pickup collects a quest item, optionally shared across levels by its Campaign key; Pickup animation is visual only. Health pickup, Damage player, Counter, Has item?, Checkpoint, Teleport player, Level exit and On start provide gameplay building blocks. Native actors retain original inventory and other game mechanics. Music and Sound import WAV/Ogg clips with Listen previews and wired playback. Sprite inspectors preview animation strips. Each module/node has a Help and comments window. Copy, Duplicate, Paste, Delete and Rename manage authored modules. Native deletion hides/stops actors and keeps their IDs reserved; Restore reverses it.");
			ImGui::SeparatorText("STATE MACHINES AND PLAYBACK");
			ImGui::TextWrapped(
			    "States with the same Machine group are mutually exclusive. Mark one Initial "
				"state. Activate enters it and exits its sibling; Entered/Exited start or stop "
				"actions. Progress emits Completed only while active. Start graph resets a private "
				"simulation; Play/Pause and Step deliver connections one at a time. Green headers "
				"are active; gold pulses travel in the delivered signal direction. Borders glow and fade when states change. Select a node to inject Use, area "
				"entry, actor events or dialogue choices. The trace records state changes and "
				"bounded cycles. Real playtests have Inspect running graph; native extensions have "
				"Monitor running native module.");
			ImGui::SeparatorText("ACTOR NODES");
			ImGui::TextWrapped(
			    "Custom actors use native crawler artwork/AI. Set patrol bounds and health. "
				"Friendly mode suspends native AI, walks on safe floor, and emits Used on Enter "
				"nearby. Wire Used to Dialogue, or Hurt/Died/Patrol endpoint to a state. Native "
				"bindings use existing actor IDs and artwork; Native rule fired observes a rule, "
				"while Activate sends the configured native message. Restart applies logic edits.");
			ImGui::SeparatorText("DIALOGUE AND ACTIONS");
			ImGui::TextWrapped(
			    "Dialogue has a speaker, paginated text and optional two choices. Up/Down selects; "
			    "Enter continues or confirms. Completed runs only after the last page. Choice 1 / "
			    "Choice 2 can lead to more dialogue nodes. Multiple conversations queue in trigger "
			    "order; stopping a node cancels its queued text. Player motion pauses while "
			    "reading, while action timers continue.");
			ImGui::TextWrapped(
			    "Play animation uses the active horizontal strip (idle fallback); a non-looping "
			    "clip emits Completed after its full duration. Looping clips need Stop. Pickup "
			    "animation is a visual effect at its anchor, not a replacement Conrad pose; use "
			    "Quest item Completed -> Pickup animation Activate. Timer emits Completed after "
			    "the configured number of 30 Hz ticks. Activate starts, Stop cancels, Reset rearms "
			    "one-shot actions, and Disable cancels and prevents activation.");
			ImGui::TextWrapped(
			    "NPC retrieval and Mission computer nodes remain available for inventory and "
			    "objective logic. Native rule nodes expose original scripts; Actor bindings "
				"connect "
			    "them to new dialogue/state graphs. F5 uses Settings; Ctrl/Cmd+F5 always "
			    "playtests the current room or selected node's room. Stop, edit, Play again for "
			    "custom projects.");
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Pixels & artwork")) {
			ImGui::SeparatorText("ROOM GEOMETRY");
			if (ImGui::BeginTable("dimensions", 2,
			                      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
				const char *rows[][2] = {
				    {"One room", "256 x 224 pixels = 57,344 pixels"},
				    {"Room step", "256 horizontal / 216 vertical (8 rows overlap)"},
				    {"Collision grid", "16 columns x 7 rows; 16 x 36 pixel cells"},
				    {"Standing floor edges", "Y = 72, 144, 216"},
				    {"Project capacity", "64 room slots / 64 modules / 256 graph nodes"}};
				for (const auto &r : rows) {
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(r[0]);
					ImGui::TableNextColumn();
					ImGui::TextWrapped("%s", r[1]);
				}
				ImGui::EndTable();
			}
			ImGui::SeparatorText("THREE VISUAL LAYERS");
			ImGui::TextWrapped("Back: distant scenery. Playfield: scenery behind the actors. "
			                   "Front: transparent foreground placed over actors, so Conrad "
			                   "can walk behind it. Collision and ledges are separate data; "
			                   "painting art alone never creates a floor.");
			ImGui::TextWrapped(
			    "Use 256 x 224 PNGs for native-resolution rooms. PNG alpha is recommended for "
			    "overlays. PNG, JPEG and BMP are accepted; JPEG has no transparency. A layer "
			    "may reference a crop of a larger sheet. Empty art is allowed.");
			ImGui::SeparatorText("ONE BIG LEVEL PICTURE");
			ImGui::TextWrapped(
			    "Classic slicing: block 256 x 224, stride 256 x 216. For C columns and R rows, "
			    "use width C*256 and height 224+(R-1)*216. Example: 4 x 2 rooms need 1024 x "
			    "440 pixels. For separate non-overlapping screens, set the vertical stride to "
			    "224 instead.");
			ImGui::TextWrapped(
			    "At 2x source resolution: block 512 x 448, stride 512 x 432. Larger source "
			    "images are kept, but this renderer samples every crop into 256 x 224 pixels. "
			    "Importing 4K art does not create 4K gameplay geometry.");
			ImGui::SeparatorText("MODULE SPRITES");
			ImGui::TextWrapped(
			    "Idle and active animations use horizontal strips. Set the crop to ONE frame, "
			    "then set frame count and FPS. Frames run to its right. Node width/height "
			    "define the displayed size. There is no universal Conrad sprite box: his "
			    "native animations carry frame-specific origins and motion.");
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Visual style")) {
			ImGui::SeparatorText("CINEMATIC PIXEL-ART DESIGN");
			ImGui::TextWrapped("These are design suggestions for matching the game's visual "
			                   "language, not requirements imposed on your artwork.");
			ImGui::BulletText("Build readable silhouettes and deliberate pixel clusters.");
			ImGui::BulletText("Keep a coherent, limited palette within a location.");
			ImGui::BulletText("Use light and contrast to reveal paths, ledges and hazards.");
			ImGui::BulletText("Give distant scenery less contrast than the active play space.");
			ImGui::TextWrapped(
			    "Stage each room as a shot: establish where Conrad enters, what draws his eye "
			    "and where the next decision sits. Foreground silhouettes can add depth, but "
			    "keep switches, pickups and landing edges readable.");
			ImGui::TextWrapped(
			    "Design for deliberate movement and commitment to actions. Test the entire "
			    "jump, landing, crouch and climb animation. Leave enough headroom, landing "
			    "space and safe approach space beside switches and doors.");
			ImGui::TextWrapped(
			    "Introduce a puzzle's parts before asking the player to combine them. Use "
			    "repeated shapes, colors or lighting to imply a connection. The editor's "
			    "virtual wires are design aids and do not appear during play.");
			ImGui::TextWrapped(
			    "Avoid assuming every edition uses one palette limit. Amiga, DOS and Sega CD "
			    "use different asset/rendering paths. Pick an edition for movement tests and "
			    "check readability at 1x as well as enlarged.");
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Workflow & controls")) {
			ImGui::TextWrapped(
			    "1. Import or slice art. Check it at 1x.\n2. Paint collision, then place "
			    "ledges and the player start.\n3. Set explicit room exits in the inspector. "
			    "World positions alone do not create exits.\n4. Place a module or open its "
			    "graph. Connect output signals to input actions.\n5. Validate, save, then "
			    "PLAY HERE (F5). Pause and One tick help inspect behavior.");
			ImGui::SeparatorText("TEST WHERE YOU ARE EDITING");
			ImGui::TextWrapped(
			    "Settings chooses the Play / F5 start mode. Current room tests this room, "
			    "or the selected graph node's room. Native campaign starts at the selected "
			    "level's authored beginning. Ctrl/Cmd+F5 always tests here. "
			    "Place TEST marker and click the room to choose a position on a native floor "
			    "height; Auto finds a supported clear position. Markers inherit player-start "
			    "facing when placed. TEST markers are session-only and never move the authored "
			    "START. "
			    "Shift+F5, or right-click PLAY HERE, tests the authored beginning.");
			ImGui::SeparatorText("GRID AND ANCHORS");
			ImGui::TextWrapped(
			    "Collision row r is drawn from Y=r*36-8 to r*36+28, clipped to the room. Floor "
			    "edges used by the native movement scripts are at Y=72/144/216. The player "
			    "start stores the native position two pixels above its standing floor "
			    "(70/142/214). A module anchor is its bottom-left; a legacy activator is "
			    "centered on its anchor.");
			ImGui::TextWrapped(
			    "Ledge stamps edit nearby collision cells to provide a floor edge and approach "
			    "clearance. Place them on the correct floor and test climbing from both sides. "
			    "Right-click an existing ledge to remove it.");
			ImGui::SeparatorText("SHORTCUTS");
			ImGui::TextWrapped(
			    "F1: these guidelines. Ctrl/Cmd+S: save. Ctrl/Cmd+Z: undo. Ctrl/Cmd+Shift+Z: "
			    "redo. Ctrl/Cmd+C/V: copy/paste whole selected modules. Shift-click toggles "
			    "selection; drag empty room space for a selection box. Lock visual layers "
			    "or collision to protect edits. Middle-drag/wheel pans/zooms module graphs; "
			    "Frame all fits every node. Validation Focus selects the problem. "
			    "Fit: show the entire room. 1x/2x: exact pixel multiples. Search filters "
			    "the object palette. Drop an image to slice it, or a project JSON to open it.");
			ImGui::TextWrapped(
			    "Play: arrows move/climb, Shift action/fire, Space draw/holster, Enter "
			    "interact. Escape returns to editing. Opening guidelines or losing focus "
			    "pauses play. Resume explicitly when ready.");
			ImGui::TextWrapped(
			    "Custom Play uses a snapshot: Stop, edit and PLAY HERE again. Native campaign "
			    "mode "
			    "opens a separate player with controlled live updates. Native preview saves "
			    "stay in the project's .native-playtest-saves folder.");
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Native campaign")) {
			ImGui::TextWrapped(
			    "Choose your assets and edition, then Original campaign. The reference viewer "
			    "decodes all seven levels' scenery, collision, exits and initial actors. "
			    "Room numbers are native slots 0..63, not campaign order. Blank slots and "
			    "hidden holding-list actors are part of the original data.");
			ImGui::TextWrapped(
			    "Make scenery copy creates a NEW project in a new subfolder, with all room "
			    "slots, raw collision, exits, player start, flattened Back art and a Front "
			    "priority mask. You can paint, place modules and PLAY this custom geometry. "
			    "The indexed 8x8 World layout is a slot shelf, not a geographical "
			    "reconstruction.");
			ImGui::TextWrapped(
			    "It does NOT recreate the campaign's enemies, triggers, inventory, dialogues, "
			    "palette animation or puzzle scripts. Foreground priority is a useful "
			    "approximation for custom actors. Initial reference actors may be hidden or "
			    "inactive until their scripts run.");
			ImGui::TextWrapped(
			    "Edit full native campaign creates seven linked project files. All original "
			    "actors, helpers and script resources stay loaded. Select actor records to "
			    "change placement, health and facing. Puzzle Connections exposes typed actor/room "
                "ports. Move/copy a complete bundle includes hidden helpers and remaps internal "
                "links; private behaviors keep copies independent. Actor slot 255 is reserved "
                "for new copies, so full levels must reuse existing modules.");
			ImGui::TextWrapped(
			    "Settings can create a native campaign project directly. Numbered room "
			    "markers and the native palette select actor records. Module Editing exposes "
			    "native conditions, actions, transitions and execution traces. Puzzle groups "
			    "follow reviewed dependencies; rename or regroup them. Rule/module edits need "
			    "Restart. Opaque dependencies stay protected.");
			ImGui::TextWrapped(
			    "PLAY HERE starts a native preview immediately in this room with fresh level "
			    "state, original actors/scripts and the pistol; it skips the arrival sequence. "
			    "Earlier quest items and objectives are not synthesized. Blank map slots "
			    "cannot be tested. Shift+F5 plays the authored beginning. Apply live changes "
			    "art and "
			    "safe geometry while retaining script/inventory state. Resume continues. "
			    "Actor setup changes, edited cells already changed by a puzzle, and new "
			    "collision over an active actor require Restart at edited level. Rejected "
			    "edits leave the running game unchanged. Wait for each command's status.");
			ImGui::TextWrapped(
			    "Pause/Apply wait for a frame boundary: close or skip open native dialogue, "
			    "cinematics or menus first. Stop native can close those immediately. Campaign "
			    "level switches editor documents; PLAY HERE tests the selected level and "
			    "room.");
			ImGui::TextWrapped("Original assets are read only. Decoded scenery copies contain "
			                   "original artwork and belong with your private assets; share "
			                   "your own art and authored modules separately.");
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	ImGui::EndChild();
}
