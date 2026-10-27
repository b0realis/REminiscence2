// SPDX-License-Identifier: GPL-3.0-or-later
#include "native.h"
#include "native_logic.h"
#include "original.h"
#include "preview_system.h"
#include "runtime.h"
#include "campaign_progress.h"
#include "game.h"
#include "fs.h"
#include "gameplay_identity.h"
#include "atomic_file.h"
#include "file.h"
#include "third_party/editor/json.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
namespace editor {
using json = nlohmann::json;
namespace fs = std::filesystem;
static Project snapshot(Game &g) {
	auto p = makeProject();
	p.rooms.clear();
	p.name = "Native level " + std::to_string(g._currentLevel + 1);
	char hash[9];
	std::snprintf(hash, sizeof(hash), "%08x", gameplayFingerprint(g._res));
	p.native = {true, g._currentLevel, int(g._res._type), hash, {}};
	for (int i = 0; i < 64; ++i) {
		auto &r = addRoom(p, i % 8, i / 8);
		r.id = "original-room-" + std::to_string(i);
		r.name = "Slot " + std::to_string(i);
		for (int c = 0; c < 112; ++c)
			r.collision[c] = g._res._ctData[256 + i * 112 + c];
		for (int d = 0; d < 4; ++d) {
			int target = g._res._ctData[d * 64 + i];
			r.exits[d] =
			    target >= 0 && target < 64 ? "original-room-" + std::to_string(target) : "";
		}
	}
	for (int i = 0; i < g._res._pgeNum; ++i) {
		const auto &a = g._res._pgeInit[i];
		p.native.actors.push_back({i,
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
	captureNativeLogic(g, p.native);
	const auto &a = p.native.actors[0];
	p.spawn = {p.rooms.at(a.room).id, a.x, a.y};
	p.spawnLeft = a.facingLeft;
	return p;
}
static bool sameSetup(const NativeActor &a, const NativeActor &b) {
	return a.type == b.type && a.behavior == b.behavior && a.data == b.data &&
	       a.source == b.source && a.objectKind == b.objectKind && a.room == b.room && a.x == b.x &&
	       a.y == b.y && a.life == b.life && a.facingLeft == b.facingLeft && a.removed == b.removed;
}
static std::string readText(const fs::path &path, uintmax_t limit = 8 * 1024 * 1024) {
	if (fs::file_size(path) > limit)
		throw std::runtime_error("Editor session file exceeds size limit");
	std::ifstream file(path, std::ios::binary);
	std::ostringstream text;
	text << file.rdbuf();
	if (!file || file.bad())
		throw std::runtime_error("Cannot read editor session file");
	return text.str();
}
static json envelope(const std::string &text) {
	return json::parse(text, [](int depth, json::parse_event_t, json &) {
		if (depth > 8)
			throw std::runtime_error("Editor session nesting limit exceeded");
		return true;
	});
}
static void writeText(const fs::path &path, const std::string &text) {
	std::string reason;
	if (!writeAtomicFile(
	        path.u8string(), false, [&](File &f) { f.write(text.data(), text.size()); }, reason))
		throw std::runtime_error("Editor session write: " + reason);
}
NativeSession::NativeSession(const fs::path &file, const assets::Source &source,
                             const fs::path &control)
    : _root(fs::absolute(file).parent_path()), _control(control), _source(source),
      _project(loadProject(file)) {
	if (!_project.native.enabled)
		throw std::runtime_error("This is not a native campaign project");
	if (_project.native.edition != source.edition)
		throw std::runtime_error("Select the edition used by this native project");
	_images.root = _root;
	validateImages(_project, _images);
	_levels[_project.native.level] = _project;
}
NativeSession::~NativeSession() = default;
void NativeSession::verify(const Project &p, const Project &original) const {
	compileCollision(p);
	if (!p.native.enabled || p.native.level != original.native.level ||
	    p.native.edition != original.native.edition ||
	    p.native.fingerprint != original.native.fingerprint)
		throw std::runtime_error(
		    "Native asset fingerprint/actor count mismatch; original assets and IDs must match");
	validateNativeLogic(p.native, original.native);
	const auto &a = p.native.actors[0];
	if (a.room >= 64 || roomIndex(p, p.spawn.room) != a.room || p.spawn.x != a.x ||
	    p.spawn.y != a.y || p.spawnLeft != a.facingLeft)
		throw std::runtime_error("Native player start must match actor 0");
}
void NativeSession::loaded(Game &g) {
	auto original = snapshot(g);
	_originals[g._currentLevel] = original;
	if (!_levels.count(g._currentLevel)) {
		const auto sibling =
		    _root / ("level-" + std::to_string(g._currentLevel + 1) + ".r2project.json");
		_levels[g._currentLevel] = fs::exists(sibling) ? loadProject(sibling) : original;
	}
	auto next = _levels.at(g._currentLevel);
	verify(next, original);
	Images checked;
	checked.root = _root;
	validateImages(next, checked);
	if (next.native.programs.empty()) {
		next.native.programs = original.native.programs;
		next.native.puzzles = original.native.puzzles;
	}
	installNativeLogic(g, next.native);
	_project = std::move(next);
	_images = std::move(checked);
	const auto ct = compileCollision(_project);
	// Preserve original out-of-range exit sentinels, unless an authored exit differs.
	const auto base = compileCollision(original);
	for (size_t i = 0; i < ct.size(); ++i)
		if (ct[i] != base[i])
			g._res._ctData[i] = ct[i];
	installNativeActors(g, _project.native);
	if (_launchAt) {
		auto &player = g._res._pgeInit[0];
		player.init_room = roomIndex(_project, _launchAt->at.room);
		player.pos_x = _launchAt->at.x;
		player.pos_y = _launchAt->at.y;
		player.mirror_x = !_launchAt->facingLeft;
		player.type = 1; // Native standing state, bypass the scripted level arrival.
	}
	_overlay.reset();
	_events.clear();
	_messages.clear();
	_stepPending = false;
	_definitionRevision = nativeRevision(_project.native);
	_drawing = false;
	_restartRequired = false;
}
void NativeSession::apply(Game &g, const Project &candidate, bool restart) {
	Project original;
	if (_originals.count(candidate.native.level))
		original = _originals.at(candidate.native.level);
	else {
		PreviewSystem stub;
		stub.audible = false;
		OriginalLevel reference(_source, &stub, candidate.native.level);
		original = reference.createProject(true);
	}
	verify(candidate, original);
	Images checked;
	checked.root = _root;
	validateImages(candidate, checked);
	if (restart) {
		_levels[candidate.native.level] = candidate;
		g._currentLevel = candidate.native.level;
		g.clearStateRewind();
		g.loadLevelData();
		g.resetGameState();
		g.loadLevelMap();
		g._vid.setPalette0xF();
		g._vid.setTextPalette();
		g._vid.fullRefresh();
		g._endLoop = false;
		_paused = true;
		_restartRequired = false;
		_message = "Restarted at edited level; paused";
		return;
	}
	if (candidate.native.level != g._currentLevel) {
		_restartRequired = true;
		throw std::runtime_error("Restart at edited level to switch the running campaign level");
	}
	if (!candidate.native.programs.empty() && !sameNativeLogic(candidate.native, _project.native)) {
		_restartRequired = true;
		throw std::runtime_error("State-machine rules changed: restart to apply safely");
	}
	bool sameModules = _project.modules.size() == candidate.modules.size();
	for (size_t i = 0; sameModules && i < _project.modules.size(); ++i)
		sameModules = moduleRevision(_project.modules[i]) == moduleRevision(candidate.modules[i]);
	if (!sameModules) {
		_restartRequired = true;
		throw std::runtime_error("Module logic changed: restart to apply safely");
	}
	if (candidate.native.actors.size() != _project.native.actors.size()) {
		_restartRequired = true;
		throw std::runtime_error("Puzzle modules added: restart to allocate their actor slots");
	}
	for (size_t i = 0; i < candidate.native.actors.size(); ++i)
		if (!sameSetup(candidate.native.actors[i], _project.native.actors[i])) {
			_restartRequired = true;
			throw std::runtime_error(
			    "Actor setup changed: restart required to preserve script/inventory dependencies");
		}
	const auto old = compileCollision(_project), next = compileCollision(candidate);
	for (size_t i = 0; i < next.size(); ++i)
		if (old[i] != next[i]) {
			if (g._res._ctData[i] != old[i]) {
				_restartRequired = true;
				throw std::runtime_error(
				    "A puzzle changed an edited collision cell/link; restart required");
			}
			if (i >= 256 && next[i] && !old[i]) {
				const int slot = int(i - 256) / 112, cell = int(i - 256) % 112, x = cell % 16 * 16,
				          y = cell / 16 * 36 - 8;
				for (int a = 0; a < g._res._pgeNum; ++a) {
					const auto &live = g._pgeLive[a];
					if (live.life > 0 && live.room_location == slot && g._pge_liveTable2[a] &&
					    live.pos_x + 16 > x && live.pos_x < x + 16 && live.pos_y + 2 > y &&
					    live.pos_y - 30 < y + 36) {
						_restartRequired = true;
						throw std::runtime_error(
						    "New collision overlaps a live actor; move it or restart");
					}
				}
			}
		}
	// Everything is preflighted. Apply only authored differences; keep doors,
	// inventory, counters, messages, actor links and all unrelated live CT state.
	for (size_t i = 0; i < next.size(); ++i)
		if (old[i] != next[i])
			g._res._ctData[i] = next[i];
	_project = candidate;
	_definitionRevision = nativeRevision(_project.native);
	_levels[g._currentLevel] = candidate;
	_images = std::move(checked);
	g.clearStateRewind();
	g._vid.fullRefresh();
	_restartRequired = false;
	_message = "Applied art/geometry between frames; native puzzle and actor state preserved";
}
void NativeSession::playHere(Game &g, const Project &candidate, const PlaytestStart &start) {
	validatePlaytestStart(candidate, start);
	// Verify map and the standing-player/pistol dependency before replacing live state.
	PreviewSystem preview;
	preview.audible = false;
	OriginalLevel reference(_source, &preview, candidate.native.level);
	verify(candidate, reference.createProject(true));
	if (!reference.hasMap(roomIndex(candidate, start.at.room)))
		throw std::runtime_error("This native slot has no map; choose a playable room");
	const auto &actors = candidate.native.actors;
	if (!reference.supportsDirectStart() || actors[0].behavior != 1 || actors[0].data[0] <= 0 ||
	    actors[0].data[0] >= int(actors.size()) || actors[actors[0].data[0]].behavior != 4)
		throw std::runtime_error("Unrecognized native player/pistol dependency");
	_launchAt = start;
	try {
		apply(g, candidate, true);
	} catch (...) {
		_launchAt.reset();
		throw;
	}
	_launchAt.reset();
	g._randSeed = 1337;
	g._score = 0;
	g._playTicks = 0;
	g._journal = {};
	g._cut._id = 0xFFFF;
	g._loadMap = false;
	g._inp_lastKeysHit = g._inp_lastKeysHitLeftRight = g._pge_inpKeysMask = 0;
	g._stub->_pi = {};
	// The level-arrival script normally equips the pistol. Keep its original ID.
	g.pge_updateInventory(&g._pgeLive[0], &g._pgeLive[actors[0].data[0]]);
	g._frameTimestamp = g._saveTimestamp = g._stub->getTimeStamp();
	_paused = false;
	_message = "Playing from TEST marker; original level scripts loaded with fresh state";
}
void NativeSession::publish(const fs::path &control, const std::string &action, const Project &p,
                            const PlaytestStart *start) {
	json command = {{"id", makeProject().id}, {"action", action}};
	if (action == "apply" || action == "restart" || action == "play-here" || action == "play-start")
		command["project"] = serialize(p);
	if (start)
		command["start"] = {{"room", start->at.room},
		                    {"x", start->at.x},
		                    {"y", start->at.y},
		                    {"facingLeft", start->facingLeft}};
	writeText(control, command.dump());
}
bool NativeSession::pending(const fs::path &control) {
	try {
		const auto request = envelope(readText(control));
		const auto reply = envelope(readText(control.u8string() + ".status", 65536));
		return request.at("id") != reply.at("ack");
	} catch (...) {
		return true;
	}
}
std::string NativeSession::statusText(const fs::path &control) {
	try {
		auto j = envelope(readText(control.u8string() + ".status", 65536));
		return "Level " + std::to_string(j.at("level").get<int>() + 1) + ", room " +
		       std::to_string(j.at("room").get<int>()) + ": " + j.at("message").get<std::string>();
	} catch (...) {
		return "Waiting for player / next simulation boundary (finish or skip any open "
		       "cinematic/dialogue)";
	}
}
void NativeSession::status(Game &g) {
	if (_control.empty())
		return;
	json j = {{"level", g._currentLevel}, {"room", g._currentRoom},
	          {"paused", _paused},        {"restartRequired", _restartRequired},
	          {"message", _message},      {"ack", _seen},
	          {"x", g._pgeLive[0].pos_x}, {"y", g._pgeLive[0].pos_y}};
	j["graphs"] = json::array();
	int remaining = 128;
	if (_overlay)
		for (size_t i = 0; i < _overlay->states().size(); ++i) {
			const auto &s = _overlay->states()[i].graph;
			json graph = {{"id", _project.modules[i].id},
			              {"revision", std::to_string(s.revision)},
			              {"nodes", json::array()},
			              {"trace", json::array()}};
			for (const auto &node : s.nodes)
				graph["nodes"].push_back({node.enabled, node.active, node.completed, node.friendly,
				                          node.patrol, node.held});
			const int count = std::min({int(s.trace.size()), 16, remaining});
			remaining -= count;
			for (auto e = s.trace.end() - count; e != s.trace.end(); ++e)
				graph["trace"].push_back({e->sequence, e->tick, int(e->kind), e->node, e->wire,
				                          int(e->signal), int(e->input), e->enabled, e->active,
				                          e->completed});
			j["graphs"].push_back(std::move(graph));
		}
	j["events"] = json::array();
	for (const auto &e : _events)
		j["events"].push_back({e.actor, e.behavior, e.rule, e.state, e.next, e.serial});
	j["nativeRevision"] = std::to_string(_definitionRevision);
	j["messages"] = json::array();
	for (const auto &e : _messages)
		j["messages"].push_back({e.source, e.target, e.value, e.serial});
	j["actors"] = json::array();
	for (int i = 0; i < g._res._pgeNum; ++i) {
		const auto &a = g._pgeLive[i];
		j["actors"].push_back({i, a.room_location, a.pos_x, a.pos_y, a.obj_type, a.life,
		                       a.counter_value, bool(g._pge_liveTable2[i])});
	}
	writeText(_control.u8string() + ".status", j.dump());
	_statusTime = g._stub->getTimeStamp();
}
void NativeSession::command(Game &g) {
	if (_control.empty() || !fs::exists(_control))
		return;
	const auto time = fs::last_write_time(_control);
	if (time == _lastCommand)
		return;
	_lastCommand = time;
	try {
		auto j = envelope(readText(_control));
		auto id = j.at("id").get<std::string>();
		if (id == _seen)
			return;
		_seen = id;
		const auto action = j.at("action").get<std::string>();
		if (action == "stop") {
			g._stub->_pi.quit = true;
			_message = "Player stopped";
		} else if (action == "pause") {
			_paused = true;
			_message = "Paused between frames; ready for edits";
		} else if (action == "step") {
			_stepPending = true;
			_paused = false;
			_message = "Stepping one native simulation frame";
		} else if (action == "resume") {
			_stepPending = false;
			_paused = false;
			_message = "Native campaign running";
		} else if (action == "play-here") {
			const auto &at = j.at("start");
			// Reject floating point and oversized integers before converting: JSON's
			// get<int>() otherwise truncates and can turn a bad coordinate into a valid one.
			auto coordinate = [&](const char *key, int maximum) {
				const auto &value = at.at(key);
				if (!value.is_number_integer() || value < 0 || value > maximum)
					throw std::runtime_error("Test position coordinate is outside the room");
				return value.get<int>();
			};
			PlaytestStart start{
			    {at.at("room").get<std::string>(), coordinate("x", 240), coordinate("y", 214)},
			    at.at("facingLeft").get<bool>()};
			playHere(g, deserialize(j.at("project").get<std::string>()), start);
			refreshPaused(g);
		} else if (action == "play-start") {
			apply(g, deserialize(j.at("project").get<std::string>()), true);
			_paused = false;
			_message = "Playing from the authored level start";
		} else if (action == "apply" || action == "restart") {
			_paused = true;
			apply(g, deserialize(j.at("project").get<std::string>()), action == "restart");
			refreshPaused(g);
		} else
			throw std::runtime_error("Unknown editor session command");
	} catch (const std::exception &e) {
		_message = e.what();
	}
	status(g);
}
void NativeSession::refreshPaused(Game &g) {
	// Redraw the current state without ticking scripts, message queues or HUD timers.
	std::copy_n(g._vid._backLayer, g._vid._layerSize, g._vid._frontLayer);
	beforeDraw(g);
	g.prepareAnims();
	g.drawAnims();
	afterActors(g);
	g.drawCurrentInventoryItem();
	afterDraw(g);
	g._vid.fullRefresh();
	g._vid.updateScreen();
}
Scene &NativeSession::campaignScene(Game &g) {
	if (!_overlay)
		_overlay = std::make_unique<Scene>(g, _project, _root);
	_overlay->bindCampaign(campaign);
	return *_overlay;
}
bool NativeSession::saveCampaign(Game &g, std::string &reason) {
	return campaign && campaign->save(campaignScene(g), reason);
}
bool NativeSession::loadCampaign(Game &g, std::string &reason) {
	return campaign && campaign->load(campaignScene(g), reason);
}
void NativeSession::boundary(Game &g) {
	if (campaign)
		campaign->poll(campaignScene(g));
	_drawing = false;
	g._vid._spriteCoverage = nullptr;
	command(g);
	if (_paused && !g._stub->_pi.quit) {
		refreshPaused(g);
		status(g);
	}
	while (_paused && !g._stub->_pi.quit) {
		g._stub->processEvents();
		command(g);
		g._stub->sleep(16);
	}
	if (g._stub->getTimeStamp() - _statusTime >= 500)
		status(g);
	// A resumed frame may enter a native cinematic before drawing gameplay.
	// Never let the paused art compositor replace that presentation.
	_drawing = false;
	g._vid._spriteCoverage = nullptr;
}
void NativeSession::beforeDraw(Game &g) {
	for (const auto &a : _project.native.actors)
		if (a.removed) {
			auto &live = g._pgeLive[a.id];
			const int old = live.room_location;
			live.room_location = 255;
			if (old != 255)
				g.pge_addToCurrentRoomList(&live, old);
		}
	_drawing = false;
	if (g._currentRoom >= 64 || g._currentLevel != _project.native.level)
		return;
	const auto &room = _project.rooms[g._currentRoom];
	if (_project.modules.empty() &&
	    std::all_of(room.art.begin(), room.art.end(), [](auto &a) { return a.file.empty(); }))
		return;
	_coverage.assign(g._vid._layerSize, 0);
	g._vid._spriteCoverage = _coverage.data();
	// With authored foreground, its alpha defines occlusion. Otherwise retain
	// native priority tests exactly, including script-driven background changes.
	if (!room.art[2].file.empty())
		for (int i = 0; i < g._vid._layerSize; ++i)
			g._vid._frontLayer[i] &= 0x7f;
	_drawing = true;
}
void NativeSession::afterActors(Game &g) {
	if (_drawing)
		_actorPixels.assign(g._vid._frontLayer, g._vid._frontLayer + g._vid._layerSize);
}
bool NativeSession::stopRequested() const {
	try {
		if (_control.empty() || !fs::exists(_control) || fs::file_size(_control) > 4096)
			return false;
		return envelope(readText(_control, 4096)).value("action", std::string{}) == "stop";
	} catch (...) {
		return false;
	}
}
void NativeSession::afterDraw(Game &g) {
	g._vid._spriteCoverage = nullptr;
	if (!_drawing)
		return;
	const auto &room = _project.rooms[g._currentRoom];
	uint8_t palette[768];
	g._stub->getPalette(palette, 256);
	auto rgb = [&](uint8_t index) {
		auto *c = palette + index * 3;
		return rgba(c[0], c[1], c[2]);
	};
	_composed = {256, 224, std::vector<uint32_t>(256 * 224)};
	for (size_t i = 0; i < _composed.pixels.size(); ++i)
		_composed.pixels[i] = rgb(g._vid._backLayer[i]);
	for (int layer = 0; layer < 2; ++layer)
		if (!room.art[layer].file.empty())
			blit(_composed, _images.get(room.art[layer].file), room.art[layer], 0, 0, 256, 224);
	for (size_t i = 0; i < _coverage.size(); ++i)
		if (_coverage[i])
			_composed.pixels[i] = rgb(g._vid._frontLayer[i]);
	if (!room.art[2].file.empty())
		blit(_composed, _images.get(room.art[2].file), room.art[2], 0, 0, 256, 224);
	else
		for (size_t i = 0; i < _composed.pixels.size(); ++i)
			if (g._vid._backLayer[i] & 0x80)
				_composed.pixels[i] = rgb(g._vid._backLayer[i]);
	// The indexed difference across the HUD stage includes icons and every text
	// palette, even when it is drawn on top of a sprite.
	for (size_t i = 0; i < _coverage.size(); ++i)
		if (g._vid._frontLayer[i] != _actorPixels[i])
			_composed.pixels[i] = rgb(g._vid._frontLayer[i]);
	if (_overlay)
		_overlay->renderOverlay(_composed);
	_baselinePixels.assign(g._vid._frontLayer, g._vid._frontLayer + g._vid._layerSize);
}
bool NativeSession::present(Game &g) {
	if (!_drawing || _composed.pixels.empty())
		return false;
	uint8_t palette[768];
	g._stub->getPalette(palette, 256);
	_rgb.resize(256 * 224 * 3);
	for (size_t i = 0; i < _composed.pixels.size(); ++i) {
		uint32_t c = _composed.pixels[i];
		// Inventory and dialogue can repaint the indexed frame while blocked in
		// their native UI loops. Retain authored scenery beneath those changes.
		if (g._vid._frontLayer[i] != _baselinePixels[i]) {
			auto *p = palette + g._vid._frontLayer[i] * 3;
			c = rgba(p[0], p[1], p[2]);
		}
		for (int k = 0; k < 3; ++k)
			_rgb[i * 3 + k] = uint8_t(c >> (k * 8));
	}
	g._stub->copyRectRgb24(0, 0, 256, 224, _rgb.data());
	g._stub->updateScreen(g._vid._shakeOffset);
	g._vid._shakeOffset = 0;
	return true;
}
int playNativeProject(const fs::path &path, const assets::Source &source, const fs::path &control,
                      std::string *exitRoute, CampaignRun *campaign) {
	std::unique_ptr<SystemStub> stub(SystemStub_SDL_create());
	try {
		NativeSession session(path, source, control);
		session.campaignStage = exitRoute != nullptr;
		session.campaign = campaign;
		const auto saves = fs::absolute(path).parent_path() / ".native-playtest-saves";
		fs::create_directories(saves);
		const std::string savePath = saves.u8string();
		FileSystem files(source.root.c_str(), source.excludedRoots);
		std::unique_ptr<Game, GameDeleter> game(
		    new Game(stub.get(), &files, &files, savePath.c_str(), session.project().native.level,
			         ResourceType(source.edition), LANG_EN, kWidescreenNone, false, 0));
		auto &g = *game;
		g._authoring = &session;
		settings::Preferences preferences;
		preferences.fullscreen = false;
		preferences.widescreen = kWidescreenNone;
		preferences.scale = 3;
		preferences.pauseOnFocusLoss = false;
		preferences.diskAutosave = false;
		preferences.rewind = false;
		g.configurePreferences(preferences);
		stub->configurePreferences(preferences);
		auto scaler = ScalerParameters::defaults();
		std::snprintf(scaler.name, sizeof(scaler.name), "point");
		scaler.factor = 3;
		stub->init("REminiscence2 - native campaign live preview", 256, 224, false, kWidescreenNone,
		           &scaler);
		initializeOriginalLevel(g);
		if (campaign)
			campaign->enter(session.campaignScene(g));
		g._mix.playMusic(Mixer::MUSIC_TRACK + Game::_gameLevels[g._currentLevel].track);
		g._vid._presentOverride = [&] { return session.present(g); };
		g._endLoop = false;
		stub->externalQuitRequested = [&] { return session.stopRequested(); };
		g._frameTimestamp = g._saveTimestamp = stub->getTimeStamp();
		while (!stub->_pi.quit && !g._endLoop)
			g.mainLoop();
		if (exitRoute)
			*exitRoute = session.exitRoute;
		if (campaign && session.campaignScene(g).canSave()) {
			campaign->progress.exit = session.exitRoute;
			campaign->capture(session.campaignScene(g));
			campaign->store();
		}
		stub->externalQuitRequested = {};
		game.reset();
		stub->destroy();
		return 0;
	} catch (const std::exception &e) {
		std::fprintf(stderr, "Native editor player: %s\n", e.what());
		stub->destroy();
		return 1;
	}
}
} // namespace editor

namespace editor {
bool NativeSession::allowRule(Game &g, LivePGE &actor, const Object &rule) {
	const int b = actor.init_PGE->obj_node_number;
	const int index = int(&rule - g._res._objectNodesMap[b]->objects);
	return b >= int(_project.native.programs.size()) ||
	       _project.native.programs[b].rules.at(index).enabled;
}
void NativeSession::ruleExecuted(Game &g, LivePGE &actor, const Object &rule) {
	if (_overlay)
		_overlay->nativeRuleFired(
		    actor.index,
		    int(&rule - g._res._objectNodesMap[actor.init_PGE->obj_node_number]->objects));
	const int b = actor.init_PGE->obj_node_number;
	_events.push_back({actor.index, b, int(&rule - g._res._objectNodesMap[b]->objects), rule.type,
	                   rule.init_obj_type, ++_eventSerial});
	if (_events.size() > 128)
		_events.erase(_events.begin());
}
std::vector<NativeSession::Event> NativeSession::readEvents(const fs::path &control,
                                                            const NativeCampaign *expected) {
	std::vector<Event> result;
	try {
		auto j = envelope(readText(control.u8string() + ".status", 65536));
		if (expected &&
		    j.value("nativeRevision", std::string{}) != std::to_string(nativeRevision(*expected)))
			return {};
		for (const auto &e : j.value("events", json::array())) {
			if (e.size() != 6 || result.size() >= 128)
				break;
			result.push_back({e[0], e[1], e[2], e[3], e[4], e[5]});
		}
	} catch (...) {
	}
	return result;
}
} // namespace editor

namespace editor {
bool NativeSession::beforeSimulation(Game &g) {
	if (_stepPending) {
		_stepPending = false;
		_paused = true;
		_message = "Stepped one native simulation frame; paused";
	}
	if (!_overlay && !_project.modules.empty())
		_overlay = std::make_unique<Scene>(g, _project, _root);
	if (_overlay) {
		const bool wasReading = _overlay->dialogueOpen();
		_overlay->tick();
		if (!_overlay->exitRoute().empty()) {
			exitRoute = _overlay->exitRoute();
			g._endLoop = true;
			return false;
		}
		return !wasReading && !_overlay->dialogueOpen();
	}
	return true;
}
bool NativeSession::beforeActor(Game &g, LivePGE &actor) {
	if (actor.index < _project.native.actors.size() &&
	    _project.native.actors[actor.index].removed) {
		const int old = actor.room_location;
		actor.room_location = 255;
		if (old != 255)
			g.pge_addToCurrentRoomList(&actor, old);
		g.pge_clearMessages(actor.index);
		return false;
	}
	return !_overlay || _overlay->beforeActor(g, actor);
}
void NativeSession::afterSimulation(Game &g) {
	for (const auto &a : _project.native.actors)
		if (a.removed) {
			auto &live = g._pgeLive[a.id];
			const int old = live.room_location;
			live.room_location = 255;
			if (old != 255)
				g.pge_addToCurrentRoomList(&live, old);
		}
	if (_overlay)
		_overlay->afterSimulation(g);
}
} // namespace editor

namespace editor {
std::map<std::string, GraphState> NativeSession::readGraphs(const fs::path &control) {
	std::map<std::string, GraphState> result;
	try {
		auto j = envelope(readText(control.u8string() + ".status", 65536));
		for (const auto &graph : j.value("graphs", json::array())) {
			if (result.size() >= 64 || graph.at("nodes").size() > 32 ||
			    graph.at("trace").size() > 16)
				break;
			GraphState s;
			s.revision = std::stoull(graph.at("revision").get<std::string>());
			for (const auto &n : graph.at("nodes")) {
				NodeState node;
				node.enabled = n.at(0);
				node.active = n.at(1);
				node.completed = n.at(2);
				if (n.size() >= 6) {
					node.friendly = n.at(3);
					node.patrol = n.at(4);
					node.held = n.at(5);
				}
				s.nodes.push_back(node);
			}
			for (const auto &e : graph.at("trace"))
				s.trace.push_back({e.at(0), e.at(1), TraceKind(e.at(2).get<int>()), e.at(3),
				                   e.at(4), Signal(e.at(5).get<int>()), Input(e.at(6).get<int>()),
				                   e.at(7), e.at(8), e.at(9)});
			result[graph.at("id").get<std::string>()] = std::move(s);
		}
	} catch (...) {
		result.clear();
	}
	return result;
}
} // namespace editor

namespace editor {
int NativeSession::behaviorIdentity(int behavior) const {
	return nativeProgramSource(_project.native, behavior);
}
void NativeSession::messageSent(Game &, int source, int target, int value) {
	_messages.push_back({source, target, value, ++_eventSerial});
	if (_messages.size() > 128)
		_messages.erase(_messages.begin());
}
NativeSession::Monitor NativeSession::readMonitor(const fs::path &control,
                                                  const NativeCampaign &expected) {
	Monitor result;
	try {
		const auto j = envelope(readText(control.u8string() + ".status", 65536));
		if (j.value("nativeRevision", std::string{}) != std::to_string(nativeRevision(expected)))
			return {};
		for (const auto &e : j.at("messages")) {
			if (e.size() != 4 || result.messages.size() >= 128)
				throw std::runtime_error("Invalid message trace");
			result.messages.push_back({e[0], e[1], e[2], e[3]});
		}
		for (const auto &a : j.at("actors")) {
			if (a.size() != 8 || result.actors.size() >= 256)
				throw std::runtime_error("Invalid actor trace");
			result.actors.push_back({a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]});
		}
	} catch (...) {
		return {};
	}
	return result;
}
} // namespace editor

namespace editor {
bool NativeSession::levelExit(Game &, int) {
	if (!campaignStage)
		return false;
	exitRoute = "complete";
	return true;
}
} // namespace editor
