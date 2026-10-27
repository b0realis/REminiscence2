// SPDX-License-Identifier: GPL-3.0-or-later
#include "original.h"
#include "campaign.h"
#include "native_logic.h"
#include "preview_system.h"
#include "game.h"
#include "fs.h"
#include "systemstub.h"
#include "gameplay_identity.h"
#include <algorithm>
#include <fstream>
#include <stdexcept>
namespace editor {
OriginalLevel::OriginalLevel(const assets::Source &source, SystemStub *stub, int level)
    : _stub(stub), _level(level), _edition(source.edition) {
	if (level < 0 || level > 6)
		throw std::runtime_error("Native level must be 1..7");
	if (source.edition != kResourceTypeAmiga && source.edition != kResourceTypeDOS &&
	    source.edition != kResourceTypeSega)
		throw std::runtime_error("Original level reference supports Amiga, DOS/GOG and Sega CD");
	_files = std::make_unique<FileSystem>(source.root.c_str(), source.excludedRoots);
	_game.reset(new Game(stub, _files.get(), _files.get(), "", level, ResourceType(source.edition),
	                     LANG_EN, kWidescreenNone, false, 0));
	if (_game->_res._isDemo)
		throw std::runtime_error("Choose full game assets to open original level one");
	initializeOriginalLevel(*_game);
	char fingerprint[9];
	std::snprintf(fingerprint, sizeof(fingerprint), "%08x", gameplayFingerprint(_game->_res));
	_fingerprint = fingerprint;
	std::copy_n(_game->_res._ctData, _collision.size(), _collision.begin());
	_startRoom = _game->_res._pgeInit[0].init_room;
	for (int i = 0; i < _game->_res._pgeNum; ++i) {
		const auto &a = _game->_res._pgeInit[i];
		_actors.push_back(
		    {i, a.init_room, a.pos_x, a.pos_y, a.type, a.obj_node_number, a.object_type});
		_initialActors.push_back({i,
		                          a.init_room,
		                          a.pos_x,
		                          a.pos_y,
		                          a.life,
		                          !bool(a.mirror_x),
		                          a.type,
		                          a.obj_node_number,
		                          a.object_type,
		                          {a.data[0], a.data[1], a.data[2], a.data[3]}});
	}
	_definition = {true, _level, _edition, _fingerprint, _initialActors};
	captureNativeLogic(*_game, _definition);
}
OriginalLevel::~OriginalLevel() = default;
bool OriginalLevel::supportsDirectStart() const {
	const auto &res = _game->_res;
	const auto &player = res._pgeInit[0];
	if (player.obj_node_number >= res._numObjectNodes)
		return false;
	const auto *node = res._objectNodesMap[player.obj_node_number];
	if (!node)
		return false;
	for (int i = 0; i < node->num_objects; ++i)
		if (node->objects[i].type == 1)
			return true;
	return false;
}
bool OriginalLevel::hasMap(int slot) const {
	return slot >= 0 && slot < MaxRooms && _game->hasLevelMap(_level, slot);
}
const OriginalRoom &OriginalLevel::room(int slot) {
	if (slot < 0 || slot >= MaxRooms)
		throw std::runtime_error("Original room slot must be 0..63");
	if (_rooms.count(slot))
		return _rooms.at(slot);
	OriginalRoom out;
	out.hasMap = hasMap(slot);
	out.scenery = {RoomWidth, RoomHeight,
	               std::vector<uint32_t>(RoomWidth * RoomHeight, rgba(12, 20, 28))};
	out.foreground = {RoomWidth, RoomHeight, std::vector<uint32_t>(RoomWidth * RoomHeight, 0)};
	if (out.hasMap) {
		auto &g = *_game;
		g.resetGameState(); // Reset draw queues; never advance the native simulation.
		g._currentRoom = slot;
		std::fill_n(g._vid._frontLayer, RoomWidth * RoomHeight, 0);
		std::fill_n(g._vid._backLayer, RoomWidth * RoomHeight, 0);
		g.loadLevelMap();
		g._vid.setPalette0xF();
		g._vid.setTextPalette();
		uint8_t palette[768];
		_stub->getPalette(palette, 256);
		auto color = [&](uint8_t index) {
			return rgba(palette[index * 3], palette[index * 3 + 1], palette[index * 3 + 2]);
		};
		for (size_t i = 0; i < out.scenery.pixels.size(); ++i) {
			const uint8_t index = g._vid._backLayer[i];
			out.scenery.pixels[i] = color(index);
			// Native bit 7 protects scenery from ordinary actor draws. This is a
			// useful priority mask, not the original tiled layer authoring data.
			if (index & 0x80)
				out.foreground.pixels[i] = color(index);
		}
		g.prepareAnims();
		g.drawAnims();
		_stub->getPalette(palette, 256);
		out.actors = out.scenery;
		for (size_t i = 0; i < out.actors.pixels.size(); ++i)
			out.actors.pixels[i] = color(g._vid._frontLayer[i]);
	} else
		out.actors = out.scenery;
	return _rooms.emplace(slot, std::move(out)).first->second;
}
Project OriginalLevel::createProject(bool native) const {
	Project p = makeProject();
	p.name = "Level " + std::to_string(_level + 1) +
	         (native ? " native campaign" : " scenery study (no campaign scripts)");
	p.rooms.clear();
	auto id = [](int slot) { return "original-room-" + std::to_string(slot); };
	for (int i = 0; i < MaxRooms; ++i) {
		auto &r = addRoom(p, i % 8, i / 8);
		r.id = id(i);
		r.name = "Slot " + std::to_string(i) + (hasMap(i) ? "" : " (no map)");
		for (int c = 0; c < 112; ++c) {
			const int v = _collision[256 + i * 112 + c];
			if (v < 0 || v > 2)
				throw std::runtime_error(
				    "This edition has collision values the project format cannot preserve");
			r.collision[c] = uint8_t(v);
		}
		for (int d = 0; d < 4; ++d) {
			const int target = _collision[d * 64 + i];
			r.exits[d] = target >= 0 && target < MaxRooms ? id(target) : "";
		}
		if (!native && hasMap(i)) {
			r.art[0].file = "art/room-" + std::to_string(i) + ".png";
			r.art[2].file = "art/priority-" + std::to_string(i) + ".png";
		}
	}
	const auto &player = _game->_res._pgeInit[0];
	p.spawn = {id(_startRoom), player.pos_x, player.pos_y};
	p.spawnLeft = !player.mirror_x;
	if (native)
		p.native = _definition;
	compileCollision(p);
	return p;
}
std::filesystem::path OriginalLevel::exportNative(const std::filesystem::path &folder) {
	if (std::filesystem::exists(folder) || !std::filesystem::create_directories(folder))
		throw std::runtime_error("Choose a new folder for native campaign editing");
	const auto path = folder / ("level-" + std::to_string(_level + 1) + ".r2project.json");
	saveProject(createProject(true), path);
	return path;
}
// Preview uses the same resource identity lookup as native play; a private
// rule-table slot must not select a different monster sprite/palette bank.
class PreviewIdentity final : public AuthoringHooks {
	Game &_game;
	AuthoringHooks *_previous;
	const NativeCampaign &_native;

