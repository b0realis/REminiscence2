// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtime.h"
#include "native.h"
#include "campaign_progress.h"
#include "game.h"
#include "fs.h"
#include "systemstub.h"
#include "config.h"
#include <algorithm>
#include <cstdio>
#include <stdexcept>
namespace editor {
void GameDeleter::operator()(Game *game) const {
	// Game's resource member is destroyed before its mixer member. Join audio
	// first, including failed scene initialization, while samples still exist.
	if (game) {
		game->_mix.free();
		delete game;
	}
}
void initializeOriginalLevel(Game &g) {
	if (g._res.isSega() && !g._res.isSegaCD())
		throw std::runtime_error("Cartridge assets are not supported by editor playtests");
	g.initCutsceneResources();
	if (g._res.isAmiga()) {
		g._res.load("ICONE", Resource::OT_ICN, "SPR");
		g._res.load("ICON", Resource::OT_ICN, "SPR");
		g._res.load("PERSO", Resource::OT_SPM);
	} else if (g._res.isDOS()) {
		g._res.load("GLOBAL", Resource::OT_ICN);
		g._res.load("GLOBAL", Resource::OT_SPC);
		g._res.load("PERSO", Resource::OT_SPR);
		g._res.load_SPR_OFF("PERSO", g._res._spr1);
		g._res.load_FIB("GLOBAL");
	}
	g._mix.init();
	g._mix._mod._isAmiga = g._res.isAmiga();
	resetOriginalLevel(g);
}
void resetOriginalLevel(Game &g) {
	// Reset the simulation without reloading global fonts, sprites, icon banks
	// or samples. ICN loads append by design, and audio may still refer to SFX.
	g._mix.stopAll();
	g._mix.stopMusic();
	g._preferences.diskAutosave = false;
	g._preferences.storyJournal = false;
	g._preferences.rewind = false;
	g._preferences.pistolStyle = weapon::Style::Original;
	g._randSeed = 1337;
	g.loadLevelData();
	g.resetGameState();
	g.loadLevelMap();
	g._vid.setPalette0xF();
	g._vid.setTextPalette();
	g._mix.stopMusic();
}
Scene::Scene(Project p, const std::filesystem::path &root, const assets::Source &source,
             SystemStub *stub)
    : _project(std::move(p)), _source(source), _stub(stub),
      _baseCollision(compileCollision(_project)) {
	if (_project.native.enabled)
		throw std::runtime_error("Native projects require the native campaign player");
	if (source.edition != kResourceTypeDOS && source.edition != kResourceTypeAmiga &&
	    source.edition != kResourceTypeSega)
		throw std::runtime_error(
		    "Editor playtests currently support Amiga, DOS/GOG and Sega CD assets");
	_images.root = root;
	// Preflight every image before touching the game or opening audio.
	validateImages(_project, _images);
	restart();
}
Scene::~Scene() = default;
Game &Scene::game() {
	return *_game;
}
void Scene::restart() {
	if (_ownedGame && !_checkpointSnapshot.empty()) {
		restoreSession(_checkpointSnapshot);
		return;
	}
	if (_values == &_localValues)
		_localValues.clear();
	_audio.reset();
	_ownedGame.reset();
	_game = nullptr;
	_files.reset();
	_states.clear();
	_exitRoute.clear();
	_checkpointDue = false;
	_tick = 0;
	_previousUse = false;
	_paused = false;
	_dialogue.clear();
	_dialogues.clear();
	_previousDirection = 0;
	_motion = {};
	_page = 0;
	_notice.clear();
	_noticeUntil = 0;
	_files = std::make_unique<FileSystem>(_source.root.c_str(), _source.excludedRoots);
	_ownedGame.reset(new Game(_stub, _files.get(), _files.get(), "", 0,
	                          ResourceType(_source.edition), LANG_EN, kWidescreenNone, false, 0));
	_game = _ownedGame.get();
	auto &g = *_game;
	initializeOriginalLevel(g);
	// Certified level-one player bundle: player -> gun -> shot -> casing, plus
	// the invisible draw/holster input controller. That controller owns the
	// Space-key latch; omitting it leaves a working player who cannot draw.
	// Only these reviewed reference fields are relocated; counters remain scalars.
	const auto player = g._res._pgeInit[0];
	const int gunId = player.data[0];
	if (gunId <= 0 || gunId >= g._res._pgeNum)
		throw std::runtime_error("Unrecognized player dependency");
	const auto gun = g._res._pgeInit[gunId];
	const int shotId = gun.data[0];
	if (shotId <= 0 || shotId >= g._res._pgeNum)
		throw std::runtime_error("Unrecognized pistol dependency");
	const auto shot = g._res._pgeInit[shotId];
	const int shellId = shot.data[3];
	if (shellId <= 0 || shellId >= g._res._pgeNum)
		throw std::runtime_error("Unrecognized shot dependency");
	const auto shell = g._res._pgeInit[shellId];
	if (player.obj_node_number != 1 || gun.obj_node_number != 4 || shot.obj_node_number != 32 ||
	    shell.obj_node_number != 37)
		throw std::runtime_error("Asset revision has an unsupported player bundle");
	InitPGE controller{};
	bool foundController = false;
	for (int i = 0; i < g._res._pgeNum; ++i)
		if (g._res._pgeInit[i].type == 316 && (g._res._pgeInit[i].obj_node_number == 228 ||
		                                       g._res._pgeInit[i].obj_node_number == 229)) {
			controller = g._res._pgeInit[i];
			foundController = true;
			break;
		}
	if (!foundController)
		throw std::runtime_error("Asset revision has no supported pistol controller");
	InitPGE crawler{};
	bool foundCrawler = false;
	for (int i = 0; i < g._res._pgeNum; ++i)
		if (g._res._pgeInit[i].obj_node_number == 161 && g._res._pgeInit[i].type == 939) {
			crawler = g._res._pgeInit[i];
			foundCrawler = true;
			break;
		}
	auto *init = g._res._pgeInit;
	init[0] = player;
	init[1] = gun;
	init[2] = shot;
	init[3] = shell;
	init[4] = controller;
	init[0].type = 1;
	init[0].data[0] = 1;
	init[1].data[0] = 2;
	init[2].data[3] = 3;
	init[0].init_room = uint8_t(roomIndex(_project, _project.spawn.room));
	init[0].pos_x = int16_t(_project.spawn.x);
	init[0].pos_y = int16_t(_project.spawn.y);
	// In the original scripts mirror_x=1 faces right (not left).
	init[0].mirror_x = !_project.spawnLeft;
	if (_checkpoint) {
		init[0].init_room = roomIndex(_project, _checkpoint->room);
		init[0].pos_x = _checkpoint->x;
		init[0].pos_y = _checkpoint->y - 2;
	}
	// Holding-list membership is essential for the original spawning scripts.
	for (int i = 1; i < 5; ++i) {
		init[i].init_room = 0xFE;
		init[i].skill = 0;
	}
	g._res._pgeNum = 5;
	for (const auto &m : _project.modules) {
		ModuleState state;
		state.open = m.initiallyOpen;
		state.graph = initialGraphState(m);
		state.actors.assign(m.nodes.size(), -1);
		state.lastLife.assign(m.nodes.size(), 0);
		if (m.kind == Kind::Crawler) {
			if (g._res._pgeNum >= 255)
				throw std::runtime_error(
				    "Custom actor pool exhausted (250 authored actors maximum)");
			if (!foundCrawler)
				throw std::runtime_error("This edition has no certified crawler prefab");
			const int n = g._res._pgeNum++;
			init[n] = crawler;
			init[n].init_room = uint8_t(roomIndex(_project, m.a.room));
			init[n].pos_x = int16_t(m.a.x);
			init[n].pos_y = int16_t(m.a.y - 2);
			init[n].mirror_x = !m.facingLeft;
			init[n].life = m.health;
			init[n].skill = 0;
			state.actor = n;
		}
		for (size_t j = 0; j < m.nodes.size(); ++j) {
			const auto &node = m.nodes[j];
			if (node.kind != NodeKind::Actor)
				continue;
			if (!foundCrawler || g._res._pgeNum >= 255)
				throw std::runtime_error("Actor pool exhausted or crawler template missing");
			const int id = g._res._pgeNum++;
			init[id] = crawler;
			init[id].init_room = uint8_t(roomIndex(_project, node.at.room));
			init[id].pos_x = node.at.x;
			init[id].pos_y = node.at.y - 2;
			init[id].life = node.health;
			init[id].skill = 0;
			state.actors[j] = id;
			state.lastLife[j] = node.health;
		}
		_states.push_back(state);
	}
	std::fill(std::begin(g._pge_liveTable1), std::end(g._pge_liveTable1), nullptr);
	std::fill(std::begin(g._pge_liveTable2), std::end(g._pge_liveTable2), nullptr);
	g._currentRoom = init[0].init_room;
	for (int i = 0; i < g._res._pgeNum; ++i)
		g.pge_loadForCurrentLevel(i);
	for (int i = 0; i < g._res._pgeNum; ++i) {
		auto &a = g._pgeLive[i];
		a.next_PGE_in_room = g._pge_liveTable1[a.room_location];
		g._pge_liveTable1[a.room_location] = &a;
	}
	g.pge_updateInventory(&g._pgeLive[0], &g._pgeLive[1]);
	g.resetGameState();
	g._cut._id = 0xffff;
	g._endLoop = false;
	g._loadMap = false;
	g._inp_lastKeysHit = g._inp_lastKeysHitLeftRight = g._pge_inpKeysMask = 0;
	_coverage.assign(size_t(g._vid._layerSize), 0);
	g._authoring = this;
	_audio = std::make_unique<SceneAudio>(_project, _images.root, g._mix);
	updateCollision();
}
void Scene::updateCollision() {
	if (_borrowed)
		return;
	auto &g = *_game;
	std::copy(_baseCollision.begin(), _baseCollision.end(), g._res._ctData);
	auto block = [&](const Anchor &a, int width, int height) {
		const int room = roomIndex(_project, a.room);
		for (int y = (a.y - height) / 36; y <= (a.y - 1) / 36; ++y)
			for (int x = a.x / 16; x < (a.x + width) / 16; ++x)
				g._res._ctData[256 + room * 112 + y * 16 + x] = 1;
	};
	for (size_t i = 0; i < _project.modules.size(); ++i) {
		const auto &m = _project.modules[i];
		if (m.kind == Kind::Door && !_states[i].open)
			block(m.a, m.width, m.height);
		for (size_t n = 0; n < m.nodes.size(); ++n)
			if (m.nodes[n].kind == NodeKind::Door && !_states[i].graph.nodes[n].active)
				block(m.nodes[n].at, m.nodes[n].width, m.nodes[n].height);
	}
}
bool Scene::canClose(const Anchor &at, int width, int height) const {
	for (int i = 0; i < _game->_res._pgeNum; ++i) {
		if (i > 0 && i < 5)
			continue; // Held gun, projectile, casing and input controller.
		const auto &actor = _game->_pgeLive[i];
		if (actor.life > 0 && actor.room_location == roomIndex(_project, at.room) &&
		    actor.pos_x + 16 > at.x && actor.pos_x < at.x + width &&
		    actor.pos_y + 2 > at.y - height && actor.pos_y - 48 < at.y)
			return false;
	}
	return true;
}
void Scene::activate(size_t index, bool onlyOutputs) {
	if (index >= _states.size())
		return;
	std::vector<size_t> pending{index};
	std::vector<bool> seen(_states.size());
	while (!pending.empty()) {
		const auto i = pending.back();
		pending.pop_back();
		if (seen[i])
			continue;
		seen[i] = true;
		const auto &m = _project.modules[i];
		if (m.kind == Kind::Door) {
			const bool next = i == index && m.toggle ? !_states[i].open : true;
			if (!next && !canClose(m.a, m.width, m.height))
				continue;
			_states[i].open = next;
		}
		if (m.kind == Kind::Graph && (i != index || !onlyOutputs)) {
			GraphMachine machine(m, _states[i].graph, _tick, _values);
			machine.canClose = [&](const Node &n) { return canClose(n.at, n.width, n.height); };
			for (size_t n = 0; n < m.nodes.size(); ++n)
				if (m.nodes[n].kind == NodeKind::Door) {
					machine.send(int(n), Input::Open);
					graphFeedback(machine, i);
				}
		}
		for (const auto &target : m.outputs)
			for (size_t j = 0; j < _project.modules.size(); ++j)
				if (_project.modules[j].id == target)
					pending.push_back(j);
	}
	updateCollision();
}
int Scene::currentRoom() const {
	return _game->_pgeLive[0].room_location;
}
bool Scene::dead() const {
	return _game->_pgeLive[0].life <= 0 || currentRoom() >= int(_project.rooms.size());
}
void Scene::syncAudio(bool seek) {
	if (!_audio)
		return;
	for (size_t i = 0; i < _states.size(); ++i)
		for (size_t j = 0; j < _states[i].graph.nodes.size(); ++j) {
			const auto &s = _states[i].graph.nodes[j];
			_audio->sync(i, j, s.enabled && s.active, s.generation, seek ? _tick - s.changedAt : 0);
		}
}
void Scene::tick() {
	auto &g = *_game;
	const auto &player = g._pgeLive[0];
	const bool use = _stub->_pi.enter;
	if (!_borrowed && _stub->consumePauseRequest()) {
		_paused = true;
		_previousUse = use;
		message("Paused - Enter to resume");
	}
	if (_paused) {
		if (use && !_previousUse) {
			_paused = false;
			message("Resumed");
			g._frameTimestamp = _stub->getTimeStamp();
		}
		_previousUse = use;
		return; // Freeze node timers too; the resume press cannot activate a puzzle.
	}
	const bool hadDialogue = !_dialogues.empty();
	const auto prior = hadDialogue ? _dialogues.front() : DialogueRequest{};
	advanceActions();
	const bool sameConversation =
	    hadDialogue && !_dialogues.empty() && prior.module == _dialogues.front().module &&
	    prior.node == _dialogues.front().node &&
	    prior.generation == _dialogues.front().generation && prior.text == _dialogues.front().text;
	if (hadDialogue || !_dialogue.empty()) {
		const auto *n = dialogueNode();
		const uint8_t direction =
		    _stub->_pi.dirMask & (PlayerInput::DIR_UP | PlayerInput::DIR_DOWN);
		if (sameConversation && n && !n->choices[0].empty() && _page + 1 == _dialogue.size() &&
		    direction && !(_previousDirection & direction))
			_choice = 1 - _choice;
		if (sameConversation && !_dialogue.empty() && use && !_previousUse &&
		    ++_page >= _dialogue.size())
			finishDialogue();
		_previousDirection = direction;
		_previousUse = use;
		finishTick();
		return; // consuming a dialogue key must never activate a nearby button
	}
	_previousDirection = _stub->_pi.dirMask;
	if (!dead()) {
		const bool graphUsed = graphTick(use && !_previousUse);
		if (!_dialogue.empty()) {
			_previousUse = use;
			finishTick();
			return;
		}
		for (size_t i = 0; i < _project.modules.size(); ++i) {
			const auto &m = _project.modules[i];
			const auto &anchor = m.kind == Kind::Door ? m.b : m.a;
			if (roomIndex(_project, anchor.room) != currentRoom() ||
			    std::abs(player.pos_x + 8 - anchor.x) > 24 ||
			    std::abs(player.pos_y + 2 - anchor.y) > 24)
				continue;
			if (m.kind == Kind::Door && !graphUsed && use && !_previousUse) {
				activate(i);
				break;
			}
			if (m.kind == Kind::Medkit && !_states[i].consumed && g._pgeLive[0].life < 5) {
				g._pgeLive[0].life = std::min(5, g._pgeLive[0].life + m.health);
				_states[i].consumed = true;
				activate(i);
			}
		}
		// Only the original actor simulation runs here. Original menus, chapter
		// triggers, autosaves and cutscenes do not belong to custom scenes.
		if (!_borrowed) {
			g.pge_getInput();
			g._weaponFx.tick();
			g.simulateActors();
			g._cut._id = 0xffff;
			g._loadMap = false;
			g._currentRoom = g._pgeLive[0].room_location;
		}
	}
	_previousUse = use;
	finishTick();
}
void Scene::finishTick() {
	++_tick;
	syncAudio();
	// Campaigns persist their checkpoint through CampaignRun. Standalone editor
	// tests need the same puzzle/inventory snapshot rather than a position-only
	// restart that can strand the player behind a freshly reset lock.
	if (_checkpointDue && !_borrowed && _values == &_localValues && canSave()) {
		_checkpointSnapshot = saveSession();
		_checkpointDue = false;
	}
}
Image Scene::render() {
	if (currentRoom() < 0 || currentRoom() >= int(_project.rooms.size()))
		return {256, 224, std::vector<uint32_t>(256 * 224, rgba(10, 16, 24))};
	auto &g = *_game;
	const auto &room = _project.rooms[currentRoom()];
	Image image{256, 224, std::vector<uint32_t>(256 * 224, rgba(12, 20, 28))};
	for (int layer = 0; layer < 3; ++layer) {
		const auto &art = room.art[layer];
		if (!art.file.empty())
			blit(image, _images.get(art.file), art, 0, 0, 256, 224);
		for (size_t i = 0; i < _project.modules.size(); ++i) {
			const auto &m = _project.modules[i];
			for (size_t n = 0; n < m.nodes.size(); ++n) {
				const auto &node = m.nodes[n];
				const auto &state = _states[i].graph.nodes[n];
				if (int(node.layer) == layer && node.at.room == room.id &&
				    (state.enabled || node.kind == NodeKind::Door) &&
				    !((node.kind == NodeKind::Item || node.kind == NodeKind::HealthPickup) &&
				      state.completed) &&
				    (node.kind != NodeKind::PickupAnimation || state.active))
					drawNode(image, node, state.active || state.completed, _tick - state.changedAt,
					         _images);
			}
			if (int(m.layer) != layer || m.kind == Kind::Crawler || _states[i].consumed)
				continue;
			if (m.a.room == room.id)
				drawModule(image, m, 0, _states[i].open, _tick, _images);
			if (m.kind == Kind::Door && m.b.room == room.id)
				drawModule(image, m, 1, _states[i].open, _tick, _images);
		}
		if (layer == 1) {
			std::fill_n(g._vid._frontLayer, g._vid._layerSize, 0);
			std::fill(_coverage.begin(), _coverage.end(), 0);
			g._vid._spriteCoverage = _coverage.data();
			g.prepareAnims();
			g.drawAnims();
			g._vid._spriteCoverage = nullptr;
			std::array<uint8_t, 768> palette{};
			_stub->getPalette(palette.data(), 256);
			for (size_t i = 0; i < _coverage.size(); ++i)
				if (_coverage[i]) {
					const auto *color = &palette[g._vid._frontLayer[i] * 3];
					image.pixels[i] = rgba(color[0], color[1], color[2]);
				}
		}
	}
	drawMessages(image);
	return image;
}
// Word-wrap before pagination: all dialogue remains readable at native resolution.
void Scene::showDialogue(const std::string &text) {
	if (text.empty())
		return;
	std::vector<std::string> lines;
	std::string line, word;
	auto flushWord = [&] {
		if (word.empty())
			return;
		if (!line.empty() && line.size() + word.size() + 1 > 30) {
			lines.push_back(line);
			line.clear();
		}
		if (!line.empty())
			line += ' ';
		line += word;
		word.clear();
		while (line.size() > 30) {
			lines.push_back(line.substr(0, 30));
			line.erase(0, 30);
		}
	};
	for (unsigned char c : text) {
		if (c == ' ' || c == '\n' || c == '\t') {
			flushWord();
			if (c == '\n') {
				lines.push_back(line);
				line.clear();
			}
		} else
			word += c >= 32 && c < 127 ? char(c) : '?';
	}
	flushWord();
	if (!line.empty())
		lines.push_back(line);
	if (lines.empty())
		lines.emplace_back(); // whitespace-only text still has a dismissible page
	_dialogue.clear();
	_page = 0;
	for (size_t i = 0; i < lines.size(); ++i) {
		if (i % (dialogueNode() && !dialogueNode()->choices[0].empty() ? 3 : 5) == 0)
			_dialogue.emplace_back();
		_dialogue.back() += lines[i] + '\n';
	}
}
void Scene::graphFeedback(GraphMachine &machine, size_t module) {
	if (_borrowed)
		for (int index : machine.actorCommands) {
			const auto &n = _project.modules[module].nodes[index];
			if (n.nativeActor > 0 && n.nativeActor < _game->_res._pgeNum && n.messageFrom >= 0 &&
			    n.messageFrom < _game->_res._pgeNum)
				_game->pge_sendMessage(n.messageFrom, n.nativeActor, n.message);
		}
	machine.actorCommands.clear();
	for (int index : machine.worldCommands) {
		const auto &n = _project.modules[module].nodes[index];
		auto &player = _game->_pgeLive[0];
		switch (n.kind) {
		case NodeKind::HealthPickup:
			if (player.life > 0 && player.life < 5)
				player.life = std::min(5, player.life + n.health);
			break;
		case NodeKind::Damage:
			player.life = std::max(0, player.life - n.health);
			break;
		case NodeKind::Checkpoint:
			if (dead())
				break; // A delayed signal can arrive after leaving the room graph.
			// Animation steps can briefly straddle a room boundary. The native
			// snapshot preserves that exact pose; keep its fallback anchor valid.
			_checkpoint =
			    Anchor{_project.rooms[currentRoom()].id, std::clamp<int>(player.pos_x, 0, 240),
				       std::clamp<int>(player.pos_y + 2, 0, 216)};
			_checkpointDue = true;
			_notice = "Checkpoint";
			_noticeUntil = _tick + 90;
			break;
		case NodeKind::Teleport: {
			const int old = player.room_location;
			player.room_location = roomIndex(_project, n.at.room);
			player.pos_x = n.at.x;
			player.pos_y = n.at.y - 2;
			_game->pge_addToCurrentRoomList(&player, old);
			_game->_currentRoom = player.room_location;
			_game->_loadMap = true;
			break;
		}
		case NodeKind::LevelExit:
			if (_exitRoute.empty())
				_exitRoute = n.text;
			break;
		default:
			break;
		}
	}
	machine.worldCommands.clear();
	syncDialogue(); // discard stale generations before reserving queue space
	if (!machine.notice.empty()) {
		_notice = machine.notice;
		_noticeUntil = _tick + 120;
		machine.notice.clear();
	}
	if (!machine.message.empty()) {
		// Reserve half the bounded queue for action nodes (at most 256 per project).
		if (_dialogues.size() < 256)
			_dialogues.push_back({module, -1, 0, machine.message});
		machine.message.clear();
	}
	for (const auto &[node, generation] : machine.dialogues) {
		const auto &s = _states[module].graph.nodes[node];
		if (s.active && s.generation == generation && _dialogues.size() < 512)
			_dialogues.push_back({module, node, generation, {}});
	}
	machine.dialogues.clear();
	syncDialogue();
	if (machine.changedDoor) {
		machine.changedDoor = false;
		activate(module, true); // retain legacy chains on asynchronously opened doors
	}
}
const Node *Scene::dialogueNode() const {
	if (_dialogues.empty() || _dialogues.front().node < 0)
		return nullptr;
	const auto &r = _dialogues.front();
	return &_project.modules[r.module].nodes[r.node];
}
void Scene::syncDialogue() {
	// Stopped/restarted nodes cannot complete an older, queued conversation.
	for (auto it = _dialogues.begin(); it != _dialogues.end();) {
		bool valid = true;
		if (it->node >= 0) {
			const auto &s = _states[it->module].graph.nodes[it->node];
			valid = s.enabled && s.active && s.generation == it->generation;
		}
		if (valid) {
			++it;
			continue;
		}
		if (it == _dialogues.begin())
			_dialogue.clear();
		it = _dialogues.erase(it);
	}
	if (!_dialogues.empty() && _dialogue.empty()) {
		const auto *n = dialogueNode();
		const auto text =
		    n ? (n->speaker.empty() ? "" : n->speaker + ":\n") + n->text : _dialogues.front().text;
		_choice = 0;
		showDialogue(text);
	}
}
void Scene::finishDialogue() {
	if (_dialogues.empty())
		return;
	const auto request = _dialogues.front();
	_dialogues.pop_front();
	_dialogue.clear();
	_page = 0;
	if (request.node >= 0) {
		GraphMachine machine(_project.modules[request.module], _states[request.module].graph, _tick,
		                     _values);
		machine.canClose = [&](const Node &n) { return canClose(n.at, n.width, n.height); };
		machine.finishDialogue(request.node, request.generation, _choice);
		graphFeedback(machine, request.module);
	}
	syncDialogue();
	updateCollision();
}
void Scene::advanceActions() {
	for (size_t i = 0; i < _project.modules.size(); ++i) {
		GraphMachine machine(_project.modules[i], _states[i].graph, _tick, _values);
		machine.canClose = [&](const Node &n) { return canClose(n.at, n.width, n.height); };
		machine.advance();
		graphFeedback(machine, i);
	}
	updateCollision();
}
bool Scene::graphTick(bool use) {
	const auto &player = _game->_pgeLive[0];
	bool used = false;
	for (size_t i = 0; i < _project.modules.size(); ++i) {
		const auto &m = _project.modules[i];
		GraphMachine machine(m, _states[i].graph, _tick, _values);
		machine.canClose = [&](const Node &n) { return canClose(n.at, n.width, n.height); };
		for (size_t j = 0; j < m.nodes.size(); ++j) {
			const auto &n = m.nodes[j];
			auto &state = _states[i].graph.nodes[j];
			const int actor = _states[i].actors[j];
			const auto at =
			    actor >= 0 ? Anchor{_project
				                        .rooms[std::min<int>(_game->_pgeLive[actor].room_location,
				                                             _project.rooms.size() - 1)]
				                        .id,
				                    _game->_pgeLive[actor].pos_x, _game->_pgeLive[actor].pos_y + 2}
				           : n.at;
			const bool sameRoom = actor >= 0 ? _game->_pgeLive[actor].room_location == currentRoom()
			                                 : roomIndex(_project, n.at.room) == currentRoom();
			const bool near = sameRoom && std::abs(player.pos_x + 8 - (at.x + n.width / 2)) <= 24 &&
			                  std::abs(player.pos_y + 2 - at.y) <= 24;
			if (n.kind == NodeKind::Zone) {
				const bool inside =
				    sameRoom && player.pos_x + 16 > n.at.x && player.pos_x < n.at.x + n.width &&
				    player.pos_y + 2 > n.at.y - n.height && player.pos_y - 48 < n.at.y;
				if (inside && !state.inside)
					machine.enter(int(j));
				state.inside = inside;
			} else if ((n.kind == NodeKind::Item ||
			            (n.kind == NodeKind::HealthPickup && player.life < 5)) &&
			           near && !state.completed)
				machine.send(int(j), Input::Activate);
			else if (near && use && !used && state.enabled &&
			         (n.kind == NodeKind::Npc ||
			          (n.kind == NodeKind::Actor && state.friendly &&
			           (_game->_pgeLive[actor].init_PGE->object_type != 10 ||
			            _game->_pgeLive[actor].life > 0)) ||
			          n.kind == NodeKind::Computer || n.kind == NodeKind::Button)) {
				machine.use(int(j));
				used = true;
			}
			graphFeedback(machine, i);
			if (!_dialogue.empty())
				break;
		}
		if (!_dialogue.empty())
			break;
	}
	updateCollision();
	return used;
}
void Scene::drawMessages(Image &image) {
	std::string text;
	int y = 8, height = 16;
	const bool notification = _paused || _dialogue.empty();
	const unsigned opacity = notification && _tick < _noticeUntil
	                             ? unsigned(std::min<uint64_t>(30, _noticeUntil - _tick) * 255 / 30)
	                             : 255;
	if (_paused) {
		text = "Paused - Enter to resume";
	} else if (!_dialogue.empty()) {
		text = _dialogue[_page];
		const auto *n = dialogueNode();
		if (n && !n->choices[0].empty() && _page + 1 == _dialogue.size()) {
			while (std::count(text.begin(), text.end(), '\n') < 3)
				text += '\n';
			for (int i = 0; i < 2; ++i)
				text += (i == _choice ? "> " : "  ") + n->choices[i] + "\n";
		}
		y = 156;
		height = 68;
	} else if (_tick < _noticeUntil)
		text = _notice.substr(0, 30);
	else
		return;
	const int panelWidth = notification ? std::min(252, int(text.size()) * 8 + 12) : 252;
	rectangle(image, 2, y - 4, panelWidth, height, rgba(8, 20, 29, 235 * opacity / 255));
	rectangle(image, 2, y - 4, panelWidth, 1, rgba(87, 220, 202, opacity));
	auto &vid = _game->_vid;
	std::fill_n(vid._frontLayer, vid._layerSize, 0);
	int row = y;
	size_t start = 0;
	while (start < text.size() && row < 208) {
		auto end = text.find('\n', start);
		if (end == std::string::npos)
			end = text.size();
		std::string line = text.substr(start, std::min(size_t(30), end - start));
		for (auto &c : line)
			if (uint8_t(c) < 32 || uint8_t(c) >= 127)
				c = '?';
		vid.drawStringLen(line.c_str(), int(line.size()), 8, row, 15);
		row += 10;
		start = end + 1;
	}
	if (!_dialogue.empty()) {
		const auto prompt =
		    "Enter: " + std::to_string(_page + 1) + "/" + std::to_string(_dialogue.size()) +
		    (dialogueNode() && !dialogueNode()->choices[0].empty() && _page + 1 == _dialogue.size()
		         ? "  Up/Down: choice"
		         : "  Continue");
		vid.drawStringLen(prompt.c_str(), int(prompt.size()), 8, 212, 15);
	}
	// DOS glyphs include palette-indexed shadow pixels. Treating every nonzero
	// index as foreground thickens the letters until they become illegible.
	// Sample native-resolution fonts too when overlaying a Macintosh campaign.
	for (int yy = 0; yy < image.height; ++yy)
		for (int xx = 0; xx < image.width; ++xx) {
			const auto ink =
			    vid._frontLayer[(yy * vid._h / image.height) * vid._w + xx * vid._w / image.width];
			if (ink)
				over(image.pixels[yy * image.width + xx],
				     ink == 15 ? rgba(222, 247, 238, opacity) : rgba(6, 14, 22, opacity));
		}
}
int playProject(const std::filesystem::path &path, const assets::Source &source,
                const std::filesystem::path &control, std::string *exitRoute,
                CampaignRun *campaign) {
	try {
		if (loadProject(path).native.enabled)
			return playNativeProject(path, source, control, exitRoute, campaign);
		if (!control.empty())
			throw std::runtime_error("--editor-session requires a native campaign project");
	} catch (const std::exception &e) {
		std::fprintf(stderr, "Project: %s\n", e.what());
		return 1;
	}
	std::unique_ptr<SystemStub> stub(SystemStub_SDL_create());
	try {
		const auto project = loadProject(path);
		settings::Preferences p;
		p.fullscreen = false;
		p.scale = 3;
		p.filter = settings::Filter::Point;
		p.widescreen = kWidescreenNone;
		stub->configurePreferences(p);
		auto scaler = ScalerParameters::defaults();
		std::snprintf(scaler.name, sizeof(scaler.name), "point");
		scaler.factor = 3;
		stub->init("REminiscence2 - custom scene (Esc exits, Enter restarts after death)", 256, 224,
		           false, kWidescreenNone, &scaler);
		{
			Scene scene(project, path.parent_path(), source, stub.get());
			if (campaign)
				campaign->enter(scene);
			std::vector<uint8_t> rgb(256 * 224 * 3);
			scene.game()._frameTimestamp = stub->getTimeStamp();
			bool previousRestart = false;
			while (!stub->_pi.quit && !stub->_pi.escape) {
				stub->setGameplayActive(!scene.paused());
				stub->processEvents();
				if (campaign)
					campaign->poll(scene);
				if (stub->_pi.quit || stub->_pi.escape)
					break;
				const bool restart = stub->_pi.enter;
				if (scene.dead() && !scene.paused() && restart && !previousRestart) {
					stub->_pi.enter = false;
					if (campaign) {
						std::string reason;
						campaign->load(scene, reason);
						scene.message(reason);
					} else
						scene.restart();
				}
				previousRestart = restart;
				scene.tick();
				if (!scene.exitRoute().empty()) {
					if (exitRoute)
						*exitRoute = scene.exitRoute();
					break;
				}
				const auto frame = scene.render();
				for (size_t i = 0; i < frame.pixels.size(); ++i)
					for (int c = 0; c < 3; ++c)
						rgb[i * 3 + c] = uint8_t(frame.pixels[i] >> (c * 8));
				stub->copyRectRgb24(0, 0, 256, 224, rgb.data());
				stub->updateScreen(0);
				// Share native pacing: carry fractional time and scheduler overshoot,
				// and recover from long stalls without a burst of catch-up movement.
				scene.game().updateTiming();
			}
			if (campaign && scene.canSave()) {
				campaign->progress.exit = scene.exitRoute();
				campaign->capture(scene);
				campaign->store();
			}
		}
		stub->destroy();
		return 0;
	} catch (const std::exception &e) {
		std::fprintf(stderr, "Custom scene: %s\n", e.what());
		stub->destroy();
		return 1;
	}
}
} // namespace editor

