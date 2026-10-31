// SPDX-License-Identifier: GPL-3.0-or-later
// Real-widget checks for document isolation, discoverable tools and small windows.
void inputUsability(int frame, Workspace &app) {
	auto &io = ImGui::GetIO();
	auto click = [&](int at, const char *id) {
		if (frame == at) {
			pointer = app.testPoints.at(id);
			io.AddMousePosEvent(pointer.x, pointer.y);
			io.AddMouseButtonEvent(0, true);
		}
		if (frame == at + 1)
			io.AddMouseButtonEvent(0, false);
	};
	if (frame == 674) {
		app.campaign = {};
		app.campaignUndo.clear();
		app.campaignRedo.clear();
		app.campaignPath = output / "usability" / "campaign.r2campaign.json";
		app.campaignSaved = serializeCampaign(app.campaign);
		app.p = makeProject();
		app.path = output / "usability" / "level.r2project.json";
		app.images.root = app.path.parent_path();
		app.saveAs(app.path);
		app.history = {};
		app.edit();
		app.p.name = "Unsaved level draft";
		playtestDocument = serialize(app.p);
		app.setWorkflow(Workflow::Campaign);
		app.storyWindowOpen = app.nodeHelpOpen = app.moduleHelpOpen = false;
	}
	click(676, "campaign-add-2");
	click(680, "campaign-add-4");
	click(684, "toolbar-undo");
	click(688, "toolbar-redo");
	click(692, "toolbar-validate");
	click(696, "toolbar-save-as");
	if (frame == 699) {
		require(app.picker.open && app.picker.file == "campaign.r2campaign.json",
		        "Campaign Save as opened a level picker");
		app.picker.directory = app.campaignPath.parent_path().u8string();
		app.picker.file = "renamed.r2campaign.json";
	}
	click(700, "picker-accept");
	click(704, "toolbar-new");
	if (frame == 708 || frame == 714)
		io.AddKeyEvent(ImGuiKey_Escape, true);
	if (frame == 709 || frame == 715)
		io.AddKeyEvent(ImGuiKey_Escape, false);
	click(710, "toolbar-open");
	if (frame == 716) {
		app.campaign = {};
		app.addCampaignStage(StageKind::End);
	}
	click(720, "play-here");
	if (frame == 726) {
		app.p = makeProject();
		app.room = 0;
		app.p.modules = {makeGraph(app.p, 5, app.p.spawn)};
		app.selected = 0;
		app.graphNode = -1;
		app.nodeSearch.Clear();
		app.nodeCategoryFilter = 0;
		app.openGraph();
	}
	click(730, "node-search");
	if (frame == 732)
		io.AddInputCharactersUTF8("not-a-node-zz");
	click(736, "node-search-clear");
	click(740, "node-search");
	if (frame == 742)
		io.AddInputCharactersUTF8("collects");
	click(746, "add-node-3");
	click(750, "node-search-clear");
	click(754, "node-category");
	click(758, "node-category-6");
	click(764, "toolbar-assets");
	if (frame == 768)
		io.AddKeyEvent(ImGuiKey_Escape, true);
	if (frame == 769)
		io.AddKeyEvent(ImGuiKey_Escape, false);
	if (frame == 770) {
		SDL_SetWindowSize(SDL_RenderGetWindow(app.renderer), 960, 640);
		app.setWorkflow(Workflow::Level);
		app.zoom = 1;
		app.tool = Tool::Solid;
		app.placingTestStart = true;
	}
	if (frame == 774)
		io.AddKeyEvent(ImGuiKey_Escape, true);
	if (frame == 775)
		io.AddKeyEvent(ImGuiKey_Escape, false);
	if (frame == 777)
		pointer = {950, 630};
	click(780, "play-options");
	click(784, "play-default-settings");
	if (frame == 790 && !app.library.games.empty()) {
		app.settingsOpen = false;
		OriginalLevel original(app.library.games.front(), &app.nativePreview, 0);
		app.p = original.createProject(true);
		app.room = app.p.native.actors[0].room;
		app.setWorkflow(Workflow::Level);
	}
}
void checkUsability(int frame, Workspace &app) {
	if (frame == 687)
		require(app.campaign.stages.size() == 1 && serialize(app.p) == playtestDocument,
		        "Toolbar Undo changed the level instead of the campaign");
	if (frame == 691)
		require(app.campaign.stages.size() == 2 && serialize(app.p) == playtestDocument,
		        "Toolbar Redo failed to restore campaign stage");
	if (frame == 695) {
		require(app.campaignValidationOpen && !app.campaignValidationResults.empty() &&
		            !app.validationOpen,
		        "Campaign validation must report missing routes");
		app.campaignValidationOpen = false;
	}
	if (frame == 703)
		require(!app.picker.open && app.campaignPath.filename() == "renamed.r2campaign.json" &&
		            fs::exists(app.campaignPath) && app.path.filename() == "level.r2project.json" &&
		            serialize(app.p) == playtestDocument,
		        "Campaign Save as modified the level document");
	if (frame == 707)
		require(app.picker.open && app.picker.save && app.picker.file == "campaign.r2campaign.json",
		        "New campaign toolbar did not open campaign destination picker");
	if (frame == 713)
		require(app.picker.open && !app.picker.save && serialize(app.p) == playtestDocument,
		        "Open campaign toolbar modified the level");
	if (frame == 724 && !app.library.games.empty())
		require(!app.scene && app.status.find("Campaign launched") == 0 && !app.campaignDirty() &&
		            serialize(app.p) == playtestDocument,
		        "Toolbar Play did not launch campaign or altered unrelated level draft");
	if (frame == 735) {
		require(app.nodeSearch.IsActive() && app.p.modules[0].nodes.empty(),
		        "Unmatched search created a node");
		screenshot(app.renderer, (output / "node-search-empty.png").u8string());
	}
	if (frame == 739)
		require(!app.nodeSearch.IsActive(), "Clear node search failed");
	if (frame == 749)
		require(app.p.modules[0].nodes.size() == 1 &&
		            app.p.modules[0].nodes[0].kind == NodeKind::Item,
		        "Searching behavior descriptions did not expose quest pickup");
	if (frame == 762) {
		require(app.nodeCategoryFilter == 6, "Audio category selection failed");
		screenshot(app.renderer, (output / "node-search-audio.png").u8string());
	}
	if (frame == 767)
		require(app.picker.open && app.picker.folder, "Module workspace cannot choose game assets");
	if (frame == 778) {
		require(app.tool == Tool::Select && !app.placingTestStart,
		        "Escape did not leave painting mode");
		for (const auto *id : {"toolbar-new", "toolbar-open", "toolbar-save", "toolbar-save-as",
		                       "toolbar-undo", "toolbar-redo", "toolbar-validate", "play-here",
		                       "play-options", "toolbar-assets", "guidelines", "workflow-help"}) {
			const auto at = app.testPoints.at(id);
			require(at.x > 0 && at.x < 960 && at.y > 0 && at.y < 640,
			        "Toolbar action clipped at minimum window size");
		}
		screenshot(app.renderer, (output / "editor-small-window.png").u8string());
	}
	if (frame == 788) {
		require(app.settingsOpen, "Visible Start menu did not open default-start settings");
		app.quit = app.library.games.empty();
	}
	if (frame == 794 && !app.library.games.empty()) {
		for (const auto *id : {"native-apply", "native-apply-resume", "native-stop"}) {
			const auto at = app.testPoints.at(id);
			require(at.x > 0 && at.x < 960 && at.y > 0 && at.y < 640,
			        "Native live-update controls clipped at minimum window size");
		}
		screenshot(app.renderer, (output / "native-small-window.png").u8string());
		app.quit = true;
	}
}
