// SPDX-License-Identifier: GPL-3.0-or-later
// GUI settings, modal isolation and native setup use the real input path.
void inputSettings(int frame, Workspace &app) {
	if (app.library.games.empty())
		return;
	auto &io = ImGui::GetIO();
	auto click = [&](int start, const char *id) {
		if (frame == start) {
			pointer = app.testPoints.at(id);
			io.AddMousePosEvent(pointer.x, pointer.y);
			io.AddMouseButtonEvent(0, true);
		}
		if (frame == start + 1)
			io.AddMouseButtonEvent(0, false);
	};
	auto key = [&](int start, ImGuiKey code, bool ctrl = false) {
		if (frame == start) {
			io.AddKeyEvent(ImGuiMod_Ctrl, ctrl);
			io.AddKeyEvent(code, true);
		}
		if (frame == start + 1) {
			io.AddKeyEvent(code, false);
			io.AddKeyEvent(ImGuiMod_Ctrl, false);
		}
	};
	click(343, "editor-settings");
	click(347, "settings-start");
	click(351, "settings-markers");
	key(356, ImGuiKey_F5);
	key(360, ImGuiKey_Escape);
	key(364, ImGuiKey_F5);
	key(372, ImGuiKey_Escape);
	if (frame == 374)
		app.setWorkflow(Workflow::Placement);
	key(376, ImGuiKey_F5, true);
	key(384, ImGuiKey_Escape);
	if (frame == 388) {
		app.saveAs(app.path);
		app.settings = {};
		app.open(app.path);
	}
	click(390, "editor-settings");
	click(394, "settings-native");
	click(398, "picker-accept");
	click(406, "editor-settings");
	click(410, "settings-markers");
	key(414, ImGuiKey_Escape);
	if (frame == 416) {
		app.setWorkflow(Workflow::Placement);
		app.nativeActor = 0;
		playtestDocument = serialize(app.p);
	}
	if (frame == 418) {
		const auto origin = app.testPoints.at("room-origin");
		pointer = {origin.x + 80 * app.zoom, origin.y + 100 * app.zoom};
		io.AddMousePosEvent(pointer.x, pointer.y);
		io.AddMouseButtonEvent(0, true);
	}
	if (frame == 420)
		pointer.x += 60;
	if (frame == 422)
		io.AddMouseButtonEvent(0, false);
	click(429, "editor-settings");
	key(436, ImGuiKey_Escape);
}
void checkSettings(int frame, Workspace &app) {
	if (app.library.games.empty())
		return;
	if (frame == 355) {
		require(app.settingsOpen && app.settings.authoredStart && !app.settings.nativeMarkers,
		        "Settings widgets update start mode and markers");
		const auto saved = loadEditorSettings(app.path.parent_path());
		require(saved.authoredStart && !saved.nativeMarkers && serialize(app.p) == playtestDocument,
		        "Settings persist separately from campaign content");
		app.dropFile(app.path);
		require(app.settingsOpen && serialize(app.p) == playtestDocument,
		        "Settings modal blocks project drops");
		screenshot(app.renderer, (output / "editor-settings.bmp").u8string());
	}
	if (frame == 359)
		require(!app.scene && app.settingsOpen, "Settings modal blocks F5");
	if (frame == 368)
		require(app.scene && app.scene->project().spawn.room == app.p.spawn.room &&
		            app.scene->project().spawn.x == app.p.spawn.x,
		        "F5 respects authored-start preference");
	if (frame == 380)
		require(app.scene && app.scene->project().spawn.room == playtestRoom,
		        "Ctrl+F5 overrides saved preference with current room");
	if (frame == 389)
		require(app.settings.authoredStart && !app.settings.nativeMarkers,
		        "Open restores folder settings");
	if (frame == 396)
		require(!app.settingsOpen && app.picker.open,
		        "Native setup closes Settings before opening folder picker");
	if (frame == 402) {
		require(app.p.native.enabled && app.p.native.actors.size() == 107 &&
		            app.settings.authoredStart &&
		            loadEditorSettings(app.path.parent_path()).authoredStart,
		        "GUI native setup enables campaign play and retains all actors");
		for (int i = 1; i <= 7; ++i)
			require(std::filesystem::exists(app.path.parent_path() /
			                                ("level-" + std::to_string(i) + ".r2project.json")),
			        "GUI native setup omitted a level");
	}
	if (frame == 426)
		require(!app.settings.nativeMarkers && serialize(app.p) == playtestDocument,
		        "Hidden native markers must not allow invisible actor dragging");
	if (frame == 433) {
		require(app.settingsOpen, "Native settings panel opens");
		screenshot(app.renderer, (output / "native-settings.bmp").u8string());
	}
}
