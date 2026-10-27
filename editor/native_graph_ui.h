// SPDX-License-Identifier: GPL-3.0-or-later
void nativeGraphUI() {
	attempt([&] { ensureNativeReference(); });
	if (p.native.programs.empty() || p.native.puzzles.empty())
		return;
	if (ImGui::Button("Custom node modules / dialogues / state machines")) {
		nativeExtensionGraph = true;
		selected = -1;
		return;
	}

	if (ImGui::Button(nativeConnections ? "Actor state-machine rules"
	                                    : "Puzzle connections / ports")) {
		nativeConnections = !nativeConnections;
		graphFrameAll = true;
	}
	mark("native-view-toggle");
	if (nativeConnections) {
		nativeConnectionsUI();
		return;
	}
	ImGui::TextWrapped("NATIVE PUZZLES / lossless state-machine rules. Shared behavior edits "
	                   "affect every actor using that behavior. Rule edits apply on Restart; Apply "
	                   "live retains running puzzle state.");
	nativePuzzle = std::clamp(nativePuzzle, 0, int(p.native.puzzles.size()) - 1);
	auto &puzzle = p.native.puzzles[nativePuzzle];
	if (ImGui::BeginCombo("Puzzle module", puzzle.name.c_str())) {
		for (int i = 0; i < int(p.native.puzzles.size()); ++i)
			if (ImGui::Selectable((p.native.puzzles[i].name + "##" + std::to_string(i)).c_str(),
			                      i == nativePuzzle)) {
				nativePuzzle = i;
				nativeActor = p.native.puzzles[i].actors.front();
				nativeRulePage = 0;
			}
		ImGui::EndCombo();
	}
	// The combo may have selected another group; use it on the next frame.
	if (&puzzle != &p.native.puzzles[nativePuzzle])
		return;
	if (std::find(puzzle.actors.begin(), puzzle.actors.end(), nativeActor) == puzzle.actors.end())
		nativeActor = puzzle.actors.front();
	if (ImGui::BeginTable("native_graph_columns", 3,
	                      ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
		ImGui::TableSetupColumn("Puzzle", ImGuiTableColumnFlags_WidthFixed, 230);
		ImGui::TableSetupColumn("Rules", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Transition", ImGuiTableColumnFlags_WidthFixed, 310);
		ImGui::TableNextColumn();
		ImGui::BeginChild("native_puzzle_info", {0, -35});
		auto before = p;
		if (textField("Module name", puzzle.name, 160))
			history.remember(before);
		if (ImGui::Button("Module help / comments"))
			moduleHelpOpen = true;
		ImGui::TextWrapped("Connected actors, across rooms. Links to player/inventory are external "
		                   "ports. Group names are yours to describe the puzzle.");
		for (int id : puzzle.actors)
			if (ImGui::Selectable(("Actor " + std::to_string(id) + " / state " +
			                       std::to_string(p.native.actors[id].type))
			                          .c_str(),
			                      nativeActor == id)) {
				nativeActor = id;
				nativeRulePage = 0;
				nativeRule = 0;
			}
		auto &actor = p.native.actors[nativeActor];
		if (puzzle.actors.size() > 1 && ImGui::Button("Split actor into new module")) {
			edit();
			const int id = nativeActor;
			puzzle.actors.erase(std::remove(puzzle.actors.begin(), puzzle.actors.end(), id),
			                    puzzle.actors.end());
			p.native.puzzles.push_back({"Actor " + std::to_string(id) + " module", {id}});
			nativePuzzle = int(p.native.puzzles.size()) - 1;
			ImGui::EndChild();
			ImGui::EndTable();
			return;
		}
		if (ImGui::BeginCombo("Move actor into module", "Choose target...")) {
			for (int group = 0; group < int(p.native.puzzles.size()); ++group)
				if (group != nativePuzzle &&
				    ImGui::Selectable(p.native.puzzles[group].name.c_str())) {
					edit();
					p.native.puzzles[group].actors.push_back(nativeActor);
					puzzle.actors.erase(
					    std::remove(puzzle.actors.begin(), puzzle.actors.end(), nativeActor),
					    puzzle.actors.end());
					if (puzzle.actors.empty()) {
						p.native.puzzles.erase(p.native.puzzles.begin() + nativePuzzle);
						if (group > nativePuzzle)
							--group;
					}
					nativePuzzle = group;
					ImGui::EndCombo();
					ImGui::EndChild();
					ImGui::EndTable();
					return;
				}
			ImGui::EndCombo();
		}
		ImGui::SeparatorText("DEPENDENCIES");
		for (int id : nativeDependencies(p.native, nativeActor)) {
			const bool external =
			    std::find(puzzle.actors.begin(), puzzle.actors.end(), id) == puzzle.actors.end();
			ImGui::Text("Actor %d%s", id, external ? " (external)" : "");
		}
		if (ImGui::Button("Locate actor")) {
			if (actor.room < 64)
				room = actor.room;
			setWorkflow(Workflow::Placement);
		}
		ImGui::SeparatorText("LIVE NATIVE TRACE");
		ImGui::TextWrapped("Play native from the level or cursor, then return here. Matching "
		                   "executed rules light up. Click a trace to inspect its transition.");
		for (auto e = nativeEvents.rbegin(); e != nativeEvents.rend(); ++e) {
			if (std::find(puzzle.actors.begin(), puzzle.actors.end(), e->actor) ==
			    puzzle.actors.end())
				continue;
			if (ImGui::Selectable((std::to_string(e->actor) + ": " + std::to_string(e->state) +
			                       " -> " + std::to_string(e->next) + "##" +
			                       std::to_string(e->serial))
			                          .c_str())) {
				nativeActor = e->actor;
				nativeRule = e->rule;
				nativeRulePage = e->rule / 12;
			}
		}
		ImGui::EndChild();
		ImGui::TableNextColumn();
		auto &a = p.native.actors[nativeActor];
		auto &program = p.native.programs.at(a.behavior);
		nativeRule = std::clamp(nativeRule, 0, int(program.rules.size()) - 1);
		nativeRulePage = std::clamp(nativeRulePage, 0, int(program.rules.size() - 1) / 12);
		ImGui::Text("Actor %d | behavior %d | %zu rules", nativeActor, a.behavior,
		            program.rules.size());
		ImGui::SliderInt("Rule page", &nativeRulePage, 0, int(program.rules.size() - 1) / 12);
		Module display;
		display.id = "native-behavior";
		display.kind = Kind::Graph;
		const int first = nativeRulePage * 12,
		          last = std::min(first + 12, int(program.rules.size()));
		for (int row = first; row < last; ++row) {
			const auto &r = program.rules[row];
			Node n;
			n.id = std::to_string(row);
			n.kind = NodeKind::State;
			n.enabled = r.enabled;
			n.name = "#" + std::to_string(row) + " State " + std::to_string(r.type) + " -> " +
			         std::to_string(r.nextType);
			n.graphX = ((row - first) % 3) * 260 + 24;
			n.graphY = ((row - first) / 3) * 180 + 24;
			n.text = std::string("C1 ") + nativeOpcodeName(r.opcode[0]);
			n.doneText = std::string("C2 ") + nativeOpcodeName(r.opcode[1]);
			n.speaker = std::string("Do ") + nativeOpcodeName(r.opcode[2]);
			display.nodes.push_back(n);
			if (r.next >= first && r.next < last)
				display.wires.push_back(
				    {n.id, std::to_string(r.next), Signal::Completed, Input::Activate});
		}
		GraphState live = initialGraphState(display);
		for (auto e = nativeEvents.rbegin(); e != nativeEvents.rend(); ++e)
			if (e->actor == nativeActor) {
				if (e->rule >= first && e->rule < last)
					live.nodes[e->rule - first].active = true;
				break;
			}
		for (const auto &e : nativeEvents)
			if (e.actor == nativeActor)
				for (int w = 0; w < int(display.wires.size()); ++w)
					if (display.wires[w].from == std::to_string(e.rule))
						live.trace.push_back({e.serial, 0, TraceKind::Delivery, -1, w});
		graphNode = nativeRule >= first && nativeRule < last ? nativeRule - first : -1;
		monitoredGraph = &live;
		graphReadOnly = true;
		graphCanvas(display);
		graphReadOnly = false;
		monitoredGraph = nullptr;
		if (graphNode >= 0)
			nativeRule = first + graphNode;
		ImGui::TableNextColumn();
		ImGui::BeginChild("native_rule_properties", {0, -35});
		auto &r = program.rules[nativeRule];
		before = p;
		if (ImGui::Button("Node help / comments"))
			nodeHelpOpen = true;
		ImGui::Text("RULE %d / STATE %d", nativeRule, r.type);
		bool changed = ImGui::Checkbox("Rule enabled", &r.enabled);
		mark("native-rule-enabled");
		ImGui::TextWrapped("Conditions 1 AND 2 must pass, then Action executes. The transition "
		                   "selects the next animation state and its first rule. Other rows for "
		                   "the same state are fallback alternatives, evaluated in order.");
		for (int slot = 0; slot < 3; ++slot) {
			ImGui::PushID(slot);
			if (ImGui::BeginCombo(slot < 2 ? (slot == 0 ? "Condition 1" : "Condition 2") : "Action",
			                      nativeOpcodeName(r.opcode[slot]))) {
				for (int op = 0; op < 140; ++op) {
					const auto &base =
					    nativeDefaults.programs[nativeProgramSource(p.native, a.behavior)].rules;
					if (op != 0 && op != 0x3e && op != 0x55 && op != 0x56 &&
					    std::none_of(base.begin(), base.end(), [&](const auto &r) {
						    return std::find(r.opcode.begin(), r.opcode.end(), op) !=
							       r.opcode.end();
					    }))
						continue;
					if (ImGui::Selectable(nativeOpcodeName(op), op == r.opcode[slot])) {
						r.opcode[slot] = op;
						changed = true;
					}
				}
				ImGui::EndCombo();
			}
			changed |= ImGui::InputInt("Operand", &r.argument[slot]);
			r.argument[slot] = std::clamp(r.argument[slot], -32768, 32767);
			ImGui::PopID();
		}
		if (ImGui::BeginCombo("Next rule", ("#" + std::to_string(r.next) + " / state " +
		                                    std::to_string(r.nextType))
		                                       .c_str())) {
			for (int next = 0; next < int(program.rules.size()); ++next)
				if (ImGui::Selectable(("#" + std::to_string(next) + " / state " +
				                       std::to_string(program.rules[next].type))
				                          .c_str(),
				                      next == r.next)) {
					r.next = next;
					r.nextType = program.rules[next].type;
					changed = true;
				}
			ImGui::EndCombo();
		}
		if (ImGui::Button("Follow transition")) {
			nativeRule = r.next;
			nativeRulePage = r.next / 12;
		}
		changed |= ImGui::SliderInt("Move X", &r.dx, -128, 127);
		changed |= ImGui::SliderInt("Move Y", &r.dy, -128, 127);
		changed |= ImGui::SliderInt("Flags", &r.flags, 0, 255);
		ImGui::TextWrapped("Flags: 1 flip facing, 2 lose health, 4 gain health, 8 kill; high "
		                   "nibble selects native score reward. Operands retaining opaque engine "
		                   "indices are checked against the installed edition before play.");
		if (changed)
			history.remember(before);
		if (program.rules.size() < 4096 && ImGui::Button("Duplicate rule as new branch")) {
			edit();
			auto copy = r;
			program.rules.push_back(copy);
			nativeRule = int(program.rules.size()) - 1;
			nativeRulePage = nativeRule / 12;
		}
		ImGui::TextWrapped("A duplicated rule is appended with a stable new index. Connect an "
		                   "existing transition to it using Next rule. Disable obsolete rules "
		                   "instead of deleting referenced indices.");
		ImGui::EndChild();
		ImGui::EndTable();
	}
}
