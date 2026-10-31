// SPDX-License-Identifier: GPL-3.0-or-later
// Three authoring workspaces share a document, but not editing tools/gestures.
void setWorkflow(Workflow next) {
	workflow = next;
	graphView = next == Workflow::Module;
	tool = Tool::Select;
	pendingFrom = -1;
	gesture = graphDrag = gestureRecorded = boxSelecting = placingTestStart = false;
}
void workflowTabs() {
	const char *labels[] = {"1  Level Editing", "2  Module Placement", "3  Module Editing",
	                        "4  Campaign", "Help"};
	const char *ids[] = {"workflow-level", "workflow-placement", "workflow-module",
	                     "workflow-campaign", "workflow-help"};
	if (ImGui::BeginTabBar("authoring_workflows")) {
		for (int i = 0; i < 5; ++i) {
			// Programmatic graph/validation navigation selects the corresponding tab.
			const auto flags = workflow == Workflow(i) ? ImGuiTabItemFlags_SetSelected : 0;
			const bool active = ImGui::BeginTabItem(labels[i], nullptr, flags);
			mark(ids[i]);
			if (ImGui::IsItemClicked())
				setWorkflow(Workflow(i));
			if (active)
				ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
}
void workflowGuide() {
	if (!showWorkflowTips || workflow == Workflow::Help)
		return;
	if (p.native.enabled && workflow == Workflow::Module && !nativeExtensionGraph) {
		ImGui::TextWrapped("Inspect an original puzzle: select a module and actor to see its "
		                   "connections or rules. Help explains each port; the live monitor shows "
		                   "events while the native campaign runs.");
		return;
	}
	if (p.native.enabled && workflow == Workflow::Placement) {
		ImGui::TextWrapped("Place original actors and puzzles using the palette. Select a room "
		                   "marker to inspect it; Module Editing exposes its connections and rules. "
		                   "Escape returns to selection.");
		return;
	}
	const char *guides[] = {
	    "Build the space: import room art on the right, or Slice image for a whole level. "
	    "Paint collision separately, add ledges, then connect rooms using Screen transitions.",
	    "Place a puzzle: choose a module on the left, then click the room. Select a part to "
	    "move it; EDIT MODULE GRAPH opens its behavior. Escape returns to selection.",
	    "Build behavior: add nodes on the left, connect an output to an input, then edit the "
	    "selected node on the right. Start graph / Play signals tests logic; toolbar Play tests the game.",
	    "Build the journey: add your current level or a story screen, then connect OUT to IN. "
	    "Select a stage to edit it. The toolbar saves, checks and plays this campaign."};
	ImGui::TextWrapped("%s", guides[int(workflow)]);
}
// Keep short toolbars usable at the minimum window width, without hiding actions.
void toolbarNext(const char *label) {
	const float width = ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2;
	if (ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + width <=
	    ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x)
		ImGui::SameLine();
}
void itemTip(const char *text) {
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled | ImGuiHoveredFlags_DelayNormal))
		ImGui::SetTooltip("%s", text);
}
void chooseGameAssets() {
	choose(
	    [&](auto f) {
		    library = assets::discover(f.u8string());
		    assetIndex = 0;
		    campaignMovieNames.clear();
		    status = library.games.empty()
			             ? "No supported game files found. Choose the extracted Amiga, DOS/GOG or "
			               "Sega CD folder."
			             : std::to_string(library.games.size()) + " editions found. Ready to play.";
	    },
	    false, true);
}
void refreshModuleLibrary() {
	moduleLibrary.clear();
	libraryRoot = images.root;
	const auto folder = images.root / "modules";
	std::error_code ec;
	if (!fs::exists(folder, ec))
		return;
	for (const auto &entry : fs::directory_iterator(folder)) {
		if (moduleLibrary.size() >= 128)
			break;
		const auto name = entry.path().filename().u8string();
		if (entry.is_regular_file() && name.size() >= 14 &&
		    name.compare(name.size() - 14, 14, ".r2module.json") == 0)
			moduleLibrary.push_back(entry.path());
	}
	std::sort(moduleLibrary.begin(), moduleLibrary.end());
}
void libraryPalette() {
	if (libraryRoot != images.root)
		attempt([&] { refreshModuleLibrary(); });
	if (ImGui::TreeNodeEx("LOCAL MODULE LIBRARY", ImGuiTreeNodeFlags_DefaultOpen)) {
		if (ImGui::SmallButton("Refresh"))
			attempt([&] { refreshModuleLibrary(); });
		ImGui::TextWrapped("Project / modules (up to 128 templates)");
		for (const auto &file : moduleLibrary) {
			const auto name = file.filename().u8string();
			if (paletteSearch.PassFilter(name.c_str()) &&
			    ImGui::Selectable(name.c_str(), tool == Tool::Prefab && pendingPrefab == file)) {
				pendingPrefab = file;
				tool = Tool::Prefab;
				status = "Click the room to place a new independent module instance";
			}
			mark(("library-" + file.filename().u8string()).c_str());
		}
		if (moduleLibrary.empty())
			ImGui::TextWrapped("Save a puzzle to library in Module Editing.");
		ImGui::TreePop();
	}
}
void registerAsset(Module &m, const Art &asset) {
	if (std::any_of(m.assets.begin(), m.assets.end(),
	                [&](const auto &a) { return a.file == asset.file; }))
		return;
	if (m.assets.size() >= 64)
		throw std::runtime_error("Maximum 64 module assets");
	m.assets.push_back(asset);
}
void moduleMetadata(Module &m) {
	ImGui::SeparatorText("MODULE / NAME / ASSETS");
	auto before = p;
	ImGui::TextUnformatted("Module name");
	ImGui::SetNextItemWidth(-1);
	if (textField("##module-name", m.name, 160))
		history.remember(before);
	mark("module-name");
	if (ImGui::Button("Module help / comments"))
		moduleHelpOpen = true;
	if (ImGui::TreeNode("Owned images")) {
		ImGui::TextWrapped("%zu images; clips select from this library. Import strips, then set "
		                   "one-frame crop and FPS on the node.",
		                   m.assets.size());
		ImGui::BeginDisabled(m.assets.size() >= 64);
		if (ImGui::Button("Add image...")) {
			const auto id = m.id;
			choose([&, id](auto file) {
				const auto image = readImage(file);
				const auto name = importImage(file, images.root);
				for (auto &module : p.modules)
					if (module.id == id) {
						edit();
						registerAsset(module, {name, 0, 0, image.width, image.height});
						break;
					}
			});
		}
		ImGui::EndDisabled();
		for (size_t i = 0; i < m.assets.size(); ++i) {
			ImGui::PushID(int(i));
			ImGui::TextWrapped("%s", m.assets[i].file.c_str());
			if (ImGui::SmallButton("Remove from library")) {
				edit();
				m.assets.erase(m.assets.begin() + i);
				ImGui::PopID();
				break;
			}
			ImGui::PopID();
		}
		ImGui::TextWrapped("Removing a library entry preserves existing clips and files.");
		ImGui::TreePop();
	}
	if (ImGui::Button("Save to library", {-1, 0}))
		attempt([&] {
			const auto folder = images.root / "modules";
			fs::create_directories(folder);
			// IDs are validated and contain no path punctuation. The readable name is
			// sanitized independently, so a module title can never escape this folder.
			std::string slug;
			for (unsigned char c : m.name) {
				if (slug.size() >= 48)
					break;
				slug += ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
				            ? char(c)
							: '-';
			}
			savePrefab(m, images.root, folder / (slug + "-" + m.id + ".r2module.json"));
			refreshModuleLibrary();
			status = "Saved complete module and image dependencies to the local library";
		});
	mark("module-library-save");
}
void moduleChooser() {
	graphView = true;
	ImGui::BeginChild("module_chooser", {0, -35});
	ImGui::SeparatorText("3. MODULE EDITING - ONE CONNECTED PUZZLE");
	if (p.native.enabled) {
		ImGui::TextWrapped("Native extensions connect existing actors to authored dialogues, "
		                   "timers and state machines. Bind Actor nodes to native IDs; restart the "
		                   "native preview to apply logic edits.");
		if (ImGui::Button("Native puzzle rule tables")) {
			nativeExtensionGraph = false;
			return;
		}
	}
	{
		ImGui::TextWrapped(
		    "Give a new puzzle a name, add its image assets and build its behavior from nodes. "
		    "Every placed instance owns its state and internal connections.");
		textField("New module name", newModuleName, 160);
		ImGui::BeginDisabled(p.modules.size() >= MaxModules || newModuleName.empty());
		if (ImGui::Button("Create module"))
			attempt([&] {
				auto next = p;
				auto m = makeGraph(next, 5, {p.rooms[room].id, 160, 216});
				m.name = newModuleName;
				edit();
				p.serial = next.serial;
				p.modules.push_back(std::move(m));
				selected = int(p.modules.size()) - 1;
				part = 0;
				openGraph();
			});
		mark("module-create");
		ImGui::EndDisabled();
		ImGui::SeparatorText("EDIT AN EXISTING MODULE INSTANCE");
		for (int i = 0; i < int(p.modules.size()); ++i)
			if (p.modules[i].kind == Kind::Graph) {
				if (ImGui::Selectable((p.modules[i].name + "##" + p.modules[i].id).c_str())) {
					selected = i;
					part = 0;
					openGraph();
				}
			}
	}
	ImGui::EndChild();
}
