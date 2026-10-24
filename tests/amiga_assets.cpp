// SPDX-License-Identifier: GPL-3.0-or-later
#include <filesystem>
#include <algorithm>
#include <iostream>
#include <memory>
#include <vector>
#include "config.h"
#include "fs.h"
#include "game.h"
#include "headless_system.h"
#include "screenshot.h"
#include "save_catalog.h"

struct BoundedSystem final : HeadlessSystem {
	int events = 0, maxEvents = 100000;
	std::function<void()> eventHook;
	std::function<void(settings::Session &)> menu;
	std::string notice;
	bool pauseRequest = false;
	void notify(const std::string &message, bool = false) override { notice = message; }
	bool consumePauseRequest() override {
		const bool result = pauseRequest;
		pauseRequest = false;
		return result;
	}
	void showSettings(settings::Session &session) override {
		if (menu)
			menu(session);
	}
	void processEvents() override {
		if (eventHook)
			eventHook();
		if (++events > maxEvents) {
			_pi.quit = true;
		}
	}
};
Options g_options;
const char *g_caption = "REminiscence2 asset tests";

int main(int argc, char **argv) {
	if (argc != 3) {
		return 1;
	}
	const std::filesystem::path output(argv[2]);
	std::filesystem::create_directories(output);
	setDefaultOptions(g_options);
	g_options.enable_password_menu = false;
	FileSystem files(argv[1]);
	BoundedSystem platform;
	auto game = std::make_unique<Game>(&platform, &files, &files, argv[2], 0, kResourceTypeAmiga,
	                                   LANG_EN, kWidescreenNone, true, 0);
	game->_res.init();
	game->_res.load_TEXT();
	game->_res.load("FONT8", Resource::OT_FNT, "SPR");
	game->_res.load("ICONE", Resource::OT_ICN, "SPR");
	game->_res.load("ICON", Resource::OT_ICN, "SPR");
	game->_res.load("PERSO", Resource::OT_SPM);
	game->_mix.init();
	game->_score = 0;
	game->_randSeed = 0;
	game->_mix._mod._isAmiga = true;
	for (int cutscene : {0x40, 0x0D}) {
		game->playCutscene(cutscene);
		if (platform._pi.quit) {
			std::cerr << "Cutscene exceeded event budget\n";
			return 8;
		}
	}
	// Journal unlocks require a completed scene, not a skipped playback.
	game->_journal = {};
	platform._pi.backspace = true;
	game->playCutscene(Cutscene::kCineMemo);
	if (game->_cut._completed || game->_journal.unlocked)
		return 32;
	game->playCutscene(Cutscene::kCineMemo);
	if (!game->_cut._completed || !(game->_journal.unlocked & (1u << 7)))
		return 33;
	game->_journal = {};
	// Test the actual Amiga sprite masks, rather than only checking palette RGB.
	// Jacket, skin, hair and trousers must never enter the recolour mask.
	int appearanceFrames = 0, shirtPixels = 0;
	for (int frame = 0; frame < Resource::NUM_SPRITES; ++frame) {
		if (Resource::_spmOffsetsTable[frame] >= 178647)
			continue;
		const auto *source = game->_res._sprData[frame];
		if (!source)
			continue;
		const int width = ((source[2] >> 7) + 1) * 16, height = source[2] & 127;
		game->_vid.AMIGA_decodeSpm(source, game->_res._scratchBuffer);
		std::vector<uint8_t> original(game->_res._scratchBuffer,
		                              game->_res._scratchBuffer + width * height);
		shirtPixels += appearance::markAmigaTshirt(game->_res._scratchBuffer, width, height,
		                                           Resource::kScratchBufferSize);
		for (size_t pixel = 0; pixel < original.size(); ++pixel) {
			if (original[pixel] != game->_res._scratchBuffer[pixel] &&
			    !((original[pixel] == 1 &&
			       game->_res._scratchBuffer[pixel] == appearance::ShirtLight) ||
			      (original[pixel] == 5 &&
			       game->_res._scratchBuffer[pixel] == appearance::ShirtShadow)))
				return 26;
		}
		if (frame == 2) {
			if (original[9 * width + 3] != 1 ||
			    game->_res._scratchBuffer[9 * width + 3] != appearance::ShirtLight)
				return 27;
			if (original[38 * width + 1] != 1 || game->_res._scratchBuffer[38 * width + 1] != 1)
				return 28;
		}
		if (frame == 298) {
			// Metal highlight on the held weapon uses the same shades as the T-shirt.
			for (int y = 3; y <= 4; ++y)
				for (int x = 0; x <= 4; ++x)
					if (game->_res._scratchBuffer[y * width + x] != original[y * width + x])
						return 29;
		}
		++appearanceFrames;
	}
	if (appearanceFrames < 500 || shirtPixels < 1000)
		return 30;

	std::vector<progress::Snapshot> chapterSaves;
	int rooms = 0;
	for (int level = 0; level < 7; ++level) {
		std::cout << "Loading Amiga level " << level + 1 << std::endl;
		game->_currentLevel = level;
		game->loadLevelData();
		game->resetGameState();
		game->_cut._id = 0xFFFF;
		game->_frameTimestamp = platform.clock;
		game->_saveTimestamp = platform.clock;
		const int firstRoom = game->_currentRoom;
		for (int room = 0; room < 64; ++room) {
			if (!game->hasLevelMap(level, room)) {
				continue;
			}
			game->_currentRoom = room;
			game->loadLevelMap();
			std::vector<uint8_t> original(game->_vid._backLayer,
			                              game->_vid._backLayer + game->_vid._layerSize);
			// Reserved shirt aliases must never be part of room artwork.
			if (std::any_of(original.begin(), original.end(), [](uint8_t color) {
				    return color == appearance::ShirtShadow || color == appearance::ShirtLight;
			    }))
				return 18;
			uint8_t paletteBefore[768], paletteAfter[768];
			platform.getPalette(paletteBefore, 256);
			platform.mode = kWidescreenAdjacentRooms;
			game->_widescreenMode = kWidescreenAdjacentRooms;
			game->loadLevelMap();
			platform.getPalette(paletteAfter, 256);
			if (memcmp(original.data(), game->_vid._backLayer, original.size()) ||
			    memcmp(paletteBefore, paletteAfter, 768)) {
				std::cerr << "Adjacent rendering changed active room " << level << ':' << room
				          << '\n';
				return 6;
			}
			platform.mode = kWidescreenNone;
			game->_widescreenMode = kWidescreenNone;
			++rooms;
		}
		game->_currentRoom = firstRoom;
		game->loadLevelMap();
		game->_loadMap = false;
		for (int frame = 0; frame < 30; ++frame) {
			game->mainLoop();
		}
		uint8_t palette[256 * 3];
		platform.getPalette(palette, 256);
		const std::string name =
		    (output / ("amiga-level" + std::to_string(level + 1) + ".bmp")).string();
		if (!saveBMP(name.c_str(), game->_vid._frontLayer, palette, game->_vid._w, game->_vid._h)) {
			return 5;
		}
		const uint32_t score = game->_score;
		progress::Snapshot chapter;
		chapter.edition = 0;
		chapter.level = level;
		chapter.room = game->_pgeLive[0].room_location;
		chapter.skill = game->_skillLevel;
		chapter.health = game->_pgeLive[0].life;
		chapter.entities = game->_res._pgeNum;
		chapter.fingerprint = game->_assetFingerprint;
		File state;
		state.openMemoryBuffer(32768);
		game->writeGameState(state);
		chapter.state.resize(state.size());
		state.seek(0);
		state.read(chapter.state.data(), chapter.state.size());
		chapterSaves.push_back(std::move(chapter));

		{
			const auto player = game->_pgeLive[0];
			const auto seed = game->_randSeed;
			const auto frameTime = game->_frameTimestamp, saveTime = game->_saveTimestamp;
			std::vector<uint8_t> front(game->_vid._frontLayer,
			                           game->_vid._frontLayer + game->_vid._layerSize);
			std::vector<uint8_t> back(game->_vid._backLayer,
			                          game->_vid._backLayer + game->_vid._layerSize);
			uint8_t originalPalette[768];
			platform.getPalette(originalPalette, 256);
			for (auto choice : {appearance::Shirt::RedPink, appearance::Shirt::White,
			                    appearance::Shirt::Original}) {
				auto preferences = game->_preferences;
				preferences.conradShirt = choice;
				std::string reason;
				if (!game->applyPreferences(preferences, reason))
					return 19;
				uint8_t recolored[768];
				platform.getPalette(recolored, 256);
				for (int color = 0; color < 256; ++color) {
					if (color != appearance::ShirtShadow && color != appearance::ShirtLight &&
					    memcmp(originalPalette + color * 3, recolored + color * 3, 3))
						return 20;
				}
				if (memcmp(front.data(), game->_vid._frontLayer, front.size()) ||
				    memcmp(back.data(), game->_vid._backLayer, back.size()) ||
				    game->_score != score || game->_randSeed != seed ||
				    game->_pgeLive[0].life != player.life ||
				    game->_pgeLive[0].pos_x != player.pos_x ||
				    game->_pgeLive[0].pos_y != player.pos_y || game->_frameTimestamp != frameTime ||
				    game->_saveTimestamp != saveTime)
					return 21;
				const auto *fill = recolored + appearance::ShirtLight * 3;
				if (choice == appearance::Shirt::White &&
				    (fill[0] != fill[1] || fill[1] != fill[2]))
					return 22;
				if (choice == appearance::Shirt::RedPink &&
				    !(fill[0] > fill[2] && fill[2] > fill[1]))
					return 23;
				if (choice == appearance::Shirt::Original &&
				    memcmp(fill, originalPalette + 0x41 * 3, 3))
					return 24;
			}
		}

		if (level == 0) {
			std::vector<uint8_t> frame(game->_vid._frontLayer,
			                           game->_vid._frontLayer + game->_vid._layerSize);
			const auto player = game->_pgeLive[0];
			const auto seed = game->_randSeed;
			auto preferences = game->_preferences;
			preferences.widescreen = kWidescreenMirrorRoom;
			std::string reason;
			if (!game->applyPreferences(preferences, reason) ||
			    memcmp(frame.data(), game->_vid._frontLayer, frame.size()) ||
			    game->_pgeLive[0].pos_x != player.pos_x ||
			    game->_pgeLive[0].pos_y != player.pos_y || game->_pgeLive[0].life != player.life ||
			    game->_randSeed != seed)
				return 9;
			preferences.widescreen = kWidescreenNone;
			if (!game->applyPreferences(preferences, reason))
				return 10;
		}
		if (!game->saveGameState(1)) {
			return 2;
		}
		game->_score = 123456;
		game->_assetFingerprint ^= 1;
		if (game->loadGameState(1) || game->_score != 123456)
			return 31;
		game->_assetFingerprint ^= 1;
		if (!game->loadGameState(1) || game->_score != score) {
			return 3;
		}
		if (level == 0) {
			File snapshot;
			snapshot.openMemoryBuffer(16384);
			game->saveState(&snapshot);
			std::vector<uint8_t> original(snapshot.size());
			snapshot.seek(0);
			snapshot.read(original.data(), original.size());
			for (int legacyVersion : {2, 3}) {
				const int slot = 95 + legacyVersion;
				File legacy;
				if (!legacy.open(saves::name(-1, 0, slot).c_str(), "wb", argv[2]))
					return 34;
				legacy.writeUint32BE(0x46425356);
				legacy.writeUint16BE(legacyVersion);
				char description[32] = "level=1 room=0";
				legacy.write(description, 32);
				legacy.write(original.data(), original.size() - 20 - (legacyVersion == 2 ? 2 : 0));
				legacy.close();
				game->_score = 123456;
				if (!game->loadGameState(slot) || game->_score != score || game->_journal.unlocked)
					return 35;
			}
			if (!game->loadGameState(1))
				return 36;
			auto malformed = [&](std::vector<uint8_t> bytes, int version) {
				File file;
				if (!file.open("fb-level1-02.state", "wb", argv[2]))
					return false;
				file.writeUint32BE(0x46425356);
				file.writeUint16BE(version);
				char description[32] = "level=1 room=0";
				file.write(description, 32);
				file.write(bytes.data(), bytes.size());
				file.close();
				if (game->loadGameState(2))
					return false;
				File after;
				after.openMemoryBuffer(16384);
				game->saveState(&after);
				std::vector<uint8_t> current(after.size());
				after.seek(0);
				after.read(current.data(), current.size());
				return current == original;
			};
			auto truncated = original;
			truncated.pop_back();
			if (!malformed(truncated, 4))
				return 11;
			auto badAnimation = original;
			badAnimation[13 + 16] = badAnimation[13 + 17] = 255;
			if (!malformed(badAnimation, 4))
				return 61;
			auto badScript = original;
			badScript[13 + 20] = badScript[13 + 21] = 255;
			if (!malformed(badScript, 4))
				return 62;
			auto badType = original;
			badType[13] = badType[14] = 255;
			if (!malformed(badType, 4))
				return 63;
			auto badLink = original;
			// First PGE's serialized next-in-room link must stay inside the entity table.
			badLink[13 + 22] = 0x7F;
			if (!malformed(badLink, 3) || !malformed(original, UINT16_MAX))
				return 12;
			const auto catalog = saves::scan(argv[2], 0, kResourceTypeAmiga);
			if (catalog.size() != 100 || !catalog[1].supported || !catalog[2].exists ||
			    catalog[2].supported)
				return 13;
			platform.menu = [&](settings::Session &s) {
				std::string message;
				s.applyCheats(kCheatOneHitKill | kCheatNoHit | kCheatLifeCounter,
				              PlayerInput::DF_FASTMODE);
				game->_pgeLive[0].life = 1;
				if (!s.command(settings::Action::Heal, 1, message) ||
				    game->_pgeLive[0].life < game->_res._pgeInit[0].life)
					throw std::runtime_error("menu healing failed");
				if (!s.command(settings::Action::Screenshot, 1, message))
					throw std::runtime_error("clean screenshot failed");
				const bool checkpointBefore = game->_validSaveState;
				s.saveSlot = 5;
				if (!s.command(settings::Action::SaveGame, 5, message) || !s.saves[5].supported)
					throw std::runtime_error("menu save/catalog refresh failed");
				if (game->_validSaveState != checkpointBefore)
					throw std::runtime_error("manual save changed checkpoint validity");
				game->_score = 9876;
				if (!s.command(settings::Action::LoadGame, 5, message) || game->_score != score)
					throw std::runtime_error("menu load failed");
				s.applyCheats(0, 0);
				s.action = settings::Action::Resume;
			};
			game->handleConfigPanel();
			platform.menu = {};
			if (game->_cheats || platform._pi.dbgMask)
				return 14;
		}
		game->_randSeed = 12345;
		game->_journal.chapter(level);
		const auto unlocked = game->_journal.unlocked;
		if (!game->saveStateRewind())
			return 4;
		for (int i = 0; i < 10; ++i)
			game->getRandomNumber();
		game->_journal.memory();
		if (!game->loadStateRewind() || game->_randSeed != 12345 ||
		    game->_journal.unlocked != unlocked)
			return 4;
		game->clearStateRewind();
	}
	const int previousLevel = game->_currentLevel;
	std::string reason;
	if (game->startSettingsLevel(7, kSkillNormal, reason) || game->_currentLevel != previousLevel)
		return 15;
	if (platform.leftCopies != rooms || platform.rightCopies != rooms)
		return 7;
	for (int level = 0; level < 7; ++level) {
		if (!game->saveStateRewind())
			return 16;
		platform.menu = [&](settings::Session &s) {
			s.targetLevel = level;
			s.targetSkill = level % 3;
			if (!s.command(settings::Action::StartLevel, 1, reason))
				throw std::runtime_error(reason);
			s.action = settings::Action::Resume;
		};
		game->handleConfigPanel();
		if (game->_currentLevel != level || game->_skillLevel != level % 3 || game->_score ||
		    game->_rewindLen || game->_deathCutsceneCounter || game->_validSaveState)
			return 17;
		game->_cut._id = 0xFFFF;
		game->mainLoop();
	}
	platform.menu = {};
	// Turn toward the camera so the T-shirt is actually visible in the captures.
	if (!game->startSettingsLevel(0, kSkillNormal, reason))
		return 31;
	game->_cut._id = 0xFFFF;
	for (int tick = 0; tick < 33; ++tick) {
		platform._pi.dirMask = tick >= 30 ? PlayerInput::DIR_RIGHT : 0;
		game->mainLoop();
	}
	platform._pi.dirMask = 0;
	const auto *frameBegin = game->_vid._frontLayer;
	const auto visibleShirt =
	    std::count(frameBegin, frameBegin + game->_vid._layerSize, appearance::ShirtLight);
	if (visibleShirt < 10)
		return 32;
	uint8_t nativePalette[768];
	platform.getPalette(nativePalette, 256);
	for (auto style :
	     {appearance::Shirt::Original, appearance::Shirt::RedPink, appearance::Shirt::White}) {
		auto preferences = game->_preferences;
		preferences.conradShirt = style;
		if (!game->applyPreferences(preferences, reason))
			return 33;
		uint8_t colors[768];
		platform.getPalette(colors, 256);
		for (int pixel = 0; pixel < game->_vid._layerSize; ++pixel) {
			const int index = game->_vid._frontLayer[pixel];
			if (index != appearance::ShirtShadow && index != appearance::ShirtLight &&
			    memcmp(colors + index * 3, nativePalette + index * 3, 3))
				return 34;
		}
		const char *name = style == appearance::Shirt::Original ? "original"
		                   : style == appearance::Shirt::White  ? "white"
		                                                        : "red-pink";
		const auto screenshot = (output / (std::string("conrad-tshirt-") + name + ".bmp")).string();
		if (!saveBMP(screenshot.c_str(), game->_vid._frontLayer, colors, game->_vid._w,
		             game->_vid._h))
			return 35;
	}

	// Inventory must support more than the inherited 24 stack entries and let
	// the player reach settings without waiting to close it with the old key.
	for (unsigned i = 1; i <= 26; ++i) {
		game->_res._pgeInit[i].icon_num = 0;
		game->_res._pgeInit[i].text_num = 0;
		game->_pgeLive[i].next_inventory_PGE = i == 26 ? 255 : i + 1;
	}
	game->_pgeLive[0].current_inventory_PGE = 1;
	platform._pi = {};
	platform.eventHook = [&] { platform._pi.backspace = true; };
	game->handleInventory();
	bool inventoryMenuSeen = false;
	platform._pi = {};
	platform.eventHook = [&] { platform._pi.escape = true; };
	platform.menu = [&](settings::Session &s) {
		inventoryMenuSeen = true;
		s.action = settings::Action::Resume;
	};
	game->handleInventory();
	platform.eventHook = {};
	platform.menu = {};
	if (!inventoryMenuSeen)
		return 64;
	// Deterministic traversal stress across every chapter. These are route probes,
	// not a full playthrough: queued story/death scenes restart the local route.
	auto replay = [&] {
		uint32_t hash = 2166136261u;
		game->_preferences.diskAutosave = false;
		for (int level = 0; level < 7; ++level) {
			if (!game->startSettingsLevel(level, kSkillNormal, reason))
				throw std::runtime_error(reason);
			game->_randSeed = 0x5a17u + level;
			game->_cut._id = 0xffff;
			for (int tick = 0; tick < 1500; ++tick) {
				if (game->_currentLevel != level || game->_deathCutsceneCounter ||
				    game->_cut._id != 0xffff || game->_pgeLive[0].life <= 0) {
					if (!game->startSettingsLevel(level, kSkillNormal, reason))
						throw std::runtime_error(reason);
					game->_cut._id = 0xffff;
				}
				platform._pi = {};
				const int phase = tick % 240;
				platform._pi.dirMask = phase < 80    ? PlayerInput::DIR_RIGHT
				                       : phase < 160 ? PlayerInput::DIR_LEFT
				                       : phase < 200 ? PlayerInput::DIR_UP
				                                     : PlayerInput::DIR_DOWN;
				platform._pi.shift = tick % 100 < 30;
				platform._pi.enter = tick % 97 == 0;
				game->mainLoop();
				if (game->_currentLevel != level || game->_cut._id != 0xffff ||
				    game->_pgeLive[0].life <= 0)
					continue;
				if (tick % 300 == 0 && (!game->saveGameState(90) || !game->loadGameState(90)))
					throw std::runtime_error("route save/load failed");
				if (tick % 30 == 0) {
					File state;
					state.openMemoryBuffer(32768);
					game->saveState(&state);
					state.seek(0);
					for (unsigned i = 0; i < state.size(); ++i)
						hash = (hash ^ state.readByte()) * 16777619u;
				}
			}
		}
		return hash;
	};
	const auto replayA = replay(), replayB = replay();
	if (replayA != replayB) {
		std::cerr << "Non-deterministic route replay " << replayA << ' ' << replayB << '\n';
		return 60;
	}
	std::cout << "21,000 deterministic route ticks; state hash " << replayA << '\n';
	game->_preferences.diskAutosave = true;
	// Recovery integration uses its own directory so repeated runs never inherit
	// candidates created by earlier rendering checks.
	const auto recovery = output / "recovery";
	std::filesystem::remove_all(recovery);
	std::filesystem::create_directories(recovery);
	const auto recoveryPath = recovery.string();
	const auto *oldPath = game->_savePath;
	game->_savePath = recoveryPath.c_str();
	if (!game->startSettingsLevel(0, kSkillNormal, reason))
		return 40;
	game->_cut._id = 0xFFFF;
	game->mainLoop();
	auto autosaves = progress::scan(recoveryPath, kResourceTypeAmiga);
	if (autosaves.size() != 1 || !autosaves[0].preview ||
	    autosaves[0].preview->rgba.size() != 128 * 112 * 4)
		return 41;
	const auto initial = autosaves[0];
	game->_preferences.diskAutosaveSeconds = 15;
	for (int n = 0; n < 449; ++n)
		game->updateProgress();
	if (progress::scan(recoveryPath, 0).size() != 1)
		return 50;
	game->updateProgress();
	if (progress::scan(recoveryPath, 0).size() != 2)
		return 51;
	game->_preferences.diskAutosave = false;
	for (int n = 0; n < 500; ++n)
		game->updateProgress();
	if (progress::scan(recoveryPath, 0).size() != 2)
		return 52;
	game->_preferences.diskAutosave = true;

	const auto ticks = game->_playTicks;
	platform.menu = [&](settings::Session &s) {
		platform.clock += 600000;
		s.action = settings::Action::Resume;
	};
	game->handleConfigPanel();
	platform.menu = {};
	if (game->_playTicks != ticks || progress::scan(recoveryPath, 0).size() != 2)
		return 42;
	game->clearStateRewind();
	// Exact selection drops the chosen point and newer branch, preserving older history.
	for (int n = 0; n < 4; ++n) {
		game->_score = n;
		game->_playTicks = 150 * (n + 1);
		if (!game->saveStateRewind())
			return 43;
	}
	if (!game->loadStateRewind(2) || game->_score != 1 || game->_playTicks != 300 ||
	    game->_rewindLen != 1 || game->_rewindRecords[game->_rewindPtr].tick != 150)
		return 44;
	if (!game->startSettingsLevel(6, kSkillNormal, reason))
		return 45;
	game->_cut._id = 0xFFFF;
	game->mainLoop();
	if (!game->loadPersistent(initial, reason) || game->_currentLevel != 0 ||
	    game->_playTicks != initial.playTicks || game->_pgeLive[0].room_location != initial.room ||
	    game->_rewindLen || game->_loadMap) {
		std::cerr << reason << '\n';
		return 46;
	}
	const auto scoreBefore = game->_score;
	auto mismatch = initial;
	mismatch.state[mismatch.state.size() - 9] ^= 1;
	if (game->loadPersistent(mismatch, reason) || game->_score != scoreBefore ||
	    game->_currentLevel != 0)
		return 47;
	// A structurally valid newer generation for a different asset revision is skipped.
	if (!game->savePersistent(reason))
		return 48;
	auto incompatible = initial;
	incompatible.fingerprint ^= 1;
	incompatible.state[incompatible.state.size() - 9] ^= 1;
	if (!progress::save(recoveryPath, incompatible, reason) || !game->continueLatest(reason) ||
	    game->_currentLevel != 0)
		return 48;
	// A new Game/Resource instance reconstructs the saved level with no live resources.
	auto restarted = std::make_unique<Game>(&platform, &files, &files, recoveryPath.c_str(), 6,
	                                        kResourceTypeAmiga, LANG_EN, kWidescreenNone, false, 0);
	restarted->initCutsceneResources();
	restarted->_res.load("ICONE", Resource::OT_ICN, "SPR");
	restarted->_res.load("ICON", Resource::OT_ICN, "SPR");
	restarted->_res.load("PERSO", Resource::OT_SPM);
	restarted->_mix.init();
	restarted->_mix._mod._isAmiga = true;
	if (!restarted->continueLatest(reason) || restarted->_currentLevel != 0 ||
	    restarted->_randSeed != game->_randSeed)
		return 49;
	for (const auto &chapter : chapterSaves) {
		if (!restarted->loadPersistent(chapter, reason) ||
		    restarted->_currentLevel != chapter.level ||
		    restarted->_pgeLive[0].room_location != chapter.room) {
			std::cerr << "Recovery level " << chapter.level << ": " << reason << '\n';
			return 53;
		}
		restarted->mainLoop();
	}
	// Named milestones load across levels through the same transactional
	// resource preflight, and the production death path offers unified recovery.
	if (!game->loadPersistent(initial, reason) ||
	    !game->saveManualState(77, "Before crossing", true, reason) || !game->saveGameState(0))
		return 65;
	const auto milestoneScore = game->_score;
	if (game->saveGameState(77))
		return 66;
	game->_stateSlot = 77;
	game->_stub->_pi.save = true;
	game->inp_handleSpecialKeys();
	if (platform.notice.find("Protected milestone") == std::string::npos) {
		std::cerr << "Quicksave hid the protected milestone explanation\n";
		return 70;
	}
	bool protectedMessage = false;
	platform.menu = [&](settings::Session &session) {
		session.browseLevel = 0;
		protectedMessage = !session.command(settings::Action::SaveGame, 77, reason) &&
		                   reason.find("Protected milestone") != std::string::npos;
		session.action = settings::Action::Resume;
	};
	game->handleConfigPanel();
	platform.menu = {};
	if (!protectedMessage)
		return 71;
	// A disconnect pause retains the composed frame for screenshots/previews.
	const std::vector<uint8_t> pausedFrame(game->_vid._frontLayer,
	                                       game->_vid._frontLayer + game->_vid._layerSize);
	bool composedPause = false;
	platform.menu = [&](settings::Session &session) {
		composedPause = std::equal(pausedFrame.begin(), pausedFrame.end(), game->_vid._frontLayer);
		session.action = settings::Action::Resume;
	};
	platform.pauseRequest = true;
	game->mainLoop();
	platform.menu = {};
	if (!composedPause)
		return 72;
	// protected against quicksave too
	if (!game->startSettingsLevel(6, kSkillNormal, reason) ||
	    !game->loadManualState(0, 77, reason) || game->_currentLevel != 0 ||
	    game->_score != milestoneScore)
		return 67;
	bool recoverySeen = false, recoveryLoaded = false;
	platform.menu = [&](settings::Session &session) {
		recoverySeen = session.recovery && !session.canSave && !session.saves.empty();
		session.browseLevel = 0;
		recoveryLoaded = session.command(settings::Action::LoadGame, 77, reason);
		session.action = settings::Action::Resume;
	};
	game->_pgeLive[0].life = 0;
	game->_deathCutsceneCounter = 1;
	game->_cut._id = game->_cut._deathCutsceneId = 0xFFFF;
	game->mainLoop();
	platform.menu = {};
	if (!recoverySeen || !recoveryLoaded || game->_pgeLive[0].life <= 0 ||
	    game->_deathCutsceneCounter || game->_playTicks != initial.playTicks)
		return 68;
	if (!progress::updateManual(recoveryPath, 0, 0, 77, "Unlocked", false, reason) ||
	    !game->saveGameState(77))
		return 69;
	int rewindRoom = 0;
	while (rewindRoom < 64 &&
	       (rewindRoom == game->_res._pgeInit[0].init_room || !game->hasLevelMap(0, rewindRoom)))
		++rewindRoom;
	if (rewindRoom == 64)
		return 73;
	game->_pgeLive[0].room_location = game->_currentRoom = rewindRoom;
	game->refreshRestoredFrame();
	if (!game->saveStateRewind())
		return 73;
	game->_stub->_pi.rewind = true;
	game->inp_handleSpecialKeys();
	if (game->_currentRoom != rewindRoom || game->_loadMap) {
		std::cerr << "Shortcut rewind left the restored room unprepared\n";
		return 74;
	}
	std::cout << "Local disk write samples (ms): last=" << game->_lastDiskSaveMs
	          << " worst=" << game->_worstDiskSaveMs << '\n';
	restarted->_mix.free();
	restarted.reset();
	game->_savePath = oldPath;
	game->_mix.free();
	// Exercise the production startup path, including the welcome-back command,
	// rather than only calling the recovery helper on an already loaded game.
	for (bool direct : {true, false}) {
		platform._pi = {};
		platform.events = 0;
		platform.maxEvents = 3;
		bool welcomeSeen = false;
		platform.menu = [&](settings::Session &session) {
			welcomeSeen = session.startup && !session.autosaves.empty();
			if (!session.command(settings::Action::ContinueLatest, 1, reason))
				throw std::runtime_error(reason);
			session.action = settings::Action::Resume;
		};
		auto launch =
		    std::make_unique<Game>(&platform, &files, &files, recoveryPath.c_str(), 6,
			                       kResourceTypeAmiga, LANG_EN, kWidescreenNone, false, 0);
		launch->_continueRequested = direct;
		launch->run();
		if (launch->_continueFailed || launch->_currentLevel != 0 || launch->_activeProgress ||
		    (!direct && !welcomeSeen))
			return 54;
	}
	platform.menu = {};
	platform.maxEvents = 100000;
	platform._pi = {};
	if (platform.leftCopies < rooms || platform.rightCopies < rooms) {
		return 7;
	}
	std::cout << "Amiga asset checks passed: 7 levels, " << rooms
	          << " room renders, 210 baseline simulation steps, " << appearanceFrames
	          << " appearance frames, manual save/load, malformed-save rejection, "
	             "rewind, menu cheats/healing/screenshots, all shirt styles, level jumps and "
	             "opening cutscenes.\n";
	return 0;
}
