// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef WAV_STREAM_H
#define WAV_STREAM_H
#include "file.h"
#include <array>

// Bounded, streaming PCM reader. CD audio remains lossless and does not need
// an external converter or a whole-track allocation on the audio thread.
struct WavStream {
	bool open(const char *name, FileSystem *fs);
	int read(int16_t *out, int frames);
	File file;
	uint32_t rate = 0, start = 0, length = 0, position = 0;
	unsigned channels = 0;
	bool ready = false;
	std::array<uint8_t, 4096> buffer{};
};
#endif
