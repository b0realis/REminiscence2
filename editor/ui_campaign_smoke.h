// SPDX-License-Identifier: GPL-3.0-or-later
void inputCampaign(int frame, Workspace &app) {
	auto &io = ImGui::GetIO();
	auto click = [&](int when, const char *id) {
		if (frame == when) {
			pointer = app.testPoints.at(id);
			io.AddMousePosEvent(pointer.x, pointer.y);
			io.AddMouseButtonEvent(0, true);
		}
		if (frame == when + 1)
			io.AddMouseButtonEvent(0, false);
	};
	if (frame == 580) {
		app.p = makeProject();
		app.p.modules = {makeGraph(app.p, 8, app.p.spawn)};
		app.room = 0;
		app.selected = 0;
		app.graphNode = 1;
		app.nativeExtensionGraph = false;
		app.nativeMonitorGraph = false;
		app.openGraph();
		app.graphNode = 1;
		app.graphFrameAll = true;
	}
	click(584, "node-help");
	if (frame == 590) {
		app.nodeHelpOpen = false;
		app.campaign = {};
		app.campaignPath = output / "campaign.r2campaign.json";
		app.campaignSaved = serializeCampaign(app.campaign);
	}
	click(594, "workflow-campaign");
	click(598, "campaign-add-2");
	click(602, "campaign-add-4");
	click(606, "campaign-port-0-1");
	click(610, "campaign-port-1-0");
	if (frame == 614) {
		app.campaignStage = 0;
		app.campaign.stages[0].name = "Arrival at the station";
		auto &e = app.campaign.stages[0].elements[0];
		e.text = "The shuttle has gone. Two paths lead into the station. One is quiet. The other "
		         "still has power.";
		e.effect = StoryEffect::Typewriter;
		e.color = {80, 230, 215, 255};
		app.storyReveal = true;
		app.campaignFrame = true;
	}
	click(618, "campaign-save");
	click(626, "campaign-layout-window");
	if (frame == 634) {
		pointer = app.testPoints.at("campaign-story-drag");
		io.AddMousePosEvent(pointer.x, pointer.y);
		io.AddMouseButtonEvent(0, true);
	}
	if (frame == 635) {
		pointer.x += 20;
		pointer.y += 15;
		io.AddMousePosEvent(pointer.x, pointer.y);
	}
	if (frame == 636)
		io.AddMouseButtonEvent(0, false);
}
void checkCampaign(int frame, Workspace &app) {
	if (frame == 587) {
		require(app.nodeHelpOpen, "Node help window did not open");
		screenshot(app.renderer, (output / "node-help.bmp").u8string());
	}
	if (frame == 605)
		require(app.workflow == Workflow::Campaign && app.campaign.stages.size() == 2,
		        "Campaign palette did not create stages");
	if (frame == 613)
		require(app.campaign.stages[0].routes.size() == 1 &&
		            app.campaign.stages[0].routes[0].to == app.campaign.stages[1].id,
		        "Campaign ports did not connect stages");
	if (frame == 622) {
		require(fs::exists(app.campaignPath) && !app.campaignDirty(),
		        "Campaign save button failed");
		require(validateCampaign(loadCampaign(app.campaignPath)).empty(),
		        "Saved UI campaign is invalid");
		screenshot(app.renderer, (output / "campaign-editor.bmp").u8string());
	}
	if (frame == 630) {
		require(app.storyWindowOpen, "Story layout window did not open");
		screenshot(app.renderer, (output / "campaign-screen-layout.bmp").u8string());
	}
	if (frame == 639) {
		require(app.campaign.stages[0].elements[0].x > 40, "Story element drag did not move text");
		app.campaignHistory(false);
		require(app.campaign.stages[0].elements[0].x == 40, "Story placement undo failed");

	}
}
