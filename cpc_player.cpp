// SPDX-License-Identifier: GPL-3.0-or-later
// CPC/SDX2 format support originally researched by Gregory Montoir.
#include "cpc_player.h"
#include "byte_reader.h"
#include "mixer.h"
#include "util.h"
#include <algorithm>
#include <cstring>

namespace {
constexpr size_t MaximumPcmSamples = 32 * 1024 * 1024; // 64 MiB decoded PCM.
int16_t decodeSDX2(int16_t previous, int8_t data) {
	const int square = int(data) * std::abs(int(data)) * 2;
	return int16_t((data & 1) ? int(previous) + square : square);
}
} // namespace
bool decodeCpc(const uint8_t *bytes, size_t size, std::vector<int16_t> &pcm, unsigned &rate) {
	if (!bytes || size > 32 * 1024 * 1024)
		return false;
	ByteReader r(bytes, size);
	std::vector<int16_t> decoded;
	unsigned sourceRate = 0;
	int16_t left = 0, right = 0;
	while (r.remaining()) {
		const size_t start = r.position();
		char tag[4];
		r.read(tag, 4);
		const unsigned length = r.be32();
		if (!r.good() || length < 8 || length > size - start)
			return false;
		if (memcmp(tag, "SNDS", 4) == 0) {
			if (length < 20)
				return false;
			r.skip(8);
			char type[4];
			r.read(type, 4);
			if (memcmp(type, "SHDR", 4) == 0) {
				if (length < 56)
					return false;
				r.skip(24);
				const unsigned hz = r.be32(), channels = r.be32();
				char compression[4];
				r.read(compression, 4);
				if (channels != 2 || hz < 8000 || hz > 192000 || memcmp(compression, "SDX2", 4) ||
				    (sourceRate && hz != sourceRate))
					return false;
				sourceRate = hz;
			} else if (memcmp(type, "SSMP", 4) == 0) {
				if (length < 24 || !sourceRate)
					return false;
				const unsigned count = r.be32();
				if ((count & 1) || count > length - 24 ||
				    count > MaximumPcmSamples - decoded.size())
					return false;
				for (unsigned i = 0; i < count; i += 2) {
					left = decodeSDX2(left, int8_t(r.u8()));
					right = decodeSDX2(right, int8_t(r.u8()));
					decoded.push_back(left);
					decoded.push_back(right);
				}
			}
		}
		if (!r.good() || !r.seek(start + length))
			return false;
	}
	if (!sourceRate || decoded.empty())
		return false;
	pcm = std::move(decoded);
	rate = sourceRate;
	return true;
}

CpcPlayer::CpcPlayer(Mixer *mixer, FileSystem *fs) : _mix(mixer), _fs(fs) {
}
CpcPlayer::~CpcPlayer() {
	stopTrack();
}
bool CpcPlayer::playTrack(int num) {
	static const char *const tunes[] = {"Options.Cpc", "Jungle.Cpc", "Subway.Cpc",
	                                    "TVshow.Cpc",  "City.Cpc",   "Alien.Cpc"};
	stopTrack();
	const int tune = num - 2;
	File file;
	if (tune < 0 || tune >= int(std::size(tunes)) || !_fs || !file.open(tunes[tune], "rb", _fs))
		return false;
	const unsigned size = file.size();
	if (!size || size > 32 * 1024 * 1024)
		return false;
	std::vector<uint8_t> bytes(size);
	if (file.read(bytes.data(), size) != size || !decodeCpc(bytes.data(), size, _pcm, _sourceRate))
		return false;
	_cursor = _phase = 0;
	_mix->setPremixHook(mixCallback, this);
	return true;
}
void CpcPlayer::stopTrack() {
	_mix->clearPremixHook(this);
	_pcm.clear();
	_cursor = _phase = 0;
}
void CpcPlayer::pauseTrack() {
	_mix->clearPremixHook(this);
}
void CpcPlayer::resumeTrack() {
	if (!_pcm.empty())
		_mix->setPremixHook(mixCallback, this);
}
bool CpcPlayer::mix(int16_t *buf, int count) {
	const unsigned rate = _mix->getSampleRate();
	if (_pcm.empty() || !rate)
		return false;
	const size_t frames = _pcm.size() / 2;
	// Immutable decoded PCM keeps disk I/O and allocations out of the callback.
	// Fractional source advancement works with the actual negotiated device rate.
	for (int i = 0; i < count; ++i) {
		*buf++ = _pcm[_cursor * 2];
		*buf++ = _pcm[_cursor * 2 + 1];
		_phase += _sourceRate;
		_cursor = (_cursor + _phase / rate) % frames;
		_phase %= rate;
	}
	return true;
}
bool CpcPlayer::mixCallback(void *param, int16_t *buf, int len) {
	return static_cast<CpcPlayer *>(param)->mix(buf, len);
}
