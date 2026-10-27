// SPDX-License-Identifier: GPL-3.0-or-later
void inputStatePlayback(int frame, Workspace &app) {
	auto &io = ImGui::GetIO();
	if (frame == 524) {
		app.p = makeProject();
		app.room = 0;
		auto m = makeGraph(app.p, 5, app.p.spawn);
		m.name = "Airlock state machine";
		for (auto kind : {NodeKind::State, NodeKind::State, NodeKind::Button, NodeKind::Dialogue})
			m.nodes.push_back(makeNode(m, kind, app.p.spawn));
		m.nodes[0].name = "Locked";
		m.nodes[0].initiallyActive = true;
		m.nodes[1].name = "Authorized";
		m.nodes[2].name = "Access switch";
		m.nodes[3].name = "Welcome dialogue";
		m.nodes[0].graphX = 24;
		m.nodes[0].graphY = 24;
		m.nodes[1].graphX = 300;
		m.nodes[1].graphY = 24;
		m.nodes[2].graphX = 24;
		m.nodes[2].graphY = 310;
		m.nodes[3].graphX = 300;
		m.nodes[3].graphY = 310;
		m.wires = {{m.nodes[2].id, m.nodes[0].id, Signal::Used, Input::Progress},
		           {m.nodes[0].id, m.nodes[1].id, Signal::Completed, Input::Activate},
		           {m.nodes[1].id, m.nodes[3].id, Signal::Entered, Input::Activate}};
		app.p.modules = {m};
		app.selected = 0;
		app.openGraph();
		app.graphFrameAll = true;
	}
	auto click = [&](int at, const char *id) {
		if (frame == at) {
			pointer = app.testPoints.at(id);
			io.AddMousePosEvent(pointer.x, pointer.y);
			io.AddMouseButtonEvent(0, true);
		}
		if (frame == at + 1)
			io.AddMouseButtonEvent(0, false);
	};
	click(528, "graph-debug-start");
	if (frame == 532) {
		GraphMachine machine(app.p.modules[0], app.graphDebugState);
		machine.automatic = false;
		machine.use(2);
	}
	click(536, "graph-debug-step");
	click(540, "graph-debug-step");
	click(544, "graph-debug-step");
	if (frame == 552 && !app.library.games.empty()) {
		OriginalLevel original(app.library.games.front(), &app.nativePreview, 0);
		app.p = original.createProject(true);
		app.room = app.p.native.actors[0].room;
		app.nativeActor = 0;
		app.nativePuzzle = 0;
		app.nativeRule = 0;
		app.nativeRulePage = 0;
		app.nativeExtensionGraph = false;
		app.nativeConnections = false;
		app.graphFrameAll = true;
		app.setWorkflow(Workflow::Module);
	}
	if (!app.library.games.empty())
		click(556, "native-rule-enabled");
	if (frame == 562 && !app.library.games.empty()) {
		OriginalLevel original(app.library.games.front(), &app.nativePreview, 0);
		app.p = original.createProject(true);
		app.history = History{};
		bool found = false;
		for (int i = 0; i < int(app.p.native.puzzles.size()); ++i) {
			const auto &group = app.p.native.puzzles[i];
			if (group.actors.size() < 2 || group.actors.size() > 8 ||
			    std::find(group.actors.begin(), group.actors.end(), 0) != group.actors.end())
				continue;
			try {
				auto copy = app.p;
				copyNativePuzzle(copy, i, {}, 0, 0, app.p.native);
				app.nativePuzzle = i;
				app.nativeActor = group.actors.front();
				found = true;
				break;
			} catch (const std::exception &) {
			}
		}
		require(found, "No native puzzle available for UI copy test");
		app.nativeConnections = true;
		app.graphFrameAll = true;
	}
	if (!app.library.games.empty()) {
		click(566, "native-place-puzzle");
		if (frame == 569) {
			app.nativePlacementX = 0;
			app.nativePlacementY = 0;
		}
		click(572, "native-copy-bundle");
	}
}
void checkStatePlayback(int frame, Workspace &app) {
	if (frame == 531)
		require(app.graphDebugOpen && app.graphDebugState.nodes[0].active,
		        "Graph Start button did not enter initial state");
	if (frame == 539)
		require(app.graphDebugState.nodes[0].completed && !app.graphDebugState.nodes[1].active,
		        "Step should deliver just one wire");
	if (frame == 543)
		require(!app.graphDebugState.nodes[0].active && app.graphDebugState.nodes[1].active &&
		            !app.graphDebugState.nodes[3].active,
		        "State transition playback");
	if (frame == 547) {
		require(app.graphDebugState.nodes[3].active,
		        "Node playback did not reach connected dialogue");
		app.graphNode = 3;
	}
	if (frame == 550) {
		screenshot(app.renderer, (output / "state-machine-playback.bmp").u8string());

	}
	if (frame == 559 && !app.library.games.empty()) {
		require(!app.p.native.programs[app.p.native.actors[app.nativeActor].behavior]
		             .rules[app.nativeRule]
		             .enabled,
		        "Native rule inspector did not edit the original event node");
		screenshot(app.renderer, (output / "native-rule-editor.bmp").u8string());
	}
	if (frame == 569 && !app.library.games.empty()) {
		require(app.nativePlacementOpen, "Native placement dialog did not open");
		screenshot(app.renderer, (output / "native-puzzle-placement.bmp").u8string());
	}
	if (frame == 575 && !app.library.games.empty()) {
		require(!app.nativePlacementOpen, "Native puzzle copy failed in UI");
		const auto &group = app.p.native.puzzles[app.nativePuzzle];
		require(app.p.native.actors[group.actors.front()].source >= 0,
		        "UI copy did not allocate new native actors");
		app.graphFrameAll = true;
	}
	if (frame == 578 && !app.library.games.empty()) {
		screenshot(app.renderer, (output / "native-puzzle-connections.bmp").u8string());
		const auto count = app.p.native.actors.size();
		require(app.history.undo(app.p) && app.p.native.actors.size() < count,
		        "Native puzzle copy cannot be undone");
		require(app.history.redo(app.p) && app.p.native.actors.size() == count,
		        "Native puzzle copy cannot be redone");

	}
}
