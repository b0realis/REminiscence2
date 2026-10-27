// SPDX-License-Identifier: GPL-3.0-or-later
static void stateMachineChecks() {
	using namespace editor;
	auto p = makeProject();
	auto m = makeGraph(p, 5, p.spawn);
	for (auto kind :
	     {NodeKind::State, NodeKind::State, NodeKind::Timer, NodeKind::Door, NodeKind::Actor})
		m.nodes.push_back(makeNode(m, kind, p.spawn));
	m.nodes[0].initiallyActive = true;
	m.nodes[2].duration = 2;
	m.wires = {{m.nodes[0].id, m.nodes[2].id, Signal::Entered, Input::Activate},
	           {m.nodes[2].id, m.nodes[0].id, Signal::Completed, Input::Progress},
	           {m.nodes[0].id, m.nodes[1].id, Signal::Completed, Input::Activate},
	           {m.nodes[1].id, m.nodes[3].id, Signal::Entered, Input::Open}};
	auto state = initialGraphState(m);
	GraphMachine start(m, state);
	start.automatic = false;
	start.start();
	check(state.nodes[0].active && !state.nodes[1].active && !state.nodes[2].active &&
	          state.pending.size() == 1,
	      "Start must show the initial state before delivering its wire");
	check(start.step() && state.nodes[2].active && state.pending.empty(),
	      "Debugger did not deliver exactly one wire");
	GraphMachine tick(m, state, 2);
	tick.automatic = false;
	tick.advance();
	check(!state.nodes[3].active && state.pending.size() == 1,
	      "Timed completion escaped step mode");
	tick.step();
	check(state.nodes[0].completed && !state.nodes[1].active,
	      "Step jumped across a state transition");
	tick.step();
	check(!state.nodes[0].active && state.nodes[1].active && !state.nodes[3].active,
	      "States were not mutually exclusive");
	tick.step();
	check(state.nodes[3].active && state.pending.empty(), "State entry did not open door");
	check(std::any_of(
	          state.trace.begin(), state.trace.end(),
	          [](auto &e) { return e.kind == TraceKind::Signal && e.signal == Signal::Exited; }),
	      "State exit missing from trace");
	auto fast = initialGraphState(m);
	GraphMachine(m, fast).start();
	GraphMachine(m, fast, 2).advance();
	for (size_t i = 0; i < state.nodes.size(); ++i)
		check(state.nodes[i].active == fast.nodes[i].active &&
		          state.nodes[i].completed == fast.nodes[i].completed,
		      "Stepped/debug and gameplay evaluators disagree");
	GraphMachine gate(m, state, 3);
	gate.send(0, Input::Progress);
	check(!state.nodes[0].active, "Inactive state accepted Progress");
	gate.send(1, Input::Disable);
	check(!state.nodes[1].active, "Disabling state left it active");
	gate.send(4, Input::Friendly);
	gate.send(4, Input::Patrol);
	check(state.nodes[4].friendly && state.nodes[4].patrol, "Actor control inputs failed");
	gate.send(4, Input::Hostile);
	gate.send(4, Input::Hold);
	check(state.nodes[4].held && !state.nodes[4].patrol,
	      "Hold must pause hostile AI, not remove its patrol limits");
	gate.send(4, Input::Patrol);
	check(!state.nodes[4].held && state.nodes[4].patrol, "Start patrol must release held AI");
	p.modules = {m};
	check(serialize(deserialize(serialize(p))) == serialize(p), "State/actor project round trip");
	p.modules[0].nodes[1].initiallyActive = true;
	rejects([&] { deserialize(serialize(p)); }, "Duplicate initial states accepted");
	// A feedback cycle terminates, is explicitly diagnosed, and can run again.
	m.wires = {{m.nodes[0].id, m.nodes[1].id, Signal::Entered, Input::Activate},
	           {m.nodes[1].id, m.nodes[0].id, Signal::Entered, Input::Activate}};
	auto cycle = initialGraphState(m);
	GraphMachine bounded(m, cycle);
	bounded.start();
	check(cycle.pending.empty() && cycle.trace.size() < 20 &&
	          std::any_of(cycle.trace.begin(), cycle.trace.end(),
	                      [](auto &e) { return e.kind == TraceKind::Cycle; }),
	      "Cycle was unbounded or silently truncated");
	for (int i = 0; i < 500; ++i)
		bounded.send(4, Input::Friendly);
	check(cycle.trace.size() == 256, "Runtime trace exceeded memory bound");
}
static void actorSceneChecks(const std::filesystem::path &root, const assets::Source &source,
                             HeadlessSystem &platform) {
	using namespace editor;
	auto p = makeProject();
	auto m = makeGraph(p, 5, p.spawn);
	m.nodes.push_back(makeNode(m, NodeKind::Actor, {p.rooms[0].id, 80, 216}));
	m.nodes[0].friendly = true;
	m.nodes[0].patrol = true;
	m.nodes[0].patrolLeft = 64;
	m.nodes[0].patrolRight = 112;
	m.nodes.push_back(makeNode(m, NodeKind::Dialogue, p.spawn));
	m.nodes[1].text = "I can help you.";
	m.wires = {{m.nodes[0].id, m.nodes[1].id, Signal::Used, Input::Activate}};
	p.modules = {m};
	platform._pi = {};
	Scene scene(p, root, source, &platform);
	const int id = scene.states()[0].actors[0];
	check(id >= 5, "Actor node did not instantiate a native actor");
	for (int i = 0; i < 180; ++i) {
		scene.tick();
		const auto &a = scene.game()._pgeLive[id];
		check(a.pos_x >= 64 && a.pos_x <= 112, "Friendly actor escaped patrol bounds");
	}
	check(std::any_of(
	          scene.states()[0].graph.trace.begin(), scene.states()[0].graph.trace.end(),
	          [](auto &e) { return e.kind == TraceKind::Signal && e.signal == Signal::Reached; }),
	      "Patrol endpoint did not emit an event");
	auto &actor = scene.game()._pgeLive[id];
	auto &player = scene.game()._pgeLive[0];
	player.pos_x = actor.pos_x;
	player.pos_y = actor.pos_y;
	platform._pi.enter = true;
	scene.tick();
	check(scene.dialogueOpen(), "Friendly actor interaction did not start connected dialogue");
	platform._pi = {};
	scene.tick();
	platform._pi.enter = true;
	scene.tick();
	platform._pi = {};
	check(!scene.dialogueOpen(), "Friendly conversation did not dismiss");
	actor.life = 0;
	scene.tick();
	scene.tick();
	int deaths = 0;
	for (const auto &e : scene.states()[0].graph.trace)
		if (e.kind == TraceKind::Signal && e.signal == Signal::Died)
			++deaths;
	check(deaths == 1, "Actor death should emit exactly once");
}
