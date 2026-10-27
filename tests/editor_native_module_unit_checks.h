// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor/native_logic.h"
// Authored miniature fixture: exercises native module authoring in public CI
// without shipping any original game scripts or assets.
static void nativeModuleUnitChecks() {
	using namespace editor;
	auto p = makeProject();
	p.rooms.clear();
	for (int i = 0; i < 64; ++i) {
		auto &r = addRoom(p, i % 8, i / 8);
		r.id = "original-room-" + std::to_string(i);
	}
	p.spawn = {p.rooms[0].id, 32, 214};
	p.native.enabled = true;
	p.native.level = 0;
	p.native.edition = 0;
	p.native.fingerprint = "12345678";
	auto &n = p.native;
	n.actors = {{0, 0, 32, 214, 5, false, 1, 0, 1, {0, 0, 0, 0}},
	            {1, 0, 64, 142, 1, false, 100, 1, 0, {2, 43, 2, 13}},
	            {2, 1, 112, 142, 1, false, 200, 2, 0, {1, 0, 2, 0}}};
	auto row = [](int state, int op, int arg) {
		NativeRule r;
		r.type = r.nextType = state;
		r.opcode[2] = op;
		r.argument[2] = arg;
		return r;
	};
	n.programs = {{{row(1, 0, 0)}},
	              {{row(100, 0x23, 9), row(100, 0x44, 1), row(100, 0x65, 2), row(100, 0x31, 2)}},
	              {{row(200, 0x38, 9), row(200, 0x57, 1)}}};
	n.puzzles = {{"Shared player", {0}}, {"Authored switch + door", {1, 2}}};
	const auto base = n;
	compileCollision(p);
	validateNativeLogic(n, base);
	auto copy = p;
	const int added = copyNativePuzzle(copy, 1, {{0, 2}, {1, 3}}, 8, 0, base);
	check(copy.native.puzzles[added].actors == std::vector<int>({3, 4}),
	      "Synthetic puzzle lost members");
	const auto &switchCopy = copy.native.actors[3], &doorCopy = copy.native.actors[4];
	check(switchCopy.data == std::array<int, 4>{4, 43, 4, 13},
	      "Actor links confused with numeric fields");
	check(doorCopy.data == std::array<int, 4>{3, 2, 2, 0},
	      "Room remap or scalar preservation failed");
	check(switchCopy.room == 2 && switchCopy.x == 72 && doorCopy.room == 3 && doorCopy.x == 120,
	      "Bundle screen placement failed");
	check(copy.native.programs[switchCopy.behavior].rules[3].argument[2] == 4,
	      "Literal actor reference not relocated");
	check(copy.native.programs[1].rules[3].argument[2] == 2, "Copy rewrote shared original rules");
	check(serialize(deserialize(serialize(copy))) == serialize(copy),
	      "Provenance round trip failed");
	auto moved = p;
	moveNativePuzzle(moved, 1, {{0, 2}, {1, 3}}, 8, 0, base);
	check(moved.native.actors[1].data == base.actors[1].data && moved.native.actors[2].data[1] == 2,
	      "Move changed actor IDs or lost room destination");
	// Independent rules still retain their original resource identity.
	auto isolated = p;
	isolateNativeBehavior(isolated, 1, base);
	auto literal = nativePorts(copy.native, 3).back();
	const int privateBehavior = switchCopy.behavior;
	reconnectNativePort(copy, literal, 2, base);
	check(copy.native.programs[privateBehavior].rules[3].argument[2] == 2,
	      "Literal port edit failed");
	auto invalid = p;
	invalid.native.actors[1].data[1] = 2;
	rejects([&] { validateNativeLogic(invalid.native, base); },
	        "Counter reinterpreted as an actor link");
	auto mixed = p;
	mixed.native.programs[1].rules.push_back(row(100, 0x58, 0));
	const auto before = serialize(mixed);
	const auto mixedBase = mixed.native;
	rejects([&] { copyNativePuzzle(mixed, 1, {}, 0, 0, mixedBase); },
	        "Mixed actor/value field relocated");
	check(serialize(mixed) == before, "Failed mixed-use copy partially mutated project");
	auto baseCycle = p;
	baseCycle.native.programs[1].rules.push_back(row(100, 0x30, 0));
	auto cycle = baseCycle;
	cycle.native.programs[1].rules.back().argument[2] = 1;
	rejects([&] { validateNativeLogic(cycle.native, baseCycle.native); },
	        "Self inventory owner accepted");
	// Authored extensions travel with their complete ownership bundle.
	auto extended = p;
	auto m = makeGraph(extended, 5, p.spawn);
	auto actor = makeNode(m, NodeKind::Actor, p.spawn);
	actor.nativeActor = 1;
	actor.messageFrom = 2;
	m.nodes.push_back(actor);
	extended.modules.push_back(m);
	copyNativePuzzle(extended, 1, {}, 0, 0, base);
	check(extended.modules.size() == 2 && extended.modules[1].id != m.id &&
	          extended.modules[1].nodes[0].nativeActor == 3 &&
	          extended.modules[1].nodes[0].messageFrom == 4,
	      "Authored native binding graph not relocated");
	// Original actor IDs cannot be swapped even when their visible data is similar.
	invalid = p;
	invalid.native.actors[1].source = 2;
	rejects([&] { validateNativeLogic(invalid.native, base); },
	        "Original template identity could be replaced");
	auto removed = p.native;
	removed.actors[1].removed = true;
	removed.actors[2].removed = true;
	validateNativeLogic(removed, base);
	removed.actors[0].removed = true;
	rejects([&] { validateNativeLogic(removed, base); }, "Removed shared player accepted");
	auto notes = p;
	notes.native.programs.push_back(notes.native.programs[1]);
	const auto revision = nativeRevision(notes.native);
	notes.native.actors[1].comments = "Switch actor";
	notes.native.puzzles[1].comments = "Door puzzle";
	notes.native.programs[1].rules[0].comments = "Activation rule";
	check(nativeRevision(notes.native) == revision,
	      "Native designer notes invalidated runtime identity");
	check(deserialize(serialize(notes)).native.actors[1].comments == "Switch actor",
	      "Native notes lost in roundtrip");
}
