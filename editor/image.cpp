// SPDX-License-Identifier: GPL-3.0-or-later
#include "image.h"
#include "audio.h"
#include "atomic_file.h"
#include "file.h"
#include <zlib.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_MAX_DIMENSIONS 16384
#include "third_party/editor/stb_image.h"
namespace editor {
Image readImage(const std::filesystem::path &path) {
	std::error_code ec;
	const auto size = std::filesystem::file_size(path, ec);
	if (ec || size > 64 * 1024 * 1024)
		throw std::runtime_error("Image missing or exceeds 64 MiB: " + path.filename().u8string());
	std::ifstream f(path, std::ios::binary);
	std::vector<unsigned char> bytes(static_cast<size_t>(size));
	if (!f.read(reinterpret_cast<char *>(bytes.data()), bytes.size()))
		throw std::runtime_error("Cannot read image");
	int w, h, channels;
	if (!stbi_info_from_memory(bytes.data(), int(bytes.size()), &w, &h, &channels) || w < 1 ||
	    h < 1 || int64_t(w) * h > 32 * 1024 * 1024)
		throw std::runtime_error("Invalid image, or image exceeds 32 megapixels");
	auto *data = stbi_load_from_memory(bytes.data(), int(bytes.size()), &w, &h, &channels, 4);
	if (!data)
		throw std::runtime_error("Cannot decode PNG, JPEG or BMP image");
	Image image{w, h, std::vector<uint32_t>(size_t(w) * h)};
	for (size_t i = 0; i < image.pixels.size(); ++i)
		image.pixels[i] = rgba(data[i * 4], data[i * 4 + 1], data[i * 4 + 2], data[i * 4 + 3]);
	stbi_image_free(data);
	return image;
}
// Small RGBA PNG writer for decoded reference art; explicit byte order also
// preserves alpha on big-endian hosts. Use an atomic replacement for each file.
void writePNG(const Image &image, const std::filesystem::path &path) {
	if (image.width < 1 || image.height < 1 || image.width > 16384 || image.height > 16384 ||
	    int64_t(image.width) * image.height > 32 * 1024 * 1024 ||
	    image.pixels.size() != size_t(image.width) * image.height)
		throw std::runtime_error("Invalid PNG dimensions or pixel buffer");
	std::vector<uint8_t> scanlines(size_t(image.height) * (1 + image.width * 4));
	size_t at = 0;
	for (int y = 0; y < image.height; ++y) {
		scanlines[at++] = 0; // PNG filter None: exact pixels, compressed by zlib.
		for (int x = 0; x < image.width; ++x) {
			const uint32_t color = image.pixels[y * image.width + x];
			for (int shift = 0; shift < 32; shift += 8)
				scanlines[at++] = uint8_t(color >> shift);
		}
	}
	uLongf size = compressBound(scanlines.size());
	std::vector<uint8_t> packed(size);
	if (compress2(packed.data(), &size, scanlines.data(), scanlines.size(), Z_BEST_SPEED) != Z_OK)
		throw std::runtime_error("Could not compress PNG");
	packed.resize(size);
	std::string reason;
	if (!writeAtomicFile(
	        path.u8string(), false,
	        [&](File &file) {
		        const uint8_t signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
		        file.write(signature, sizeof(signature));
		        auto chunk = [&](const char *type, const std::vector<uint8_t> &data) {
			        file.writeUint32BE(uint32_t(data.size()));
			        file.write(type, 4);
			        if (!data.empty())
				        file.write(data.data(), data.size());
			        uLong crc = crc32(0, reinterpret_cast<const Bytef *>(type), 4);
			        if (!data.empty())
				        crc = crc32(crc, data.data(), data.size());
			        file.writeUint32BE(uint32_t(crc));
		        };
		        std::vector<uint8_t> header(13, 0);
		        for (int i = 0; i < 4; ++i) {
			        header[i] = uint8_t(image.width >> (24 - 8 * i));
			        header[4 + i] = uint8_t(image.height >> (24 - 8 * i));
		        }
		        header[8] = 8; // 8 bits per RGBA component.
		        header[9] = 6;
		        chunk("IHDR", header);
		        chunk("IDAT", packed);
		        chunk("IEND", {});
	        },
	        reason))
		throw std::runtime_error("Could not save PNG: " + reason);
}
uint32_t rgba(int r, int g, int b, int a) {
	return uint32_t(r) | (uint32_t(g) << 8) | (uint32_t(b) << 16) | (uint32_t(a) << 24);
}
void over(uint32_t &dst, uint32_t src) {
	const unsigned a = src >> 24;
	if (!a)
		return;
	if (a == 255) {
		dst = src;
		return;
	}
	const unsigned da = dst >> 24, oa = a + (da * (255 - a) + 127) / 255;
	uint32_t out = oa << 24;
	for (int shift : {0, 8, 16}) {
		const unsigned c = (((src >> shift) & 255) * a +
		                    (((dst >> shift) & 255) * da * (255 - a) + 127) / 255 + oa / 2) /
		                   oa;
		out |= c << shift;
	}
	dst = out;
}
void blit(Image &dst, const Image &src, const Art &crop, int x, int y, int w, int h) {
	if (w <= 0 || h <= 0)
		return;
	for (int dy = std::max(0, y); dy < std::min(dst.height, y + h); ++dy)
		for (int dx = std::max(0, x); dx < std::min(dst.width, x + w); ++dx) {
			const int sx = crop.x + int(int64_t(dx - x) * crop.w / w);
			const int sy = crop.y + int(int64_t(dy - y) * crop.h / h);
			// Partial edge slices are transparently padded, never stretched.
			if (sx >= 0 && sy >= 0 && sx < src.width && sy < src.height)
				over(dst.pixels[size_t(dy) * dst.width + dx],
				     src.pixels[size_t(sy) * src.width + sx]);
		}
}
void rectangle(Image &dst, int x, int y, int w, int h, uint32_t c) {
	for (int yy = std::max(0, y); yy < std::min(dst.height, y + h); ++yy)
		for (int xx = std::max(0, x); xx < std::min(dst.width, x + w); ++xx)
			over(dst.pixels[size_t(yy) * dst.width + xx], c);
}
const Image &Images::get(const std::string &file) {
	auto it = cache.find(file);
	if (it == cache.end()) {
		if (cache.size() >= 256)
			throw std::runtime_error("Too many simultaneously loaded images (256 maximum)");
		auto value = std::make_shared<Image>(readImage(assetPath(root, file)));
		size_t total = value->pixels.size();
		for (const auto &entry : cache)
			total += entry.second->pixels.size();
		if (total > 64 * 1024 * 1024)
			throw std::runtime_error("Decoded image cache exceeds 256 MiB");
		it = cache.emplace(file, std::move(value)).first;
	}
	return *it->second;
}
Image composeRoom(const Project &p, int index, Images &images, Layer last) {
	Image out{256, 224, std::vector<uint32_t>(256 * 224, rgba(12, 20, 28))};
	const auto &r = p.rooms.at(index);
	for (int layer = 0; layer <= int(last); ++layer) {
		const auto &a = r.art[layer];
		if (!a.file.empty())
			blit(out, images.get(a.file), a, 0, 0, 256, 224);
	}
	return out;
}
std::vector<Issue> imageIssues(const Project &p, Images &images) {
	std::vector<Issue> out;
	auto check = [&](const Art &a, int frames, Issue issue) {
		if (a.file.empty())
			return;
		try {
			const auto &image = images.get(a.file);
			if (frames && (int64_t(a.x) + int64_t(a.w) * frames > image.width ||
			               int64_t(a.y) + a.h > image.height))
				throw std::runtime_error("Animation extends outside its sprite sheet");
		} catch (const std::exception &e) {
			issue.message = e.what();
			out.push_back(issue);
		}
	};
	for (const auto &r : p.rooms)
		for (int layer = 0; layer < 3; ++layer)
			check(r.art[layer], 0, {true, "", r.id, "", "", -1, layer});
	for (const auto &m : p.modules) {
		check(m.sprite, m.frames, {true, "", m.a.room, m.id});
		for (const auto &a : m.assets)
			check(a, 1, {true, "", m.a.room, m.id});
		for (const auto &n : m.nodes)
			for (const auto &a : n.visuals)
				check(a.art, a.frames, {true, "", n.at.room, m.id, n.id});
	}
	const auto sounds = audioIssues(p, images.root);
	out.insert(out.end(), sounds.begin(), sounds.end());
	return out;
}
void validateImages(const Project &p, Images &images) {
	const auto issues = imageIssues(p, images);
	if (!issues.empty())
		throw std::runtime_error(issues.front().message);
}
void drawModule(Image &out, const Module &m, int part, bool open, uint64_t tick, Images &images) {
	if (m.kind == Kind::Graph)
		return;
	const auto &a = part ? m.b : m.a;
	if (m.kind == Kind::Prop && !m.sprite.file.empty()) {
		Art crop = m.sprite;
		crop.x += int((tick * unsigned(m.fps) / 30) % unsigned(m.frames)) * crop.w;
		blit(out, images.get(crop.file), crop, a.x, a.y - m.height, m.width, m.height);
	} else if (m.kind == Kind::Door) {
		if (part) {
			rectangle(out, a.x - 6, a.y - 24, 12, 24, rgba(26, 44, 54));
			rectangle(out, a.x - 4, a.y - 22, 8, 8, open ? rgba(62, 236, 180) : rgba(240, 165, 56));
		} else {
			rectangle(out, a.x - 2, a.y - m.height, 2, m.height, rgba(98, 151, 167));
			rectangle(out, a.x + m.width, a.y - m.height, 2, m.height, rgba(98, 151, 167));
			if (!open) {
				rectangle(out, a.x, a.y - m.height, m.width, m.height, rgba(50, 89, 105));
				for (int y = a.y - m.height + 4; y < a.y; y += 8)
					rectangle(out, a.x + 2, y, m.width - 4, 2, rgba(108, 177, 184));
			}
		}
	} else if (m.kind == Kind::Medkit) {
		rectangle(out, a.x - 6, a.y - 12, 12, 12, rgba(31, 100, 86));
		rectangle(out, a.x - 1, a.y - 10, 3, 8, rgba(155, 255, 216));
		rectangle(out, a.x - 4, a.y - 7, 9, 3, rgba(155, 255, 216));
	} else if (m.kind == Kind::Crawler) {
		rectangle(out, a.x - 8, a.y - 12, 20, 10, rgba(186, 110, 206));
		rectangle(out, a.x + 6, a.y - 10, 3, 3, rgba(250, 220, 126));
	} else {
		const int pulse = 100 + int(tick % 30) * 4;
		rectangle(out, a.x, a.y - m.height, m.width, m.height, rgba(32, pulse, 155));
		rectangle(out, a.x + 2, a.y - m.height + 2, std::max(1, m.width - 4), 3,
		          rgba(170, 245, 224));
	}
}
// Animate the already-composited background, before any physical scenery or
// actor is drawn. Each sample comes from an immutable regional copy: drawing
// order within the region cannot smear pixels or accumulate deformation.
static void backgroundMotion(Image &out, const Node &n, uint64_t age) {
	if (out.width < 1 || out.height < 1 || out.pixels.size() != size_t(out.width) * out.height)
		return;
	const int left = std::clamp(n.at.x, 0, out.width);
	const int top = int(std::clamp<int64_t>(int64_t(n.at.y) - n.height, 0, out.height));
	const int width = std::clamp(n.width, 0, std::min(256, out.width - left));
	const int height = std::clamp(n.height, 0, std::min(224, out.height - top));
	if (width < 8 || height < 8)
		return;
	std::vector<uint32_t> source(size_t(width) * height);
	for (int y = 0; y < height; ++y)
		std::copy_n(out.pixels.begin() + size_t(top + y) * out.width + left, width,
		            source.begin() + size_t(y) * width);
	constexpr double tau = 6.283185307179586;
	const int period = std::clamp(n.duration, 60, 600);
	const double phase = double(age % period) * tau / period;
	const double strength = std::clamp(n.ambientAmount, 1, 3);
	const double breath = std::sin(phase);
	// Particles are reconstructed from simulation age, never from a global RNG
	// or render count. Pause, save/load and repeated renders therefore agree.
	if (n.ambientStyle == 5 || n.ambientStyle == 6) {
		const bool mist = n.ambientStyle == 6;
		const int count = 2 + int(strength) * 2;
		uint32_t seed = uint32_t(left * 65537 + top * 257 + width * 17 + height);
		auto random = [&] {
			seed = seed * 1664525u + 1013904223u;
			return double(seed >> 8) / 16777216.0;
		};
		for (int i = 0; i < count; ++i) {
			const double offset = random(), lane = random();
			const double t = std::fmod(double(age % period) / period + offset, 1.0);
			const double fade = std::sin(t * tau * .5);
			const int x = int(width * (.2 + .6 * lane + .06 * std::sin(t * tau + offset * tau)));
			const int y = int(height * (.85 - .7 * t));
			for (int tail = 0; tail < (mist ? 3 : 1); ++tail) {
				const int px = x + tail, py = y + (tail == 2 ? 1 : 0);
				if (px < 0 || py < 0 || px >= width || py >= height)
					continue;
				const double u = 2.0 * (px + .5) / width - 1.0;
				const double v = 2.0 * (py + .5) / height - 1.0;
				if (u * u + v * v >= 1.0)
					continue;
				auto &pixel = out.pixels[size_t(top + py) * out.width + left + px];
				const int gain = int((mist ? 46 : 65) * fade * fade / (1.0 + tail));
				uint32_t color = pixel & 0xff000000u;
				for (int shift : {0, 8, 16})
					color |= uint32_t(std::min(255, int((pixel >> shift) & 255) + gain)) << shift;
				pixel = color;
			}
		}
		return;
	}
	const double rotorCos = std::cos(phase), rotorSin = std::sin(phase);
	for (int y = 0; y < height; ++y) {
		const double v = 2.0 * (y + .5) / height - 1.0;
		for (int x = 0; x < width; ++x) {
			const double u = 2.0 * (x + .5) / width - 1.0;
			const double radius = u * u + v * v;
			if (radius >= 1.0)
				continue;
			// Zero displacement and brightness at the edge prevents rectangular
			// seams. Integer sampling keeps the original pixel edges crisp.
			const double mask = (1.0 - radius) * (1.0 - radius);
			int dx = 0, dy = 0;
			double glow = 0;
			if (n.ambientStyle == 0) {
				dx = int(std::lround(-u * strength * 2.0 * mask * breath));
				dy = int(std::lround(-v * strength * 2.0 * mask * breath));
				glow = .025 * strength * mask * breath;
			} else if (n.ambientStyle == 1) {
				glow = .08 * strength * mask * std::sin(phase - radius * tau);
			} else if (n.ambientStyle == 2) {
				const double wave = std::sin(phase + v * tau);
				dx = int(std::lround(strength * mask * wave));
				dy = int(std::lround(.5 * strength * mask * std::sin(phase + u * tau)));
				glow = .05 * strength * mask * wave;
			} else if (n.ambientStyle == 3) {
				// Fit the selection inside the fan casing. Keep the outer rim and
				// central bearing still; rotate only its blade annulus.
				if (radius < .08 || radius >= .90)
					continue;
				const int sx = int(std::lround((u * rotorCos + v * rotorSin + 1) * width * .5 - .5));
				const int sy = int(std::lround((v * rotorCos - u * rotorSin + 1) * height * .5 - .5));
				dx = sx - x;
				dy = sy - y;
			}
			const uint32_t pixel = source[size_t(std::clamp(y + dy, 0, height - 1)) * width +
			                              std::clamp(x + dx, 0, width - 1)];
			uint32_t color = pixel & 0xff000000u;
			for (int shift : {0, 8, 16}) {
				const int value = (pixel >> shift) & 255;
				// A bounded additive pool illuminates even a black recess. The
				// four intensity steps retain the room's restrained pixel look.
				const double light = n.ambientStyle == 4 ?
				    std::floor(mask * (4.0 + .6 * breath)) / 4.0 * strength *
				        (shift == 0 ? 9 : shift == 8 ? 17 : 19) : 0;
				color |= uint32_t(std::clamp(int(std::lround(value * (1.0 + glow) + light)), 0, 255))
				         << shift;
			}
			out.pixels[size_t(top + y) * out.width + left + x] = color;
		}
	}
}
// The authored sheet is four columns by two rows, with left-facing poses.
// Scale a single cell using the same nearest sampling as room artwork; mirror
// the complete cell around the collision actor's center without changing it.
void drawGuard(Image &out, const Node &n, int frame, bool left, int x, int feet, Images &images) {
	auto art = n.visuals[0].art;
	art.w /= 4; art.h /= 2;
	art.x += (frame % 4) * art.w;
	art.y += (frame / 4) * art.h;
	Image sprite{n.width, n.height, std::vector<uint32_t>(n.width * n.height)};
	blit(sprite, images.get(art.file), art, 0, 0, n.width, n.height);
	int bottom = n.height - 1;
	while (bottom > 0 && std::none_of(sprite.pixels.begin() + bottom * n.width,
	                                sprite.pixels.begin() + (bottom + 1) * n.width,
	                                [](uint32_t c) { return (c >> 24) >= 128; })) --bottom;
	const int origin = x + 8 - n.width / 2;
	for (int y = 0; y < n.height; ++y)
		for (int xx = 0; xx < n.width; ++xx) {
			const int tx = origin + xx, ty = feet - bottom - 1 + y;
			if (tx >= 0 && tx < out.width && ty >= 0 && ty < out.height)
				over(out.pixels[ty * out.width + tx],
				     sprite.pixels[y * n.width + (left ? xx : n.width - xx - 1)]);
		}
}
void drawNode(Image &out, const Node &n, bool active, uint64_t age, Images &images, bool editing) {
	if (n.kind == NodeKind::Actor && n.actorStyle == 1 && editing) {
		drawGuard(out, n, 0, true, n.at.x, n.at.y, images);
		return;
	}
	if (n.kind == NodeKind::Ambient) {
		if (active && !editing)
			backgroundMotion(out, n, age);
		return; // Editor selection outlines the region; PLAY previews its motion.
	}
	if (!editing && n.kind == NodeKind::Animation && !active && n.visuals[0].art.file.empty())
		return;
	if (!editing &&
	    ((n.kind >= NodeKind::Damage && n.kind != NodeKind::Lift && n.kind != NodeKind::Target) || n.kind == NodeKind::Actor ||
	     n.kind == NodeKind::Relay || n.kind == NodeKind::Zone || n.kind == NodeKind::Dialogue ||
	     (n.kind == NodeKind::Timer || n.kind == NodeKind::State) ||
	     (n.kind == NodeKind::Item && active)))
		return;
	const auto &animation = n.visuals[active && !n.visuals[1].art.file.empty() ? 1 : 0];
	if (!animation.art.file.empty()) {
		// Editor inspectors may temporarily contain unfinished values; rendering
		// must stay safe while strict validation prevents playing/saving them.
		Art crop = animation.art;
		const uint64_t frame = age * unsigned(std::clamp(animation.fps, 1, 30)) / 30;
		const unsigned count = unsigned(std::clamp(animation.frames, 1, 128));
		crop.x += int(animation.loop ? frame % count : std::min(frame, uint64_t(count - 1))) *
		          std::clamp(crop.w, 1, 16384);
		blit(out, images.get(crop.file), crop, n.at.x,
		     n.at.y - (n.kind == NodeKind::Lift ? 0 : n.height), n.width, n.height);
		return;
	}
	if (n.kind == NodeKind::Target) {
		const int top = n.at.y - n.height;
		rectangle(out, n.at.x, top, n.width, n.height, rgba(84, 105, 111));
		rectangle(out, n.at.x + 2, top + 2, std::max(1, n.width - 4), std::max(1, n.height - 4), rgba(193, 208, 198));
		rectangle(out, n.at.x + n.width / 2 - 2, top + n.height / 2 - 2, 4, 4, rgba(208, 117, 47));
		return;
	}
	if (n.kind == NodeKind::Lift) {
		rectangle(out, n.at.x, n.at.y, n.width, n.height, rgba(54, 87, 99));
		rectangle(out, n.at.x, n.at.y, n.width, std::min(2, n.height), rgba(111, 224, 190));
		return;
	}
	if (n.kind == NodeKind::Door) {
		Module door;
		door.kind = Kind::Door;
		door.a = n.at;
		door.width = n.width;
		door.height = n.height;
		drawModule(out, door, 0, active, age, images);
		return;
	}
	const uint32_t color = n.kind == NodeKind::Npc    ? rgba(191, 133, 224)
	                       : n.kind == NodeKind::Item ? rgba(243, 198, 86)
	                       : n.kind == NodeKind::Zone ? rgba(75, 170, 228, 100)
	                       : active                   ? rgba(67, 216, 171)
	                                                  : rgba(81, 139, 160);
	rectangle(out, n.at.x, n.at.y - n.height, n.width, n.height, color);
	if (n.kind == NodeKind::Npc) {
		rectangle(out, n.at.x + 3, n.at.y - n.height + 2, std::max(1, n.width - 6), 9,
		          rgba(229, 204, 169));
	} else
		rectangle(out, n.at.x + 2, n.at.y - n.height + 3, std::max(1, n.width - 4), 4,
		          rgba(213, 249, 235));
}
} // namespace editor
