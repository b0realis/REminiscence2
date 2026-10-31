// SPDX-License-Identifier: GPL-3.0-or-later
static editor::Project guardFixture(const std::filesystem::path &root) {
	using namespace editor;
	auto p = makeProject();
	p.spawn.x = 48;
	auto m = makeGraph(p, 5, {p.rooms[0].id, 176, 216});
	auto n = makeNode(m, NodeKind::Actor, m.a);
	n.actorStyle = 1;
	n.width = 40;
	n.height = 48;
	// Synthetic color cells verify the renderer without proprietary fixtures.
	Image sheet{8, 4, std::vector<uint32_t>(32)};
	for (int i = 0; i < 32; ++i)
		sheet.pixels[i] = rgba(20 + i * 5, 150, 60);
	writePNG(sheet, root / "guard.png");
	n.visuals[0].art = {"guard.png", 0, 0, 8, 4};
	m.nodes = {n};
	p.modules = {m};
	check(deserialize(serialize(p)).modules[0].nodes[0].actorStyle == 1,
	      "Guard behavior lost during project serialization");
	for (int kind : {2, -1}) {
		auto invalid = p;
		invalid.modules[0].nodes[0].actorStyle = kind;
		rejects([&] { deserialize(serialize(invalid)); }, "Unknown guard behavior accepted");
	}
	auto invalid = p;
	invalid.modules[0].nodes[0].visuals[0].art.w = 7;
	rejects([&] { deserialize(serialize(invalid)); }, "Unequal guard sheet cells accepted");
	Images images;
	images.root = root;
	Image left{256, 224, std::vector<uint32_t>(256 * 224)}, right = left;
	drawGuard(left, n, 0, true, 96, 216, images);
	drawGuard(right, n, 0, false, 96, 216, images);
	check(left.pixels != right.pixels, "Guard art did not mirror");
	for (int y = 168; y < 216; ++y)
		for (int x = 84; x < 124; ++x)
			check(left.pixels[y * 256 + x] == right.pixels[y * 256 + 207 - x],
			      "Guard mirror shifted its anchor");
	return p;
}
static void guardSceneChecks(const std::filesystem::path &root, const assets::Source &source) {
	using namespace editor;
	using json = nlohmann::json;
	auto p = guardFixture(root);
	struct GuardSystem : HeadlessSystem {
		bool pause = false;
		bool consumePauseRequest() override {
			bool p = pause;
			pause = false;
			return p;
		}
	} stub;
	auto scene = std::make_unique<Scene>(p, root, source, &stub);
	auto idle = [&](int count) {
		stub._pi = {};
		while (count--)
			scene->tick();
	};
	const int startingLife = scene->game()._pgeLive[0].life;
	idle(10);
	check(scene->states()[0].graph.nodes[0].guardPhase == 1, "Guard never aimed");
	const auto saved = scene->saveSession();
	const auto frame = scene->render().pixels;
	stub.pause = true;
	scene->tick();
	const auto pausedFrame = scene->render().pixels;
	idle(20);
	check(scene->paused() && scene->saveSession() == saved && scene->render().pixels == pausedFrame,
	      "Paused guard advanced its attack or animation");
	stub._pi.enter = true;
	scene->tick();
	idle(70);
	check(scene->game()._pgeLive[0].life < startingLife, "Guard rifle failed to damage Conrad");
	scene.reset();
	scene = std::make_unique<Scene>(p, root, source, &stub);
	scene->restoreSession(saved);
	check(scene->saveSession() == saved && scene->render().pixels == frame,
	      "Guard attack phase did not restore exactly");
	auto broken = json::parse(saved);
	broken["modules"][0]["nodes"][0]["guard"][0] = 6;
	rejects([&] { scene->restoreSession(broken.dump()); }, "Invalid guard phase accepted");
	check(scene->saveSession() == saved, "Invalid guard state mutated the live scene");
	// Drive the public graph inputs, not the actor fields, to cancel a pending shot.
	auto &states = const_cast<std::vector<ModuleState> &>(scene->states());
	GraphMachine machine(p.modules[0], states[0].graph, 10);
	machine.send(0, Input::Friendly);
	const int life = scene->game()._pgeLive[0].life;
	idle(100);
	check(scene->game()._pgeLive[0].life == life && states[0].graph.nodes[0].guardPhase == 0,
	      "Pacified guard completed a pending attack");
	// Same floor but a closed wall: no perception through the intervening tile.
	auto blocked = p;
	blocked.rooms[0].collision[5 * 16 + 7] = 1;
	scene.reset();
	scene = std::make_unique<Scene>(blocked, root, source, &stub);
	const int protectedLife = scene->game()._pgeLive[0].life;
	idle(150);
	check(scene->game()._pgeLive[0].life == protectedLife &&
	          scene->states()[0].graph.nodes[0].guardPhase == 0,
	      "Guard fired through a solid wall");
	// Real native gun input in both directions and all cosmetic styles. The
	// guard is held still so only the native shot query can be responsible.
	for (bool left : {false, true})
		for (int style = 0; style < 3; ++style) {
			auto combat = p;
			combat.spawn.x = left ? 192 : 48;
			combat.spawnLeft = left;
			combat.modules[0].nodes[0].at.x = left ? 64 : 176;
			scene.reset();
			scene = std::make_unique<Scene>(combat, root, source, &stub);
			scene->game()._preferences.pistolStyle = weapon::Style(style);
			const int enemy = scene->states()[0].actors[0];
			auto &combatState = const_cast<std::vector<ModuleState> &>(scene->states());
			GraphMachine hold(combat.modules[0], combatState[0].graph);
			hold.send(0, Input::Hold);
			bool fired = false, hurt = false;
			for (int t = 0; t < 120; ++t) {
				stub._pi = {};
				stub._pi.space = t == 20;
				stub._pi.shift = t >= 45;
				scene->tick();
				fired |= scene->game()._pgeLive[2].room_location == 0;
				hurt |= scene->game()._pgeLive[enemy].life < 2;
			}
			check(fired && hurt, "Native pistol could not damage authored guard");
			check(scene->game()._pgeLive[enemy].life == 0, "Repeated shots did not defeat guard");
			const auto &trace = scene->states()[0].graph.trace;
			check(std::count_if(trace.begin(), trace.end(),
			                    [](const auto &e) {
				                    return e.kind == TraceKind::Signal && e.signal == Signal::Died;
			                    }) == 1,
			      "Guard death event was missing or repeated");
		}
	stub._pi = {};
	std::cout << "Armed guards: aim, damage, walls, pacification, pause/save, death and 6 pistol "
	             "variants passed\n";
}
