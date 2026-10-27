// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "runtime.h"
#include <map>
namespace editor {
// Atomically create all seven native levels; refuses an existing destination.
void exportNativeCampaign(const assets::Source &, const std::filesystem::path &folder);
struct OriginalActor {
	int id, room, x, y, type, behavior, objectKind;
};
struct OriginalRoom {
	bool hasMap = false;
	Image scenery, foreground, actors;
};
// Read-only native campaign inspection. Slot IDs are retained, including empty
// rooms and actors in holding lists. Native exports retain lossless rule tables
// and named ownership groups; scenery exports omit those tables.
class OriginalLevel {
  public:
	OriginalLevel(const assets::Source &, SystemStub *, int level = 0);
	~OriginalLevel();
	const OriginalRoom &room(int slot);
	const std::array<int8_t, 7424> &collision() const { return _collision; }
	const std::vector<OriginalActor> &actors() const { return _actors; }
	int startRoom() const { return _startRoom; }
	bool hasMap(int slot) const;
	bool supportsDirectStart() const;
	Project createProject(bool native) const;
	Image actorOverlay(const Project &, int room);
	std::filesystem::path exportNative(const std::filesystem::path &folder);
	// Creates a new folder only. Copies decoded art and geometry, NOT campaign
	// logic. Original files and any existing project are never overwritten.
	std::filesystem::path exportScenery(const std::filesystem::path &folder);

  private:
	SystemStub *_stub;
	std::unique_ptr<FileSystem> _files;
	std::unique_ptr<Game, GameDeleter> _game;
	std::array<int8_t, 7424> _collision;
	std::vector<OriginalActor> _actors;
	std::map<int, OriginalRoom> _rooms;
	int _startRoom = 0, _level = 0;
	int _edition = 0;
	std::string _fingerprint;
	std::vector<NativeActor> _initialActors;
	NativeCampaign _definition;
	uint64_t _actorPreviewRevision = 0;
	int _actorPreviewRoom = -1;
	Image _actorPreview;
};
} // namespace editor
