#include "scaler_xbr.h"
#include <algorithm>
#include <vector>

static void xbrNx(int factor, uint32_t *dst, int dstPitch, const uint32_t *src, int srcPitch, int w,
                  int h) {
	// Pitches in the engine's Scaler interface are pixels, not bytes. xBRZ's
	// scale() requires packed rows; SDL texture locks may include row padding.
	// Bound multiplication before entering the inherited integer-indexed code.
	if (!src || !dst || factor < 2 || factor > 6 || w <= 0 || h <= 0 || w > 4096 || h > 4096 ||
	    srcPitch < w || dstPitch < w * factor || size_t(w) * h * factor * factor > 16 * 1024 * 1024)
		return;
	thread_local std::vector<uint32_t> input, output;
	if (srcPitch != w) {
		input.resize(size_t(w) * h);
		for (int y = 0; y < h; ++y)
			std::copy_n(src + size_t(y) * srcPitch, w, input.data() + size_t(y) * w);
		src = input.data();
	}
	const int width = w * factor, height = h * factor;
	uint32_t *packed = dst;
	if (dstPitch != width) {
		output.resize(size_t(width) * height);
		packed = output.data();
	}
	xbrz::scale(factor, src, packed, w, h, xbrz::ColorFormat::RGB);
	if (packed != dst)
		for (int y = 0; y < height; ++y)
			std::copy_n(packed + size_t(y) * width, width, dst + size_t(y) * dstPitch);
}

// Static lifetime also avoids leaking a descriptor when loaded as a plugin.
const Scaler scaler_xbr = {SCALER_TAG, "xbrz", 2, 6, xbrNx};

const Scaler EXPORT *getScaler() {
	return &scaler_xbr;
}
