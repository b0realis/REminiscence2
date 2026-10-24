// SPDX-License-Identifier: GPL-3.0-or-later
// Exercise the bundled audio implementations even in libmodplug-enabled builds.
#define USE_STB_VORBIS
#include "ogg_player.cpp"
#include "mod_player.cpp"
#include "input_state.h"
#include "decode_seq.h"
#include "object_data.h"
#include "seq_player.h"
#include "fs.h"
#include "tests/headless_system.h"
#include "game.h"
#include <filesystem>
#include <iostream>
Options g_options;
const char *g_caption = "Input/audio regression tests";
static int failures;
static void check(bool value, const char *what) {
	if (!value) {
		std::cerr << "FAIL: " << what << '\n';
		++failures;
	}
}
int main(int argc, char **argv) {
	if (argc != 2)
		return 1;
	std::filesystem::create_directories(argv[1]);
	settings::Preferences p;
	PlayerInput pi{};
	InputState input;
	input.key(SDL_SCANCODE_RIGHT, SDLK_RIGHT, true);
	input.publish(pi, p);
	input.axis(0, 0);
	input.publish(pi, p);
	check(pi.dirMask == PlayerInput::DIR_RIGHT, "neutral stick preserves held keyboard direction");
	input.button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT, true, false);
	input.publish(pi, p);
	input.key(SDL_SCANCODE_RIGHT, SDLK_RIGHT, false);
	input.publish(pi, p);
	check(pi.dirMask == PlayerInput::DIR_RIGHT, "keyboard release preserves D-pad");
	input.clear(pi);
	input.axis(0, -20000);
	input.axis(2, 0);
	input.publish(pi, p);
	check(pi.dirMask == PlayerInput::DIR_LEFT, "neutral right stick cannot cancel left stick");
	input.axis(0, 4600);
	input.publish(pi, p);
	check(pi.dirMask == PlayerInput::DIR_LEFT, "dead-zone hysteresis suppresses jitter");
	input.axis(0, 0);
	input.publish(pi, p);
	check(pi.dirMask == 0, "stick recentres");
	input.key(SDL_SCANCODE_LSHIFT, SDLK_LSHIFT, true);
	input.key(SDL_SCANCODE_RSHIFT, SDLK_LSHIFT, true);
	input.publish(pi, p);
	input.key(SDL_SCANCODE_LSHIFT, SDLK_LSHIFT, false);
	input.publish(pi, p);
	check(pi.shift, "second Shift source stays held");
	input.clearPad(pi, p);
	check(pi.shift, "unplugging pad preserves keyboard action");
	input.clear(pi);
	p.padButtons[5] = SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
	input.button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, true, false);
	input.publish(pi, p);
	check(pi.save, "remapped pad quick save");
	pi.save = false;
	input.axis(0, 0);
	input.publish(pi, p);
	check(!pi.save, "axis noise cannot repeat quick save");
	input.button(SDL_CONTROLLER_BUTTON_START, true, false);
	input.publish(pi, p);
	check(pi.escape, "Start always opens settings");
	std::string reason;
	check(!settings::validPadBinding(SDL_CONTROLLER_BUTTON_DPAD_UP, p, 0, false, reason),
	      "D-pad movement stays reserved");
	check(!settings::validPadBinding(SDL_CONTROLLER_BUTTON_A, p, 0, false, reason),
	      "duplicate pad bindings rejected");
	input.clear(pi);
	p.padButtons[0] = 32;
	input.axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT, 20000);
	input.publish(pi, p);
	check(pi.shift, "left trigger can hold a gameplay action");
	input.axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT, 12000);
	input.publish(pi, p);
	check(pi.shift, "trigger hysteresis rejects threshold noise");
	input.axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT, 0);
	input.publish(pi, p);
	check(!pi.shift, "trigger release releases action");
	input.clear(pi);
	p.stickyAction = true;
	input.key(SDL_SCANCODE_LSHIFT, SDLK_LSHIFT, true);
	input.publish(pi, p, true);
	input.key(SDL_SCANCODE_LSHIFT, SDLK_LSHIFT, false);
	input.publish(pi, p, true);
	check(pi.shift, "Sticky Action survives physical release");
	input.key(SDL_SCANCODE_LSHIFT, SDLK_LSHIFT, true);
	input.publish(pi, p, true);
	check(!pi.shift, "second press toggles Action off");
	input.clear(pi);
	p.stickyAction = false;
	p.inputBuffer = true;
	input.key(SDL_SCANCODE_UP, SDLK_UP, true);
	input.publish(pi, p, true, 10);
	input.key(SDL_SCANCODE_UP, SDLK_UP, false);
	input.publish(pi, p, true, 20);
	check(pi.dirMask & PlayerInput::DIR_UP, "100 ms buffer retains a short jump tap");
	input.publish(pi, p, true, 109);
	check(pi.dirMask & PlayerInput::DIR_UP, "buffer inside deadline");
	input.publish(pi, p, true, 110);
	check(!pi.dirMask, "buffer expires without new physical events");
	input.key(SDL_SCANCODE_RETURN, SDLK_RETURN, true);
	input.publish(pi, p, true, 200);
	input.clear(pi);
	input.publish(pi, p, true, 205);
	check(!pi.enter, "pause clear cancels buffered Use");
	check(std::string(settings::padButtonName(0, 1)) == "Cross" &&
	          std::string(settings::padButtonName(32)) == "LT / L2",
	      "controller-aware labels include triggers");
	journal::State journal;
	journal.chapter(0);
	check(journal.entries().size() == 1 && journal.unread(),
	      "journal reveals only reached chapter");
	journal.markRead(0);
	check(!journal.unread(), "journal read status");
	journal.chapter(6);
	check(journal.entries().size() == 2, "level jump does not reveal intervening chapters");
	{
		OggDecoder_impl decoder;
		decoder._decodedSamplesLen = 6;
		for (int i = 0; i < 6; ++i)
			decoder._decodedSamples[0][i] = decoder._decodedSamples[1][i] = (i + 1) * 100;
		int16_t a[4]{}, b[4]{}, c[4]{};
		decoder.read(a, 2);
		decoder.read(b, 2);
		decoder.read(c, 2);
		check(a[0] == 75 && b[0] == 225 && c[0] == 375 && !decoder._decodedSamplesLen,
		      "OGG remainder is consumed once across arbitrary callback sizes");
	}
	{
		File file;
		file.openMemoryBuffer(2108);
		std::vector<uint8_t> bytes(2108);
		bytes[0] = 'A';
		bytes[950] = 1;
		memcpy(bytes.data() + 1080, "M.K.", 4);
		file.write(bytes.data(), bytes.size());
		file.seek(0);
		ModPlayer_impl mod;
		mod.init(22050);
		check(mod.load(&file), "valid silent internal MOD");
		int16_t samples[16]{};
		mod.mix(samples, 8);
		mod.unload();
		mod.init(22050);
		bytes[1086] = 15;
		bytes[1087] = 150;
		File tempo;
		tempo.openMemoryBuffer(bytes.size());
		tempo.write(bytes.data(), bytes.size());
		tempo.seek(0);
		check(mod.load(&tempo), "MOD with tempo command");
		mod.mix(samples, 1);
		check(mod._songTempo == 150 && mod._samplesLeft == 366,
		      "tempo effect changes this tick with fractional timing");
		mod.unload();
		File output;
		check(output.open(ModPlayer::_names[0], "wb", argv[1]), "create synthetic MOD");
		output.write(bytes.data(), bytes.size());
		output.close();
		FileSystem files(argv[1]);
		HeadlessSystem platform;
		Mixer mixer(&files, &platform);
		mixer.init();
		mixer._mod._isAmiga = false;
		mixer.playMusic(0);
		check(mixer._mod._playing, "public MOD fallback starts with unspecified tempo");
		mixer.mix(samples, 8);
		mixer.stopMusic();
		mixer.free();
		File empty;
		empty.openMemoryBuffer(0);
		check(!mod.load(&empty), "empty MOD rejected");
		SeqDemuxer demux{};
		check(!demux.open(&empty), "empty SEQ rejected");
		demux.close();
	}
	{
		struct RatedSystem : HeadlessSystem {
			unsigned rate = 22050;
			uint32_t getOutputSampleRate() override { return rate; }
		} platform;
		FileSystem files(argv[1]);
		Mixer mixer(&files, &platform);
		for (unsigned rate : {8000, 22050, 44100, 48000, 192000}) {
			platform.rate = rate;
			SeqPlayer player(&platform, &mixer);
			player._audioCount = 1;
			player._audioReady = true;
			for (int i = 0; i < SeqDemuxer::kAudioBufferSize; ++i)
				player._audioBuffers[0][i] = i + 1;
			int16_t pcm[64]{};
			player.mix(pcm, 32);
			check(player._audioOffset == 32 * 22050 / rate && pcm[0] == 1,
			      "SEQ duration follows source rate, independent of output rate");
		}
		auto game = std::make_unique<Game>(&platform, &files, &files, argv[1], 0,
		                                   kResourceTypeAmiga, LANG_EN, kWidescreenNone, false, 0);
		game->_frameTimestamp = 0;
		for (int i = 0; i < 300; ++i)
			game->updateTiming();
		check(platform.clock == 10000, "30 Hz carries fractional milliseconds without drift");
		game->_preferences.gameSpeed = 50;
		game->_frameTimestamp = platform.clock;
		const auto start = platform.clock;
		for (int i = 0; i < 300; ++i)
			game->updateTiming();
		check(platform.clock - start == 20000,
		      "50 percent assist doubles reaction time without changing simulation steps");
	}
	{
		std::array<uint8_t, 256 * 224> pixels{};
		std::vector<uint8_t> frame(128 + 64, 0);
		frame[0] = 2;
		for (int i = 0; i < 64; ++i)
			frame[128 + i] = i + 1;
		check(decodeSeqFrame(frame.data(), frame.size(), pixels.data(), pixels.size()) &&
		          pixels[48 * 256] == 1 && pixels[55 * 256 + 7] == 64,
		      "SEQ literal block");
		for (size_t n = 0; n < frame.size(); ++n)
			check(!decodeSeqFrame(frame.data(), n, pixels.data(), pixels.size()),
			      "SEQ literal truncation rejected");
		frame.resize(129);
		frame[0] = 1;
		frame[128] = 0;
		check(!decodeSeqFrame(frame.data(), frame.size(), pixels.data(), pixels.size()),
		      "SEQ empty palette rejected");
		frame.assign(141, 0);
		frame[0] = 1;
		frame[128] = 0x81;
		for (int i = 0; i < 4; ++i)
			frame[129 + i] = 0x88;
		for (int i = 0; i < 8; ++i)
			frame[133 + i] = i + 1;
		check(decodeSeqFrame(frame.data(), frame.size(), pixels.data(), pixels.size()) &&
		          pixels[48 * 256] == 1 && pixels[55 * 256] == 8,
		      "SEQ bounded signed-run block");
		frame[128] = 0x82;
		check(decodeSeqFrame(frame.data(), frame.size(), pixels.data(), pixels.size()) &&
		          pixels[48 * 256 + 7] == 8 && pixels[55 * 256] == 1,
		      "SEQ transposed run block");
		DecodedObjects objects;
		const uint8_t object[] = {0, 0, 0, 4, 0, 1, 0, 1, 0, 0, 0, 0,
		                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		check(decodeObjects(object, sizeof(object), 1, 0, true, objects) &&
		          objects.nodes.size() == 1,
		      "bounded object parser");
		for (size_t n = 0; n < sizeof(object); ++n)
			check(!decodeObjects(object, n, 1, 0, true, objects), "object truncation rejected");
		check(!decodeObjects(object, sizeof(object), 256, 0, true, objects),
		      "object directory count bounded");
	}
	{
		// A complete synthetic movie exercises preroll, demux, presentation and
		// cleanup. No commercial SEQ data is required by this regression.
		std::vector<uint8_t> movie(102 * SeqDemuxer::kFrameSize, 0);
		movie[256] = 192; // One buffer: 128-byte op map + 64 literal bytes.
		for (int frame = 1; frame <= 100; ++frame)
			movie[frame * SeqDemuxer::kFrameSize + 4] = 255;
		const unsigned base = 101 * SeqDemuxer::kFrameSize;
		movie[base + 8] = 16;
		movie[base + 14] = 208;
		File file;
		file.openMemoryBuffer(movie.size());
		file.write(movie.data(), movie.size());
		file.seek(0);
		HeadlessSystem platform;
		FileSystem files(argv[1]);
		Mixer mixer(&files, &platform);
		SeqPlayer player(&platform, &mixer);
		std::array<uint8_t, 256 * 224> pixels{};
		player.setBackBuffer(pixels.data());
		check(player.play(&file) && player._completed, "complete synthetic SEQ plays");
		struct SkipSystem : HeadlessSystem {
			void processEvents() override { _pi.cutsceneSkip = true; }
		} skipPlatform;
		Mixer skipMixer(&files, &skipPlatform);
		SeqPlayer skipped(&skipPlatform, &skipMixer);
		skipped.setBackBuffer(pixels.data());
		file.seek(0);
		check(skipped.play(&file) && !skipped._completed && !skipPlatform._cutsceneSkipEnabled &&
		          !skipPlatform._pi.cutsceneSkip,
		      "keyboard-skipped SEQ stays incomplete and clears shortcut state");
		skipped._skipWithKeyboard = false;
		file.seek(0);
		check(skipped.play(&file) && skipped._completed, "disabled keyboard skip lets SEQ finish");
		movie[base + 5] = 31; // Invalid destination buffer must fall back safely.
		File corrupt;
		corrupt.openMemoryBuffer(movie.size());
		corrupt.write(movie.data(), movie.size());
		corrupt.seek(0);
		check(!player.play(&corrupt) && !player._completed,
		      "malformed SEQ fails without claiming completion");
	}
	std::cout << failures << " input/audio regression failures\n";
	return failures ? 1 : 0;
}
