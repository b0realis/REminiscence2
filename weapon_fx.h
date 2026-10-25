// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_WEAPON_FX_H
#define REMINISCENCE2_WEAPON_FX_H
#include "intern.h"
#include <array>
#include <vector>

namespace weapon {
enum class Style { Original, Heavy, Laser };
enum class LaserColor { Orange, Red, Green, Blue, Purple };
constexpr int PaletteBase = 0x60; // Free gameplay bank, below the shirt aliases.
const std::array<Color, 6> &palette(LaserColor color = LaserColor::Orange);
const std::vector<uint8_t> &laserSample(); // Synthesized fallback: signed 8-bit PCM, 22050 Hz.

// Presentation only: no actor pointers, RNG, collision data or serialized state.
struct Shot {
	int age = 16, x = 0, y = 0, direction = 1, room = -1;
	int flashX = 0, flashY = 0; // Native render pixels, including odd Mac coordinates.
	Style style = Style::Original;
	int rayLength = -1; // Native render pixels; -1 until the original gun query runs.
	bool impact = false;
};
struct State {
	std::array<Shot, 4> shots{};
	unsigned next = 0;
	int poseType = -1, poseAge = 16, direction = 1;
	Style style = Style::Original;
	bool nativeFlashMatched = false;
	void clear() { *this = State{}; }
	void tick();
	// x/y are the native flash's rear edge and centerline in render pixels.
	void fire(Style choice, int x, int y, int dir, int room, int type);
	void followFlash(int x, int y);
	void trace(int endX, bool impact);
	int recoil(int type) const;
	void draw(uint8_t *pixels, int width, int height, int scale, int room) const;
};
} // namespace weapon
#endif
