// SPDX-License-Identifier: GPL-3.0-or-later
// Native modules retain the interpreter's real objects, resources and scripts.
// This view exposes their typed connections, including holding-list helpers.
void openNativePlacement(int group) {
	const auto members = nativePuzzleClosure(p.native, group);
	nativePlacementPuzzle = group;
	nativePlacementProject = p.id;
	nativePlacementRooms.clear();
	nativePlacementX = 16;
	nativePlacementY = 0;
	for (int id : members) {
		const auto &a = p.native.actors[id];
		if (a.room < 64)
			nativePlacementRooms[a.room] = a.room;
		for (const auto &port : nativePorts(p.native, id))
			if (port.kind == NativePortKind::Room && port.target >= 0 && port.target < 64)
				nativePlacementRooms[port.target] = port.target;
	}
	nativePlacementOpen = true;
}
void nativePlacementDialog() {
	if (!nativePlacementOpen)
		return;
	ImGui::SetNextWindowSize({610, 590}, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowBgAlpha(1.f);
	if (ImGui::Begin("Place native puzzle module", &nativePlacementOpen)) {
		if (!p.native.enabled || nativePlacementProject != p.id || nativePlacementPuzzle < 0 ||
		    nativePlacementPuzzle >= int(p.native.puzzles.size())) {
			nativePlacementOpen = false;
			ImGui::End();
			return;
		}
		std::vector<int> members;
		try {
			members = nativePuzzleClosure(p.native, nativePlacementPuzzle);
		} catch (const std::exception &e) {
			status = e.what();
			nativePlacementOpen = false;
			ImGui::End();
			return;
		}
		ImGui::TextWrapped("%s", p.native.puzzles[nativePlacementPuzzle].name.c_str());
		ImGui::Text("Complete connected bundle: %zu actors / %d free copy slots", members.size(),
		            std::max(0, 255 - int(p.native.actors.size())));
		ImGui::TextWrapped("Includes incoming activators, outgoing targets and hidden helpers. "
		                   "Shared player/weapon services stay external. A copy gets new actor IDs "
		                   "and private behavior tables. Holding-list coordinates stay intact.");
		ImGui::SliderInt("Offset X", &nativePlacementX, -256, 256);
		ImGui::SliderInt("Offset Y", &nativePlacementY, -224, 224);
		ImGui::BeginChild("native_room_mapping", {0, 180}, ImGuiChildFlags_Borders);
		for (auto &[from, to] : nativePlacementRooms) {
			ImGui::PushID(from);
			ImGui::SetNextItemWidth(240);
			ImGui::SliderInt(("Room " + std::to_string(from) + " ->").c_str(), &to, 0, 63);
			ImGui::PopID();
		}
		ImGui::EndChild();
		ImGui::TextWrapped(
		    "Room art, collision painting and screen exits remain level layers. Review them at the "
		    "destination. Dynamic collision/inventory lookups and campaign-wide effects remain "
		    "shared; use the live message trace to test the placement.");
		if (ImGui::Button("Move complete bundle"))
			attempt([&] {
				const auto before = p;
				moveNativePuzzle(p, nativePlacementPuzzle, nativePlacementRooms, nativePlacementX,
				                 nativePlacementY, nativeDefaults);
				history.remember(before);
				nativePlacementOpen = false;
				status = "Moved complete puzzle bundle; restart native preview to apply";
			});
		mark("native-move-bundle");
		ImGui::SameLine();
		ImGui::BeginDisabled(p.native.actors.size() + members.size() > 255);
		if (ImGui::Button("Place independent copy"))
			attempt([&] {
				const auto before = p;
				nativePuzzle = copyNativePuzzle(p, nativePlacementPuzzle, nativePlacementRooms,
				                                nativePlacementX, nativePlacementY, nativeDefaults);
				history.remember(before);
				nativePlacementOpen = false;
				nativeActor = p.native.puzzles[nativePuzzle].actors.front();
				nativeConnections = true;
				nativeExtensionGraph = false;
				graphFrameAll = true;
				setWorkflow(Workflow::Module);
				status = "Copied puzzle with relocated internal ports and private rules; restart "
				         "to play";
			});
		mark("native-copy-bundle");
		ImGui::EndDisabled();
	}
	ImGui::End();
}
void nativePortsInspector() {
	if (nativeActor < 0 || nativeActor >= int(p.native.actors.size()))
		return;
	const int id = nativeActor;
	const auto a = p.native.actors[id];
	if (ImGui::Button("Node help / comments"))
		nodeHelpOpen = true;
	ImGui::Text("ACTOR %d / template %d", id, nativeActorSource(a));
	ImGui::TextWrapped("%s", nativeActorRole(p.native, id).c_str());
	ImGui::Text("Room %d | state %d | behavior %d", a.room, a.type, a.behavior);
	for (const auto &live : nativeMonitor.actors)
		if (live.id == id) {
			ImGui::SeparatorText("RUNNING STATE");
			ImGui::Text("State %d | life %d | counter %d", live.state, live.life, live.counter);
			ImGui::Text("Room %d (%d,%d) | %s", live.room, live.x, live.y,
			            live.active ? "active" : "sleeping");
		}
	if (ImGui::Button("Open actor state machine")) {
		nativeConnections = false;
		nativeRule = 0;
		nativeRulePage = 0;
		graphFrameAll = true;
	}
	mark("native-open-rules");
	if (ImGui::Button("Make behavior private"))
		attempt([&] {
			const auto before = p;
			isolateNativeBehavior(p, id, nativeDefaults);
			history.remember(before);
			status =
			    "Actor behavior isolated; subsequent rule edits affect only its private owners";
		});
	mark("native-isolate");
	int owners = 0;
	for (const auto &other : p.native.actors)
		owners += other.behavior == p.native.actors[id].behavior;
	ImGui::Text("Behavior shared by %d actor(s)", owners);
	ImGui::SeparatorText("DEPENDENCY PORTS");
	const auto ports = nativePorts(p.native, id);
	if (ports.empty())
		ImGui::TextDisabled("No fixed actor or room references");
	for (int index = 0; index < int(ports.size()); ++index) {
		const auto &port = ports[index];
		ImGui::PushID(index);
		ImGui::TextWrapped("%s", port.label.c_str());
		if (port.data >= 0)
			ImGui::TextDisabled("data[%d]%s", port.data,
			                    port.editable ? "" : " / mixed use, protected");
		else
			ImGui::TextDisabled("Rule %d, operand %d", port.rule, port.operand + 1);
		const auto label =
		    (port.kind == NativePortKind::Actor ? "Actor " : "Room ") + std::to_string(port.target);
		ImGui::BeginDisabled(!port.editable || id == 0);
		if (ImGui::BeginCombo("Target", label.c_str())) {
			const int count = port.kind == NativePortKind::Actor ? int(p.native.actors.size()) : 64;
			for (int target = 0; target < count; ++target) {
				const auto name = port.kind == NativePortKind::Actor
				                      ? "Actor " + std::to_string(target) + " / " +
				                            nativeActorRole(p.native, target)
				                      : "Room " + std::to_string(target);
				if (ImGui::Selectable(name.c_str(), target == port.target))
					attempt([&] {
						const auto before = p;
						reconnectNativePort(p, port, target, nativeDefaults);
						history.remember(before);
						status = "Dependency rewired; restart to apply. Literal operands get a "
						         "private behavior automatically.";
					});
			}
			ImGui::EndCombo();
		}
		ImGui::EndDisabled();
		if (port.kind == NativePortKind::Actor && port.target >= 0 &&
		    port.target < int(p.native.actors.size()) && ImGui::SmallButton("Inspect target")) {
			nativeActor = port.target;
			for (int g = 0; g < int(p.native.puzzles.size()); ++g)
				if (std::find(p.native.puzzles[g].actors.begin(), p.native.puzzles[g].actors.end(),
				              nativeActor) != p.native.puzzles[g].actors.end())
					nativePuzzle = g;
			graphFrameAll = true;
		}
		ImGui::PopID();
	}
	ImGui::SeparatorText("DYNAMIC / SHARED LINKS");
	for (const auto &description : nativeDynamicLinks(p.native, id))
		ImGui::BulletText("%s", description.c_str());
	ImGui::TextWrapped("These are native runtime lookups, not fixed wires. Message values and "
	                   "native animation states remain in the actor's rule nodes.");
}
void nativeConnectionsUI() {
	ImGui::TextWrapped("CAMPAIGN PUZZLE MODULES / select an actor node to inspect or reconnect its "
	                   "ports. Teal wires are fixed dependencies; gold shows the latest matching "
	                   "delivered message. External services are labeled. Runtime collision and "
	                   "inventory targets appear in the message trace.");
	nativePuzzle = std::clamp(nativePuzzle, 0, int(p.native.puzzles.size()) - 1);
	if (ImGui::BeginCombo("Puzzle module", p.native.puzzles[nativePuzzle].name.c_str())) {
		for (int i = 0; i < int(p.native.puzzles.size()); ++i)
			if (ImGui::Selectable((p.native.puzzles[i].name + "##" + std::to_string(i)).c_str(),
			                      i == nativePuzzle)) {
				nativePuzzle = i;
				nativeActor = p.native.puzzles[i].actors.front();
				graphFrameAll = true;
			}
		ImGui::EndCombo();
	}
	const auto puzzle = p.native.puzzles[nativePuzzle];
	if (std::find(puzzle.actors.begin(), puzzle.actors.end(), nativeActor) == puzzle.actors.end())
		nativeActor = puzzle.actors.front();
	if (!ImGui::BeginTable("native_connections", 3,
	                       ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
		return;
	ImGui::TableSetupColumn("Module", ImGuiTableColumnFlags_WidthFixed, 235);
	ImGui::TableSetupColumn("Connections", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupColumn("Ports", ImGuiTableColumnFlags_WidthFixed, 310);
	ImGui::TableNextColumn();
	ImGui::BeginChild("native_module_members", {0, -35});
	auto before = p;
	if (textField("Module name", p.native.puzzles[nativePuzzle].name, 160))
		history.remember(before);
	if (ImGui::Button("Module help / comments"))
		moduleHelpOpen = true;
	ImGui::Text("%zu actors | level %d", puzzle.actors.size(), p.native.level + 1);
	if (ImGui::Button("Move / copy complete puzzle"))
		attempt([&] { openNativePlacement(nativePuzzle); });
	mark("native-place-puzzle");
	const bool removed = std::any_of(puzzle.actors.begin(), puzzle.actors.end(),
	                                 [&](int id) { return p.native.actors[id].removed; });
	if (ImGui::Button(removed ? "Restore puzzle actors" : "Delete puzzle actors"))
		attempt([&] {
			auto next = p;
			const auto closure = nativePuzzleClosure(next.native, nativePuzzle);
			for (int id : closure)
				next.native.actors[id].removed = !removed;
			validateNativeLogic(next.native, nativeDefaults);
			edit();
			for (int id : closure)
				p.native.actors[id].removed = !removed;
			status = "Puzzle actors updated; restart preview. Stable slots remain reserved; Undo "
			         "restores the change.";
		});
	ImGui::TextWrapped(
	    "Deletion hides/stops the connected actors and preserves their slots for references and "
		"Undo. Shared player/weapon services are protected. Restore returns the puzzle.");
	for (int id : puzzle.actors) {
		const auto &a = p.native.actors[id];
		if (ImGui::Selectable((std::to_string(id) + " / " + nativeActorRole(p.native, id)).c_str(),
		                      nativeActor == id))
			nativeActor = id;
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Room %d | state %d | %s", a.room, a.type,
			                  a.room >= 64 ? "hidden dependency" : "visible actor");
	}
	ImGui::SeparatorText("DELIVERED MESSAGES");
	const std::set<int> members(puzzle.actors.begin(), puzzle.actors.end());
	for (auto e = nativeMonitor.messages.rbegin(); e != nativeMonitor.messages.rend(); ++e) {
		if (!members.count(e->source) && !members.count(e->target))
			continue;
		const auto label = std::to_string(e->source) + " -> " + std::to_string(e->target) + " : " +
		                   std::to_string(e->value) + "##" + std::to_string(e->serial);
		if (ImGui::Selectable(label.c_str()))
			nativeActor = members.count(e->target) ? e->target : e->source;
	}
	ImGui::TextWrapped("Use Play, Pause and Step native above. Monitoring appears only for the "
	                   "matching project revision; restart after rule or wiring edits.");
	ImGui::EndChild();
	ImGui::TableNextColumn();
	Module display;
	display.id = "native-puzzle";
	display.kind = Kind::Graph;
	std::set<int> external;
	std::set<std::pair<int, int>> edges;
	for (int id : puzzle.actors)
		for (int target : nativeDependencies(p.native, id)) {
			edges.insert({id, target});
			if (!members.count(target))
				external.insert(target);
		}
	for (const auto &e : nativeMonitor.messages)
		if (members.count(e.source) || members.count(e.target)) {
			if (e.source < 0 || e.target < 0 || e.source >= int(p.native.actors.size()) ||
			    e.target >= int(p.native.actors.size()))
				continue;
			edges.insert({e.source, e.target});
			if (!members.count(e.source))
				external.insert(e.source);
			if (!members.count(e.target))
				external.insert(e.target);
		}
	std::vector<int> ids = puzzle.actors;
	ids.insert(ids.end(), external.begin(), external.end());
	for (int i = 0; i < int(ids.size()); ++i) {
		const int id = ids[i];
		const auto &a = p.native.actors[id];
		Node n;
		n.id = std::to_string(id);
		n.kind = NodeKind::State;
		n.name = "Actor " + n.id + (members.count(id) ? "" : " [external]");
		n.text = nativeActorRole(p.native, id);
		n.doneText = "Room " + std::to_string(a.room) + " | state " + std::to_string(a.type);
		n.speaker = std::string(a.removed ? "REMOVED / " : "") + "Behavior " +
		            std::to_string(a.behavior) + (a.room >= 64 ? " | helper" : "");
		n.graphX = 24 + (i % 3) * 270;
		n.graphY = 24 + (i / 3) * 190;
		display.nodes.push_back(n);
	}
	for (const auto &[a, b] : edges)
		display.wires.push_back(
		    {std::to_string(a), std::to_string(b), Signal::Completed, Input::Activate});
	auto live = initialGraphState(display);
	for (int i = 0; i < int(ids.size()); ++i)
		for (const auto &state : nativeMonitor.actors)
			if (state.id == ids[i]) {
				live.nodes[i].active = state.active;
				live.nodes[i].count = state.state;
			}
	for (const auto &e : nativeMonitor.messages)
		for (int w = 0; w < int(display.wires.size()); ++w)
			if (display.wires[w].from == std::to_string(e.source) &&
			    display.wires[w].to == std::to_string(e.target))
				live.trace.push_back({e.serial, 0, TraceKind::Delivery, -1, w});
	graphNode = int(std::find(ids.begin(), ids.end(), nativeActor) - ids.begin());
	graphReadOnly = true;
	monitoredGraph = &live;
	graphCanvas(display);
	graphReadOnly = false;
	monitoredGraph = nullptr;
	if (graphNode >= 0 && graphNode < int(ids.size())) {
		nativeActor = ids[graphNode];
		if (!members.count(nativeActor)) {
			for (int group = 0; group < int(p.native.puzzles.size()); ++group)
				if (std::find(p.native.puzzles[group].actors.begin(),
				              p.native.puzzles[group].actors.end(),
				              nativeActor) != p.native.puzzles[group].actors.end())
					nativePuzzle = group;
			graphFrameAll = true;
		}
	}
	ImGui::TableNextColumn();
	ImGui::BeginChild("native_port_details", {0, -35});
	nativePortsInspector();
	ImGui::EndChild();
	ImGui::EndTable();
}
