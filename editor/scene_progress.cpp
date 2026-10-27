// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtime.h"
#include "campaign_progress.h"
#include "game.h"
#include "file.h"
#include "save_catalog.h"
#include "systemstub.h"
#include "third_party/editor/json.hpp"
#include <algorithm>
#include <stdexcept>
namespace editor {
using json = nlohmann::json;
static void require(bool ok) {
	if (!ok)
		throw std::runtime_error("Invalid or incompatible campaign level snapshot");
}
static uint64_t number(const json &v, uint64_t max) {
	require(v.is_number_integer() && v >= 0 && v <= max);
	return v.get<uint64_t>();
}
static int signedNumber(const json &v, int lo, int hi) {
	require(v.is_number_integer() && v >= lo && v <= hi);
	return v.get<int>();
}
void Scene::bindCampaign(CampaignRun *run) {
	_values = run ? &run->progress.values : &_localValues;
}
bool Scene::canSave() const {
	return _game && _game->_pgeLive[0].life > 0 && !_game->_deathCutsceneCounter &&
	       _game->_pgeLive[0].room_location < _project.rooms.size();
}
std::string Scene::saveSession() const {
	if (!canSave())
		throw std::runtime_error("Save after returning to living gameplay");
	auto &g = *_game;
	File file;
	file.openMemoryBuffer(32768);
	g.writeGameState(file);
	std::vector<uint8_t> bytes(file.size());
	file.seek(0);
	require(file.read(bytes.data(), bytes.size()) == bytes.size());
	std::string hex;
	for (auto b : bytes) {
		hex += "0123456789abcdef"[b >> 4];
		hex += "0123456789abcdef"[b & 15];
	}
	json j = {{"schema", 1},
	          {"project", _project.id},
	          {"game", hex},
	          {"tick", _tick},
	          {"page", _page},
	          {"choice", _choice},
	          {"dialogue", _dialogue},
	          {"cutscene", g._cut._id},
	          {"deathScene", g._cut._deathCutsceneId},
	          {"text", g._textToDisplay},
	          {"blinking", g._blinkingConradCounter},
	          {"playTicks", g._playTicks},
	          {"icon", g._currentInventoryIconNum},
	          {"localValues", _localValues},
	          {"modules", json::array()},
	          {"requests", json::array()},
	          {"messages", json::array()}};
	j["checkpoint"] =
	    _checkpoint ? json::array({_checkpoint->room, _checkpoint->x, _checkpoint->y}) : json();
	for (size_t i = 0; i < _states.size(); ++i) {
		const auto &s = _states[i];
		json state = {{"id", _project.modules[i].id},
		              {"revision", std::to_string(moduleRevision(_project.modules[i]))},
		              {"open", s.open},
		              {"consumed", s.consumed},
		              {"started", s.graph.started},
		              {"life", s.lastLife},
		              {"nodes", json::array()}};
		for (const auto &n : s.graph.nodes)
			state["nodes"].push_back({{"enabled", n.enabled},
			                          {"active", n.active},
			                          {"completed", n.completed},
			                          {"collected", n.collected},
			                          {"inside", n.inside},
			                          {"friendly", n.friendly},
			                          {"patrol", n.patrol},
			                          {"held", n.held},
			                          {"count", n.count},
			                          {"progress", n.progress},
			                          {"changedAt", n.changedAt},
			                          {"generation", n.generation}});
		j["modules"].push_back(std::move(state));
	}
	for (const auto &r : _dialogues)
		j["requests"].push_back({r.module, r.node, r.generation, r.text});
	// Native messages are a queued part of the puzzle, omitted by legacy saves.
	// Keep queue order; restore directly rather than sending fresh graph signals.
	for (int target = 0; target < g._res._pgeNum; ++target) {
		int count = 0;
		for (auto *m = g._pge_messagesTable[target]; m; m = m->next_entry) {
			require(++count <= 256 && j["messages"].size() < 256);
			j["messages"].push_back({target, m->src_pge, m->msg_num});
		}
	}
	return j.dump();
}
void Scene::restoreSession(const std::string &text, bool revisit) {
	require(text.size() <= 2 * 1024 * 1024);
	const auto j = json::parse(text, [](int depth, json::parse_event_t, json &) {
		require(depth <= 16);
		return true;
	});
	require(j.at("schema").is_number_integer() && j.at("schema") == 1 &&
	        j.at("project") == _project.id);
	std::map<std::string, bool> localValues;
	// Optional for older snapshots; campaign-owned values remain in the outer
	// save document. Standalone checkpoints need their own shared quest state.
	if (j.contains("localValues")) {
		const auto &values = j.at("localValues");
		require(values.is_object() && values.size() <= 4096);
		for (auto it = values.begin(); it != values.end(); ++it) {
			require(it.key().size() > 5 && validCampaignKey(it.key().substr(5)) &&
			        (it.key().rfind("item:", 0) == 0 || it.key().rfind("flag:", 0) == 0));
			localValues[it.key()] = it.value().get<bool>();
		}
	}
	const auto tick = number(j.at("tick"), UINT32_MAX);
	auto states = _states; // Everything is parsed and checked before live state changes.
	const auto &modules = j.at("modules");
	require(modules.is_array() && modules.size() == states.size());
	for (size_t i = 0; i < states.size(); ++i) {
		auto &s = states[i];
		const auto &v = modules[i];
		require(v.at("id") == _project.modules[i].id &&
		        v.at("revision") == std::to_string(moduleRevision(_project.modules[i])));
		s.open = v.at("open").get<bool>();
		s.consumed = v.at("consumed").get<bool>();
		s.graph.started = v.at("started").get<bool>();
		s.graph.trace.clear();
		s.graph.pending.clear();
		s.graph.delivered.clear();
		s.graph.sequence = 0;
		require(v.at("life").is_array() && v.at("life").size() == s.lastLife.size());
		for (size_t k = 0; k < s.lastLife.size(); ++k)
			s.lastLife[k] = signedNumber(v.at("life")[k], INT16_MIN, INT16_MAX);
		require(v.at("nodes").is_array() && v.at("nodes").size() == s.graph.nodes.size());
		for (size_t k = 0; k < s.graph.nodes.size(); ++k) {
			auto &n = s.graph.nodes[k];
			const auto &a = v.at("nodes")[k];
			n.enabled = a.at("enabled").get<bool>();
			n.active = a.at("active").get<bool>();
			n.completed = a.at("completed").get<bool>();
			n.collected = a.at("collected").get<bool>();
			n.inside = a.at("inside").get<bool>();
			n.friendly = a.at("friendly").get<bool>();
			n.patrol = a.at("patrol").get<bool>();
			n.held = a.at("held").get<bool>();
			n.count = number(a.at("count"), 32);
			n.changedAt = number(a.at("changedAt"), tick);
			n.generation = number(a.at("generation"), UINT32_MAX);
			require(a.at("progress").is_array() && a.at("progress").size() <= s.graph.nodes.size());
			n.progress.clear();
			for (const auto &p : a.at("progress"))
				n.progress.insert(number(p, s.graph.nodes.size() - 1));
			if (revisit) {
				n.inside = false;
				if (_project.modules[i].nodes[k].kind == NodeKind::LevelExit)
					n.completed = n.active = false;
			}
		}
	}
	std::optional<Anchor> checkpoint;
	if (!j.at("checkpoint").is_null()) {
		const auto &a = j.at("checkpoint");
		require(a.is_array() && a.size() == 3);
		checkpoint =
		    Anchor{a[0].get<std::string>(), int(number(a[1], 240)), int(number(a[2], 216))};
		require(roomIndex(_project, checkpoint->room) >= 0);
	}
	const auto &pages = j.at("dialogue");
	require(pages.is_array() && pages.size() <= 256);
	std::vector<std::string> dialogue;
	for (const auto &p : pages) {
		auto line = p.get<std::string>();
		require(line.size() <= 4096);
		dialogue.push_back(std::move(line));
	}
	const auto page = number(j.at("page"), dialogue.empty() ? 0 : dialogue.size() - 1);
	const auto choice = number(j.at("choice"), 1);
	std::deque<DialogueRequest> requests;
	require(j.at("requests").is_array() && j.at("requests").size() <= 512);
	for (const auto &r : j.at("requests")) {
		require(r.is_array() && r.size() == 4 && !states.empty());
		DialogueRequest q;
		q.module = number(r[0], states.size() - 1);
		require(!states[q.module].graph.nodes.empty());
		q.node = signedNumber(r[1], -1, int(states[q.module].graph.nodes.size()) - 1);
		q.generation = number(r[2], UINT32_MAX);
		q.text = r[3].get<std::string>();
		require(q.text.size() <= 4096);
		require(q.node == -1 ||
		        _project.modules[q.module].nodes[q.node].kind == NodeKind::Dialogue);
		requests.push_back(std::move(q));
	}
	// A visible page must belong to a live queue entry. Without this invariant,
	// finishing an orphan page leaves an out-of-range page index in drawMessages.
	require(dialogue.empty() == requests.empty());
	std::set<std::pair<size_t, int>> conversations;
	for (const auto &q : requests) {
		if (q.node < 0) {
			require(!q.text.empty());
		} else {
			const auto &state = states[q.module].graph.nodes[q.node];
			require(state.enabled && state.active && state.generation == q.generation &&
			        conversations.emplace(q.module, q.node).second);
		}
	}
	auto &g = *_game;
	const auto cut = number(j.at("cutscene"), 65535),
	           deathScene = number(j.at("deathScene"), 65535), story = number(j.at("text"), 65535),
	           blinking = number(j.at("blinking"), 255),
	           playTicks = number(j.at("playTicks"), UINT32_MAX);
	const auto icon = number(j.at("icon"), 255);
	const size_t cutCount =
	    g._res.isAmiga() ? Cutscene::_offsetsCountAmiga : Cutscene::_offsetsCountDOS;
	require((cut == 65535 || cut < cutCount) && (deathScene == 65535 || deathScene < cutCount));
	if (story != 65535) {
		const auto *table = g._res.isMac() ? g._res._str : g._res._stringsTable;
		require(table != nullptr);
		const unsigned count = g._res.isMac() ? READ_BE_UINT16(table) : READ_LE_UINT16(table) / 2;
		require(story < count);
	}
	if (g._res.isDOS()) {
		require(g._res._icn && g._res._icnLen >= 2 && icon < READ_LE_UINT16(g._res._icn) / 2);
		require(READ_LE_UINT16(g._res._icn + icon * 2) + 130 <= g._res._icnLen);
	} else if (g._res.isSegaCD())
		require((icon + 1) * 128 <= unsigned(g._res._icnLen));
	else
		require(icon <= 78);
	std::vector<std::array<int, 3>> messages;
	require(j.at("messages").is_array() && j.at("messages").size() <= 256);
	for (const auto &m : j.at("messages")) {
		require(m.is_array() && m.size() == 3);
		messages.push_back({int(number(m[0], g._res._pgeNum - 1)),
		                    int(number(m[1], g._res._pgeNum - 1)), int(number(m[2], 65535))});
	}
	const auto hex = j.at("game").get<std::string>();
	require(hex.size() <= 131072 && hex.size() % 2 == 0);
	std::vector<uint8_t> bytes;
	auto nibble = [](char c) {
		require((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
		return c <= '9' ? c - '0' : c - 'a' + 10;
	};
	for (size_t i = 0; i < hex.size(); i += 2)
		bytes.push_back(nibble(hex[i]) * 16 + nibble(hex[i + 1]));
	File file;
	file.openMemoryBuffer(bytes.size());
	file.write(bytes.data(), bytes.size());
	file.seek(0);
	int version = 0;
	std::string reason;
	if (!saves::validate(file, g._res._pgeNum, version, reason, g._res._type, g._currentLevel,
	                     g._assetFingerprint, &g._res))
		throw std::runtime_error(reason);
	require(bytes.size() > 60 && bytes[58] < _project.rooms.size() &&
	        (bytes[59] * 256 + bytes[60]) > 0 && (bytes[59] * 256 + bytes[60]) < 32768);
	// All fallible parsing/preflight above. Legacy decoder only sees the verified bytes.
	g.loadState(&file, version);
	_states = std::move(states);
	_localValues = std::move(localValues);
	_tick = tick;
	_checkpoint = std::move(checkpoint);
	_dialogue = std::move(dialogue);
	_dialogues = std::move(requests);
	_page = page;
	_choice = choice;
	_exitRoute.clear();
	_checkpointDue = false;
	_notice.clear();
	_noticeUntil = 0;
	_nativeEvents.clear();
	_previousUse = false;
	_paused = false;
	_previousDirection = 0;
	_motion = {};
	g._cut._id = cut;
	g._cut._deathCutsceneId = deathScene;
	g._textToDisplay = story;
	g._currentInventoryIconNum = icon;
	g._blinkingConradCounter = blinking;
	g._playTicks = playTicks;
	// Rebuild allocated/free message chains without invoking gameplay callbacks.
	for (size_t i = 0; i < messages.size(); ++i) {
		auto &m = g._pge_messages[i];
		const auto &a = messages[i];
		m.src_pge = a[1];
		m.msg_num = a[2];
		m.next_entry = nullptr;
		auto **tail = &g._pge_messagesTable[a[0]];
		while (*tail)
			tail = &(*tail)->next_entry;
		*tail = &m;
	}
	g._pge_nextFreeMessage = messages.size() == 256 ? nullptr : &g._pge_messages[messages.size()];
	if (revisit) {
		auto &a = g._pgeLive[0];
		const int old = a.room_location;
		a.room_location = roomIndex(_project, _project.spawn.room);
		a.pos_x = _project.spawn.x;
		a.pos_y = _project.spawn.y;
		a.obj_type = a.init_PGE->type;
		a.anim_seq = 0;
		a.counter_value = 0;
		a.flags = (a.flags & ~1) | _project.spawnLeft;
		const auto *program = g._res._objectNodesMap[a.init_PGE->obj_node_number];
		for (int row = 0; row < program->num_objects; ++row)
			if (program->objects[row].type == a.obj_type) {
				a.first_obj_number = row;
				break;
			}
		g.pge_clearMessages(0);
		g.pge_addToCurrentRoomList(&a, old);
		g.pge_setupDefaultAnim(&a);
		_dialogue.clear();
		_dialogues.clear();
		_page = 0;
		g._cut._id = g._textToDisplay = 0xffff;
	}
	g._currentRoom = g._pgeLive[0].room_location;
	g._loadMap = _borrowed;
	if (!_borrowed)
		updateCollision();
	g._endLoop = false;
	g._inp_lastKeysHit = g._inp_lastKeysHitLeftRight = g._pge_inpKeysMask = 0;
	g._stub->_pi.save = g._stub->_pi.load = false;
	if (_audio) {
		_audio->reset();
		syncAudio(true);
	}
	if (!_borrowed && _values == &_localValues)
		_checkpointSnapshot = _checkpoint ? text : std::string{};
}
} // namespace editor
