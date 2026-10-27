// SPDX-License-Identifier: GPL-3.0-or-later
// Included inside Workspace. Original viewing never changes the open project.
void openOriginal() {
	if (library.games.empty())
		throw std::runtime_error("Choose your original assets folder and edition first");
	referencePreview.audible = false;
	auto next = std::make_unique<OriginalLevel>(library.games.at(assetIndex), &referencePreview,
	                                            referenceLevel);
	referenceRoom = next->startRoom();
	next->room(referenceRoom); // Decode before replacing the current reference.
	reference = std::move(next);
	status = "Original level " + std::to_string(referenceLevel + 1) +
	         " reference: " + library.games.at(assetIndex).label;
}
// Shared entry point for the reference viewer and Editor Settings. A native
// campaign is a new set of overrides with intact dependencies, never a checkbox
// that reinterprets a custom module document as a native level.
void importNativeCampaign(int level, bool campaignStart) {
	if (library.games.empty())
		throw std::runtime_error("Choose your original assets folder and edition first");
	if (nativePlayer.running())
		throw std::runtime_error("Stop the native preview before creating another campaign");
	const auto source = library.games.at(assetIndex);
	PreviewSystem checked;
	checked.audible = false;
	OriginalLevel verify(source, &checked, level);
	choose(
	    [&, source, level, campaignStart](auto parent) {
		    const auto folder = parent / ("NativeCampaign-" + makeProject().id);
		    exportNativeCampaign(source,folder);
		    open(folder / ("level-" + std::to_string(level + 1) + ".r2project.json"));
		    reference.reset();
		    settings.authoredStart = campaignStart;
		    saveEditorSettings(path.parent_path(), settings);
		    status = "Native campaign opened with all original actors and scripts. "
		             "Editor Settings selects campaign start or current-room testing.";
	    },
	    false, true);
}
void originalUI() {
	if (ImGui::Button("Back to project")) {
		reference.reset();
		return;
	}
	ImGui::SameLine();
	if (ImGui::Button("Make scenery copy..."))
		choose(
		    [&](auto parent) {
			    const auto folder = parent / ("Level" + std::to_string(referenceLevel + 1) +
				                              "-study-" + makeProject().id);
			    const auto file = reference->exportScenery(folder);
			    open(file);
			    room = reference->startRoom();
			    reference.reset();
			    status = "Scenery copy opened. Actors and campaign scripts are not imported. " +
				         file.u8string();
		    },
		    false, true);
	mark("scenery-copy");
	ImGui::SameLine();
	if (ImGui::Button("Guidelines (F1)"))
		guidelines = true;
	ImGui::SameLine();
	if (ImGui::Button("Edit full native campaign..."))
		attempt([&] { importNativeCampaign(referenceLevel, false); });
	mark("native-import");
	ImGui::SetNextItemWidth(140);
	if (ImGui::Combo("Reference level", &referenceLevel,
	                 "Level 1\0Level 2\0Level 3\0Level 4\0Level 5\0Level 6\0Level 7\0"))
		attempt([&] { openOriginal(); });
	ImGui::TextWrapped("ORIGINAL CAMPAIGN - reference only. Copy scenery to edit geometry and add "
	                   "modules. No scripts run in this view.");
	if (!ImGui::BeginTable("original_reference", 3,
	                       ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
		return;
	ImGui::TableSetupColumn("Slots", ImGuiTableColumnFlags_WidthFixed, 160);
	ImGui::TableSetupColumn("Room", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupColumn("Records", ImGuiTableColumnFlags_WidthFixed, 290);
	ImGui::TableNextColumn();
	ImGui::Checkbox("Empty slots", &referenceEmpty);
	ImGui::BeginChild("native_slots", {0, -60});
	for (int i = 0; i < MaxRooms; ++i) {
		if (!referenceEmpty && !reference->hasMap(i))
			continue;
		const auto label = "Slot " + std::to_string(i) +
		                   (i == reference->startRoom() ? " [START]" : "") +
		                   (reference->hasMap(i) ? "" : " [empty]");
		if (ImGui::Selectable(label.c_str(), referenceRoom == i))
			referenceRoom = i;
	}
	ImGui::EndChild();
	ImGui::TableNextColumn();
	ImGui::Checkbox("Initial actors", &referenceActors);
	ImGui::SameLine();
	ImGui::Checkbox("Collision", &referenceCollision);
	const auto &r = reference->room(referenceRoom);
	const auto &picture = referenceActors ? r.actors : r.scenery;
	SDL_UpdateTexture(texture, nullptr, picture.pixels.data(), RoomWidth * 4);
	const float scale =
	    std::max(0.5f, std::min((ImGui::GetContentRegionAvail().x - 12) / RoomWidth,
		                        (ImGui::GetContentRegionAvail().y - 120) / RoomHeight));
	const auto origin = ImGui::GetCursorScreenPos();
	ImGui::Image((ImTextureID)(intptr_t)texture, {RoomWidth * scale, RoomHeight * scale});
	if (referenceCollision) {
		auto *draw = ImGui::GetWindowDrawList();
		for (int y = 0; y < 7; ++y)
			for (int x = 0; x < 16; ++x) {
				const int v = reference->collision()[256 + referenceRoom * 112 + y * 16 + x];
				if (!v)
					continue;
				ImVec2 a{origin.x + x * 16 * scale, origin.y + std::max(0, y * 36 - 8) * scale};
				ImVec2 b{a.x + 16 * scale, origin.y + std::min(224, y * 36 + 28) * scale};
				draw->AddRectFilled(a, b, IM_COL32(240, 168, 70, 65));
				draw->AddRect(a, b, IM_COL32(240, 190, 80, 175));
			}
	}
	ImGui::Text("Slot %d | 256 x 224 | %s", referenceRoom,
	            r.hasMap ? "native decoded map" : "no map data");
	const char *dirs[] = {"Up", "Down", "Right", "Left"};
	for (int d = 0; d < 4; ++d) {
		if (d)
			ImGui::SameLine();
		const int target = reference->collision()[d * 64 + referenceRoom];
		ImGui::BeginDisabled(target < 0 || target >= MaxRooms);
		const auto label = std::string(dirs[d]) + ": " +
		                   (target >= 0 && target < MaxRooms ? std::to_string(target) : "sealed");
		if (ImGui::Button(label.c_str()))
			referenceRoom = target;
		ImGui::EndDisabled();
	}
	ImGui::TextWrapped(
	    "Links follow original slot IDs. Some maps are unused or reached only through scripted "
	    "events. This is an initial-state inspection, not a running campaign.");
	ImGui::TableNextColumn();
	ImGui::Text("%zu native actor records", reference->actors().size());
	ImGui::Checkbox("All actors / helpers", &referenceAllActors);
	ImGui::BeginChild("native_records", {0, -60});
	for (const auto &a : reference->actors()) {
		if (!referenceAllActors && a.room != referenceRoom)
			continue;
		ImGui::SeparatorText(("Actor " + std::to_string(a.id)).c_str());
		ImGui::Text("Slot %d | X %d Y %d", a.room, a.x, a.y);
		ImGui::Text("Type %d | behavior %d", a.type, a.behavior);
		if (a.room >= MaxRooms)
			ImGui::TextDisabled("Holding / offscreen helper");
	}
	ImGui::EndChild();
	ImGui::EndTable();
}