namespace editor {
void Scene::enforceDoors() {
	if (_borrowed)
		return; // Native campaign actors keep their original collision semantics.
	for (int id = 0; id < _game->_res._pgeNum; ++id) {
		const auto before = _motion[id];
		_motion[id].room = -1;
		auto &actor = _game->_pgeLive[id];
		const int room = actor.room_location;
		if (before.room < 0 || before.room >= int(_project.rooms.size()) ||
		    room >= int(_project.rooms.size()))
			continue;
		int offset = 0;
		if (room != before.room) {
			const auto &exits = _project.rooms[before.room].exits;
			if (roomIndex(_project, exits[2]) == room)
				offset = 256;
			else if (roomIndex(_project, exits[3]) == room)
				offset = -256;
			else
				continue; // Vertical transitions and teleports are not horizontal motion.
		}
		const int end = actor.pos_x + offset;
		int stopped = end;
		auto barrier = [&](const Anchor &at, int width, int height) {
			const int gateRoom = roomIndex(_project, at.room);
			if (gateRoom != before.room && gateRoom != room)
				return;
			const int left = at.x + (gateRoom == before.room ? 0 : offset);
			// Native animation rules own the approach and small overlaps. Only
			// prevent the actor's cell center crossing the gate's middle plane.
			// Snapping a body edge to a pixel boundary breaks stride/ledge alignment.
			const int boundary = left + width / 2 - 8;
			if (!((end > before.x && before.x < boundary && end >= boundary) ||
			      (end < before.x && before.x > boundary && end <= boundary)))
				return;
			const double part = double(boundary - before.x) / (end - before.x);
			const double feet = before.y + 2 + (actor.pos_y - before.y) * part;
			if (feet <= at.y - height || feet - 48 >= at.y)
				return;
			stopped = before.x; // Retain the native pose coordinate, not a new grid phase.
		};
		for (size_t m = 0; m < _project.modules.size(); ++m) {
			const auto &module = _project.modules[m];
			if (module.kind == Kind::Door && !_states[m].open)
				barrier(module.a, module.width, module.height);
			for (size_t n = 0; n < module.nodes.size(); ++n)
				if (module.nodes[n].kind == NodeKind::Door && !_states[m].graph.nodes[n].active)
					barrier(module.nodes[n].at, module.nodes[n].width, module.nodes[n].height);
		}
		if (stopped != end) {
			// Original animation rules sample cells at keyframes and can stride
			// through a narrow authored gate. Sweep its physical bounds each tick,
			// before graph feedback, without altering scripts or teleport commands.
			actor.pos_x = stopped >= 0 && stopped < 256 ? stopped : stopped - offset;
			actor.room_location = stopped >= 0 && stopped < 256 ? before.room : room;
			if (actor.room_location != room)
				_game->pge_addToCurrentRoomList(&actor, room);
		}
	}
}
bool Scene::beforeActor(Game &g, LivePGE &actor) {
	if (!_borrowed && (actor.index == 0 || actor.index >= 5))
		_motion[actor.index] = {actor.pos_x, actor.pos_y, actor.room_location};
	for (size_t i = 0; i < _states.size(); ++i)
		for (size_t j = 0; j < _states[i].actors.size(); ++j) {
			if (_states[i].actors[j] != actor.index)
				continue;
			const auto &s = _states[i].graph.nodes[j];
			const auto &n = _project.modules[i].nodes[j];
			if (!s.enabled || (s.held && (actor.init_PGE->object_type != 10 || actor.life > 0))) {
				g.pge_clearMessages(actor.index);
				return false;
			}
			if (!s.friendly || (actor.init_PGE->object_type == 10 && actor.life <= 0))
				return true;
			// A friendly actor never evaluates attack rules. Keep its native artwork;
			// walking stays on the authored floor and stops at collision/ledge edges.
			if (s.patrol) {
				const int x = actor.pos_x + ((actor.flags & 1) ? -n.speed : n.speed);
				const int row = (actor.pos_y + 2) / 36, column = (x + 8) / 16;
				const bool clear =
				    row > 0 && row < 7 && column >= 0 && column < 16 && actor.room_location < 64 &&
				    g._res._ctData[256 + actor.room_location * 112 + row * 16 + column] != 0 &&
				    g._res._ctData[256 + actor.room_location * 112 + (row - 1) * 16 + column] == 0;
				if (clear)
					actor.pos_x = x;
				else
					actor.flags ^= 1;
			}
			g.pge_setupDefaultAnim(&actor);
			g.pge_clearMessages(actor.index);
			return false;
		}
	return true;
}
void Scene::afterSimulation(Game &g) {
	enforceDoors();
	for (size_t i = 0; i < _states.size(); ++i) {
		const auto &m = _project.modules[i];
		GraphMachine machine(m, _states[i].graph, _tick, _values);
		machine.canClose = [&](const Node &n) { return canClose(n.at, n.width, n.height); };
		for (size_t j = 0; j < _states[i].actors.size(); ++j) {
			const int id = _states[i].actors[j];
			if (id < 0)
				continue;
			auto &actor = g._pgeLive[id];
			const auto &node = m.nodes[j];
			const auto &s = _states[i].graph.nodes[j];
			const int before = _states[i].lastLife[j];
			_states[i].lastLife[j] = actor.life;
			for (auto [eventActor, rule] : _nativeEvents)
				if (eventActor == id && (node.nativeRule == -1 || node.nativeRule == rule))
					machine.signal(j, Signal::StateChanged);
			const bool combat = actor.init_PGE->object_type == 10;
			if (combat)
				_states[i].graph.nodes[j].completed = actor.life <= 0;
			if (combat && actor.life < before)
				machine.signal(j, Signal::Hurt);
			if (combat && actor.life <= 0 && before > 0)
				machine.signal(j, Signal::Died);
			if (s.enabled && s.patrol && (!combat || actor.life > 0)) {
				const int home = roomIndex(_project, node.at.room);
				if (actor.room_location != home) {
					const int previous = actor.room_location;
					actor.room_location = home;
					g.pge_addToCurrentRoomList(&actor, previous);
					actor.pos_x = std::clamp(node.at.x, node.patrolLeft, node.patrolRight);
					actor.pos_y = node.at.y - 2;
					machine.signal(j, Signal::Reached);
				}
				actor.pos_x = std::clamp<int>(actor.pos_x, node.patrolLeft, node.patrolRight);
				if (actor.pos_x <= node.patrolLeft && (actor.flags & 1)) {
					actor.pos_x = node.patrolLeft;
					actor.flags &= ~1;
					machine.signal(j, Signal::Reached);
				} else if (actor.pos_x >= node.patrolRight && !(actor.flags & 1)) {
					actor.pos_x = node.patrolRight;
					actor.flags |= 1;
					machine.signal(j, Signal::Reached);
				}
			}
		}
		graphFeedback(machine, i);
	}
	_nativeEvents.clear();
}
} // namespace editor

