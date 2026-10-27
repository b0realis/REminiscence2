// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor/campaign.h"
#include "editor/graph_runtime.h"
static void campaignChecks(const std::filesystem::path &root) {
	using namespace editor;
	auto p = makeProject();
	auto m = makeGraph(p, 5, p.spawn);
	auto add = [&](NodeKind kind) {
		m.nodes.push_back(makeNode(m, kind, p.spawn));
		return int(m.nodes.size()) - 1;
	};
	const int item = add(NodeKind::Item), condition = add(NodeKind::ItemCondition),
	          counter = add(NodeKind::Counter), exit = add(NodeKind::LevelExit),
	          start = add(NodeKind::Start), damage = add(NodeKind::Damage),
	          health = add(NodeKind::HealthPickup);
	m.nodes[condition].requiredItem = m.nodes[item].id;
	m.nodes[condition].consumeItem = true;
	m.nodes[counter].goal = 2;
	m.nodes[exit].text = "secret";
	m.comments = "Designer module notes";
	m.nodes[item].comments = "Pickup explanation";
	m.wires = {{m.nodes[condition].id, m.nodes[counter].id, Signal::Choice1, Input::Progress},
	           {m.nodes[counter].id, m.nodes[exit].id, Signal::Completed, Input::Activate}};
	auto state = initialGraphState(m);
	GraphMachine machine(m, state);
	machine.start();
	check(state.nodes[start].completed, "On start did not fire");
	const auto seq = state.sequence;
	machine.start();
	check(seq == state.sequence, "On start fired twice");
	machine.send(condition, Input::Activate);
	check(!state.nodes[condition].active, "Missing item condition passed");
	machine.send(item, Input::Activate);
	machine.send(condition, Input::Activate);
	check(state.nodes[counter].count == 1 && !state.nodes[item].collected,
	      "Item test did not consume and signal");
	machine.send(counter, Input::Progress);
	check(state.nodes[exit].completed && machine.worldCommands.back() == exit,
	      "Counter did not finish named exit");
	machine.send(counter, Input::Reset);
	check(state.nodes[counter].count == 0 && !state.nodes[counter].completed,
	      "Counter reset failed");
	machine.send(damage, Input::Activate);
	machine.send(health, Input::Activate);
	const auto size = machine.worldCommands.size();
	machine.send(health, Input::Activate);
	check(machine.worldCommands.size() == size, "Health pickup repeated without reset");
	machine.send(health, Input::Reset);
	machine.send(health, Input::Activate);
	check(machine.worldCommands.size() == size + 1, "Health pickup failed to rearm");
	const auto revision = moduleRevision(m);
	m.comments += " changed";
	m.nodes[item].comments += " changed";
	check(moduleRevision(m) == revision, "Comments changed graph runtime identity");
	p.modules.push_back(m);
	const auto round = deserialize(serialize(p));
	check(round.schema == 9 && round.modules[0].comments == m.comments &&
	          round.modules[0].nodes[item].comments == m.nodes[item].comments,
	      "Schema 7 lost designer comments");
	for (int k = 0; k < int(NodeKind::Count); ++k)
		check(std::string(nodeHelp(NodeKind(k))).size() > 30, "Missing node help");
	const auto folder = root / "campaign";
	std::filesystem::create_directories(folder);
	saveProject(p, folder / "level.r2project.json");
	Campaign c;
	auto level = newStage(c, StageKind::Level);
	level.level = "level.r2project.json";
	c.entry = level.id;
	c.stages.push_back(level);
	auto story = newStage(c, StageKind::Story);
	c.stages.push_back(story);
	auto end = newStage(c, StageKind::End);
	c.stages.push_back(end);
	c.stages[0].routes = {{"secret", "Dock", story.id}, {"secret", "Transit", end.id}};
	c.stages[1].routes = {{"complete", "Continue", end.id}};
	check(validateCampaign(c, folder, true).empty(), "Valid campaign failed preflight");
	check(campaignRoutes(c.stages[0], "secret").size() == 2 &&
	          campaignRoutes(c.stages[0], "bad").empty(),
	      "Named fork routing failed");
	saveCampaign(c, folder / "campaign.r2campaign.json");
	check(serializeCampaign(loadCampaign(folder / "campaign.r2campaign.json")) ==
	          serializeCampaign(c),
	      "Campaign roundtrip lost fields");
	auto multipart = c;
	multipart.stages[0].kind = StageKind::Movie;
	multipart.stages[0].movie = "intro:1";
	multipart.stages[0].routes = {{"complete", "Continue", end.id}};
	check(validateCampaign(multipart).empty(), "Multipart movie rejected as a path");
	auto bad = c;
	bad.stages[0].level = "../escape.json";
	check(!validateCampaign(bad, folder, true).empty(), "Campaign traversal accepted");
	bad = c;
	bad.stages[0].routes[0].to = "missing";
	check(!validateCampaign(bad).empty(), "Dangling route accepted");
	bad = c;
	bad.stages[0].routes[0].condition = "not a valid key";
	check(!validateCampaign(bad).empty(), "Invalid conditional route key accepted");
	bad = c;
	bad.stages[0].routes.clear();
	check(!validateCampaign(bad).empty(), "Unconnected exit accepted");
	bad = c;
	bad.stages[0].routes[0].key = "other";
	bad.stages[0].routes.erase(bad.stages[0].routes.begin() + 1);
	check(!validateCampaign(bad, folder, true).empty(), "Actual level exit wasn't checked");
	bad = c;
	bad.stages[1].elements[0].width = 1000;
	check(!validateCampaign(bad).empty(), "Offscreen story geometry accepted");
	bad = c;
	bad.stages[1].routes[0].to = c.entry;
	check(validateCampaign(bad).empty(), "Intentional nonlinear loop rejected");
	rejects([&] { deserializeCampaign("{\"schema\":1.5}"); },
	        "Fractional campaign schema accepted");
	rejects([&] { deserializeCampaign("{\"schema\":18446744073709551615}"); },
	        "Overflowing campaign integer accepted");
	const auto imported = importCampaignLevel(folder / "level.r2project.json", folder);
	check(loadProject(assetPath(folder, imported)).modules[0].comments == m.comments,
	      "Imported level lost nodes/notes");
	Images images;
	images.root = folder;
	auto screen = c.stages[1];
	screen.elements[0].effect = StoryEffect::Typewriter;
	screen.elements[0].text = "Welcome to the station.";
	screen.elements[0].rate = 12;
	const auto blank = renderStory(screen, images, 0), typed = renderStory(screen, images, 5000);
	check(blank.pixels != typed.pixels, "Story typewriter drew no text");
	check(typed.pixels == renderStory(screen, images, 0, true).pixels,
	      "Reveal did not display final text");
	screen.elements[0].effect = StoryEffect::Fade;
	screen.elements[0].delay = 200;
	check(renderStory(screen, images, 199).pixels == blank.pixels, "Delayed text appeared early");
	check(renderStory(screen, images, 700).pixels != renderStory(screen, images, 1200).pixels,
	      "Fade did not animate");
	writePNG(typed, folder / "story.png");
	StoryElement photo;
	photo.kind = StoryKind::Photo;
	photo.file = "story.png";
	screen.elements = {photo};
	check(renderStory(screen, images, 0).pixels != blank.pixels, "Photo compositor failed");
	screen.elements[0].color[3] = 0;
	check(renderStory(screen, images, 0).pixels == blank.pixels, "Photo ignored its opacity");
	// Cinematic motion uses elapsed time even when captions are revealed. Offscreen
	// travel must clip safely, and the last frame must fade every layer to black.
	screen.elements[0] = photo;
	screen.elements[0].width = 120;
	screen.elements[0].height = 80;
	screen.elements[0].moveX = -240;
	screen.elements[0].moveDuration = 1000;
	check(renderStory(screen, images, 0, true).pixels != renderStory(screen, images, 500, true).pixels,
	      "Reveal froze photo motion");
	check(renderStory(screen, images, 1000).pixels == blank.pixels, "Offscreen photo did not clip");
	screen.elements[0].moveX = 80;
	screen.advance = 2000;
	screen.fadeOut = 1000;
	const auto black = renderStory(screen, images, 2000, true);
	check(std::all_of(black.pixels.begin(), black.pixels.end(),
	                  [](auto pixel) { return pixel == rgba(0, 0, 0); }), "Final fade left visible pixels");
	c.stages[1] = screen;
	const auto motion = deserializeCampaign(serializeCampaign(c));
	check(motion.stages[1].fadeOut == 1000 && motion.stages[1].elements[0].moveX == 80,
	      "Campaign lost motion/fade settings");
	c.stages[1].fadeOut = 2001;
	rejects([&] { deserializeCampaign(serializeCampaign(c)); }, "Accepted fade longer than screen");
	c.stages[1].fadeOut = 0;
	c.stages[1].elements[0].moveDuration = 0;
	rejects([&] { deserializeCampaign(serializeCampaign(c)); }, "Accepted zero travel duration");
	c.stages[1].elements[0].moveDuration = 1;
	c.stages[1].elements[0].moveX = 1281;
	rejects([&] { deserializeCampaign(serializeCampaign(c)); }, "Accepted unbounded travel");
}
static void campaignSceneChecks(const std::filesystem::path &root, const assets::Source &source,
                                SystemStub &stub) {
	using namespace editor;
	auto p = makeProject();
	auto m = makeGraph(p, 5, p.spawn);
	for (auto k : {NodeKind::Start, NodeKind::Damage, NodeKind::HealthPickup, NodeKind::Teleport,
	               NodeKind::Checkpoint, NodeKind::LevelExit})
		m.nodes.push_back(makeNode(m, k, p.spawn));
	m.nodes[1].health = 3;
	m.nodes[2].health = 2;
	m.nodes[3].at = {p.spawn.room, 160, 216};
	m.nodes[5].text = "secret";
	for (int i = 0; i < 5; ++i)
		m.wires.push_back({m.nodes[i].id, m.nodes[i + 1].id, Signal::Completed, Input::Activate});
	p.modules = {m};
	check(!listCampaignMovies(source).empty(), "Movie picker catalog is empty");
	Scene scene(p, root, source, &stub);
	scene.tick();
	check(scene.game()._pgeLive[0].life == 4, "Health/damage nodes did not reach native player");
	check(scene.game()._pgeLive[0].pos_x == 160, "Teleport did not move Conrad to destination");
	check(scene.exitRoute() == "secret", "Scene lost named campaign exit");
	scene.restart();
	check(scene.game()._pgeLive[0].pos_x == 160 && scene.exitRoute().empty(),
	      "Checkpoint restart failed to restore position/reset exit");
	auto delayed = makeProject();
	auto timing = makeGraph(delayed, 5, delayed.spawn);
	for (auto k : {NodeKind::Start, NodeKind::Timer, NodeKind::Checkpoint})
		timing.nodes.push_back(makeNode(timing, k, delayed.spawn));
	timing.nodes[1].duration = 1;
	timing.wires = {{timing.nodes[0].id, timing.nodes[1].id, Signal::Completed, Input::Activate},
	                {timing.nodes[1].id, timing.nodes[2].id, Signal::Completed, Input::Activate}};
	delayed.modules = {timing};
	Scene falling(delayed, root, source, &stub);
	falling.tick();
	falling.game()._pgeLive[0].room_location = 255;
	falling.tick();
	check(falling.dead(), "Delayed checkpoint resurrected an invalid-room player");
}
