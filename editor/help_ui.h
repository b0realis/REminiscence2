// SPDX-License-Identifier: GPL-3.0-or-later
bool designerComments(std::string &value) {
	ImGui::SeparatorText("DESIGNER COMMENTS (saved with this object)");
	char buffer[2048];
	std::snprintf(buffer, sizeof(buffer), "%s", value.c_str());
	if (ImGui::InputTextMultiline("##designer-comments", buffer, sizeof(buffer), {-1, 130})) {
		value = buffer;
		return true;
	}
	return false;
}
void helpWindows() {
	const bool native = p.native.enabled && !nativeExtensionGraph && workflow == Workflow::Module;
	if (moduleHelpOpen) {
		ImGui::SetNextWindowSize({570, 540}, ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowBgAlpha(1.f);
		if (ImGui::Begin("Module help and notes", &moduleHelpOpen)) {
			auto before = p;
			if (native && nativePuzzle >= 0 && nativePuzzle < int(p.native.puzzles.size())) {
				auto &m = p.native.puzzles[nativePuzzle];
				ImGui::TextWrapped("%s", m.name.c_str());
				ImGui::TextWrapped(
				    "A connected original puzzle. Fixed dependencies appear as teal arrows; "
					"delivered native messages animate in gold. Move/copy includes hidden helpers. "
					"Each original actor keeps its ID and resource identity.");
				ImGui::SeparatorText("ACTOR INPUTS / OUTGOING DEPENDENCIES");
				for (int id : m.actors) {
					ImGui::Text("Actor %d", id);
					for (const auto &port : nativePorts(p.native, id))
						ImGui::BulletText("%s -> %d", port.label.c_str(), port.target);
				}
				if (designerComments(m.comments))
					history.remember(before);
			} else if (selected >= 0 && selected < int(p.modules.size())) {
				auto &m = p.modules[selected];
				ImGui::TextWrapped("%s / %s", m.name.c_str(), kindName(m.kind));
				ImGui::TextWrapped("A reusable module owns its nodes, connections, assets and "
				                   "puzzle state. Copy/Duplicate makes an independent instance. "
				                   "Rename edits the title; Delete removes its links too. Save to "
				                   "library makes a reusable template. Undo restores edits.");
				ImGui::SeparatorText("NODE INPUTS AND OUTPUTS");
				for (const auto &n : m.nodes) {
					if (ImGui::TreeNode((n.name + "##" + n.id).c_str())) {
						ImGui::TextWrapped("%s", nodeHelp(n.kind));
						for (auto i : nodeInputs(n.kind))
							ImGui::BulletText("IN: %s", inputName(i));
						for (auto s : nodeSignals(n.kind))
							ImGui::BulletText("OUT: %s", signalName(s));
						ImGui::TreePop();
					}
				}
				if (m.kind != Kind::Graph)
					ImGui::TextWrapped(
					    "Legacy module: door takes interaction at its activator; health pickup "
						"collects on proximity; crawler uses native AI; prop is visual. Outputs "
						"open linked doors. Convert a door to nodes for explicit ports.");
				if (designerComments(m.comments))
					history.remember(before);
			} else
				ImGui::TextUnformatted("Select a module to view its help.");
		}
		ImGui::End();
	}
	if (nodeHelpOpen) {
		ImGui::SetNextWindowSize({570, 580}, ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowBgAlpha(1.f);
		if (ImGui::Begin("Node help and notes", &nodeHelpOpen)) {
			auto before = p;
			if (native && nativeActor >= 0 && nativeActor < int(p.native.actors.size())) {
				auto &a = p.native.actors[nativeActor];
				ImGui::Text("Native actor %d", nativeActor);
				ImGui::TextWrapped("%s", nativeActorRole(p.native, nativeActor).c_str());
				ImGui::TextWrapped("Inputs: native messages and the rule's conditions. Outputs: "
				                   "state transitions, messages and actions. Typed ports below "
				                   "expose fixed actor/room dependencies. Dynamic collision and "
				                   "inventory lookups are monitored during play.");
				for (const auto &port : nativePorts(p.native, nativeActor))
					ImGui::BulletText("%s -> %d", port.label.c_str(), port.target);
				if (!nativeConnections && a.behavior < int(p.native.programs.size()) &&
				    nativeRule < int(p.native.programs[a.behavior].rules.size())) {
					auto &r = p.native.programs[a.behavior].rules[nativeRule];
					ImGui::Text("Rule %d: state %d -> %d; next row %d", nativeRule, r.type,
					            r.nextType, r.next);
					for (int i = 0; i < 3; ++i)
						ImGui::BulletText("%s: %s (%d)", i < 2 ? "Condition" : "Action",
						                  nativeOpcodeName(r.opcode[i]), r.argument[i]);
					ImGui::TextWrapped("Rule notes belong to this shared behavior table. Isolate "
					                   "the behavior before giving one actor different logic.");
					if (designerComments(r.comments))
						history.remember(before);
				} else if (designerComments(a.comments))
					history.remember(before);
			} else if (selected >= 0 && selected < int(p.modules.size()) && graphNode >= 0 &&
			           graphNode < int(p.modules[selected].nodes.size())) {
				auto &n = p.modules[selected].nodes[graphNode];
				ImGui::TextWrapped("%s / %s", n.name.c_str(), nodeName(n.kind));
				ImGui::TextWrapped("%s", nodeHelp(n.kind));
				ImGui::SeparatorText("INPUTS");
				for (auto i : nodeInputs(n.kind))
					ImGui::BulletText("%s", inputName(i));
				ImGui::SeparatorText("OUTPUTS");
				for (auto s : nodeSignals(n.kind))
					ImGui::BulletText("%s", signalName(s));
				if (nodeSignals(n.kind).empty())
					ImGui::TextDisabled("No outgoing signals");
				if (designerComments(n.comments))
					history.remember(before);
			} else
				ImGui::TextUnformatted("Select a node to view its help.");
		}
		ImGui::End();
	}
}
void updateGraphPulses(const Module &m, const GraphState *state) {
	std::string key = m.id + ":" + std::to_string(moduleRevision(m));
	if (graphReadOnly) {
		key = m.id + ":" + std::to_string(nativeActor) + ":" + std::to_string(nativePuzzle);
		for (const auto &n : m.nodes)
			key += ":" + n.id;
		for (const auto &w : m.wires)
			key += ":" + w.from + ">" + w.to;
	}
	if (pulseContext != key || !state ||
	    (!state->trace.empty() && state->trace.back().sequence < pulseSequence)) {
		pulseContext = key;
		pulseSequence = 0;
		wirePulses.clear();
		pulseStates.clear();
		statePulses.clear();
	}
	if (!state)
		return;
	const double now = ImGui::GetTime();
	wirePulses.erase(std::remove_if(wirePulses.begin(), wirePulses.end(),
	                                [&](auto p) { return now - p.second > 1.2; }),
	                 wirePulses.end());
	for (const auto &e : state->trace)
		if (e.sequence > pulseSequence) {
			if (e.kind == TraceKind::Delivery && e.wire >= 0) {
				if (wirePulses.size() >= 64)
					wirePulses.erase(wirePulses.begin());
				wirePulses.emplace_back(e.wire, now);
			}
			pulseSequence = e.sequence;
		}
	statePulses.resize(state->nodes.size(), -100);
	for (size_t i = 0; i < state->nodes.size(); ++i) {
		const auto &s = state->nodes[i];
		const int flags = s.enabled | (s.active << 1) | (s.completed << 2) | (s.collected << 3) |
		                  (s.friendly << 4) | (s.held << 5) | ((s.count & 65535) << 6);
		if (i >= pulseStates.size()) {
			pulseStates.push_back(flags);
			continue;
		}
		if (pulseStates[i] != flags) {
			statePulses[i] = now;
			pulseStates[i] = flags;
		}
	}
}
