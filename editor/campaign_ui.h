// SPDX-License-Identifier: GPL-3.0-or-later
bool campaignDirty() const {
	return serializeCampaign(campaign) != campaignSaved;
}
void campaignRemember(const Campaign &before) {
	const auto text = serializeCampaign(before);
	if (campaignUndo.empty() || campaignUndo.back() != text)
		campaignUndo.push_back(text);
	if (campaignUndo.size() > 40)
		campaignUndo.erase(campaignUndo.begin());
	campaignRedo.clear();
}
void campaignHistory(bool redo) {
	auto &from = redo ? campaignRedo : campaignUndo;
	auto &to = redo ? campaignUndo : campaignRedo;
	if (from.empty())
		return;
	to.push_back(serializeCampaign(campaign));
	campaign = deserializeCampaign(from.back());
	from.pop_back();
	campaignStage = std::clamp(campaignStage, -1, int(campaign.stages.size()) - 1);
	campaignLink = -1;
}
void saveCampaignDocument() {
	saveCampaign(campaign, campaignPath);
	campaignSaved = serializeCampaign(campaign);
	status = "Saved campaign: " + campaignPath.u8string();
}
void openCampaignDocument(const fs::path &file) {
	if (campaignDirty())
		saveCampaignDocument();
	auto next = loadCampaign(file);
	campaign = std::move(next);
	campaignPath = fs::absolute(file);
	campaignSaved = serializeCampaign(campaign);
	campaignUndo.clear();
	campaignRedo.clear();
	campaignStage = stageIndex(campaign, campaign.entry);
	campaignLink = -1;
	campaignFrame = true;
	setWorkflow(Workflow::Campaign);
}
void launchCampaign(bool selectedStage = false, bool resume = false) {
	const auto issues = validateCampaign(campaign, campaignPath.parent_path(), true);
	if (!issues.empty())
		throw std::runtime_error(issues.front());
	if (library.games.empty())
		throw std::runtime_error("Choose original game assets before playing a campaign");
	saveCampaignDocument();
	char *base = SDL_GetBasePath();
	if (!base)
		throw std::runtime_error("Cannot locate player");
	fs::path engine = fs::u8path(base);
	SDL_free(base);
#ifdef _WIN32
	engine /= "reminiscence2.exe";
#else
	engine /= "reminiscence2";
#endif
	std::vector<std::string> args{
	    engine.u8string(), "--campaign=" + campaignPath.u8string(),
	    "--datapath=" + library.games[assetIndex].root,
	    "--asset-set=" + std::string(assets::editionId(library.games[assetIndex].edition))};
	args.push_back(resume ? "--campaign-resume" : "--campaign-new");
	if (selectedStage && campaignStage >= 0)
		args.push_back("--campaign-start=" + campaign.stages[campaignStage].id);
	campaignPlayer.launch(args, campaignPath.parent_path() / ".campaign-playtest.log");
	status =
	    "Campaign launched; Esc in a story screen leaves. Level exits follow the campaign routes.";
}
void addCampaignStage(StageKind kind) {
	campaignRemember(campaign);
	auto next = newStage(campaign, kind);
	if (campaign.entry.empty())
		campaign.entry = next.id;
	campaign.stages.push_back(next);
	campaignStage = int(campaign.stages.size()) - 1;
}
void campaignGraph() {
	ImGui::SetNextItemWidth(100);
	ImGui::SliderFloat("Zoom", &campaignZoom, .1f, 2.f, "%.1fx");
	ImGui::SameLine();
	if (ImGui::Button("Frame stages"))
		campaignFrame = true;
	ImGui::TextWrapped("Drag headers; middle-drag pans; wheel zooms. Click OUT then IN to connect. "
	                   "Edit exit names and choice labels in the inspector.");
	ImGui::BeginChild("campaign_graph", {0, -35}, ImGuiChildFlags_Borders,
	                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	const auto origin = ImGui::GetCursorScreenPos(), size = ImGui::GetContentRegionAvail();
	if (campaignFrame) {
		float x = 240, y = 150;
		for (const auto &s : campaign.stages) {
			x = std::max(x, float(s.x + 230));
			y = std::max(y, float(s.y + 130));
		}
		campaignZoom = std::clamp(std::min((size.x - 24) / x, (size.y - 24) / y), .1f, 2.f);
		campaignPan = {12, 12};
		campaignFrame = false;
	}
	const auto &io = ImGui::GetIO();
	if (ImGui::IsWindowHovered()) {
		if (ImGui::IsMouseDragging(2)) {
			campaignPan.x += io.MouseDelta.x;
			campaignPan.y += io.MouseDelta.y;
		}
		if (io.MouseWheel) {
			const float z =
			    std::clamp(campaignZoom * (io.MouseWheel > 0 ? 1.15f : 1 / 1.15f), .1f, 2.f);
			const float x = io.MousePos.x - origin.x, y = io.MousePos.y - origin.y;
			campaignPan = {x - (x - campaignPan.x) * z / campaignZoom,
			               y - (y - campaignPan.y) * z / campaignZoom};
			campaignZoom = z;
		}
	}
	auto at = [&](int i) {
		return ImVec2(origin.x + campaignPan.x + campaign.stages[i].x * campaignZoom,
		              origin.y + campaignPan.y + campaign.stages[i].y * campaignZoom);
	};
	auto pin = [&](int i, bool out) {
		auto v = at(i);
		return ImVec2(v.x + (out ? 208 : 0) * campaignZoom, v.y + 84 * campaignZoom);
	};
	auto *draw = ImGui::GetWindowDrawList();
	for (int i = 0; i < int(campaign.stages.size()); ++i)
		for (const auto &r : campaign.stages[i].routes) {
			const int j = stageIndex(campaign, r.to);
			if (j < 0)
				continue;
			auto a = pin(i, true), b = pin(j, false);
			draw->AddBezierCubic(a, {a.x + 60, a.y}, {b.x - 60, b.y}, b,
			                     IM_COL32(80, 213, 207, 230), 2);
			draw->AddTriangleFilled(b, {b.x - 10, b.y - 5}, {b.x - 10, b.y + 5},
			                        IM_COL32(80, 213, 207, 255));
			draw->AddText({(a.x + b.x) * .5f, (a.y + b.y) * .5f}, IM_COL32(242, 211, 145, 255),
			              r.key.c_str());
		}
	ImGui::SetWindowFontScale(campaignZoom);
	for (int i = 0; i < int(campaign.stages.size()); ++i) {
		auto &s = campaign.stages[i];
		auto q = at(i);
		ImGui::PushID(i);
		draw->AddRectFilled(q, {q.x + 208 * campaignZoom, q.y + 110 * campaignZoom},
		                    IM_COL32(25, 43, 55, 255), 7);
		draw->AddRect(q, {q.x + 208 * campaignZoom, q.y + 110 * campaignZoom},
		              i == campaignStage ? IM_COL32(255, 208, 95, 255)
		                                 : IM_COL32(60, 140, 150, 255),
		              7, 0, 2);
		draw->AddText({q.x + 8 * campaignZoom, q.y + 10 * campaignZoom}, IM_COL32_WHITE,
		              s.name.substr(0, 24).c_str());
		draw->AddText(
		    {q.x + 8 * campaignZoom, q.y + 40 * campaignZoom}, IM_COL32(130, 210, 210, 255),
		    (std::string(stageName(s.kind)) + (s.id == campaign.entry ? " / START" : "")).c_str());
		ImGui::SetCursorScreenPos(q);
		ImGui::InvisibleButton("stage", {208 * campaignZoom, 65 * campaignZoom});
		mark(("campaign-stage-" + std::to_string(i)).c_str());
		if (ImGui::IsItemClicked()) {
			campaignStage = i;
			storyElement = 0;
			campaignDrag = false;
			storyBegan = SDL_GetTicks();
		}
		if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0, 2)) {
			if (!campaignDrag) {
				campaignRemember(campaign);
				campaignDrag = true;
			}
			s.x = std::clamp(s.x + int(io.MouseDelta.x / campaignZoom), 0, 8192);
			s.y = std::clamp(s.y + int(io.MouseDelta.y / campaignZoom), 0, 8192);
		}
		for (int side = 0; side < 2; ++side) {
			if (side && s.kind == StageKind::End)
				continue;
			auto v = pin(i, side);
			ImGui::SetCursorScreenPos({v.x - 10, v.y - 10});
			ImGui::PushID(side);
			ImGui::InvisibleButton("port", {20, 20});
			mark(("campaign-port-" + std::to_string(i) + "-" + std::to_string(side)).c_str());
			draw->AddCircleFilled(v, 6, IM_COL32(100, 240, 210, 255));
			draw->AddText({v.x + (side ? -40 : 12) * campaignZoom, v.y - 6 * campaignZoom},
			              IM_COL32_WHITE, side ? "OUT" : "IN");
			if (ImGui::IsItemClicked()) {
				if (side)
					campaignLink = i;
				else if (campaignLink >= 0 && campaignLink < int(campaign.stages.size())) {
					auto &src = campaign.stages[campaignLink];
					if (src.routes.size() < 8 &&
					    std::none_of(src.routes.begin(), src.routes.end(), [&](auto &r) {
						    return r.to == s.id && r.key == "complete";
					    })) {
						campaignRemember(campaign);
						src.routes.push_back({"complete", s.name, s.id});
					}
					campaignLink = -1;
				}
			}
			ImGui::PopID();
		}
		ImGui::PopID();
	}
	if (campaignLink >= 0) {
		auto a = pin(campaignLink, true);
		draw->AddLine(a, io.MousePos, IM_COL32(255, 211, 100, 255), 2);
		if (ImGui::IsKeyPressed(ImGuiKey_Escape))
			campaignLink = -1;
	}
	ImGui::SetWindowFontScale(1);
	ImGui::SetCursorScreenPos(origin);
	ImGui::Dummy(size);
	ImGui::EndChild();
}
void storyDesigner(Stage &s) {
	ImGui::SeparatorText("TEXT + PHOTO SCREEN / 640 x 360");
	if (ImGui::Button("Open screen layout window"))
		storyWindowOpen = true;
	mark("campaign-layout-window");
	if (ImGui::Button("Add text") && s.elements.size() < 16)
		s.elements.emplace_back();
	ImGui::SameLine();
	if (ImGui::Button("Add photo") && s.elements.size() < 16) {
		StoryElement e;
		e.kind = StoryKind::Photo;
		e.x = 40;
		e.y = 60;
		e.width = 240;
		e.height = 240;
		s.elements.push_back(e);
		storyElement = int(s.elements.size()) - 1;
	}
	float bg[4];
	for (int i = 0; i < 4; ++i)
		bg[i] = s.background[i] / 255.f;
	if (ImGui::ColorEdit4("Background", bg))
		for (int i = 0; i < 4; ++i)
			s.background[i] = int(bg[i] * 255);
	ImGui::SliderInt("Auto advance ms (0 waits)", &s.advance, 0, 60000);
	s.fadeOut = s.kind == StageKind::Choice || s.routes.size() > 1
	                ? 0 : std::min(s.fadeOut, s.advance);
	if (s.advance > 0 && s.kind != StageKind::Choice && s.routes.size() <= 1)
		ImGui::SliderInt("Fade to black ms", &s.fadeOut, 0, s.advance);
	if (ImGui::Button("Restart preview"))
		storyBegan = SDL_GetTicks();
	ImGui::SameLine();
	ImGui::Checkbox("Reveal", &storyReveal);
	if (!s.elements.empty()) {
		storyElement = std::clamp(storyElement, 0, int(s.elements.size()) - 1);
		if (ImGui::BeginCombo(
		        "Element", (std::to_string(storyElement + 1) + " / " +
				            (s.elements[storyElement].kind == StoryKind::Text ? "Text" : "Photo"))
		                       .c_str())) {
			for (int i = 0; i < int(s.elements.size()); ++i)
				if (ImGui::Selectable((std::to_string(i + 1) + " " +
				                       (s.elements[i].kind == StoryKind::Text ? "Text" : "Photo"))
				                          .c_str(),
				                      i == storyElement))
					storyElement = i;
			ImGui::EndCombo();
		}
		auto &e = s.elements[storyElement];
		if (e.kind == StoryKind::Text) {
			char b[4097];
			std::snprintf(b, sizeof(b), "%s", e.text.c_str());
			if (ImGui::InputTextMultiline("Text", b, sizeof(b), {-1, 90}))
				e.text = b;
			ImGui::SliderInt("Font size", &e.size, 8, 64);
			float c[4];
			for (int i = 0; i < 4; ++i)
				c[i] = e.color[i] / 255.f;
			if (ImGui::ColorEdit4("Text color", c))
				for (int i = 0; i < 4; ++i)
					e.color[i] = int(c[i] * 255);
		} else {
			ImGui::TextWrapped("%s", e.file.c_str());
			if (ImGui::Button("Choose photo...")) {
				const auto id = s.id;
				const int elem = storyElement;
				choose([&, id, elem](auto file) {
					const auto index = stageIndex(campaign, id);
					if (index < 0 || elem >= int(campaign.stages[index].elements.size()))
						return;
					const auto name = importImage(file, campaignPath.parent_path());
					campaignRemember(campaign);
					campaign.stages[index].elements[elem].file = name;
				});
			}
		}
		ImGui::SliderInt("Left", &e.x, 0, 639);
		ImGui::SliderInt("Top", &e.y, 0, 359);
		e.width = std::clamp(e.width, 1, 640 - e.x);
		e.height = std::clamp(e.height, 1, 360 - e.y);
		ImGui::SliderInt("Width", &e.width, 1, 640 - e.x);
		ImGui::SliderInt("Height", &e.height, 1, 360 - e.y);
		int fx = int(e.effect);
		if (ImGui::Combo("Effect", &fx, "None\0Typewriter (text)\0Fade in\0"))
			e.effect = StoryEffect(fx);
		ImGui::SliderInt("Start delay ms", &e.delay, 0, 30000);
		if (e.effect == StoryEffect::Typewriter)
			ImGui::SliderInt("Characters / second", &e.rate, 1, 120);
		if (e.effect == StoryEffect::Fade)
			ImGui::SliderInt("Fade ms", &e.duration, 1, 10000);
		if (ImGui::TreeNode("Motion")) {
			ImGui::SliderInt("Horizontal travel", &e.moveX, -1280, 1280);
			ImGui::SliderInt("Vertical travel", &e.moveY, -720, 720);
			ImGui::SliderInt("Travel time ms", &e.moveDuration, 1, 30000);
			ImGui::TextWrapped("Travel starts after the delay. Negative travel moves left/up. "
			                   "Motion continues when text is revealed; the canvas clips the image.");
			ImGui::TreePop();
		}
		if (ImGui::Button("Delete element"))
			s.elements.erase(s.elements.begin() + storyElement);
		ImGui::SameLine();
		if (ImGui::Button("Move forward") && storyElement + 1 < int(s.elements.size())) {
			std::swap(s.elements[storyElement], s.elements[storyElement + 1]);
			++storyElement;
		}
	}
	storyPreview(s, ImGui::GetContentRegionAvail().x, false);
	ImGui::TextWrapped(
	    "Elements draw in list order. Photos fit their rectangle without stretching. Preview uses "
	    "the same renderer as gameplay. Story text uses ASCII; Enter reveals effects, then "
	    "continues. Route choices appear along the bottom.");
}
void campaignInspector() {
	if (campaignStage < 0 || campaignStage >= int(campaign.stages.size())) {
		ImGui::TextWrapped(
		    "Select a stage. Add levels, movies or story screens, then connect OUT to IN.");
		return;
	}
	auto before = campaign;
	auto &s = campaign.stages[campaignStage];
	textField("Stage name", s.name, 160);
	ImGui::Text("%s / %s", stageName(s.kind), s.id.c_str());
	if (ImGui::Button("Set as campaign start"))
		campaign.entry = s.id;
	if (s.kind == StageKind::Level) {
		ImGui::Checkbox("Remember world on return", &s.remember);
		ImGui::TextWrapped("%s", s.level.c_str());
		if (ImGui::Button("Import / replace level...")) {
			const auto id = s.id;
			choose([&, id](auto file) {
				const int i = stageIndex(campaign, id);
				if (i < 0)
					return;
				const auto name = importCampaignLevel(file, campaignPath.parent_path());
				campaignRemember(campaign);
				campaign.stages[i].level = name;
			});
		}
		if (!s.level.empty() && ImGui::Button("Open level in editor")) {
			const auto file = assetPath(campaignPath.parent_path(), s.level);
			attempt([&] {
				if (serialize(p) != saved)
					saveAs(path);
				open(file);
			});
		}
		ImGui::TextWrapped("Custom levels finish through Level exit nodes. Native levels also emit "
		                   "complete on their original transition. Re-entering a level starts "
		                   "at its authored entrance while preserving its world. Turn off Remember "
		                   "to reset puzzles on each visit.");
	}
	if (s.kind == StageKind::Movie) {
		textField("Movie catalog name", s.movie, 80);
		if (ImGui::Button("Read available movies"))
			attempt([&] {
				if (library.games.empty())
					throw std::runtime_error("Select original assets first");
				campaignMovieNames = listCampaignMovies(library.games[assetIndex]);
			});
		if (ImGui::BeginCombo("Available movies", s.movie.c_str())) {
			for (const auto &name : campaignMovieNames)
				if (ImGui::Selectable(name.c_str(), s.movie == name))
					s.movie = name;
			ImGui::EndCombo();
		}
		ImGui::TextWrapped("Use a name from --list-cutscenes for the selected asset edition (e.g. "
		                   "SC1 for Sega CD). The existing engine player handles the movie and "
		                   "sound; missing movies stop with an error.");
	}
	if (s.kind == StageKind::Story || s.kind == StageKind::Choice || s.kind == StageKind::End)
		storyDesigner(s);
	ImGui::SeparatorText("EXIT ROUTES / BRANCHES");
	if (s.kind != StageKind::End && s.routes.size() < 8 && ImGui::Button("Add route"))
		s.routes.emplace_back();
	for (int i = 0; i < int(s.routes.size()); ++i) {
		ImGui::PushID(i);
		auto &r = s.routes[i];
		textField("Exit name", r.key, 80);
		textField("Choice label", r.label, 80);
		textField("Required flag (blank: always)", r.condition, 65);
		if (!r.condition.empty())
			ImGui::Checkbox("Flag must be true", &r.requiredValue);
		const int target = stageIndex(campaign, r.to);
		if (ImGui::BeginCombo("To", target < 0 ? "Select destination"
		                                       : campaign.stages[target].name.c_str())) {
			for (const auto &t : campaign.stages)
				if (ImGui::Selectable((t.name + "##" + t.id).c_str(), t.id == r.to))
					r.to = t.id;
			ImGui::EndCombo();
		}
		if (ImGui::SmallButton("Remove route")) {
			s.routes.erase(s.routes.begin() + i);
			ImGui::PopID();
			break;
		}
		ImGui::Separator();
		ImGui::PopID();
	}
	ImGui::TextWrapped("Several routes with the same exit name present a choice. Different names "
	                   "follow the exit emitted by the level. Choice stages offer all routes; "
	                   "movies and story screens emit complete. Loops are allowed.");
	designerComments(s.comments);
	if (serializeCampaign(before) != serializeCampaign(campaign))
		campaignRemember(before);
}
void campaignUI() {
	ImGui::TextWrapped("4. CAMPAIGN / %s%s", campaign.name.c_str(), campaignDirty() ? " *" : "");
	if (ImGui::Button("New campaign..."))
		choose(
		    [&](auto f) {
			    if (fs::exists(f))
				    throw std::runtime_error("Choose a new filename for the campaign");
			    if (campaignDirty())
				    saveCampaignDocument();
			    campaign = {};
			    campaignPath = fs::absolute(f);
			    campaignSaved = serializeCampaign(campaign);
			    campaignUndo.clear();
			    campaignRedo.clear();
			    campaignStage = -1;
			    campaignLink = -1;
			    saveCampaignDocument();
		    },
		    true, false, "campaign.r2campaign.json");
	ImGui::SameLine();
	if (ImGui::Button("Open campaign..."))
		choose([&](auto f) { openCampaignDocument(f); });
	ImGui::SameLine();
	if (ImGui::Button("Save campaign"))
		attempt([&] { saveCampaignDocument(); });
	mark("campaign-save");
	ImGui::SameLine();
	if (ImGui::Button("Save as..."))
		choose(
		    [&](auto f) {
			    if (fs::absolute(f).parent_path() != campaignPath.parent_path())
				    throw std::runtime_error("Save in the same campaign folder; copy the entire "
					                         "folder to relocate its assets");
			    campaignPath = fs::absolute(f);
			    saveCampaignDocument();
		    },
		    true, false, "campaign.r2campaign.json");
	ImGui::SameLine();
	ImGui::BeginDisabled(campaignPlayer.running());
	if (ImGui::Button("Play campaign (F5)"))
		attempt([&] { launchCampaign(); });
	ImGui::SameLine();
	if (ImGui::Button("Play selected stage"))
		attempt([&] { launchCampaign(true); });
	ImGui::SameLine();
	if (ImGui::Button("Resume campaign"))
		attempt([&] { launchCampaign(false, true); });
	ImGui::EndDisabled();
	if (!library.games.empty() &&
	    ImGui::BeginCombo("Playback assets", library.games[assetIndex].label.c_str())) {
		for (int i = 0; i < int(library.games.size()); ++i)
			if (ImGui::Selectable(library.games[i].label.c_str(), i == assetIndex)) {
				assetIndex = i;
				campaignMovieNames.clear();
			}
		ImGui::EndCombo();
	}
	if (ImGui::BeginTable("campaign_workspace", 3,
	                      ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
		ImGui::TableSetupColumn("Stages", ImGuiTableColumnFlags_WidthFixed, 210);
		ImGui::TableSetupColumn("Routes", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Stage", ImGuiTableColumnFlags_WidthFixed, 365);
		ImGui::TableNextColumn();
		ImGui::BeginChild("campaign_palette", {0, -35});
		auto before = campaign;
		if (textField("Campaign name", campaign.name, 160))
			campaignRemember(before);
		if (ImGui::Checkbox("Carry health", &campaign.carryHealth) |
		    ImGui::Checkbox("Carry native inventory", &campaign.carryInventory))
			campaignRemember(before);
		ImGui::TextWrapped("%s", campaignPath.u8string().c_str());
		for (int i = 0; i < 5; ++i) {
			if (ImGui::Button(("Add " + std::string(stageName(StageKind(i)))).c_str(), {-1, 0}))
				attempt([&] { addCampaignStage(StageKind(i)); });
			mark(("campaign-add-" + std::to_string(i)).c_str());
		}
		if (ImGui::Button("Add current level", {-1, 0}))
			attempt([&] {
				if (serialize(p) != saved)
					saveAs(path);
				const auto level = importCampaignLevel(path, campaignPath.parent_path());
				addCampaignStage(StageKind::Level);
				campaign.stages.back().level = level;
				campaign.stages.back().name = p.name;
			});
		if (campaignStage >= 0 && campaignStage < int(campaign.stages.size())) {
			if (ImGui::Button("Duplicate stage"))
				attempt([&] {
					auto next = newStage(campaign, campaign.stages[campaignStage].kind);
					auto copy = campaign.stages[campaignStage];
					copy.id = next.id;
					copy.x = next.x;
					copy.y = next.y;
					copy.name += " copy";
					campaignRemember(campaign);
					campaign.stages.push_back(copy);
					campaignStage = int(campaign.stages.size()) - 1;
				});
			if (ImGui::Button("Delete stage")) {
				campaignRemember(campaign);
				const auto id = campaign.stages[campaignStage].id;
				campaign.stages.erase(campaign.stages.begin() + campaignStage);
				for (auto &s : campaign.stages)
					s.routes.erase(std::remove_if(s.routes.begin(), s.routes.end(),
					                              [&](auto &r) { return r.to == id; }),
					               s.routes.end());
				if (campaign.entry == id)
					campaign.entry.clear();
				campaignStage = -1;
				campaignLink = -1;
			}
		}
		if (ImGui::Button("Undo"))
			campaignHistory(false);
		ImGui::SameLine();
		if (ImGui::Button("Redo"))
			campaignHistory(true);
		before = campaign;
		if (designerComments(campaign.comments))
			campaignRemember(before);
		ImGui::SeparatorText("VALIDATION");
		for (const auto &error : validateCampaign(campaign))
			ImGui::TextWrapped("%s", error.c_str());
		if (ImGui::Button("Check files and exits"))
			attempt([&] {
				auto errors = validateCampaign(campaign, campaignPath.parent_path(), true);
				status =
				    errors.empty() ? "Campaign files and level exits validated" : errors.front();
			});
		ImGui::EndChild();
		ImGui::TableNextColumn();
		campaignGraph();
		ImGui::TableNextColumn();
		ImGui::BeginChild("campaign_inspector", {0, -35});
		campaignInspector();
		ImGui::EndChild();
		ImGui::EndTable();
	}
}
void storyPreview(Stage &s, float width, bool editable) {
	attempt([&] {
		if (storyImages.root != campaignPath.parent_path()) {
			storyImages.clear();
			storyImages.root = campaignPath.parent_path();
		}
		auto frame = renderStory(s, storyImages, SDL_GetTicks() - storyBegan, storyReveal);
		if (!storyTexture)
			storyTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
			                                 SDL_TEXTUREACCESS_STREAMING, 640, 360);
		if (!storyTexture)
			throw std::runtime_error(SDL_GetError());
		SDL_UpdateTexture(storyTexture, nullptr, frame.pixels.data(), 640 * 4);
		const auto at = ImGui::GetCursorScreenPos();
		const float scale = width / 640;
		ImGui::Image((ImTextureID)(intptr_t)storyTexture, {width, width * 360 / 640});
		mark("campaign-story-preview");
		if (editable) {
			ImGui::SetCursorScreenPos(at);
			ImGui::InvisibleButton("story-layout-drag", {width, width * 360 / 640});
			mark("campaign-story-drag");
			const auto mouse = ImGui::GetIO().MousePos;
			if (ImGui::IsItemClicked()) {
				storyDragging = false;
				for (int i = int(s.elements.size()) - 1; i >= 0; --i) {
					const auto &e = s.elements[i];
					const float x = (mouse.x - at.x) / scale, y = (mouse.y - at.y) / scale;
					if (x >= e.x && x < e.x + e.width && y >= e.y && y < e.y + e.height) {
						storyElement = i;
						break;
					}
				}
			}
			if (storyElement >= 0 && storyElement < int(s.elements.size())) {
				auto &e = s.elements[storyElement];
				if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0, 2)) {
					if (!storyDragging) {
						campaignRemember(campaign);
						storyDragging = true;
					}
					e.x = std::clamp(e.x + int(ImGui::GetIO().MouseDelta.x / scale), 0,
					                 640 - e.width);
					e.y = std::clamp(e.y + int(ImGui::GetIO().MouseDelta.y / scale), 0,
					                 360 - e.height);
				}
				ImGui::GetWindowDrawList()->AddRect(
				    {at.x + e.x * scale, at.y + e.y * scale},
				    {at.x + (e.x + e.width) * scale, at.y + (e.y + e.height) * scale},
				    IM_COL32(255, 210, 90, 255), 0, 0, 2);
			}
		}
	});
}
void storyWindow() {
	if (!storyWindowOpen)
		return;
	ImGui::SetNextWindowSize({800, 580}, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowBgAlpha(1.f);
	if (ImGui::Begin("Campaign screen layout", &storyWindowOpen)) {
		if (campaignStage >= 0 && campaignStage < int(campaign.stages.size())) {
			auto &s = campaign.stages[campaignStage];
			ImGui::TextWrapped("%s / click an element and drag to place it. The selected element "
			                   "has a gold border.",
			                   s.name.c_str());
			if (ImGui::Button("Restart effects"))
				storyBegan = SDL_GetTicks();
			ImGui::SameLine();
			ImGui::Checkbox("Reveal all", &storyReveal);
			storyPreview(s, ImGui::GetContentRegionAvail().x, true);
			ImGui::TextWrapped("Edit text, colors, size, effects and photo files in the stage "
			                   "inspector. Rectangles clip text and contain-fit photos. Undo/Redo "
			                   "lives in the Campaign workspace.");
		} else
			ImGui::TextWrapped("Select a story, choice or ending stage.");
	}
	ImGui::End();
}
