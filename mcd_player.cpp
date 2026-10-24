// SPDX-License-Identifier: GPL-3.0-or-later
#include "mcd_player.h"
#include "mixer.h"
#include "systemstub.h"
#include "util.h"
#include <algorithm>
#include <cstring>

namespace mcd {
const char *eventName(unsigned event, unsigned level) {
	static const char *table[] = {
	    "DEBUT",    "TKHOLO",  nullptr,    nullptr,    "TKGUN",    nullptr,    "CHUTE1",
	    nullptr,    nullptr,   "DESINTEG", nullptr,    nullptr,    nullptr,    "SC1",
	    "PILE1",    "TKPILE",  nullptr,    "HOLO2",    "TKID",     nullptr,    "BRIDGE",
	    "GVTELEC",  nullptr,   nullptr,    nullptr,    "MAP",      "CARTE1",   "CARTE2",
	    "CARTE3",   "CARTE4",  nullptr,    nullptr,    "GETCOLIS", "GVCOLIS",  "GETWORK",
	    "GETID",    "TKFUSE",  "MISSION1", nullptr,    "MISSION2", "MISSION4", "MISSION3",
	    "MISSION5", "GVID1",   "GVTUNE1",  "MISSIONV", nullptr,    "ANTIG",    "MEMORY",
	    "GETANTIG", "TAXI",    nullptr,    "VOYAGE",   "TELEPORT", "LIFTU",    "LIFTD",
	    nullptr,    "SPY",     "GETDYNAM", "JOURNAL",  nullptr,    "SC2",      "GENEXPLO",
	    "BADEND",   nullptr,   "OVER",     "MISSEND1", nullptr,    "MISSEND2", "MISSEND3",
	    "MISSEND4", "MISSEND5"};
	if (level >= 7)
		return nullptr;
	static const char *keys[] = {"TKKEY1", "TKKEY2", "TKKEY3", "TKKEY3",
	                             "TKKEY3", "TKKEY4", "TKKEY4"};
	static const char *shields[] = {"SHIELD1", "SHIELD2", "SHIELD3", "SHIELD4",
	                                "SHIELD4", "SHIELD5", "SHIELD5"};
	static const char *tele[] = {"TKTELEC1", "TKTELEC2", "TKTELEC2", "TKTELEC2",
	                             "TKTELEC2", "TKTELEC3", "TKTELEC3"};
	if (event == 2)
		return keys[level];
	if (event == 5)
		return shields[level];
	if (event == 10)
		return level ? "TKCREDI2" : "TKCREDIT";
	if (event == 16)
		return tele[level];
	if (event == 6)
		return level == 0 || level == 3 ? "CHUTE1" : "CHUTE2";
	if (event == 43)
		return level == 1 ? "GVID1" : "GVID2";
	if (event == 44)
		return level == 0 ? "GVTUNE1" : "GVTUNE2";
	if (event == 60)
		return level < 5 ? "TKRECEP1" : "TKRECEP2";
	return event < sizeof(table) / sizeof(*table) ? table[event] : nullptr;
}
const std::vector<std::string> &names() {
	static const std::vector<std::string> values = [] {
		std::vector<std::string> v;
		for (unsigned e = 0; e < 72; ++e)
			for (unsigned l = 0; l < 7; ++l)
				if (const char *name = eventName(e, l))
					v.emplace_back(name);
		v.insert(v.end(), {"CREATE", "CHUTE2", "GVID2", "GVTUNE2"});
		std::sort(v.begin(), v.end());
		v.erase(std::unique(v.begin(), v.end()), v.end());
		return v;
	}();
	return values;
}
Codec codecFor(const char *name) {
	std::string s(name);
	for (auto &c : s)
		if (c >= 'a' && c <= 'z')
			c -= 'a' - 'A';
	if (s == "MEMORY.MCD" || s == "SPY.MCD")
		return Codec::Split;
	if (s == "SC2.MCD" || s == "BRIDGE.MCD")
		return Codec::Msb;
	return Codec::Lsb;
}
size_t Stream::playbackFrames(const char *name) {
	std::string stem(name);
	for (auto &c : stem)
		if (c >= 'a' && c <= 'z')
			c -= 'a' - 'A';
	if (stem == "SC2.MCD")
		return std::min(frames.size(), size_t(5312));
	uint8_t header[36];
	if (!readVideo(0, header, sizeof(header)))
		return 0;
	if (!header[32] || READ_BE_UINT16(header + 34) != 0x4000)
		return frames.size();
	// Defaults from the US disc event directory; explicit movie headers take
	// precedence except for SC2, which joins three sequences into one ending.
	static const struct {
		const char *name;
		size_t frames;
	} limits[] = {{"SC1", 691},      {"HOLO2", 300},    {"SHIELD1", 95},   {"PILE1", 95},
	              {"MISSION1", 375}, {"MISSION2", 468}, {"MISSION3", 474}, {"MISSION4", 474},
	              {"MISSION5", 435}, {"MISSIONV", 100}, {"LIFTU", 100},    {"LIFTD", 100},
	              {"GENEXPLO", 80},  {"MISSEND1", 95},  {"MISSEND2", 105}, {"MISSEND3", 110},
	              {"MISSEND4", 115}, {"MISSEND5", 120}};
	for (const auto &limit : limits)
		if (stem == std::string(limit.name) + ".MCD")
			return std::min(frames.size(), limit.frames);
	return frames.size();
}
bool decodeFrame(const uint8_t *data, size_t size, Codec codec, uint8_t *pixels, uint8_t *palette) {
	if (!data || size < 36 || !pixels || !palette || data[32] > 1)
		return false;
	std::array<uint8_t, 16384> packed{};
	size_t in = 36, out = 0, refs = 0;
	if (!data[32]) {
		const size_t end = in + READ_BE_UINT16(data + 34);
		if (end > size)
			return false;
		while (in < end) {
			const int code = int8_t(data[in++]);
			const size_t count = size_t(code < 0 ? -code + 1 : code + 1) * 2;
			if (count > packed.size() - out || (code < 0 ? 2 : count) > end - in)
				return false;
			for (size_t i = 0; i < count; ++i)
				packed[out++] = data[in + (code < 0 ? i % 2 : i)];
			in += code < 0 ? 2 : count;
		}
	} else {
		if (codec == Codec::Split) {
			if (size < 38)
				return false;
			refs = 38 + READ_BE_UINT16(data + 36);
			in = 38;
			if (refs > size)
				return false;
		}
		const size_t literalsEnd = codec == Codec::Split ? refs : size;
		while (out < packed.size()) {
			if (in >= literalsEnd)
				return false;
			const unsigned flags = data[in++];
			for (unsigned bit = 0; bit < 8 && out < packed.size(); ++bit) {
				if (flags & (codec == Codec::Lsb ? 1u << bit : 128u >> bit)) {
					if (in >= literalsEnd)
						return false;
					packed[out++] = data[in++];
				} else {
					auto &at = codec == Codec::Split ? refs : in;
					if (at > size || size - at < 2)
						return false;
					const unsigned pair = READ_BE_UINT16(data + at);
					at += 2;
					const unsigned distance = (pair >> 4) + 1, count = (pair & 15) + 2;
					if (distance > out || count > packed.size() - out)
						return false;
					for (unsigned i = 0; i < count; ++i) {
						packed[out] = packed[out - distance];
						++out;
					}
				}
			}
		}
	}
	if (out != packed.size())
		return false;
	// Unlike object sprites, movie tiles are stored in row-major order.
	for (unsigned y = 0; y < 128; ++y)
		for (unsigned x = 0; x < 256; ++x) {
			const unsigned index = ((y / 8) * 32 + x / 8) * 64 + (y % 8) * 8 + x % 8;
			pixels[y * 256 + x] = (packed[index / 2] >> (index % 2 ? 0 : 4)) & 15;
		}
	for (unsigned i = 0; i < 16; ++i) {
		const unsigned color = READ_BE_UINT16(data + i * 2);
		for (unsigned c = 0; c < 3; ++c)
			palette[i * 3 + c] = ((color >> (c * 4)) & 14) * 255 / 14;
	}
	return true;
}
bool Stream::readVideo(uint32_t offset, uint8_t *out, uint32_t bytes) {
	if (offset > videoSize || bytes > videoSize - offset)
		return false;
	while (bytes) {
		const uint32_t count = std::min(bytes, 8192 - offset % 8192);
		file.seek((offset / 8192) * 10240 + offset % 8192);
		if (file.read(out, count) != count)
			return false;
		out += count;
		offset += count;
		bytes -= count;
	}
	return !file.ioErr();
}
bool Stream::open(const char *name, FileSystem *fs) {
	frames.clear();
	videoSize = audioSamples = 0;
	codec = codecFor(name);
	if (!file.open(name, "rb", fs))
		return false;
	const uint32_t size = file.size();
	if (!size || size > 1024 * 1024 * 1024 || size % 10240)
		return false;
	videoSize = size / 10240 * 8192;
	audioSamples = size / 10240 * 2048;
	uint32_t offset = 0, remaining = 0;
	std::string normalized(name);
	for (auto &c : normalized)
		if (c >= 'a' && c <= 'z')
			c -= 'a' - 'A';
	const bool ending = normalized == "SC2.MCD";
	while (offset + 36 <= videoSize) {
		uint8_t h[36];
		if (!readVideo(offset, h, sizeof(h)))
			return false;
		const unsigned sectors = h[33];
		// The disc pads the last CD transfer group. A movie with no explicit
		// frame count can end in an incomplete, unused copy of its last frame.
		if (!sectors || sectors > 7 || sectors * 2048 > videoSize - offset) {
			if (remaining || videoSize - offset > 8192)
				return false;
			break;
		}
		const unsigned count = READ_BE_UINT16(h + 34);
		if (h[32] && count != 0x4000 && !remaining)
			remaining = count;
		frames.push_back({offset / 2048, sectors});
		offset += sectors * 2048;
		if (frames.size() > 100000)
			return false;
		if (remaining && !--remaining && !ending)
			break;
	}
	return !remaining && !frames.empty();
}
bool Stream::frame(size_t index, uint8_t *pixels, uint8_t *palette) {
	if (index >= frames.size())
		return false;
	std::array<uint8_t, 7 * 2048> data{};
	const auto &f = frames[index];
	return readVideo(f.sector * 2048, data.data(), f.sectors * 2048) &&
	       decodeFrame(data.data(), f.sectors * 2048, codec, pixels, palette);
}
bool Player::play(const char *name, FileSystem *fs, bool skip,
                  const std::function<bool()> &interrupt) {
	completed = false;
	Stream stream;
	if (!stream.open(name, fs) || !audio.open(name, "rb", fs))
		return false;
	const size_t count = stream.playbackFrames(name);
	if (!count) {
		audio.close();
		return false;
	}
	CutsceneInputScope input(stub, skip);
	std::array<uint8_t, 256 * 224> screen{};
	std::array<uint8_t, 768> saved{}, palette{};
	stub->getPalette(saved.data(), 256);
	samples = stream.audioSamples;
	phase = 0;
	loadedBlock = UINT32_MAX;
	mixer->setPremixHook(callback, this);
	const uint32_t start = stub->getTimeStamp();
	const uint32_t pausedAtStart = stub->getPausedTime();
	bool valid = true, skipped = false;
	for (size_t i = 0; i < count; ++i) {
		if (!stream.frame(i, screen.data() + 48 * 256, palette.data())) {
			valid = false;
			break;
		}
		stub->setPalette(palette.data(), 256);
		stub->copyRect(0, 0, 256, 224, screen.data(), 256);
		stub->updateScreen(0);
		const auto &f = stream.frames[i];
		const uint32_t deadline = start + uint64_t(f.sector + f.sectors) * 1000 / 60;
		do {
			stub->processEvents();
			skipped =
			    (interrupt && interrupt()) || stub->consumeCutsceneSkip() || stub->_pi.backspace;
			if (stub->_pi.quit || skipped)
				break;
			const uint32_t activeTime = stub->getTimeStamp() -
			    (stub->getPausedTime() - pausedAtStart);
			const int wait = int32_t(deadline - activeTime);
			if (wait <= 0)
				break;
			stub->sleep(std::min(wait, 10));
		} while (true);
		if (stub->_pi.quit || skipped)
			break;
	}
	mixer->clearPremixHook(this);
	audio.close();
	stub->_pi.backspace = false;
	stub->setPalette(saved.data(), 256);
	completed = valid && !skipped && !stub->_pi.quit;
	return valid;
}
bool Player::callback(void *self, int16_t *out, int frames) {
	return static_cast<Player *>(self)->mix(out, frames);
}
bool Player::mix(int16_t *out, int frames) {
	const uint32_t rate = mixer->getSampleRate();
	if (!rate)
		return false;
	for (int i = 0; i < frames; ++i) {
		const uint64_t sample = phase / rate;
		if (sample >= samples)
			return false;
		const uint32_t block = sample / 2048;
		if (block != loadedBlock) {
			audio.seek(block * 10240 + 8192);
			if (audio.read(audioBuffer.data(), 2048) != 2048)
				return false;
			loadedBlock = block;
		}
		const int value = audioBuffer[sample % 2048];
		const int pcm = (value & 128 ? -(value & 127) : value) * 256;
		out[i * 2] = out[i * 2 + 1] = int16_t(pcm);
		phase += 30720; // 75 CD sectors/s, one PCM sector out of every five.
	}
	return true;
}
} // namespace mcd
