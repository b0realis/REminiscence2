// SPDX-License-Identifier: GPL-3.0-or-later
// Macintosh formats researched in REminiscence by Gregory Montoir.
#include "decode_mac.h"
#include "appearance.h"
#include "byte_reader.h"
#include <algorithm>
#include <array>
#include <cstdlib>

namespace {
constexpr size_t kMaximumImageBytes = 16 * 1024 * 1024;

bool decodeC103(ByteReader &input, std::vector<uint8_t> &output) {
	std::array<uint8_t, 4096> window = {};
	size_t cursor = 0, produced = 0;
	unsigned flags = 0, bits = 0;
	while (produced < output.size()) {
		if (!bits) {
			flags = input.u8();
			bits = 8;
		}
		const bool copy = flags & 1;
		flags >>= 1;
		--bits;
		if (!copy) {
			const uint8_t color = input.u8();
			if (!input.good()) {
				return false;
			}
			output[produced++] = color;
			window[cursor++ & 4095] = color;
		} else {
			const uint16_t command = input.be16();
			const size_t distance = (command & 4095) + 1;
			if (!input.good() || distance > std::min(produced, window.size())) {
				return false;
			}
			const size_t count = std::min<size_t>((command >> 12) + 3, output.size() - produced);
			for (size_t n = 0; n < count; ++n) {
				const uint8_t color = window[(cursor - distance) & 4095];
				output[produced++] = color;
				window[cursor++ & 4095] = color;
			}
		}
	}
	return true;
}

bool decodeC211(ByteReader &input, MacImage &image) {
	struct Repeat {
		size_t position;
		unsigned remaining;
	};
	std::vector<Repeat> loops;
	int x = 0, y = 0;
	// Bound nested empty loops as well as pixel writes.
	size_t budget =
	    std::min<size_t>(64 * 1024 * 1024, 1024 + image.pixels.size() * 8 + input.remaining() * 8);
	while (budget--) {
		const uint8_t command = input.u8();
		if (command & 0x80) {
			++y;
			x = 0;
		}
		unsigned count = command & 31;
		if (!count) {
			count = input.be16();
		}
		if (!input.good() || !count) {
			return false;
		}
		const unsigned operation = command & 0x60;
		if (operation == 0x40 && count == 1) {
			return loops.empty();
		}
		if (operation == 0) {
			if (count == 1) {
				if (loops.empty()) {
					return false;
				}
				if (--loops.back().remaining) {
					input.seek(loops.back().position);
				} else {
					loops.pop_back();
				}
			} else {
				if (loops.size() == 512) {
					return false;
				}
				loops.push_back({input.position(), count});
			}
			continue;
		}
		if (y >= image.height || count > unsigned(image.width - x)) {
			return false;
		}
		uint8_t *destination = image.pixels.data() + size_t(y) * image.width + x;
		if (operation == 0x40) {
			const uint8_t color = input.u8();
			if (!input.good()) {
				return false;
			}
			std::fill_n(destination, count, color);
		} else if (operation == 0x60 && !input.read(destination, count)) {
			return false;
		}
		x += count; // 0x20 leaves transparent pixels initialized to zero.
	}
	return false;
}
} // namespace

uint8_t *decodeLzss(File &file, uint32_t encodedSize, uint32_t &decodedSize) {
	decodedSize = 0;
	if (encodedSize < 4 || encodedSize > kMaximumImageBytes) {
		return nullptr;
	}
	std::vector<uint8_t> encoded(encodedSize);
	if (file.read(encoded.data(), encodedSize) != encodedSize) {
		return nullptr;
	}
	ByteReader input(encoded.data(), encoded.size());
	const uint32_t length = input.be32();
	if (!length || length > kMaximumImageBytes) {
		return nullptr;
	}
	std::vector<uint8_t> output(length);
	if (!decodeC103(input, output)) {
		return nullptr;
	}
	uint8_t *result = static_cast<uint8_t *>(std::malloc(length));
	if (result) {
		std::copy(output.begin(), output.end(), result);
		decodedSize = length;
	}
	return result;
}

bool macImageFrameOffset(const uint8_t *data, size_t size, int frame, size_t &offset) {
	ByteReader input(data, size);
	const uint16_t signature = input.be16(), count = input.be16();
	if ((signature != 0xC103 && signature != 0xC211) || frame < 0 || frame >= count ||
	    size < 8 + size_t(count) * 4) {
		return false;
	}
	input.seek(8 + size_t(frame) * 4);
	offset = input.be32();
	return input.good() && offset >= 8 + size_t(count) * 4 && offset <= size &&
	       size - offset >= (signature == 0xC211 ? 8u : 4u);
}

bool decodeMacImage(const uint8_t *data, size_t size, int frame, MacImage &image) {
	image = {};
	size_t offset = 0;
	if (!macImageFrameOffset(data, size, frame, offset)) {
		return false;
	}
	size_t end = size;
	ByteReader offsets(data, size);
	offsets.seek(2);
	const unsigned count = offsets.be16();
	offsets.seek(8);
	for (unsigned i = 0; i < count; ++i) {
		const uint32_t other = offsets.be32();
		if (other > offset && other < end) {
			end = other;
		}
	}
	ByteReader input(data + offset, end - offset);
	MacImage decoded;
	decoded.width = input.be16();
	decoded.height = input.be16();
	if (!decoded.width || !decoded.height ||
	    size_t(decoded.width) * decoded.height > kMaximumImageBytes) {
		return false;
	}
	decoded.pixels.resize(size_t(decoded.width) * decoded.height);
	if (data[0] == 0xC2) {
		decoded.originX = static_cast<int16_t>(input.be16());
		decoded.originY = static_cast<int16_t>(input.be16());
		if (!decodeC211(input, decoded)) {
			return false;
		}
	} else if (!decodeC103(input, decoded.pixels)) {
		return false;
	}
	image = std::move(decoded);
	return true;
}

void positionMacSprite(const MacImage &image, bool flip, int &x, int &y) {
	x += flip ? image.originX - image.width - 1 : -image.originX;
	y -= image.originY;
}

void blitMacImage(const MacImage &image, const DecodeBuffer &dst) {
	if (!dst.ptr || dst.w <= 0 || dst.h <= 0 || dst.pitch < dst.w || image.width <= 0 ||
	    image.height <= 0 || size_t(image.width) * image.height > image.pixels.size()) {
		return;
	}
	const int left = std::max(0, dst.x), top = std::max(0, dst.y);
	const int right = std::min<int64_t>(dst.w, int64_t(dst.x) + image.width),
	          bottom = std::min<int64_t>(dst.h, int64_t(dst.y) + image.height);
	for (int y = top; y < bottom; ++y) {
		uint8_t *row = dst.ptr + size_t(y) * dst.pitch;
		const uint8_t *source = image.pixels.data() + size_t(y - dst.y) * image.width;
		for (int x = left; x < right; ++x) {
			const int sx = dst.xflip ? image.width - 1 - (x - dst.x) : x - dst.x;
			uint8_t color = source[sx];
			if ((dst.transparent && !color) || (dst.respectMask && (row[x] & 0x80))) {
				continue;
			}
			if (dst.font) {
				if (color == 0xC0) {
					color = dst.shadowColor;
				} else if (color == 0xC1) {
					color = dst.frontColor;
				} else {
					continue;
				}
			}
			row[x] = appearance::shirtPixel(false, color, dst.conrad);
		}
	}
}
