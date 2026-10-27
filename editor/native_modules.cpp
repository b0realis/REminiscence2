// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_logic.h"
#include "game.h"
#include <algorithm>
#include <stdexcept>

namespace editor {
int nativeActorSource(const NativeActor &a) {
	return a.source < 0 ? a.id : a.source;
}
int nativeProgramSource(const NativeCampaign &n, int b) {
	const int source = n.programs.at(b).source;
	return source < 0 ? b : source;
}
namespace {
enum Use { ActorUse = 1, RoomUse = 2, ValueUse = 4 };
bool literalActor(int op) {
	return op == 0x30 || op == 0x31 || op == 0x4a || op == 0x58 || op == 0x59;
}
struct Uses {
	std::array<int, 4> mask{};
	std::array<std::string, 4> labels{};
};
Uses dataUses(const NativeProgram &program) {
	Uses u;
	auto mark = [&](int slot, int kind, const char *label) {
		if (slot < 0 || slot >= 4)
			return;
		u.mask[slot] |= kind;
		if (u.labels[slot].find(label) == std::string::npos) {
			if (!u.labels[slot].empty())
				u.labels[slot] += ", ";
			u.labels[slot] += label;
		}
	};
	// Exhaustive InitPGE.data reads in piege.cpp. In particular 48/6c take an
	// object CLASS, not an actor ID; 82 takes TWO actor IDs, not room numbers.
	for (const auto &r : program.rules)
		for (int k = 0; k < 3; ++k) {
			const int op = r.opcode[k], a = r.argument[k];
			if (op >= 0x23 && op <= 0x26)
				mark(op - 0x23, ActorUse, "Send message");
			if (op >= 0x38 && op <= 0x3b)
				mark(op - 0x38, ActorUse, "Listen to actor");
			if (op == 0x41 || op == 0x42)
				mark(a, ActorUse, op == 0x41 ? "Wake actor" : "Sleep actor");
			if (op == 0x49)
				mark(0, ActorUse, "Exclude collision actor");
			if (op == 0x65 || op == 0x66) {
				mark(a, ActorUse, "Credits holder");
				mark(a + 1, ValueUse, "Credits amount");
			}
			if (op == 0x82) {
				mark(a, ActorUse, "Transport actor");
				mark(a + 1, ActorUse, "Destination marker");
			}
			if (op == 0x57)
				mark(a, RoomUse, "Restore room");
			if (op == 0x44)
				mark(a, ValueUse, "Counter");
			if (op == 0x48 || op == 0x6c)
				mark(0, ValueUse, "Object class");
			if (op == 0x58)
				mark(0, ValueUse, "Health value");
			if (op == 0x5c)
				mark(3, ValueUse, "Cutscene delay");
			if (op == 0x5f)
				mark(0, ValueUse, "Collision distance");
			if (op == 0x87)
				mark(a, ValueUse, "Sound group");
		}
	return u;
}
bool hasOp(const NativeCampaign &n, int actor, int op) {
	const auto &a = n.actors.at(actor);
	for (const auto &r : n.programs.at(a.behavior).rules)
		if (std::find(r.opcode.begin(), r.opcode.end(), op) != r.opcode.end())
			return true;
	return false;
}
int privateProgram(NativeCampaign &n, int behavior) {
	std::set<int> used;
	for (const auto &a : n.actors)
		used.insert(a.behavior);
	int slot = 0;
	while (slot < int(n.programs.size()) && used.count(slot))
		++slot;
	if (slot >= 255)
		throw std::runtime_error("All 255 native behavior slots are in use");
	auto copy = n.programs.at(behavior);
	copy.source = nativeProgramSource(n, behavior);
	if (slot == int(n.programs.size()))
		n.programs.push_back(copy);
	else
		n.programs[slot] = copy;
	return slot;
}
void checked(Project &p, const NativeCampaign &base) {
	compileCollision(p);
	validateNativeLogic(p.native, base);
}
void relocate(NativeCampaign &n, const std::vector<int> &ids, const std::map<int, int> &rooms,
              int dx, int dy) {
	for (const auto &[from, to] : rooms)
		if (from < 0 || from >= 64 || to < 0 || to >= 64)
			throw std::runtime_error("Room mapping must use slots 0..63");
	if (dx < -256 || dx > 256 || dy < -224 || dy > 224)
		throw std::runtime_error("Puzzle offset exceeds one screen");
	for (int id : ids) {
		auto &a = n.actors.at(id);
		// Holding lists (FE/FF) are inventory/spawn state, never screen coordinates.
		if (a.room < 64) {
			if (rooms.count(a.room))
				a.room = rooms.at(a.room);
			a.x += dx;
			a.y += dy;
			if (a.x < -256 || a.x > 511 || a.y < -224 || a.y > 447)
				throw std::runtime_error("Puzzle placement leaves the supported screen margin");
		}
		for (const auto &port : nativePorts(n, id))
			if (port.kind == NativePortKind::Room && rooms.count(port.target)) {
				if (!port.editable && rooms.at(port.target) != port.target)
					throw std::runtime_error(
					    "A room operand also holds another value; relocate this actor manually");
				a.data[port.data] = rooms.at(port.target);
			}
	}
}
} // namespace
std::vector<NativePort> nativePorts(const NativeCampaign &n, int actor) {
	if (actor < 0 || actor >= int(n.actors.size()))
		return {};
	const auto &a = n.actors[actor];
	if (a.behavior < 0 || a.behavior >= int(n.programs.size()))
		return {};
	const auto &program = n.programs[a.behavior];
	const auto uses = dataUses(program);
	std::vector<NativePort> ports;
	for (int d = 0; d < 4; ++d) {
		if (uses.mask[d] & ActorUse)
			ports.push_back({NativePortKind::Actor, actor, d, -1, -1, a.data[d], uses.labels[d],
			                 uses.mask[d] == ActorUse});
		if (uses.mask[d] & RoomUse)
			ports.push_back({NativePortKind::Room, actor, d, -1, -1, a.data[d], uses.labels[d],
			                 uses.mask[d] == RoomUse});
	}
	for (int r = 0; r < int(program.rules.size()); ++r)
		for (int k = 0; k < 3; ++k) {
			const auto &row = program.rules[r];
			if (literalActor(row.opcode[k]))
				ports.push_back({NativePortKind::Actor, actor, -1, r, k, row.argument[k],
				                 nativeOpcodeName(row.opcode[k]), true});
		}
	return ports;
}
std::set<int> nativeSharedActors(const NativeCampaign &n) {
	std::set<int> shared{0};
	if (!n.actors.empty()) {
		int gun = n.actors[0].data[0];
		for (int depth = 0; depth < 3 && gun > 0 && gun < int(n.actors.size()); ++depth) {
			shared.insert(gun);
			gun = n.actors[gun].data[depth == 1 ? 3 : 0];
		}
	}
	for (const auto &a : n.actors) {
		const int b = nativeProgramSource(n, a.behavior);
		if (a.type == 316 && (b == 228 || b == 229))
			shared.insert(a.id); // global draw/holster latch
	}
	// DOS level-seven replay fix explicitly addresses this original slot.
	if (n.edition == kResourceTypeDOS && n.level == 6 && n.actors.size() > 79)
		shared.insert(79);
	return shared;
}
std::vector<int> nativePuzzleClosure(const NativeCampaign &n, int puzzle) {
	if (puzzle < 0 || puzzle >= int(n.puzzles.size()))
		throw std::runtime_error("Choose a native puzzle module");
	const auto shared = nativeSharedActors(n);
	std::set<int> members(n.puzzles[puzzle].actors.begin(), n.puzzles[puzzle].actors.end());
	for (int id : members)
		if (shared.count(id))
			throw std::runtime_error(
			    "Player, weapon and engine-owned actors remain shared campaign services");
	bool changed = true;
	while (changed) {
		changed = false;
		for (const auto &a : n.actors)
			if (!shared.count(a.id))
				for (int target : nativeDependencies(n, a.id))
					if (!shared.count(target) && (members.count(a.id) || members.count(target))) {
						changed |= members.insert(a.id).second;
						changed |= members.insert(target).second;
					}
	}
	return {members.begin(), members.end()};
}
std::string nativeActorRole(const NativeCampaign &n, int id) {
	if (id == 0)
		return "Conrad";
	const auto &a = n.actors.at(id);
	if (a.objectKind == 10)
		return "Enemy / NPC";
	if (hasOp(n, id, 0x82))
		return "Transport / destination";
	if (hasOp(n, id, 0x84))
		return "Level transition";
	if (hasOp(n, id, 0x36) || hasOp(n, id, 0x37) || hasOp(n, id, 0x68))
		return "Door / collision control";
	if (hasOp(n, id, 0x7b))
		return "Dialogue / terminal";
	if (hasOp(n, id, 0x69))
		return "Save station";
	if (hasOp(n, id, 0x30))
		return "Inventory / pickup";
	if (hasOp(n, id, 0x65) || hasOp(n, id, 0x66))
		return "Credits / mission";
	if (hasOp(n, id, 0x6e) || hasOp(n, id, 0x71))
		return "Lift / attachment";
	if (a.room >= 64)
		return "Hidden helper";
	if (!nativeDependencies(n, id).empty())
		return "Trigger / controller";
	return "Actor / scenery";
}
std::vector<std::string> nativeDynamicLinks(const NativeCampaign &n, int id) {
	std::set<std::string> links;
	for (const auto &r : n.programs.at(n.actors.at(id).behavior).rules)
		for (int op : r.opcode) {
			if (op == 0x2f || op == 0x33 || op == 0x60 || (op >= 0x6e && op <= 0x71) ||
			    op == 0x80 || op == 0x81)
				links.insert("Runtime inventory / incoming-message sender");
			if (op == 0x48 || op == 0x49 || op == 0x6c || op == 0x7c || op == 0x7e || op == 0x45 ||
			    op == 0x2b || op == 0x2c || op == 0x2d)
				links.insert("Collision-selected actor (depends on room placement)");
			if (op == 0x32 || op == 0x83)
				links.insert("Shared player inventory / item identity");
			if (op == 0x5a || op == 0x5c || op == 0x7b || op == 0x84 || op == 0x8a || op == 0x8b)
				links.insert("Shared campaign text / cinematic / level / gun state");
		}
	return {links.begin(), links.end()};
}
void isolateNativeBehavior(Project &p, int actor, const NativeCampaign &base) {
	Project next = p;
	auto &n = next.native;
	auto &a = n.actors.at(actor);
	if (std::count_if(n.actors.begin(), n.actors.end(),
	                  [&](const auto &b) { return b.behavior == a.behavior; }) > 1)
		a.behavior = privateProgram(n, a.behavior);
	checked(next, base);
	p = std::move(next);
}
void reconnectNativePort(Project &p, const NativePort &port, int target,
                         const NativeCampaign &base) {
	if (port.actor == 0)
		throw std::runtime_error("Conrad's shared dependency ports are protected");
	Project next = p;
	auto &n = next.native;
	auto actual = nativePorts(n, port.actor);
	auto found = std::find_if(actual.begin(), actual.end(), [&](const auto &a) {
		return a.kind == port.kind && a.data == port.data && a.rule == port.rule &&
		       a.operand == port.operand;
	});
	if (found == actual.end() || !found->editable)
		throw std::runtime_error("This operand has mixed uses; preserve its original value");
	if (target < 0 || target >= (port.kind == NativePortKind::Room ? 64 : int(n.actors.size())))
		throw std::runtime_error("Port target is outside this level");
	auto &a = n.actors.at(port.actor);
	if (port.data >= 0)
		a.data[port.data] = target;
	else {
		isolateNativeBehavior(next, port.actor, base);
		auto &owner = next.native.actors.at(port.actor);
		next.native.programs[owner.behavior].rules.at(port.rule).argument.at(port.operand) = target;
	}
	checked(next, base);
	p = std::move(next);
}
int copyNativePuzzle(Project &p, int puzzle, const std::map<int, int> &rooms, int dx, int dy,
                     const NativeCampaign &base) {
	const auto members = nativePuzzleClosure(p.native, puzzle);
	// 255 is the inventory sentinel. Original levels may have an actor there,
	// but newly allocated modules must never acquire this ambiguous identity.
	if (p.native.actors.size() + members.size() > 255)
		throw std::runtime_error("Not enough native actor slots: need " +
		                         std::to_string(members.size()) + ", available " +
		                         std::to_string(std::max(0, 255 - int(p.native.actors.size()))) +
		                         ". Edit or move existing modules in this level.");
	Project next = p;
	auto &n = next.native;
	std::map<int, int> actors, programs;
	const int first = int(n.actors.size());
	for (int id : members) {
		auto a = p.native.actors[id];
		a.source = nativeActorSource(a);
		a.id = int(n.actors.size());
		actors[id] = a.id;
		n.actors.push_back(a);
	}
	// Allocate one private behavior per original behavior, keeping same-behavior
	// checks between transport endpoints meaningful inside the copied bundle.
	for (int id : members) {
		const int b = p.native.actors[id].behavior;
		if (!programs.count(b))
			programs[b] = privateProgram(n, b);
		n.actors[actors[id]].behavior = programs[b];
	}
	for (int id : members)
		for (const auto &port : nativePorts(p.native, id)) {
			if (port.kind != NativePortKind::Actor || !actors.count(port.target))
				continue;
			if (!port.editable)
				throw std::runtime_error(
				    "Actor " + std::to_string(id) +
				    " has a mixed-use dependency; it cannot be automatically relocated");
			auto &a = n.actors[actors[id]];
			if (port.data >= 0)
				a.data[port.data] = actors[port.target];
			else
				n.programs[a.behavior].rules[port.rule].argument[port.operand] =
				    actors[port.target];
		}
	std::vector<int> copied;
	for (int id = first; id < int(n.actors.size()); ++id)
		copied.push_back(id);
	relocate(n, copied, rooms, dx, dy);
	auto name = p.native.puzzles[puzzle].name + " copy";
	name.resize(std::min(name.size(), size_t(160)));
	n.puzzles.push_back({name, copied});
	// Authored extension graphs that belong entirely to this bundle travel too.
	// Partial ownership is explicit: silently sharing a bound actor is forbidden.
	for (const auto &m : p.modules) {
		int inside = 0, outside = 0;
		for (const auto &node : m.nodes)
			if (node.kind == NodeKind::Actor)
				(actors.count(node.nativeActor) ? inside : outside)++;
		if (!inside)
			continue;
		if (outside)
			throw std::runtime_error("An authored graph binds actors inside and outside the "
			                         "puzzle; split that graph before copying");
		Module copy = m;
		copy.id = newId(next, "module");
		copy.name += " copy";
		copy.name.resize(std::min(copy.name.size(), size_t(160)));
		auto move = [&](Anchor &at) {
			const int old = roomIndex(p, at.room);
			if (rooms.count(old))
				at.room = p.rooms[rooms.at(old)].id;
			at.x += dx;
			at.y += dy;
		};
		move(copy.a);
		move(copy.b);
		for (auto &node : copy.nodes) {
			move(node.at);
			if (actors.count(node.nativeActor))
				node.nativeActor = actors[node.nativeActor];
			if (actors.count(node.messageFrom))
				node.messageFrom = actors[node.messageFrom];
		}
		next.modules.push_back(std::move(copy));
	}
	checked(next, base);
	const int result = int(n.puzzles.size()) - 1;
	p = std::move(next);
	return result;
}
void moveNativePuzzle(Project &p, int puzzle, const std::map<int, int> &rooms, int dx, int dy,
                      const NativeCampaign &base) {
	const auto members = nativePuzzleClosure(p.native, puzzle);
	Project next = p;
	relocate(next.native, members, rooms, dx, dy);
	// Bound authored nodes need their interaction positions moved with the actors.
	const std::set<int> owned(members.begin(), members.end());
	for (auto &m : next.modules) {
		bool bound = false;
		for (const auto &node : m.nodes)
			bound |= node.kind == NodeKind::Actor && owned.count(node.nativeActor);
		if (!bound)
			continue;
		for (const auto &node : m.nodes)
			if (node.kind == NodeKind::Actor && !owned.count(node.nativeActor))
				throw std::runtime_error(
				    "Split the authored graph spanning multiple puzzles before moving");
		auto move = [&](Anchor &at) {
			const int old = roomIndex(p, at.room);
			if (rooms.count(old))
				at.room = p.rooms[rooms.at(old)].id;
			at.x += dx;
			at.y += dy;
		};
		move(m.a);
		move(m.b);
		for (auto &node : m.nodes)
			move(node.at);
	}
	checked(next, base);
	p = std::move(next);
}
void installNativeActors(Game &g, const NativeCampaign &n) {
	// Keep a full pristine template snapshot: assigning earlier IDs must not
	// overwrite a later clone's source. All 32 bytes of InitPGE are preserved.
	const std::vector<InitPGE> original(g._res._pgeInit, g._res._pgeInit + g._res._pgeNum);
	for (const auto &a : n.actors) {
		auto init = original.at(nativeActorSource(a));
		init.type = a.type;
		init.obj_node_number = a.behavior;
		init.init_room = a.removed ? 255 : a.room;
		init.pos_x = a.x;
		init.pos_y = a.y;
		init.life = a.life;
		init.mirror_x = !a.facingLeft;
		for (int d = 0; d < 4; ++d)
			init.data[d] = a.data[d];
		g._res._pgeInit[a.id] = init;
	}
	g._res._pgeNum = n.actors.size();
}
} // namespace editor
