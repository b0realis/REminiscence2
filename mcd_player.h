// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MCD_PLAYER_H
#define MCD_PLAYER_H
#include "file.h"
#include <array>
#include <functional>
#include <string>
#include <vector>
struct Mixer;
struct SystemStub;
namespace mcd {
enum class Codec { Lsb, Msb, Split };
Codec codecFor(const char *name);
bool decodeFrame(const uint8_t *data, size_t size, Codec codec, uint8_t *pixels, uint8_t *palette);
struct Frame {
	uint32_t sector, sectors;
};
// Four video sectors followed by one PCM sector. Video frames can cross
// these groups; the byte at header offset 33 counts VIDEO sectors only.
class Stream {
public:
	bool open(const char *name, FileSystem *fs);
	bool readVideo(uint32_t offset, uint8_t *out, uint32_t bytes);
	bool frame(size_t index, uint8_t *pixels, uint8_t *palette);
	// Disc event lengths can exclude valid duplicate/padding frames. Indexing
	// retains them for inspection; playback uses the original event duration.
	size_t playbackFrames(const char *name);
	File file;
	Codec codec = Codec::Lsb;
	std::vector<Frame> frames;
	uint32_t videoSize = 0, audioSamples = 0;
};
class Player {
public:
	Player(SystemStub *stub, Mixer *mixer) : stub(stub), mixer(mixer) {}
	bool play(const char *name, FileSystem *fs, bool skip,
	          const std::function<bool()> &interrupt = {});
	bool completed = false;

private:
	static bool callback(void *self, int16_t *out, int frames);
	bool mix(int16_t *out, int frames);
	SystemStub *stub;
	Mixer *mixer;
	File audio;
	uint32_t samples = 0, loadedBlock = UINT32_MAX;
	uint64_t phase = 0;
	std::array<uint8_t, 2048> audioBuffer{};
};
const char *eventName(unsigned event, unsigned level);
const std::vector<std::string> &names();
} // namespace mcd
#endif
