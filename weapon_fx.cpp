// SPDX-License-Identifier: GPL-3.0-or-later
#include "weapon_fx.h"
#include <algorithm>
#include <cmath>

namespace weapon {
const std::array<Color, 6> &palette(LaserColor color) {
	// Dark halo, saturated beam and pale core retain contrast for each hue.
	// Heavy's red palette is independent of the selected laser colour.
	static const std::array<std::array<Color, 6>, 5> colors{{{{{112, 12, 24},
	                                                           {235, 38, 47},
	                                                           {255, 149, 120},
	                                                           {114, 42, 8},
	                                                           {255, 117, 12},
	                                                           {255, 225, 136}}},
	                                                         {{{112, 12, 24},
	                                                           {235, 38, 47},
	                                                           {255, 149, 120},
	                                                           {108, 12, 20},
	                                                           {255, 35, 45},
	                                                           {255, 177, 180}}},
	                                                         {{{112, 12, 24},
	                                                           {235, 38, 47},
	                                                           {255, 149, 120},
	                                                           {10, 82, 30},
	                                                           {42, 244, 82},
	                                                           {194, 255, 184}}},
	                                                         {{{112, 12, 24},
	                                                           {235, 38, 47},
	                                                           {255, 149, 120},
	                                                           {14, 35, 108},
	                                                           {55, 125, 255},
	                                                           {183, 224, 255}}},
	                                                         {{{112, 12, 24},
	                                                           {235, 38, 47},
	                                                           {255, 149, 120},
	                                                           {66, 15, 96},
	                                                           {179, 65, 255},
	                                                           {237, 190, 255}}}}};
	const int choice = int(color);
	return colors[choice >= 0 && choice < int(colors.size()) ? choice : 0];
}
const std::vector<uint8_t> &laserSample() {
	// Missing-asset fallback: a dry, short electrical pulse with a narrow pitch
	// movement and rough harmonics, rather than a wide descending "pew".
	// Generated once outside the audio callback; its storage outlives every voice.
	static const std::vector<uint8_t> pcm = [] {
		std::vector<uint8_t> data(1985); // 90 ms at 22050 Hz.
		double phase = 0;
		for (size_t i = 0; i < data.size(); ++i) {
			const double t = double(i) / (data.size() - 1);
			const double frequency = 680 + 120 * std::exp(-6 * t);
			phase += 6.283185307179586 * frequency / 22050;
			const double envelope = std::min(1.0, double(i) / 22) * std::exp(-4 * t) * (1 - t);
			const int sample =
			    int(75 * envelope *
				    (std::sin(phase) + .3 * std::sin(3 * phase) + .15 * std::sin(5 * phase)));
			data[i] = uint8_t(int8_t(std::clamp(sample, -127, 127)));
		}
		return data;
	}();
	return pcm;
}
void State::tick() {
	nativeFlashMatched = false;
	poseAge = std::min(poseAge + 1, 16);
	for (auto &s : shots)
		s.age = std::min(s.age + 1, 16);
}
void State::fire(Style choice, int x, int y, int dir, int room, int type) {
	if (choice == Style::Original)
		return;
	nativeFlashMatched = true;
	style = choice;
	poseType = type;
	poseAge = 0;
	direction = dir;
	shots[next++ % shots.size()] = {0, x, y, dir, room, x, y, choice};
}
void State::followFlash(int x, int y) {
	if (next && poseAge < 2) {
		nativeFlashMatched = true;
		auto &shot = shots[(next - 1) % shots.size()];
		shot.flashX = x;
		shot.flashY = y;
	}
}
void State::trace(int endX, bool impact) {
	if (!next || poseAge > 6)
		return;
	auto &shot = shots[(next - 1) % shots.size()];
	if (shot.style != Style::Laser)
		return;
	const int length = std::max(0, (endX - shot.x) * shot.direction);
	if (shot.rayLength < 0 || length < shot.rayLength) {
		shot.rayLength = length;
		shot.impact = impact;
	} else if (length == shot.rayLength) {
		shot.impact |= impact;
	}
}
int State::recoil(int type) const {
	if (poseAge >= 6 || type != poseType)
		return 0;
	static const int heavy[] = {3, 3, 2, 1, 0, 0};
	return style == Style::Heavy ? -direction * heavy[poseAge] : 0;
}
void State::draw(uint8_t *pixels, int width, int height, int scale, int room) const {
	if (!pixels || scale < 1 || scale > 2 || width <= 0 || height <= 0)
		return;
	auto dot = [&](int x, int y, int color) {
		for (int dy = 0; dy < scale; ++dy)
			for (int dx = 0; dx < scale; ++dx) {
				const int xx = x + dx, yy = y + dy;
				if (xx >= 0 && xx < width && yy >= 0 && yy < height) {
					auto &p = pixels[yy * width + xx];
					if (!(p & 0x80))
						p = uint8_t(PaletteBase + color);
				}
			}
	};
	for (const auto &s : shots) {
		if (s.age >= 12 || s.room != room)
			continue;
		const bool laser = s.style == Style::Laser;
		const int base = laser ? 3 : 0;
		// Start at the native flash position. Extra body recoil must not move
		// the shot origin or the laser trajectory away from that reference.
		if (s.age < 2) {
			for (int x = 0; x < (laser ? 6 : 13); ++x) {
				const int spread = x < 5 ? (laser ? 2 : 3) : 1;
				for (int y = -spread; y <= spread; ++y)
					dot(s.flashX + s.direction * x * scale, s.flashY + y * scale,
					    base + (y == 0 ? 2 : 1));
			}
		}
		if (laser) {
			// The original script resolves damage immediately after the flash.
			// Wait for that ray, then animate only as far as its actual endpoint.
			const int head = std::max(0, s.age - 2) * 24;
			for (int x = 0; x < 15 && (head + x) * scale <= s.rayLength; ++x) {
				dot(s.x + s.direction * (head + x) * scale, s.y - scale, 3);
				dot(s.x + s.direction * (head + x) * scale, s.y + scale, 3);
				dot(s.x + s.direction * (head + x) * scale, s.y, x > 10 ? 5 : 4);
			}
			if (s.impact && (head + 15) * scale >= s.rayLength && head * scale <= s.rayLength + 24 * scale) {
				const int end = s.x + s.direction * s.rayLength;
				for (int y = -2; y <= 2; ++y)
					dot(end, s.y + y * scale, y == 0 ? 5 : 4);
			}
			if (s.age < 3)
				dot(s.x - s.direction * 5 * scale, s.y + scale, 4);
		} else if (s.age < 10) {
			// One enlarged red casing follows a deterministic backward/upward arc.
			const int x = s.x - s.direction * (8 + 2 * s.age) * scale;
			const int y = s.y + (-2 - 2 * s.age + s.age * s.age / 3) * scale;
			dot(x, y, 1);
			dot(x + scale, y, 2);
			dot(x, y + scale, 0);
		}
	}
}
} // namespace weapon
