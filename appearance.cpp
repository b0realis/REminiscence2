// SPDX-License-Identifier: GPL-3.0-or-later
#include "appearance.h"
#include <algorithm>
#include <vector>

namespace appearance {
uint8_t shirtPixel(ResourceType edition, uint8_t color, bool conrad) {
	if (conrad) {
		// Amiga clothing is tagged in the decoded frame. Index 7 is the jacket
		// and must stay brown; untagged light pixels include shoes/equipment.
		if (edition == kResourceTypeAmiga) {
			if (color == 0x7C)
				return ShirtShadow;
			if (color == 0x7D)
				return ShirtLight;
			return color;
		}
		if (color == 0x4C)
			return ShirtShadow;
		if (color == 0x4D)
			return ShirtLight;
	}
	return color;
}
size_t markAmigaTshirt(uint8_t *pixels, int width, int height, size_t capacity) {
	if (!pixels || width <= 0 || height <= 0 || width > 32 || height > 127 ||
	    size_t(width) * height > capacity)
		return 0;
	const size_t count = size_t(width) * height;
	std::vector<uint8_t> seen(count, 0);
	std::vector<int> component;
	component.reserve(count);
	size_t tagged = 0;
	auto light = [](uint8_t color) { return color == 1 || color == 5; };
	for (size_t start = 0; start < count; ++start) {
		if (seen[start] || !light(pixels[start]))
			continue;
		component.clear();
		component.push_back(start);
		seen[start] = 1;
		int jacketEdges = 0;
		for (size_t cursor = 0; cursor < component.size(); ++cursor) {
			const int index = component[cursor], x = index % width, y = index / width;
			for (int neighbour :
			     {x > 0 ? index - 1 : -1, x + 1 < width ? index + 1 : -1,
			      y > 0 ? index - width : -1, y + 1 < height ? index + width : -1}) {
				if (neighbour < 0)
					continue;
				jacketEdges += pixels[neighbour] == 7;
				if (!seen[neighbour] && light(pixels[neighbour])) {
					seen[neighbour] = 1;
					component.push_back(neighbour);
				}
			}
		}
		// T-shirt fabric sits against the jacket. Soles meet trousers or the
		// silhouette; metal highlights meet the hand. Two contacts also avoid
		// accepting a single incidental corner in a folded/rolling pose.
		if (jacketEdges >= 2) {
			for (int index : component)
				pixels[index] = pixels[index] == 1 ? ShirtLight : ShirtShadow;
			tagged += component.size();
		}
	}
	return tagged;
}
std::array<Color, 2> shirtColors(ResourceType edition, Shirt choice,
                                 const std::array<Color, 2> &original) {
	if (choice == Shirt::Original)
		return original;
	std::array<Color, 2> colors = choice == Shirt::White
	                                  ? std::array<Color, 2>{{{136, 136, 136}, {204, 204, 204}}}
	                                  : std::array<Color, 2>{{{153, 17, 68}, {187, 34, 85}}};
	const auto &light = original[1];
	const int brightest = std::max({light.r, light.g, light.b});
	const int darkest = std::min({light.r, light.g, light.b});
	// Keep room lighting, rather than turning a dark-room sprite into a lamp.
	// The reference is the native undershirt's red or neutral white ramp.
	// Original always preserves exact asset RGB, including the jacket.
	const int reference = brightest - darkest <= brightest / 6 ? 204 : 187;
	const int lighting = std::min(320, (brightest * 256 + reference / 2) / reference);
	for (auto &color : colors) {
		color.r = std::min(255, (color.r * lighting + 128) / 256);
		color.g = std::min(255, (color.g * lighting + 128) / 256);
		color.b = std::min(255, (color.b * lighting + 128) / 256);
	}
	return colors;
}
} // namespace appearance
