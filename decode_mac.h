// SPDX-License-Identifier: GPL-3.0-or-later
// Macintosh formats researched in REminiscence by Gregory Montoir.
#ifndef DECODE_MAC_H__
#define DECODE_MAC_H__
#include <cstddef>
#include <cstdint>
#include <vector>
#include "file.h"

uint8_t *decodeLzss(File &file, uint32_t encodedSize, uint32_t &decodedSize);
struct MacImage {
	int width = 0, height = 0;
	int16_t originX = 0, originY = 0;
	std::vector<uint8_t> pixels;
};
// Decode fully before touching the display, so failures leave it intact.
bool decodeMacImage(const uint8_t *data, size_t size, int frame, MacImage &image);
bool macImageFrameOffset(const uint8_t *data, size_t size, int frame, size_t &offset);
struct DecodeBuffer {
	uint8_t *ptr;
	int w, h, pitch;
	int x, y;
	bool xflip;
	bool transparent;
	bool respectMask;
	bool font;
	uint8_t frontColor, shadowColor;
	bool conrad = false;
};
void blitMacImage(const MacImage &image, const DecodeBuffer &destination);
#endif