  public:
	PreviewIdentity(Game &g, const NativeCampaign &n)
	    : _game(g), _previous(g._authoring), _native(n) {
		g._authoring = this;
	}
	~PreviewIdentity() { _game._authoring = _previous; }
	void loaded(Game &) override {}
	void boundary(Game &) override {}
	void beforeDraw(Game &) override {}
	void afterDraw(Game &) override {}
	int behaviorIdentity(int b) const override { return nativeProgramSource(_native, b); }
};
Image OriginalLevel::actorOverlay(const Project &project, int slot) {
	if (project.native.fingerprint != _fingerprint)
		throw std::runtime_error("Native preview asset fingerprint mismatch");
	const auto &baseline = _definition;
	const uint64_t revision = nativeRevision(project.native);
	if (_actorPreviewRoom == slot && _actorPreviewRevision == revision)
		return _actorPreview;
	compileCollision(project);
	validateNativeLogic(project.native, baseline);
	auto &g = *_game;
	const auto count = g._res._pgeNum;
	const std::vector<InitPGE> pristine(g._res._pgeInit, g._res._pgeInit + count);
	installNativeLogic(g, project.native);
	installNativeActors(g, project.native);
	PreviewIdentity identity(g, project.native.programs.empty() ? baseline : project.native);
	g.resetGameState();
	g._currentRoom = slot;
	if (hasMap(slot))
		g.loadLevelMap();
	std::fill_n(g._vid._frontLayer, RoomWidth * RoomHeight, 0);
	std::fill_n(g._pge_liveTable1, 256, nullptr);
	std::fill_n(g._pge_liveTable2, 256, nullptr);
	for (size_t i = 0; i < project.native.actors.size(); ++i) {
		const auto &a = project.native.actors[i];
		auto &init = g._res._pgeInit[i];
		g.pge_loadForCurrentLevel(i);
		auto &live = g._pgeLive[i];
		if (init.skill <= g._skillLevel) {
			live.next_PGE_in_room = g._pge_liveTable1[live.room_location];
			g._pge_liveTable1[live.room_location] = &live;
		}
	}
	std::vector<uint8_t> coverage(RoomWidth * RoomHeight);
	g._vid._spriteCoverage = coverage.data();
	g.prepareAnims();
	g.drawAnims();
	g._vid._spriteCoverage = nullptr;
	uint8_t palette[768];
	_stub->getPalette(palette, 256);
	Image image{RoomWidth, RoomHeight, std::vector<uint32_t>(RoomWidth * RoomHeight)};
	for (size_t i = 0; i < coverage.size(); ++i)
		if (coverage[i]) {
			const auto *c = palette + g._vid._frontLayer[i] * 3;
			image.pixels[i] = rgba(c[0], c[1], c[2]);
		}
	std::copy(pristine.begin(), pristine.end(), g._res._pgeInit);
	g._res._pgeNum = count;
	installNativeLogic(g, baseline);
	// Restore pristine actor lists for subsequent cached reference-room captures.
	std::fill_n(g._pge_liveTable1, 256, nullptr);
	std::fill_n(g._pge_liveTable2, 256, nullptr);
	for (int i = 0; i < g._res._pgeNum; ++i) {
		g.pge_loadForCurrentLevel(i);
		auto &a = g._pgeLive[i];
		if (g._res._pgeInit[i].skill <= g._skillLevel) {
			a.next_PGE_in_room = g._pge_liveTable1[a.room_location];
			g._pge_liveTable1[a.room_location] = &a;
		}
	}
	_actorPreview = image;
	_actorPreviewRoom = slot;
	_actorPreviewRevision = revision;
	return image;
}
std::filesystem::path OriginalLevel::exportScenery(const std::filesystem::path &folder) {
	const auto p = createProject(false);
	compileCollision(p); // Validate everything before creating output.
	if (std::filesystem::exists(folder) || !std::filesystem::create_directories(folder))
		throw std::runtime_error("Choose a NEW folder for the scenery copy");
	std::filesystem::create_directory(folder / "art");
	for (int i = 0; i < MaxRooms; ++i) {
		const auto &r = p.rooms[i];
		if (!r.art[0].file.empty()) {
			const auto &decoded = room(i);
			writePNG(decoded.scenery, folder / r.art[0].file);
			writePNG(decoded.foreground, folder / r.art[2].file);
		}
	}
	std::ofstream note(folder / "README.txt");
	note << "Private level-one scenery study decoded from your game assets.\n"
	        "Original files are unchanged. This copy contains copyrighted game artwork.\n"
	        "All 64 room slots, collision cells, exits and player start are retained.\n"
	        "World view is an indexed 8x8 slot shelf, not a geographic map.\n"
	        "Back is flattened scenery; Front is its native priority mask.\n"
	        "No campaign actors, puzzles, story scripts, palette animation or saves are imported.\n"
	        "PLAY tests custom geometry with the native player. Add modules to build a remix.\n";
	note.close();
	if (!note)
		throw std::runtime_error("Could not write scenery-copy notes");
	const auto path = folder / "project.r2project.json";
	saveProject(p, path); // Publish the usable project only after every dependency.
	return path;
}
} // namespace editor

