// SPDX-License-Identifier: GPL-3.0-or-later
#include "content.h"
#include "image.h"
#include "audio.h"
#include "atomic_file.h"
#include "file.h"
#include "third_party/editor/json.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <zlib.h>
namespace editor {
using json = nlohmann::json;
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Art, file, x, y, w, h)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Ledge, column, row, facing)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Room, id, name, column, row, art, collision, ledges, exits)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Anchor, room, x, y)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Animation, art, frames, fps, loop)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Node, id, name, kind, at, graphX, graphY, width, height, layer,
                                   visuals, enabled, initiallyActive, once, consumeItem,
                                   requiredItem, text, doneText, goal, speaker, choices, duration,
                                   stateGroup, friendly, patrol, patrolLeft, patrolRight, speed,
                                   health, nativeActor, nativeRule, message, messageFrom, comments,
                                   campaignKey, audioFile, audioVolume, audioLoop)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Wire, from, to, signal, input)
void to_json(json &j, const Module &m) {
	j = {{"id", m.id},
	     {"name", m.name},
	     {"comments", m.comments},
	     {"kind", int(m.kind)},
	     {"a", m.a},
	     {"b", m.b},
	     {"initiallyOpen", m.initiallyOpen},
	     {"toggle", m.toggle},
	     {"facingLeft", m.facingLeft},
	     {"health", m.health},
	     {"width", m.width},
	     {"height", m.height},
	     {"sprite", m.sprite},
	     {"frames", m.frames},
	     {"fps", m.fps},
	     {"layer", int(m.layer)},
	     {"outputs", m.outputs},
	     {"assets", m.assets},
	     {"nodes", m.nodes},
	     {"wires", m.wires}};
}
static void stripComments(json &j) {
	if (j.is_object()) {
		j.erase("comments");
		for (auto &v : j)
			stripComments(v);
	} else if (j.is_array())
		for (auto &v : j)
			stripComments(v);
}
uint64_t moduleRevision(const Module &m) {
	uint64_t hash = 14695981039346656037ull;
	json data = m;
	stripComments(data);
	for (unsigned char byte : data.dump()) {
		hash ^= byte;
		hash *= 1099511628211ull;
	}
	return hash;
}
void from_json(const json &j, Module &m) {
	m.id = j.at("id");
	m.name = j.at("name");
	m.comments = j.value("comments", std::string{});
	const int kind = j.at("kind"), layer = j.at("layer");
	if (kind < 0 || kind > 4 || layer < 0 || layer > 2)
		throw std::runtime_error("Unsupported module kind or layer");
	m.kind = Kind(kind);
	m.layer = Layer(layer);
	j.at("a").get_to(m.a);
	j.at("b").get_to(m.b);
	j.at("initiallyOpen").get_to(m.initiallyOpen);
	j.at("toggle").get_to(m.toggle);
	j.at("facingLeft").get_to(m.facingLeft);
	j.at("health").get_to(m.health);
	j.at("width").get_to(m.width);
	j.at("height").get_to(m.height);
	j.at("sprite").get_to(m.sprite);
	j.at("frames").get_to(m.frames);
	j.at("fps").get_to(m.fps);
	j.at("outputs").get_to(m.outputs);
	m.assets = j.value("assets", std::vector<Art>{});
	auto nodes = j.value("nodes", json::array());
	for (auto &n : nodes) {
		if (!n.contains("audioFile"))
			n["audioFile"] = "";
		if (!n.contains("audioVolume"))
			n["audioVolume"] = 70;
		if (!n.contains("audioLoop"))
			n["audioLoop"] = false;
		if (!n.contains("campaignKey"))
			n["campaignKey"] = "";
		if (!n.contains("comments"))
			n["comments"] = "";
		if (!n.contains("speaker"))
			n["speaker"] = "";
		if (!n.contains("choices"))
			n["choices"] = std::array<std::string, 2>{};
		if (!n.contains("duration"))
			n["duration"] = 30;
		if (!n.contains("stateGroup"))
			n["stateGroup"] = "main";
		const Node defaults;
		if (!n.contains("nativeActor"))
			n["nativeActor"] = -1;
		if (!n.contains("nativeRule"))
			n["nativeRule"] = -1;
		if (!n.contains("message"))
			n["message"] = 1;
		if (!n.contains("messageFrom"))
			n["messageFrom"] = 0;
		if (!n.contains("friendly"))
			n["friendly"] = defaults.friendly;
		if (!n.contains("patrol"))
			n["patrol"] = defaults.patrol;
		if (!n.contains("patrolLeft"))
			n["patrolLeft"] = defaults.patrolLeft;
		if (!n.contains("patrolRight"))
			n["patrolRight"] = defaults.patrolRight;
		if (!n.contains("speed"))
			n["speed"] = defaults.speed;
		if (!n.contains("health"))
			n["health"] = defaults.health;
	}
	m.nodes = nodes.get<std::vector<Node>>();
	m.wires = j.value("wires", std::vector<Wire>{});
	for (const auto &n : j.value("nodes", json::array())) {
		if (n.at("visuals").size() != 2)
			throw std::runtime_error("Nodes need two visual states");
		if (n.contains("choices") && n.at("choices").size() != 2)
			throw std::runtime_error("Dialogue needs exactly two choice slots");
	}
}
void to_json(json &j, const NativeActor &a) {
	j = {{"id", a.id},
	     {"room", a.room},
	     {"x", a.x},
	     {"y", a.y},
	     {"life", a.life},
	     {"facingLeft", a.facingLeft},
	     {"type", a.type},
	     {"behavior", a.behavior},
	     {"objectKind", a.objectKind},
	     {"data", a.data},
	     {"source", a.source},
	     {"comments", a.comments},
	     {"removed", a.removed}};
}
void from_json(const json &j, NativeActor &a) {
	j.at("id").get_to(a.id);
	j.at("room").get_to(a.room);
	j.at("x").get_to(a.x);
	j.at("y").get_to(a.y);
	j.at("life").get_to(a.life);
	j.at("facingLeft").get_to(a.facingLeft);
	j.at("type").get_to(a.type);
	j.at("behavior").get_to(a.behavior);
	j.at("objectKind").get_to(a.objectKind);
	j.at("data").get_to(a.data);
	a.source = j.value("source", -1);
	a.comments = j.value("comments", std::string{});
	a.removed = j.value("removed", false);
}
void to_json(json &j, const NativeRule &r) {
	j = {{"type", r.type},       {"nextType", r.nextType},
	     {"next", r.next},       {"dx", r.dx},
	     {"dy", r.dy},           {"flags", r.flags},
	     {"opcode", r.opcode},   {"argument", r.argument},
	     {"enabled", r.enabled}, {"comments", r.comments}};
}
void from_json(const json &j, NativeRule &r) {
	j.at("type").get_to(r.type);
	j.at("nextType").get_to(r.nextType);
	j.at("next").get_to(r.next);
	j.at("dx").get_to(r.dx);
	j.at("dy").get_to(r.dy);
	j.at("flags").get_to(r.flags);
	j.at("opcode").get_to(r.opcode);
	j.at("argument").get_to(r.argument);
	j.at("enabled").get_to(r.enabled);
	r.comments = j.value("comments", std::string{});
}
void to_json(json &j, const NativeProgram &p) {
	j = {{"rules", p.rules}, {"source", p.source}};
}
void from_json(const json &j, NativeProgram &p) {
	j.at("rules").get_to(p.rules);
	p.source = j.value("source", -1);
}
void to_json(json &j, const NativePuzzle &p) {
	j = {{"name", p.name}, {"actors", p.actors}, {"comments", p.comments}};
}
void from_json(const json &j, NativePuzzle &p) {
	j.at("name").get_to(p.name);
	j.at("actors").get_to(p.actors);
	p.comments = j.value("comments", std::string{});
}
void to_json(json &j, const NativeCampaign &n) {
	j = {{"enabled", n.enabled},         {"level", n.level},   {"edition", n.edition},
	     {"fingerprint", n.fingerprint}, {"actors", n.actors}, {"puzzles", n.puzzles}};
	// Many native behavior slots alias an identical table. Store backward
	// references rather than multiplying document/history size by those aliases.
	j["programs"] = json::array();
	std::map<std::string, int> known;
	for (const auto &program : n.programs) {
		json table = program;
		const auto key = table.dump();
		auto found = known.find(key);
		if (found != known.end())
			j["programs"].push_back(found->second);
		else {
			known[key] = int(j["programs"].size());
			j["programs"].push_back(std::move(table));
		}
	}
}
uint64_t nativeRevision(const NativeCampaign &n) {
	uint64_t hash = 14695981039346656037ULL;
	auto clean = n;
	for (auto &a : clean.actors)
		a.comments.clear();
	for (auto &p : clean.programs)
		for (auto &r : p.rules)
			r.comments.clear();
	for (auto &p : clean.puzzles)
		p.comments.clear();
	json data = clean;
	stripComments(data);
	for (unsigned char c : data.dump()) {
		hash ^= c;
		hash *= 1099511628211ULL;
	}
	return hash;
}
void from_json(const json &j, NativeCampaign &n) {
	j.at("enabled").get_to(n.enabled);
	j.at("level").get_to(n.level);
	j.at("edition").get_to(n.edition);
	j.at("fingerprint").get_to(n.fingerprint);
	j.at("actors").get_to(n.actors);
	n.programs.clear();
	size_t total = 0;
	for (const auto &table : j.value("programs", json::array())) {
		if (n.programs.size() >= 255)
			throw std::runtime_error("Too many native behavior slots");
		if (table.is_number_integer()) {
			const int index = table.get<int>();
			if (index < 0 || index >= int(n.programs.size()))
				throw std::runtime_error("Invalid native behavior alias");
			n.programs.push_back(n.programs[index]);
		} else {
			for (const auto &rule : table.at("rules"))
				if (rule.at("opcode").size() != 3 || rule.at("argument").size() != 3)
					throw std::runtime_error("Native rules need three opcode/operand pairs");
			n.programs.push_back(table.get<NativeProgram>());
		}
		total += n.programs.back().rules.size();
		if (total > 24000)
			throw std::runtime_error("Native rule expansion limit exceeded");
	}
	n.puzzles = j.value("puzzles", std::vector<NativePuzzle>{});
}
void to_json(json &j, const Project &p) {
	j = {{"schema", p.schema}, {"id", p.id},
	     {"name", p.name},     {"serial", p.serial},
	     {"rooms", p.rooms},   {"modules", p.modules},
	     {"spawn", p.spawn},   {"spawnLeft", p.spawnLeft},
	     {"native", p.native}};
}
void from_json(const json &j, Project &p) {
	p.schema = j.at("schema");
	if (p.schema >= 1 && p.schema <= 8)
		p.schema = 9; // Additive migration; native projects require a schema-aware player.
	j.at("id").get_to(p.id);
	j.at("name").get_to(p.name);
	j.at("serial").get_to(p.serial);
	j.at("rooms").get_to(p.rooms);
	j.at("modules").get_to(p.modules);
	j.at("spawn").get_to(p.spawn);
	j.at("spawnLeft").get_to(p.spawnLeft);
	p.native = j.value("native", NativeCampaign{});
	if (j.contains("native"))
		for (const auto &a : j.at("native").at("actors"))
			if (a.at("data").size() != 4)
				throw std::runtime_error("Native actor metadata needs four operands");
}
const char *kindName(Kind k) {
	switch (k) {
	case Kind::Graph:
		return "Node module";
	case Kind::Door:
		return "Door + activator";
	case Kind::Prop:
		return "Animated prop";
	case Kind::Medkit:
		return "Health pickup";
	case Kind::Crawler:
		return "Alien crawler";
	}
	return "Unknown";
}
const char *layerName(Layer l) {
	return l == Layer::Back ? "Back" : l == Layer::Front ? "Front" : "Playfield";
}
static bool safeId(const std::string &id) {
	return !id.empty() && id.size() <= 80 && std::all_of(id.begin(), id.end(), [](unsigned char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
		       c == '-' || c == '_';
	});
}
bool safeAssetName(const std::string &name) {
	if (name.empty())
		return true;
	if (name.size() > 512 || name.find('\\') != std::string::npos ||
	    name.find(':') != std::string::npos || name.find('\0') != std::string::npos)
		return false;
	const auto p = std::filesystem::u8path(name);
	if (p.is_absolute())
		return false;
	for (const auto &part : p)
		if (part == ".." || part == ".")
			return false;
	return true;
}
std::string newId(Project &p, const char *prefix) {
	for (;;) {
		if (p.serial >= INT32_MAX)
			throw std::runtime_error("Project identifier range exhausted");
		auto id = std::string(prefix) + "-" + std::to_string(p.serial++);
		bool used = false;
		for (const auto &r : p.rooms)
			used |= r.id == id;
		for (const auto &m : p.modules)
			used |= m.id == id;
		if (!used)
			return id;
	}
}
Project makeProject() {
	Project p;
	std::random_device rd;
	std::ostringstream id;
	id << "campaign-" << std::hex << rd() << rd() << rd() << rd();
	p.id = id.str();
	auto &r = addRoom(p, 0, 0);
	p.spawn = {r.id, 32, 214};
	return p;
}
Room &addRoom(Project &p, int column, int row) {
	if (p.rooms.size() >= MaxRooms)
		throw std::runtime_error("Maximum 64 rooms per project");
	Room room;
	room.id = newId(p, "room");
	room.name = "Room " + std::to_string(p.rooms.size() + 1);
	room.column = column;
	room.row = row;
	std::fill(room.collision.begin() + 6 * 16, room.collision.end(), 1);
	p.rooms.push_back(room);
	return p.rooms.back();
}
int roomIndex(const Project &p, const std::string &id) {
	for (size_t i = 0; i < p.rooms.size(); ++i)
		if (p.rooms[i].id == id)
			return int(i);
	return -1;
}
Module makeModule(Project &p, Kind kind, const Anchor &at) {
	if (p.modules.size() >= MaxModules)
		throw std::runtime_error("Maximum 64 modules per project");
	Module m;
	m.id = newId(p, "module");
	m.kind = kind;
	m.name = kindName(kind);
	m.a = at;
	m.b = at;
	m.a.x = std::clamp((at.x / 16) * 16, 0, 240);
	m.a.y = std::clamp(((at.y + 36) / 72) * 72, 72, 216);
	m.b.x = std::max(8, m.a.x - 48);
	m.b.y = m.a.y;
	if (kind != Kind::Door) {
		m.width = 16;
		m.height = 16;
	}
	return m;
}
Module duplicateModule(Project &p, const Module &source) {
	if (p.modules.size() >= MaxModules)
		throw std::runtime_error("Maximum 64 modules");
	Module m = source;
	m.id = newId(p, "module");
	m.name += " copy";
	m.name.resize(std::min(size_t(160), m.name.size()));
	int right = std::min(256 - m.width - m.a.x, m.kind == Kind::Door ? 248 - m.b.x : 248);
	int left = std::min(m.a.x, m.kind == Kind::Door ? m.b.x : 248);
	for (const auto &n : m.nodes) {
		right = std::min(right, 256 - n.width - n.at.x);
		left = std::min(left, n.at.x);
	}
	const int dx = right >= 16 ? 16 : left >= 16 ? -16 : 0;
	m.a.x += dx;
	m.b.x += dx;
	for (auto &n : m.nodes)
		n.at.x += dx;
	// External connections are deliberately explicit; a fresh bundle does not
	// inherit links into another puzzle. Its owned activator remains connected.
	m.outputs.clear();
	return m;
}
void connectRooms(Project &p, int index, int direction, int target, bool reciprocal) {
	if (index < 0 || index >= int(p.rooms.size()) || direction < 0 || direction > 3 ||
	    target >= int(p.rooms.size()))
		throw std::runtime_error("Invalid room connection");
	const int reverse[] = {1, 0, 3, 2};
	auto &room = p.rooms[index];
	const int old = roomIndex(p, room.exits[direction]);
	if (reciprocal && old >= 0 && p.rooms[old].exits[reverse[direction]] == room.id)
		p.rooms[old].exits[reverse[direction]].clear();
	if (target < 0)
		room.exits[direction].clear();
	else {
		room.exits[direction] = p.rooms[target].id;
		if (reciprocal) {
			auto &back = p.rooms[target].exits[reverse[direction]];
			const int displaced = roomIndex(p, back);
			if (displaced >= 0 && p.rooms[displaced].exits[direction] == p.rooms[target].id)
				p.rooms[displaced].exits[direction].clear();
			back = room.id;
		}
	}
}
std::vector<Issue> validate(const Project &p) {
	std::vector<Issue> issues;
	std::string whereRoom, whereModule;
	auto error = [&](const std::string &s) { issues.push_back({true, s, whereRoom, whereModule}); };
	if (p.schema != 9)
		error("Unsupported project schema (expected 9)");
	if (!safeId(p.id))
		error("Invalid campaign ID");
	if (p.name.size() > 160)
		error("Project name is too long");
	if (p.serial == 0 || p.serial > INT32_MAX)
		error("Project identifier range exhausted");
	if (p.rooms.empty() || p.rooms.size() > MaxRooms)
		error("A project needs 1 to 64 rooms");
	if (p.modules.size() > MaxModules)
		error("Maximum 64 modules");
	size_t nodeCount = 0;
	std::set<std::string> ids;
	std::set<std::pair<int, int>> positions;
	auto art = [&](const Art &a) {
		if (!safeAssetName(a.file) || a.x < 0 || a.y < 0 || a.x > 16384 || a.y > 16384 || a.w < 1 ||
		    a.h < 1 || a.w > 16384 || a.h > 16384)
			error("Invalid image path or crop");
	};
	auto anchor = [&](const Anchor &a) {
		return roomIndex(p, a.room) >= 0 && a.x >= 0 && a.x <= 248 && a.y >= 0 && a.y <= 216;
	};
	if (!anchor(p.spawn) || p.spawn.y >= 216)
		error("Player start is outside a room");
	for (const auto &r : p.rooms) {
		whereRoom = r.id;
		if (!positions.emplace(r.column, r.row).second)
			error("Rooms overlap in the world grid");
		if (!safeId(r.id) || !ids.insert(r.id).second)
			error("Duplicate or invalid room ID");
		if (r.name.size() > 160 || std::abs(int64_t(r.column)) > 128 ||
		    std::abs(int64_t(r.row)) > 128)
			error("Invalid room name or world position");
		for (const auto &a : r.art)
			art(a);
		for (auto c : r.collision)
			if (c > 2) {
				error(r.name + ": unsupported collision value");
				break;
			}
		if (r.ledges.size() > 64)
			error(r.name + ": too many ledges");
		for (const auto &l : r.ledges)
			if (l.column < 1 || l.column > 14 || (l.row != 2 && l.row != 4 && l.row != 6) ||
			    (l.facing != 1 && l.facing != -1))
				error(r.name + ": ledge must use a supported floor row and have side clearance");
		for (const auto &exit : r.exits)
			if (!exit.empty() && roomIndex(p, exit) < 0)
				error(r.name + ": exit refers to a missing room");
	}
	whereRoom.clear();
	for (const auto &m : p.modules) {
		whereModule = m.id;
		whereRoom = m.a.room;
		nodeCount += m.nodes.size();
		validateGraph(p, m, issues);
		if (!safeId(m.id) || !ids.insert(m.id).second)
			error("Duplicate or invalid module ID");
		if (int(m.kind) < 0 || int(m.kind) > 4 || int(m.layer) < 0 || int(m.layer) > 2)
			error("Unsupported module type/layer");
		if (m.comments.size() > 2047 || m.name.size() > 160 || !anchor(m.a) ||
		    (m.kind == Kind::Door && !anchor(m.b)))
			error(m.name + ": invalid part position");
		if (m.width < 1 || m.width > 256 || m.height < 1 || m.height > 216 || m.frames < 1 ||
		    m.frames > 128 || m.fps < 1 || m.fps > 30 || m.health < 1 || m.health > 100)
			error(m.name + ": parameter outside supported range");
		if (m.kind == Kind::Door && (m.a.x % 16 || m.a.y % 72 || m.a.y < 72 || m.width % 16 ||
		                             m.height % 72 || m.height > m.a.y || m.a.x + m.width > 256))
			error(m.name + ": door needs 16-pixel X/width and 72-pixel floor/height alignment");
		art(m.sprite);
		if (m.outputs.size() > 64)
			error("Too many module output connections");
		for (const auto &id : m.outputs) {
			auto found = std::find_if(p.modules.begin(), p.modules.end(), [&](const auto &n) {
				return n.id == id &&
				       (n.kind == Kind::Door ||
				        (n.kind == Kind::Graph &&
				         std::any_of(n.nodes.begin(), n.nodes.end(), [](const auto &node) {
					         return node.kind == NodeKind::Door;
				         })));
			});
			if (found == p.modules.end())
				error(m.name + ": output must target an existing door module");
		}
	}
	whereModule.clear();
	whereRoom.clear();
	if (p.native.enabled) {
		if (p.native.level < 0 || p.native.level > 6 || p.native.edition < 0 ||
		    p.native.fingerprint.size() != 8 ||
		    p.native.fingerprint.find_first_not_of("0123456789abcdef") != std::string::npos)
			error("Invalid native edition, level or asset fingerprint");
		if (p.rooms.size() != 64)
			error("Native projects retain 64 room slots");
		std::set<int> bindings;
		for (const auto &m : p.modules) {
			if (m.kind != Kind::Graph)
				error("Native extensions use node modules");
			for (const auto &n : m.nodes) {
				if (n.kind == NodeKind::Door)
					error("Control a native door with an Actor binding and its native message; "
					      "custom collision doors belong to custom scenes");
				if (n.kind == NodeKind::Actor &&
				    (n.nativeActor <= 0 || n.nativeActor >= int(p.native.actors.size()) ||
				     !bindings.insert(n.nativeActor).second || n.messageFrom < 0 ||
				     n.messageFrom >= int(p.native.actors.size())))
					error("Each native actor binding needs a unique non-player actor and valid "
					      "message sender");
			}
		}
		for (size_t i = 0; i < p.rooms.size(); ++i)
			if (p.rooms[i].id != "original-room-" + std::to_string(i))
				error("Native room slot order/IDs cannot change");
		if (p.native.actors.empty() || p.native.actors.size() > 256)
			error("Invalid native actor count");
		for (size_t i = 0; i < p.native.actors.size(); ++i) {
			const auto &a = p.native.actors[i];
			for (int v : a.data)
				if (v < -32768 || v > 32767)
					error("Native data operands must be signed 16-bit values");
			if (a.comments.size() > 2047 || a.id != int(i) || a.source < -1 || a.source > 255 ||
			    a.behavior < 0 ||
			    (!p.native.programs.empty() && a.behavior >= int(p.native.programs.size())) ||
			    a.type < 0 || a.type > 65535 || a.objectKind < 0 || a.objectKind > 255 ||
			    a.room < 0 || a.room > 255 || a.x < -32768 || a.x > 32767 || a.y < -32768 ||
			    a.y > 32767 || a.life < -32768 || a.life > 32767)
				issues.push_back(
				    {true, "Invalid native actor placement or fixed slot ID", "", "", "", int(i)});
		}
		size_t rules = 0;
		if (p.native.programs.size() > 255)
			error("Maximum 255 native behaviors");
		for (const auto &program : p.native.programs) {
			rules += program.rules.size();
			if (program.source < -1 || program.source >= 255)
				error("Invalid native behavior provenance");
			if (program.rules.size() > 65535)
				error("Too many native state transitions");
			for (const auto &rule : program.rules) {
				if (rule.comments.size() > 2047 || rule.type < 0 || rule.type > 65535 ||
				    rule.nextType < 0 || rule.nextType > 65535 || rule.next < 0 ||
				    rule.next >= int(program.rules.size()) || rule.dx < -128 || rule.dx > 127 ||
				    rule.dy < -128 || rule.dy > 127 || rule.flags < 0 || rule.flags > 255)
					error("Invalid native state transition");
				for (int i = 0; i < 3; ++i)
					if (rule.opcode[i] < 0 || rule.opcode[i] >= 0x8c || rule.argument[i] < -32768 ||
					    rule.argument[i] > 32767)
						error("Invalid native opcode or operand");
			}
		}
		if (rules > 24000)
			error("Maximum 24000 native state transitions");
		std::set<int> owned;
		if (p.native.puzzles.size() > 256)
			error("Too many native puzzle groups");
		for (const auto &puzzle : p.native.puzzles) {
			if (puzzle.comments.size() > 2047 || puzzle.name.empty() || puzzle.name.size() > 160 ||
			    puzzle.actors.empty())
				error("Invalid native puzzle name or empty group");
			for (int id : puzzle.actors)
				if (id < 0 || id >= int(p.native.actors.size()) || !owned.insert(id).second)
					error("Native actors belong to exactly one puzzle group");
		}
		if (!p.native.puzzles.empty() && owned.size() != p.native.actors.size())
			error("Native puzzle groups must cover every actor");
		if (!p.native.actors.empty()) {
			const auto &a = p.native.actors[0];
			if (roomIndex(p, p.spawn.room) != a.room || a.x != p.spawn.x || a.y != p.spawn.y ||
			    a.facingLeft != p.spawnLeft)
				issues.push_back(
				    {true, "Native player start must match actor 0", p.spawn.room, "", "", 0});
		}
	}
	if (nodeCount > 256)
		error("Maximum 256 graph nodes per project");
	std::set<int> reached;
	std::vector<int> pending{roomIndex(p, p.spawn.room)};
	while (!pending.empty()) {
		int i = pending.back();
		pending.pop_back();
		if (i < 0 || !reached.insert(i).second)
			continue;
		for (const auto &id : p.rooms[i].exits)
			pending.push_back(roomIndex(p, id));
	}
	for (int i = 0; i < int(p.rooms.size()); ++i)
		if (!reached.count(i))
			issues.push_back(
			    {false, p.rooms[i].name + ": not reachable from the start through screen links",
				 p.rooms[i].id});
	return issues;
}
static void requireValid(const Project &p) {
	for (const auto &issue : validate(p))
		if (issue.error)
			throw std::runtime_error(issue.message);
}
std::array<int8_t, 7424> compileCollision(const Project &p) {
	requireValid(p);
	std::array<int8_t, 7424> result{};
	std::fill_n(result.begin(), 256, -1);
	for (size_t i = 0; i < p.rooms.size(); ++i) {
		const auto &r = p.rooms[i];
		for (int d = 0; d < 4; ++d)
			result[d * 64 + i] = int8_t(roomIndex(p, r.exits[d]));
		for (int c = 0; c < 112; ++c)
			result[256 + i * 112 + c] = r.collision[c];
		for (const auto &l : r.ledges) {
			// Original climb behavior reads neighboring CT cells. This stamp
			// supplies one floor edge and clearance on the approach side.
			auto *grid = &result[256 + i * 112];
			grid[l.row * 16 + l.column] = 1;
			grid[(l.row - 1) * 16 + l.column] = 0;
			grid[l.row * 16 + l.column - l.facing] = 0;
			grid[(l.row - 1) * 16 + l.column - l.facing] = 0;
		}
	}
	return result;
}
std::string serialize(const Project &p) {
	return json(p).dump(2) + '\n';
}
static json parse(const std::string &text) {
	if (text.size() > 4 * 1024 * 1024)
		throw std::runtime_error("Project exceeds 4 MiB");
	return json::parse(text, [](int depth, json::parse_event_t event, json &value) {
		if (depth > 32)
			throw std::runtime_error("Project nesting exceeds 32 levels");
		if (event == json::parse_event_t::value &&
		    (value.is_number_float() ||
		     (value.is_number_unsigned() && value.get<uint64_t>() > INT32_MAX) ||
		     (value.is_number_integer() && !value.is_number_unsigned() &&
		      (value.get<int64_t>() < INT32_MIN || value.get<int64_t>() > INT32_MAX))))
			throw std::runtime_error("Project numbers must be bounded integers");
		return true;
	});
}
Project deserialize(const std::string &text) {
	auto j = parse(text);
	// Check BEFORE narrowing byte values through the legacy array type.
	for (const auto &room : j.at("rooms")) {
		const auto &cells = room.at("collision");
		if (cells.size() != 112)
			throw std::runtime_error("Collision grid must contain exactly 112 cells");
		for (const auto &c : cells)
			if (!c.is_number_integer() || c.get<int64_t>() < 0 || c.get<int64_t>() > 2)
				throw std::runtime_error("Invalid collision cell");
		if (room.at("art").size() != 3 || room.at("exits").size() != 4)
			throw std::runtime_error("Invalid room layer/exit count");
	}
	auto p = j.get<Project>();
	requireValid(p);
	return p;
}
static std::string readText(const std::filesystem::path &path) {
	std::error_code ec;
	auto n = std::filesystem::file_size(path, ec);
	if (ec || n > 4 * 1024 * 1024)
		throw std::runtime_error("Project missing or too large");
	std::ifstream file(path, std::ios::binary);
	std::string s(size_t(n), '\0');
	if (!file.read(s.data(), s.size()))
		throw std::runtime_error("Cannot read project");
	return s;
}
Project loadProject(const std::filesystem::path &path) {
	return deserialize(readText(path));
}
static void writeText(const std::string &text, const std::filesystem::path &path) {
	std::filesystem::create_directories(std::filesystem::absolute(path).parent_path());
	std::string reason;
	if (!writeAtomicFile(
	        path.u8string(), false, [&](File &f) { f.write(text.data(), uint32_t(text.size())); },
	        reason))
		throw std::runtime_error("Cannot save project: " + reason);
}
void saveProject(const Project &p, const std::filesystem::path &path) {
	requireValid(p);
	const auto text = serialize(p);
	if (text.size() > 4 * 1024 * 1024)
		throw std::runtime_error("Project exceeds the 4 MiB document limit");
	writeText(text, path);
}
std::filesystem::path assetPath(const std::filesystem::path &root, const std::string &relative) {
	if (relative.empty() || !safeAssetName(relative))
		throw std::runtime_error("Unsafe asset path");
	const auto base = std::filesystem::weakly_canonical(std::filesystem::absolute(root));
	const auto target = std::filesystem::weakly_canonical(base / std::filesystem::u8path(relative));
	auto a = base.begin(), b = target.begin();
	for (; a != base.end(); ++a, ++b)
		if (b == target.end() || *a != *b)
			throw std::runtime_error("Asset escapes project folder");
	return target;
}
std::string importImage(const std::filesystem::path &source, const std::filesystem::path &root) {
	readImage(source); // Validate before accepting a project dependency.
	std::ifstream f(source, std::ios::binary);
	std::vector<char> bytes((std::istreambuf_iterator<char>(f)), {});
	const auto crc = crc32(0, reinterpret_cast<const Bytef *>(bytes.data()), uInt(bytes.size()));
	std::ostringstream stem;
	stem << "art/" << std::hex << crc << '-' << bytes.size();
	auto ext = source.extension().u8string();
	std::transform(ext.begin(), ext.end(), ext.begin(),
	               [](unsigned char c) { return char(std::tolower(c)); });
	if (ext != ".png" && ext != ".jpg" && ext != ".jpeg" && ext != ".bmp")
		ext = ".png";
	std::string name = stem.str() + ext;
	auto target = assetPath(root, name);
	std::filesystem::create_directories(target.parent_path());
	if (std::filesystem::exists(target)) {
		std::ifstream old(target, std::ios::binary);
		std::vector<char> existing((std::istreambuf_iterator<char>(old)), {});
		if (existing != bytes)
			throw std::runtime_error("Asset name collision: existing file preserved");
	} else
		writeText(std::string(bytes.begin(), bytes.end()), target);
	return name;
}
void sliceImage(Project &p, const std::string &image, int w, int h, const Slice &s, Layer layer,
                bool rebuild) {
	if (int(layer) < 0 || int(layer) > 2 || s.width < 1 || s.height < 1 || s.strideX < 1 ||
	    s.strideY < 1 || w < 1 || h < 1 || w > 16384 || h > 16384 || s.width > 16384 ||
	    s.height > 16384 || s.strideX > 16384 || s.strideY > 16384)
		throw std::runtime_error("Invalid slice geometry");
	const int cols = 1 + (std::max(0, w - s.width) + s.strideX - 1) / s.strideX;
	const int rows = 1 + (std::max(0, h - s.height) + s.strideY - 1) / s.strideY;
	if (int64_t(cols) * rows > 64)
		throw std::runtime_error("Image produces more than 64 rooms; increase source slice size");
	Project next = p;
	if (rebuild) {
		next.rooms.clear();
		next.modules.clear();
	}
	if (!rebuild && int(next.rooms.size()) != cols * rows)
		throw std::runtime_error("Layer sheet must match the existing room grid");
	for (int y = 0; y < rows; ++y)
		for (int x = 0; x < cols; ++x) {
			const int index = y * cols + x;
			Room *r;
			if (rebuild)
				r = &addRoom(next, x, y);
			else {
				auto it = std::find_if(next.rooms.begin(), next.rooms.end(),
				                       [&](const auto &a) { return a.column == x && a.row == y; });
				if (it == next.rooms.end())
					throw std::runtime_error(
					    "Layer sheet grid differs from existing room positions");
				r = &*it;
			}
			r->art[int(layer)] = {image, x * s.strideX, y * s.strideY, s.width, s.height};
			(void)index;
		}
	if (rebuild) {
		for (int y = 0; y < rows; ++y)
			for (int x = 0; x < cols; ++x) {
				const int i = y * cols + x;
				if (x + 1 < cols)
					connectRooms(next, i, 2, i + 1, true);
				if (y + 1 < rows)
					connectRooms(next, i, 1, i + cols, true);
			}
		next.spawn = {next.rooms[0].id, 32, 214};
	}
	requireValid(next);
	p = std::move(next);
}
void savePrefab(const Module &module, const std::filesystem::path &root,
                const std::filesystem::path &dest) {
	Module m = module;
	m.outputs.clear();
	m.a.room = m.b.room = "local";
	for (auto &n : m.nodes)
		n.at.room = "local";
	Project checked = makeProject();
	checked.rooms[0].id = "local";
	checked.spawn.room = "local";
	checked.modules = {m};
	requireValid(checked);
	forEachArt(m, [&](Art &a) {
		if (!a.file.empty())
			a.file =
			    importImage(assetPath(root, a.file), std::filesystem::absolute(dest).parent_path());
	});
	copyModuleAudio(m, root, std::filesystem::absolute(dest).parent_path());
	writeText(json{{"schema", 7}, {"module", m}}.dump(2) + '\n', dest);
}
Module loadPrefab(Project &p, const Anchor &at, const std::filesystem::path &src,
                  const std::filesystem::path &root) {
	const auto j = parse(readText(src));
	if (j.at("schema") != 1 && j.at("schema") != 2 && j.at("schema") != 3 && j.at("schema") != 4 &&
	    j.at("schema") != 5 && j.at("schema") != 6 && j.at("schema") != 7)
		throw std::runtime_error("Unsupported prefab schema");
	Module m = j.at("module").get<Module>();
	m.a.room = m.b.room = at.room;
	for (auto &n : m.nodes)
		n.at.room = at.room;
	m.outputs.clear();
	Project candidate = p;
	m.id = newId(candidate, "module");
	candidate.modules.push_back(m);
	requireValid(candidate); // Validate before coordinate arithmetic.
	if (at.x < 0 || at.x > 248 || at.y < 0 || at.y > 216)
		throw std::runtime_error("Invalid prefab destination");
	const int dx = at.x - m.a.x, dy = at.y - m.a.y;
	m.a.x += dx;
	m.a.y += dy;
	m.b.x += dx;
	m.b.y += dy;
	for (auto &n : m.nodes) {
		n.at.x += dx;
		n.at.y += dy;
	}
	candidate.modules.back() = m;
	requireValid(candidate);
	forEachArt(m, [&](Art &a) {
		if (!a.file.empty())
			a.file = importImage(assetPath(src.parent_path(), a.file), root);
	});
	copyModuleAudio(m, src.parent_path(), root);
	// Commit the ID counter only after validation and dependency import succeed.
	p.serial = candidate.serial;
	return m;
}
void History::remember(const Project &p) {
	const auto s = serialize(p);
	if (undoStack.empty() || undoStack.back() != s)
		undoStack.push_back(s);
	size_t bytes = 0;
	for (const auto &v : undoStack)
		bytes += v.size();
	while (undoStack.size() > 100 || (bytes > 16 * 1024 * 1024 && undoStack.size() > 1)) {
		bytes -= undoStack.front().size();
		undoStack.erase(undoStack.begin());
	}
	redoStack.clear();
}
// Internal snapshots may contain unfinished inspector edits. They must remain
// undoable even while the strict public project loader would reject them.
bool History::undo(Project &p) {
	if (undoStack.empty())
		return false;
	auto next = json::parse(undoStack.back()).get<Project>();
	redoStack.push_back(serialize(p));
	p = std::move(next);
	undoStack.pop_back();
	return true;
}
bool History::redo(Project &p) {
	if (redoStack.empty())
		return false;
	auto next = json::parse(redoStack.back()).get<Project>();
	undoStack.push_back(serialize(p));
	p = std::move(next);
	redoStack.pop_back();
	return true;
}
} // namespace editor
