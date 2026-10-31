// SPDX-License-Identifier: GPL-3.0-or-later
static void sceneBoundaryChecks(const std::filesystem::path &root, const assets::Source &source) {
	using namespace editor;
	for (const bool left : {true, false})
		for (int mode = 0; mode < 3; ++mode) {
			auto p = makeProject();
			p.spawn.x = left ? 24 : 216;
			p.spawnLeft = left;
			HeadlessSystem stub;
			Scene scene(p, root, source, &stub);
			for (int t = 0; t < 180; ++t) {
				stub._pi.dirMask = left ? PlayerInput::DIR_LEFT : PlayerInput::DIR_RIGHT;
				if (mode == 2)
					stub._pi.dirMask |= PlayerInput::DIR_DOWN;
				stub._pi.shift = mode != 0;
				scene.tick();
				const auto &a = scene.game()._pgeLive[0];
				// Native stride frames may briefly overlap the edge; the room must
				// remain valid and Conrad must remain visible and able to turn back.
				check(scene.currentRoom() == 0 && a.pos_x >= -24 && a.pos_x <= 272 && !scene.dead(),
				      ("Walking/running/rolling escaped an unlinked room edge: mode=" +
				       std::to_string(mode) + " left=" + std::to_string(left) +
				       " x=" + std::to_string(a.pos_x) + " room=" + std::to_string(a.room_location))
				          .c_str());
			}
			stub._pi = {};
			stub._pi.dirMask = left ? PlayerInput::DIR_RIGHT : PlayerInput::DIR_LEFT;
			for (int t = 0; t < 80; ++t)
				scene.tick();
			check(scene.game()._pgeLive[0].pos_x > 16 && scene.game()._pgeLive[0].pos_x < 224,
			      "Player could not turn away from a sealed edge");
		}
	for (int direction = 0; direction < 4; ++direction) {
		auto p = makeProject();
		HeadlessSystem stub;
		Scene closed(p, root, source, &stub);
		closed.tick(); // Initialize native collision lists before direct rule probes.
		auto &g = closed.game();
		auto &a = g._pgeLive[0];
		closed.beforeActor(g, a);
		const auto x = a.pos_x, y = a.pos_y;
		if (direction == 0)
			a.pos_y = -4;
		if (direction == 1)
			a.pos_y = 220;
		if (direction == 2)
			a.pos_x = 260;
		if (direction == 3)
			a.pos_x = -16;
		g.pge_setupOtherPieges(&a, a.init_PGE);
		check(a.room_location == 0 && g._currentRoom == 0 && a.pos_x == x && a.pos_y == y,
		      "Native coordinate wrap entered a missing authored screen");
		// The same transition must still work when a designer connects that edge.
		auto next = p.rooms.front();
		next.id = "neighbor";
		next.column = 1;
		p.rooms.push_back(next);
		p.rooms[0].exits[direction] = next.id;
		Scene linked(p, root, source, &stub);
		linked.tick();
		auto &lg = linked.game();
		auto &la = lg._pgeLive[0];
		linked.beforeActor(lg, la);
		if (direction == 0)
			la.pos_y = -4;
		if (direction == 1)
			la.pos_y = 220;
		if (direction == 2)
			la.pos_x = 260;
		if (direction == 3)
			la.pos_x = -16;
		lg.pge_setupOtherPieges(&la, la.init_PGE);
		check(la.room_location == 1 && lg._currentRoom == 1,
		      "Boundary guard blocked an authored room link");
	}
}

static void sceneMenuChecks(const std::filesystem::path &root, const assets::Source &source) {
	using namespace editor;
	struct MenuSystem : HeadlessSystem {
		std::function<void(settings::Session &)> menu;
		void showSettings(settings::Session &s) override { menu(s); }
	} stub;
	auto p = makeProject();
	Scene scene(p, root, source, &stub);
	const auto before = scene.saveSession();
	int opened = 0;
	stub.menu = [&](settings::Session &s) {
		++opened;
		check(s.authoredScene && s.campaign && s.canSave && !stub._pi.escape,
		      "Authored QoL menu did not consume its opening input");
		check(scene.saveSession() == before, "Opening QoL advanced the simulation");
		std::string why;
		check(s.command(settings::Action::SaveGame, 1, why), "QoL checkpoint save failed");
		scene.game()._pgeLive[0].life = 1;
		check(s.command(settings::Action::LoadGame, 1, why) && scene.game()._pgeLive[0].life == 5,
		      "QoL checkpoint did not restore the authored scene");
		auto values = s.values;
		values.conradShirt = appearance::Shirt::White;
		values.pistolStyle = weapon::Style::Laser;
		values.filter = settings::Filter::Xbrz;
		check(s.apply(values, why) && scene.game()._preferences.pistolStyle == weapon::Style::Laser,
		      "Authored settings could not apply shared graphics/cosmetics");
		check(!s.command(settings::Action::StartLevel, 0, why),
		      "Native level selection replaced the authored scene");
		stub._pi.enter = true;
		s.action = settings::Action::Resume;
	};
	stub._pi.escape = true; // SDL maps both Esc and F10 to this pause action.
	check(scene.handleMenuInput(nullptr, root) && opened == 1 && !stub._pi.quit &&
	          !stub._pi.enter && !stub._pi.escape,
	      "QoL input exited or leaked into gameplay");
	check(!scene.handleMenuInput(nullptr, root), "QoL reopened without a fresh request");
	stub.menu = [&](settings::Session &s) { s.action = settings::Action::Quit; };
	stub._pi.escape = true;
	check(scene.handleMenuInput(nullptr, root) && stub._pi.quit, "Explicit Exit Game did not quit");
}
