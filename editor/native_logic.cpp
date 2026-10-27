// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_logic.h"
#include "game.h"
#include <algorithm>
#include <cstdlib>
#include <map>
#include <set>
#include <stdexcept>
namespace editor {
const char *nativeOpcodeName(int op) {
	static const char *names[] = {
	    "None",
	    "pge_op_isInpUp",
	    "pge_op_isInpBackward",
	    "pge_op_isInpDown",
	    "pge_op_isInpForward",
	    "pge_op_isInpUpMod",
	    "pge_op_isInpBackwardMod",
	    "pge_op_isInpDownMod",
	    "pge_op_isInpForwardMod",
	    "pge_op_isInpIdle",
	    "pge_op_isInpNoMod",
	    "pge_op_getCollision0u",
	    "pge_op_getCollision00",
	    "pge_op_getCollision0d",
	    "pge_op_getCollision1u",
	    "pge_op_getCollision10",
	    "pge_op_getCollision1d",
	    "pge_op_getCollision2u",
	    "pge_op_getCollision20",
	    "pge_op_getCollision2d",
	    "pge_op_doesNotCollide0u",
	    "pge_op_doesNotCollide00",
	    "pge_op_doesNotCollide0d",
	    "pge_op_doesNotCollide1u",
	    "pge_op_doesNotCollide10",
	    "pge_op_doesNotCollide1d",
	    "pge_op_doesNotCollide2u",
	    "pge_op_doesNotCollide20",
	    "pge_op_doesNotCollide2d",
	    "pge_op_collides0o0d",
	    "pge_op_collides2o2d",
	    "pge_op_collides0o0u",
	    "pge_op_collides2o2u",
	    "pge_op_collides2u2o",
	    "pge_hasPiegeSentMessage",
	    "pge_op_sendMessageData0",
	    "pge_op_sendMessageData1",
	    "pge_op_sendMessageData2",
	    "pge_op_sendMessageData3",
	    "pge_op_isPiegeDead",
	    "pge_op_collides1u2o",
	    "pge_op_collides1u1o",
	    "pge_op_collides1o1u",
	    "pge_o_unk0x2B",
	    "pge_o_unk0x2C",
	    "pge_o_unk0x2D",
	    "pge_op_nop",
	    "pge_op_pickupObject",
	    "pge_op_addItemToInventory",
	    "pge_op_copyPiege",
	    "pge_op_canUseCurrentInventoryItem",
	    "pge_op_removeItemFromInventory",
	    "pge_o_unk0x34",
	    "pge_op_isInpMod",
	    "pge_op_setCollisionState1",
	    "pge_op_setCollisionState0",
	    "pge_hasMessageData0",
	    "pge_hasMessageData1",
	    "pge_hasMessageData2",
	    "pge_hasMessageData3",
	    "pge_o_unk0x3C",
	    "pge_o_unk0x3D",
	    "pge_op_setPiegeCounter",
	    "pge_op_decPiegeCounter",
	    "pge_o_unk0x40",
	    "pge_op_wakeUpPiege",
	    "pge_op_removePiege",
	    "pge_op_removePiegeIfNotNear",
	    "pge_op_loadPiegeCounter",
	    "pge_o_unk0x45",
	    "pge_o_unk0x46",
	    "pge_o_unk0x47",
	    "pge_o_unk0x48",
	    "pge_o_unk0x49",
	    "pge_op_killInventoryPiege",
	    "pge_op_killPiege",
	    "pge_op_isInCurrentRoom",
	    "pge_op_isNotInCurrentRoom",
	    "pge_op_scrollPosY",
	    "pge_op_playDefaultDeathCutscene",
	    "pge_o_unk0x50",
	    "None",
	    "pge_o_unk0x52",
	    "pge_o_unk0x53",
	    "pge_op_isPiegeNear",
	    "pge_op_setLife",
	    "pge_op_incLife",
	    "pge_op_setPiegeDefaultAnim",
	    "pge_op_setLifeCounter",
	    "pge_op_decLifeCounter",
	    "pge_op_playCutscene",
	    "pge_op_compareUnkVar",
	    "pge_op_playDeathCutscene",
	    "pge_o_unk0x5D",
	    "pge_o_unk0x5E",
	    "pge_o_unk0x5F",
	    "pge_op_findAndCopyPiege",
	    "pge_op_isInRandomRange",
	    "pge_o_unk0x62",
	    "pge_o_unk0x63",
	    "pge_o_unk0x64",
	    "pge_op_addToCredits",
	    "pge_op_subFromCredits",
	    "pge_o_unk0x67",
	    "pge_op_setCollisionState2",
	    "pge_op_saveState",
	    "pge_o_unk0x6A",
	    "pge_isToggleable",
	    "pge_o_unk0x6C",
	    "pge_op_isCollidingObject",
	    "pge_o_unk0x6E",
	    "pge_o_unk0x6F",
	    "pge_o_unk0x70",
	    "pge_o_unk0x71",
	    "pge_o_unk0x72",
	    "pge_o_unk0x73",
	    "pge_op_collides4u",
	    "pge_op_doesNotCollide4u",
	    "pge_op_isBelowConrad",
	    "pge_op_isAboveConrad",
	    "pge_op_isNotFacingConrad",
	    "pge_op_isFacingConrad",
	    "pge_op_collides2u1u",
	    "pge_op_displayText",
	    "pge_o_unk0x7C",
	    "pge_op_playSound",
	    "pge_o_unk0x7E",
	    "pge_o_unk0x7F",
	    "pge_op_setPiegePosX",
	    "pge_op_setPiegePosModX",
	    "pge_op_changeRoom",
	    "pge_op_hasInventoryItem",
	    "pge_op_changeLevel",
	    "pge_op_shakeScreen",
	    "pge_o_unk0x86",
	    "pge_op_playSoundGroup",
	    "pge_op_adjustPos",
	    "None",
	    "pge_op_setGunVar",
	    "pge_op_compareGunVar",
	};
	return op >= 0 && op < 140 ? names[op] : "Invalid";
}
} // namespace editor
namespace editor {
std::vector<int> nativeDependencies(const NativeCampaign &n, int actor) {
	std::set<int> refs;
	for (const auto &p : nativePorts(n, actor))
		if (p.kind == NativePortKind::Actor && p.target >= 0 && p.target < int(n.actors.size()) &&
		    p.target != actor)
			refs.insert(p.target);
	return {refs.begin(), refs.end()};
}
void captureNativeLogic(Game &g, NativeCampaign &n) {
	n.programs.clear();
	for (int b = 0; b < g._res._numObjectNodes; ++b) {
		NativeProgram program;
		const auto *native = g._res._objectNodesMap[b];
		if (native)
			for (int i = 0; i < native->num_objects; ++i) {
				const auto &r = native->objects[i];
				program.rules.push_back({r.type,
				                         r.init_obj_type,
				                         r.init_obj_number,
				                         r.dx,
				                         r.dy,
				                         r.flags,
				                         {r.opcode1, r.opcode2, r.opcode3},
				                         {r.opcode_arg1, r.opcode_arg2, r.opcode_arg3},
				                         true});
			}
		n.programs.push_back(std::move(program));
	}
	// Connected components of reviewed actor-reference operands, not arbitrary
	// integers. Player, inventory and weapon bundle remain external dependencies.
	std::vector<int> parent(n.actors.size());
	const auto external = nativeSharedActors(n);
	for (size_t i = 0; i < parent.size(); ++i)
		parent[i] = int(i);
	auto root = [&](int id) {
		while (parent[id] != id)
			id = parent[id];
		return id;
	};
	for (size_t i = 0; i < parent.size(); ++i)
		for (int other : nativeDependencies(n, int(i)))
			if (!external.count(int(i)) && !external.count(other))
				parent[root(other)] = root(int(i));
	std::map<int, std::vector<int>> groups;
	for (size_t i = 0; i < parent.size(); ++i)
		groups[external.count(int(i)) ? -1 : root(int(i))].push_back(int(i));
	n.puzzles.clear();
	for (const auto &[id, actors] : groups)
		n.puzzles.push_back({id < 0 ? "Player / shared inventory"
		                            : nativeActorRole(n, actors.front()) + " / actor " +
		                                  std::to_string(actors.front()),
		                     actors});
}
static bool sameRules(const std::vector<NativeRule> &a, const std::vector<NativeRule> &b) {
	if (a.size() != b.size())
		return false;
	for (size_t i = 0; i < a.size(); ++i) {
		const auto &r = a[i], &s = b[i];
		if (r.type != s.type || r.nextType != s.nextType || r.next != s.next || r.dx != s.dx ||
		    r.dy != s.dy || r.flags != s.flags || r.opcode != s.opcode ||
		    r.argument != s.argument || r.enabled != s.enabled)
			return false;
	}
	return true;
}
bool sameNativeLogic(const NativeCampaign &a, const NativeCampaign &b) {
	if (a.programs.size() != b.programs.size())
		return false;
	for (size_t i = 0; i < a.programs.size(); ++i) {
		if (a.programs[i].source != b.programs[i].source)
			return false;
		const auto &x = a.programs[i].rules, &y = b.programs[i].rules;
		if (x.size() != y.size())
			return false;
		for (size_t j = 0; j < x.size(); ++j) {
			const auto &r = x[j], &s = y[j];
			if (r.type != s.type || r.nextType != s.nextType || r.next != s.next || r.dx != s.dx ||
			    r.dy != s.dy || r.flags != s.flags || r.opcode != s.opcode ||
			    r.argument != s.argument || r.enabled != s.enabled)
				return false;
		}
	}
	return true;
}
void validateNativeLogic(const NativeCampaign &n, const NativeCampaign &base) {
	for (int id : nativeSharedActors(base))
		if (id < int(n.actors.size()) && n.actors[id].removed)
			throw std::runtime_error("Shared player/weapon/campaign services cannot be removed");
	if (n.actors.size() < base.actors.size() || n.actors.size() > 256 ||
	    (n.actors.size() > base.actors.size() && n.actors.size() > 255))
		throw std::runtime_error(
		    "Keep original actor slots; new modules cannot use reserved actor 255");
	if (n.programs.empty()) {
		if (n.actors.size() != base.actors.size())
			throw std::runtime_error("Copies require native rule tables");
		auto legacy = n;
		legacy.programs = base.programs;
		for (size_t i = 0; i < n.actors.size(); ++i)
			if (n.actors[i].behavior != base.actors[i].behavior ||
			    n.actors[i].type != base.actors[i].type || n.actors[i].data != base.actors[i].data)
				throw std::runtime_error("Import native rule tables before changing script setup");
		validateNativeLogic(legacy, base);
		return;
	}
	if (n.programs.size() < base.programs.size() || n.programs.size() > 255)
		throw std::runtime_error("Native behavior slots must remain stable");
	for (size_t i = 0; i < n.programs.size(); ++i) {
		const int origin = nativeProgramSource(n, int(i));
		if (origin < 0 || origin >= int(base.programs.size()))
			throw std::runtime_error("Invalid native behavior template");
		const auto &original = base.programs[origin].rules;
		// Unchanged source tables are already certified by the resource loader.
		// Most campaign edits change one table; avoid repeatedly rebuilding the
		// same operand/state sets for every untouched alias in every frame.
		if (sameRules(n.programs[i].rules, original))
			continue;
		std::set<std::pair<int, int>> operands;
		std::set<int> opcodes, animationTypes;
		for (const auto &r : original) {
			animationTypes.insert(r.type);
			animationTypes.insert(r.nextType);
			for (int k = 0; k < 3; ++k) {
				operands.insert({r.opcode[k], r.argument[k]});
				opcodes.insert(r.opcode[k]);
			}
		}
		const auto &rows = n.programs[i].rules;
		if (rows.empty() != original.empty() || rows.size() > 4096)
			throw std::runtime_error("Invalid native behavior size");
		std::set<int> stateEntries;
		for (const auto &r : rows)
			stateEntries.insert(r.type);
		for (const auto &originalRow : original)
			if (!stateEntries.count(originalRow.type))
				throw std::runtime_error("Keep at least one rule for every original animation "
				                         "state; disable rules instead of removing state entries");
		for (const auto &r : rows) {
			if (!animationTypes.count(r.type) || !animationTypes.count(r.nextType) || r.next < 0 ||
			    r.next >= int(rows.size()) || rows[r.next].type != r.nextType)
				throw std::runtime_error(
				    "Native transition must target a rule with its destination animation state");
			for (int k = 0; k < 3; ++k) {
				const int op = r.opcode[k], arg = r.argument[k];
				const bool actor =
				    (op == 0x30 || op == 0x31 || op == 0x4a || op == 0x58 || op == 0x59) &&
				    opcodes.count(op) && arg >= 0 && arg < int(n.actors.size()) &&
				    ((op != 0x30 && op != 0x4a) || arg < 255);
				const bool scalar =
				    op == 0 || op == 0x3e || op == 0x55 || op == 0x56 ||
				    (opcodes.count(op) &&
					 (op == 0x22 || (op >= 0x23 && op <= 0x26) || (op >= 0x38 && op <= 0x3b)) &&
					 arg >= 0 && arg <= 255);
				if (!actor && !scalar && !operands.count({op, arg}))
					throw std::runtime_error("Uncertified native operand; use this behavior's "
					                         "original vocabulary or a typed dependency port");
			}
		}
	}
	const auto shared = nativeSharedActors(base);
	for (size_t i = 0; i < n.actors.size(); ++i) {
		const auto &a = n.actors[i];
		const int source = nativeActorSource(a);
		if (a.id != int(i) || source < 0 || source >= int(base.actors.size()) ||
		    (i < base.actors.size() && source != int(i)) ||
		    (i >= base.actors.size() && shared.count(source)))
			throw std::runtime_error(
			    "Invalid actor template or attempted copy of a shared campaign service");
		const auto &b = base.actors[source];
		if (a.behavior < 0 || a.behavior >= int(n.programs.size()) ||
		    nativeProgramSource(n, a.behavior) != b.behavior || a.objectKind != b.objectKind)
			throw std::runtime_error(
			    "Actor behavior must derive from its original resource template");
		if (b.room >= 64 && (a.room != b.room || a.x != b.x || a.y != b.y || a.life != b.life ||
		                     a.facingLeft != b.facingLeft || a.type != b.type))
			throw std::runtime_error(
			    "Holding-list helpers retain their native spawn/inventory setup");
		if (a.data != b.data) {
			const auto ports = nativePorts(n, int(i)), originals = nativePorts(base, source);
			for (int d = 0; d < 4; ++d)
				if (a.data[d] != b.data[d]) {
					auto typed = [&](const auto &v) {
						return std::find_if(v.begin(), v.end(), [&](const auto &port) {
							return port.data == d && port.editable;
						});
					};
					const auto port = typed(ports), old = typed(originals);
					if (i == 0 || port == ports.end() || old == originals.end() ||
					    port->kind != old->kind || a.data[d] < 0 ||
					    a.data[d] >=
					        (port->kind == NativePortKind::Actor ? int(n.actors.size()) : 64))
						throw std::runtime_error("Only unambiguous actor/room ports may change; "
						                         "numeric and mixed-use data remain original");
				}
		}
		const auto &rows = n.programs[a.behavior].rules;
		if (std::none_of(rows.begin(), rows.end(), [&](const auto &r) { return r.type == a.type; }))
			throw std::runtime_error("Actor initial state is missing from its behavior");
		for (const auto &r : rows)
			for (int k = 0; k < 3; ++k) {
				const int op = r.opcode[k], arg = r.argument[k];
				if (op == 0x30 && arg == int(i))
					throw std::runtime_error("An actor cannot own itself in inventory");
			}
	}
	// The indexed native renderer has one monster sprite/palette bank at a time.
	// Preserve original scripted arrangements, but reject newly mixed banks in
	// one room rather than drawing actors using the last-loaded bank's pixels.
	auto banks = [&](const NativeCampaign &campaign) {
		std::array<std::set<int>, 64> result;
		for (const auto &a : campaign.actors) {
			if (a.room >= 64)
				continue;
			const int behavior = nativeProgramSource(campaign, a.behavior);
			if (a.objectKind != 10 && behavior != 0x49)
				continue;
			const uint8_t *list = Game::_monsterListLevels[campaign.level];
			while (*list != 0xff && *list != behavior)
				list += 2;
			if (*list != 0xff)
				result[a.room].insert(list[1]);
		}
		return result;
	};
	const auto oldBanks = banks(base), newBanks = banks(n);
	for (int room = 0; room < 64; ++room)
		if (newBanks[room].size() > 1 &&
		    !std::includes(oldBanks[room].begin(), oldBanks[room].end(), newBanks[room].begin(),
		                   newBanks[room].end()))
			throw std::runtime_error(
			    "Room " + std::to_string(room) +
			    " mixes native enemy sprite banks; place these enemy families in separate rooms");
}
void installNativeLogic(Game &g, const NativeCampaign &n) {
	if (n.programs.empty())
		return;
	std::vector<ObjectNode *> compiled;
	auto release = [&] {
		for (auto *p : compiled)
			if (p) {
				std::free(p->objects);
				std::free(p);
			}
	};
	try {
		for (const auto &program : n.programs) {
			auto *p = static_cast<ObjectNode *>(std::calloc(1, sizeof(ObjectNode)));
			if (!p)
				throw std::bad_alloc();
			compiled.push_back(p);
			p->num_objects = program.rules.size();
			if (program.rules.empty())
				continue;
			p->objects = static_cast<Object *>(std::calloc(program.rules.size(), sizeof(Object)));
			if (!p->objects)
				throw std::bad_alloc();
			for (size_t i = 0; i < program.rules.size(); ++i) {
				const auto &r = program.rules[i];
				auto &o = p->objects[i];
				o.type = r.type;
				o.init_obj_type = r.nextType;
				o.init_obj_number = r.next;
				o.dx = r.dx;
				o.dy = r.dy;
				o.flags = r.flags;
				o.opcode1 = r.opcode[0];
				o.opcode2 = r.opcode[1];
				o.opcode3 = r.opcode[2];
				o.opcode_arg1 = r.argument[0];
				o.opcode_arg2 = r.argument[1];
				o.opcode_arg3 = r.argument[2];
			}
		}
	} catch (...) {
		release();
		throw;
	}
	// Resource owns malloc-backed tables. Allocate the whole replacement before
	// releasing anything, including the original contiguous alias groups.
	g._res.free_OBJ();
	g._res._numObjectNodes = compiled.size();
	for (size_t i = 0; i < compiled.size(); ++i)
		g._res._objectNodesMap[i] = compiled[i];
}
} // namespace editor
