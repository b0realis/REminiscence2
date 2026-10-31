// SPDX-License-Identifier: GPL-3.0-or-later
static editor::Project targetFixture() {
	using namespace editor;
	auto p = makeProject();
	p.spawn.x = 48;
	auto m = makeGraph(p, 5, {p.rooms[0].id, 224, 216});
	m.nodes.clear(); m.wires.clear();
	auto target = makeNode(m, NodeKind::Target, m.a);
	target.id = "plate";
	target.travel = 96; target.duration = 180;
	m.nodes.push_back(target);
	auto start = makeNode(m, NodeKind::Start, m.a); start.id = "start";
	m.nodes.push_back(start);
	auto count = makeNode(m, NodeKind::Counter, m.a); count.id = "count"; count.goal = 32;
	m.nodes.push_back(count);
	auto light = makeNode(m, NodeKind::PickupAnimation, m.a); light.id = "light";
	light.visuals[1].frames = 1; light.visuals[1].fps = 8; light.visuals[1].loop = false;
	m.nodes.push_back(light);
	m.wires = {{"start", "plate", Signal::Completed, Input::Activate},
	           {"plate", "count", Signal::Hit, Input::Progress},
	           {"plate", "light", Signal::Hit, Input::Reset},
	           {"plate", "light", Signal::Hit, Input::Activate}};
	p.modules = {m};
	return p;
}
static void targetChecks() {
	using namespace editor;
	auto p = targetFixture();
	check(deserialize(serialize(p)).modules[0].nodes[0].travel == 96, "Target travel lost");
	const auto json = nlohmann::json::parse(serialize(p));
	check(!json["modules"][0]["nodes"][1].contains("travel"), "Unrelated module revisions gained target settings");
	for (int travel : {-1, 193}) {
		auto bad = p; bad.modules[0].nodes[0].travel = travel;
		rejects([&] { deserialize(serialize(bad)); }, "Invalid target travel accepted");
	}
	auto &m = p.modules[0];
	auto s = initialGraphState(m);
	GraphMachine(m, s, 0).advance();
	check(s.nodes[0].active && targetFeet(m.nodes[0], s.nodes[0]) == 216, "Target did not start at lower stop");
	GraphMachine(m, s, 90).advance();
	check(targetFeet(m.nodes[0], s.nodes[0]) == 120, "Target missed upper stop");
	GraphMachine(m, s, 180).advance();
	check(targetFeet(m.nodes[0], s.nodes[0]) == 216, "Target did not reverse");
	GraphMachine(m, s, 200).advance();
	GraphMachine(m, s, 200).send(0, Input::Stop);
	const int stopped = targetFeet(m.nodes[0], s.nodes[0]);
	GraphMachine(m, s, 500).advance();
	check(targetFeet(m.nodes[0], s.nodes[0]) == stopped, "Stopped target moved");
	GraphMachine(m, s, 500).signal(0, Signal::Hit);
	GraphMachine(m, s, 505).signal(0, Signal::Hit);
	check(s.nodes[2].count == 2 && s.nodes[3].changedAt == 505 && s.nodes[3].generation == 4,
	      "Repeated hit did not restart light");
	GraphMachine(m, s, 509).advance();
	check(!s.nodes[3].active, "Hit light did not expire");
	GraphMachine(m, s, 510).send(0, Input::Activate);
	GraphMachine(m, s, 510).advance();
	check(targetFeet(m.nodes[0], s.nodes[0]) == stopped, "Resumed target jumped to its lower stop");
	const int resumed = s.nodes[0].targetPhase;
	GraphMachine(m, s, 511).advance();
	check(s.nodes[0].targetPhase == resumed + 1, "Resumed target failed to continue its previous phase");
	GraphMachine(m, s, 515).send(0, Input::Disable);
	GraphMachine(m, s, 516).signal(0, Signal::Hit);
	check(s.nodes[2].count == 2, "Disabled target emitted a hit");
	GraphMachine(m, s, 516).send(0, Input::Enable);
	GraphMachine(m, s, 516).send(0, Input::Reset);
	check(targetFeet(m.nodes[0], s.nodes[0]) == 216, "Reset target retained travel");
}
static void targetSceneChecks(const std::filesystem::path &root, const assets::Source &source) {
	using namespace editor;
	// Test the native firing path rather than synthesizing Hit graph events.
	for (bool left : {false, true}) for (int style = 0; style < 3; ++style) {
		auto p = targetFixture();
		p.spawnLeft = left; p.spawn.x = left ? 208 : 48;
		auto &plate = p.modules[0].nodes[0];
		plate.at.x = left ? 48 : 208;
		plate.travel = 0;
		plate.height = 56; // covers every edition's native muzzle centerline
		HeadlessSystem stub;
		auto scene = std::make_unique<Scene>(p, root, source, &stub);
		scene->game()._preferences.pistolStyle = weapon::Style(style);
		int flashes = 0, shots = 0, lastFrame = -1;
		bool offBetweenShots = false;
		uint64_t generation = 0;
		for (int t = 0; t < 160; ++t) {
			stub._pi = {}; stub._pi.space = t == 20; stub._pi.shift = t >= 45 && t < 130;
			scene->tick();
			auto &g = scene->game();
			const int gun = g._res._pgeInit[0].data[0];
			const int effect = g._res._pgeInit[gun].data[0];
			const int frame = g._pgeLive[effect].anim_number;
			if (frame == 287 && lastFrame != 287) ++shots;
			lastFrame = frame;
			const auto &s = scene->states()[0].graph.nodes;
			if (t > 60 && t < 120 && !s[3].active) offBetweenShots = true;
			if (s[3].generation != generation) {
				++flashes; generation = s[3].generation;
				check(s[3].active, "Hit did not light indicator immediately");
				if (style == 2) {
					const auto &shot = g._weaponFx.shots[(g._weaponFx.next - 1) % g._weaponFx.shots.size()];
					const int edge = left ? plate.at.x + plate.width - 1 : plate.at.x;
					check(shot.impact && shot.x + shot.direction * shot.rayLength == edge,
					      "Laser continued past the plate or stopped short");
				}
			}
		}
		const int hits = scene->states()[0].graph.nodes[2].count;
		check(hits >= 3 && hits == shots && flashes == hits, "Native pistol failed repeated target hits");
		check(!scene->states()[0].graph.nodes[3].active && offBetweenShots, "Hit light stuck on during or after repeated firing");
		for (int miss : {0, 1, 2}) {
			auto q = p;
			if (miss == 0) q.modules[0].nodes[0].at.y = 120; // above the pistol
			if (miss == 1) q.rooms[0].collision[5 * 16 + 8] = 1; // solid intervening wall
			if (miss == 2) q.modules[0].nodes[0].at.x = left ? 232 : 0; // behind Conrad
			scene.reset(); scene = std::make_unique<Scene>(q, root, source, &stub);
			scene->game()._preferences.pistolStyle = weapon::Style(style);
			for (int t = 0; t < 100; ++t) {
				stub._pi = {}; stub._pi.space = t == 20; stub._pi.shift = t >= 45;
				scene->tick();
			}
			check(scene->states()[0].graph.nodes[2].count == 0, "Missed/blocked/backward shot lit target");
		}
	}
	// Saved motion uses the same clock as the game; pause freezes it.
	auto p = targetFixture();
	struct TargetSystem : HeadlessSystem {
		bool pause = false;
		bool consumePauseRequest() override { const bool p = pause; pause = false; return p; }
	} stub;
	Scene scene(p, root, source, &stub);
	for (int i = 0; i < 40; ++i) scene.tick();
	const auto saved = scene.saveSession();
	const auto pixels = scene.render().pixels;
	stub.pause = true; scene.tick();
	for (int i = 0; i < 20; ++i) scene.tick();
	check(scene.paused() && scene.saveSession() == saved, "Paused target advanced");
	scene.restoreSession(saved);
	check(scene.render().pixels == pixels, "Target motion changed on restore");
	auto bad = nlohmann::json::parse(saved);
	bad["modules"][0]["nodes"][0]["targetPhase"] = 180;
	rejects([&] { scene.restoreSession(bad.dump()); }, "Invalid target clock accepted");
	std::cout << "Moving targets: native hits/misses, occlusion, 6 pistol variants, repeated lights, motion and saves passed\n";
}
