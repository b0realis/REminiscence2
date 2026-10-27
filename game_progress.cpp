// SPDX-License-Identifier: GPL-3.0-or-later
#include "authoring_hooks.h"
#include "game.h"
#include "gameplay_identity.h"
#include "save_catalog.h"
#include "systemstub.h"
#include "util.h"
#include <algorithm>
#include <filesystem>
#include <chrono>

uint32_t gameplayFingerprint(const Resource &res) {
	uint32_t result;
	result = 2166136261u;
	auto hash = [&](uint32_t value) {
		for (int n = 0; n < 4; ++n) {
			result = (result ^ (value & 255)) * 16777619u;
			value >>= 8;
		}
	};
	for (uint8_t value : res._ctData)
		hash(value);
	for (unsigned n = 0; n < res._pgeNum; ++n) {
		const auto &p = res._pgeInit[n];
		hash(p.type);
		hash(uint16_t(p.pos_x));
		hash(uint16_t(p.pos_y));
		hash(p.obj_node_number);
		hash(p.life);
		for (auto value : p.data)
			hash(uint16_t(value));
		hash(p.object_type);
		hash(p.init_room);
		hash(p.room_location);
		hash(p.init_flags);
		hash(p.colliding_icon_num);
		hash(p.icon_num);
		hash(p.object_id);
		hash(p.skill);
		hash(p.mirror_x);
		hash(p.flags);
		hash(p.collision_data_len);
		hash(p.text_num);
	}
	for (unsigned n = 0; n < res._numObjectNodes; ++n) {
		const auto *node = res._objectNodesMap[n];
		hash(node->num_objects);
		for (unsigned i = 0; i < node->num_objects; ++i) {
			const auto &o = node->objects[i];
			hash(o.type);
			hash(uint8_t(o.dx));
			hash(uint8_t(o.dy));
			hash(o.init_obj_type);
			hash(o.opcode1);
			hash(o.opcode2);
			hash(o.opcode3);
			hash(o.flags);
			hash(o.init_obj_number);
			hash(o.opcode_arg1);
			hash(o.opcode_arg2);
			hash(o.opcode_arg3);
		}
	}

	return result;
}

namespace {
// Probe only immutable simulation resources. Rejected autosaves never replace
// the live level, its actors, audio, or rewind history.
bool identity(Resource &r, int level) {
	if (level < 0 || level > 6)
		return false;
	const auto &data = Game::_gameLevels[level];
	if (r.isMac()) {
		static const char *const numbers[] = {"1", "2", "3", "4-1", "4-2", "5-1", "5-2"};
		const auto base = std::string("Level ") + numbers[level];
		const auto map = std::string("Level ") + numbers[level][0] + " map";
		if (!r._mac || !r._mac->findEntry((base + " objects").c_str(), "OBJD") ||
		    !r._mac->findEntry((base + " conditions").c_str(), "COND") ||
		    !r._mac->findEntry(map.c_str(), "LMAP"))
			return false;
		auto *bytes = r.decodeResourceMacData((base + " objects").c_str(), true, "OBJD");
		r.decodePGE(bytes, r._resourceMacDataSize);
		free(bytes);
		if (!r._pgeNum)
			return false;
		bytes = r.decodeResourceMacData((base + " conditions").c_str(), true, "COND");
		r.decodeOBJ(bytes, r._resourceMacDataSize);
		free(bytes);
		if (!r._numObjectNodes)
			return false;
		bytes = r.decodeResourceMacData(map.c_str(), true, "LMAP");
		const bool size = bytes && r._resourceMacDataSize == sizeof(r._ctData);
		if (size)
			memcpy(r._ctData, bytes, sizeof(r._ctData));
		free(bytes);
		if (!r._mac->findEntry((base + " sequences").c_str(), "ANIM"))
			return false;
		r._ani = r.decodeResourceMacData((base + " sequences").c_str(), true, "ANIM");
		r._aniSize = r._resourceMacDataSize;
		return size;
	}
	const char *ct = data.name, *pge = data.name2, *obj = data.name2;
	auto type = Resource::OT_OBJ;
	const char *ext = ".obj";
	if (r.isAmiga()) {
		ct = Game::_gameLevels[level == 4 ? 3 : level == 6 ? 5 : level].nameAmiga;
		pge = obj = data.nameAmiga;
		type = Resource::OT_OBC;
		ext = ".obc";
		if (r._isDemo) {
			ct = obj = "demo";
			pge = "demof";
			type = Resource::OT_OBJ;
			ext = ".obj";
		}
	} else if (r.isSegaCD()) {
		static const char *graphics[] = {"level1", "level2", "dt",    "level3",
		                                 "level3", "level4", "level4"};
		ct = graphics[level];
		pge = obj = data.nameAmiga;
	} else if (r.isSega())
		obj = "GLOBAL";
	const char *ani = r.isSega() && !r.isSegaCD() ? "GLOBAL" : pge;
	if (!r.fileExists((std::string(ani) + ".ani").c_str()) ||
	    !r.fileExists((std::string(ct) + ".ct").c_str()) ||
	    !r.fileExists((std::string(pge) + ".pge").c_str()) ||
	    !r.fileExists((std::string(obj) + ext).c_str()))
		return false;
	r.load(ct, Resource::OT_CT);
	r.load(pge, Resource::OT_PGE);
	r.load(obj, type);
	r.load(ani, Resource::OT_ANI);
	return r._pgeNum != 0 && r._numObjectNodes != 0;
}
} // namespace

