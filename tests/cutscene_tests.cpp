// SPDX-License-Identifier: GPL-3.0-or-later
#include "cutscene_assets.h"
#include "cutscene_tool.h"
#include "config.h"
#include "fs.h"
#include "game.h"
#include "headless_system.h"
#include "screenshot.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>

Options g_options;
const char *g_caption = "cutscene tests";
using Bytes = std::vector<uint8_t>;
static int failures = 0;
static void check(bool condition, const char *message) {
	if (!condition) {
		std::cerr << "FAIL: " << message << '\n';
		++failures;
	}
}
static void be32(Bytes &bytes, uint32_t value) {
	bytes.push_back(value >> 24);
	bytes.push_back(value >> 16);
	bytes.push_back(value >> 8);
	bytes.push_back(value);
}
static void rawBlock(Bytes &bytes, const Bytes &data) {
	be32(bytes, 0U - static_cast<uint32_t>(data.size()));
	bytes.insert(bytes.end(), data.begin(), data.end());
}
static void save(const std::filesystem::path &path, const Bytes &bytes) {
	std::ofstream stream(path, std::ios::binary);
	stream.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
}

struct PlaybackSystem : HeadlessSystem {
	size_t events = 0, frames = 0;
	Bytes pixels;
	std::filesystem::path capture;
	void processEvents() override {
		// LOG and some gameplay panels wait for player input or loop by design.
		// Advance their prompts and exercise the ordinary skip path after a full
		// preview, rather than treating a deliberate loop as a decoder failure.
		if (events && events % 100 == 0) {
			_pi.enter = true;
		}
		if (frames > 2000) {
			_pi.backspace = true;
		}
		if (++events > 1000000) {
			_pi.quit = true;
		}
	}
	void copyRect(int x, int y, int w, int h, const uint8_t *source, int pitch) override {
		++frames;
		if (!capture.empty() && frames == 80 && x == 0 && y == 0) {
			pixels.resize(w * h);
			for (int row = 0; row < h; ++row) {
				std::copy_n(source + row * pitch, w, pixels.data() + row * w);
			}
			uint8_t colors[768];
			getPalette(colors, 256);
			check(saveBMP(capture.string().c_str(), pixels.data(), colors, w, h),
			      "capture actual rendered cutscene");
		}
		// Exercise the MOD mixer through its real platform boundary too.
		if (audioCallback) {
			int16_t samples[1024] = {};
			audioCallback(audioUser, samples, 512);
		}
	}
};

