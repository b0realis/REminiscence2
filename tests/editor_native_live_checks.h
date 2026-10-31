// SPDX-License-Identifier: GPL-3.0-or-later
// Real edition data, every chapter: prove hot rule replacement preserves live
// cursors, inventory and queued messages, and rejected edits do not partially land.
static void nativeLiveChecks(editor::NativeSession &session, Game &g, const editor::Project &p) {
	using namespace editor;
	session.apply(g, p, true);
	g._pgeLive[0].counter_value = 123;
	g.pge_sendMessage(1, 0, 7);
	const auto *message = g._pge_messagesTable[0];
	check(message && message->msg_num == 7, "Fixture did not queue a native message");
	const auto before = state(g);
	const auto actors = std::vector<LivePGE>(g._pgeLive, g._pgeLive + g._res._pgeNum);
	// A borrowed extension scene must retain progress and adopt the new definition.
	const auto *overlay = &session.campaignScene(g);
	auto changed = p;
	const int behavior = p.native.actors[0].behavior;
	auto &rule = changed.native.programs[behavior].rules[0];
	rule.opcode = {0, 0, 0x55};
	rule.argument = {0, 0, 7};
	rule.flags = 0;
	rule.dx = rule.dy = 0;
	rule.enabled = false;
	session.apply(g, changed, false);
	check(state(g) == before &&
	          std::memcmp(actors.data(), g._pgeLive, actors.size() * sizeof(LivePGE)) == 0,
	      "Live rule update reset native actor/inventory/counter state");
	check(g._pge_messagesTable[0] == message && message->msg_num == 7,
	      "Live rule update dropped queued native messages");
	check(&session.campaignScene(g) == overlay &&
	          nativeRevision(overlay->project().native) == nativeRevision(changed.native),
	      "Live update replaced extension state or retained its stale definition");
	const auto *compiled = g._res._objectNodesMap[behavior]->objects;
	const int priorLife = g._pgeLive[0].life;
	check(g.pge_execute(&g._pgeLive[0], &g._res._pgeInit[0], compiled) == 0 &&
	          g._pgeLive[0].life == priorLife,
	      "Disabled hot-updated rule executed");
	rule.enabled = true;
	session.apply(g, changed, false);
	compiled = g._res._objectNodesMap[behavior]->objects;
	check(g.pge_execute(&g._pgeLive[0], &g._res._pgeInit[0], compiled) == 0xffff &&
	          g._pgeLive[0].life == 7,
	      "Hot-updated action did not execute without restart");

	session.apply(g, p, true);
	changed = p;
	isolateNativeBehavior(changed, 1, p.native);
	const auto beforePrivate = state(g);
	session.apply(g, changed, false);
	check(state(g) == beforePrivate &&
	          g._res._pgeInit[1].obj_node_number == changed.native.actors[1].behavior,
	      "Live private behavior isolation reset the actor or failed to install");
	auto appended = changed;
	auto &rows = appended.native.programs[appended.native.actors[1].behavior].rules;
	rows.push_back(rows.front());
	session.apply(g, appended, false);
	check(state(g) == beforePrivate, "Appending a stable rule reset live state");

	session.apply(g, p, true);
	bool rewired = false;
	for (size_t id = 1; !rewired && id < p.native.actors.size(); ++id)
		for (const auto &port : nativePorts(p.native, int(id))) {
			if (!port.editable || port.data < 0)
				continue;
			auto wired = p;
			const int target = port.target == 0 ? 1 : 0;
			try {
				reconnectNativePort(wired, port, target, p.native);
			} catch (const std::exception &) {
				continue;
			}
			const auto beforeWire = state(g);
			session.apply(g, wired, false);
			check(state(g) == beforeWire && g._res._pgeInit[id].data[port.data] == target,
			      "Live dependency rewiring reset runtime state or missed target");
			rewired = true;
			break;
		}
	check(rewired, "Fixture did not exercise a live typed data port");

	session.apply(g, p, true);
	changed = p;
	changed.native.actors[0].life += 2;
	g._pgeLive[0].counter_value = 321;
	session.apply(g, changed, false);
	check(g._pgeLive[0].life == changed.native.actors[0].life && g._pgeLive[0].counter_value == 321,
	      "Untouched live health did not update independently of counters");
	--g._pgeLive[0].life;
	auto conflict = changed;
	++conflict.native.actors[0].life;
	conflict.native.programs[behavior].rules[0].argument = {0, 0, 9};
	conflict.native.programs[behavior].rules[0].opcode = {0, 0, 0x55};
	const auto failedState = state(g);
	const auto *oldTable = g._res._objectNodesMap[behavior];
	rejects([&] { session.apply(g, conflict, false); },
	        "Live damage was overwritten by initial health edit");
	check(state(g) == failedState && oldTable == g._res._objectNodesMap[behavior] &&
	          serialize(session.project()) == serialize(changed),
	      "Rejected transaction partly installed rules/health");
	session.apply(g, p, true);
}