std::shared_ptr<const progress::Preview> Game::capturePreview() {
	auto p = std::make_shared<progress::Preview>();
	p->width = progress::PreviewWidth;
	p->height = progress::PreviewHeight;
	p->rgba.resize(p->width * p->height * 4);
	uint8_t palette[768];
	_stub->getPalette(palette, 256);
	for (unsigned y = 0; y < p->height; ++y)
		for (unsigned x = 0; x < p->width; ++x) {
			const unsigned color =
			    _vid._frontLayer[(y * _vid._h / p->height) * _vid._w + x * _vid._w / p->width];
			auto *dst = &p->rgba[(y * p->width + x) * 4];
			memcpy(dst, palette + color * 3, 3);
			dst[3] = 255;
		}
	return p;
}
progress::Snapshot Game::captureSnapshot() {
	progress::Snapshot snapshot;
	snapshot.edition = _res._type;
	snapshot.level = _currentLevel;
	snapshot.room = _pgeLive[0].room_location;
	snapshot.skill = _skillLevel;
	snapshot.health = _pgeLive[0].life;
	snapshot.entities = _res._pgeNum;
	snapshot.fingerprint = _assetFingerprint;
	snapshot.playTicks = _playTicks;
	snapshot.preview = capturePreview();
	File f;
	f.openMemoryBuffer(32768);
	writeGameState(f);
	snapshot.state.resize(f.size());
	f.seek(0);
	if (f.ioErr() || f.read(snapshot.state.data(), snapshot.state.size()) != snapshot.state.size())
		snapshot.state.clear();
	return snapshot;
}
bool Game::saveManualState(int slot, const std::string &label, bool protect, std::string &reason) {
	if (_authoring && _authoring->ownsSaves()) return _authoring->saveCampaign(*this, reason);
	if (_demoBin != -1) {
		reason = "Demo playback cannot be saved.";
		return false;
	}
	if (!_res._pgeNum || _pgeLive[0].life <= 0 || _deathCutsceneCounter) {
		reason = "Save after returning to living gameplay.";
		return false;
	}
	const auto start = std::chrono::steady_clock::now();
	auto snapshot = captureSnapshot();
	snapshot.label = label;
	snapshot.protectedSave = protect;
	const bool result = progress::saveManual(_savePath, slot, std::move(snapshot), reason);
	_lastDiskSaveMs =
	    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	_worstDiskSaveMs = std::max(_worstDiskSaveMs, _lastDiskSaveMs);
	return result;
}
bool Game::loadManualState(int level, int slot, std::string &reason) {
	if (_authoring && _authoring->ownsSaves()) return _authoring->loadCampaign(*this, reason);
	if (level < 0 || level > 6 || slot < 0 || slot > 99) {
		reason = "Invalid level or save slot.";
		return false;
	}
	const auto modern =
	    (std::filesystem::u8path(_savePath) / progress::manualFilename(_res._type, level, slot))
	        .u8string();
	std::error_code ec;
	if (std::filesystem::exists(std::filesystem::u8path(modern), ec)) {
		progress::Snapshot snapshot;
		if (!progress::read(modern, snapshot, reason) || !snapshot.manual ||
		    snapshot.edition != _res._type || snapshot.level != level) {
			reason =
			    "Manual save is damaged or belongs to different assets. Current progress was kept.";
			return false;
		}
		if (!loadPersistent(snapshot, reason))
			return false;
		reason = "Manual save restored.";
		return true;
	}
	// Legacy files have no envelope. Read the complete bounded stream, derive
	// only the summary, then use the same resource preflight as modern saves.
	File f;
	if (!f.open(saves::name(_res._type, level, slot).c_str(), "zrb", _savePath) &&
	    !f.open(saves::name(-1, level, slot).c_str(), "zrb", _savePath)) {
		reason = "No save exists in this level and slot.";
		return false;
	}
	const unsigned size = f.size();
	if (size < 81 || size > 65536) {
		reason = "Save size is invalid.";
		return false;
	}
	progress::Snapshot snapshot;
	snapshot.state.resize(size);
	f.seek(0);
	if (f.read(snapshot.state.data(), size) != size || !f.atEnd()) {
		reason = "Save is truncated or its compressed checksum is invalid.";
		return false;
	}
	snapshot.edition = _res._type;
	snapshot.level = level;
	snapshot.skill = snapshot.state[38];
	if (!loadPersistent(snapshot, reason))
		return false;
	reason = slot ? "Legacy manual save restored." : "Checkpoint restored.";
	return true;
}
bool Game::savePersistent(std::string &reason) {
	if (!_activeProgress || _demoBin != -1 || !_res._pgeNum || _pgeLive[0].life <= 0 ||
	    _deathCutsceneCounter || _cut._id != 0xFFFF || _loadMap || _currentRoom >= 64) {
		reason = "Autosave waits for an active, living gameplay frame.";
		return false;
	}
	const auto start = std::chrono::steady_clock::now();
	const bool saved = progress::save(_savePath, captureSnapshot(), reason);
	_lastDiskSaveMs =
	    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	_worstDiskSaveMs = std::max(_worstDiskSaveMs, _lastDiskSaveMs);
	if (!saved)
		return false;
	_lastPersistentTick = _playTicks;
	_persistentDue = false;
	_stub->notify("Autosave stored.");
	return true;
}
bool Game::loadPersistent(const progress::Snapshot &snapshot, std::string &reason) {
	if (snapshot.edition != _res._type || snapshot.level < 0 || snapshot.level > 6) {
		reason = "Autosave belongs to a different edition or unsupported level.";
		return false;
	}
	unsigned entities = _res._pgeNum;
	uint32_t fingerprint = _assetFingerprint;
	std::unique_ptr<Resource> probe;
	const Resource *validation = &_res;
	if (!entities || snapshot.level != _currentLevel) {
		probe = std::make_unique<Resource>(_fs, _res._type, _res._lang);
		probe->init(true);
		if (!identity(*probe, snapshot.level)) {
			reason = "Autosave level resources are missing or invalid.";
			return false;
		}
		entities = probe->_pgeNum;
		fingerprint = gameplayFingerprint(*probe);
		validation = probe.get();
	}
	File f;
	f.openMemoryBuffer(snapshot.state.size());
	f.write(snapshot.state.data(), snapshot.state.size());
	int version;
	if (!saves::validate(f, entities, version, reason, _res._type, snapshot.level, fingerprint,
	                     validation))
		return false;
	// All file and asset checks have passed. Commit at the paused boundary.
	const int savedSkill = f.readByte();
	f.seek(38);
	if (version < 4)
		warning("Legacy save has no asset identity or random/journal state; use matching original "
		        "assets");
	_demoBin = -1;
	_skillLevel = _menu._skill = savedSkill;
	if (!_res._pgeNum || _currentLevel != snapshot.level) {
		_currentLevel = snapshot.level;
		loadLevelData();
	}
	_menu._level = _currentLevel;
	loadState(&f, version);
	clearStateRewind();
	_playTicks = _lastPersistentTick = _lastRewindTick = snapshot.playTicks;
	_persistentDue = false;
	_nextPersistentAttempt = 0;
	_activeProgress = true;
	_continued = true;
	_cut._id = 0xFFFF;
	_endLoop = false;
	refreshRestoredFrame();
	reason = "Autosave restored.";
	return true;
}
bool Game::continueLatest(std::string &reason) {
	const auto candidates = progress::scan(_savePath, _res._type);
	for (const auto &snapshot : candidates)
		if (loadPersistent(snapshot, reason))
			return true;
	if (candidates.empty())
		reason = "No valid autosave is available for this asset edition.";
	return false;
}
bool Game::resumeLatestSave(std::string &reason) {
	for (const auto &snapshot : progress::recent(_savePath, _res._type))
		if (loadPersistent(snapshot, reason))
			return true;
	reason = "No compatible quick/manual save or autosave could be restored.";
	return false;
}
void Game::refreshRestoredFrame() {
	_currentRoom = _pgeLive[0].room_location;
	_vid.setPalette0xF();
	_vid.setTextPalette();
	loadLevelMap();
	_loadMap = false;
	memcpy(_vid._frontLayer, _vid._backLayer, _vid._layerSize);
	// The per-frame collision cache is derived and is not serialized. A fresh
	// process has no cache to read; the next pge_prepare builds it before simulation.
	col_clearState();
	prepareAnims();
	drawAnims();
	drawCurrentInventoryItem();
	_vid.fullRefresh();
	_stub->copyRect(0, 0, _vid._w, _vid._h, _vid._frontLayer, _vid._w);
	_frameTimestamp = _saveTimestamp = _stub->getTimeStamp();
}
void Game::updateProgress() {
	if (_authoring && _authoring->ownsSaves()) { ++_playTicks; return; }
	if (_demoBin != -1) {
		_stub->setContextHint({});
		return;
	}
	++_playTicks;
	_activeProgress = true;
	if (_autoSave && _playTicks - _lastRewindTick >= 150 && _pgeLive[0].life > 0 &&
	    !_deathCutsceneCounter && _cut._id == 0xFFFF && !_loadMap)
		saveStateRewind();
	const auto kind = _hints.update(_playTicks, _preferences.contextualHints, _nearInteraction,
	                                _pgeLive[0].current_inventory_PGE != 255,
	                                _pgeLive[0].life > 0 && _pgeLive[0].life <= 1);
	auto inputPreferences = _preferences;
	_stub->readPreferences(inputPreferences);
	_stub->setContextHint(hints::text(kind, inputPreferences, _stub->rawJoystickActive(),
	                                  _stub->controllerLabelStyle()));
	if (!_preferences.diskAutosave || _playTicks < _nextPersistentAttempt ||
	    (!_persistentDue &&
	     _playTicks - _lastPersistentTick < uint64_t(_preferences.diskAutosaveSeconds) * 30))
		return;
	std::string reason;
	if (!savePersistent(reason)) {
		// Retry disk errors infrequently; an unsafe frame is retried as soon as it is coherent.
		const bool unsafe =
		    _pgeLive[0].life <= 0 || _deathCutsceneCounter || _cut._id != 0xFFFF || _loadMap;
		if (!unsafe) {
			warning("Autosave: %s", reason.c_str());
			_stub->notify("Autosave failed: " + reason, true);
			_nextPersistentAttempt = _playTicks + 900;
		}
	}
}
