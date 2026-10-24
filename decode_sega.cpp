// SPDX-License-Identifier: GPL-3.0-or-later
// Format researched in REminiscence by Gregory Montoir.
#include "decode_sega.h"
#include "intern.h"
#include "byte_reader.h"
#include <array>
#include <algorithm>

bool decodeSegaCDCharacter(const uint8_t *source, size_t size, uint8_t *output, size_t capacity) {
	if (!source || size < 6 || !output || capacity < 32 * 48)
		return false;
	// CD sprites use split-stream LZSS: MSB-first flags and literal bytes,
	// followed by big-endian references (9-bit distance, 7-bit length).
	size_t in = 6, refs = 6 + READ_BE_UINT16(source + 4), produced = 0;
	const size_t literalEnd = refs;
	if (refs > size)
		return false;
	std::array<uint8_t, 768> packed{};
	while (produced < packed.size()) {
		if (in >= literalEnd)
			return false;
		const unsigned flags = source[in++];
		for (unsigned bit = 0; bit < 8 && produced < packed.size(); ++bit) {
			if (flags & (128u >> bit)) {
				if (in >= literalEnd)
					return false;
				packed[produced++] = source[in++];
			} else {
				if (refs > size || size - refs < 2)
					return false;
				const unsigned pair = READ_BE_UINT16(source + refs);
				refs += 2;
				const unsigned distance = (pair >> 7) + 1, count = (pair & 127) + 2;
				if (distance > produced || count > packed.size() - produced)
					return false;
				for (unsigned i = 0; i < count; ++i) {
					packed[produced] = packed[produced - distance];
					++produced;
				}
			}
		}
	}
	return decodeSegaTiles(packed.data(), 384, 32, 24, output, capacity) &&
	       decodeSegaTiles(packed.data() + 384, 384, 32, 24, output + 768, capacity - 768);
}

bool decodeSegaTiles(const uint8_t *source, size_t size, int width, int height, uint8_t *output,
                     size_t capacity) {
	if (!source || !output || width <= 0 || height <= 0 || width > 512 || height > 512 ||
	    (width & 7) || (height & 7)) {
		return false;
	}
	const size_t pixels = size_t(width) * height;
	if (pixels > capacity || pixels / 2 > size) {
		return false;
	}
	for (int tileX = 0; tileX < width; tileX += 8) {
		for (int tileY = 0; tileY < height; tileY += 8) {
			for (int y = 0; y < 8; ++y) {
				uint8_t *row = output + size_t(tileY + y) * width + tileX;
				for (int x = 0; x < 8; x += 2) {
					const uint8_t pair = *source++;
					row[x] = pair >> 4;
					row[x + 1] = pair & 15;
				}
			}
		}
	}
	return true;
}
bool decodeSegaCharacter(const uint8_t *source, size_t size, uint8_t *output, size_t capacity) {
	constexpr size_t pixels = 32 * 48, packedSize = pixels / 2;
	if (!source || !output || size < 4 || capacity < pixels) {
		return false;
	}
	ByteReader input(source, size);
	input.skip(2);
	const size_t length = size_t(input.be16()) + 1;
	if (length > input.remaining()) {
		return false;
	}
	ByteReader commands(input.current(), length);
	std::array<uint8_t, packedSize> packed{};
	size_t produced = 0;
	while (commands.remaining()) {
		const uint8_t value = commands.u8();
		if ((value & 0xF0) == 0xF0) {
			const size_t count = size_t(commands.u8()) + 1;
			if (!commands.good() || count > packed.size() - produced) {
				return false;
			}
			std::fill_n(packed.data() + produced, count, uint8_t((value & 15) * 17));
			produced += count;
		} else {
			if (produced == packed.size() || (value & 15) == 15) {
				return false;
			}
			packed[produced++] = value;
		}
	}
	if (produced != packed.size()) {
		return false;
	}
	// The top and bottom 32x24 parts each restart column-major tile order.
	return decodeSegaTiles(packed.data(), packedSize / 2, 32, 24, output, capacity) &&
	       decodeSegaTiles(packed.data() + packedSize / 2, packedSize / 2, 32, 24,
	                       output + pixels / 2, capacity - pixels / 2);
}
