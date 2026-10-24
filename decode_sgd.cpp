// SPDX-License-Identifier: GPL-3.0-or-later
// Planar and packed foreground formats researched by Gregory Montoir.
#include "decode_sgd.h"
#include "byte_reader.h"
#include <algorithm>
#include <array>
#include <cstring>
bool decodeAmigaRle(const uint8_t *source, size_t size, std::vector<uint8_t> &output,
                    size_t capacity) {
	if (!source || size < 2 || capacity > 1024 * 1024)
		return false;
	ByteReader header(source, size);
	const unsigned count = header.be16() & 0x7fff;
	if (count > header.remaining())
		return false;
	ByteReader r(header.current(), count);
	std::vector<uint8_t> decoded;
	while (r.remaining()) {
		const uint8_t code = r.u8();
		const unsigned length = code & 128 ? 1 - int(int8_t(code)) : code + 1;
		if (length > capacity - decoded.size())
			return false;
		if (code & 128) {
			const auto value = r.u8();
			if (!r.good())
				return false;
			decoded.insert(decoded.end(), length, value);
		} else {
			if (length > r.remaining())
				return false;
			decoded.insert(decoded.end(), r.current(), r.current() + length);
			r.skip(length);
		}
	}
	output = std::move(decoded);
	return true;
}
bool decodeSgdForeground(uint8_t *frame, const uint8_t *commands, size_t commandSize,
                         const uint8_t *tiles, size_t tileSize, bool amiga, int level, int room) {
	if (!frame || !commands || !tiles || commandSize < 2 || tileSize < 4)
		return false;
	ByteReader list(commands, commandSize);
	const unsigned count = list.be16();
	if (count > list.remaining() / 6)
		return false;
	std::array<uint8_t, 256 * 224> result;
	std::copy_n(frame, result.size(), result.data());
	std::vector<uint8_t> tile;
	int previous = -1;
	for (unsigned n = 0; n < count; ++n) {
		const unsigned entry = list.be16();
		const int x0 = int16_t(list.be16());
		int y0 = int16_t(list.be16());
		unsigned id = entry;
		if (id == 0xffff) {
			if (previous < 0)
				return false;
			id = previous;
		} else
			id &= 0x7fff;
		if (int(id) != previous) {
			if (id >= tileSize / 4)
				return false;
			ByteReader directory(tiles + id * 4, 4);
			const int64_t offset = int32_t(directory.be32());
			const uint64_t position = offset < 0 ? uint64_t(-offset) : uint64_t(offset);
			if (position > tileSize || tileSize - position < 2)
				return false;
			if (offset < 0) {
				ByteReader raw(tiles + position, tileSize - position);
				const unsigned length = raw.be16();
				if (length > 8192 || length > raw.remaining())
					return false;
				tile.assign(raw.current(), raw.current() + length);
			} else if (!decodeAmigaRle(tiles + position, tileSize - position, tile, 8192))
				return false;
			previous = id;
		}
		if (tile.size() < 4)
			return false;
		const unsigned w = (unsigned(tile[0]) + 1) / 2, h = unsigned(tile[1]) + 1;
		const unsigned plane = unsigned(tile[2]) * 256 + tile[3];
		if (!w || plane < w * 2 * h || plane > (tile.size() - 4) / 5)
			return false;
		if (level == 0 && room == 26 && id == 38 && y0 == 176)
			y0 += 8;
		const auto *mask = tile.data() + 4, *colors = mask + plane;
		auto put = [&](int x, int y, uint8_t color) {
			if (x >= 0 && x < 256 && y >= 0 && y < 224)
				result[y * 256 + x] = color;
		};
		for (unsigned y = 0; y < h; ++y) {
			if (amiga) {
				for (unsigned b = 0; b < w * 2; ++b)
					for (unsigned bit = 0; bit < 8; ++bit) {
						const unsigned at = y * w * 2 + b, flag = 1 << (7 - bit);
						if (!(mask[at] & flag))
							continue;
						uint8_t color = 0;
						for (unsigned p = 0; p < 4; ++p)
							if (colors[at + p * plane] & flag)
								color |= 1 << p;
						put(x0 + int(b * 8 + bit), y0 + int(y), color);
					}
			} else {
				for (unsigned x = 0; x < w; ++x)
					for (unsigned bit = 0; bit < 8; ++bit) {
						const unsigned at = y * w * 2 + x * 2,
						               flags = unsigned(mask[at]) * 256 + mask[at + 1];
						const uint8_t color = colors[(y * w + x) * 8 + bit];
						const int px = x0 + int(2 * (x * 8 + bit));
						if (flags & (1 << (15 - bit * 2)))
							put(px, y0 + int(y), color >> 4);
						if (flags & (1 << (14 - bit * 2)))
							put(px + 1, y0 + int(y), color & 15);
					}
			}
		}
	}
	std::copy(result.begin(), result.end(), frame);
	return true;
}
