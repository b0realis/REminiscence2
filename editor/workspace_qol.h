// SPDX-License-Identifier: GPL-3.0-or-later
// Workspace helpers keep selection, locking and diagnostics independent of UI order.
bool moduleLocked(const Module &m) const {
	return locked[int(m.layer)] || std::any_of(m.nodes.begin(), m.nodes.end(),
	                                           [&](auto &n) { return locked[int(n.layer)]; });
}
void reconcileSelection() {
	selection.erase(std::remove_if(selection.begin(), selection.end(),
	                               [&](auto &id) {
		                               return std::none_of(p.modules.begin(), p.modules.end(),
									                       [&](auto &m) { return m.id == id; });
	                               }),
	                selection.end());
	if (selected >= 0 && selected < int(p.modules.size()) && selection.empty())
		selection.push_back(p.modules[selected].id);
}
void copySelection() {
	reconcileSelection();
	clipboard = copyModules(p, selection, images.root, p.rooms[room].id);
	status = "Copied " + std::to_string(clipboard.modules.size()) + " complete modules";
}
void pasteSelection() {
	if (std::any_of(clipboard.modules.begin(), clipboard.modules.end(),
	                [&](auto &m) { return moduleLocked(m); }))
		throw std::runtime_error("Unlock the destination layers before pasting");
	auto next = p;
	auto ids = pasteModules(next, clipboard, images.root, p.rooms[room].id);
	if (ids.empty())
		return;
	edit();
	p = std::move(next);
	selection = std::move(ids);
	selected = int(p.modules.size() - selection.size());
	part = 0;
	images.clear();
	status = "Pasted modules with independent IDs and internal links";
}
void deleteSelection() {
	reconcileSelection();
	if (selection.empty())
		return;
	for (auto &m : p.modules)
		if (std::find(selection.begin(), selection.end(), m.id) != selection.end() &&
		    moduleLocked(m))
			throw std::runtime_error("Unlock selected layers before deleting");
	edit();
	auto contains = [&](auto &id) {
		return std::find(selection.begin(), selection.end(), id) != selection.end();
	};
	p.modules.erase(
	    std::remove_if(p.modules.begin(), p.modules.end(), [&](auto &m) { return contains(m.id); }),
	    p.modules.end());
	for (auto &m : p.modules)
		m.outputs.erase(std::remove_if(m.outputs.begin(), m.outputs.end(), contains),
		                m.outputs.end());
	selected = -1;
	selection.clear();
	setWorkflow(Workflow::Placement);
}
void focusIssue(const Issue &issue) {
	int target = roomIndex(p, issue.room);
	if (target >= 0)
		room = target;
	selected = -1;
	nativeActor = issue.actor;
	setWorkflow(Workflow::Placement);
	world = false;
	selection.clear();
	for (int i = 0; i < int(p.modules.size()); ++i)
		if (p.modules[i].id == issue.module)
			selected = i;
	if (selected >= 0) {
		selection = {p.modules[selected].id};
		graphNode = nodeIndex(p.modules[selected], issue.node);
		if (graphNode >= 0) {
			setWorkflow(Workflow::Module);
			part = graphNode;
			graphFrameAll = true;
		}
	}
	if (nativeActor >= 0 && nativeActor < int(p.native.actors.size()) &&
	    p.native.actors[nativeActor].room < 64)
		room = p.native.actors[nativeActor].room;
	if (selected < 0 && nativeActor < 0)
		setWorkflow(Workflow::Level);
	status = issue.message;
}
void issueList(const std::vector<Issue> &issues) {
	for (size_t i = 0; i < issues.size(); ++i) {
		ImGui::PushID(int(i));
		if (ImGui::SmallButton("Focus"))
			pendingIssue = issues[i];
		mark(("issue-" + std::to_string(i)).c_str());
		ImGui::SameLine();
		ImGui::TextColored(issues[i].error ? ImVec4(1, .52f, .38f, 1) : ImVec4(.95f, .82f, .42f, 1),
		                   "%s", issues[i].error ? "Error" : "Note");
		ImGui::TextWrapped("%s", issues[i].message.c_str());
		ImGui::Separator();
		ImGui::PopID();
	}
}
void validateProject() {
	validationResults = validate(p);
	auto pictures = imageIssues(p, images);
	validationResults.insert(validationResults.end(), pictures.begin(), pictures.end());
	if (p.native.enabled) {
		try {
			ensureNativeReference();
			validateNativeLogic(p.native, nativeDefaults);
		} catch (const std::exception &e) {
			validationResults.push_back({true, e.what(), "", "", "", nativeActor});
		}
	}
	validationOpen = true;
	status = validationResults.empty() ? "Project, image and audio dependencies validated"
	                                   : "Select a validation result to locate it";
}
void validationUI() {
	if (!validationOpen)
		return;
	ImGui::SetNextWindowSize({570, 400}, ImGuiCond_Appearing);
	if (ImGui::Begin("Project validation", &validationOpen)) {
		if (ImGui::Button("Refresh"))
			validateProject();
		if (validationResults.empty())
			ImGui::TextUnformatted("No issues found.");
		issueList(validationResults);
	}
	ImGui::End();
}
