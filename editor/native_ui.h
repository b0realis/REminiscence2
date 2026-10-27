// SPDX-License-Identifier: GPL-3.0-or-later
const assets::Source &nativeSource() {
	if (assetIndex >= 0 && assetIndex < int(library.games.size()) &&
	    library.games[assetIndex].edition == p.native.edition)
		return library.games[assetIndex];
	for (int i = 0; i < int(library.games.size()); ++i)
		if (library.games[i].edition == p.native.edition) {
			assetIndex = i;
			return library.games[i];
		}
	throw std::runtime_error("Choose the original assets for this project's edition");
}
void ensureNativeReference() {
	if (!p.native.enabled)
		return;
	const auto key = std::to_string(p.native.edition) + ":" + std::to_string(p.native.level) + ":" +
	                 p.native.fingerprint;
	if (!nativeReference || nativeReferenceKey != key) {
		nativePreview.audible = false;
		auto next = std::make_unique<OriginalLevel>(nativeSource(), &nativePreview, p.native.level);
		if (next->createProject(true).native.fingerprint != p.native.fingerprint)
			throw std::runtime_error("Native project asset fingerprint mismatch");
		nativeDefaults = next->createProject(true).native;
		nativeReference = std::move(next);
		nativeReferenceKey = key;
	}
	if (p.native.programs.empty()) {
		auto definition = nativeDefaults;
		p.native.programs = std::move(definition.programs);
		p.native.puzzles = std::move(definition.puzzles);
	}
}
void launchNative(const PlaytestStart *start = nullptr) {
	ensureNativeReference();
	compileCollision(p);
	validateNativeLogic(p.native, nativeDefaults);
	if (nativePlayer.running()) {
		if (NativeSession::pending(nativeControl))
			throw std::runtime_error(
			    "Waiting for the previous command; finish or skip any open native menu/dialogue");
		NativeSession::publish(nativeControl, start ? "play-here" : "play-start", p, start);
		status = "Starting a fresh playtest at the requested location";
		return;
	}
	saveAs(path);
	nativeSessionRoot = path.parent_path();
	nativeControl = path.parent_path() / (".editor-session-" + makeProject().id + ".json");
	NativeSession::publish(nativeControl, start ? "play-here" : "resume", p, start);
	char *base = SDL_GetBasePath();
	fs::path binary = base ? fs::u8path(base) : fs::current_path();
	SDL_free(base);
#ifdef _WIN32
	binary /= "reminiscence2.exe";
#else
	binary /= "reminiscence2";
#endif
	const auto source = nativeSource();
	nativePlayer.launch({binary.u8string(), "--no-preferences", "--datapath=" + source.root,
	                     "--project=" + path.u8string(),
	                     "--editor-session=" + nativeControl.u8string()},
	                    path.parent_path() / ".native-player.log");
	status =
	    "Native playtest launching with original actors and scripts; test position is session-only";
}
void nativeCommand(const char *action) {
	if (nativeControl.empty() || !nativePlayer.running())
		throw std::runtime_error("Launch the native preview first");
	if (std::string(action) != "stop" && NativeSession::pending(nativeControl))
		throw std::runtime_error("Waiting for the previous command; finish or skip any "
		                         "cinematic/dialogue in the preview");
	NativeSession::publish(nativeControl, action, p);
	status = std::string("Requested ") + action + " at the next simulation boundary";
}
void nativeToolbar() {
	if (!p.native.enabled)
		return;
	if (ImGui::Button("Pause native"))
		attempt([&] { nativeCommand("pause"); });
	ImGui::SameLine();
	if (ImGui::Button("Step native"))
		attempt([&] { nativeCommand("step"); });
	ImGui::SameLine();
	if (ImGui::Button("Apply live"))
		attempt([&] { nativeCommand("apply"); });
	mark("native-apply");
	ImGui::SameLine();
	if (ImGui::Button("Resume native"))
		attempt([&] { nativeCommand("resume"); });
	ImGui::SameLine();
	if (ImGui::Button("Restart at edited level"))
		attempt([&] { nativeCommand("restart"); });
	ImGui::SameLine();
	if (ImGui::Button("Stop native"))
		attempt([&] { nativeCommand("stop"); });
	if (!nativeControl.empty()) {
		if (SDL_GetTicks() - nativeStatusTime >= 250) {
			nativeStatus = NativeSession::statusText(nativeControl);
			nativeEvents = NativeSession::readEvents(nativeControl, &p.native);
			nativeMonitor = NativeSession::readMonitor(nativeControl, p.native);
			nativeGraphs = NativeSession::readGraphs(nativeControl);
			nativeStatusTime = SDL_GetTicks();
			if (!nativePlayer.running())
				nativeStatus = "Native preview stopped. PLAY HERE starts a new session.";
		}
		ImGui::TextWrapped("%s", nativeStatus.c_str());
	}
	if (ImGui::BeginCombo("Campaign level", std::to_string(p.native.level + 1).c_str())) {
		for (int i = 0; i < 7; ++i) {
			const auto file =
			    path.parent_path() / ("level-" + std::to_string(i + 1) + ".r2project.json");
			if (fs::exists(file) &&
			    ImGui::Selectable(("Level " + std::to_string(i + 1)).c_str(), p.native.level == i))
				attempt([&] {
					saveAs(path);
					open(file);
				});
		}
		ImGui::EndCombo();
	}
}
void nativePalette() {
	attempt([&] { ensureNativeReference(); });
	ImGui::SeparatorText("NATIVE PUZZLE MODULES");
	for (int i = 0; i < int(p.native.puzzles.size()); ++i) {
		const auto &puzzle = p.native.puzzles[i];
		if (ImGui::TreeNode((puzzle.name + "##puzzle-" + std::to_string(i)).c_str())) {
			if (ImGui::SmallButton("Edit puzzle nodes")) {
				nativePuzzle = i;
				nativeConnections = true;
				graphFrameAll = true;
				nativeActor = puzzle.actors.front();
				nativeRulePage = 0;
				nativeExtensionGraph = false;
				setWorkflow(Workflow::Module);
			}
			if (ImGui::SmallButton("Place complete copy / move"))
				attempt([&] { openNativePlacement(i); });
			for (int actor : puzzle.actors) {
				if (ImGui::Selectable(("Actor " + std::to_string(actor) + " / " +
				                       nativeActorRole(p.native, actor))
				                          .c_str(),
				                      nativeActor == actor)) {
					nativeActor = actor;
					if (p.native.actors[actor].room < 64)
						room = p.native.actors[actor].room;
				}
			}
			ImGui::TreePop();
		}
	}
	ImGui::SeparatorText("AUTHORED EVENT MODULES");
	if (ImGui::SmallButton("New event module")) {
		selected = -1;
		nativeExtensionGraph = true;
		setWorkflow(Workflow::Module);
	}
	for (int i = 0; i < int(p.modules.size()); ++i)
		if (ImGui::Selectable(p.modules[i].name.c_str(), selected == i)) {
			selected = i;
			openGraph();
		}
	ImGui::SeparatorText("ALL NATIVE ACTORS");
	paletteSearch.Draw("Search", 135);
	ImGui::Checkbox("All rooms / helpers", &nativeAllActors);
	ImGui::TextWrapped("Fixed IDs preserve scripts, puzzles and inventory. Select a marker or "
	                   "record. Actor setup edits require restart.");
	for (const auto &a : p.native.actors) {
		if (!nativeAllActors && a.room != room)
			continue;
		const auto label = "Actor " + std::to_string(a.id) + " / type " + std::to_string(a.type) +
		                   (a.room >= 64 ? " [helper]" : "");
		if (paletteSearch.PassFilter(label.c_str()) &&
		    ImGui::Selectable(label.c_str(), nativeActor == a.id)) {
			nativeActor = a.id;
			if (a.room < 64)
				room = a.room;
			selected = -1;
			tool = Tool::Select;
		}
	}
}
void nativeInspector() {
	if (nativeActor < 0 || nativeActor >= int(p.native.actors.size()))
		return;
	auto &a = p.native.actors[nativeActor];
	auto before = p;
	ImGui::SeparatorText(("NATIVE ACTOR " + std::to_string(a.id)).c_str());
	ImGui::Text("Type %d | behavior %d", a.type, a.behavior);
	ImGui::Text("Class %d | slot %d", a.objectKind, a.room);
	ImGui::TextWrapped("Operands: %d, %d, %d, %d. Use Puzzle Connections to reconnect typed "
	                   "actor/room ports; counters and mixed-use values remain protected.",
	                   a.data[0], a.data[1], a.data[2], a.data[3]);
	ImGui::TextWrapped("All %zu original actor slots and original behavior tables remain loaded. "
	                   "Inventory and message links keep their native IDs.",
	                   p.native.actors.size());
	const bool helper = a.room >= 64;
	ImGui::BeginDisabled(helper || locked[1]);
	bool changed = ImGui::SliderInt("Native room", &a.room, 0, 63);
	changed |= ImGui::InputInt("Native X", &a.x);
	changed |= ImGui::InputInt("Native Y", &a.y);
	changed |= ImGui::InputInt("Initial health", &a.life);
	changed |= ImGui::Checkbox("Face left", &a.facingLeft);
	if (changed) {
		a.x = std::clamp(a.x, -256, 511);
		a.y = std::clamp(a.y, -224, 447);
		a.life = std::clamp(a.life, -32768, 32767);
		if (a.id == 0) {
			a.x = std::clamp(a.x, 0, 248);
			a.y = std::clamp(a.y, 0, 215);
			p.spawn = {p.rooms[a.room].id, a.x, a.y};
			p.spawnLeft = a.facingLeft;
		}
		history.remember(before);
	}
	ImGui::EndDisabled();
	if (helper)
		ImGui::TextWrapped("Protected holding-list dependency. Its native script decides when to "
		                   "spawn or attach it.");
	if (a.id > 0 && ImGui::Button("Create actor event module")) {
		edit();
		const auto at = Anchor{p.rooms[a.room < 64 ? a.room : room].id, std::clamp(a.x, 0, 240),
		                       std::clamp(a.y + 2, 0, 216)};
		auto module = makeGraph(p, 5, at);
		module.name = "Actor " + std::to_string(a.id) + " events";
		auto node = makeNode(module, NodeKind::Actor, at);
		node.nativeActor = a.id;
		node.at = at;
		node.health = std::clamp(a.life, 1, 100);
		module.nodes.push_back(node);
		p.modules.push_back(module);
		selected = int(p.modules.size()) - 1;
		openGraph();
	}
	if (ImGui::Button("Edit native state machine")) {
		for (int i = 0; i < int(p.native.puzzles.size()); ++i)
			for (int id : p.native.puzzles[i].actors)
				if (id == nativeActor)
					nativePuzzle = i;
		nativeRulePage = 0;
		nativeConnections = false;
		nativeExtensionGraph = false;
		setWorkflow(Workflow::Module);
	}
	if (ImGui::Button("Room artwork / exits"))
		nativeActor = -1;
}
