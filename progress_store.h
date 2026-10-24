// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_PROGRESS_STORE_H
#define REMINISCENCE2_PROGRESS_STORE_H
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace progress {
constexpr unsigned RotationCount = 3;
constexpr unsigned PreviewWidth = 128, PreviewHeight = 112;
struct Preview {
	unsigned width = 0, height = 0;
	std::vector<uint8_t> rgba;
};
struct Snapshot {
	uint64_t sequence = 0, timestamp = 0, playTicks = 0;
	int edition = 0, level = 0, room = 0, skill = 1, health = 0;
	unsigned entities = 0;
	uint32_t fingerprint = 0;
	std::vector<uint8_t> state;
	std::shared_ptr<const Preview> preview;
	std::string filename, label;
	bool manual = false, protectedSave = false;
};
std::string filename(int edition, unsigned slot);
bool read(const std::string &path, Snapshot &snapshot, std::string &reason);
// Newest first. Invalid files are ignored; loading revalidates the immutable
// in-memory snapshot against the player's actual game data as a second step.
std::vector<Snapshot> scan(const std::string &directory, int edition);
// Includes modern quick/manual/checkpoint bundles, ordered by save time.
std::vector<Snapshot> recent(const std::string &directory, int edition);
// Replace only one rotation slot, atomically. A failed write leaves every
// previous valid generation and all manual/checkpoint slots untouched.
bool save(const std::string &directory, Snapshot snapshot, std::string &reason);
// Manual bundles keep state, thumbnail and user metadata in one atomic write.
std::string manualFilename(int edition, int level, int slot);
bool saveManual(const std::string &directory, int slot, Snapshot snapshot, std::string &reason);
bool updateManual(const std::string &directory, int edition, int level, int slot,
                  const std::string &label, bool protect, std::string &reason);
bool validLabel(const std::string &label);
std::string date(uint64_t milliseconds);
} // namespace progress
#endif
