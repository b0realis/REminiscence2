// SPDX-License-Identifier: GPL-3.0-or-later
// Workspace member functions. Kept separate from room/collision authoring UI.
void openGraph() {
	if (selected < 0 || selected >= int(p.modules.size()))
		return;
	nativeExtensionGraph = p.native.enabled;
	setWorkflow(Workflow::Module);
	graphNode = p.modules[selected].nodes.empty()
	                ? -1
	                : std::clamp(part, 0, int(p.modules[selected].nodes.size()) - 1);
	pendingFrom = -1;
	graphDrag = false;
}
void convertDoor() {
	if (selected < 0 || p.modules[selected].kind != Kind::Door)
		return;
	edit();
	auto &m = p.modules[selected];
	auto door = makeNode(m, NodeKind::Door, m.a);
	door.width = m.width;
	door.height = m.height;
	door.initiallyActive = m.initiallyOpen;
	m.nodes.push_back(door);
	auto button = makeNode(m, NodeKind::Button, m.b);
	button.at = m.b; // The old activator anchor was centered; graph sprites use a left edge.
	button.width = 12;
	button.at.x = std::clamp(m.b.x - 6, 0, 244);
	m.nodes.push_back(button);
	m.wires.push_back({button.id, door.id, Signal::Used, m.toggle ? Input::Toggle : Input::Open});
	m.kind = Kind::Graph;
	m.nodes[0].graphX = 264;
	m.nodes[1].graphX = 24;
	openGraph();
}
void nodeInspector(Module &m) {
	ImGui::SeparatorText("NODE INSPECTOR");
	if (graphNode < 0 || graphNode >= int(m.nodes.size())) {
		ImGui::TextWrapped("Select a node to edit its behavior and appearance.");
		return;
	}
	auto before = p;
	auto &n = m.nodes[graphNode];
	bool changed = textField("Name", n.name, 160);
	ImGui::TextWrapped("%s | %s", nodeName(n.kind), n.id.c_str());
	if (ImGui::Button("Node help / comments"))
		nodeHelpOpen = true;
	mark("node-help");
	changed |= ImGui::Checkbox("Enabled", &n.enabled);
	if (n.kind == NodeKind::HealthPickup || n.kind == NodeKind::Damage)
		changed |= ImGui::SliderInt("Amount", &n.health, 1, 100);
	if (n.kind == NodeKind::LevelExit)
		changed |= textField("Exit route", n.text, 80);
	if (n.kind >= NodeKind::HealthPickup && n.kind < NodeKind::Music)
		ImGui::TextWrapped("%s", nodeHelp(n.kind));
	if (n.kind == NodeKind::Item || n.kind == NodeKind::Npc || n.kind == NodeKind::ItemCondition ||
	    n.kind == NodeKind::CampaignSet || n.kind == NodeKind::CampaignTest) {
		changed |= textField("Campaign key", n.campaignKey, 65);
		ImGui::TextWrapped(
		    "Same key shares this item or flag across modules and levels. Blank item keys remain "
		    "module-local. Use letters, digits, dot, underscore or hyphen.");
		if (n.kind == NodeKind::CampaignSet)
			changed |= ImGui::Checkbox("Write true (unchecked: false)", &n.initiallyActive);
	}
	if (n.kind == NodeKind::Actor) {
		if (p.native.enabled) {
			changed |= ImGui::SliderInt("Native actor ID", &n.nativeActor, 1,
			                            int(p.native.actors.size()) - 1);
			changed |= ImGui::InputInt("Watch rule (-1 = any)", &n.nativeRule);
			n.nativeRule = std::clamp(n.nativeRule, -1, 65535);
			changed |= ImGui::SliderInt("Message on Activate", &n.message, 0, 255);
			changed |= ImGui::SliderInt("Message sender", &n.messageFrom, 0,
			                            int(p.native.actors.size()) - 1);
			ImGui::TextWrapped(
			    "Bind an existing native actor (enemy, door, switch, helper). Native rule fired "
			    "emits after the watched rule executes; Activate sends the specified native "
			    "message. Native logic still runs unless Friendly is enabled.");
		}
		changed |= ImGui::Checkbox("Friendly (no attack AI)", &n.friendly);
		changed |= ImGui::Checkbox("Patrol bounds", &n.patrol);
		changed |= ImGui::SliderInt("Patrol left", &n.patrolLeft, 0, n.patrolRight - 16);
		changed |= ImGui::SliderInt("Patrol right", &n.patrolRight, n.patrolLeft + 16, 240);
		changed |= ImGui::SliderInt("Friendly walk speed", &n.speed, 1, 4);
		changed |= ImGui::SliderInt("Health", &n.health, 1, 100);
		ImGui::TextWrapped("Uses native crawler artwork/AI. Friendly actors use Enter to emit "
		                   "Used: connect to Dialogue or another action. Hurt, Died and patrol "
		                   "endpoints are events. Hostile actors retain native AI within patrol "
		                   "bounds; Friendly/Hostile and Start patrol/Hold position can be wired.");
	}
	if (n.kind == NodeKind::State) {
		changed |= textField("Machine group", n.stateGroup, 80);
		changed |= ImGui::Checkbox("Initial state", &n.initiallyActive);
		ImGui::TextWrapped(
		    "States in the same group are mutually exclusive. Activate enters this state and exits "
		    "its sibling. Progress emits Completed only while active. Wire Completed to the next "
		    "state's Activate; Entered/Exited drive and stop actions.");
	}
	if (n.kind == NodeKind::Door)
		changed |= ImGui::Checkbox("Initially open", &n.initiallyActive);
	if (n.kind == NodeKind::Button || n.kind == NodeKind::Zone || n.kind == NodeKind::Relay ||
	    GraphMachine::action(n.kind) || n.kind == NodeKind::Damage ||
	    n.kind == NodeKind::Teleport || n.kind == NodeKind::Checkpoint)
		changed |= ImGui::Checkbox("One shot", &n.once);
	if (n.kind == NodeKind::Computer || n.kind == NodeKind::Counter) {
		changed |= ImGui::SliderInt("Objectives", &n.goal, 1, 32);
		ImGui::TextWrapped(n.kind == NodeKind::Counter
		                       ? "Progress counts every signal until the goal; Reset clears it."
							   : "Use the terminal to start. Progress counts distinct source "
							     "nodes, even when completed before acceptance.");
	}
	if (n.kind == NodeKind::Npc || n.kind == NodeKind::ItemCondition) {
		int item = nodeIndex(m, n.requiredItem);
		if (ImGui::BeginCombo("Required item", n.requiredItem.empty() ? "None"
		                                       : item < 0 ? "MISSING ITEM"
		                                                  : m.nodes[item].name.c_str())) {
			if (ImGui::Selectable("None", n.requiredItem.empty())) {
				n.requiredItem.clear();
				changed = true;
			}
			for (const auto &candidate : m.nodes)
				if (candidate.kind == NodeKind::Item)
					if (ImGui::Selectable((candidate.name + "##" + candidate.id).c_str(),
					                      candidate.id == n.requiredItem)) {
						n.requiredItem = candidate.id;
						changed = true;
					}
			ImGui::EndCombo();
		}
		changed |= ImGui::Checkbox("Consume item on completion", &n.consumeItem);
		ImGui::TextWrapped("Or connect an item's Completed port to the gold Needs item port. Talk "
		                   "with Enter to hand it in.");
	}
	if (n.kind == NodeKind::Npc || n.kind == NodeKind::Computer || n.kind == NodeKind::Dialogue) {
		auto multiline = [&](const char *label, std::string &value) {
			char text[1025];
			std::snprintf(text, sizeof(text), "%s", value.c_str());
			if (ImGui::InputTextMultiline(label, text, sizeof(text), {-1, 70})) {
				value = text;
				changed = true;
			}
		};
		ImGui::TextUnformatted(n.kind == NodeKind::Dialogue ? "Dialogue text (ASCII in game)"
		                                                    : "Before completion (ASCII in game)");
		multiline("##before", n.text);
		if (n.kind == NodeKind::Dialogue) {
			changed |= textField("Speaker", n.speaker, 28);
			changed |= textField("Choice 1", n.choices[0], 26);
			changed |= textField("Choice 2", n.choices[1], 26);
			ImGui::TextWrapped("Leave both choices empty for Continue. Otherwise supply both; "
			                   "Up/Down selects, Enter confirms. Wire Choice 1/2 to the next "
			                   "dialogue or action. Completed fires only after dismissal.");
		} else {
			ImGui::TextUnformatted("After completion");
			multiline("##after", n.doneText);
		}
	}
	if (n.kind == NodeKind::Timer) {
		changed |= ImGui::SliderInt("Delay (ticks)", &n.duration, 1, 18000);
		ImGui::Text("%.2f seconds at 30 Hz", n.duration / 30.f);
	}
	if ((GraphMachine::action(n.kind) && n.kind != NodeKind::Music && n.kind != NodeKind::Sound) ||
	    n.kind == NodeKind::Damage || n.kind == NodeKind::Teleport ||
	    n.kind == NodeKind::Checkpoint)
		ImGui::TextWrapped(
		    "Activate triggers the action; Stop cancels without Completed. Reset also rearms One "
		    "shot. Disable stops a running action. Repeated Activate while running is ignored.");
	if (n.kind == NodeKind::PickupAnimation)
		ImGui::TextWrapped(
		    "Visual pickup effect at this anchor, visible only while running. Connect a Quest item "
		    "Completed output to Activate. This does not override Conrad movement.");
	if (n.kind == NodeKind::Music || n.kind == NodeKind::Sound) {
		ImGui::SeparatorText("AUDIO CLIP");
		ImGui::TextWrapped("%s",
		                   n.audioFile.empty() ? "Import a clip to begin" : n.audioFile.c_str());
		ImGui::Text("Duration: %.2f s", n.duration / 30.f);
		changed |= ImGui::SliderInt("Clip volume", &n.audioVolume, 0, 100);
		changed |= ImGui::Checkbox("Loop until Stop", &n.audioLoop);
		ImGui::TextWrapped(
		    "16-bit PCM WAV / Ogg Vorbis. Music uses Music volume; Sound uses Effects. Connect On "
		    "start or any trigger to Activate. Looping tracks need Stop to transition.");
		if (ImGui::Button("Import audio...")) {
			const auto mid = m.id, nid = n.id;
			choose([&, mid, nid](auto file) {
				const auto clip = readAudio(file);
				const auto stored = importAudio(file, images.root);
				for (auto &module : p.modules)
					if (module.id == mid) {
						const int at = nodeIndex(module, nid);
						if (at < 0)
							return;
						edit();
						module.nodes[at].audioFile = stored;
						module.nodes[at].duration = clip.ticks();
						return;
					}
			});
		}
		mark("audio-import");
		ImGui::SameLine();
		if (ImGui::Button("Listen"))
			attempt([&] {
				audition.play(readAudio(assetPath(images.root, n.audioFile)), n.audioVolume);
			});
		mark("audio-listen");
		if (ImGui::Button("Stop listening"))
			audition.stop();
		mark("audio-stop");
		ImGui::TextWrapped(
		    "Listen previews once. Graph simulation shows timing; PLAY auditions the wired scene.");
	}
	if (n.kind != NodeKind::Music && n.kind != NodeKind::Sound) {
		ImGui::SeparatorText("ROOM PLACEMENT");
		if (ImGui::BeginCombo("Room", n.at.room.c_str())) {
			for (const auto &r : p.rooms)
				if (ImGui::Selectable((r.name + "##" + r.id).c_str(), n.at.room == r.id)) {
					n.at.room = r.id;
					changed = true;
				}
			ImGui::EndCombo();
		}
		changed |= ImGui::SliderInt("X", &n.at.x, 0, 256 - n.width);
		changed |= ImGui::SliderInt("Feet Y", &n.at.y, 0, 216);
		changed |= ImGui::SliderInt("Width", &n.width, 1, 128);
		changed |= ImGui::SliderInt("Height", &n.height, 1, 216);
		if (n.kind == NodeKind::Door) {
			n.at.x = n.at.x / 16 * 16;
			n.width = std::max(16, n.width / 16 * 16);
			n.height = std::max(72, n.height / 72 * 72);
			n.at.y = std::clamp((n.at.y + 36) / 72 * 72, n.height, 216);
		}
		n.at.x = std::clamp(n.at.x, 0, 256 - n.width);
		int layer = int(n.layer);
		if (ImGui::Combo("Layer", &layer, "Back\0Playfield\0Front\0")) {
			n.layer = Layer(layer);
			changed = true;
		}
		if (ImGui::Button("Locate in room")) {
			room = roomIndex(p, n.at.room);
			part = graphNode;
			setWorkflow(Workflow::Placement);
			tool = Tool::Select;
		}
		ImGui::SeparatorText("SPRITES / ANIMATION");
		ImGui::TextWrapped(
		    "PNG / JPEG / BMP; horizontal frame strip. Active falls back to idle. No "
		    "original campaign artwork is exported.");
		for (int state = 0; state < 2; ++state) {
			ImGui::PushID(state);
			const bool openClip = ImGui::TreeNode(state ? "Active / completed" : "Idle / closed");
			mark(state ? "clip-active" : "clip-idle");
			if (openClip) {
				auto &a = n.visuals[state];
				if (ImGui::BeginCombo("Module asset",
				                      a.art.file.empty() ? "Choose..." : a.art.file.c_str())) {
					for (const auto &asset : m.assets)
						if (ImGui::Selectable(asset.file.c_str(), a.art.file == asset.file)) {
							a.art = asset;
							a.frames = 1;
							changed = true;
						}
					ImGui::EndCombo();
				}
				ImGui::TextWrapped("%s", a.art.file.empty() ? "Built-in placeholder"
				                                            : a.art.file.c_str());
				if (ImGui::Button("Import strip...")) {
					const auto moduleId = m.id, nodeId = n.id;
					choose([&, moduleId, nodeId, state](auto f) {
						auto image = readImage(f);
						auto file = importImage(f, images.root);
						for (auto &module : p.modules)
							if (module.id == moduleId) {
								int index = nodeIndex(module, nodeId);
								if (index < 0)
									return;
								edit();
								auto &clip = module.nodes[index].visuals[state];
								clip.art = {file, 0, 0, image.width, image.height};
								clip.frames = 1;
								registerAsset(module, clip.art);
								images.clear();
								return;
							}
					});
				}
				ImGui::SameLine();
				if (ImGui::Button("Clear")) {
					a = {};
					changed = true;
				}
				changed |= ImGui::InputInt("Crop X", &a.art.x);
				changed |= ImGui::InputInt("Crop Y", &a.art.y);
				changed |= ImGui::InputInt("Frame W", &a.art.w);
				changed |= ImGui::InputInt("Frame H", &a.art.h);
				changed |= ImGui::SliderInt("Frames", &a.frames, 1, 128);
				changed |= ImGui::SliderInt("FPS", &a.fps, 1, 30);
				changed |= ImGui::Checkbox("Loop animation", &a.loop);
				a.art.x = std::clamp(a.art.x, 0, 16384);
				a.art.y = std::clamp(a.art.y, 0, 16384);
				a.art.w = std::clamp(a.art.w, 1, 16384);
				a.art.h = std::clamp(a.art.h, 1, 16384);
				if (!a.art.file.empty()) {
					attempt([&] {
						const auto &source = images.get(a.art.file);
						Image thumb{128, 96, std::vector<uint32_t>(128 * 96)};
						for (int y = 0; y < 96; ++y)
							for (int x = 0; x < 128; ++x)
								thumb.pixels[y * 128 + x] =
								    ((x / 8 + y / 8) % 2) ? rgba(24, 35, 45) : rgba(34, 46, 56);
						Art crop = a.art;
						const auto frame = uint64_t(ImGui::GetTime() * a.fps) % a.frames;
						crop.x += int(frame) * crop.w;
						const float scale = std::min(120.f / crop.w, 88.f / crop.h);
						const int w = std::max(1, int(crop.w * scale)),
						          h = std::max(1, int(crop.h * scale));
						blit(thumb, source, crop, (128 - w) / 2, (96 - h) / 2, w, h);
						if (!clipTexture[state]) {
							clipTexture[state] =
							    SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
								                  SDL_TEXTUREACCESS_STREAMING, 128, 96);
							if (!clipTexture[state])
								throw std::runtime_error(SDL_GetError());
							SDL_SetTextureScaleMode(clipTexture[state], SDL_ScaleModeNearest);
						}
						SDL_UpdateTexture(clipTexture[state], nullptr, thumb.pixels.data(),
						                  128 * 4);
						ImGui::Image((ImTextureID)(intptr_t)clipTexture[state], {256, 192});
						ImGui::Text("Strip preview: frame %d / %d", int(frame) + 1, a.frames);
					});
					ImGui::TextWrapped("Frame dimensions crop the strip; Width/Height above set "
					                   "its in-room size.");
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}
	if (changed)
		history.remember(before);
	if (ImGui::Button("Delete node")) {
		edit();
		removeNode(m, size_t(graphNode));
		graphNode = -1;
		pendingFrom = -1;
	}
}
void graphCanvas(Module &m) {
	const bool nativePuzzleView = graphReadOnly && m.id == "native-puzzle";
	const bool nativeRulesView = graphReadOnly && (m.id == "native-behavior" || nativePuzzleView);
	const auto *live = displayGraphState(m);
	updateGraphPulses(m, live);
	int recentWire = -1;
	if (live)
		for (auto e = live->trace.rbegin(); e != live->trace.rend(); ++e)
			if (e->kind == TraceKind::Delivery) {
				recentWire = e->wire;
				break;
			}
	if (nativePuzzleView)
		ImGui::TextWrapped("Select an actor header; use its typed Target ports to reconnect "
		                   "dependencies. Open actor state machine to edit its behavior.");
	else if (nativeRulesView)
		ImGui::TextWrapped(
		    "Native transitions: select a rule, then edit its conditions, action and Next rule in "
		    "the inspector. Follow transition opens its destination.");
	else
		ImGui::TextWrapped("Click an output, then an input to connect. Drag node headers. "
		                   "Right-click a wire to remove it. Gold links require an item.");
	if (pendingFrom >= 0) {
		ImGui::Text("Connecting %s...", signalName(pendingSignal));
		ImGui::SameLine();
		if (ImGui::Button("Cancel link") || ImGui::IsKeyPressed(ImGuiKey_Escape))
			pendingFrom = -1;
	} else
		ImGui::TextUnformatted(
		    "Events flow left to right; room placement is independent of this layout.");
	ImGui::SetNextItemWidth(110);
	ImGui::SliderFloat("Graph zoom", &graphZoom, .02f, 2.f, "%.2fx");
	mark("graph-zoom");
	ImGui::SameLine();
	if (ImGui::Button("Frame all"))
		graphFrameAll = true;
	mark("graph-frame");
	ImGui::SameLine();
	if (ImGui::Button("100%")) {
		graphZoom = 1;
		graphPan = {0, 0};
	}
	ImGui::TextDisabled("Middle-drag pans; wheel zooms at the pointer");
	ImGui::BeginChild("graph_canvas", {0, -35}, ImGuiChildFlags_Borders,
	                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	const auto origin = ImGui::GetCursorScreenPos();
	const auto size = ImGui::GetContentRegionAvail();
	if (graphFrameAll) {
		float maxX = 240, maxY = 220;
		for (auto &n : m.nodes) {
			maxX = std::max(maxX, float(n.graphX + 240));
			maxY = std::max(maxY, float(n.graphY + (nativeRulesView ? 170 : 300)));
		}
		graphZoom = std::clamp(std::min((size.x - 24) / maxX, (size.y - 24) / maxY), .02f, 2.f);
		graphPan = {12, 12};
		graphFrameAll = false;
	}
	if (ImGui::IsWindowHovered()) {
		const auto &io = ImGui::GetIO();
		if (ImGui::IsMouseDragging(2)) {
			graphPan.x += io.MouseDelta.x;
			graphPan.y += io.MouseDelta.y;
		}
		if (io.MouseWheel != 0) {
			const float next =
			    std::clamp(graphZoom * (io.MouseWheel > 0 ? 1.15f : 1 / 1.15f), .02f, 2.f);
			const float x = io.MousePos.x - origin.x, y = io.MousePos.y - origin.y;
			graphPan = {x - (x - graphPan.x) * next / graphZoom,
			            y - (y - graphPan.y) * next / graphZoom};
			graphZoom = next;
		}
	}
	ImGui::SetWindowFontScale(graphZoom);
	auto *draw = ImGui::GetWindowDrawList();
	auto nodePos = [&](int i) {
		return ImVec2(origin.x + graphPan.x + m.nodes[i].graphX * graphZoom,
		              origin.y + graphPan.y + m.nodes[i].graphY * graphZoom);
	};
	auto pin = [&](int i, bool output, int port) {
		auto at = nodePos(i);
		return ImVec2(at.x + (output ? 208.f : 0.f) * graphZoom,
		              at.y + ((nativeRulesView ? 110 : 64) + port * 21.f) * graphZoom);
	};
	auto inputPort = [&](int i, Input input) {
		auto ports =
		    nativeRulesView ? std::vector<Input>{Input::Activate} : nodeInputs(m.nodes[i].kind);
		return int(std::find(ports.begin(), ports.end(), input) - ports.begin());
	};
	auto outputPort = [&](int i, Signal signal) {
		auto ports =
		    nativeRulesView ? std::vector<Signal>{Signal::Completed} : nodeSignals(m.nodes[i].kind);
		return int(std::find(ports.begin(), ports.end(), signal) - ports.begin());
	};
	auto curve = [&](ImVec2 a, ImVec2 b, ImU32 color, int wire = -1) {
		const float bend = b.x >= a.x ? std::clamp((b.x - a.x) * 0.5f, 12.f, 100.f) : 65.f;
		ImVec2 c{a.x + bend, a.y}, d{b.x - bend, b.y};
		draw->AddBezierCubic(a, c, d, b, color, 2.f);
		auto point = [&](float t) {
			float u = 1 - t;
			return ImVec2(
			    u * u * u * a.x + 3 * u * u * t * c.x + 3 * u * t * t * d.x + t * t * t * b.x,
			    u * u * u * a.y + 3 * u * u * t * c.y + 3 * u * t * t * d.y + t * t * t * b.y);
		};
		auto arrow = [&](float t, ImU32 col, float radius) {
			const auto q = point(t), prev = point(std::max(0.f, t - .015f));
			float dx = q.x - prev.x, dy = q.y - prev.y, len = std::sqrt(dx * dx + dy * dy);
			if (len < .01f)
				return;
			dx /= len;
			dy /= len;
			draw->AddTriangleFilled(
			    q, {q.x - dx * radius - dy * radius * .55f, q.y - dy * radius + dx * radius * .55f},
			    {q.x - dx * radius + dy * radius * .55f, q.y - dy * radius - dx * radius * .55f},
			    col);
		};
		arrow(.6f, color, 8);
		for (auto [index, began] : wirePulses)
			if (index == wire && wire >= 0) {
				const double age = ImGui::GetTime() - began;
				if (age >= 0 && age < .9) {
					const float t = float(age / .9);
					draw->AddCircleFilled(point(t), 5, IM_COL32(255, 225, 110, 255));
					arrow(t, IM_COL32(255, 245, 190, 255), 10);
				}
			}
		bool hover = false;
		const auto mouse = ImGui::GetIO().MousePos;
		for (int k = 0; k <= 50; ++k) {
			float t = k / 50.f, u = 1 - t;
			float x = u * u * u * a.x + 3 * u * u * t * c.x + 3 * u * t * t * d.x + t * t * t * b.x;
			float y = u * u * u * a.y + 3 * u * u * t * c.y + 3 * u * t * t * d.y + t * t * t * b.y;
			hover |= (x - mouse.x) * (x - mouse.x) + (y - mouse.y) * (y - mouse.y) < 49;
		}
		return hover && ImGui::IsWindowHovered() && ImGui::IsMouseClicked(1);
	};
	int remove = -1;
	for (size_t i = 0; i < m.wires.size(); ++i) {
		const auto &w = m.wires[i];
		int a = nodeIndex(m, w.from), b = nodeIndex(m, w.to);
		if (a >= 0 && b >= 0 &&
		    curve(pin(a, true, outputPort(a, w.signal)), pin(b, false, inputPort(b, w.input)),
		          int(i) == recentWire ? IM_COL32(255, 215, 75, 255) : IM_COL32(79, 213, 212, 255),
		          int(i)))
			remove = int(i);
	}
	if (remove >= 0) {
		if (!graphReadOnly && !moduleLocked(m)) {
			edit();
			m.wires.erase(m.wires.begin() + remove);
		}
	}
	for (size_t i = 0; i < m.nodes.size(); ++i) {
		int item = nodeIndex(m, m.nodes[i].requiredItem);
		if (!graphReadOnly && !moduleLocked(m) && item >= 0 &&
		    curve(pin(item, true, 0), pin(int(i), false, int(nodeInputs(m.nodes[i].kind).size())),
		          IM_COL32(235, 187, 80, 255))) {
			edit();
			m.nodes[i].requiredItem.clear();
		}
	}
	ImVec2 extent{850, 500};
	for (int i = 0; i < int(m.nodes.size()); ++i) {
		auto &n = m.nodes[i];
		const auto at = nodePos(i);
		extent.x = std::max(extent.x, float(n.graphX + 240));
		extent.y = std::max(extent.y, float(n.graphY + 220));
		const auto inputs =
		    nativeRulesView ? std::vector<Input>{Input::Activate} : nodeInputs(n.kind);
		const auto outputs =
		    nativeRulesView ? std::vector<Signal>{Signal::Completed} : nodeSignals(n.kind);
		const float height =
		    (nativeRulesView ? 125 : 82) +
		    std::max(inputs.size() + (n.kind == NodeKind::Npc || n.kind == NodeKind::ItemCondition),
			         outputs.size()) *
		        21.f;
		draw->AddRectFilled(at, {at.x + 208 * graphZoom, at.y + height * graphZoom},
		                    IM_COL32(24, 39, 49, 255), 6);
		draw->AddRectFilled(at, {at.x + 208 * graphZoom, at.y + 29 * graphZoom},
		                    live && live->nodes[i].active ? IM_COL32(32, 130, 85, 255)
		                                                  : IM_COL32(39, 85, 98, 255),
		                    6);
		draw->AddRect(at, {at.x + 208 * graphZoom, at.y + height * graphZoom},
		              i == graphNode ? IM_COL32(248, 205, 102, 255) : IM_COL32(75, 117, 129, 255),
		              6, 0, 2);
		if (i < int(statePulses.size())) {
			const double age = ImGui::GetTime() - statePulses[i];
			if (age >= 0 && age < 1.2)
				draw->AddRect({at.x - 3, at.y - 3},
				              {at.x + 208 * graphZoom + 3, at.y + height * graphZoom + 3},
				              IM_COL32(255, 215, 90, int(230 * (1 - age / 1.2))), 8, 0, 3);
		}
		draw->AddText({at.x + 8 * graphZoom, at.y + 8 * graphZoom}, IM_COL32_WHITE,
		              n.name.substr(0, 25).c_str());
		std::string mode;
		if (live) {
			const auto &s = live->nodes[i];
			mode = !s.enabled                  ? " / OFF"
			       : n.kind == NodeKind::Actor ? (s.completed  ? " / DEAD"
			                                      : s.held     ? " / HELD"
			                                      : s.friendly ? " / FRIENDLY"
			                                                   : " / HOSTILE")
			       : s.active                  ? " / ACTIVE"
			       : s.completed               ? " / DONE"
			                                   : " / idle";
		} else if (n.kind == NodeKind::State && n.initiallyActive)
			mode = " / START";
		draw->AddText({at.x + 8 * graphZoom, at.y + 36 * graphZoom}, IM_COL32(155, 185, 197, 255),
		              (std::string(nativePuzzleView  ? "Native actor"
		                           : nativeRulesView ? "Native rule"
		                                             : nodeName(n.kind)) +
		               mode)
		                  .c_str());

		if (nativeRulesView) {
			const std::string details[] = {n.text, n.doneText, n.speaker};
			for (int line = 0; line < 3; ++line)
				draw->AddText({at.x + 8 * graphZoom, at.y + (54 + line * 14) * graphZoom},
				              IM_COL32(210, 222, 230, 255), details[line].substr(0, 27).c_str());
		}
		ImGui::PushID(i);
#ifdef REMINISCENCE2_EDITOR_TESTING
		testPoints["header-" + n.id] = {at.x + 80 * graphZoom, at.y + 15 * graphZoom};
#endif
		ImGui::SetCursorScreenPos(at);
		ImGui::InvisibleButton("header", {208 * graphZoom, 29 * graphZoom});
		if (ImGui::IsItemClicked()) {
			graphNode = i;
			graphDrag = false;
		}
		if (!graphReadOnly && !moduleLocked(m) && ImGui::IsItemActive() &&
		    ImGui::IsMouseDragging(0, 2)) {
			if (!graphDrag) {
				edit();
				graphDrag = true;
				graphStartX = n.graphX;
				graphStartY = n.graphY;
				const auto io = ImGui::GetIO();
				graphDragOrigin = {io.MousePos.x - io.MouseDelta.x,
				                   io.MousePos.y - io.MouseDelta.y};
			}
			n.graphX = std::clamp(
			    graphStartX + int((ImGui::GetIO().MousePos.x - graphDragOrigin.x) / graphZoom), 0,
			    8192);
			n.graphY = std::clamp(
			    graphStartY + int((ImGui::GetIO().MousePos.y - graphDragOrigin.y) / graphZoom), 0,
			    8192);
		}
		for (int side = 0; side < 2; ++side) {
			int count = side ? int(outputs.size())
			                 : int(inputs.size()) +
			                       (n.kind == NodeKind::Npc || n.kind == NodeKind::ItemCondition);
			for (int j = 0; j < count; ++j) {
				const bool requirement = !side && j == int(inputs.size());
				const auto q = pin(i, side, j);
#ifdef REMINISCENCE2_EDITOR_TESTING
				testPoints["port-" + n.id + "-" + std::to_string(side) + "-" + std::to_string(j)] =
				    q;
#endif
				const char *label = nativePuzzleView  ? (side ? "Dependency" : "Actor")
				                    : nativeRulesView ? (side ? "Next rule" : "State entry")
				                    : side            ? signalName(outputs[j])
				                    : requirement     ? "Needs item"
				                                      : inputName(inputs[j]);
				ImGui::PushID(side * 100 + j);
				ImGui::SetCursorScreenPos({q.x - 8 * graphZoom, q.y - 8 * graphZoom});
				ImGui::InvisibleButton("port", {16 * graphZoom, 16 * graphZoom});
				draw->AddCircleFilled(q, 5 * graphZoom,
				                      requirement ? IM_COL32(235, 187, 80, 255)
				                                  : IM_COL32(82, 218, 208, 255));
				draw->AddText(
				    {q.x + (side ? -ImGui::CalcTextSize(label).x - 10 * graphZoom : 10 * graphZoom),
					 q.y - 6 * graphZoom},
				    IM_COL32(220, 234, 239, 255), label);
				if (!graphReadOnly && !moduleLocked(m) && ImGui::IsItemClicked()) {
					graphNode = i;
					if (side) {
						pendingFrom = i;
						pendingSignal = outputs[j];
					} else if (pendingFrom >= 0 && pendingFrom < int(m.nodes.size())) {
						if (requirement) {
							if (m.nodes[pendingFrom].kind == NodeKind::Item) {
								edit();
								n.requiredItem = m.nodes[pendingFrom].id;
								pendingFrom = -1;
							} else
								status = "Needs item accepts a quest item's output";
						} else {
							Wire w{m.nodes[pendingFrom].id, n.id, pendingSignal, inputs[j]};
							if (std::none_of(m.wires.begin(), m.wires.end(), [&](auto &old) {
								    return old.from == w.from && old.to == w.to &&
									       old.signal == w.signal && old.input == w.input;
							    })) {
								if (m.wires.size() < 128) {
									edit();
									m.wires.push_back(w);
								} else
									status = "Maximum 128 wires";
							}
							pendingFrom = -1;
						}
					}
				}
				ImGui::PopID();
			}
		}
		ImGui::PopID();
	}
	if (pendingFrom >= 0 && pendingFrom < int(m.nodes.size()))
		curve(pin(pendingFrom, true, outputPort(pendingFrom, pendingSignal)),
		      ImGui::GetIO().MousePos, IM_COL32(249, 212, 112, 230));
	ImGui::SetCursorScreenPos(origin);
	ImGui::Dummy(size);
	ImGui::SetWindowFontScale(1);
	ImGui::EndChild();
}
void moduleGraphUI() {
	if (p.native.enabled && !nativeExtensionGraph) {
		nativeGraphUI();
		return;
	}
	if (selected < 0 || selected >= int(p.modules.size()) ||
	    p.modules[selected].kind != Kind::Graph) {
		moduleChooser();
		return;
	}
	auto &m = p.modules[selected];
	if (ImGui::Button("< Module placement")) {
		setWorkflow(Workflow::Placement);
		pendingFrom = -1;
	}
	ImGui::SameLine();
	ImGui::Text("MODULE / %s", m.name.c_str());
	ImGui::SameLine();
	if (ImGui::Button("Choose / new module")) {
		selected = -1;
		graphNode = -1;
		return;
	}
	if (ImGui::Button("Copy module")) {
		selection = {m.id};
		copySelection();
	}
	ImGui::SameLine();
	if (ImGui::Button("Duplicate module")) {
		attempt([&] {
			selection = {m.id};
			copySelection();
			pasteSelection();
			openGraph();
		});
		return;
	}
	ImGui::SameLine();
	if (ImGui::Button("Delete module")) {
		selection = {m.id};
		attempt([&] { deleteSelection(); });
		return;
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(clipboard.modules.empty());
	const bool paste = ImGui::Button("Paste module");
	ImGui::EndDisabled();
	if (paste) {
		attempt([&] {
			pasteSelection();
			openGraph();
		});
		return;
	}
	graphDebugToolbar(m);
	if (ImGui::BeginTable("module_graph", 3,
	                      ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
		ImGui::TableSetupColumn("Nodes", ImGuiTableColumnFlags_WidthFixed, 225);
		ImGui::TableSetupColumn("Graph", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Node", ImGuiTableColumnFlags_WidthFixed, 300);
		ImGui::TableNextColumn();
		ImGui::BeginChild("graph_palette", {0, -35});
		moduleMetadata(m);
		ImGui::SeparatorText("ADD NODE");
		ImGui::BeginDisabled(moduleLocked(m));
		for (int kind = 0; kind < int(NodeKind::Count); ++kind) {
			if (p.native.enabled &&
			    (kind == int(NodeKind::Door) || kind == int(NodeKind::Checkpoint)))
				continue;
			if (ImGui::Button(nodeName(NodeKind(kind)), {-1, 0}))
				attempt([&] {
					auto n = makeNode(m, NodeKind(kind), {p.rooms[room].id, 96, 216});
					if (p.native.enabled && n.kind == NodeKind::Actor)
						n.nativeActor = std::clamp(nativeActor, 1, int(p.native.actors.size()) - 1);
					edit();
					m.nodes.push_back(n);
					graphNode = int(m.nodes.size()) - 1;
				});
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s\n%s", nodeCategory(NodeKind(kind)), nodeHelp(NodeKind(kind)));
			mark(("add-node-" + std::to_string(kind)).c_str());
		}
		ImGui::EndDisabled();
		ImGui::SeparatorText("CONNECTIONS");
		ImGui::TextWrapped("%zu nodes / %zu event wires", m.nodes.size(), m.wires.size());
		ImGui::TextWrapped("Enable / Disable gates interaction. Disabled doors retain collision. "
		                   "Reset clears a mission's progress.");
		if (ImGui::Button("Save module...", {-1, 0})) {
			auto copy = m;
			choose(
			    [&, copy](auto f) {
				    savePrefab(copy, images.root, f);
				    status = "Module and animation dependencies saved";
			    },
			    true, false, "module.r2module.json");
		}
		issueList(validate(p));
		ImGui::EndChild();
		ImGui::TableNextColumn();
		graphCanvas(m);
		ImGui::TableNextColumn();
		ImGui::BeginChild("node_inspector", {0, -35});
		ImGui::BeginDisabled(moduleLocked(m));
		graphDebugInspector(m);
		nodeInspector(m);
		ImGui::EndDisabled();
		ImGui::EndChild();
		ImGui::EndTable();
	}
}
