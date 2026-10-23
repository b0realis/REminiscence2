// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_APPEARANCE_H
#define REMINISCENCE2_APPEARANCE_H
#include "intern.h"
#include <array>

namespace appearance {
enum class Shirt { Original, RedPink, White };
// Gameplay leaves palette bank 6 free (Mac explicitly clears it). Reserve two
// entries for Conrad's clothing only; never modify the shared source palette.
constexpr uint8_t ShirtShadow = 0x6C, ShirtLight = 0x6D;
uint8_t shirtPixel(ResourceType edition, uint8_t color, bool conrad);
// Amiga reuses its light T-shirt shades for footwear and equipment. Tag only
// light components bordered by the jacket, before clipping or mirroring.
size_t markAmigaTshirt(uint8_t *pixels, int width, int height, size_t capacity);
std::array<Color, 2> shirtColors(ResourceType edition, Shirt choice,
                                 const std::array<Color, 2> &original);
} // namespace appearance
#endif
