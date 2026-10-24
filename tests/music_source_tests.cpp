// SPDX-License-Identifier: GPL-3.0-or-later
#include "config.h"
#include "fs.h"
#include "mixer.h"
#include "headless_system.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

Options g_options;
const char *g_caption = "soundtrack tests";
static void check(bool condition, const char *message) {
	if (!condition)
		throw std::runtime_error(message);
}
static bool audible(Mixer &mixer) {
	std::vector<int16_t> samples(2048);
	bool result = false;
	for (int i = 0; i < 50; ++i) {
		std::fill(samples.begin(), samples.end(), 0);
		mixer.mix(samples.data(), 1024);
		result |= std::any_of(samples.begin(), samples.end(), [](int16_t n) { return n != 0; });
	}
	return result;
}
int main(int argc, char **argv) {
	if (argc != 3 && argc != 5)
		return 1;
	try {
		namespace fs = std::filesystem;
		setDefaultOptions(g_options);
		const fs::path root(argv[1]);
		for (const char *folder : {"native", "external", "empty"})
			fs::create_directories(root / folder);
		std::vector<uint8_t> module(2108 + 64);
		std::copy_n("Original test tone", 18, module.data());
		module[43] = module[49] = 32; // 64-byte sample and loop, big-endian word lengths.
		module[45] = 64;
		module[950] = 1;
		module[951] = 127;
		module[1084] = 1; // Sample 1, period 428 (C-3).
		module[1085] = 0xac;
		module[1086] = 0x10;
		for (int i = 0; i < 64; ++i)
			module[2108 + i] = i < 32 ? 60 : uint8_t(-60);
		std::copy_n("M.K.", 4, module.data() + 1080);
		for (const char *name : {"intro", "options"}) {
			std::ofstream out(root / "native" / name, std::ios::binary);
			out.write(reinterpret_cast<const char *>(module.data()), module.size());
		}
		for (const char *name : {"intro.ogg", "track03.ogg"})
			fs::copy_file(argv[2], root / "external" / name, fs::copy_options::overwrite_existing);
		fs::copy_file(fs::path(argv[2]).parent_path() / "sine-stereo.ogg",
		              root / "external/track04.ogg", fs::copy_options::overwrite_existing);
		FileSystem native((root / "native").string().c_str());
		FileSystem external((root / "external").string().c_str());
		FileSystem empty((root / "empty").string().c_str());
		HeadlessSystem platform;
		Mixer mixer(&native, &platform);
		mixer.configureMusic({}, &native);
		mixer.init();
		mixer.playMusic(0);
		check(mixer._musicType == Mixer::MT_MOD, "Native MOD did not start");
		mixer.selectMusicSource(&external, -1);
		mixer.playMusic(Mixer::MUSIC_TRACK + 3);
		check(mixer._musicType == Mixer::MT_OGG && audible(mixer),
		      "External level track is silent");
		mixer.playMusic(0);
		check(mixer._musicType == Mixer::MT_CUE && audible(mixer),
		      "Mapped external intro is silent");
		mixer.stopMusic();
		check(mixer._musicType == Mixer::MT_OGG && audible(mixer),
		      "Background did not resume after cue");
		mixer.playMusic(Mixer::MUSIC_TRACK + 4);
		std::vector<int16_t> resampled(22050 * 2);
		mixer.mix(resampled.data(), 22050);
		int crossings = 0;
		for (size_t i = 2; i < resampled.size(); i += 2)
			if (resampled[i - 2] <= 0 && resampled[i] > 0)
				++crossings;
		check(crossings >= 430 && crossings <= 450,
		      "44.1 kHz soundtrack changed pitch at 22.05 kHz output");
		check(resampled[100] != resampled[101], "Stereo soundtrack lost channel separation");
		mixer.playMusic(Mixer::MUSIC_TRACK + 8); // Absent from both sources.
		check(mixer._musicTrack == -1 && mixer._backgroundMusicType == Mixer::MT_NONE &&
		          mixer._musicType == Mixer::MT_NONE,
		      "Missing next-level track retained stale background state");
		mixer.playMusic(0);
		mixer.stopMusic();
		check(mixer._musicType == Mixer::MT_NONE && !audible(mixer),
		      "Cue resumed a discarded background decoder");
		mixer.selectMusicSource(&empty, kResourceTypeDOS);
		mixer._mod._isAmiga = true;
		mixer.playCutsceneMusic(0x0D, true);
		check(mixer._musicType == Mixer::MT_MOD && audible(mixer),
		      "Missing DOS override lost native Amiga intro");
		mixer.selectMusicSource(&empty, -1);
		mixer.playMusic(1);
		check(mixer._musicType == Mixer::MT_MOD, "Missing external track did not fall back");
		mixer.selectMusicSource(nullptr, -1);
		check(mixer._musicType == Mixer::MT_NONE && mixer._musicTrack == -1,
		      "Source change retained an old background hook");
		mixer.free();
		if (argc == 5) {
			FileSystem amiga(argv[3]), dos(argv[4]);
			Mixer cross(&dos, &platform);
			cross.configureMusic({}, &dos);
			cross.init();
			cross.selectMusicSource(&amiga, kResourceTypeAmiga);
			cross.playMusic(0);
			check(cross._musicType == Mixer::MT_MOD && audible(cross),
			      "Amiga music with DOS data failed");
			cross.selectMusicSource(&dos, kResourceTypeDOS);
			cross.playMusic(0);
			check(cross._musicType == Mixer::MT_SOURCE_PRF && audible(cross),
			      "Selected DOS MIDI/PRF failed");
			cross.stopMusic();
			cross.playCutsceneMusic(5, true); // Silent on Amiga, DOS PRF cue 25.
			check(cross._musicType == Mixer::MT_SOURCE_PRF && audible(cross),
			      "DOS-only scene cue 25 was lost on Amiga graphics");
			cross.free();
		}
		std::cout << "Soundtrack selection, Ogg cues/backgrounds, native fallback and hook cleanup "
		             "passed.\n";
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 2;
	}
}
