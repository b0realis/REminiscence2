
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef SEQ_PLAYER_H__
#define SEQ_PLAYER_H__

#include "intern.h"
#include <array>

struct File;
struct SystemStub;
struct Mixer;

struct SeqDemuxer {
	enum {
		kFrameSize = 6144,
		kAudioBufferSize = 882,
		kBuffersCount = 30
	};

	bool open(File *f);
	void close();

	bool readHeader();
	bool readFrameData();
	bool fillBuffer(int num, int offset, int size);
	void clearBuffer(int num);
	bool readPalette(uint8_t *dst);
	bool readAudio(int16_t *dst);

	int _frameOffset;
	int _audioDataOffset;
	int _paletteDataOffset;
	int _videoData;
	bool _failed = false;
	struct {
		int size;
		int avail;
		uint8_t *data;
	} _buffers[kBuffersCount];
	int _fileSize;
	File *_f;
};

struct SeqPlayer {
	enum {
		kVideoWidth = 256,
		kVideoHeight = 128,
		kSoundPreloadSize = 4
	};

	static const char *_namesTable[];

	SeqPlayer(SystemStub *stub, Mixer *mixer);
	~SeqPlayer();

	void setBackBuffer(uint8_t *buf) { _buf = buf; }
	bool play(File *f);
	bool mix(int16_t *buf, int len);
	static bool mixCallback(void *param, int16_t *buf, int len);

	SystemStub *_stub;
	uint8_t *_buf;
	Mixer *_mix;
	SeqDemuxer _demux;
	bool _completed = false;
	// A bounded queue avoids allocating/freeing memory on the audio thread.
	std::array<std::array<int16_t, SeqDemuxer::kAudioBufferSize>, 16> _audioBuffers{};
	unsigned _audioRead = 0, _audioWrite = 0, _audioCount = 0, _audioOffset = 0;
	uint32_t _audioPhase = 0;
	bool _audioReady = false;
};

#endif // SEQ_PLAYER_H__
