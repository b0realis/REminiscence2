
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef OGG_PLAYER_H__
#define OGG_PLAYER_H__

#include "intern.h"
#include <array>
#include "wav_stream.h"

struct FileSystem;
struct Mixer;
struct OggDecoder_impl;

struct OggPlayer {
	OggPlayer(Mixer *mixer, FileSystem *fs);
	~OggPlayer();

	bool playTrack(int num);
	bool playFile(const char *name);
	void stopTrack();
	void pauseTrack();
	void resumeTrack();
	bool isPlaying() const { return _impl != 0; }
	bool mix(int16_t *buf, int len);
	bool nextFrame(int16_t *frame);
	static bool mixCallback(void *param, int16_t *buf, int len);

	Mixer *_mix;
	FileSystem *_fs;
	OggDecoder_impl *_impl;
	WavStream _wave;
	bool _useWave = false, _loaded = false;
	int readSamples(int16_t *buffer, int frames);
	std::array<int16_t, 1024> _input{};
	int _inputCount = 0, _inputOffset = 0;
	int16_t _left[2]{}, _right[2]{};
	double _phase = 0, _step = 1;
	bool _primed = false;
};

#endif // OGG_PLAYER_H__
