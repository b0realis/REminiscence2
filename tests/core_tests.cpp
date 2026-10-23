// SPDX-License-Identifier: GPL-3.0-or-later
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include "config.h"
#include "file.h"
#include "resource_paq.h"
#include "game.h"
#include "util.h"

Options g_options;
const char *g_caption = "REminiscence2 tests";

static int failures = 0;
static void check(bool condition, const char *message) {
	if (!condition) {
		std::cerr << "FAIL: " << message << '\n';
		++failures;
	}
}

int main(int argc, char **argv) {
	if (argc != 2) {
		return 1;
	}
	// This path exceeds the old 16-byte configuration filename buffer.
	const auto root = std::filesystem::path(argv[1]) / "configuration with spaces";
	std::filesystem::create_directories(root);
	const auto path = root / "custom-options.cfg";
	{
		std::ofstream config(path);
		config << "  # Leading whitespace and CRLF are accepted\r\n"
		       << " enable_language_selection = TRUE \r\n"
		       << "fade_out_palette=0\n"
		       << "play_caillou_cutscene=1\n"
		       << "use_white_tshirt=false\n"
		       << "use_white_tshirt_extra=true\n"
		       << "use_tile_data=maybe\n"
		       << "restore_memo_cutscene=false";
	}
	Options options;
	setDefaultOptions(options);
	check(options.bypass_protection && options.fade_out_palette, "historical defaults");
	check(!options.use_tile_data, "tile rendering has an explicit default");
	check(loadConfiguration(path.string().c_str(), options), "load long configuration path");
	check(options.enable_language_selection, "parse uppercase boolean and whitespace");
	check(!options.fade_out_palette && options.play_caillou_cutscene, "parse numeric booleans");
	check(!options.use_white_tshirt, "unknown keys must not match a valid prefix");
	check(!options.use_tile_data, "invalid booleans preserve the previous value");
	check(!options.restore_memo_cutscene, "parse final line without newline");
	check(!loadConfiguration((root / "missing.cfg").string().c_str(), options),
	      "report missing config");

	// PAQ lookup uses the inherited filename table and needs no commercial data.
	ResourcePaq paq(nullptr);
	ResourceArchive *archive = &paq;
	check(archive->hasEntry("GLOBAL.ICN"), "known PAQ entry through virtual interface");
	check(archive->hasEntry("global.icn"), "case insensitive PAQ entry lookup");
	check(!archive->hasEntry("REMINISCENCE2-NOT-A-GAME-FILE"), "missing PAQ entry returns false");
	for (int i = 1; i < ResourcePaq::_entriesCount; ++i) {
		check(ResourcePaq::_entries[i - 1].hash < ResourcePaq::_entries[i].hash,
		      "PAQ hashes remain sorted for binary search");
	}
	check(Game::_monsterListLevel1[4] == 0xFF && Game::_monsterListLevel5_2[6] == 0xFF,
	      "monster list terminators retain their original bytes");
	check(Game::_gameLevels[2].cutscene_id == 0xFFFF, "missing level cutscene uses its sentinel");
	check(Game::_demoInputs[2].room == 0xFF, "demo starts in its original room");
	check(Video::_font8Jp[33] == 0xFF && Resource::_gameSavedSoundData[0] == 0xFF,
	      "embedded font and sample bytes retain their original values");

	// Exercise the memory stream used by rewind without changing its format.
	File stream;
	stream.openMemoryBuffer(4);
	stream.writeUint16BE(0x1234);
	stream.writeUint32LE(0x89ABCDEF);
	check(stream.size() == 6, "rewind buffer grows without losing bytes");
	stream.seek(0);
	check(stream.readUint16BE() == 0x1234, "big endian stream round trip");
	check(stream.readUint32LE() == 0x89ABCDEF, "little endian stream round trip");
	check(!stream.ioErr(), "successful stream round trip");
	uint8_t sentinel = 0xAA;
	check(stream.read(&sentinel, 1) == 0 && stream.ioErr(), "EOF sets stream error");
	check(sentinel == 0xAA, "EOF leaves destination unchanged");

	// A long debug message used to overflow the stack buffer. Sanitizers check this.
	g_debugMask = DBG_INFO;
	const std::string longMessage(4096, 'x');
	debug(DBG_INFO, "%s", longMessage.c_str());
	std::cout << (failures == 0 ? "Core checks passed\n" : "Core checks failed\n");
	return failures == 0 ? 0 : 1;
}
