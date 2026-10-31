// SPDX-License-Identifier: GPL-3.0-or-later
static editor::Project ambientChecks(const std::filesystem::path &root) {
	using namespace editor;
	using json = nlohmann::json;
	auto p = makeProject();
	auto m = makeGraph(p, 5, {p.spawn.room, 64, 144});
	m.nodes.push_back(makeNode(m, NodeKind::Start, m.a));
	m.nodes.push_back(makeNode(m, NodeKind::Ambient, m.a));
	auto &n = m.nodes[1];
	n.width = 128;
	n.height = 112;
	n.ambientAmount = 3;
	m.wires.push_back({m.nodes[0].id, n.id, Signal::Completed, Input::Activate});
	p.modules = {m};
	const auto serialized = serialize(p);
	const auto roundtrip = deserialize(serialized);
	check(serialize(roundtrip) == serialized, "Background motion roundtrip lost settings");
	const auto document = json::parse(serialized);
	check(!document["modules"][0]["nodes"][0].contains("ambientStyle"),
	      "Unrelated modules gained motion parameters and invalidated old save revisions");
	for (int kind = 0; kind < 4; ++kind) {
		auto invalid = p;
		auto &node = invalid.modules[0].nodes[1];
		if (kind == 0)
			node.layer = Layer::Front;
		if (kind == 1)
			node.ambientStyle = 8;
		if (kind == 2)
			node.ambientAmount = 4;
		if (kind == 3)
			node.duration = 0;
		rejects([&] { deserialize(serialize(invalid)); }, "Invalid background motion accepted");
	}
	Image original{256, 224, std::vector<uint32_t>(256 * 224)};
	for (int y = 0; y < original.height; ++y)
		for (int x = 0; x < original.width; ++x)
			original.pixels[y * 256 + x] =
			    rgba(30 + (x % 48) * 3, 35 + (y % 32) * 5, 40 + ((x + y) % 48) * 3);
	writePNG(original, root / "ambient-background.png");
	p.rooms[0].art[0] = {"ambient-background.png", 0, 0, 256, 224};
	Images images;
	for (int style = 0; style < 8; ++style) {
		n.ambientStyle = style;
		const int sampleAge = style == 7 ? 8 : n.duration / 4;
		p.modules[0].nodes[1] = n;
		check(serialize(deserialize(serialize(p))) == serialize(p),
		      "Background effect selection did not survive saving");
		auto first = original, changed = original, looped = original, stopped = original;
		drawNode(first, n, true, 0, images);
		drawNode(changed, n, true, sampleAge, images);
		drawNode(looped, n, true, n.duration, images);
		drawNode(stopped, n, false, sampleAge, images);
		check(first.pixels != changed.pixels, "Background machine does not move");
		check(first.pixels == looped.pixels, "Background machine has a loop seam");
		check(stopped.pixels == original.pixels, "Stopped background altered the artwork");
		auto repeated = original;
		drawNode(repeated, n, true, sampleAge, images);
		check(repeated.pixels == changed.pixels, "Effect depends on render order or random state");
		for (int y = 0; y < 224; ++y)
			for (int x = 0; x < 256; ++x) {
				const double u = 2.0 * (x - n.at.x + .5) / n.width - 1.0;
				const double v = 2.0 * (y - (n.at.y - n.height) + .5) / n.height - 1.0;
				if (u * u + v * v >= 1.0)
					check(changed.pixels[y * 256 + x] == original.pixels[y * 256 + x],
					      "Ambient motion escaped its soft elliptical mask");
				if (style == 3 && (u * u + v * v < .08 || u * u + v * v >= .90))
					check(changed.pixels[y * 256 + x] == original.pixels[y * 256 + x],
					      "Fan rotor moved its bearing or outer casing");
			}
		if (style >= 4) {
			Image black{256,224,std::vector<uint32_t>(256*224,rgba(0,0,0))};
			const auto still = black;
			drawNode(black,n,true,style == 7 ? 8 : 30,images);
			check(black.pixels != still.pixels, "Light or particles invisible over a black background");
			if (style == 7) {
				auto quiet = still;
				drawNode(quiet, n, true, n.duration - 1, images);
				check(quiet.pixels == still.pixels, "Electrical discharge has no quiet interval");
				// A tiny emitter at the room edge must remain confined even
				// late in a trajectory, and long sessions must retain the phase.
				auto edge = n;
				edge.at.x = 248;
				edge.at.y = 224;
				edge.width = edge.height = 8;
				for (const uint64_t age : {uint64_t(8), uint64_t(20), ~uint64_t(0)}) {
					auto frame = still, equivalent = still;
					drawNode(frame, edge, true, age, images);
					drawNode(equivalent, edge, true, age % edge.duration, images);
					check(frame.pixels == equivalent.pixels, "Long-running sparks lost their phase");
					for (int y = 0; y < 224; ++y)
						for (int x = 0; x < 256; ++x)
							if (x < 248 || y < 216)
								check(frame.pixels[y * 256 + x] == still.pixels[y * 256 + x],
								      "Edge sparks escaped their authored region");
				}
			}
		}
	}
	auto state = initialGraphState(m);
	GraphMachine begin(m, state, 0);
	begin.start();
	check(state.nodes[1].active, "On start did not start background machinery");
	GraphMachine later(m, state, 1200);
	later.advance();
	check(state.nodes[1].active && !state.nodes[1].completed, "Ambient loop completed itself");
	later.send(1, Input::Stop);
	check(!state.nodes[1].active, "Stop did not stop background motion");
	later.send(1, Input::Activate);
	check(state.nodes[1].active && state.nodes[1].changedAt == 1200, "Ambient restart lost phase");
	later.send(1, Input::Disable);
	check(!state.nodes[1].active && !state.nodes[1].enabled, "Disable did not stop machinery");
	return p;
}
static void ambientSceneChecks(const std::filesystem::path &root, const assets::Source &source) {
	using namespace editor;
	struct AmbientSystem : HeadlessSystem {
		bool pause = false;
		bool consumePauseRequest() override {
			const bool result = pause;
			pause = false;
			return result;
		}
	} stub;
	auto p = ambientChecks(root);
	for (int style = 0; style < 8; ++style) {
		p.modules[0].nodes[1].ambientStyle = style;
		stub._pi = {};
		Scene scene(p, root, source, &stub);
		for (int i = 0; i < 10; ++i)
			scene.tick();
		const auto saved = scene.saveSession();
		const auto collision = scene.game()._res._ctData[256];
		const auto before = scene.render();
		for (int i = 0; i < 30; ++i)
			scene.tick();
		check(scene.render().pixels != before.pixels, "Scene did not animate background machinery");
		scene.restoreSession(saved);
		check(scene.render().pixels == before.pixels, "Save/load reset machine animation phase");
		stub.pause = true;
		for (int i = 0; i < 30; ++i)
			scene.tick();
		const auto paused = scene.render();
		for (int y = 32; y < 144; ++y)
			for (int x = 64; x < 192; ++x)
				check(before.pixels[y * 256 + x] == paused.pixels[y * 256 + x],
				      "Pause did not freeze machine motion");
		check(scene.saveSession() == saved && scene.game()._res._ctData[256] == collision,
		      "Ambient renderer changed simulation or collision state");
	}
	// Player is painted after the background; foreground art occludes it too.
	p.rooms[0].art[2] = p.rooms[0].art[0];
	Scene covered(p, root, source, &stub);
	for (int i = 0; i < 30; ++i)
		covered.tick();
	check(covered.render().pixels == readImage(root / "ambient-background.png").pixels,
	      "Background effect leaked over foreground art");
}
