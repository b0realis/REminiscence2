// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor/content.h"
#include "editor/image.h"
#include "editor/runtime.h"
#include "editor/original.h"
#include "editor/preview_system.h"
#include "game.h"
#include "config.h"
#include "headless_system.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <fstream>
Options g_options;
const char *g_caption = "Editor tests";
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
#include "editor_audio_checks.h"
#include "editor_campaign_checks.h"
#include "editor_progress_checks.h"
#include "editor_graph_checks.h"
#include "editor_action_checks.h"
#include "editor_editing_checks.h"
#include "editor_state_checks.h"
#include "editor_native_module_unit_checks.h"
#include "editor_gameplay_audit_checks.h"
int main(int argc, char **argv) {
	try {
		using namespace editor;
		if (argc < 2)
			return 2;
		if (argc == 3 && std::string(argv[1]) == "--audio-file") {
			const auto clip = readAudio(argv[2]);
			check(clip.frames() > 0 && clip.rate > 0, "Ogg clip decoded empty");
			check(std::any_of(clip.samples.begin(), clip.samples.end(),
			                  [](auto v) { return v != 0; }),
			      "Ogg clip decoded silent");
			std::cout << "Audio decoded: " << clip.frames() << " stereo frames at " << clip.rate
			          << " Hz\n";
			return 0;
		}
		setDefaultOptions(g_options);
		nativeModuleUnitChecks();
		struct DrawStorage {
			AnimBufferState states[AnimBuffers::kCapacity];
			uint64_t guard = 123456;
		} storage{};
		AnimBuffers draws{};
		draws._states[2] = storage.states;
		draws._curPos[2] = 0xff;
		for (int i = 0; i < 400; ++i)
			draws.addState(2, i, 0, nullptr, nullptr);
		check(storage.guard == 123456 && draws._curPos[2] == 254 &&
		          draws._states[2] == storage.states + AnimBuffers::kCapacity,
		      "Sprite queue exceeded its capacity or wrapped its empty sentinel");
		// Verify the SDL byte-count -> engine stereo-frame boundary, with guard
		// samples beyond the supplied buffer. A 2x overwrite corrupted teardown.
		PreviewSystem audio;
		audio.callback = [](void *, int16_t *out, int frames) { std::fill_n(out, frames * 2, 0); };
		std::vector<int16_t> audioBuffer(256, 12345);
		PreviewSystem::fillAudio(&audio, reinterpret_cast<Uint8 *>(audioBuffer.data()),
		                         128 * sizeof(int16_t));
		check(std::all_of(audioBuffer.begin(), audioBuffer.begin() + 128,
		                  [](auto n) { return n == 0; }) &&
		          std::all_of(audioBuffer.begin() + 128, audioBuffer.end(),
		                      [](auto n) { return n == 12345; }),
		      "Editor audio callback overwrote its stereo buffer");
		const auto root = std::filesystem::path(argv[1]);
		std::filesystem::create_directories(root);
		audioChecks(root);
		campaignChecks(root);
		campaignProgressChecks(root);
		graphChecks(root);
		actionChecks(root);
		stateMachineChecks();
		editingChecks(root);
		Image rgbaTest{
		    2, 2, {rgba(1, 2, 3), rgba(91, 17, 36, 0), rgba(7, 23, 56, 128), rgba(45, 67, 89)}};
		writePNG(rgbaTest, root / "alpha-roundtrip.png");
		check(readImage(root / "alpha-roundtrip.png").pixels == rgbaTest.pixels,
		      "PNG export must preserve exact RGBA pixels including transparent RGB");
		rejects([&] { writePNG({2, 2, {}}, root / "invalid.png"); }, "Invalid PNG buffer accepted");
		auto p = makeProject();
		const auto room = p.rooms[0].id;
		auto door = makeModule(p, Kind::Door, {room, 160, 214});
		p.modules.push_back(door);
		auto duplicate = duplicateModule(p, door);
		p.modules.push_back(duplicate);
		check(duplicate.id != door.id && duplicate.a.x - door.a.x == duplicate.b.x - door.b.x,
		      "Module duplication loses part relationship");
		saveProject(p, root / "test.r2project.json");
		auto restored = loadProject(root / "test.r2project.json");
		check(serialize(p) == serialize(restored), "Project round trip");
		History h;
		h.remember(p);
		p.modules.erase(p.modules.begin());
		check(h.undo(p) && p.modules.size() == 2, "Undo module deletion");
		check(h.redo(p) && p.modules.size() == 1, "Redo module deletion");
		p = makeProject();
		sliceImage(p, "art/sheet.png", 512, 440, Slice{}, Layer::Back, true);
		check(p.rooms.size() == 4 && p.rooms[2].art[0].y == 216,
		      "Slicing respects vertical overlap");
		check(p.rooms[0].exits[2] == p.rooms[1].id && p.rooms[1].exits[3] == p.rooms[0].id,
		      "Slice room links");
		p.rooms[0].collision[3] = 2;
		p.rooms[0].ledges.push_back({4, 4, 1});
		sliceImage(p, "art/front.png", 512, 440, Slice{}, Layer::Front, false);
		check(p.rooms[0].collision[3] == 2 && p.rooms[0].ledges.size() == 1,
		      "Artwork import changes collision or ledges");
		const auto ct = compileCollision(p);
		check(ct[256 + 4 * 16 + 4] == 1 && ct[256 + 4 * 16 + 3] == 0, "Ledge compilation");
		const auto before = serialize(p);
		rejects([&] { sliceImage(p, "x", 9999, 9999, Slice{}, Layer::Back, true); },
		        "Oversized slice accepted");
		check(serialize(p) == before, "Failed slice destroyed project");
		rejects([&] { assetPath(root, "../secret.png"); }, "Path traversal accepted");
		auto bad = before;
		auto offset = bad.find("\"schema\": 9");
		bad.replace(offset, 11, "\"schema\": 10");
		rejects([&] { deserialize(bad); }, "Future schema accepted");
		bad = before;
		offset = bad.find("\"schema\": 9");
		bad.replace(offset, 11, "\"schema\": 18446744073709551615");
		rejects([&] { deserialize(bad); }, "Unsigned JSON overflow accepted");
		auto independent = makeProject();
		independent.modules.push_back(
		    makeModule(independent, Kind::Door, {independent.rooms[0].id, 160, 216}));
		savePrefab(independent.modules[0], root, root / "pair.r2module.json");
		auto imported = loadPrefab(independent, {independent.rooms[0].id, 176, 216},
		                           root / "pair.r2module.json", root);
		check(imported.id != independent.modules[0].id && imported.a.x - imported.b.x == 48,
		      "Prefab lost identity or paired spacing");
		independent.modules.push_back(imported);
		auto malformed = serialize(independent);
		offset = malformed.find("\"width\": 16");
		malformed.replace(offset, 11, "\"width\": 4294967312");
		rejects([&] { deserialize(malformed); }, "Module width overflow accepted");
		uint32_t pixel = rgba(0, 0, 0);
		over(pixel, rgba(255, 0, 0, 128));
		check((pixel & 255) == 128, "Alpha composition");
		Image target{4, 4, std::vector<uint32_t>(16, rgba(0, 0, 0))};
		Image source{2, 2, std::vector<uint32_t>(4, rgba(40, 80, 120))};
		blit(target, source, {"", 0, 0, 4, 4}, 0, 0, 4, 4);
		check(target.pixels[1] == source.pixels[0] && target.pixels[3] == rgba(0, 0, 0),
		      "Partial source crop must pad, not stretch");
		HeadlessSystem platform;
		if (argc > 2) {
			const auto library = assets::discover(argv[2]);
			check(!library.games.empty(), "No original assets found");
			gameplayAuditChecks(root, library.games.front(), platform);
			{
				OriginalLevel native(library.games.front(), &platform);
				check(native.startRoom() == 27 && native.actors().size() == 107,
				      "Original level-one records or starting room changed");
				int maps = 0;
				for (int slot = 0; slot < MaxRooms; ++slot) {
					const auto &r = native.room(slot);
					if (r.hasMap) {
						++maps;
						check(
						    r.scenery.pixels.size() == RoomWidth * RoomHeight &&
						        std::any_of(r.scenery.pixels.begin(), r.scenery.pixels.end(),
								            [&](auto c) { return c != r.scenery.pixels.front(); }),
						    "Original map decoded to an empty or solid image");
					}
				}
				check(maps >= 26, "Too few original level-one maps decoded");
				int actorRooms = 0;
				HeadlessSystem reversePlatform;
				OriginalLevel reverse(library.games.front(), &reversePlatform);
				for (int slot = MaxRooms - 1; slot >= 0; --slot) {
					const auto &a = native.room(slot), &b = reverse.room(slot);
					check(a.scenery.pixels == b.scenery.pixels &&
					          a.actors.pixels == b.actors.pixels,
					      "Native room palette or actor rendering depends on browsing order");
					actorRooms += a.scenery.pixels != a.actors.pixels;
				}
				check(actorRooms > 0, "Reference view rendered no native actors in any room");
				const auto copyFolder = root / ("scenery-" + makeProject().id);
				const auto copyPath = native.exportScenery(copyFolder);
				auto imported = loadProject(copyPath);
				check(imported.rooms.size() == 64 && imported.modules.empty() &&
				          roomIndex(imported, imported.spawn.room) == 27,
				      "Scenery copy lost native slots or falsely imported actors");
				const auto compiled = compileCollision(imported);
				check(std::equal(compiled.begin() + 256, compiled.end(),
				                 native.collision().begin() + 256),
				      "Scenery copy changed raw collision cells");
				for (int i = 0; i < 256; ++i) {
					const int raw = native.collision()[i];
					check(compiled[i] == (raw >= 0 && raw < 64 ? raw : -1),
					      "Scenery copy changed a native room exit");
				}
				Images copyImages;
				copyImages.root = copyFolder;
				validateImages(imported, copyImages);
				check(copyImages.get(imported.rooms[27].art[0].file).pixels ==
				          native.room(27).scenery.pixels,
				      "Scenery export pixels differ from native decoder");
				rejects([&] { native.exportScenery(copyFolder); },
				        "Scenery export overwrote an existing project");
				rejects([&] { native.room(64); }, "Invalid native room slot accepted");
				// A real authored edit, reload and native-player run in the copied geometry.
				imported.rooms[27].collision[0] = imported.rooms[27].collision[0] ? 0 : 1;
				saveProject(imported, copyPath);
				Scene study(loadProject(copyPath), copyFolder, library.games.front(), &platform);
				for (int t = 0; t < 12; ++t)
					study.tick();
				check(study.currentRoom() == 27 && !study.dead() &&
				          study.render().pixels.size() == RoomWidth * RoomHeight,
				      "Edited first-level geometry failed native-player playtest");
				std::cout << "Original level study: " << maps
				          << " maps, 107 actors, exact CT copy and edited playtest passed\n";
			}
			graphSceneChecks(root, library.games.front(), platform);
			actionSceneChecks(root, library.games.front(), platform);
			actorSceneChecks(root, library.games.front(), platform);
			campaignSceneChecks(root, library.games.front(), platform);
			campaignSnapshotChecks(root, library.games.front(), platform);
			campaignRestartChecks(root, library.games.front(), platform);
			platform._pi = {};
			p = makeProject();
			addRoom(p, 1, 0);
			connectRooms(p, 0, 2, 1, true);
			p.modules.push_back(makeModule(p, Kind::Door, {p.rooms[0].id, 160, 214}));
			p.modules.push_back(makeModule(p, Kind::Door, {p.rooms[1].id, 160, 214}));
			Scene scene(p, root, library.games.front(), &platform);
			scene.activate(0);
			check(scene.states()[0].open && !scene.states()[1].open,
			      "Door activation leaked into another module");
			check(scene.game()._res._ctData[256 + 4 * 16 + 10] == 0, "Open door still blocks");
			check(scene.game()._res._ctData[256 + 112 + 4 * 16 + 10] == 1,
			      "Other door lost collision");
			for (int t = 0; t < 320; ++t) {
				platform._pi.dirMask = t >= 30 ? PlayerInput::DIR_RIGHT : 0;
				scene.tick();
			}
			std::cout << "Player room=" << scene.currentRoom()
			          << " x=" << scene.game()._pgeLive[0].pos_x
			          << " y=" << scene.game()._pgeLive[0].pos_y
			          << " health=" << scene.game()._pgeLive[0].life << '\n';
			check(!scene.dead(), "Conrad died on a flat authored floor");
			check(scene.currentRoom() == 1, "Original movement did not cross authored room link");
			const auto frame = scene.render();
			check(std::count_if(frame.pixels.begin(), frame.pixels.end(),
			                    [](auto c) { return c != rgba(12, 20, 28); }) > 100,
			      "Original actors were not rendered");
			scene.restart();
			check(!scene.states()[0].open && scene.currentRoom() == 0,
			      "Restart did not restore module state");
			// Same original player script and independently allocated enemy actor.
			auto combat = makeProject();
			combat.modules.push_back(
			    makeModule(combat, Kind::Crawler, {combat.rooms[0].id, 160, 214}));
			combat.modules[0].health = 3;
			Scene encounter(combat, root, library.games.front(), &platform);
			encounter.game()._cheats = kCheatLifeCounter;
			const int enemy = encounter.states()[0].actor;
			bool fired = false, damaged = false;
			for (int t = 0; t < 220; ++t) {
				platform._pi = {};
				platform._pi.space = t >= 20 && t < 28;
				platform._pi.dirMask = t >= 35 && t < 50 ? PlayerInput::DIR_DOWN : 0;
				platform._pi.shift = t >= 55;
				encounter.tick();

				fired |= encounter.game()._pgeLive[2].room_location == 0;
				damaged |= encounter.game()._pgeLive[enemy].obj_type == 947 ||
				           encounter.game()._pgeLive[enemy].obj_type == 948;
				if (t == 50)
					encounter.render();
			}
			check(fired, "Relocated player bundle did not fire its original projectile");
			check(damaged, "Original crouched projectile did not trigger crawler death animation");
			auto climbing = makeProject();
			climbing.spawn.x = 96;
			std::fill(climbing.rooms[0].collision.begin() + 4 * 16 + 8,
			          climbing.rooms[0].collision.begin() + 5 * 16, 1);
			climbing.rooms[0].ledges.push_back({8, 4, 1});
			Scene climb(climbing, root, library.games.front(), &platform);
			for (int t = 0; t < 150; ++t) {
				platform._pi = {};
				platform._pi.dirMask = t < 20 ? PlayerInput::DIR_RIGHT : PlayerInput::DIR_UP;
				climb.tick();
			}
			check(!climb.dead() && climb.game()._pgeLive[0].pos_y < 170,
			      "Authored ledge cannot be climbed");
			auto vertical = makeProject();
			addRoom(vertical, 0, 1);
			connectRooms(vertical, 0, 1, 1, true);
			vertical.rooms[0].collision.fill(0);
			std::fill_n(vertical.rooms[1].collision.begin() + 2 * 16, 16, 1);
			Scene drop(vertical, root, library.games.front(), &platform);
			for (int t = 0; t < 120; ++t) {
				platform._pi = {};
				drop.tick();
			}
			check(!drop.dead() && drop.currentRoom() == 1 && drop.game()._pgeLive[0].pos_y == 70,
			      "Vertical room link/216-pixel stride failed");
			auto crowded = makeProject();
			crowded.spawn.y = 70;
			std::fill_n(crowded.rooms[0].collision.begin() + 2 * 16, 5, 1);
			for (int i = 0; i < MaxModules; ++i)
				crowded.modules.push_back(
				    makeModule(crowded, Kind::Crawler, {crowded.rooms[0].id, 160, 216}));
			Scene crowd(crowded, root, library.games.front(), &platform);
			for (int t = 0; t < 40; ++t) {
				platform._pi = {};
				crowd.tick();
				crowd.render();
			}
			check(!crowd.dead(), "Crowded authored room corrupted the player");
			// A real input edge opens one pair; holding the key must not toggle
			// repeatedly. Cyclic external wires are bounded and idempotent.
			auto puzzle = independent;
			puzzle.modules[0].toggle = true;
			puzzle.spawn.x = puzzle.modules[0].b.x - 8;
			Scene switches(puzzle, root, library.games.front(), &platform);
			for (int t = 0; t < 20; ++t) {
				platform._pi = {};
				platform._pi.enter = true;
				switches.tick();
			}
			check(switches.states()[0].open && !switches.states()[1].open,
			      "Held activator repeated or changed another door");
			puzzle.modules[0].outputs = {puzzle.modules[1].id};
			puzzle.modules[1].outputs = {puzzle.modules[0].id};
			Scene chained(puzzle, root, library.games.front(), &platform);
			chained.activate(0);
			check(chained.states()[0].open && chained.states()[1].open,
			      "Cyclic connections did not open both doors");
			// An opaque foreground must occlude every actor without changing
			// movement, collision or the underlying source artwork.
			std::vector<unsigned char> bmp(58);
			bmp[0] = 'B';
			bmp[1] = 'M';
			bmp[2] = 58;
			bmp[10] = 54;
			bmp[14] = 40;
			bmp[18] = bmp[22] = bmp[26] = 1;
			bmp[28] = 24;
			bmp[34] = 4;
			bmp[54] = 11;
			bmp[55] = 93;
			bmp[56] = 210;
			{
				std::ofstream f(root / "opaque.bmp", std::ios::binary);
				f.write(reinterpret_cast<char *>(bmp.data()), bmp.size());
			}
			auto cover = makeProject();
			cover.rooms[0].art[2] = {"opaque.bmp", 0, 0, 1, 1};
			Scene occluded(cover, root, library.games.front(), &platform);
			for (int t = 0; t < 45; ++t) {
				platform._pi = {};
				platform._pi.dirMask = PlayerInput::DIR_RIGHT;
				occluded.tick();
			}
			check(!occluded.dead() && occluded.game()._pgeLive[0].pos_x > 32,
			      "Foreground became collision");
			const auto covered = occluded.render();
			check(std::all_of(covered.pixels.begin(), covered.pixels.end(),
			                  [](auto v) { return v == rgba(210, 93, 11); }),
			      "Foreground fails to cover actors");
		}
		std::cout << "Editor checks passed\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
