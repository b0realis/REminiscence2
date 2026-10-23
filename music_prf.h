// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_MUSIC_PRF_H
#define REMINISCENCE2_MUSIC_PRF_H
#include "music_synth.h"
#include <array>
struct FileSystem;
struct Mixer;
namespace music {
class Sequencer {
	struct Cursor {
		size_t event = 0;
		uint64_t cycle = 0;
		unsigned instrument = 0;
		bool active = false;
	};
	Song _song;
	Prf _prf;
	std::array<AdlibInstrument, 16> _patches{};
	std::array<Cursor, 16> _cursors{};
	std::unique_ptr<Synth> _synth;
	bool _mt32 = false, _playing = false;
	uint32_t _rate = 0, _clock = 0, _framesLeft = 0;
	uint64_t _remainder = 0, _tick = 0;
	void dispatch(size_t track, const Event &event);
	void step();

public:
	bool start(Song song, Prf prf, std::array<AdlibInstrument, 16> patches,
	           std::unique_ptr<Synth> synth, bool mt32, int rate, std::string &error);
	void stop();
	bool playing() const { return _playing; }
	uint64_t ticks() const { return _tick; }
	void render(int16_t *stereo, int frames);
};

class PrfPlayer {
	Mixer *_mixer;
	FileSystem *_files, *_fallback;
	Options _options;
	Sequencer _sequencer;
	bool _hooked = false;
	static bool callback(void *, int16_t *, int);

public:
	PrfPlayer(Mixer *mixer, FileSystem *files, FileSystem *fallback, Options options);
	~PrfPlayer();
	bool play(int number);
	void stop();
};
} // namespace music
#endif
