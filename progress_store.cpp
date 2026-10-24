// SPDX-License-Identifier: GPL-3.0-or-later
#include "progress_store.h"
#include "atomic_file.h"
#include "byte_reader.h"
#include "file.h"
#include "save_catalog.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <limits>
#include <zlib.h>
namespace progress {
namespace {
constexpr size_t HeaderSize = 52, StateLimit = 65536;
constexpr uint32_t Magic = 0x52324153; // R2AS
bool writeSnapshot(const std::string &, const Snapshot &, std::string &);
uint64_t u64(ByteReader &r) {
	const uint64_t hi = r.be32();
	return (hi << 32) | r.be32();
}
bool validState(const Snapshot &s, std::string &reason) {
	reason = "Autosave state or summary is damaged or unsupported.";
	if (s.edition < 0 || s.edition > 4 || s.level < 0 || s.level > 6 || s.room < 0 || s.room > 63 ||
	    s.playTicks > UINT64_MAX - 1000000 || s.skill < 0 || s.skill > 2 || s.health <= 0 ||
	    s.health > 32767 || !s.entities || s.entities > 256 ||
	    s.state.size() < 38 + 13 + 30 + 0x1C00 + 22 || s.state.size() > StateLimit)
		return false;
	File f;
	f.openMemoryBuffer(s.state.size());
	f.write(s.state.data(), s.state.size());
	int version;
	if (!saves::validate(f, s.entities, version, reason, s.edition, s.level, s.fingerprint))
		return false;
	if (version != 4) {
		reason = "Autosaves require state format 4.";
		return false;
	}
	// Bind the summary to the serialized state, rather than trusting UI metadata.
	const size_t hero = 38 + 13;
	reason = "Autosave summary does not match its serialized state.";
	if (s.state[38] != s.skill || s.state[hero + 7] != s.room ||
	    int(s.state[hero + 8] * 256 + s.state[hero + 9]) != s.health)
		return false;
	const size_t collisions = (uint32_t(s.state[43]) << 24) | (uint32_t(s.state[44]) << 16) |
	                          (uint32_t(s.state[45]) << 8) | s.state[46];
	const size_t count = collisions == UINT32_MAX ? 0 : collisions;
	return s.state.size() == 38 + 13 + s.entities * 30 + 0x1C00 + count * 25 + 22;
}
} // namespace
std::string filename(int edition, unsigned slot) {
	return "reminiscence2-" + std::to_string(edition) + "-auto-" + std::to_string(slot) + ".r2s";
}
std::string date(uint64_t milliseconds) {
	const auto seconds = static_cast<time_t>(milliseconds / 1000);
	const auto *time = std::gmtime(&seconds);
	char text[40] = {};
	if (time)
		std::strftime(text, sizeof(text), "%Y-%m-%d %H:%M UTC", time);
	return text;
}
bool read(const std::string &path, Snapshot &snapshot, std::string &reason) {
	reason = "Autosave is missing, damaged or unsupported.";
	std::error_code ec;
	if (!std::filesystem::is_regular_file(
	        std::filesystem::symlink_status(std::filesystem::u8path(path), ec)))
		return false;
	File f;
	if (!f.open(path.c_str(), "rb", "."))
		return false;
	const unsigned size = f.size();
	if (size < HeaderSize + 4 ||
	    size > HeaderSize + 97 + StateLimit + PreviewWidth * PreviewHeight * 4 + 4)
		return false;
	std::vector<uint8_t> bytes(size);
	if (f.read(bytes.data(), size) != size || !f.close())
		return false;
	ByteReader r(bytes.data(), size);
	if (r.be32() != Magic)
		return false;
	const unsigned schema = r.be16(), flags = r.be16();
	if ((schema != 1 && schema != 2) || (schema == 1 ? flags != 0 : flags > 1))
		return false;
	Snapshot parsed;
	parsed.manual = schema == 2;
	parsed.protectedSave = flags & 1;
	parsed.sequence = u64(r);
	parsed.timestamp = u64(r);
	parsed.playTicks = u64(r);
	parsed.edition = r.u8();
	parsed.level = r.u8();
	parsed.room = r.u8();
	parsed.skill = r.u8();
	parsed.health = r.be16();
	parsed.entities = r.be16();
	parsed.fingerprint = r.be32();
	const unsigned stateSize = r.be32(), width = r.be16(), height = r.be16();
	if (parsed.manual) {
		const unsigned length = r.u8();
		if (length > 96 || length > r.remaining())
			return false;
		parsed.label.resize(length);
		r.read(reinterpret_cast<uint8_t *>(parsed.label.data()), length);
		if (!validLabel(parsed.label))
			return false;
	}
	if (!parsed.sequence || !stateSize || stateSize > StateLimit ||
	    !((width == PreviewWidth && height == PreviewHeight) || (!width && !height)) ||
	    size_t(stateSize) + width * height * 4 + 4 != r.remaining())
		return false;
	parsed.state.resize(stateSize);
	r.read(parsed.state.data(), stateSize);
	if (width) {
		auto preview = std::make_shared<Preview>();
		preview->width = width;
		preview->height = height;
		preview->rgba.resize(width * height * 4);
		r.read(preview->rgba.data(), preview->rgba.size());
		parsed.preview = preview;
	}
	const auto checksum = r.be32();
	if (!r.good() || checksum != crc32(0, bytes.data(), size - 4) || !validState(parsed, reason))
		return false;
	parsed.filename = path;
	snapshot = std::move(parsed);
	reason.clear();
	return true;
}
std::vector<Snapshot> scan(const std::string &directory, int edition) {
	std::vector<Snapshot> result;
	if (edition < 0 || edition > 4)
		return result;
	for (unsigned i = 0; i < RotationCount; ++i) {
		Snapshot s;
		std::string reason;
		const auto path = (std::filesystem::u8path(directory) / filename(edition, i)).u8string();
		if (read(path, s, reason) && !s.manual && s.edition == edition)
			result.push_back(std::move(s));
	}
	std::sort(result.begin(), result.end(), [](const Snapshot &a, const Snapshot &b) {
		return a.sequence != b.sequence ? a.sequence > b.sequence : a.filename < b.filename;
	});
	return result;
}
bool save(const std::string &directory, Snapshot s, std::string &reason) {
	s.manual = s.protectedSave = false;
	s.label.clear();
	if (!validState(s, reason)) {
		reason = "Current progress cannot be serialized as an autosave.";
		return false;
	}
	if (s.preview && (s.preview->width != PreviewWidth || s.preview->height != PreviewHeight ||
	                  s.preview->rgba.size() != PreviewWidth * PreviewHeight * 4)) {
		reason = "Invalid autosave preview.";
		return false;
	}
	const auto existing = scan(directory, s.edition);
	const auto latest = existing.empty() ? 0 : existing.front().sequence;
	if (latest == UINT64_MAX) {
		reason = "Autosave generation counter is invalid.";
		return false;
	}
	s.sequence = latest + 1;
	s.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
	                  std::chrono::system_clock::now().time_since_epoch())
	                  .count();
	std::string target;
	for (unsigned i = 0; i < RotationCount; ++i) {
		const auto path = (std::filesystem::u8path(directory) / filename(s.edition, i)).u8string();
		if (std::none_of(existing.begin(), existing.end(),
		                 [&](const Snapshot &v) { return v.filename == path; })) {
			target = path;
			break;
		}
	}
	if (target.empty())
		target = existing.back().filename;
	return writeSnapshot(target, s, reason);
}
bool validLabel(const std::string &label) {
	// Printable ASCII keeps names portable across every frontend/font and prevents
	// control characters in notifications. Names are data, never filesystem paths.
	return label.size() <= 96 && std::all_of(label.begin(), label.end(),
	                                         [](unsigned char c) { return c >= 32 && c <= 126; });
}
std::vector<Snapshot> recent(const std::string &directory, int edition) {
	auto result = scan(directory, edition);
	for (int level = 0; level < 7; ++level) {
		for (int slot = 0; slot < 100; ++slot) {
			Snapshot saved;
			std::string reason;
			const auto path =
			    (std::filesystem::u8path(directory) / manualFilename(edition, level, slot))
			        .u8string();
			if (read(path, saved, reason) && saved.manual && saved.edition == edition &&
			    saved.level == level)
				result.push_back(std::move(saved));
		}
	}
	std::stable_sort(result.begin(), result.end(), [](const Snapshot &a, const Snapshot &b) {
		if (a.timestamp != b.timestamp)
			return a.timestamp > b.timestamp;
		if (a.manual != b.manual)
			return a.manual;
		return a.sequence > b.sequence;
	});
	return result;
}

