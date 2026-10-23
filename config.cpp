/*
 * Configuration handling adapted from REminiscence.
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 * New REminiscence2 contributions are licensed under GPL-3.0-or-later.
 * Inherited portions retain their original licensing terms.
 */
#include <ctype.h>
#include <string>
#include "config.h"
#include "util.h"

void setDefaultOptions(Options &options) {
	options = {};
	options.bypass_protection = true;
	options.enable_password_menu = true;
	options.fade_out_palette = true;
	options.use_seq_cutscenes = true;
	options.restore_memo_cutscene = true;
 options.use_prf_music = true;
}

static std::string trim(const std::string &value) {
	const size_t first = value.find_first_not_of(" \t\r\n");
	if (first == std::string::npos) {
		return {};
	}
	return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}

bool loadConfiguration(const char *filename, Options &options) {
	FILE *fp = fopen(filename, "rb");
	if (!fp) {
		return false;
	}
	const struct {
		const char *name;
		bool *value;
	} entries[] = {
		{ "bypass_protection", &options.bypass_protection },
		{ "enable_password_menu", &options.enable_password_menu },
		{ "enable_language_selection", &options.enable_language_selection },
		{ "fade_out_palette", &options.fade_out_palette },
		{ "use_tile_data", &options.use_tile_data },
		{ "use_text_cutscenes", &options.use_text_cutscenes },
		{ "use_seq_cutscenes", &options.use_seq_cutscenes },
		{ "use_words_protection", &options.use_words_protection },
		{ "use_white_tshirt", &options.use_white_tshirt },
		{ "play_asc_cutscene", &options.play_asc_cutscene },
		{ "play_caillou_cutscene", &options.play_caillou_cutscene },
		{ "play_metro_cutscene", &options.play_metro_cutscene },
		{ "play_serrure_cutscene", &options.play_serrure_cutscene },
		{ "play_carte_cutscene", &options.play_carte_cutscene },
		{ "play_gamesaved_sound", &options.play_gamesaved_sound },
		{ "restore_memo_cutscene", &options.restore_memo_cutscene },
		{ "order_inventory_original", &options.order_inventory_original },
		{ "fix_fmopl_e0_reg", &options.fix_fmopl_e0_reg },
  { "use_prf_music", &options.use_prf_music },
	};
	std::string line;
	int lineNumber = 0;
	auto parseLine = [&]() {
		++lineNumber;
		const std::string text = trim(line);
		if (text.empty() || text[0] == '#') {
			return;
		}
		const size_t separator = text.find('=');
		if (separator == std::string::npos) {
			warning("%s:%d: Expected name=value", filename, lineNumber);
			return;
		}
		const std::string name = trim(text.substr(0, separator));
		std::string value = trim(text.substr(separator + 1));
		for (char &c : value) {
			c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
		}
		for (const auto &entry : entries) {
			if (name == entry.name) {
				if (value == "true" || value == "1") {
					*entry.value = true;
				} else if (value == "false" || value == "0") {
					*entry.value = false;
				} else {
					warning("%s:%d: Invalid boolean for '%s'", filename, lineNumber, name.c_str());
				}
				return;
			}
		}
		warning("%s:%d: Unhandled option '%s', ignoring", filename, lineNumber, name.c_str());
	};
	int c;
	while ((c = fgetc(fp)) != EOF) {
		if (c == '\n') {
			parseLine();
			line.clear();
		} else {
			line += static_cast<char>(c);
		}
	}
	if (!line.empty()) {
		parseLine();
	}
	const bool success = !ferror(fp);
	fclose(fp);
	return success;
}
