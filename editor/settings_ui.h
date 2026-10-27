// SPDX-License-Identifier: GPL-3.0-or-later
// Included inside Workspace. Keep authoring preferences out of project content.
void settingsUI() {
	if (!settingsOpen || closing)
		return;
	ImGui::OpenPopup("Editor Settings");
	const auto screen = ImGui::GetIO().DisplaySize;
	ImGui::SetNextWindowSize({std::min(790.f, screen.x - 30), std::min(610.f, screen.y - 30)},
	                         ImGuiCond_Appearing);
	ImGui::SetNextWindowBgAlpha(1);
	bool createNative = false, openExisting = false;
	if (ImGui::BeginPopupModal("Editor Settings", nullptr, ImGuiWindowFlags_NoCollapse)) {
		ImGui::BeginChild("editor_settings_body", {0, -38});
		ImGui::SeparatorText("PROJECT MODE");
		ImGui::Text("Current project: %s", p.native.enabled ? "Native campaign" : "Custom modules");
		ImGui::TextWrapped(
		    p.native.enabled ? "Original actors and scripts run in the native game window. "
		                       "Edits are stored as overrides alongside your project."
			                 : "Custom modules use editable node graphs. Create a native campaign "
			                   "project to work with the original levels and their scripts.");
		if (!library.games.empty())
			ImGui::TextWrapped("Selected assets: %s", library.games.at(assetIndex).label.c_str());
		else
			ImGui::TextWrapped("Choose an asset folder and edition in the palette before creating "
			                   "a native campaign project.");
		ImGui::BeginDisabled(library.games.empty() || nativePlayer.running());
		createNative = ImGui::Button("Create native campaign project...");
		mark("settings-native");
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(nativePlayer.running());
		openExisting = ImGui::Button("Open existing project...");
		ImGui::EndDisabled();
		if (nativePlayer.running())
			ImGui::TextDisabled("Stop native preview before switching project folders.");
		ImGui::SeparatorText("PLAY BUTTON AND F5");
		bool changed = false;
		if (ImGui::RadioButton("Current room / TEST marker", !settings.authoredStart)) {
			settings.authoredStart = false;
			changed = true;
		}
		mark("settings-here");
		if (ImGui::RadioButton(p.native.enabled ? "Native campaign / authored level start"
		                                        : "Authored project start",
		                       settings.authoredStart)) {
			settings.authoredStart = true;
			changed = true;
		}
		mark("settings-start");
		ImGui::TextWrapped("Native campaign mode includes the selected level's arrival sequence "
		                   "and continues into later levels. Current-room tests start fresh at "
		                   "the TEST marker. Shift+F5 always uses the authored start; Ctrl/Cmd+F5 "
		                   "always tests here. Pause / Apply live / Resume keeps earned progress.");
		changed |= ImGui::Checkbox("Show native actor markers", &settings.nativeMarkers);
		mark("settings-markers");
		if (changed)
			attempt([&] { saveEditorSettings(path.parent_path(), settings); });
		ImGui::TextDisabled(
		    "Preferences belong to this project folder; campaign content is unchanged.");
		ImGui::SeparatorText("NATIVE CAMPAIGN EVENTS: CURRENT SUPPORT");
		if (ImGui::BeginTable("native_capabilities", 2,
		                      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
			const char *rows[][2] = {
			    {"Original actors and hidden helpers",
				 "Selectable palette records and room markers"},
			    {"Actor placement, facing and health", "Editable; restart to apply"},
			    {"Original story and puzzle scripts", "Preserved until edited; restart applies rules"},
			    {"Native rule execution / live graph trace", "Available in Module Editing"},
			    {"Native puzzle modules / event graph editing",
				 "Rule nodes + authored actor bindings"},
			    {"Custom connected puzzle modules", "Editable and placeable in custom projects"}};
			for (const auto &row : rows) {
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextWrapped("%s", row[0]);
				ImGui::TableNextColumn();
				ImGui::TextWrapped("%s", row[1]);
			}
			ImGui::EndTable();
		}
		ImGui::TextWrapped(
		    "Numbered room markers identify actors, not every event. Some helpers "
		    "have no room position. Puzzle Connections shows typed dependency ports, "
            "private behaviors and complete-bundle move/copy. Runtime collision and "
            "inventory links remain native lookups and can be watched in the message trace.");
		ImGui::EndChild();
		if (ImGui::Button("Close settings") || ImGui::IsKeyPressed(ImGuiKey_Escape) ||
		    createNative || openExisting) {
			settingsOpen = false;
			ImGui::CloseCurrentPopup();
		}
		mark("settings-close");
		ImGui::EndPopup();
	}
	// Start a file picker after closing this modal, rather than nesting two modals.
	if (createNative)
		attempt([&] { importNativeCampaign(p.native.enabled ? p.native.level : 0, true); });
	if (openExisting)
		choose([&](auto file) { open(file); });
}
