// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_MUSIC_MIDI_H
#define REMINISCENCE2_MUSIC_MIDI_H
#include <array>
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace music {
// Events own no pointers into a file. Tick positions include skipped meta and
// SysEx events, so their delays survive even though PRF supplies its own clock.
struct Event {
	uint64_t tick;
	uint8_t status, first, second;
};
struct Track {
	std::vector<Event> events;
	uint64_t endTick = 0;
};
struct Song {
	std::vector<Track> tracks;
	uint16_t division = 0;
};
bool parseMidi(const uint8_t *data, size_t size, Song &result, std::string &error);

struct Prf {
	std::array<std::string, 16> instruments;
	std::string midi;
	std::array<int16_t, 16> adlibNotes{}, adlibVelocity{}, adlibPrograms{};
	std::array<int16_t, 16> mt32Notes{}, mt32Velocity{}, mt32Programs{};
	std::array<uint8_t, 16> channels{}, programs{}, noLoop{};
	uint32_t timerTicks = 0, duration = 0;
	uint16_t timerMod = 0;
	bool lookupAdlib = false, lookupChannels = false;
};
bool parsePrf(const uint8_t *data, size_t size, Prf &result, std::string &error);
struct AdlibInstrument {
	uint8_t mode = 0, channel = 0, modWave = 0, carrierWave = 0;
	std::array<std::array<uint16_t, 13>, 2> operators{};
};
bool parseInstrument(const uint8_t *data, size_t size, AdlibInstrument &result);
} // namespace music
#endif
