
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "unpack.h"
#include "util.h"

// Reverse LZ decoding uses indexes rather than pointers before either buffer.
// A reference can only address bytes already produced at the output tail.
struct BytekillerCtx {
	uint32_t size, outputSize, crc, bits;
	uint8_t *dst;
	const uint8_t *src;
	size_t sourcePosition;
	bool good;
};

static int nextBit(BytekillerCtx *context) {
	if (!context->good) {
		return 0;
	}
	int bit = context->bits & 1;
	context->bits >>= 1;
	if (!context->bits) {
		if (context->sourcePosition < 4) {
			context->good = false;
			return 0;
		}
		context->sourcePosition -= 4;
		const uint32_t word = READ_BE_UINT32(context->src + context->sourcePosition);
		context->crc ^= word;
		bit = word & 1;
		context->bits = (uint32_t(1) << 31) | (word >> 1);
	}
	return bit;
}

template <int count> static uint32_t getBits(BytekillerCtx *context) {
	uint32_t value = 0;
	for (int i = 0; i < count && context->good; ++i) {
		value = (value << 1) | nextBit(context);
	}
	return value;
}
static void copyLiteral(BytekillerCtx *context, uint32_t length) {
	length = MIN(length, context->size);
	for (uint32_t i = 0; i < length && context->good; ++i) {
		const uint8_t byte = getBits<8>(context);
		if (context->good) {
			context->dst[--context->size] = byte;
		}
	}
}
static void copyReference(BytekillerCtx *context, uint32_t length, uint32_t distance) {
	if (!distance || distance > context->outputSize - context->size) {
		context->good = false;
		return;
	}
	length = MIN(length, context->size);
	for (uint32_t i = 0; i < length; ++i) {
		--context->size;
		context->dst[context->size] = context->dst[context->size + distance];
	}
}
bool bytekiller_unpack(uint8_t *dst, int capacity, const uint8_t *src, int encodedSize) {
	if (!src || encodedSize < 12 || capacity < 0) {
		return false;
	}
	const uint32_t length = READ_BE_UINT32(src + encodedSize - 4);
	if (length > unsigned(capacity) || (length && !dst)) {
		return false;
	}
	const uint32_t bits = READ_BE_UINT32(src + encodedSize - 12);
	BytekillerCtx context{length,
	                      length,
	                      READ_BE_UINT32(src + encodedSize - 8) ^ bits,
	                      bits,
	                      dst,
	                      src,
	                      size_t(encodedSize - 12),
	                      true};
	while (context.size && context.good) {
		if (!nextBit(&context)) {
			if (!nextBit(&context)) {
				copyLiteral(&context, getBits<3>(&context) + 1);
			} else {
				copyReference(&context, 2, getBits<8>(&context));
			}
		} else {
			switch (getBits<2>(&context)) {
			case 3:
				copyLiteral(&context, getBits<8>(&context) + 9);
				break;
			case 2: {
				const uint32_t count = getBits<8>(&context) + 1;
				copyReference(&context, count, getBits<12>(&context));
				break;
			}
			case 1:
				copyReference(&context, 4, getBits<10>(&context));
				break;
			case 0:
				copyReference(&context, 3, getBits<9>(&context));
				break;
			}
		}
	}
	return context.good && !context.size && !context.crc;
}

// PC-98 stream: rolling-key literals and overlapping backward references.
// Consume bits MSB-first without speculative reads beyond the supplied span.
bool pc98_unpack(uint8_t *dst, int dstSize, const uint8_t *src, int srcSize) {
	if (!dst || !src || dstSize <= 0 || srcSize < 2) return false;
	size_t bit = 8;
	bool valid = true;
	auto bits = [&](int count) {
		unsigned value = 0;
		for (int i = 0; i < count; ++i) {
			if (bit >= size_t(srcSize) * 8) { valid = false; return 0u; }
			value = (value << 1) | ((src[bit / 8] >> (7 - bit % 8)) & 1);
			++bit;
		}
		return value;
	};
	uint8_t key = src[0];
	int written = 0;
	while (written < dstSize) {
		if (bits(1) == 0) {
			const uint8_t value = bits(8);
			if (!valid) return false;
			dst[written++] = value ^ key;
			key = uint8_t((value << 1) | (value >> 7));
		} else {
			int countBits = 1;
			while (countBits <= 8 && bits(1)) ++countBits;
			const int count = int(bits(countBits)) + 3;
			const int distance = int(bits(13)) + 1;
			if (!valid || distance > written || count > dstSize - written) return false;
			for (int i = 0; i < count; ++i) {
				dst[written] = dst[written - distance];
				++written;
			}
		}
	}
	return valid;
}