int main(int argc, char **argv) {
	if (argc < 2 || argc > 3) {
		return 2;
	}
	const std::filesystem::path root(argv[1]);
	std::filesystem::create_directories(root);
	setDefaultOptions(g_options);
	std::string reason;
	std::vector<size_t> parts;
	Bytes commands{0, 2, 0, 0, 0, 2, 0, 0x80, 0, 0x80};
	check(cutscenePartOffsets(commands.data(), commands.size(), false, parts, reason) &&
	          parts == std::vector<size_t>({6, 8}),
	      "relative part table");
	Bytes mac{0, 2, 0, 6, 0, 8, 0, 0x80, 0, 0x80};
	check(cutscenePartOffsets(mac.data(), mac.size(), true, parts, reason) &&
	          parts == std::vector<size_t>({6, 8}),
	      "absolute Mac part table");
	for (size_t length = 0; length < 9; ++length) {
		check(!cutscenePartOffsets(commands.data(), length, false, parts, reason) && parts.empty(),
		      "every truncated part table/body rejected");
	}
	mac[5] = 99;
	check(!cutscenePartOffsets(mac.data(), mac.size(), true, parts, reason),
	      "Mac part outside body rejected");
	mac[5] = 1;
	check(!cutscenePartOffsets(mac.data(), mac.size(), true, parts, reason),
	      "Mac part inside header rejected");
	Bytes polygons(20);
	Bytes cmp;
	rawBlock(cmp, polygons);
	rawBlock(cmp, commands);
	Bytes decodedPolygons, decodedCommands;
	check(decodeCutsceneCmp(cmp.data(), cmp.size(), decodedPolygons, decodedCommands, reason) &&
	          decodedPolygons == polygons && decodedCommands == commands,
	      "raw CMP blocks retain all bytes");
	for (size_t length = 0; length < cmp.size(); ++length) {
		check(!decodeCutsceneCmp(cmp.data(), length, decodedPolygons, decodedCommands, reason) &&
		          decodedPolygons.empty() && decodedCommands.empty(),
		      "truncated CMP never publishes a partial result");
	}
	Bytes invalid;
	be32(invalid, 0x80000000U);
	check(!decodeCutsceneCmp(invalid.data(), invalid.size(), decodedPolygons, decodedCommands,
	                         reason),
	      "INT32_MIN CMP length rejected without overflow");
	invalid.clear();
	be32(invalid, 12);
	be32(invalid, 0);
	be32(invalid, 1);
	be32(invalid, 2);
	check(!decodeCutsceneCmp(invalid.data(), invalid.size(), decodedPolygons, decodedCommands,
	                         reason),
	      "bad Bytekiller checksum rejected");

	cinematics::Selection selection;
	check(cinematics::parseSelection("INTRO:0", selection) && selection.name == "intro" &&
	          !selection.part,
	      "case-insensitive named selection");
	check(cinematics::parseSelection("objet:19", selection) && selection.part == 19,
	      "explicit part selection");
	for (const char *value :
	     {"", "../intro", "intro:", "intro:-1", "intro:65536", "intro:1junk", "intro:0:1"}) {
		check(!cinematics::parseSelection(value, selection), "invalid selection rejected");
	}
	// Large transformed ellipses in GENEXP used to overflow their incremental
	// error terms. Cover both the arithmetic and clipping at the layer boundary.
	Graphics graphics;
	Bytes layer(256 * 224 + 2, 0xAA);
	graphics.setLayer(layer.data() + 1, 256);
	graphics.setClippingRect(0, 0, 256, 224);
	Point center{128, 112};
	graphics.drawEllipse(0xC1, false, &center, 2000, 1000);
	check(layer.front() == 0xAA && layer.back() == 0xAA && layer[1 + 112 * 256 + 128] == 0xC1,
	      "large ellipse clips to its layer without integer overflow");
	// A cutscene-only folder suffices for the catalog. A menu bitmap CMP and a
	// missing DOS polygon partner must never appear as selectable animations.
	const auto fixture = root / "incomplete fixtures with spaces";
	std::filesystem::create_directories(fixture);
	save(fixture / "INTRO.CMP", cmp);
	save(fixture / "PRESENT.CMP", cmp);
	save(fixture / "DEBUT.CMD", commands);
	FileSystem files(fixture.string().c_str());
	Resource amiga(&files, kResourceTypeAmiga, LANG_EN);
	amiga.init();
	const auto catalog = cinematics::catalog(amiga);
	check(catalog.size() == 1 && catalog[0].name == "intro" && catalog[0].parts == 2,
	      "catalog uses actual assets and part counts, excluding menu bitmaps");
	Resource dos(&files, kResourceTypeDOS, LANG_EN);
	dos.init();
	check(cinematics::catalog(dos).empty(), "incomplete DOS pair excluded");
	const auto completeFixture = root / "complete DOS fixtures";
	std::filesystem::create_directories(completeFixture);
	save(completeFixture / "DEBUT.CMD", commands);
	save(completeFixture / "DEBUT.POL", polygons);
	FileSystem completeFiles(completeFixture.string().c_str());
	Resource complete(&completeFiles, kResourceTypeDOS, LANG_EN);
	complete.init();
	const auto dosCatalog = cinematics::catalog(complete);
	check(dosCatalog.size() == 1 && dosCatalog[0].name == "debut" && dosCatalog[0].parts == 2,
	      "complete DOS pair cataloged");

	if (argc == 3) {
		FileSystem original(argv[2]);
		PlaybackSystem platform;
		auto game = std::make_unique<Game>(&platform, &original, &original, argv[1], 0,
		                                   kResourceTypeAmiga, LANG_EN, kWidescreenNone, false, 0);
		game->initCutsceneResources();
		const auto actual = cinematics::catalog(game->_res);
		check(!actual.empty(), "original Amiga cutscenes found");
		size_t played = 0, skipped = 0;
		for (const auto &entry : actual) {
			for (unsigned part = 0; part < entry.parts; ++part) {
				std::cout << "Playing " << entry.name << ':' << part << std::endl;
				platform.events = platform.frames = 0;
				platform.capture = root / (entry.name + '-' + std::to_string(part) + ".bmp");
				check(cinematics::play(*game, {entry.name, part}),
				      "actual cutscene playback succeeds");
				check(!platform._pi.quit, "cutscene finishes within instruction budget");
				if (platform._pi.quit) {
					return 1;
				}
				if (game->_cut._interrupted) {
					++skipped;
				}
				++played;
			}
		}
		std::cout << "Rendered " << actual.size() << " original Amiga files / " << played
		          << " parts (" << skipped << " previews ended via skip).\n";
	}
	std::cout << (failures ? "Cutscene checks failed\n" : "Cutscene checks passed\n");
	return failures ? 1 : 0;
}
