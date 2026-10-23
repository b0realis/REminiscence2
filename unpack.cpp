
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

struct bitstream_t {
	uint16_t mask;
	int size;
	const uint8_t *src;
};

static uint8_t read_byte(struct bitstream_t *bs) {
	const uint8_t b = *(bs->src);
	++bs->src;
	return b;
}

static void fill(struct bitstream_t *bs) {
	if (bs->size < 9) {
		const uint8_t b = read_byte(bs);
		bs->mask |= b << (8 - bs->size);
		bs->size += 8;
	}
}

static int get_bit(struct bitstream_t *bs) {
	fill(bs);
	assert(bs->size > 0);
	const int val = (bs->mask & 0x8000) != 0;
	bs->mask <<= 1;
	--bs->size;
	return val;
}

static int get_bits(struct bitstream_t *bs, int count) {
	if (count > bs->size) {
		const int val = bs->mask >> (16 - bs->size);
		count -= bs->size;
		bs->size = 0;
		bs->mask = 0;
		fill(bs);
		return (val << count) | get_bits(bs, count);
	} else {
		assert(count <= bs->size);
		const int val = bs->mask >> (16 - count);
		bs->mask <<= count;
		bs->size -= count;
		return val;
	}
}

static uint8_t rol1(uint8_t r) {
	return (r << 1) | (r >> 7);
}

struct UnpackCtx {
	struct bitstream_t bs;
	uint8_t *dst;
	uint32_t dstSize, dstOffset;
};

static void outputb(UnpackCtx *ctx, uint8_t b) {
	assert(ctx->dstOffset < ctx->dstSize);
	ctx->dst[ctx->dstOffset++] = b;
}

static void outputs(UnpackCtx *ctx, int offset, int count) {
	assert(ctx->dstOffset + count <= ctx->dstSize);
	for (int i = 0; i < count; ++i) {
		ctx->dst[ctx->dstOffset + i] = ctx->dst[ctx->dstOffset + offset + i];
	}
	ctx->dstOffset += count;
}

static int decode_bs(UnpackCtx *ctx, const uint8_t *src, int size) {
	struct bitstream_t *bs = &ctx->bs;
	bs->mask = 0;
	bs->size = 0;
	bs->src = src;

	uint8_t key = read_byte(bs);
	while (size != 0) {
		if (get_bit(bs) == 0) {
			const uint8_t val = get_bits(bs, 8);
			outputb(ctx, val ^ key);
			--size;
			if (size == 0) {
				break;
			}
			key = rol1(val);
			continue;
		}
		int count = 1;
		while (count <= 8 && get_bit(bs)) {
			++count;
		}
		const int count2 = get_bits(bs, count) + 3;
		const int offset = get_bits(bs, 13);
		outputs(ctx, -offset - 1, count2);
		size -= count2;
	}
	const int read = bs->src - src;
	debug(DBG_UNPACK, "end %d bytes, out %d", read, ctx->dstOffset);
	return read;
}

bool pc98_unpack(uint8_t *dst, int dstSize, const uint8_t *src, int srcSize) {
	UnpackCtx ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.dst = dst;
	ctx.dstSize = dstSize;
	ctx.dstOffset = 0;
	return decode_bs(&ctx, src, dstSize);
}
