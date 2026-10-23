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
	int events = 0;
	std::function<void(settings::Session &)> menu;
	void showSettings(settings::Session &session) override {
		if (menu)
			menu(session);
	}
	void processEvents() override {
		if (++events > 100000) {
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
			if (!malformed(truncated, 3))
				return 11;
			auto badLink = original;
			// First PGE's serialized next-in-room link must stay inside the entity table.
			badLink[13 + 22] = 0x7F;
			if (!malformed(badLink, 3) || !malformed(original, 4))
				return 12;
			const auto catalog = saves::scan(argv[2], 0);
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
				s.saveSlot = 5;
				if (!s.command(settings::Action::SaveGame, 5, message) || !s.saves[5].supported)
					throw std::runtime_error("menu save/catalog refresh failed");
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
		if (!game->saveStateRewind() || !game->loadStateRewind()) {
			return 4;
		}
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

	game->_mix.free();
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
