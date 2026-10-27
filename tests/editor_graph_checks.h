// SPDX-License-Identifier: GPL-3.0-or-later
// Behavioral regressions for author graphs; no original assets needed here.
static void graphChecks(const std::filesystem::path &root) {
	using namespace editor;
	auto p = makeProject();
	const auto room = p.rooms[0].id;
	auto pair = makeGraph(p, 0, {room, 192, 216});
	p.modules.push_back(pair);
	auto state = initialGraphState(pair);
	GraphMachine machine(pair, state);
	machine.use(1);
	check(state.nodes[0].active, "Button did not open graph door");
	machine.canClose = [](const Node &) { return false; };
	machine.use(1);
	check(state.nodes[0].active, "Graph door crushed an occupying actor");
	machine.canClose = [](const Node &) { return true; };
	machine.use(1);
	check(!state.nodes[0].active, "Graph door did not toggle closed");
	machine.send(1, Input::Disable);
	machine.use(1);
	check(!state.nodes[0].active, "Disabled button activated");
	machine.send(1, Input::Enable);
	machine.use(1);
	check(state.nodes[0].active, "Re-enabled button did not activate");
	auto copy = duplicateModule(p, pair);
	auto other = initialGraphState(copy);
	check(!other.nodes[0].active && copy.nodes[0].id == pair.nodes[0].id && copy.id != pair.id,
	      "Copied module shares graph state or loses scoped identities");
	p.modules.push_back(copy);
	check(serialize(deserialize(serialize(p))) == serialize(p), "Graph project roundtrip");
	savePrefab(pair, root, root / "graph.r2module.json");
	auto imported = loadPrefab(p, {room, 176, 216}, root / "graph.r2module.json", root);
	check(imported.nodes[1].at.x - imported.nodes[0].at.x == -48 &&
	          imported.wires[0].to == imported.nodes[0].id,
	      "Graph prefab relocation broke owned connections");
	const auto before = serialize(p);
	rejects([&] { loadPrefab(p, {room, 248, 216}, root / "graph.r2module.json", root); },
	        "Out-of-room prefab placement accepted");
	check(before == serialize(p), "Failed prefab import advanced the project ID counter");
	auto npc = makeGraph(p, 2, {room, 192, 216});
	auto quest = initialGraphState(npc);
	GraphMachine talk(npc, quest);
	talk.use(2);
	check(!quest.nodes[0].active && !quest.nodes[2].completed && talk.message == npc.nodes[2].text,
	      "NPC completed without required item");
	talk.send(1, Input::Activate);
	check(quest.nodes[1].collected, "Quest item not added to inventory");
	talk.use(2);
	check(quest.nodes[0].active && quest.nodes[2].completed && !quest.nodes[1].collected &&
	          talk.message == npc.nodes[2].doneText,
	      "Item hand-in did not complete NPC and open door");
	talk.use(2);
	check(quest.nodes[0].active, "Repeated dialogue reversed completed quest");
	auto mission = makeGraph(p, 3, {room, 192, 216});
	auto second = makeNode(mission, NodeKind::Button, {room, 32, 216});
	mission.nodes.push_back(second);
	mission.nodes[1].goal = 2;
	mission.wires.push_back({second.id, mission.nodes[1].id, Signal::Used, Input::Progress});
	auto work = initialGraphState(mission);
	GraphMachine jobs(mission, work);
	jobs.use(2);
	jobs.use(2);
	jobs.use(1);
	check(!work.nodes[0].active && work.nodes[1].progress.size() == 1,
	      "Repeated objective counted twice");
	jobs.use(3);
	check(work.nodes[0].active && work.nodes[1].completed,
	      "Mission completion did not open connected door");
	jobs.send(1, Input::Reset);
	check(!work.nodes[1].completed && work.nodes[1].progress.empty(),
	      "Mission reset did not clear progress");
	// Collecting an item can complete a mission: preserve its full completion
	// dialogue separately from the transient pickup notice.
	auto delivery = makeGraph(p, 3, {room, 192, 216});
	delivery.nodes[2].kind = NodeKind::Item;
	delivery.wires[0].signal = Signal::Completed;
	auto deliveryState = initialGraphState(delivery);
	GraphMachine deliver(delivery, deliveryState);
	deliver.use(1);
	deliver.send(2, Input::Activate);
	check(deliver.message == delivery.nodes[1].doneText && !deliver.notice.empty(),
	      "Item-triggered mission replaced dialogue with pickup notification");

	auto cycle = makeGraph(p, 5, {room, 192, 216});
	cycle.nodes.push_back(makeNode(cycle, NodeKind::Relay, {room, 32, 216}));
	cycle.nodes.push_back(makeNode(cycle, NodeKind::Relay, {room, 64, 216}));
	cycle.wires = {{cycle.nodes[0].id, cycle.nodes[1].id, Signal::Used, Input::Activate},
	               {cycle.nodes[1].id, cycle.nodes[0].id, Signal::Used, Input::Activate}};
	auto loop = initialGraphState(cycle);
	GraphMachine cyclic(cycle, loop);
	cyclic.use(0);
	check(loop.nodes[0].completed && loop.nodes[1].completed,
	      "Cyclic graph failed bounded dispatch");
	auto invalid = p;
	invalid.modules[0].wires[0].input = Input::Progress;
	rejects([&] { deserialize(serialize(invalid)); }, "Incompatible port accepted");
	invalid = p;
	invalid.modules[0].nodes[0].visuals[0].art.file = "../outside.png";
	rejects([&] { deserialize(serialize(invalid)); }, "Graph sprite path traversal accepted");
	invalid = p;
	invalid.modules[0].nodes[0].kind = NodeKind(999);
	rejects([&] { deserialize(serialize(invalid)); }, "Unknown graph node kind accepted");
	invalid = p;
	invalid.modules[0] = npc;
	removeNode(invalid.modules[0], 1);
	rejects([&] { deserialize(serialize(invalid)); },
	        "Deleting required item silently unlocked the NPC");
	auto replacement = makeNode(invalid.modules[0], NodeKind::Item, {room, 48, 216});
	check(replacement.id != npc.nodes[1].id, "New node reused a dangling item requirement ID");
	invalid = p;
	addRoom(invalid, 0, 0);
	rejects([&] { deserialize(serialize(invalid)); }, "Overlapping world rooms accepted");
	for (int recipe = 0; recipe < 6; ++recipe) {
		auto demo = makeProject();
		demo.modules.push_back(makeGraph(demo, recipe, {demo.rooms[0].id, 192, 216}));
		deserialize(serialize(demo));
	}
	// A two-frame fixture: idle red, active green. Exercise dependency copies,
	// animation bounds, hold-last-frame rendering and clipping after reopen.
	std::vector<unsigned char> bmp(62);
	bmp[0] = 'B';
	bmp[1] = 'M';
	bmp[2] = 62;
	bmp[10] = 54;
	bmp[14] = 40;
	bmp[18] = 2;
	bmp[22] = bmp[26] = 1;
	bmp[28] = 24;
	bmp[34] = 8;
	bmp[56] = 255;
	bmp[58] = 255;
	{
		std::ofstream f(root / "states.bmp", std::ios::binary);
		f.write(reinterpret_cast<char *>(bmp.data()), bmp.size());
	}
	pair.nodes[0].visuals[0] = {{"states.bmp", 0, 0, 1, 1}, 2, 30, false};
	pair.nodes[0].visuals[1] = {{"states.bmp", 1, 0, 1, 1}, 1, 6, true};
	p.modules = {pair};
	Images images;
	images.root = root;
	validateImages(p, images);
	Image frame{256, 224, std::vector<uint32_t>(256 * 224)};
	drawNode(frame, pair.nodes[0], false, 100, images);
	check(frame.pixels[180 * 256 + 192] == rgba(0, 255, 0),
	      "Non-looping sprite did not hold its final frame");
	savePrefab(pair, root, root / "export" / "animated.r2module.json");
	auto importedArt = loadPrefab(p, {room, 176, 216}, root / "export" / "animated.r2module.json",
	                              root / "imported");
	check(std::filesystem::exists(
	          assetPath(root / "imported", importedArt.nodes[0].visuals[0].art.file)),
	      "Prefab lost node sprite dependency");
	p.modules[0].nodes[0].visuals[1].frames = 2;
	rejects([&] { validateImages(p, images); }, "Out-of-bounds node animation strip accepted");
}
static void graphSceneChecks(const std::filesystem::path &root, const assets::Source &source,
                             HeadlessSystem &platform) {
	using namespace editor;
	auto p = makeProject();
	p.modules.push_back(makeGraph(p, 0, {p.rooms[0].id, 192, 216}));
	p.spawn.x = p.modules[0].nodes[1].at.x;
	Scene pair(p, root, source, &platform);
	for (int t = 0; t < 20; ++t) {
		platform._pi = {};
		platform._pi.enter = true;
		pair.tick();
	}
	check(pair.states()[0].graph.nodes[0].active,
	      "Native Enter input failed graph door or repeated while held");
	check(pair.game()._res._ctData[256 + 4 * 16 + 12] == 0,
	      "Graph door still collides after opening");
	pair.restart();
	check(!pair.states()[0].graph.nodes[0].active, "Restart retained graph state");
	// Legacy chain targets survive conversion to graph modules; graph endpoints
	// may be placed in another room without any raw actor-index remapping.
	auto linked = makeProject();
	addRoom(linked, 1, 0);
	auto graph = makeGraph(linked, 0, {linked.rooms[0].id, 192, 216});
	graph.nodes[0].at.room = linked.rooms[1].id;
	linked.modules.push_back(graph);
	auto legacy = makeModule(linked, Kind::Door, {linked.rooms[0].id, 96, 216});
	legacy.outputs = {graph.id};
	linked.modules.push_back(legacy);
	Scene crossRoom(linked, root, source, &platform);
	crossRoom.activate(1);
	check(crossRoom.states()[0].graph.nodes[0].active &&
	          crossRoom.game()._res._ctData[256 + 112 + 4 * 16 + 12] == 0,
	      "Legacy chain failed to open converted cross-room graph door");

	// Native item pickup + dialogue hand-in + collision release, with actual
	// sprite/font rendering on every supported original asset edition.
	p = makeProject();
	p.modules.push_back(makeGraph(p, 2, {p.rooms[0].id, 192, 216}));
	p.modules[0].nodes[2].doneText =
	    std::string(850, 'A'); // Long unbroken text must wrap and paginate.
	p.spawn.x = p.modules[0].nodes[1].at.x;
	Scene quest(p, root, source, &platform);
	platform._pi = {};
	quest.tick();
	quest.render();
	check(quest.states()[0].graph.nodes[1].collected,
	      "Native proximity did not collect quest item");
	quest.game()._pgeLive[0].pos_x = p.modules[0].nodes[2].at.x;
	platform._pi.enter = true;
	quest.tick();
	auto dialogue = quest.render();
	check(quest.dialogueOpen() && quest.states()[0].graph.nodes[0].active &&
	          !quest.states()[0].graph.nodes[1].collected,
	      "Native NPC hand-in failed");
	check(dialogue.pixels[152 * 256 + 10] == rgba(87, 220, 202),
	      "Dialogue did not render at native resolution");
	auto x = quest.game()._pgeLive[0].pos_x;
	platform._pi.dirMask = PlayerInput::DIR_RIGHT;
	for (int t = 0; t < 20; ++t)
		quest.tick();
	check(quest.game()._pgeLive[0].pos_x == x && quest.dialogueOpen(),
	      "Held Enter dismissed dialogue or player moved during dialogue");
	int pages = 0;
	while (quest.dialogueOpen() && pages < 20) {
		platform._pi = {};
		quest.tick();
		platform._pi.enter = true;
		quest.tick();
		quest.render();
		++pages;
	}
	check(!quest.dialogueOpen() && pages > 1 && pages < 20,
	      "Long dialogue was truncated or could not be paged through");
	// Two objectives required; native input at a terminal starts the job.
	p = makeProject();
	p.modules.push_back(makeGraph(p, 3, {p.rooms[0].id, 192, 216}));
	p.spawn.x = p.modules[0].nodes[2].at.x;
	Scene mission(p, root, source, &platform);
	platform._pi = {};
	platform._pi.enter = true;
	mission.tick();
	check(mission.states()[0].graph.nodes[1].progress.size() == 1 &&
	          !mission.states()[0].graph.nodes[0].active,
	      "Mission started without accepting at terminal");
	platform._pi = {};
	mission.tick();
	mission.game()._pgeLive[0].pos_x = p.modules[0].nodes[1].at.x;
	platform._pi.enter = true;
	mission.tick();
	mission.render();
	check(mission.states()[0].graph.nodes[1].completed && mission.states()[0].graph.nodes[0].active,
	      "Native mission completion failed");
}
