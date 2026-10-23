// SPDX-License-Identifier: GPL-3.0-or-later
#include "music_midi.h"
#include "intern.h"
#include "music_prf.h"
#include "music_synth.h"
#include <algorithm>
#include <iostream>
#include <tuple>

Options g_options;
const char *g_caption = "music tests";
static int failures = 0;
static void check(bool result, const char *message) {
	if (!result) {
		std::cerr << "FAIL: " << message << '\n';
		++failures;
	}
}
static void le16(std::vector<uint8_t> &b, size_t p, uint16_t n) {
	b[p] = n;
	b[p + 1] = n >> 8;
}
static void le32(std::vector<uint8_t> &b, size_t p, uint32_t n) {
	le16(b, p, n);
	le16(b, p + 2, n >> 16);
}
static std::vector<uint8_t> midi() {
	std::vector<uint8_t> data = {'M', 'T', 'h', 'd', 0,   0,   0,   6, 0, 0, 0,
	                             1,   0,   96,  'M', 'T', 'r', 'k', 0, 0, 0, 0};
	const uint8_t notes[] = {0,  0x90, 60, 100, 2, 64, 80,   2,    0x80,
	                         60, 0,    0,  64,  0, 2,  0xFF, 0x2F, 0};
	data[21] = sizeof(notes);
	data.insert(data.end(), std::begin(notes), std::end(notes));
	return data;
}
class Capture final : public music::Synth {
public:
	int rendered = 0;
	std::vector<std::tuple<int, int, int, int>> messages;
	bool reset(int, std::string &) override {
		rendered = 0;
		messages.clear();
		return true;
	}
	void send(uint8_t status, uint8_t a, uint8_t b) override {
		messages.emplace_back(rendered, status, a, b);
	}
	void render(int16_t *samples, int frames) override {
		std::fill_n(samples, frames * 2, 42);
		rendered += frames;
	}
};
int main(int argc, char **argv) {
	auto bytes = midi();
	music::Song song;
	std::string error;
	check(music::parseMidi(bytes.data(), bytes.size(), song, error),
	      "parse Standard MIDI with running status");
	check(song.tracks.size() == 1 && song.tracks[0].events.size() == 4 &&
	          song.tracks[0].events[1].tick == 2 && song.tracks[0].endTick == 6,
	      "event delays and end-of-track preserved");
	for (size_t i = 0; i < bytes.size(); ++i) {
		music::Song broken;
		check(!music::parseMidi(bytes.data(), i, broken, error), "every truncated MIDI rejected");
	}
	auto bad = bytes;
	bad[25] = 0xF8;
	check(!music::parseMidi(bad.data(), bad.size(), song, error), "invalid MIDI status rejected");
	check(music::parseMidi(bytes.data(), bytes.size(), song, error),
	      "valid MIDI reusable after parse failure");
	std::vector<uint8_t> record(753);
	le32(record, 544, 1);
	le16(record, 548, 8);
	std::copy_n("test.mid", 8, record.begin() + 550);
	le32(record, 748, 2082);
	music::Prf prf;
	check(music::parsePrf(record.data(), record.size(), prf, error),
	      "bounded PRF clock and filename decoding");
	for (size_t i = 0; i < record.size(); ++i) {
		music::Prf broken;
		check(!music::parsePrf(record.data(), i, broken, error), "every truncated PRF rejected");
	}
	auto invalid = record;
	le16(invalid, 548, 0);
	check(!music::parsePrf(invalid.data(), invalid.size(), prf, error),
	      "zero timer modulus rejected");
	check(music::parsePrf(record.data(), record.size(), prf, error), "valid PRF reusable");
	record[715] = record[731] = record[747] = 0xFF;
	check(music::parsePrf(record.data(), record.size(), prf, error),
	      "unused PRF slots and nonzero loop flags retain their sentinel semantics");
	std::vector<uint8_t> instrument(80);
	le16(instrument, 78, 1);
	music::AdlibInstrument patch;
	check(music::parseInstrument(instrument.data(), instrument.size(), patch),
	      "bounded INS parsing");
	instrument[0] = 1;
	instrument[1] = 255;
	check(!music::parseInstrument(instrument.data(), instrument.size(), patch),
	      "invalid percussion channel rejected");

	std::array<music::AdlibInstrument, 16> patches{};
	auto first = std::make_unique<Capture>();
	auto *captureA = first.get();
	music::Sequencer whole;
	check(whole.start(song, prf, patches, std::move(first), false, 8000, error),
	      "start owned sequencer");
	std::vector<int16_t> output(8001 * 2);
	whole.render(output.data(), 8001);
	check(captureA->rendered == 8000 && !whole.playing(),
	      "fractional PRF clock produces exactly one second without drift");
	check(output[0] == 42 && output[15999] == 42 && output[16000] == 0,
	      "duration boundary clears remaining output");
	auto second = std::make_unique<Capture>();
	auto *captureB = second.get();
	music::Sequencer partitioned;
	check(partitioned.start(song, prf, patches, std::move(second), false, 8000, error),
	      "restart independent sequencer");
	for (int offset = 0; offset < 8001;) {
		const int count = std::min(37, 8001 - offset);
		partitioned.render(output.data(), count);
		offset += count;
	}
	check(captureA->messages == captureB->messages,
	      "event timing independent of audio callback partitioning");
	check(std::get<0>(captureA->messages[0]) == 0 && std::get<0>(captureA->messages[1]) == 7,
	      "MIDI deltas map to sample boundaries");

	music::Options options;
	auto synth = music::makeSynth(options, error);
	check(synth && synth->reset(44100, error), "AdLib backend initializes");
	patch = {};
	patch.operators[0][1] = patch.operators[1][1] = 1;
	patch.operators[0][3] = patch.operators[1][3] = 15;
	patch.operators[0][5] = patch.operators[1][5] = 1;
	patch.operators[0][7] = patch.operators[1][7] = 5;
	patch.operators[0][8] = 16;
	patch.operators[0][12] = 1;
	synth->instrument(0, patch);
	synth->send(0x90, 48, 100);
	output.resize(44100 * 2);
	synth->render(output.data(), 44100);
	check(std::any_of(output.begin(), output.end(), [](int16_t v) { return v != 0; }),
	      "AdLib instrument generates audible samples");
	synth->send(0xE0, 0, 0);
	synth->send(0xE0, 0, 64);
	synth->send(0x80, 48, 0);
	check(synth->reset(48000, error), "AdLib backend can reset to a different rate");
	if (music::backendAvailable(music::Backend::FluidSynth)) {
		options.backend = music::Backend::FluidSynth;
		options.soundFont.clear();
		auto fluid = music::makeSynth(options, error);
		for (int i = 0; i < 3; ++i) {
			check(!fluid->reset(44100, error), "missing soundfont fails safely on repeated reset");
		}
		if (argc > 1 && argv[1][0]) {
			options.soundFont = argv[1];
			fluid = music::makeSynth(options, error);
			check(fluid->reset(44100, error), "FluidSynth loads reference soundfont");
			fluid->send(0xC0, 0, 0);
			fluid->send(0x90, 60, 100);
			fluid->render(output.data(), 44100);
			check(std::any_of(output.begin(), output.begin() + 88200,
			                  [](int16_t v) { return v != 0; }),
			      "FluidSynth generates samples");
			check(fluid->reset(48000, error),
			      "FluidSynth repeated successful reset owns resources");
		}
	}
	if (music::backendAvailable(music::Backend::Mt32)) {
		options.backend = music::Backend::Mt32;
		options.romDirectory = "/reminiscence2-nonexistent-roms";
		auto mt32 = music::makeSynth(options, error);
		for (int i = 0; i < 3; ++i) {
			check(!mt32->reset(44100, error), "missing ROM pair fails safely on repeated reset");
		}
	}
	std::cout << (failures ? "Music checks failed\n" : "Music checks passed\n");
	return failures ? 1 : 0;
}
