// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_DECODE_SEGA_H
#define REMINISCENCE2_DECODE_SEGA_H
#include <cstddef>
#include <cstdint>
bool decodeSegaCDCharacter(const uint8_t *source, size_t size, uint8_t *output, size_t capacity);
// Mega Drive tiles are packed nibbles, ordered by columns of 8x8 tiles.
// These functions know the source and output extents and perform no I/O.
bool decodeSegaTiles(const uint8_t *source, size_t size, int width, int height, uint8_t *output,
                     size_t capacity);
bool decodeSegaCharacter(const uint8_t *source, size_t size, uint8_t *output, size_t capacity);
#endif
