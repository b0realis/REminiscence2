// SPDX-License-Identifier: GPL-3.0-or-later
// Shared fixture also exercises validation and signal timing without game assets.
static editor::Project liftGraphChecks(const std::filesystem::path &root) {
	using namespace editor;
	auto p = makeProject();
	addRoom(p, 0, 1);
	connectRooms(p, 0, 1, 1, true);
	p.spawn.x = 120;
	auto m = makeGraph(p, 5, {p.rooms[0].id, 96, 216});
	m.nodes.push_back(makeNode(m, NodeKind::Lift, m.a));
	m.nodes[0].duration = 90;
	writePNG({1, 1, {rgba(98, 51, 134)}}, root / "lift.png");
	m.nodes[0].visuals[0].art = {"lift.png", 0, 0, 1, 1};
	for (int room = 0; room < 2; ++room) {
		m.nodes.push_back(makeNode(m, NodeKind::Button, {p.rooms[room].id, 120, 216}));
		m.wires.push_back({m.nodes.back().id, m.nodes[0].id, Signal::Used, Input::Activate});
	}
	p.modules = {m};
	deserialize(serialize(p));
	auto invalid = p;
	invalid.rooms[1].exits[0].clear();
	rejects([&] { deserialize(serialize(invalid)); },
	        "Lift without reciprocal room links accepted");
	savePrefab(m, root, root / "lift.r2module.json");
	check(loadPrefab(p, m.a, root / "lift.r2module.json", root).nodes[0].kind == NodeKind::Lift,
	      "Lift prefab roundtrip");
	auto state = initialGraphState(m);
	GraphMachine start(m, state, 0);
	start.send(0, Input::Open);
	GraphMachine end(m, state, 90);
	end.advance();
	check(state.nodes[0].liftPosition == 90 && state.nodes[0].completed && !state.nodes[0].active,
	      "Graph debugger time jump lost lift arrival");
	check(std::count_if(state.trace.begin(), state.trace.end(),
	                    [](const auto &event) {
		                    return event.kind == TraceKind::Signal &&
							       event.signal == Signal::Opened;
	                    }) == 1,
	      "Lift arrival emitted incorrect signals");
	return p;
}
// Exercise native passengers, not just the graph's logical endpoint.
static void liftChecks(const std::filesystem::path &root, const assets::Source &source) {
	using namespace editor;
	using json = nlohmann::json;
	struct LiftSystem : HeadlessSystem {
		bool request = false;
		bool consumePauseRequest() override {
			const bool r = request;
			request = false;
			return r;
		}
	} stub;
	auto p = liftGraphChecks(root);
	auto scene = std::make_unique<Scene>(p, root, source, &stub);
	auto idle = [&](int ticks) {
		stub._pi = {};
		while (ticks--)
			scene->tick();
	};
	auto use = [&] {
		idle(1);
		stub._pi.enter = true;
		scene->tick();
		stub._pi = {};
	};
	idle(10);
	use();
	check(scene->states()[0].graph.nodes[0].liftPassenger, "Lift did not board Conrad");
	const int x = scene->game()._pgeLive[0].pos_x;
	for (int i = 0; i < 35; ++i) {
		stub._pi.dirMask = PlayerInput::DIR_RIGHT;
		scene->tick();
		check(!scene->dead() && scene->game()._pgeLive[0].pos_x == x, "Passenger left moving lift");
	}
	check(scene->currentRoom() == 1 && scene->game()._pgeLive[0].pos_y > 0 &&
	          scene->game()._pgeLive[0].pos_y < 150,
	      "Lift teleported instead of carrying");
	const auto picture = scene->render();
	const int carY = 216 * scene->states()[0].graph.nodes[0].liftPosition / 90;
	check(picture.pixels[(carY + 3) * 256 + 96] == rgba(98, 51, 134),
	      "Moving lift sprite hidden or detached from the passenger's feet");
	const auto saved = scene->saveSession();
	stub.request = true;
	idle(20);
	check(scene->paused() && scene->saveSession() == saved, "Paused lift continued moving");
	stub._pi.enter = true;
	scene->tick();
	idle(1);
	scene.reset();
	scene = std::make_unique<Scene>(p, root, source, &stub);
	scene->restoreSession(saved);
	check(scene->render().pixels == picture.pixels, "Saved lift restored wrong visual position");
	auto bad = json::parse(saved);
	bad["modules"][0]["nodes"][0]["liftPosition"] = 91;
	rejects([&] { scene->restoreSession(bad.dump()); }, "Invalid lift position accepted");
	check(scene->saveSession() == saved, "Rejected lift save mutated live state");
	idle(100);
	check(!scene->dead() && scene->currentRoom() == 1 && scene->game()._pgeLive[0].pos_y == 214 &&
	          !scene->states()[0].graph.nodes[0].liftPassenger,
	      "Downward arrival failed");
	use();
	idle(100);
	check(!scene->dead() && scene->currentRoom() == 0 && scene->game()._pgeLive[0].pos_y == 214,
	      "Upward arrival failed");
	// Put a caller at the other landing: first call travels empty, second boards.
	auto &actor = scene->game()._pgeLive[0];
	actor.room_location = 1;
	scene->game().pge_addToCurrentRoomList(&actor, 0);
	use();
	check(!scene->states()[0].graph.nodes[0].liftPassenger, "Empty car abducted distant caller");
	idle(100);
	check(scene->currentRoom() == 1 && actor.pos_y == 214, "Calling lift moved the caller");
	use();
	idle(100);
	check(!scene->dead() && scene->currentRoom() == 0, "Called lift could not carry caller back");
	// A designer can wire an arrival to an automatic return. The passenger stays
	// on board through the reversal; commands are not lost before world feedback.
	const auto liftId = p.modules[0].nodes[0].id;
	p.modules[0].wires.push_back({liftId, liftId, Signal::Opened, Input::Close});
	scene.reset();
	scene = std::make_unique<Scene>(p, root, source, &stub);
	idle(10);
	use();
	idle(95);
	check(scene->states()[0].graph.nodes[0].active &&
	          !scene->states()[0].graph.nodes[0].liftTarget &&
	          scene->states()[0].graph.nodes[0].liftPassenger,
	      "Scripted lift return lost its passenger or command");
	idle(100);
	check(!scene->dead() && scene->currentRoom() == 0 && scene->game()._pgeLive[0].pos_y == 214,
	      "Automatic lift return failed to arrive");
}
