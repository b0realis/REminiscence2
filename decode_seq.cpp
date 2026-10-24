// SPDX-License-Identifier: GPL-3.0-or-later
// SEQ block semantics researched by Gregory Montoir in REminiscence.
#include "decode_seq.h"
#include "byte_reader.h"
#include <array>
#include <cstring>

namespace {
class Bits {
	ByteReader &_input;
	unsigned _bits = 0, _count = 0;

public:
	explicit Bits(ByteReader &input) : _input(input) {}
	unsigned get(unsigned count) {
		while (_count < count) {
			_bits |= unsigned(_input.u8()) << _count;
			_count += 8;
		}
		const unsigned result = _bits & ((1u << count) - 1);
		_bits >>= count;
		_count -= count;
		return result;
	}
};
bool runs(ByteReader &input, uint8_t *pixels) {
	Bits bits(input);
	std::array<int, 64> codes{};
	unsigned n = 0, total = 0;
	while (n < codes.size() && total < 64) {
		const unsigned x = bits.get(4);
		codes[n] = int(x ^ 8) - 8;
		total += codes[n] < 0 ? -codes[n] : codes[n];
		++n;
	}
	if (!input.good() || total != 64)
		return false;
	unsigned position = 0;
	for (unsigned i = 0; i < n; ++i) {
		const int code = codes[i];
		const unsigned len = code < 0 ? -code : code;
		if (code < 0)
			std::memset(pixels + position, input.u8(), len);
		else if (!input.read(pixels + position, len))
			return false;
		position += len;
	}
	return input.good();
}
bool block(ByteReader &input, unsigned opcode, uint8_t *dst) {
	if (opcode == 1) {
		const unsigned length = input.u8();
		if (length & 0x80) {
			std::array<uint8_t, 64> pixels{};
			if ((length & 3) != 1 && (length & 3) != 2)
				return false;
			if (!runs(input, pixels.data()))
				return false;
			for (int y = 0; y < 8; ++y)
				for (int x = 0; x < 8; ++x)
					dst[y * 256 + x] = pixels[(length & 3) == 1 ? y * 8 + x : x * 8 + y];
		} else {
			if (!length || length > 16)
				return false;
			std::array<uint8_t, 16> palette{};
			if (!input.read(palette.data(), length))
				return false;
			unsigned count = 1;
			while ((1u << count) < length)
				++count;
			Bits bits(input);
			for (int y = 0; y < 8; ++y)
				for (int x = 0; x < 8; ++x) {
					const unsigned index = bits.get(count);
					if (index >= length)
						return false;
					dst[y * 256 + x] = palette[index];
				}
		}
	} else if (opcode == 2) {
		for (int y = 0; y < 8; ++y)
			if (!input.read(dst + y * 256, 8))
				return false;
	} else if (opcode == 3) {
		unsigned position;
		do {
			position = input.u8();
			dst[((position >> 3) & 7) * 256 + (position & 7)] = input.u8();
			if (!input.good())
				return false;
		} while (!(position & 0x80));
	}
	return input.good();
}
} // namespace
bool decodeSeqFrame(const uint8_t *source, size_t size, uint8_t *destination, size_t capacity) {
	if (!source || size < 128 || !destination || capacity < 256 * 224)
		return false;
	ByteReader commands(source, 128), pixels(source + 128, size - 128);
	Bits bits(commands);
	for (int y = 0; y < 128; y += 8)
		for (int x = 0; x < 256; x += 8)
			if (!block(pixels, bits.get(2), destination + (48 + y) * 256 + x))
				return false;
	return commands.good() && pixels.good();
}
