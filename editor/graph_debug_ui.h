// SPDX-License-Identifier: GPL-3.0-or-later
// Graph-only execution uses the gameplay evaluator. Only world inputs (use,
// proximity, actor events) are supplied manually; there is no second VM.
const GraphState *displayGraphState(const Module &m) const {
	if (monitoredGraph)
		return monitoredGraph;
	if (p.native.enabled && nativeMonitorGraph) {
		auto found = nativeGraphs.find(m.id);
		return found != nativeGraphs.end() && found->second.nodes.size() == m.nodes.size() &&
		               found->second.revision == moduleRevision(m)
		           ? &found->second
				   : nullptr;
	}
	return graphDebugModule == m.id && graphDebugOpen ? &graphDebugState : nullptr;
}
void resetGraphDebug(const Module &m) {
	graphDebugState = initialGraphState(m);
	graphDebugTick = 0;
	graphDebugModule = m.id;
	graphDebugOpen = true;
	graphDebugRunning = false;
	graphDebugLast = SDL_GetTicks();
	GraphMachine machine(m, graphDebugState, 0);
	machine.automatic = false;
	machine.start();
}
void graphDebugStep(const Module &m) {
	GraphMachine machine(m, graphDebugState, graphDebugTick);
	machine.automatic = false;
	if (!machine.step()) {
		++graphDebugTick;
		GraphMachine next(m, graphDebugState, graphDebugTick);
		next.automatic = false;
		next.advance();
	}
}
void graphDebugToolbar(const Module &m) {
	if (p.native.enabled) {
		ImGui::Checkbox("Monitor running native module", &nativeMonitorGraph);
		if (nativeMonitorGraph) {
			ImGui::TextDisabled("Read-only gameplay state; Pause native holds the world. Restart "
			                    "applies graph edits.");
			return;
		}
	}
	Project snapshot;
	snapshot.modules = {m};
	const auto signature = std::to_string(moduleRevision(m));
	if (signature != graphDebugSignature) {
		graphDebugSignature = signature;
		graphDebugOpen = graphDebugRunning = false;
		graphDebugModule.clear();
	}
	if (ImGui::Button(graphDebugOpen ? "Reset graph" : "Start graph"))
		resetGraphDebug(m);
	mark("graph-debug-start");
	ImGui::SameLine();
	ImGui::BeginDisabled(!graphDebugOpen);
	if (ImGui::Button(graphDebugRunning ? "Pause signals" : "Play signals")) {
		graphDebugRunning = !graphDebugRunning;
		graphDebugLast = SDL_GetTicks();
	}
	mark("graph-debug-play");
	ImGui::SameLine();
	if (ImGui::Button("Step signal / tick")) {
		graphDebugRunning = false;
		graphDebugStep(m);
	}
	mark("graph-debug-step");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(90);
	ImGui::SliderInt("Steps/sec", &graphDebugSpeed, 1, 30);
	ImGui::EndDisabled();
	if (graphDebugRunning && SDL_GetTicks() - graphDebugLast >= 1000u / graphDebugSpeed) {
		graphDebugLast = SDL_GetTicks();
		graphDebugStep(m);
	}
	if (graphDebugOpen)
		ImGui::Text("Tick %llu | %zu queued signals | green = active, gold wire = last delivery",
		            (unsigned long long)graphDebugTick, graphDebugState.pending.size());
	else
		ImGui::TextDisabled(
		    "Start runs initial states. Editing resets the simulation. No assets required.");
}
void graphTraceUI(const Module &m, const GraphState &state) {
	if (ImGui::TreeNodeEx("SIGNAL TRACE / newest first", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::BeginChild("signal_trace", {0, 180}, ImGuiChildFlags_Borders);
		for (auto it = state.trace.rbegin(); it != state.trace.rend(); ++it) {
			const auto &e = *it;
			if (e.node < 0 || e.node >= int(m.nodes.size()))
				continue;
			const char *event = e.kind == TraceKind::Signal     ? signalName(e.signal)
			                    : e.kind == TraceKind::Delivery ? inputName(e.input)
			                    : e.kind == TraceKind::Cycle    ? "Cycle bounded"
			                                                    : "State changed";
			const std::string label = std::to_string(e.tick) + " " + m.nodes[e.node].name + ": " +
			                          event + "##trace-" + std::to_string(e.sequence);
			if (ImGui::Selectable(label.c_str()))
				graphNode = e.node;
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Enabled %s | Active %s | Completed %s", e.enabled ? "yes" : "no",
				                  e.active ? "yes" : "no", e.completed ? "yes" : "no");
		}
		ImGui::EndChild();
		ImGui::TreePop();
	}
}
void graphDebugInspector(const Module &m) {
	if (p.native.enabled && nativeMonitorGraph) {
		if (const auto *state = displayGraphState(m))
			graphTraceUI(m, *state);
		else
			ImGui::TextWrapped(
			    "Launch or restart the native preview with this module to see live events.");
		return;
	}
	if (!graphDebugOpen || graphDebugModule != m.id)
		return;
	ImGui::SeparatorText("GRAPH PLAYTEST / EXTERNAL INPUT");
	if (graphNode >= 0 && graphNode < int(m.nodes.size())) {
		const auto &n = m.nodes[graphNode];
		const auto &s = graphDebugState.nodes[graphNode];
		ImGui::Text("%s | %s | %s", s.enabled ? "Enabled" : "Disabled",
		            s.active ? "ACTIVE" : "Idle", s.completed ? "Completed" : "Pending");
		if (n.kind == NodeKind::Actor)
			ImGui::Text("%s | %s", s.friendly ? "Friendly" : "Hostile",
			            s.patrol ? "Patrolling" : "Holding / native AI");
		if (n.kind == NodeKind::Counter)
			ImGui::Text("Count: %d / %d", s.count, n.goal);
		if (n.kind == NodeKind::Computer)
			ImGui::Text("Objectives: %zu / %d", s.progress.size(), n.goal);
		ImGui::BeginDisabled(!graphDebugState.pending.empty());
		GraphMachine machine(m, graphDebugState, graphDebugTick);
		machine.automatic = false;
		if (n.kind == NodeKind::Zone) {
			if (ImGui::Button("Simulate entering area"))
				machine.enter(graphNode);
		} else if (n.kind == NodeKind::Actor || n.kind == NodeKind::Npc ||
		           n.kind == NodeKind::Button || n.kind == NodeKind::Computer) {
			if (ImGui::Button("Simulate player Use"))
				machine.use(graphNode);
		}
		if (n.kind == NodeKind::Actor)
			for (auto output : nodeSignals(n.kind)) {
				if (ImGui::SmallButton((std::string("Simulate ") + signalName(output)).c_str()))
					machine.signal(graphNode, output);
			}
		int column = 0;
		for (auto input : nodeInputs(n.kind)) {
			if (column++ % 3)
				ImGui::SameLine();
			if (ImGui::SmallButton(inputName(input)))
				machine.send(graphNode, input);
		}
		ImGui::NewLine();
		if (n.kind == NodeKind::Dialogue && s.active) {
			ImGui::TextWrapped("%s: %s", n.speaker.c_str(), n.text.c_str());
			for (int choice = 0; choice < (n.choices[0].empty() ? 1 : 2); ++choice) {
				if (ImGui::Button(n.choices[0].empty() ? "Continue dialogue"
				                                       : n.choices[choice].c_str()))
					machine.finishDialogue(graphNode, s.generation, choice);
			}
		}
		if (!machine.message.empty())
			graphDebugMessage = machine.message;
		ImGui::TextWrapped("%s", graphDebugMessage.c_str());
		ImGui::EndDisabled();
		ImGui::TextWrapped("Drain queued wires before injecting another interaction. Step delivers "
		                   "one wire; with no wires it advances one 1/30-second tick.");
	}
	graphTraceUI(m, graphDebugState);
}