namespace editor {
Scene::Scene(Game &g, Project p, const std::filesystem::path &root)
    : _project(std::move(p)), _stub(g._stub), _game(&g), _borrowed(true) {
	_images.root = root;
	validateImages(_project, _images);
	for (const auto &m : _project.modules) {
		ModuleState state;
		state.graph = initialGraphState(m);
		for (const auto &n : m.nodes) {
			const int actor = n.kind == NodeKind::Actor ? n.nativeActor : -1;
			state.actors.push_back(actor);
			state.lastLife.push_back(actor >= 0 ? g._pgeLive[actor].life : 0);
		}
		_states.push_back(std::move(state));
	}
	_audio = std::make_unique<SceneAudio>(_project, root, g._mix);
}
bool Scene::bindsActor(int id) const {
	for (const auto &s : _states)
		if (std::find(s.actors.begin(), s.actors.end(), id) != s.actors.end())
			return true;
	return false;
}
void Scene::nativeRuleFired(int actor, int rule) {
	if (_nativeEvents.size() < 256)
		_nativeEvents.emplace_back(actor, rule);
}
void Scene::renderOverlay(Image &image) {
	for (size_t i = 0; i < _project.modules.size(); ++i)
		for (size_t j = 0; j < _project.modules[i].nodes.size(); ++j) {
			const auto &n = _project.modules[i].nodes[j];
			const auto &s = _states[i].graph.nodes[j];
			if (roomIndex(_project, n.at.room) != currentRoom() ||
			    (!s.enabled && n.kind != NodeKind::Door) ||
			    (n.kind == NodeKind::PickupAnimation && !s.active) ||
			    ((n.kind == NodeKind::Item || n.kind == NodeKind::HealthPickup) && s.completed))
				continue;
			drawNode(image, n, s.active, _tick - s.changedAt, _images, false);
		}
	// Font drawing uses the indexed native scratch layer. Restore it so the
	// presentation compositor never mistakes our overlay for a native HUD edit.
	std::vector<uint8_t> pixels(_game->_vid._frontLayer,
	                            _game->_vid._frontLayer + _game->_vid._layerSize);
	drawMessages(image);
	std::copy(pixels.begin(), pixels.end(), _game->_vid._frontLayer);
}
} // namespace editor
