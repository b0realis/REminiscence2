// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor/native.h"
#include "editor/original.h"
#include "editor/runtime.h"
#include "editor/campaign_progress.h"
#include "editor/native_logic.h"
#include "config.h"
#include "game.h"
#include "fs.h"
#include "file.h"
#include "headless_system.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <fstream>
Options g_options;
const char *g_caption = "Native campaign editor tests";
static void check(bool ok, const char *why) {
	if (!ok)
		throw std::runtime_error(why);
}
template <class F> static void rejects(F f, const char *why) {
	try {
		f();
	} catch (const std::exception &) {
		return;
	}
	throw std::runtime_error(why);
}
static std::vector<uint8_t> state(Game &g) {
	File f;
	f.openMemoryBuffer(32768);
	g.saveState(&f);
	f.seek(0);
	std::vector<uint8_t> b(f.size());
	f.read(b.data(), b.size());
	return b;
}
#include "editor_native_module_checks.h"
struct RecordingSystem : HeadlessSystem {
	std::vector<uint8_t> pixels;
	std::function<void()> events;
	void copyRectRgb24(int, int, int w, int h, const uint8_t *rgb) override {
		pixels.assign(rgb, rgb + w * h * 3);
	}
	void processEvents() override {
		if (events) {
			auto callback = std::move(events);
			events = {};
			callback();
		}
	}
};
int main(int argc, char **argv) {
	try {
		using namespace editor;
		namespace fs = std::filesystem;
		if (argc != 3)
			return 2;
		setDefaultOptions(g_options);
		const fs::path root = fs::absolute(argv[1]);
		fs::create_directories(root);
		const auto library = assets::discover(argv[2]);
		check(!library.games.empty(), "Missing native fixture assets");
		const auto source = library.games.front();
		HeadlessSystem viewer;
		for (int level = 0; level < 7; ++level) {
			OriginalLevel original(source, &viewer, level);
			auto p = original.createProject(true);
			check(p.native.enabled && p.native.level == level && p.native.actors.size() >= 90,
			      "Incomplete original actor table");
			const auto file = root / ("level-" + std::to_string(level + 1) + ".r2project.json");
			saveProject(p, file);
			check(serialize(loadProject(file)) == serialize(p), "Native project round trip");
			// Exercise sprite banks and preview reconstruction beyond just level one.
			check(original.actorOverlay(p, original.startRoom()).pixels.size() == 256 * 224,
			      "Native actor preview failed");
		}
		for (int level = 0; level < 7; ++level) {
			const auto file = root / ("level-" + std::to_string(level + 1) + ".r2project.json");
			auto p = loadProject(file);
			const int copyableGroup = nativeModuleChecks(p);
			RecordingSystem liveStub;
			HeadlessSystem baselineStub;
			FileSystem files(source.root.c_str(), source.excludedRoots);
			const auto control = root / ("control-" + std::to_string(level) + ".json");
			NativeSession session(file, source, control);
			std::unique_ptr<Game, GameDeleter> live(new Game(&liveStub, &files, &files, "", level,
			                                                 ResourceType(source.edition), LANG_EN,
			                                                 kWidescreenNone, false, 0));
			std::unique_ptr<Game, GameDeleter> baseline(
			    new Game(&baselineStub, &files, &files, "", level, ResourceType(source.edition),
				         LANG_EN, kWidescreenNone, false, 0));
			live->_authoring = &session;
			initializeOriginalLevel(*live);
			initializeOriginalLevel(*baseline);
			check(state(*live) == state(*baseline),
			      "Unmodified native project changed initial campaign state");
			{
				Scene snapshotScene(*live, p, root);
				const auto snapshot = snapshotScene.saveSession();
				live->_pgeLive[0].life = 1;
				snapshotScene.restoreSession(snapshot);
				check(state(*live) == state(*baseline),
				      "Native campaign snapshot changed initial actor state");
				CampaignRun run;
				run.campaign.stages = {Stage{}};
				run.campaign.stages[0].id = "level";
				run.campaign.entry = "level";
				run.progress.stage = "level";
				run.progress.identity = "native-test";
				run.progress.edition = source.edition;
				run.path = root / ("campaign-progress-" + std::to_string(level) + ".json");
				for (int id = 1; id < live->_res._pgeNum; ++id) {
					const auto &a = live->_res._pgeInit[id];
					if (a.icon_num == 0 || a.object_type == 1 || a.object_type == 10)
						continue;
					int matches = 0;
					for (int j = 1; j < live->_res._pgeNum; ++j) {
						const auto &b = live->_res._pgeInit[j];
						if (a.type == b.type && a.obj_node_number == b.obj_node_number &&
						    a.object_type == b.object_type && a.icon_num == b.icon_num)
							++matches;
					}
					if (matches != 1)
						continue;
					run.progress.inventory.push_back(
					    {a.type, a.obj_node_number, a.object_type, a.icon_num, int16_t(a.life)});
					run.enter(snapshotScene);
					check(live->_pgeLive[id].ref_inventory_PGE == 0,
					      "Carried native item was not remapped to destination slot");
					const auto carried = snapshotScene.saveSession();
					snapshotScene.restoreSession(carried);
					check(live->_pgeLive[id].ref_inventory_PGE == 0,
					      "Native inventory transfer created invalid snapshot links");
					break;
				}
				snapshotScene.restoreSession(snapshot);
			}
			int parityTicks = 0;
			// Deterministic movement/combat probes, each from the original spawn.
			// Compare the complete native state with/without authoring hooks. This
			// exercises every chapter without pretending to solve its campaign.
			for (int style = 0; style < 3; ++style) {
				if (style) {
					const auto *sprites = live->_res._spr1;
					const auto *font = live->_res._fnt;
					const auto *sounds = live->_res._sfxList;
					const int icons = live->_res._icnLen;
					live->_currentLevel = baseline->_currentLevel = level;
					resetOriginalLevel(*live);
					resetOriginalLevel(*baseline);
					check((live->_res.isSegaCD() || live->_res._spr1 == sprites) &&
					          live->_res._fnt == font && live->_res._icnLen == icons &&
					          (!live->_res.isDOS() || live->_res._sfxList == sounds),
					      "Native reset must retain global graphics and samples");
				}
				for (int t = 0; t < 600; ++t) {
					PlayerInput input{};
					const int phase = t / 30;
					if (t >= 20)
						input.dirMask =
						    ((phase + style) & 1) ? PlayerInput::DIR_LEFT : PlayerInput::DIR_RIGHT;
					input.shift = style == 1 || (style == 2 && phase % 3 == 0);
					if (style == 1 && phase % 3 == 0)
						input.dirMask |= PlayerInput::DIR_UP;
					input.space = style == 2 && t % 90 == 20;
					liveStub._pi = baselineStub._pi = input;
					live->pge_getInput();
					baseline->pge_getInput();
					live->simulateActors();
					baseline->simulateActors();
					++parityTicks;
					check(state(*live) == state(*baseline),
					      "Native editor changed campaign actor/script simulation");
					if (live->_currentLevel != level || live->_pgeLive[0].life <= 0 ||
					    live->_pgeLive[0].room_location >= 64)
						break;
				}
			}
			liveStub._pi = baselineStub._pi = {};
			// Keep this transaction test independent of scripted level transitions.
			live->_currentLevel = level;
			live->loadLevelData();
			live->resetGameState();
			live->loadLevelMap();
			auto malformed = p;
			malformed.native.actors[0].data[0] ^= 1;
			auto unchanged = state(*live);
			rejects([&] { session.apply(*live, malformed, false); },
			        "Script dependency mutation accepted");
			check(state(*live) == unchanged, "Rejected dependency edit changed live state");
			auto moved = p;
			moved.native.actors[0].x = std::min(248, moved.native.actors[0].x + 8);
			moved.spawn.x = moved.native.actors[0].x;
			rejects([&] { session.apply(*live, moved, false); },
			        "Actor relocation applied without restart");
			check(state(*live) == unchanged, "Rejected actor edit mutated running game");
			// Choose a cell in a room with no active initial actor, and preserve a
			// separate simulated puzzle mutation during the authored merge.
			int cell = -1;
			for (int r = 63; r >= 0 && cell < 0; --r) {
				bool occupied = false;
				for (int a = 0; a < live->_res._pgeNum; ++a)
					occupied |= live->_pgeLive[a].room_location == r;
				if (!occupied)
					cell = r * 112;
			}
			check(cell >= 0, "Fixture needs an unused slot for merge checks");
			auto changed = p;
			changed.rooms[cell / 112].collision[cell % 112] =
			    p.rooms[cell / 112].collision[cell % 112] ? 0 : 1;
			live->_res._ctData[256 + cell + 1] = 2;
			const auto actorsBefore =
			    std::vector<LivePGE>(live->_pgeLive, live->_pgeLive + live->_res._pgeNum);
			session.apply(*live, changed, false);
			check(live->_res._ctData[256 + cell] ==
			              changed.rooms[cell / 112].collision[cell % 112] &&
			          live->_res._ctData[256 + cell + 1] == 2,
			      "Live merge lost unrelated native puzzle collision");
			check(std::memcmp(actorsBefore.data(), live->_pgeLive,
			                  actorsBefore.size() * sizeof(LivePGE)) == 0,
			      "Live geometry apply changed actor pointers, inventory or script state");
			auto conflict = changed;
			conflict.rooms[cell / 112].collision[cell % 112] = 2;
			live->_res._ctData[256 + cell] =
			    changed.rooms[cell / 112].collision[cell % 112] ? 0 : 1;
			unchanged = state(*live);
			rejects([&] { session.apply(*live, conflict, false); },
			        "Conflicting script collision was overwritten");
			check(state(*live) == unchanged, "Failed merge partially applied");
			session.apply(*live, moved, true);
			check(live->_pgeLive[0].pos_x == moved.spawn.x &&
			          live->_res._pgeNum == int(p.native.actors.size()),
			      "Restart lost native actors or authored placement");
			if (copyableGroup >= 0) {
				auto removed = moved;
				const auto ids = nativePuzzleClosure(removed.native, copyableGroup);
				for (int id : ids)
					removed.native.actors[id].removed = true;
				rejects([&] { session.apply(*live, removed, false); },
				        "Native deletion was applied live without restart");
				session.apply(*live, removed, true);
				for (int id : ids)
					check(live->_pgeLive[id].room_location == 255 &&
					          !session.beforeActor(*live, live->_pgeLive[id]),
					      "Removed native actor remained active/visible");
				session.apply(*live, moved, true);
			}
			session.campaignStage = true;
			check(session.levelExit(*live, level + 1) && session.exitRoute == "complete",
			      "Native campaign transition did not produce route");
			session.campaignStage = false;
			session.exitRoute.clear();
			check(!session.levelExit(*live, level + 1),
			      "Standalone native transition was intercepted");
			// Render an authored background while keeping native inventory/HUD pixels.
			const int room = live->_currentRoom;
			auto art = moved;
			writePNG({256, 224, std::vector<uint32_t>(256 * 224, rgba(12, 34, 56))},
			         root / "test-art.png");
			art.rooms[room].art[0] = {"test-art.png", 0, 0, 256, 224};
			session.apply(*live, art, false);
			std::copy_n(live->_vid._backLayer, 256 * 224, live->_vid._frontLayer);
			session.beforeDraw(*live);
			live->prepareAnims();
			live->drawAnims();
			session.afterActors(*live);
			live->_vid._frontLayer[0] = 0xA1;
			session.afterDraw(*live);
			check(session.present(*live), "Authored native art was not presented");
			check(liveStub.pixels.size() == 256 * 224 * 3 &&
			          liveStub.pixels[0] == liveStub.palette[0xA1].r &&
			          liveStub.pixels[1] == liveStub.palette[0xA1].g &&
			          liveStub.pixels[2] == liveStub.palette[0xA1].b,
			      "Authored foreground covered native HUD pixels");
			NativeSession::publish(control, "pause", art);
			liveStub.events = [&] { NativeSession::publish(control, "resume", art); };
			session.boundary(*live);
			check(!session.paused() && !session.present(*live),
			      "Paused art compositor leaked into resumed cinematics");
			OriginalLevel reference(source, &viewer, level);
			std::optional<PlaytestStart> direct;
			for (int r = 0; r < 64 && !direct; ++r) {
				if (r == roomIndex(p, p.spawn.room) || !reference.hasMap(r))
					continue;
				try {
					direct = roomPlaytestStart(p, p.rooms[r].id);
				} catch (const std::exception &) {
				}
			}
			check(bool(direct), "No alternate playable native room");
			const auto sourceDocument = serialize(p);
			session.playHere(*live, p, *direct);
			check(!session.paused() && live->_currentRoom == roomIndex(p, direct->at.room) &&
			          live->_pgeLive[0].pos_x == direct->at.x &&
			          live->_pgeLive[0].pos_y == direct->at.y && live->_pgeLive[0].obj_type == 1 &&
			          live->_cut._id == 0xFFFF,
			      "Play here did not start immediately at the exact alternate room marker");
			check(serialize(session.project()) == sourceDocument &&
			          serialize(p) == sourceDocument &&
			          live->_res._pgeNum == int(p.native.actors.size()) &&
			          live->_pgeLive[0].current_inventory_PGE == p.native.actors[0].data[0],
			      "Direct start changed authored spawn or lost native actors/pistol dependency");
			for (size_t a = 1; a < p.native.actors.size(); ++a)
				check(live->_res._pgeInit[a].obj_node_number == p.native.actors[a].behavior,
				      "Direct start replaced original enemy/puzzle scripts");
			const auto directState = state(*live);
			auto invalid = *direct;
			invalid.at.x = 999;
			rejects([&] { session.playHere(*live, p, invalid); }, "Invalid direct start accepted");
			check(state(*live) == directState, "Rejected direct start changed live game");
			for (int t = 0; t < 10; ++t) {
				live->pge_getInput();
				live->simulateActors();
			}
			session.playHere(*live, p, *direct);
			check(state(*live) == directState,
			      "Repeated Play here did not restart deterministically");
			// Native rows remain editable and disabled rules never execute.
			auto scripted = p;
			const int behavior = scripted.native.actors[0].behavior;
			auto &rule = scripted.native.programs[behavior].rules[0];
			rule.opcode = {0, 0, 0x55};
			rule.argument = {0, 0, 7};
			rule.flags = 0;
			rule.dx = rule.dy = 0;
			rule.enabled = false;
			session.apply(*live, scripted, true);
			const auto *compiled = live->_res._objectNodesMap[behavior]->objects;
			const int priorHealth = live->_pgeLive[0].life;
			check(live->pge_execute(&live->_pgeLive[0], &live->_res._pgeInit[0], compiled) == 0 &&
			          live->_pgeLive[0].life == priorHealth,
			      "Disabled native rule executed");
			rule.enabled = true;
			session.apply(*live, scripted, true);
			compiled = live->_res._objectNodesMap[behavior]->objects;
			check(live->pge_execute(&live->_pgeLive[0], &live->_res._pgeInit[0], compiled) ==
			              0xFFFF &&
			          live->_pgeLive[0].life == 7,
			      "Native action edit did not execute");
			auto unsafe = scripted;
			unsafe.native.programs[behavior].rules[0].next = 65535;
			rejects([&] { session.apply(*live, unsafe, true); },
			        "Out-of-range native transition accepted");
			auto legacyUnsafe = p;
			legacyUnsafe.native.programs.clear();
			legacyUnsafe.native.actors[0].type = 65535;
			rejects([&] { session.apply(*live, legacyUnsafe, true); },
			        "Legacy native project bypassed initial state validation");
			// An authored initial state opens dialogue in the original world. It
			// pauses native simulation and sends a native message after dismissal.
			auto extended = p;
			auto module = makeGraph(extended, 5, p.spawn);
			module.nodes.push_back(makeNode(module, NodeKind::State, p.spawn));
			module.nodes[0].initiallyActive = true;
			module.nodes.push_back(makeNode(module, NodeKind::Dialogue, p.spawn));
			module.nodes[1].text = "Native extension test";
			module.nodes.push_back(makeNode(module, NodeKind::Actor, p.spawn));
			module.nodes[2].nativeActor = 1;
			module.nodes[2].friendly = true;
			module.nodes[2].message = 42;
			module.nodes[2].messageFrom = int(p.native.actors.size()) - 1;
			module.wires = {
			    {module.nodes[0].id, module.nodes[1].id, Signal::Entered, Input::Activate},
			    {module.nodes[1].id, module.nodes[2].id, Signal::Completed, Input::Activate}};
			module.wires.push_back(
			    {module.nodes[2].id, module.nodes[1].id, Signal::Used, Input::Activate});
			extended.modules = {module};
			session.apply(*live, extended, true);
			liveStub._pi = {};
			check(!session.beforeSimulation(*live),
			      "Native extension dialogue did not pause world");
			live->_pgeLive[1].flags |= 4;
			liveStub._pi.enter = true;
			check(!session.beforeSimulation(*live),
			      "Dialogue dismissal leaked Enter into native simulation");
			check(live->_pge_messagesTable[1] && live->_pge_messagesTable[1]->msg_num == 42,
			      "Authored graph did not send native actor message");
			liveStub._pi = {};
			check(session.beforeSimulation(*live),
			      "Native extension failed to resume after dialogue");
			session.beforeDraw(*live);
			live->prepareAnims();
			live->drawAnims();
			session.afterActors(*live);
			session.afterDraw(*live);
			check(session.present(*live),
			      "Native extension overlay without custom room art was not presented");
			NativeSession::publish(control, "resume", extended);
			session.boundary(*live);
			const auto observed = NativeSession::readGraphs(control);
			check(observed.count(module.id) &&
			          observed.at(module.id).revision == moduleRevision(module) &&
			          observed.at(module.id).nodes[1].completed,
			      "Native graph monitor lost its revision/state snapshot");
			// Noncombat NPCs use life as a counter; zero is not necessarily death.
			auto &npc = live->_pgeLive[1];
			const int oldRoom = npc.room_location;
			live->_res._pgeInit[1].object_type = 0;
			npc.life = 0;
			npc.room_location = live->_pgeLive[0].room_location;
			npc.pos_x = live->_pgeLive[0].pos_x;
			npc.pos_y = live->_pgeLive[0].pos_y;
			live->pge_addToCurrentRoomList(&npc, oldRoom);
			liveStub._pi.enter = true;
			check(!session.beforeSimulation(*live),
			      "Zero-counter friendly native NPC could not start dialogue");
			liveStub._pi = {};
			session.playHere(*live, p, *direct);
			nativeModuleRuntimeChecks(session, *live, p, copyableGroup, source, control);
			session.playHere(*live, p, *direct);
			// Fixture for the actual-process command regression, preserving the authored spawn.
			if (level == 0) {
				std::ofstream marker(root / "test-start.json");
				marker << "{\"room\":\"" << direct->at.room << "\",\"x\":" << direct->at.x
				       << ",\"y\":" << direct->at.y << ",\"facingLeft\":false}";
			}
			std::cout << "Level " << level + 1 << ": " << p.native.actors.size() << " actors, "
			          << parityTicks << " parity ticks, atomic merge and restart passed\n";
		}
		// A first-level position edit may travel to a subsequent level without
		// replacing that level's native scripts or accidentally applying slot IDs.
		std::cout << "Native campaign tests passed\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
