// SPDX-License-Identifier: GPL-3.0-or-later
// Real native movement and recovery regressions shared by all supported editions.
static void gameplayAuditChecks(const std::filesystem::path &root, const assets::Source &source,
                                HeadlessSystem &stub) {
	using namespace editor;
	using json = nlohmann::json;
	auto p = makeProject();
	auto m = makeGraph(p, 5, p.spawn);
	m.nodes.push_back(makeNode(m, NodeKind::Door, {p.spawn.room, 160, 216}));
	p.modules = {m};
	stub._pi = {};
	Scene visible(p, root, source, &stub);
	const auto closed = visible.render();
	p.modules[0].nodes[0].enabled = false;
	Scene disabled(p, root, source, &stub);
	check(disabled.render().pixels == closed.pixels,
	      "Disabled physical door became an invisible wall");
	// Include single-cell doors and doors flush with a horizontal screen edge.
	for (bool left : {false, true}) {
		for (int edge : {160, left ? 0 : 240}) {
			for (bool run : {false, true}) {
				auto door = p;
				addRoom(door, 1, 0);
				connectRooms(door, 0, 2, 1, true);
				door.spawn.x = left ? 224 : 32;
				door.spawnLeft = left;
				door.modules[0].nodes[0].at.x = edge;
				stub._pi = {};
				Scene blocked(door, root, source, &stub);
				for (int t = 0; t < 250; ++t) {
					stub._pi.dirMask = left ? PlayerInput::DIR_LEFT : PlayerInput::DIR_RIGHT;
					stub._pi.shift = run;
					blocked.tick();
				}
				const auto &a = blocked.game()._pgeLive[0];
				check(blocked.currentRoom() == 0 && (left ? a.pos_x >= edge : a.pos_x <= edge),
				      "Native movement tunneled through a narrow authored door");
			}
		}
	}
	// Open gates and a jump entirely above a low gate remain traversable.
	for (bool open : {false, true}) {
		auto gate = makeProject();
		auto module = makeGraph(gate, 5, gate.spawn);
		module.nodes.push_back(makeNode(module, NodeKind::Door, {gate.spawn.room, 160, 216}));
		module.nodes[0].height = 72;
		module.nodes[0].initiallyActive = open;
		gate.modules = {module};
		Scene crossing(gate, root, source, &stub);
		auto &g = crossing.game();
		auto &a = g._pgeLive[0];
		a.pos_x = 128;
		a.pos_y = open ? 214 : 128;
		crossing.beforeActor(g, a);
		a.pos_x = 184;
		crossing.afterSimulation(g);
		check(a.pos_x == 184, "Door sweep blocked an open gate or a clear jump");
	}
	// Disconnects must freeze the whole custom simulation until a fresh confirm.
	struct DisconnectSystem : HeadlessSystem {
		bool requested = false;
		bool consumePauseRequest() override {
			const bool result = requested;
			requested = false;
			return result;
		}
	} disconnect;
	p = makeProject();
	m = makeGraph(p, 0, {p.spawn.room, 192, 216}); // door and its button
	m.nodes[1].at = p.spawn;
	m.nodes.push_back(makeNode(m, NodeKind::Start, p.spawn));
	m.nodes.push_back(makeNode(m, NodeKind::Timer, p.spawn));
	m.nodes[3].duration = 30;
	m.wires.push_back({m.nodes[2].id, m.nodes[3].id, Signal::Completed, Input::Activate});
	p.modules = {m};
	Scene paused(p, root, source, &disconnect);
	paused.tick();
	check(paused.states()[0].graph.nodes[3].active, "Pause fixture did not start its timer");
	const auto frozen = paused.saveSession();
	disconnect.requested = disconnect._pi.enter = true;
	for (int i = 0; i < 100; ++i)
		paused.tick();
	check(paused.paused() && paused.saveSession() == frozen,
	      "Disconnect failed to freeze native simulation and node timers");
	disconnect._pi.enter = false;
	paused.tick();
	disconnect._pi.enter = true;
	paused.tick();
	check(!paused.paused() && paused.saveSession() == frozen,
	      "Resume gesture advanced the world or operated a puzzle");
	paused.tick();
	check(!paused.states()[0].graph.nodes[0].active, "Held resume gesture activated a door");
	disconnect._pi.enter = false;
	paused.tick();
	disconnect._pi.enter = true;
	paused.tick();
	check(paused.states()[0].graph.nodes[0].active, "Fresh interaction failed after resuming");
	// A checkpoint must restore the puzzle and carried quest state, not merely
	// respawn downstream of freshly locked gates without the previously used key.
	p = makeProject();
	m = makeGraph(p, 5, p.spawn);
	for (auto k : {NodeKind::Item, NodeKind::Door, NodeKind::Checkpoint})
		m.nodes.push_back(makeNode(m, k, p.spawn));
	m.nodes[0].campaignKey = "checkpoint-key";
	m.nodes[1].at.x = 192;
	m.wires = {{m.nodes[0].id, m.nodes[1].id, Signal::Completed, Input::Open},
	           {m.nodes[0].id, m.nodes[2].id, Signal::Completed, Input::Activate}};
	p.modules = {m};
	stub._pi = {};
	Scene checkpoint(p, root, source, &stub);
	checkpoint.tick();
	check(checkpoint.states()[0].graph.nodes[1].active, "Checkpoint fixture did not open door");
	checkpoint.game()._pgeLive[0].life = 0;
	checkpoint.restart();
	check(!checkpoint.dead() && checkpoint.states()[0].graph.nodes[1].active &&
	          checkpoint.states()[0].graph.nodes[0].collected,
	      "Checkpoint restart reset puzzle state");
	const auto snap = checkpoint.saveSession();
	check(json::parse(snap).at("localValues").at("item:checkpoint-key") == true,
	      "Standalone snapshot lost quest inventory");
	Scene restored(p, root, source, &stub);
	restored.restoreSession(snap);
	check(restored.saveSession() == snap, "Standalone quest snapshot did not round trip");
	// Contradictory dialogue metadata used to load successfully and then index
	// past the last page as soon as the player pressed Enter.
	for (int mutation = 0; mutation < 6; ++mutation) {
		auto bad = json::parse(snap);
		if (mutation == 0)
			bad["schema"] = 1.0;
		if (mutation == 1)
			bad["dialogue"] = {"orphan page"};
		if (mutation == 2)
			bad["requests"] = {{0, -1, 0, "orphan request"}};
		if (mutation == 3)
			bad["localValues"]["item:checkpoint-key"] = 1;
		if (mutation == 4)
			bad["localValues"]["item:../invalid"] = true;
		if (mutation == 5)
			bad["localValues"] = json::array();
		rejects([&] { restored.restoreSession(bad.dump()); }, "Inconsistent snapshot accepted");
		check(restored.saveSession() == snap, "Bad snapshot changed the running world");
	}
	auto old = json::parse(snap);
	old.erase("localValues");
	restored.restoreSession(old.dump());
	check(restored.states()[0].graph.nodes[1].active,
	      "Earlier snapshot without standalone quest state was rejected");
	stub._pi = {};
}
