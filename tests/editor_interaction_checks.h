// SPDX-License-Identifier: GPL-3.0-or-later
// Adjacent controls must stay individually usable regardless of module order.
static void interactionSceneChecks(const std::filesystem::path &root, const assets::Source &source) {
	using namespace editor;
	auto p = makeProject();
	p.spawn = {p.rooms[0].id, 136, 214};
	auto m = makeGraph(p, 5, p.spawn);
	m.nodes.clear();
	m.wires.clear();
	for (int x : {112, 136, 136, 136}) {
		auto n = makeNode(m, NodeKind::Button, p.spawn);
		n.at = {p.spawn.room, x, 216};
		n.width = 16;
		n.name = "Control " + std::to_string(m.nodes.size());
		m.nodes.push_back(n);
	}
	m.nodes[1].once = true;
	m.nodes[2].enabled = false;
	m.nodes[3].at.y = 144;
	p.modules = {m};
	HeadlessSystem stub;
	Scene scene(p, root, source, &stub);
	for (int i = 0; i < 20; ++i) scene.tick();
	const auto hinted = scene.render();
	stub._pi.enter = true;
	scene.tick();
	check(!scene.states()[0].graph.nodes[0].completed && scene.states()[0].graph.nodes[1].completed &&
	          !scene.states()[0].graph.nodes[2].completed && !scene.states()[0].graph.nodes[3].completed,
	      "Nearest control lost to earlier, disabled or overhead control");
	for (int i = 0; i < 10; ++i) scene.tick();
	check(!scene.states()[0].graph.nodes[0].completed, "Held Enter activated a second nearby control");
	stub._pi.enter = false; scene.tick();
	stub._pi.enter = true; scene.tick();
	check(scene.states()[0].graph.nodes[0].completed, "Exhausted one-shot control blocked its neighbor");
	for (auto &n : p.modules[0].nodes) n.enabled = false;
	stub._pi = {};
	Scene empty(p, root, source, &stub);
	const auto unhinted = empty.render();
	check(!std::equal(hinted.pixels.begin(), hinted.pixels.begin() + 256 * 24, unhinted.pixels.begin()),
	      "Nearby control did not show an interaction prompt");
}
