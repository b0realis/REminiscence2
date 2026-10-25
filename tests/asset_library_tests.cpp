// SPDX-License-Identifier: GPL-3.0-or-later
#include "asset_library.h"
#include "fs.h"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <iostream>
#include <stdexcept>

const char *g_caption = "asset library tests";

static void check(bool condition, const char *message) {
	if (!condition)
		throw std::runtime_error(message);
}
int main(int argc, char **argv) {
	if (argc != 2)
		return 1;
	try {
		namespace fs = std::filesystem;
		const auto root =
		    fs::absolute(fs::u8path(argv[1])) /
		    std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
		auto put = [&](const fs::path &path, const std::string &text = "fixture") {
			fs::create_directories(path.parent_path());
			std::ofstream(path, std::ios::binary) << text;
		};
		const auto libraryRoot = root / "library";
		put(libraryRoot / "amiga/data/LEVEL1.LEV");
		put(libraryRoot / "amiga/FONT8.SPR");
		put(libraryRoot / "amiga/music/intro");
		put(libraryRoot / "amiga/SHARED.DAT", "Amiga");
		put(libraryRoot / "gog/Flashback_Data/StreamingAssets/data/level1.map");
		put(libraryRoot / "gog/Flashback_Data/StreamingAssets/data/level1.lev");
		put(libraryRoot / "gog/Flashback_Data/StreamingAssets/data/option3.prf");
		put(libraryRoot / "gog/Flashback_Data/StreamingAssets/data/shared.dat", "DOS");
		put(libraryRoot / "mac/FLASHBACK.RSRC");
		put(libraryRoot / "packed/data/level1.lev", "RNC\1packed");
		put(libraryRoot / "packed/music/intro", "RNC\1packed");
		put(libraryRoot / "music/Sega CD/track02.ogg");
		const auto library = assets::discover(libraryRoot.u8string());
		check(library.games.size() == 3, "Mixed LEV marker or packed WHDLoad detection");
		check(library.music.size() == 3, "Native music and external soundtrack discovery");
		const int amiga = assets::find(library.games, "amiga");
		const int dos = assets::find(library.games, "dos");
		check(amiga >= 0 && dos >= 0, "Edition shorthand selection");
		FileSystem amigaFiles(library.games[amiga].root.c_str(),
		                      library.games[amiga].excludedRoots);
		FileSystem dosFiles(library.games[dos].root.c_str(), library.games[dos].excludedRoots);
		check(amigaFiles.exists("font8.spr") && !amigaFiles.exists("level1.map"),
		      "Amiga sibling font or isolation");
		check(dosFiles.exists("level1.map") && !dosFiles.exists("font8.spr"), "DOS isolation");
		char *path = dosFiles.findPath("SHARED.DAT");
		check(path && fs::u8path(path).parent_path().filename() == "data",
		      "Duplicate name crossed edition boundary");
		free(path);
		put(libraryRoot / "segacd/DATA/PRESENT.LEV");
		put(libraryRoot / "segacd/SC1.MCD");
		put(libraryRoot / "segacd/SUB.BIN");
		put(libraryRoot / "segacd/music/track02.wav");
		const auto waveLibrary = assets::discover(libraryRoot.u8string());
		const int waveSega = assets::find(waveLibrary.games, "sega:segacd");
		check(waveSega >= 0, "CD WAV library root");
		fs::remove(libraryRoot / "segacd/music/track02.wav");
		put(libraryRoot / "segacd/music/track02.ogg");
		put(libraryRoot / "amiga/music/track02.ogg");
		const auto oggLibrary = assets::discover(libraryRoot.u8string());
		const int oggSega = assets::find(oggLibrary.games, "sega:segacd");
		check(oggSega >= 0 && oggLibrary.games[oggSega].label == waveLibrary.games[waveSega].label,
		      "Converting native CD music changed the edition/root");
		check(assets::find(oggLibrary.music, "sega:segacd") >= 0 &&
		          assets::find(oggLibrary.music, "music:segacd/music") < 0,
		      "Converted CD soundtrack detached from its game");
		const auto &sega = oggLibrary.games[oggSega];
		FileSystem segaFiles(sega.root.c_str(), sega.excludedRoots);
		check(segaFiles.exists("SC1.MCD") && segaFiles.exists("SUB.BIN") &&
		          segaFiles.exists("track02.ogg") && !segaFiles.exists("FONT8.SPR"),
		      "Ogg CD edition lost sibling resources or crossed edition boundary");
		check(assets::find(oggLibrary.games, "amiga:amiga") >= 0 &&
		          assets::find(oggLibrary.music, "music:music/Sega CD") >= 0,
		      "Native Ogg music swallowed an external soundtrack or lost Amiga siblings");
		put(root / "nested/level1.map");
		put(root / "nested/amiga/level1.lev");
		put(root / "nested/amiga/only-amiga.dat");
		put(root / "nested/music/Modules/intro");
		put(root / "nested/music/Disc/track02.ogg");
		put(root / "nested/packed/level1.lev", "RNC\1packed");
		put(root / "nested/packed/only-packed.dat");
		const auto nested = assets::discover((root / "nested").u8string());
		const auto &parent = nested.games[assets::find(nested.games, "dos")];
		FileSystem isolated(parent.root.c_str(), parent.excludedRoots);
		check(!isolated.exists("only-amiga.dat") && !isolated.exists("only-packed.dat") &&
		          !isolated.exists("track02.ogg") && !isolated.exists("intro"),
		      "Nested edition was not excluded");
		check(assets::find(nested.music, "music:music/Modules") >= 0 &&
		          assets::find(nested.music, "music:music/Disc") >= 0,
		      "Explicit soundtrack packs inside a parent game were not discoverable");
		put(libraryRoot / "second-amiga/level1.lev");
		const auto duplicate = assets::discover(libraryRoot.u8string());
		check(assets::find(duplicate.games, "amiga") < 0 &&
		          assets::find(duplicate.games, "amiga:amiga") >= 0,
		      "Ambiguous shorthand must require a full ID");
		fs::remove_all(root);
		std::cout
		    << "Asset discovery, nested layouts, collision isolation and music folders passed.\n";
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 2;
	}
}