namespace editor {
void exportNativeCampaign(const assets::Source &source, const std::filesystem::path &folder) {
	namespace fs = std::filesystem;
	const auto destination = fs::absolute(folder);
	if (fs::exists(destination))
		throw std::runtime_error(
		    "Choose a new campaign folder; existing campaigns are never overwritten");
	fs::create_directories(destination.parent_path());
	const auto staging = destination.parent_path() / (".campaign-import-" + makeProject().id);
	if (!fs::create_directory(staging))
		throw std::runtime_error("Cannot create campaign staging directory");
	try {
		PreviewSystem loader;
		loader.audible = false;
		std::ofstream guide(staging / "CAMPAIGN.md");
		guide << "# Private native campaign\n\nDecoded from your installed Flashback assets. "
		         "Original game files stay untouched.\n"
		      << "This folder contains asset-derived script tables; keep it with your private game "
		         "data.\n\n"
		      << "Open level-1.r2project.json in REminiscence2 Editor. Module Placement lists the "
		         "complete\n"
		      << "native puzzle catalog. Module Editing exposes actor connections and native state "
		         "rules.\n"
		      << "Campaign level switches between the seven files. Save each level before "
		         "switching.\n"
		      << "Use Play here to test a room, or authored start to follow campaign "
		         "progression.\n\n";
		Campaign campaign;
		campaign.name = "Native campaign";
		for (int level = 0; level < 7; ++level) {
			OriginalLevel original(source, &loader, level);
			const auto p = original.createProject(true);
			validateNativeLogic(p.native, p.native);
			saveProject(p, staging / ("level-" + std::to_string(level + 1) + ".r2project.json"));
			auto stage = newStage(campaign, StageKind::Level);
			stage.name = p.name;
			stage.level = "level-" + std::to_string(level + 1) + ".r2project.json";
			stage.routes.push_back({"complete", "Continue", "stage-" + std::to_string(level + 2)});
			if (level == 0)
				campaign.entry = stage.id;
			campaign.stages.push_back(stage);
			guide << "- Level " << level + 1 << ": " << p.native.actors.size() << " actors, "
			      << p.native.puzzles.size() << " modules.\n";
		}
		auto end = newStage(campaign, StageKind::End);
		end.name = "Campaign complete";
		end.elements[0].text = "Campaign complete. Thank you for playing.";
		campaign.stages.push_back(end);
		saveCampaign(campaign, staging / "campaign.r2campaign.json");
		guide << "\nOpen campaign.r2campaign.json in the fourth workspace to reroute levels or add "
		         "interstitial scenes.\n";
		guide.close();
		if (!guide)
			throw std::runtime_error("Cannot write campaign guide");
		fs::rename(staging, destination);
	} catch (...) {
		std::error_code ignored;
		fs::remove_all(staging, ignored);
		throw;
	}
}
} // namespace editor
