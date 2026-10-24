// SPDX-License-Identifier: GPL-3.0-or-later
#include "cutscene_tool.h"
#include "cutscene_assets.h"
#include "mcd_player.h"
#include "game.h"
#include "systemstub.h"
#include "util.h"
#include <algorithm>
#include <cctype>
#include <set>

namespace cinematics {
namespace {
std::string lower(std::string value) {
	for (char &c : value) {
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}
	return value;
}

const char *title(const std::string &name) {
	static const char *const titles[][2] = {{"debut", "Conrad awakens"},
	                                        {"objet", "Objects and interactions"},
	                                        {"carte", "Key card"},
	                                        {"gen", "Generator"},
	                                        {"chute", "Falling"},
	                                        {"code", "Code"},
	                                        {"desinteg", "Disintegration"},
	                                        {"intro", "Introduction"},
	                                        {"intro1", "Introduction, first file"},
	                                        {"intro2", "Introduction, second file"},
	                                        {"strem", "Strem"},
	                                        {"holoseq", "Holocube message"},
	                                        {"carteid", "Identity card"},
	                                        {"pont", "Bridge"},
	                                        {"asc", "Elevator"},
	                                        {"map", "Travel map"},
	                                        {"metro", "Metro"},
	                                        {"missions", "Mission briefings"},
	                                        {"genmis", "Mission generator"},
	                                        {"memo", "Restoring Conrad's memory"},
	                                        {"taxi", "Taxi"},
	                                        {"accroche", "Hanging"},
	                                        {"voyage", "Journey to Earth"},
	                                        {"teleport", "Teleportation"},
	                                        {"lift", "Lift"},
	                                        {"espions", "The conspiracy"},
	                                        {"log", "Journal"},
	                                        {"fin", "Ending"},
	                                        {"genexp", "Generator explosion"},
	                                        {"logos", "Publisher logos"},
	                                        {"over", "Continue screen animation"},
	                                        {"score", "Score screen animation"},
	                                        {"serrure", "Lock"},
	                                        {"holocube", "Demo holocube"},
	                                        {"chute2", "Demo falling"},
	                                        {"logosssi", "SSI publisher logos"},
	                                        {"alimpont", "Bridge power"}};
	for (const auto &item : titles) {
		if (name == item[0]) {
			return item[1];
		}
	}
	return "Cutscene";
}

bool available(Resource &res, const char *name) {
	if (res.isAmiga()) {
		return res.fileExists((std::string(name) + ".CMP").c_str());
	}
	if (res.isMac()) {
		const auto stem = lower(name);
		return res._mac->findEntry((stem + " movie").c_str(), "PMOV") &&
		       res._mac->findEntry((stem + " polygons").c_str(), "POLY");
	}
	return res.fileExists((std::string(name) + ".CMD").c_str()) &&
	       res.fileExists((std::string(name) + ".POL").c_str());
}

void loadData(Resource &res, const char *name) {
	if (res.isAmiga()) {
		res.load(name, Resource::OT_CMP);
	} else if (res.isMac()) {
		res.MAC_loadCutscene(name);
	} else {
		res.load(name, Resource::OT_CMD);
		res.load(name, Resource::OT_POL);
	}
}

// Map raw file/part selections to the existing game event tables for music and
// the MEMO/ESPIONS corrections. No second copy of those edition tables is kept.
int gameEvent(const Resource &res, const std::string &name, unsigned part) {
	const auto *table = res.isAmiga() ? Cutscene::_offsetsTableAmiga : Cutscene::_offsetsTableDOS;
	const size_t count = res.isAmiga() ? Cutscene::_offsetsCountAmiga : Cutscene::_offsetsCountDOS;
	for (size_t id = 0; id < count; ++id) {
		const auto index = table[id * 2];
		if (index != 0xFFFF && table[id * 2 + 1] == part) {
			const char *stem = Cutscene::resourceName(index & 0xFF, res._type);
			if (stem && lower(stem) == name) {
				return static_cast<int>(id);
			}
		}
	}
	return -1;
}
} // namespace

bool parseSelection(const char *value, Selection &selection) {
	Selection parsed;
	if (!value) {
		return false;
	}
	const std::string text(value);
	const auto colon = text.find(':');
	parsed.name = lower(text.substr(0, colon));
	if (parsed.name.empty() || parsed.name.size() > 16 ||
	    parsed.name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") !=
	        std::string::npos) {
		return false;
	}
	if (colon != std::string::npos) {
		const auto digits = text.substr(colon + 1);
		if (digits.empty() || digits.size() > 5 ||
		    digits.find_first_not_of("0123456789") != std::string::npos) {
			return false;
		}
		parsed.part = static_cast<unsigned>(std::stoul(digits));
		if (parsed.part > 65535) {
			return false;
		}
	}
	selection = parsed;
	return true;
}

std::vector<Entry> catalog(Resource &res) {
	std::vector<Entry> result;
	if (res.isSegaCD()) {
		for (const auto &name : mcd::names())
			if (res.fileExists((name + ".MCD").c_str()))
				result.push_back({lower(name),
				                  name == "SC1"   ? "Opening (including logos)"
				                  : name == "SC2" ? "Ending and credits"
				                                  : name,
				                  0, 1});
		return result;
	}
	std::set<std::string> seen;
	for (size_t index = 0; index < Cutscene::_namesCount; ++index) {
		const char *stem = Cutscene::resourceName(index, res._type);
		const auto name = lower(stem);
		if (!seen.insert(name).second || !available(res, stem)) {
			continue;
		}
		loadData(res, stem);
		std::vector<size_t> offsets;
		std::string reason;
		if (res._polSize >= 20 &&
		    cutscenePartOffsets(res._cmd, res._cmdSize, res.isMac(), offsets, reason)) {
			result.push_back({name, title(name), static_cast<int>(index),
			                  static_cast<unsigned>(offsets.size())});
		} else {
			warning("Skipping cutscene %s: %s", stem,
			        reason.empty() ? "truncated polygon header" : reason.c_str());
		}
		res.unload(Resource::OT_CMP);
	}
	std::sort(result.begin(), result.end(),
	          [](const Entry &a, const Entry &b) { return a.name < b.name; });
	return result;
}

void printCatalog(FILE *stream, const std::vector<Entry> &entries, const Resource &res) {
	const char *edition = res.isAmiga()                    ? "Amiga"
	                      : res.isMac()                    ? "Macintosh"
	                      : res.isSegaCD()                 ? "Sega CD (US)"
	                      : res.isSega()                   ? "Sega"
	                      : res._type == kResourceTypePC98 ? "PC-98"
	                                                       : "DOS";
	// All strings here come from our fixed catalog, never from asset filenames.
	fprintf(stream, "{\"schema\":1,\"edition\":\"%s\",\"cutscenes\":[", edition);
	for (size_t i = 0; i < entries.size(); ++i) {
		const auto &entry = entries[i];
		fprintf(stream, "%s{\"name\":\"%s\",\"title\":\"%s\",\"parts\":%u}", i ? "," : "",
		        entry.name.c_str(), entry.title.c_str(), entry.parts);
	}
	fputs("]}\n", stream);
}

bool play(Game &game, const Selection &selection) {
	const auto entries = catalog(game._res);
	const auto found = std::find_if(entries.begin(), entries.end(), [&](const Entry &entry) {
		return entry.name == selection.name;
	});
	if (found == entries.end() || selection.part >= found->parts) {
		fprintf(stderr,
		        "Cutscene '%s:%u' is unavailable. Use --list-cutscenes to see this edition's files "
		        "and parts.\n",
		        selection.name.c_str(), selection.part);
		return false;
	}
	if (game._res.isSegaCD()) {
		game._mix.init();
		mcd::Player player(game._stub, &game._mix);
		const bool ok = player.play((selection.name + ".MCD").c_str(), game._fs,
		                            game._preferences.skipCutscenesWithKeyboard,
		                            [&] { return game._stub->_pi.escape; });
		game._mix.free();
		return ok;
	}
	const int event = gameEvent(game._res, selection.name, selection.part);
	game._cut._id = event >= 0 ? event : 0xFFFF;
	game._cut._creditsSequence = false;
	game._cut._allowEscape = true;
	game._cut._skipWithKeyboard = selection.name == "logos" || selection.name == "logosssi"
	                                  ? game._preferences.skipLogosWithKeyboard
	                                  : game._preferences.skipCutscenesWithKeyboard;
	game._cut._textCurBuf = nullptr;
	memset(game._vid._frontLayer, 0, game._vid._layerSize);
	memset(game._vid._backLayer, 0, game._vid._layerSize);
	memset(game._vid._tempLayer, 0, game._vid._layerSize);
	memset(game._vid._tempLayer2, 0, game._vid._layerSize);
	memset(game._cut._palBuf, 0, sizeof(game._cut._palBuf));
	game._cut.prepare();
	if (!game._cut.load(found->resourceIndex)) {
		return false;
	}
	game._mix.init();
	game._mix._mod._isAmiga = game._res.isAmiga();
	game._mix.playCutsceneMusic(event, game._res.isAmiga());
	game._cut.mainLoop(selection.part);
	game._mix.stopMusic();
	game._mix.free();
	game._cut.unload();
	return true;
}
} // namespace cinematics
