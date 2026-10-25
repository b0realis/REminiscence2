// SPDX-License-Identifier: GPL-3.0-or-later
#include "scaler.h"
#include "scalers/xbr/xbrz.h"
#include <algorithm>
#include <iostream>
#include <vector>

int main() {
	constexpr uint32_t guard = 0xDEADBEEF;
	int failures = 0;
	auto check = [&](bool ok, const char *why) {
		if (!ok) {
			++failures;
			std::cerr << why << '\n';
		}
	};
	// Compare the engine adapter with the original packed-row implementation.
	// Odd widths, narrow images and poisoned padding expose stride overruns.
	for (int factor = 2; factor <= 6; ++factor) {
		for (int w : {1, 7, 17}) {
			const int h = w == 1 ? 1 : 9, width = w * factor, height = h * factor;
			std::vector<uint32_t> source(w * h);
			for (int y = 0; y < h; ++y)
				for (int x = 0; x < w; ++x)
					source[y * w + x] = x > y ? 0xFFDD8844 : 0xFF223366;
			std::vector<uint32_t> expected(width * height);
			xbrz::scale(factor, source.data(), expected.data(), w, h, xbrz::ColorFormat::RGB);
			for (int pad : {0, 5}) {
				const int srcPitch = w + pad, dstPitch = width + pad;
				std::vector<uint32_t> input(srcPitch * h + 2, guard);
				std::vector<uint32_t> output(dstPitch * height + 2, guard);
				for (int y = 0; y < h; ++y)
					std::copy_n(source.data() + y * w, w, input.data() + 1 + y * srcPitch);
				const auto original = input;
				scaler_xbr.scale(factor, output.data() + 1, dstPitch, input.data() + 1, srcPitch, w,
				                 h);
				check(input == original, "xBRZ modified source pixels/padding");
				check(output.front() == guard && output.back() == guard,
				      "xBRZ escaped destination");
				for (int y = 0; y < height; ++y) {
					const auto row = output.begin() + 1 + y * dstPitch;
					check(std::equal(row, row + width, expected.begin() + y * width),
					      "xBRZ stride mismatch");
					check(std::all_of(row + width, row + dstPitch,
					                  [](uint32_t p) { return p == guard; }),
					      "xBRZ overwrote row padding");
				}
			}
		}
	}
	uint32_t pixel = guard;
	for (int factor : {0, 1, 7, 2147483647})
		scaler_xbr.scale(factor, &pixel, 1, &pixel, 1, 1, 1);
	scaler_xbr.scale(6, &pixel, 1, &pixel, 1, 4096, 4096);
	scaler_xbr.scale(2, &pixel, 1, &pixel, 1, -1, 1);
	check(pixel == guard, "xBRZ must reject invalid geometry before writing");
	std::cout << failures << " xBRZ failures\n";
	return failures ? 1 : 0;
}
