// SPDX-License-Identifier: GPL-3.0-or-later
#include "config.h"
#include "decode_sega.h"
#include "fs.h"
#include "headless_system.h"
#include "mcd_player.h"
#include "mixer.h"
#include "wav_stream.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <algorithm>
Options g_options;
const char *g_caption = "Sega CD media tests";
static void check(bool value, const char *why) {
	if (!value)
		throw std::runtime_error(why);
}
static void be16(std::vector<uint8_t> &v, unsigned n) {
	v.push_back(n >> 8);
	v.push_back(n);
}
static void le16(std::vector<uint8_t> &v, unsigned n) {
	v.push_back(n);
	v.push_back(n >> 8);
}
static void le32(std::vector<uint8_t> &v, unsigned n) {
	le16(v, n);
	le16(v, n >> 16);
}
static void text(std::vector<uint8_t> &v, const char *s) {
	v.insert(v.end(), s, s + 4);
}
static void write(const std::filesystem::path &p, const std::vector<uint8_t> &v) {
	std::ofstream f(p, std::ios::binary);
	f.write(reinterpret_cast<const char *>(v.data()), v.size());
}
static std::vector<uint8_t> packed(size_t output, mcd::Codec codec, unsigned lengthBits = 4) {
	std::vector<uint8_t> literals, references;
	size_t produced = 0;
	while (produced < output) {
		const size_t at = literals.size();
		literals.push_back(0);
		for (int bit = 0; bit < 8 && produced < output; ++bit) {
			if (!produced || output - produced == 1) {
				literals[at] |= codec == mcd::Codec::Lsb ? 1 << bit : 128 >> bit;
				literals.push_back(0x22);
				++produced;
			} else {
				const unsigned count = std::min(output - produced, size_t((1u << lengthBits) + 1));
				be16(codec == mcd::Codec::Split ? references : literals, count - 2);
				produced += count;
			}
		}
	}
	if (codec == mcd::Codec::Split) {
		std::vector<uint8_t> result;
		be16(result, literals.size());
		result.insert(result.end(), literals.begin(), literals.end());
		result.insert(result.end(), references.begin(), references.end());
		return result;
	}
	return literals;
}
struct MediaSystem : HeadlessSystem {
	bool skip = false;
	int polls = 0;
	unsigned mixed = 0;
	uint32_t paused = 0, pendingPause = 0;
	uint32_t getPausedTime() const override { return paused; }
	void processEvents() override {
		++polls;
		// Model SDL focus loss: wall time advances while audio is suspended.
		clock += pendingPause;
		paused += pendingPause;
		pendingPause = 0;
		if (skip)
			_pi.cutsceneSkip = true;
	}
	void sleep(int ms) override {
		HeadlessSystem::sleep(ms);
		std::vector<int16_t> audio(size_t(ms * getOutputSampleRate() / 1000) * 2);
		if (audioCallback && !audio.empty()) {
			audioCallback(audioUser, audio.data(), audio.size() / 2);
			++mixed;
		}
	}
};
int main(int argc, char **argv) {
	if (argc != 2)
		return 1;
	try {
		const std::filesystem::path root(argv[1]);
		std::filesystem::create_directories(root);
		std::array<uint8_t, 32768> image{};
		std::array<uint8_t, 768> palette{};
		int n = 0;
		for (auto codec : {mcd::Codec::Lsb, mcd::Codec::Msb, mcd::Codec::Split}) {
			std::vector<uint8_t> frame(36);
			frame[32] = 1;
			frame[35] = 1;
			const auto bytes = packed(16384, codec);
			frame.insert(frame.end(), bytes.begin(), bytes.end());
			check(mcd::decodeFrame(frame.data(), frame.size(), codec, image.data(), palette.data()),
			      "valid movie compression");
			check(std::all_of(image.begin(), image.end(), [](uint8_t c) { return c == 2; }),
			      "movie tile placement");
			for (size_t cut : {size_t(0), size_t(35), frame.size() - 1})
				check(!mcd::decodeFrame(frame.data(), cut, codec, image.data(), palette.data()),
				      "truncated movie compression");
			const unsigned sectors = (frame.size() + 2047) / 2048;
			frame[33] = sectors;
			frame.resize(8192);
			frame.resize(10240, 0x20);
			const char *name = n == 0 ? "DEBUT.MCD" : n == 1 ? "BRIDGE.MCD" : "SPY.MCD";
			++n;
			write(root / name, frame);
		}
		std::vector<uint8_t> sprite(4);
		const auto data = packed(768, mcd::Codec::Split, 7);
		sprite.insert(sprite.end(), data.begin(), data.end());
		check(decodeSegaCDCharacter(sprite.data(), sprite.size(), image.data(), image.size()),
		      "CD sprite compression");
		check(!decodeSegaCDCharacter(sprite.data(), sprite.size() - 1, image.data(), image.size()),
		      "truncated CD sprite");
		sprite[4] = 0xff;
		check(!decodeSegaCDCharacter(sprite.data(), sprite.size(), image.data(), image.size()),
		      "CD sprite reference bounds");
		std::vector<uint8_t> wav;
		text(wav, "RIFF");
		le32(wav, 36 + 16);
		text(wav, "WAVE");
		text(wav, "fmt ");
		le32(wav, 16);
		le16(wav, 1);
		le16(wav, 2);
		le32(wav, 44100);
		le32(wav, 176400);
		le16(wav, 4);
		le16(wav, 16);
		text(wav, "data");
		le32(wav, 16);
		for (int i = 0; i < 4; ++i) {
			le16(wav, 1000);
			le16(wav, unsigned(-2000));
		}
		write(root / "track02.wav", wav);
		FileSystem files(argv[1]);
		WavStream wave;
		check(wave.open("track02.wav", &files), "valid PCM wave");
		std::array<int16_t, 40> sound{};
		check(wave.read(sound.data(), 20) == 20, "tiny WAV loops without stalling");
		check(sound[0] == 750 && sound[1] == -1500 && sound[38] == 750,
		      "stereo sample order and looping");
		std::filesystem::create_directories(root / "native");
		FileSystem native((root / "native").string().c_str());
		MediaSystem audioStub;
		Mixer background(&native, &audioStub);
		background.init();
		background.selectMusicSource(&files, kResourceTypeSega);
		background.playMusic(1);
		sound.fill(0);
		background.mix(sound.data(), sound.size() / 2);
		check(background._musicType == Mixer::MT_OGG && sound[0] == 750 && sound[1] == -1500,
		      "external CD WAV source and output-rate conversion");
		background.free();
		const auto validWave = wav;
		text(wav, "fmt ");
		le32(wav, 16);
		wav.insert(wav.end(), validWave.begin() + 20, validWave.begin() + 36);
		wav[4] += 24;
		write(root / "track02.wav", wav);
		check(!wave.open("track02.wav", &files), "duplicate WAV format chunk rejected");
		sound.fill(0);
		check(wave.read(sound.data(), 20) == 0 && sound[0] == 0,
		      "failed reopen must not retain a playable stream");
		wav = validWave;
		wav[24] = 0;
		wav[25] = 0;
		wav[26] = 0;
		wav[27] = 0;
		write(root / "track02.wav", wav);
		check(!wave.open("track02.wav", &files), "zero sample rate rejected");
		MediaSystem stub;
		Mixer mixer(&files, &stub);
		mixer.init();
		mcd::Player player(&stub, &mixer);
		check(player.play("DEBUT.MCD", &files, true) && player.completed && stub.mixed,
		      "movie completes with audio");
		const uint32_t normalDuration = stub.clock;
		const unsigned normalMixes = stub.mixed;
		stub.pendingPause = 4000;
		check(player.play("DEBUT.MCD", &files, true) && player.completed &&
		          stub.clock == normalDuration * 2 + 4000 && stub.mixed == normalMixes * 2,
		      "focus pause preserves complete movie duration and audio");
		const uint32_t before = stub.clock;
		check(player.play("DEBUT.MCD", &files, true) && player.completed &&
		          stub.clock - before == normalDuration,
		      "earlier pauses do not delay the next movie");
		stub.skip = true;
		check(player.play("DEBUT.MCD", &files, true) && !player.completed, "movie skip");
		check(!mixer.hasPremixHook(&player) && !stub._cutsceneSkipEnabled,
		      "movie hook/input cleanup");
		check(player.play("DEBUT.MCD", &files, false) && player.completed,
		      "skip preference respected");
		check(std::string(mcd::eventName(16, 0)) == "TKTELEC1" &&
		          std::string(mcd::eventName(60, 6)) == "TKRECEP2" && !mcd::eventName(0x40, 0),
		      "CD event variants");
		std::cout << "Sega CD bounded codecs, WAV and movie lifecycle passed\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
