// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_MUSIC_SYNTH_H
#define REMINISCENCE2_MUSIC_SYNTH_H
#include "music_midi.h"
#include "music_options.h"
#include <memory>
namespace music {
// One instance belongs to one player. No process-wide chip, settings, ROM,
// current-instrument pointer or audio callback state is shared between songs.
class Synth {
public:
	virtual ~Synth() = default;
	virtual bool reset(int rate, std::string &error) = 0;
	virtual void instrument(int track, const AdlibInstrument &) {}
	virtual void send(uint8_t status, uint8_t first, uint8_t second) = 0;
	virtual void render(int16_t *stereo, int frames) = 0;
};
std::unique_ptr<Synth> makeSynth(const Options &options, std::string &error);
} // namespace music
#endif
