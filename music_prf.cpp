// SPDX-License-Identifier: GPL-3.0-or-later
// PRF format and track semantics researched in REminiscence by Gregory Montoir.
#include "music_prf.h"
#include "mixer.h"
#include "file.h"
#include "fs.h"
#include "util.h"
#include <algorithm>
#include <limits>

namespace music {
bool Sequencer::start(Song song, Prf prf, std::array<AdlibInstrument, 16> patches,
                      std::unique_ptr<Synth> synth, bool mt32, int rate, std::string &error) {
	stop();
	_synth.reset();
	if (!synth || song.tracks.empty() || song.tracks.size() > 16 || !prf.timerMod ||
	    !prf.timerTicks || rate < 8000 || rate > 192000) {
		error = "Invalid PRF playback setup";
		return false;
	}
	if (mt32) {
		for (size_t i = 0; i < song.tracks.size(); ++i) {
			if (!song.tracks[i].events.empty() && (prf.channels[i] > 15 || prf.programs[i] > 127)) {
				error = "Invalid active PRF track mapping";
				return false;
			}
		}
	}
	if (!synth->reset(rate, error)) {
		return false;
	}
	_song = std::move(song);
	_prf = std::move(prf);
	_patches = std::move(patches);
	_synth = std::move(synth);
	_mt32 = mt32;
	_rate = rate;
	_clock = mt32 ? 2242 : 2082;
	_cursors = {};
	_framesLeft = 0;
	_remainder = 0;
	_tick = 0;
	for (size_t i = 0; i < _song.tracks.size(); ++i) {
		_cursors[i].instrument = i;
		_cursors[i].active = !_song.tracks[i].events.empty();
		if (mt32 && _cursors[i].active) {
			const unsigned channel = _prf.channels[i];
			_synth->send(0xC0 | channel, _prf.programs[i], 0);
			// One semitone pitch bend sensitivity, matching the original PRF player.
			_synth->send(0xB0 | channel, 101, 0);
			_synth->send(0xB0 | channel, 100, 0);
			_synth->send(0xB0 | channel, 6, 1);
			_synth->send(0xB0 | channel, 38, 0);
		}
	}
	_playing = true;
	return true;
}
void Sequencer::stop() {
	if (_synth && _playing) {
		for (int i = 0; i < 16; ++i) {
			_synth->send(0xB0 | i, 123, 0);
		}
	}
	_playing = false;
}
void Sequencer::dispatch(size_t track, const Event &event) {
	auto &cursor = _cursors[track];
	const unsigned kind = event.status & 0xF0;
	const unsigned channel = _mt32 ? _prf.channels[track] : track;
	if (kind == 0x90 || kind == 0x80) {
		const unsigned patch = cursor.instrument;
		const unsigned lookup = _prf.lookupAdlib ? patch : track;
		int note = event.first + (_mt32 ? _prf.mt32Notes[patch] : _prf.adlibNotes[lookup]);
		if (_mt32 && _prf.lookupChannels && channel == 9) {
			note = _prf.mt32Notes[patch];
		}
		if (note < 0 || note > (_mt32 ? 127 : 95)) {
			return;
		}
		const bool on = kind == 0x90 && event.second != 0;
		int velocity = on ? std::clamp<int>(event.second + (_mt32 ? _prf.mt32Velocity[patch]
		                                                          : _prf.adlibVelocity[lookup]),
		                                    0, 127)
		                  : 0;
		if (_mt32) {
			velocity = velocity * 63 / 64;
		} else {
			_synth->instrument(track, _patches[patch]);
		}
		_synth->send((on ? 0x90 : 0x80) | channel, note, velocity);
	} else if (kind == 0xC0) {
		if (_mt32 || _prf.lookupAdlib) {
			const auto &programs = _mt32 ? _prf.mt32Programs : _prf.adlibPrograms;
			for (size_t i = 0; i < programs.size(); ++i) {
				if (programs[i] == event.first) {
					if (_mt32) {
						if (_prf.programs[i] <= 127) {
							_synth->send(0xC0 | channel, _prf.programs[i], 0);
						}
					} else {
						cursor.instrument = i;
					}
					break;
				}
			}
		}
	} else {
		_synth->send(kind | channel, event.first, event.second);
	}
}
void Sequencer::step() {
	if (_prf.duration && _tick >= _prf.duration) {
		stop();
		return;
	}
	bool any = false;
	for (size_t i = 0; i < _song.tracks.size(); ++i) {
		auto &cursor = _cursors[i];
		const auto &track = _song.tracks[i];
		if (!cursor.active) {
			continue;
		}
		while (cursor.event < track.events.size() &&
		       cursor.cycle + track.events[cursor.event].tick <= _tick) {
			dispatch(i, track.events[cursor.event++]);
		}
		if (cursor.event == track.events.size() && cursor.cycle + track.endTick <= _tick) {
			if (_prf.noLoop[i]) {
				cursor.active = false;
			} else {
				const uint64_t period = (track.endTick / _prf.timerMod + 1) * _prf.timerMod;
				cursor.cycle += period;
				cursor.event = 0;
			}
		}
		any |= cursor.active;
	}
	if (!any) {
		stop();
		return;
	}
	++_tick;
}
void Sequencer::render(int16_t *stereo, int frames) {
	if (!stereo || frames <= 0) {
		return;
	}
	while (frames > 0) {
		if (!_playing) {
			std::fill_n(stereo, size_t(frames) * 2, 0);
			return;
		}
		if (!_framesLeft) {
			step();
			if (!_playing) {
				continue;
			}
			// Carry the fractional sample count between ticks. Changing output rate
			// cannot change the PRF clock, and rounding does not accumulate song drift.
			const uint64_t numerator = uint64_t(_rate) * _prf.timerTicks + _remainder;
			_framesLeft = numerator / _clock;
			_remainder = numerator % _clock;
			if (!_framesLeft) {
				continue;
			}
		}
		const int count = std::min<uint32_t>(frames, _framesLeft);
		_synth->render(stereo, count);
		stereo += count * 2;
		frames -= count;
		_framesLeft -= count;
	}
}

PrfPlayer::PrfPlayer(Mixer *mixer, FileSystem *files, FileSystem *fallback, Options options)
    : _mixer(mixer), _files(files), _fallback(fallback), _options(std::move(options)) {
}
PrfPlayer::~PrfPlayer() {
	stop();
}
namespace {
bool readAsset(FileSystem *primary, FileSystem *fallback, const char *name,
               std::vector<uint8_t> &data) {
	File file;
	if ((!primary || !file.open(name, "rb", primary)) &&
	    (!fallback || !file.open(name, "rb", fallback))) {
		return false;
	}
	const uint32_t size = file.size();
	if (!size || size > 16 * 1024 * 1024) {
		return false;
	}
	data.resize(size);
	return file.read(data.data(), size) == size;
}
} // namespace
bool PrfPlayer::play(int number) {
	static const char *names[] = {
	    "introl3",  "option3",  "journal3", "chute3",   "desinte3", "capture3", "voyage3",
	    "telepor3", "planexp3", "end31",    "lift3",    "present3", "gameove3", "holo3",
	    "memory3",  "chutevi3", "reveil3",  "misvali3", "taxi3",    "donner3",  "mission3",
	    "objet3",   "recharg3", "generat3", "pont3",    "rechage3"};
	if (_options.backend == Backend::None || number < 0 || size_t(number) >= std::size(names)) {
		return false;
	}
	stop();
	const bool mt32 = _options.backend != Backend::Adlib;
	const std::string name = std::string(number == 1 && mt32 ? "opt" : names[number]) + ".prf";
	std::vector<uint8_t> bytes;
	if (!readAsset(_files, _fallback, name.c_str(), bytes)) {
		return false;
	}
	std::string error;
	Prf prf;
	Song song;
	std::array<AdlibInstrument, 16> instruments{};
	auto fail = [&]() {
		warning("PRF '%s': %s", name.c_str(), error.c_str());
		return false;
	};
	if (!parsePrf(bytes.data(), bytes.size(), prf, error)) {
		return fail();
	}
	if (!readAsset(_files, _fallback, prf.midi.c_str(), bytes)) {
		error = "MIDI asset is missing";
		return fail();
	}
	if (!parseMidi(bytes.data(), bytes.size(), song, error)) {
		return fail();
	}
	if (!mt32) {
		for (size_t i = 0; i < instruments.size(); ++i) {
			if (prf.instruments[i].empty()) {
				continue;
			}
			const auto filename = prf.instruments[i] + ".INS";
			if (!readAsset(_files, _fallback, filename.c_str(), bytes) ||
			    !parseInstrument(bytes.data(), bytes.size(), instruments[i])) {
				error = "Missing or invalid instrument: " + filename;
				return fail();
			}
		}
	}
	auto synth = makeSynth(_options, error);
	if (!synth) {
		return fail();
	}
	if (!_sequencer.start(std::move(song), std::move(prf), std::move(instruments), std::move(synth),
	                      mt32, _mixer->getSampleRate(), error)) {
		return fail();
	}
	_mixer->setPremixHook(callback, this);
	_hooked = true;
	return true;
}
void PrfPlayer::stop() {
	if (_hooked) {
		_mixer->clearPremixHook(this);
		_hooked = false;
	}
	_sequencer.stop();
}
bool PrfPlayer::callback(void *context, int16_t *buffer, int frames) {
	auto *player = static_cast<PrfPlayer *>(context);
	player->_sequencer.render(buffer, frames);
	return player->_sequencer.playing();
}
} // namespace music
