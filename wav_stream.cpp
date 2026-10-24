// SPDX-License-Identifier: GPL-3.0-or-later
#include "wav_stream.h"
#include <algorithm>
#include <cstring>

bool WavStream::open(const char *name, FileSystem *fs) {
	ready = false;
	file.close();
	rate = start = length = position = channels = 0;
	if (!file.open(name, "rb", fs))
		return false;
	uint8_t header[12];
	if (file.read(header, 12) != 12 || memcmp(header, "RIFF", 4) || memcmp(header + 8, "WAVE", 4))
		return false;
	const uint64_t end = uint64_t(READ_LE_UINT32(header + 4)) + 8;
	if (end > file.size() || end > 1024 * 1024 * 1024 || end < 12)
		return false;
	bool format = false, data = false;
	for (uint32_t offset = 12; offset < end;) {
		uint8_t chunk[8];
		file.seek(offset);
		if (end - offset < 8 || file.read(chunk, 8) != 8)
			return false;
		const uint32_t size = READ_LE_UINT32(chunk + 4);
		const uint64_t next = uint64_t(offset) + 8 + size + (size & 1);
		if (next > end)
			return false;
		if (!memcmp(chunk, "fmt ", 4)) {
			uint8_t fmt[16];
			if (format || size < 16 || file.read(fmt, 16) != 16 || READ_LE_UINT16(fmt) != 1)
				return false;
			channels = READ_LE_UINT16(fmt + 2);
			rate = READ_LE_UINT32(fmt + 4);
			if ((channels != 1 && channels != 2) || rate < 8000 || rate > 192000 ||
			    READ_LE_UINT16(fmt + 14) != 16 || READ_LE_UINT16(fmt + 12) != channels * 2 ||
			    READ_LE_UINT32(fmt + 8) != rate * channels * 2)
				return false;
			format = true;
		} else if (!memcmp(chunk, "data", 4)) {
			if (data || !size)
				return false;
			start = offset + 8;
			length = size;
			data = true;
		}
		offset = uint32_t(next);
	}
	if (!format || !data || length % (channels * 2) || file.ioErr())
		return false;
	file.seek(start);
	ready = !file.ioErr();
	return ready;
}

int WavStream::read(int16_t *out, int frames) {
	if (!ready || frames <= 0)
		return 0;
	int produced = 0;
	while (produced < frames) {
		if (position == length) {
			file.seek(start);
			position = 0;
		}
		const unsigned count =
		    std::min({unsigned(frames - produced), unsigned(buffer.size() / (channels * 2)),
			          (length - position) / (channels * 2)});
		if (!count || file.read(buffer.data(), count * channels * 2) != count * channels * 2)
			break;
		for (unsigned i = 0; i < count; ++i) {
			const auto *p = buffer.data() + i * channels * 2;
			for (unsigned c = 0; c < 2; ++c) {
				const int sample = int16_t(READ_LE_UINT16(p + (channels == 1 ? 0 : c * 2)));
				*out = int16_t(std::clamp(int(*out) + sample * 3 / 4, -32768, 32767));
				++out;
			}
		}
		produced += count;
		position += count * channels * 2;
	}
	return produced;
}
