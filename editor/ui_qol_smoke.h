// SPDX-License-Identifier: GPL-3.0-or-later
// Continuation of the real-widget regression, isolated from example layout.
void inputExtras(int frame, Workspace &app) {
	auto &io = ImGui::GetIO();
	auto click = [&](int start, ImVec2 at) {
		if (frame == start) {
			pointer = at;
			io.AddMousePosEvent(at.x, at.y);
			io.AddMouseButtonEvent(0, true);
		}
		if (frame == start + 1)
			io.AddMouseButtonEvent(0, false);
	};
	auto key = [&](int start, ImGuiKey code) {
		if (frame == start) {
			io.AddKeyEvent(ImGuiMod_Super, true);
			io.AddKeyEvent(code, true);
		}
		if (frame == start + 1) {
			io.AddKeyEvent(code, false);
			io.AddKeyEvent(ImGuiMod_Super, false);
		}
	};
	if (frame == 180) {
		app.scene.reset();
		app.reference.reset();
		app.paletteSearch.Clear();
		app.p = makeProject();
		app.room = 0;
		app.selected = -1;
		app.selection.clear();
		app.setWorkflow(Workflow::Placement);
		app.guidelines = app.world = false;
		app.tool = Tool::Select;
		app.zoom = 2;
		app.resetCanvasScroll = true;
		const auto room = app.p.rooms[0].id;
		app.p.modules.push_back(makeModule(app.p, Kind::Prop, {room, 64, 144}));
		app.p.modules.push_back(makeModule(app.p, Kind::Prop, {room, 144, 144}));
	}
	if (frame >= 184 && frame <= 210) {
		const auto origin = app.testPoints.at("room-origin");
		auto at = [&](int x, int y) {
			return ImVec2(origin.x + x * app.zoom, origin.y + y * app.zoom);
		};
		click(184, at(72, 136));
		if (frame == 187)
			io.AddKeyEvent(ImGuiMod_Super, true);
		click(188, at(152, 136));
		if (frame == 190)
			io.AddKeyEvent(ImGuiMod_Super, false);
		key(194, ImGuiKey_C);
		key(198, ImGuiKey_V);
		if (frame == 202)
			app.locked[1] = true;
		if (frame == 203)
			io.AddKeyEvent(ImGuiKey_Delete, true);
		if (frame == 204)
			io.AddKeyEvent(ImGuiKey_Delete, false);
		if (frame == 206) {
			pointer = at(72, 136);
			io.AddMousePosEvent(pointer.x, pointer.y);
			io.AddMouseButtonEvent(0, true);
		}
		if (frame == 208)
			pointer.x += 48;
		if (frame == 209)
			io.AddMouseButtonEvent(0, false);
	}
	if (frame == 220) {
		app.locked[1] = false;
		app.p.modules.push_back(makeGraph(app.p, 0, {app.p.rooms[0].id, 192, 216}));
		app.selected = int(app.p.modules.size()) - 1;
		app.selection = {app.p.modules.back().id};
		app.openGraph();
		app.graphZoom = .5f;
		app.graphPan = {20, 15};
		graphX = app.p.modules.back().nodes[1].graphX;
		graphY = app.p.modules.back().nodes[1].graphY;
	}
	if (frame == 224) {
		pointer = app.testPoints.at("header-node-2");
		io.AddMousePosEvent(pointer.x, pointer.y);
		io.AddMouseButtonEvent(0, true);
	}
	if (frame == 226) {
		pointer.x += 40;
		pointer.y += 30;
	}
	if (frame == 227)
		io.AddMouseButtonEvent(0, false);
	if (frame == 230) {
		previousPan = app.graphPan;
		io.AddMouseButtonEvent(2, true);
	}
	if (frame == 232) {
		pointer.x += 50;
		pointer.y += 30;
	}
	if (frame == 233)
		io.AddMouseButtonEvent(2, false);
	if (frame == 238)
		io.AddMouseWheelEvent(0, 1);
	if (frame == 242) {
		app.p.modules.back().nodes[0].graphX = 400;
		app.p.modules.back().nodes[0].graphY = 40;
		app.p.modules.back().nodes[1].graphX = 40;
		app.p.modules.back().nodes[1].graphY = 120;
		app.status = "Graph navigation: wheel zoom, middle-drag pan, Frame all";
	}
	if (frame == 245 || frame == 246)
		click(245, app.testPoints.at("graph-frame"));
	if (frame == 250) {
		app.p.modules.back().nodes[0].at.room = "missing-room";
		app.validateProject();
		for (int i = 0; i < int(app.validationResults.size()); ++i)
			if (app.validationResults[i].node == app.p.modules.back().nodes[0].id) {
				focusIndex = i;
				break;
			}
		require(focusIndex >= 0, "Graph validation provides focus target");
		app.selected = -1;
		app.setWorkflow(Workflow::Placement);
	}
	if (frame == 254 || frame == 255)
		click(254, app.testPoints.at("issue-" + std::to_string(focusIndex)));
	if (frame == 259) {
		app.validationOpen = false;
		app.p.modules.back().nodes[0].at.room = app.p.rooms[0].id;
	}
	if (!app.library.games.empty()) {
		if (frame == 263)
			app.openOriginal();
		if (frame == 268 || frame == 269)
			click(268, app.testPoints.at("native-import"));
		if (frame == 272 || frame == 273)
			click(272, app.testPoints.at("picker-accept"));
	}
}
void checkExtras(int frame, Workspace &app) {
	if (frame == 192)
		require(app.selection.size() == 2, "Ctrl-click multi-selection");
	if (frame == 201)
		require(app.p.modules.size() == 4 && app.selection.size() == 2,
		        "Keyboard copy/paste complete selection");
	if (frame == 211)
		require(app.p.modules.size() == 4 && app.p.modules[0].a.x == 64,
		        "Layer lock prevents deletion and dragging");
	if (frame == 228)
		require(app.p.modules.back().nodes[1].graphX == graphX + 80 &&
		            app.p.modules.back().nodes[1].graphY == graphY + 60,
		        "Zoom-correct node dragging");
	if (frame == 235)
		require(app.graphPan.x == previousPan.x + 50 && app.graphPan.y == previousPan.y + 30,
		        "Middle-button graph pan");
	if (frame == 241)
		require(app.graphZoom > .5f, "Graph wheel zoom");
	if (frame == 248) {
		require(!app.graphFrameAll && app.graphPan.x == 12, "Frame all button");
		screenshot(app.renderer, (output / "graph-navigation.bmp").u8string());
	}
	if (frame == 258)
		require(app.selected == int(app.p.modules.size()) - 1 && app.graphView &&
		            app.graphNode == 0,
		        "Clickable issue focuses graph node");

	if (frame == 278 && !app.library.games.empty()) {
		require(app.p.native.enabled && app.p.native.actors.size() == 107 && app.room == 27 &&
		            !app.reference,
		        "Full native campaign import UI");
		for (int i = 1; i <= 7; ++i)
			require(std::filesystem::exists(app.path.parent_path() /
			                                ("level-" + std::to_string(i) + ".r2project.json")),
			        "Missing native sibling level");
		screenshot(app.renderer, (output / "native-campaign.bmp").u8string());
	}
}
