// SPDX-License-Identifier: GPL-3.0-or-later
#include "save_catalog.h"
#include "file.h"
#include "resource.h"
#include <array>
#include <charconv>
#include <cstdio>
#include <ctime>
#include <filesystem>

namespace saves {
namespace {
uint32_t be32(const uint8_t *p) {
	return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
}
bool header(File &file, int &version, int &room) {
	std::array<uint8_t, 38> bytes{};
	if (file.read(bytes.data(), bytes.size()) != bytes.size() || be32(bytes.data()) != 0x46425356)
		return false;
	version = bytes[4] * 256 + bytes[5];
	char description[33] = {};
	memcpy(description, bytes.data() + 6, 32);
	room = -1;
	if (const char *value = strstr(description, " room=")) {
		const auto parsed = std::from_chars(value + 6, description + 32, room);
		if (parsed.ec != std::errc{} || room < 0 || room >= 64)
			room = -1;
	}
	return version == 2 || version == 3 || version == 4;
}
bool link(uint32_t value, unsigned limit) {
	return value == UINT32_MAX || value < limit;
}
} // namespace
std::string name(int edition, int level, int slot) {
	char suffix[40];
	snprintf(suffix, sizeof(suffix), "level%d-%02d.state", level + 1, slot);
	// ResourceType is part of the persisted edition identity, independent of OS.
	return "fb-" + (edition >= 0 ? std::to_string(edition) + "-" : std::string()) + suffix;
}
std::vector<settings::SaveSlot> scan(const char *directory, int level, int edition) {
	std::vector<settings::SaveSlot> result;
	if (level < 0 || level >= 7)
		return result;
	for (int slot = 0; slot <= 99; ++slot) {
		auto filename = name(edition, level, slot);
		settings::SaveSlot info;
		info.slot = slot;
		info.level = level;
		const auto modernPath =
		    (std::filesystem::u8path(directory) / progress::manualFilename(edition, level, slot))
		        .u8string();
		std::error_code modernError;
		if (edition >= 0 &&
		    std::filesystem::exists(std::filesystem::u8path(modernPath), modernError)) {
			progress::Snapshot snapshot;
			std::string reason;
			info.exists = info.modern = true;
			info.supported = progress::read(modernPath, snapshot, reason) && snapshot.manual &&
			                 snapshot.edition == edition && snapshot.level == level;
			if (info.supported) {
				info.room = snapshot.room;
				info.label = snapshot.label;
				info.protectedSave = snapshot.protectedSave;
				info.modified = progress::date(snapshot.timestamp);
				info.preview = snapshot.preview;
			}
			result.push_back(std::move(info));
			continue;
		}
		File file;
		info.exists = file.open(filename.c_str(), "zrb", directory);
		if (!info.exists && edition >= 0) {
			filename = name(-1, level, slot);
			info.exists = file.open(filename.c_str(), "zrb", directory);
		}
		if (info.exists) {
			int version;
			info.supported = header(file, version, info.room);
			std::error_code error;
			const auto date = std::filesystem::last_write_time(
			    std::filesystem::u8path(directory) / filename, error);
			if (!error) {
				const auto systemTime =
				    std::chrono::system_clock::now() + (date - decltype(date)::clock::now());
				const auto stamp = std::chrono::system_clock::to_time_t(
				    std::chrono::time_point_cast<std::chrono::system_clock::duration>(systemTime));
				if (const auto *local = std::localtime(&stamp)) {
					char text[32];
					if (std::strftime(text, sizeof(text), "%Y-%m-%d %H:%M", local))
						info.modified = text;
				}
			}
		}
		result.push_back(info);
	}
	return result;
}
std::vector<settings::SaveSlot> scanAll(const char *directory, int edition) {
	std::vector<settings::SaveSlot> result;
	for (int level = 0; level < 7; ++level)
		for (auto &entry : scan(directory, level, edition))
			if (entry.exists)
				result.push_back(std::move(entry));
	return result;
}
bool validate(File &file, unsigned entities, int &version, std::string &reason, int edition,
              int level, uint32_t fingerprint, const Resource *resource) {
	int room;
	file.seek(0);
	auto bad = [&] {
		reason = "Save is truncated, incompatible or contains invalid state links. Current "
		         "progress was kept.";
		return false;
	};
	if (!entities || entities > 256 || !header(file, version, room))
		return bad();
	std::array<uint8_t, 13> prefix{};
	if (file.read(prefix.data(), prefix.size()) != prefix.size() || prefix[0] > 2)
		return bad();
	const uint32_t current = be32(prefix.data() + 5), next = be32(prefix.data() + 9);
	const unsigned collisions = current == UINT32_MAX ? 0 : current;
	if (collisions > 256 || !link(next, collisions))
		return bad();
	std::vector<uint8_t> inventoryNext(entities);
	for (unsigned i = 0; i < entities; ++i) {
		std::array<uint8_t, 30> pge{};
		if (file.read(pge.data(), pge.size()) != pge.size() ||
		    !link(be32(pge.data() + 22), entities) || be32(pge.data() + 26) != i || pge[19] != i ||
		    (i == 0 && pge[7] >= 64) ||
		    (resource && !resource->validStateActor(pge.data(), prefix[0])))
			return bad();
		inventoryNext[i] = pge[13];
		for (int offset : {13, 14, 15})
			if (pge[offset] != 255 && pge[offset] >= entities)
				return bad();
	}
	// Each inventory list must terminate. Range checks alone permit a valid
	// checksum to conceal a cycle that hangs the inventory/opcode walkers.
	for (unsigned start = 0; start < entities; ++start) {
		unsigned n = start, steps = 0;
		while (n != 255 && steps++ < entities)
			n = inventoryNext[n];
		if (n != 255)
			return bad();
	}
	std::array<uint8_t, 0x1C00> grid{};
	if (file.read(grid.data(), grid.size()) != grid.size())
		return bad();
	for (unsigned i = 0; i < collisions; ++i) {
		std::array<uint8_t, 25> entry{};
		if (file.read(entry.data(), entry.size()) != entry.size() || !link(be32(entry.data()), i) ||
		    !link(be32(entry.data() + 4), grid.size()) || entry[8] > 16)
			return bad();
		const uint32_t offset = be32(entry.data() + 4);
		if (offset != UINT32_MAX && offset + entry[8] > grid.size())
			return bad();
	}
	if (version >= 3) {
		uint8_t gun[2];
		if (file.read(gun, sizeof(gun)) != sizeof(gun))
			return bad();
	}
	if (version >= 4) {
		std::array<uint8_t, 20> metadata{};
		if (file.read(metadata.data(), metadata.size()) != metadata.size() ||
		    metadata[4] > kResourceTypeSega || metadata[5] >= 7 ||
		    unsigned(metadata[6] * 256 + metadata[7]) != entities ||
		    (edition >= 0 && metadata[4] != edition) || (level >= 0 && metadata[5] != level) ||
		    (edition >= 0 && be32(metadata.data() + 8) != fingerprint) ||
		    (be32(metadata.data() + 12) & ~journal::ValidMask) ||
		    (be32(metadata.data() + 16) & ~be32(metadata.data() + 12))) {
			reason = "Save belongs to different game assets, or its metadata is damaged. Current "
			         "progress was kept.";
			return false;
		}
	}
	if (file.ioErr() || !file.atEnd())
		return bad();
	file.seek(38);
	reason.clear();
	return true;
}
} // namespace saves
