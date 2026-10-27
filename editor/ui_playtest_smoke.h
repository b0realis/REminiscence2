// SPDX-License-Identifier: GPL-3.0-or-later
// Actual-widget checks for session-only markers and room/graph playtest starts.
void inputPlaytest(int frame, Workspace &app) {
	if (app.library.games.empty())
		return;
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
	auto key = [&](int start, ImGuiKey code, bool shift = false) {
		if (frame == start) {
			io.AddKeyEvent(ImGuiMod_Shift, shift);
			io.AddKeyEvent(code, true);
		}
		if (frame == start + 1) {
			io.AddKeyEvent(code, false);
			io.AddKeyEvent(ImGuiMod_Shift, false);
		}
	};
	if (frame == 282) {
		app.open(output / "project.r2project.json");
		playtestRoom = addRoom(app.p, 127, 127).id;
		app.room = int(app.p.rooms.size()) - 1;
		app.p.rooms.back().name = "Playtest this room";
		app.p.rooms.back().art = app.p.rooms[0].art;
		app.zoom = 2;
		app.resetCanvasScroll = true;
		playtestDocument = serialize(app.p);
	}
	if (frame == 286 || frame == 287)
		click(286, app.testPoints.at("test-marker"));
	if (frame == 290 || frame == 291) {
		const auto origin = app.testPoints.at("room-origin");
		click(290, {origin.x + 128 * app.zoom, origin.y + 210 * app.zoom});
	}
	key(296, ImGuiKey_F5);
	key(304, ImGuiKey_Escape);
	if (frame == 312) {
		app.p.modules.push_back(makeGraph(app.p, 0, {app.p.rooms[0].id, 192, 216}));
		app.selected = int(app.p.modules.size()) - 1;
		app.openGraph();
		app.graphNode = 1;
		playtestDocument = serialize(app.p);
	}
	if (frame == 316 || frame == 317)
		click(316, app.testPoints.at("play-here"));
	key(324, ImGuiKey_Escape);
	key(328, ImGuiKey_F5, true);
	key(336, ImGuiKey_Escape);
}
void checkPlaytest(int frame, Workspace &app) {
	if (app.library.games.empty())
		return;
	if (frame == 294) {
		require(app.testStarts.count(playtestRoom) && app.testStarts.at(playtestRoom).at.x == 120 &&
		            app.testStarts.at(playtestRoom).at.y == 214,
		        "TEST marker widget uses the clicked room position");
		require(serialize(app.p) == playtestDocument, "TEST marker changed the document");
		screenshot(app.renderer, (output / "play-here.bmp").u8string());
	}
	if (frame == 300) {
		require(app.scene && !app.paused && app.scene->project().spawn.room == playtestRoom &&
		            app.scene->project().spawn.x == 120 && app.scene->project().spawn.y == 214,
		        "F5 starts immediately at the current room TEST marker");
		require(serialize(app.p) == playtestDocument, "Room test changed the document");
	}
	if (frame == 308)
		require(!app.scene && app.p.rooms[app.room].id == playtestRoom &&
		            app.testStarts.count(playtestRoom),
		        "Stop restores room selection and TEST marker");
	if (frame == 320)
		require(app.scene && app.scene->project().spawn.room == app.p.rooms[0].id &&
		            app.p.rooms[app.room].id == playtestRoom &&
		            serialize(app.p) == playtestDocument,
		        "PLAY HERE uses the selected graph node's room, not the canvas room");
	if (frame == 332)
		require(app.scene && app.scene->project().spawn.room == app.p.spawn.room &&
		            app.scene->project().spawn.x == app.p.spawn.x &&
		            app.scene->project().spawn.y == app.p.spawn.y,
		        "Shift+F5 preserves the authored campaign start");
	if (frame == 340) {
		require(!app.scene && serialize(app.p) == playtestDocument,
		        "Repeated playtests leave the authored document unchanged");
	}
}
