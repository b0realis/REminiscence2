// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor/playtest.h"
#include "editor/settings.h"
static void editingChecks(const std::filesystem::path &root) {
	using namespace editor;
	{
		check(!loadEditorSettings(root / "missing-preferences").authoredStart,
		      "Missing settings should default to current-room tests");
		const auto folder = root / "settings-regression";
		saveEditorSettings(folder, {true, false});
		const auto saved = loadEditorSettings(folder);
		check(saved.authoredStart && !saved.nativeMarkers, "Settings did not round-trip");
		for (const auto &bad : std::vector<std::string>{
		         "{}", "{\"schema\":1.0,\"authoredStart\":false,\"nativeMarkers\":true}",
		         "{\"schema\":1,\"authoredStart\":1,\"nativeMarkers\":true}",
		         "{\"schema\":2,\"authoredStart\":false,\"nativeMarkers\":true}", "[[[[[[]]]]]]",
		         std::string(2048, ' ')}) {
			std::ofstream(folder / ".editor-settings.json") << bad;
			rejects([&] { loadEditorSettings(folder); }, "Malformed settings accepted");
		}
		saveEditorSettings(folder, {});
		check(loadEditorSettings(folder).nativeMarkers, "Could not repair malformed settings");
	}
	{
		auto project = makeProject();
		const auto room = addRoom(project, 1, 0).id;
		const auto unchanged = serialize(project);
		const auto start = roomPlaytestStart(project, room);
		check(start.at.room == room && start.at.y == 214 &&
		          clearPlaytestStart(project, start.at, true),
		      "Room playtest chose the campaign start instead of current room");
		check(serialize(project) == unchanged,
		      "Choosing a test start changed the authored campaign");
		project.modules.push_back(makeModule(project, Kind::Door, {room, 32, 216}));
		const auto besideDoor = roomPlaytestStart(project, room);
		check(besideDoor.at.x + 16 <= 32 || besideDoor.at.x >= 48,
		      "Automatic test start overlaps a closed door");
		rejects([&] { validatePlaytestStart(project, {{room, 32, 214}, false}); },
		        "Blocked TEST marker accepted");
		rejects([&] { validatePlaytestStart(project, {{room, 999, 214}, false}); },
		        "Out-of-bounds TEST marker accepted");
	}
	auto p = makeProject();
	const auto room = p.rooms[0].id;
	p.modules.push_back(makeGraph(p, 0, {room, 128, 216}));
	p.modules.push_back(makeModule(p, Kind::Door, {room, 192, 216}));
	p.modules.push_back(makeModule(p, Kind::Door, {room, 80, 144}));
	p.modules[0].outputs = {p.modules[1].id, p.modules[2].id};
	const std::vector<std::string> selected{p.modules[0].id, p.modules[1].id};
	const auto copied = copyModules(p, selected, root, room);
	const auto original = serialize(p);
	const auto ids = pasteModules(p, copied, root, room);
	check(ids.size() == 2 && ids[0] != ids[1] && ids[0] != selected[0], "Paste reused module IDs");
	check(p.modules[3].outputs == std::vector<std::string>{ids[1]},
	      "Paste must remap internal chains and discard external outputs");
	check(p.modules[3].wires.size() == copied.modules[0].wires.size() &&
	          p.modules[3].nodes[0].id == copied.modules[0].nodes[0].id,
	      "Paste broke scoped graph references");
	check(p.modules[0].nodes[0].at.x == copied.modules[0].nodes[0].at.x,
	      "Paste mutated its source module");
	const int gap = p.modules[4].a.x - p.modules[3].nodes[0].at.x;
	translateModules(p, ids, 999, -999);
	check(p.modules[4].a.x - p.modules[3].nodes[0].at.x == gap,
	      "Clamped group move changed relative positions");
	compileCollision(p);
	// Whole graph and group coordinates move together, with door grid constraints.
	for (int m : {3, 4}) {
		const auto &a = p.modules[m].a;
		check(a.x % 16 == 0 && a.y % 72 == 0, "Group move broke native door alignment");
	}
	auto destination = makeProject();
	writePNG({1, 1, {rgba(20, 50, 90)}}, root / "clipboard.png");
	auto withArt = copied;
	withArt.modules[0].nodes[0].visuals[0].art = {"clipboard.png", 0, 0, 1, 1};
	const auto otherRoot = root / "clipboard-destination";
	std::filesystem::create_directories(otherRoot);
	pasteModules(destination, withArt, otherRoot, destination.rooms[0].id);
	Images images;
	images.root = otherRoot;
	validateImages(destination, images);
	check(destination.modules[0].nodes[0].at.room == destination.rooms[0].id &&
	          readImage(assetPath(otherRoot, destination.modules[0].nodes[0].visuals[0].art.file))
	                  .pixels[0] == rgba(20, 50, 90),
	      "Cross-project paste lost room mapping or artwork");
	auto bad = copied;
	bad.modules[0].nodes[0].at.room = "missing-remote-room";
	const auto before = serialize(destination);
	rejects([&] { pasteModules(destination, bad, root, destination.rooms[0].id); },
	        "Dangling remote room accepted by clipboard");
	check(serialize(destination) == before, "Failed paste partially changed the document");
	auto broken = deserialize(original);
	broken.modules[0].nodes[0].at.room = "missing-room";
	const auto issues = validate(broken);
	check(std::any_of(issues.begin(), issues.end(),
	                  [&](auto &issue) {
		                  return issue.error && issue.module == broken.modules[0].id &&
						         issue.node == broken.modules[0].nodes[0].id;
	                  }),
	      "Graph validation lost clickable module/node location");
	broken = deserialize(original);
	broken.rooms[0].art[2].file = "missing.png";
	Images missing;
	missing.root = root;
	const auto artwork = imageIssues(broken, missing);
	check(artwork.size() == 1 && artwork[0].room == room && artwork[0].layer == 2,
	      "Missing artwork validation lost room/layer location");
}