std::string manualFilename(int edition, int level, int slot) {
	return "reminiscence2-" + std::to_string(edition) + "-level" + std::to_string(level + 1) +
	       "-manual-" + std::to_string(slot) + ".r2m";
}
namespace {
bool writeSnapshot(const std::string &target, const Snapshot &s, std::string &reason) {
	std::vector<uint8_t> bytes;
	auto put = [&](uint64_t value, unsigned count) {
		for (unsigned i = count; i; --i)
			bytes.push_back(uint8_t(value >> ((i - 1) * 8)));
	};
	put(Magic, 4);
	put(s.manual ? 2 : 1, 2);
	put(s.manual && s.protectedSave ? 1 : 0, 2);
	put(s.sequence, 8);
	put(s.timestamp, 8);
	put(s.playTicks, 8);
	put(s.edition, 1);
	put(s.level, 1);
	put(s.room, 1);
	put(s.skill, 1);
	put(s.health, 2);
	put(s.entities, 2);
	put(s.fingerprint, 4);
	put(s.state.size(), 4);
	put(s.preview ? s.preview->width : 0, 2);
	put(s.preview ? s.preview->height : 0, 2);
	if (s.manual) {
		put(s.label.size(), 1);
		bytes.insert(bytes.end(), s.label.begin(), s.label.end());
	}
	bytes.insert(bytes.end(), s.state.begin(), s.state.end());
	if (s.preview)
		bytes.insert(bytes.end(), s.preview->rgba.begin(), s.preview->rgba.end());
	put(crc32(0, bytes.data(), bytes.size()), 4);
	return writeAtomicFile(
	    target, false, [&](File &file) { file.write(bytes.data(), bytes.size()); }, reason);
}
} // namespace
bool saveManual(const std::string &directory, int slot, Snapshot s, std::string &reason) {
	if (slot < 0 || slot > 99 || !validLabel(s.label) || !validState(s, reason) ||
	    (s.preview && (s.preview->width != PreviewWidth || s.preview->height != PreviewHeight ||
	                   s.preview->rgba.size() != PreviewWidth * PreviewHeight * 4))) {
		reason = "Invalid manual slot, name, preview or game state.";
		return false;
	}
	const auto target =
	    (std::filesystem::u8path(directory) / manualFilename(s.edition, s.level, slot)).u8string();
	Snapshot old;
	if (read(target, old, reason) && old.protectedSave) {
		reason = "Protected milestone. Unlock it in Save / Load before overwriting.";
		return false;
	}
	s.manual = true;
	if (slot == 0) {
		s.protectedSave = false;
		s.label = "Checkpoint";
	}
	s.sequence = 1;
	s.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
	                  std::chrono::system_clock::now().time_since_epoch())
	                  .count();
	return writeSnapshot(target, s, reason);
}
bool updateManual(const std::string &directory, int edition, int level, int slot,
                  const std::string &label, bool protect, std::string &reason) {
	if (edition < 0 || edition > 4 || level < 0 || level > 6 || slot < 1 || slot > 99 ||
	    !validLabel(label)) {
		reason = "Invalid save name or slot.";
		return false;
	}
	const auto target =
	    (std::filesystem::u8path(directory) / manualFilename(edition, level, slot)).u8string();
	Snapshot s;
	if (!read(target, s, reason) || !s.manual || s.edition != edition || s.level != level) {
		reason = "Only a valid new-format manual save can be named or protected.";
		return false;
	}
	s.label = label;
	s.protectedSave = protect;
	return writeSnapshot(target, s, reason);
}
} // namespace progress
