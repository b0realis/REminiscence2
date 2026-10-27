// SPDX-License-Identifier: GPL-3.0-or-later
#include "image.h"
#include "audio.h"
#include "atomic_file.h"
#include "file.h"
#include <zlib.h>
#include <algorithm>
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
void drawNode(Image &out, const Node &n, bool active, uint64_t age, Images &images, bool editing) {
	if (!editing && n.kind == NodeKind::Animation && !active && n.visuals[0].art.file.empty())
		return;
	if (!editing &&
	    (n.kind >= NodeKind::Damage || n.kind == NodeKind::Actor || n.kind == NodeKind::Relay ||
	     n.kind == NodeKind::Zone || n.kind == NodeKind::Dialogue ||
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
		blit(out, images.get(crop.file), crop, n.at.x, n.at.y - n.height, n.width, n.height);
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
