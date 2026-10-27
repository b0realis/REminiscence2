// SPDX-License-Identifier: GPL-3.0-or-later
// Observable behavior of asynchronous node actions and portable module assets.
static void actionChecks(const std::filesystem::path &root) {
	using namespace editor;
	auto p = makeProject();
	auto m = makeGraph(p, 0, {p.rooms[0].id, 192, 216});
	m.wires.clear();
	m.nodes.push_back(makeNode(m, NodeKind::Dialogue, m.a)); // 2
	m.nodes[2].speaker = "Technician";
	m.nodes[2].text = "Open the way?";
	m.nodes[2].choices = {"Open it", "Keep it closed"};
	m.nodes.push_back(makeNode(m, NodeKind::Animation, m.a)); // 3
	m.nodes.push_back(makeNode(m, NodeKind::Timer, m.a));     // 4
	m.nodes[4].duration = 5;
	m.wires = {{m.nodes[1].id, m.nodes[2].id, Signal::Used, Input::Activate},
	           {m.nodes[2].id, m.nodes[3].id, Signal::Choice1, Input::Activate},
	           {m.nodes[3].id, m.nodes[0].id, Signal::Completed, Input::Open},
	           {m.nodes[4].id, m.nodes[2].id, Signal::Completed, Input::Stop}};
	auto state = initialGraphState(m);
	GraphMachine machine(m, state, 10);
	machine.use(1);
	check(state.nodes[2].active && !state.nodes[2].completed && !state.nodes[0].active &&
	          machine.dialogues.size() == 1,
	      "Dialogue completed before dismissal");
	const auto generation = state.nodes[2].generation;
	machine.use(1);
	check(machine.dialogues.size() == 1 && state.nodes[2].generation == generation,
	      "Repeated activation restarted a running conversation");
	machine.finishDialogue(2, generation, 1);
	check(state.nodes[2].completed && !state.nodes[3].active && !state.nodes[0].active,
	      "Choice 2 took the Choice 1 branch");
	machine.use(1);
	machine.finishDialogue(2, state.nodes[2].generation, 0);
	check(state.nodes[3].active && !state.nodes[0].active, "Choice 1 did not start animation");
	GraphMachine(m, state, 39).advance();
	check(!state.nodes[0].active, "Animation completed before its last frame duration");
	GraphMachine(m, state, 40).advance();
	check(state.nodes[3].completed && !state.nodes[3].active && state.nodes[0].active,
	      "Non-looping animation did not complete/open door");
	machine.send(0, Input::Close);
	machine.send(3, Input::Reset);
	m.nodes[3].visuals[1].loop = true;
	machine.send(3, Input::Activate);
	GraphMachine(m, state, 1000).advance();
	check(state.nodes[3].active && !state.nodes[0].active, "Looping animation completed itself");
	machine.send(3, Input::Stop);
	check(!state.nodes[3].active && !state.nodes[0].active, "Stop emitted Completed");
	machine.send(2, Input::Activate);
	const auto canceled = state.nodes[2].generation;
	machine.send(4, Input::Activate);
	GraphMachine(m, state, 15).advance();
	check(!state.nodes[2].active && state.nodes[4].completed, "Timer failed to stop dialogue");
	machine.send(2, Input::Activate);
	machine.finishDialogue(2, canceled, 0);
	check(state.nodes[2].active && !state.nodes[3].active,
	      "Stale dialogue callback changed a new run");
	machine.send(2, Input::Disable);
	check(!state.nodes[2].active && !state.nodes[2].enabled, "Disable retained active dialogue");
	machine.send(2, Input::Activate);
	check(!state.nodes[2].active, "Disabled dialogue started");
	machine.send(2, Input::Enable);
	m.nodes[2].once = true;
	machine.send(2, Input::Activate);
	machine.finishDialogue(2, state.nodes[2].generation, 1);
	machine.send(2, Input::Activate);
	check(!state.nodes[2].active, "One-shot dialogue replayed");
	machine.send(2, Input::Reset);
	machine.send(2, Input::Activate);
	check(state.nodes[2].active, "Reset failed to rearm dialogue");
	// Start -> Stop -> Start cycles must stay bounded and preserve only current callbacks.
	m.wires.push_back({m.nodes[2].id, m.nodes[2].id, Signal::Started, Input::Stop});
	m.wires.push_back({m.nodes[2].id, m.nodes[2].id, Signal::Stopped, Input::Activate});
	machine.send(2, Input::Reset);
	check(machine.dialogues.size() < 16, "Action cycle escaped the wire-delivery budget");
	m.wires.resize(4);
	// Two due timers: the first restarts the second. The old deadline must not fire.
	auto timers = makeGraph(p, 5, {p.rooms[0].id, 160, 216});
	for (int i = 0; i < 2; ++i) {
		timers.nodes.push_back(makeNode(timers, NodeKind::Timer, timers.a));
		timers.nodes.back().duration = 1;
	}
	timers.wires = {{timers.nodes[0].id, timers.nodes[1].id, Signal::Completed, Input::Reset},
	                {timers.nodes[0].id, timers.nodes[1].id, Signal::Completed, Input::Activate}};
	auto timed = initialGraphState(timers);
	GraphMachine start(timers, timed, 0);
	start.send(0, Input::Activate);
	start.send(1, Input::Activate);
	GraphMachine(timers, timed, 1).advance();
	check(timed.nodes[0].completed && timed.nodes[1].active && !timed.nodes[1].completed,
	      "Restarted timer used its stale completion deadline");
	GraphMachine(timers, timed, 2).advance();
	check(timed.nodes[1].completed, "Restarted timer never completed at its new deadline");
	p.modules = {m};
	check(serialize(deserialize(serialize(p))) == serialize(p), "Action schema roundtrip");
	auto bad = p;
	bad.modules[0].nodes[2].choices[1].clear();
	rejects([&] { deserialize(serialize(bad)); }, "Half-defined dialogue choices accepted");
	bad = p;
	bad.modules[0].nodes[4].duration = 0;
	rejects([&] { deserialize(serialize(bad)); }, "Zero-duration timer accepted");
	bad = p;
	bad.modules[0].assets = {{"../outside.png", 0, 0, 1, 1}};
	rejects([&] { deserialize(serialize(bad)); }, "Module library path traversal accepted");
	writePNG({1, 1, {rgba(10, 20, 30)}}, root / "owned.png");
	m.assets = {{"owned.png", 0, 0, 1, 1}}; // unused asset still belongs to the module
	savePrefab(m, root, root / "action-export" / "actions.r2module.json");
	auto imported = loadPrefab(p, m.a, root / "action-export" / "actions.r2module.json",
	                           root / "action-import");
	check(imported.assets.size() == 1 &&
	          std::filesystem::exists(assetPath(root / "action-import", imported.assets[0].file)) &&
	          imported.nodes[2].choices == m.nodes[2].choices && imported.wires.size() == 4,
	      "Module export lost library assets, dialogue branches or wires");
}
static void actionSceneChecks(const std::filesystem::path &root, const assets::Source &source,
                              HeadlessSystem &platform) {
	using namespace editor;
	auto p = makeProject();
	auto m = makeGraph(p, 0, {p.rooms[0].id, 192, 216});
	m.wires.clear();
	m.nodes.push_back(makeNode(m, NodeKind::Dialogue, m.a));
	m.nodes[2].text = "Choose a route.";
	m.nodes[2].choices = {"Wait", "Open the door"};
	m.nodes.push_back(makeNode(m, NodeKind::Dialogue, m.a));
	m.nodes[3].text = "Second queued conversation.";
	m.nodes.push_back(makeNode(m, NodeKind::PickupAnimation, m.a));
	m.wires = {{m.nodes[1].id, m.nodes[2].id, Signal::Used, Input::Activate},
	           {m.nodes[1].id, m.nodes[3].id, Signal::Used, Input::Activate},
	           {m.nodes[2].id, m.nodes[0].id, Signal::Choice2, Input::Open},
	           {m.nodes[3].id, m.nodes[4].id, Signal::Completed, Input::Activate}};
	auto downstream = makeModule(p, Kind::Door, {p.rooms[0].id, 224, 216});
	m.outputs = {downstream.id};
	p.modules = {m, downstream};
	p.spawn.x = m.nodes[1].at.x;
	Scene scene(p, root, source, &platform);
	platform._pi = {};
	platform._pi.enter = true;
	scene.tick();
	check(scene.dialogueOpen() && !scene.states()[0].graph.nodes[0].active,
	      "Trigger dialogue did not pause before branch selection");
	scene.render();
	platform._pi = {};
	scene.tick();
	platform._pi.dirMask = PlayerInput::DIR_DOWN;
	scene.tick();
	for (int i = 0; i < 5; ++i)
		scene.tick(); // held arrow must not oscillate choices
	platform._pi.enter = true;
	scene.tick();
	check(scene.states()[0].graph.nodes[0].active && scene.states()[1].open &&
	          scene.dialogueOpen() && !scene.states()[0].graph.nodes[3].completed,
	      "Choice navigation failed or second queued dialogue was overwritten");
	check(scene.game()._res._ctData[256 + 4 * 16 + 12] == 0,
	      "Dialogue completion did not refresh door collision");
	for (int i = 0; i < 5; ++i)
		scene.tick();
	check(scene.dialogueOpen(), "Held Enter dismissed a queued conversation");
	platform._pi = {};
	scene.tick();
	platform._pi.enter = true;
	scene.tick();
	check(!scene.dialogueOpen() && scene.states()[0].graph.nodes[4].active,
	      "Second conversation did not start the pickup effect");
	platform._pi = {};
	for (int i = 0; i < 32; ++i)
		scene.tick();
	check(!scene.states()[0].graph.nodes[4].active && scene.states()[0].graph.nodes[4].completed,
	      "Pickup effect did not finish");
	scene.restart();
	check(!scene.dialogueOpen() && !scene.states()[0].graph.nodes[2].completed,
	      "Restart retained dialogue queue or action state");
	// Timers still run while reading and can cancel queued/current dialogue safely.
	m.nodes.push_back(makeNode(m, NodeKind::Timer, m.a));
	m.nodes[5].duration = 3;
	m.wires.push_back({m.nodes[1].id, m.nodes[5].id, Signal::Used, Input::Activate});
	m.wires.push_back({m.nodes[5].id, m.nodes[2].id, Signal::Completed, Input::Stop});
	m.wires.push_back({m.nodes[5].id, m.nodes[3].id, Signal::Completed, Input::Stop});
	p.modules = {m, downstream};
	Scene canceled(p, root, source, &platform);
	platform._pi = {};
	platform._pi.enter = true;
	canceled.tick();
	platform._pi = {};
	for (int i = 0; i < 4; ++i)
		canceled.tick();
	check(!canceled.dialogueOpen() && !canceled.states()[0].graph.nodes[0].active &&
	          !canceled.states()[0].graph.nodes[4].active,
	      "Stopped dialogue remained visible or emitted its completion action");
	canceled.render();
	m.nodes[2].text = " ";
	m.nodes[2].choices = {};
	m.wires = {{m.nodes[1].id, m.nodes[2].id, Signal::Used, Input::Activate}};
	p.modules = {m, downstream};
	Scene blank(p, root, source, &platform);
	platform._pi = {};
	platform._pi.enter = true;
	blank.tick();
	check(blank.dialogueOpen(), "Whitespace-only dialogue has no dismissible page");
	platform._pi = {};
	blank.tick();
	platform._pi.enter = true;
	blank.tick();
	check(!blank.dialogueOpen() && blank.states()[0].graph.nodes[2].completed,
	      "Whitespace dialogue trapped an active node");
}
