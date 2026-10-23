// SPDX-License-Identifier: GPL-3.0-or-later
// PRF instrument semantics researched in REminiscence by Gregory Montoir.
#include "music_synth.h"
#include "third_party/nuked-opl3/opl3.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#ifdef REMINISCENCE2_FLUIDSYNTH
#include <fluidsynth.h>
#endif
#ifdef REMINISCENCE2_MT32EMU
#define MT32EMU_API_TYPE 1
#include <mt32emu/mt32emu.h>
#endif

namespace music {
namespace {
class Adlib final : public Synth {
	struct Voice {
		int track = -1, note = 0;
		AdlibInstrument patch;
	};
	opl3_chip _chip{};
	std::array<Voice, 11> _voices{};
	std::array<AdlibInstrument, 16> _patches{};
	std::array<int, 16> _bends{};
	uint8_t _rhythm = 0x20;
	bool _correctWaveform;
	void write(unsigned reg, unsigned value) { OPL3_WriteRegBuffered(&_chip, reg, value); }
	static int physical(int voice) { return voice == 9 ? 8 : voice == 10 ? 7 : voice; }
	void frequency(int voice, bool key) {
		static const int notes[] = {0x157, 0x16C, 0x181, 0x198, 0x1B1, 0x1CB,
		                            0x1E6, 0x203, 0x222, 0x243, 0x266, 0x28A};
		const auto &v = _voices[voice];
		int block = std::clamp(v.note / 12, 0, 7);
		const double semitones = (_bends[v.track] - 8192) / 8192.0;
		int fnum = std::lround(notes[v.note % 12] * std::pow(2.0, semitones / 12.0));
		if (fnum > 1023 && block < 7) {
			fnum /= 2;
			++block;
		}
		const int channel = physical(voice);
		write(0xA0 + channel, fnum & 255);
		write(0xB0 + channel, (key ? 0x20 : 0) | (block << 2) | ((fnum >> 8) & 3));
	}
	void keyOff(int voice) {
		if (_voices[voice].track < 0) {
			return;
		}
		if (voice >= 6) {
			_rhythm &= ~(1 << (10 - voice));
			write(0xBD, _rhythm);
		} else {
			frequency(voice, false);
		}
		_voices[voice].track = -1;
	}
	void setup(int voice, const AdlibInstrument &patch, int velocity) {
		static const int slots[11][2] = {{0, 3},   {1, 4},   {2, 5},   {8, 11},  {9, 12}, {10, 13},
		                                 {16, 19}, {20, 20}, {18, 18}, {21, 21}, {17, 17}};
		const int volume = velocity * 48 / 64;
		for (int index = 0; index < 2; ++index) {
			if (index == 0 && voice > 6) {
				continue;
			}
			const auto &op = patch.operators[index];
			const int slot = slots[voice][index];
			write(0x20 + slot, (op[9] ? 0x80 : 0) | (op[10] ? 0x40 : 0) | (op[5] ? 0x20 : 0) |
			                       (op[11] ? 0x10 : 0) | (op[1] & 15));
			const int level = op[8] & 63;
			const int attenuation =
			    (index == 0 && op[12]) ? level : 63 - ((63 - level) * volume + 63) / 127;
			write(0x40 + slot, std::clamp(attenuation, 0, 63) | ((op[0] & 3) << 6));
			write(0x60 + slot, ((op[3] & 15) << 4) | (op[6] & 15));
			write(0x80 + slot, ((op[4] & 15) << 4) | (op[7] & 15));
			write((_correctWaveform ? 0xE0 : 0x0E) + slot,
			      index ? patch.carrierWave : patch.modWave);
		}
		if (voice <= 6) {
			const auto &op = patch.operators[0];
			write(0xC0 + voice, ((op[2] & 7) << 1) | (op[12] ? 0 : 1));
		}
	}

public:
	explicit Adlib(bool waveform) : _correctWaveform(waveform) {}
	bool reset(int rate, std::string &) override {
		if (rate < 8000 || rate > 192000) {
			return false;
		}
		OPL3_Reset(&_chip, rate);
		_voices = {};
		_bends.fill(8192);
		_rhythm = 0x20;
		write(1, 0x20);
		write(8, 0x40);
		write(0xBD, _rhythm);
		return true;
	}
	void instrument(int track, const AdlibInstrument &patch) override {
		if (track >= 0 && track < 16) {
			_patches[track] = patch;
		}
	}
	void send(uint8_t status, uint8_t first, uint8_t second) override {
		const unsigned track = status & 15, kind = status & 0xF0;
		if (kind == 0x80 || (kind == 0x90 && !second)) {
			for (int i = 0; i < 11; ++i) {
				if (_voices[i].track == int(track) && _voices[i].note == first) {
					keyOff(i);
				}
			}
		} else if (kind == 0x90) {
			const auto &patch = _patches[track];
			int voice = -1;
			if (patch.mode) {
				if (patch.channel >= 6 && patch.channel <= 10 && _voices[patch.channel].track < 0) {
					voice = patch.channel;
				}
			} else {
				for (int i = 0; i < 6; ++i) {
					if (_voices[i].track < 0) {
						voice = i;
						break;
					}
				}
			}
			if (voice < 0) {
				return;
			}
			_voices[voice] = {int(track), std::min<int>(first, 95), patch};
			setup(voice, patch, second);
			if (voice >= 6) {
				_rhythm &= ~(1 << (10 - voice));
				write(0xBD, _rhythm);
				frequency(voice, false);
				_rhythm |= 1 << (10 - voice);
				write(0xBD, _rhythm);
			} else {
				write(0xB0 + voice, 0);
				frequency(voice, true);
			}
		} else if (kind == 0xE0) {
			_bends[track] = first | (int(second) << 7);
			for (int i = 0; i < 11; ++i) {
				if (_voices[i].track == int(track)) {
					frequency(i, i < 6);
				}
			}
		} else if (kind == 0xB0 && (first == 120 || first == 123)) {
			for (int i = 0; i < 11; ++i) {
				if (_voices[i].track == int(track)) {
					keyOff(i);
				}
			}
		}
	}
	void render(int16_t *samples, int frames) override {
		OPL3_GenerateStream(&_chip, samples, frames);
	}
};

#ifdef REMINISCENCE2_FLUIDSYNTH
// MT-32 patch numbers mapped to General MIDI for user-supplied GM soundfonts.
// This approximates the instrumentation; it does not emulate an MT-32.
constexpr uint8_t mt32ToGm[128] = {
    0,   1,   0,   2,   4,   4,   5,   3,   16, 17,  18,  16,  16, 19, 20, 21, 6,  6,  6,
    7,   7,   7,   8,   112, 62,  62,  63,  63, 38,  38,  39,  39, 88, 95, 52, 98, 97, 99,
    14,  54,  102, 96,  53,  102, 81,  100, 14, 80,  48,  48,  49, 45, 41, 40, 42, 42, 43,
    46,  45,  24,  25,  28,  27,  104, 32,  32, 34,  33,  36,  37, 35, 35, 79, 73, 72, 72,
    74,  75,  64,  65,  66,  67,  71,  71,  68, 69,  70,  22,  56, 59, 57, 57, 60, 60, 58,
    61,  61,  11,  11,  98,  14,  9,   14,  13, 12,  107, 107, 77, 78, 78, 76, 76, 47, 117,
    127, 118, 118, 116, 115, 119, 115, 112, 55, 124, 123, 0,   14, 117};
class Fluid final : public Synth {
	using Settings = std::unique_ptr<fluid_settings_t, decltype(&delete_fluid_settings)>;
	using Engine = std::unique_ptr<fluid_synth_t, decltype(&delete_fluid_synth)>;
	Settings _settings{nullptr, delete_fluid_settings};
	Engine _engine{nullptr, delete_fluid_synth};
	std::string _font;

public:
	explicit Fluid(std::string font) : _font(std::move(font)) {}
	bool reset(int rate, std::string &error) override {
		_engine.reset();
		_settings.reset();
		if (_font.empty()) {
			error = "FluidSynth requires --soundfont=PATH";
			return false;
		}
		Settings settings(new_fluid_settings(), delete_fluid_settings);
		if (!settings) {
			error = "Cannot allocate FluidSynth settings";
			return false;
		}
		fluid_settings_setnum(settings.get(), "synth.sample-rate", rate);
		fluid_settings_setnum(settings.get(), "synth.gain", 0.6);
		fluid_settings_setint(settings.get(), "synth.threadsafe-api", 0);
		Engine engine(new_fluid_synth(settings.get()), delete_fluid_synth);
		if (!engine || fluid_synth_sfload(engine.get(), _font.c_str(), 1) < 0) {
			error = "Cannot load soundfont: " + _font;
			return false;
		}
		_settings = std::move(settings);
		_engine = std::move(engine);
		return true;
	}
	void send(uint8_t status, uint8_t a, uint8_t b) override {
		if (!_engine) {
			return;
		}
		auto *s = _engine.get();
		const unsigned ch = status & 15;
		switch (status & 0xF0) {
		case 0x80:
			fluid_synth_noteoff(s, ch, a);
			break;
		case 0x90:
			fluid_synth_noteon(s, ch, a, b);
			break;
		case 0xA0:
			fluid_synth_key_pressure(s, ch, a, b);
			break;
		case 0xB0:
			fluid_synth_cc(s, ch, a, b);
			break;
		case 0xC0:
			fluid_synth_program_change(s, ch, mt32ToGm[a & 127]);
			break;
		case 0xD0:
			fluid_synth_channel_pressure(s, ch, a);
			break;
		case 0xE0:
			fluid_synth_pitch_bend(s, ch, a | (b << 7));
			break;
		}
	}
	void render(int16_t *samples, int frames) override {
		fluid_synth_write_s16(_engine.get(), frames, samples, 0, 2, samples, 1, 2);
	}
};
#endif

#ifdef REMINISCENCE2_MT32EMU
class Mt32 final : public Synth {
	mt32emu_context _context = nullptr;
	std::filesystem::path _directory;
	void clear() {
		if (_context) {
			mt32emu_free_context(_context);
			_context = nullptr;
		}
	}

public:
	explicit Mt32(std::string directory) : _directory(std::move(directory)) {}
	~Mt32() override { clear(); }
	bool reset(int rate, std::string &error) override {
		clear();
		for (const char *model : {"MT32", "CM32L"}) {
			mt32emu_report_handler_i handler{};
			_context = mt32emu_create_context(handler, nullptr);
			if (!_context) {
				break;
			}
			const auto control = (_directory / (std::string(model) + "_CONTROL.ROM")).string();
			const auto pcm = (_directory / (std::string(model) + "_PCM.ROM")).string();
			if (mt32emu_add_rom_file(_context, control.c_str()) == MT32EMU_RC_ADDED_CONTROL_ROM &&
			    mt32emu_add_rom_file(_context, pcm.c_str()) == MT32EMU_RC_ADDED_PCM_ROM) {
				mt32emu_set_stereo_output_samplerate(_context, rate);
				mt32emu_set_midi_delay_mode(_context, MT32EMU_MDM_IMMEDIATE);
				if (mt32emu_open_synth(_context) == MT32EMU_RC_OK) {
					return true;
				}
			}
			clear(); // A partially loaded ROM pair cannot contaminate the next attempt.
		}
		error = "MT-32 requires a compatible control/PCM ROM pair in --rompath";
		return false;
	}
	void send(uint8_t status, uint8_t a, uint8_t b) override {
		if (_context) {
			mt32emu_play_msg_now(_context, status | (uint32_t(a) << 8) | (uint32_t(b) << 16));
		}
	}
	void render(int16_t *samples, int frames) override {
		mt32emu_render_bit16s(_context, samples, frames);
	}
};
#endif
} // namespace

bool backendAvailable(Backend backend) {
	switch (backend) {
	case Backend::None:
	case Backend::Adlib:
		return true;
#ifdef REMINISCENCE2_FLUIDSYNTH
	case Backend::FluidSynth:
		return true;
#endif
#ifdef REMINISCENCE2_MT32EMU
	case Backend::Mt32:
		return true;
#endif
	default:
		return false;
	}
}
std::unique_ptr<Synth> makeSynth(const Options &options, std::string &error) {
	error.clear();
	switch (options.backend) {
	case Backend::None:
		return nullptr;
	case Backend::Adlib:
		return std::make_unique<Adlib>(options.correctWaveformRegister);
#ifdef REMINISCENCE2_FLUIDSYNTH
	case Backend::FluidSynth:
		return std::make_unique<Fluid>(options.soundFont);
#endif
#ifdef REMINISCENCE2_MT32EMU
	case Backend::Mt32:
		return std::make_unique<Mt32>(options.romDirectory);
#endif
	default:
		error = "Requested MIDI backend was not compiled into this build";
		return nullptr;
	}
}
} // namespace music
