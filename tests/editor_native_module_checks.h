// SPDX-License-Identifier: GPL-3.0-or-later
// Runs against every level of each installed edition; no original data in Git.
static int nativeModuleChecks(const editor::Project &p) {
	using namespace editor;
	const auto &base = p.native;
	const auto originalDocument = serialize(p);
	bool checkedMove = false;
	validateNativeLogic(base, base);
	std::set<int> owned;
	for (const auto &group : base.puzzles)
		for (int id : group.actors)
			check(owned.insert(id).second, "Actor has multiple puzzle owners");
	check(owned.size() == base.actors.size(), "Native module catalog lost actors");
	int copies = 0, blocked = 0, first = -1;
	const auto shared = nativeSharedActors(base);
	for (int group = 0; group < int(base.puzzles.size()); ++group) {
		bool service = false;
		for (int id : base.puzzles[group].actors)
			service |= shared.count(id) > 0;
		if (service)
			continue;
		const auto closure = nativePuzzleClosure(base, group);
		if (!checkedMove) {
			auto moved = p;
			moveNativePuzzle(moved, group, {}, 0, 0, base);
			check(serialize(moved) == originalDocument,
			      "Zero-offset module move changed campaign data");
			checkedMove = true;
		}
		auto copy = p;
		if (base.actors.size() + closure.size() > 255) {
			rejects([&] { copyNativePuzzle(copy, group, {}, 0, 0, base); },
			        "Native actor capacity overflow accepted");
			check(serialize(copy) == originalDocument, "Failed copy partially edited project");
			++blocked;
			continue;
		}
		const int added = copyNativePuzzle(copy, group, {}, 0, 0, base);
		++copies;
		if (first < 0)
			first = group;
		check(copy.native.puzzles[added].actors.size() == closure.size(),
		      "Copy dropped a hidden dependency");
		if (copies == 1)
			check(serialize(deserialize(serialize(copy))) == serialize(copy),
			      "Copied puzzle round trip");
		std::map<int, int> map;
		for (size_t i = 0; i < closure.size(); ++i)
			map[closure[i]] = copy.native.puzzles[added].actors[i];
		for (int old : closure) {
			const auto &a = copy.native.actors[map[old]], &b = base.actors[old];
			check(a.source == old && a.objectKind == b.objectKind && a.room == b.room,
			      "Actor template not preserved");
			check(a.behavior != b.behavior &&
			          nativeProgramSource(copy.native, a.behavior) == b.behavior,
			      "Copied behavior still aliases original");
			for (const auto &port : nativePorts(base, old)) {
				if (port.kind != NativePortKind::Actor)
					continue;
				const int target = map.count(port.target) ? map[port.target] : port.target;
				const int actual =
				    port.data >= 0
				        ? a.data[port.data]
				        : copy.native.programs[a.behavior].rules[port.rule].argument[port.operand];
				check(actual == target, "Copied dependency port points to the wrong instance");
			}
		}
		for (size_t i = 0; i < base.actors.size(); ++i)
			check(copy.native.actors[i].data == base.actors[i].data &&
			          copy.native.actors[i].behavior == base.actors[i].behavior,
			      "Copy changed original instance");
	}
	// Inspect and reconnect a real data-backed message port; its numerical
	// neighbor must not be mistaken for another actor or silently changed.
	bool rewired = false;
	for (const auto &a : base.actors)
		if (a.id > 0 && !shared.count(a.id) && !rewired)
			for (const auto &port : nativePorts(base, a.id)) {
				if (port.data < 0 || !port.editable || port.kind != NativePortKind::Actor)
					continue;
				auto copy = p;
				const auto before = serialize(copy);
				rejects([&] { reconnectNativePort(copy, port, 256, base); },
				        "Out-of-range actor port accepted");
				check(serialize(copy) == before, "Failed port edit changed document");
				reconnectNativePort(copy, port, port.target == 0 ? 1 : 0, base);
				check(copy.native.actors[a.id].data[port.data] != (a.data[port.data]),
				      "Port did not reconnect");
				for (int d = 0; d < 4; ++d)
					if (d != port.data)
						check(copy.native.actors[a.id].data[d] == a.data[d],
						      "Rewire changed scalar neighbor");
				rewired = true;
				break;
			}
	check(rewired, "No editable native data ports found");
	auto forged = p;
	forged.native.actors[0].source = 1;
	rejects([&] { validateNativeLogic(forged.native, base); }, "Original actor identity replaced");
	std::cout << "  Module audit: " << base.puzzles.size() << " groups, " << copies
	          << " copyable bundles, " << blocked << " capacity-limited\n";
	return first;
}
static void nativeModuleRuntimeChecks(editor::NativeSession &session, Game &g,
                                      const editor::Project &p, int group,
                                      const assets::Source &source,
                                      const std::filesystem::path &control) {
	using namespace editor;
	// Prefer an enemy bundle: changing only rule-table identity must not lose
	// the original graphics bank, even when its private slot is a different ID.
	for (int candidate = 0; candidate < int(p.native.puzzles.size()); ++candidate) {
		bool enemy = false;
		for (int id : p.native.puzzles[candidate].actors)
			enemy |= p.native.actors[id].objectKind == 10 && p.native.actors[id].room < 64;
		if (!enemy)
			continue;
		try {
			if (p.native.actors.size() + nativePuzzleClosure(p.native, candidate).size() <= 255) {
				group = candidate;
				break;
			}
		} catch (const std::exception &) {
		}
	}
	auto copy = p;
	if (group < 0)
		return;
	const std::vector<InitPGE> originals(g._res._pgeInit, g._res._pgeInit + g._res._pgeNum);
	const int added = copyNativePuzzle(copy, group, {}, 0, 0, p.native);
	auto before = state(g);
	rejects([&] { session.apply(g, copy, false); }, "Added actor applied without restart");
	check(state(g) == before, "Rejected live copy changed simulation");
	session.apply(g, copy, true);
	for (int id : copy.native.puzzles[added].actors) {
		const auto &a = copy.native.actors[id];
		auto expected = originals.at(a.source);
		expected.obj_node_number = a.behavior;
		for (int d = 0; d < 4; ++d)
			expected.data[d] = a.data[d];
		check(std::memcmp(&expected, &g._res._pgeInit[id], sizeof(InitPGE)) == 0,
		      "Clone lost native sprite/flags/text/collision metadata");
	}
	const auto oldRoom = g._currentRoom;
	for (int id : copy.native.puzzles[added].actors) {
		const auto &a = copy.native.actors[id];
		if (a.objectKind != 10 || a.room >= 64)
			continue;
		g._currentRoom = a.room;
		g._curMonsterFrame = 0xff;
		const int originalResult = g.loadMonsterSprites(&g._pgeLive[a.source]);
		const int originalBank = g._curMonsterFrame;
		g._curMonsterFrame = 0xff;
		const int copiedResult = g.loadMonsterSprites(&g._pgeLive[id]);
		check(copiedResult == originalResult && g._curMonsterFrame == originalBank,
		      "Private behavior changed native enemy sprite bank selection");
	}
	g._currentRoom = oldRoom;
	g.loadLevelMap();
	HeadlessSystem preview;
	OriginalLevel reference(source, &preview, p.native.level);
	check(reference.actorOverlay(copy, p.native.actors[0].room).pixels.size() == 256 * 224,
	      "Copied actors cannot render in editor");
	auto legacy = p;
	legacy.native.programs.clear();
	legacy.native.puzzles.clear();
	reference.actorOverlay(legacy, p.native.actors[0].room);
	check(nativeRevision(reference.createProject(true).native) == nativeRevision(p.native),
	      "Clone preview contaminated pristine campaign");
	// Exercise new native slots and real rule execution, then capture a delivered
	// message and step through the same simulation as the standalone native player.
	g._stub->_pi = {};
	for (int t = 0; t < 60; ++t) {
		g.pge_getInput();
		g.simulateActors();
	}
	const int target = copy.native.puzzles[added].actors.front();
	g._pgeLive[target].flags |= 4;
	g.pge_sendMessage(0, target, 42);
	NativeSession::publish(control, "resume", copy);
	session.boundary(g);
	const auto monitor = NativeSession::readMonitor(control, copy.native);
	check(monitor.actors.size() == copy.native.actors.size(),
	      "Native monitor omitted new actor slots");
	check(std::any_of(
	          monitor.messages.begin(), monitor.messages.end(),
	          [&](const auto &m) { return m.source == 0 && m.target == target && m.value == 42; }),
	      "Delivered native message absent from puzzle trace");
	check(NativeSession::readMonitor(control, p.native).actors.empty(),
	      "Monitor accepted stale project revision");
	NativeSession::publish(control, "step", copy);
	session.boundary(g);
	g.simulateActors();
	check(session.paused(), "Step native did not pause after one simulation frame");
	session.apply(g, p, true);
}
