// SPDX-License-Identifier: GPL-3.0-or-later
// Drive tabs, node creation, reusable templates and workspace edit isolation.
void inputWorkflows(int frame, Workspace &app) {
	auto &io = ImGui::GetIO();
	auto click = [&](int start, const std::string &id) {
		if (frame == start) {
			pointer = app.testPoints.at(id);
			io.AddMousePosEvent(pointer.x, pointer.y);
			io.AddMouseButtonEvent(0, true);
		}
		if (frame == start + 1)
			io.AddMouseButtonEvent(0, false);
	};
	if (frame == 450) {
		app.scene.reset();
		app.reference.reset();
		app.settingsOpen = app.guidelines = false;
		app.p = makeProject();
		app.p.name = "Dialogue Workshop";
		app.selected = app.nativeActor = -1;
		app.room = 0;
		app.selection.clear();
		app.world = false;
		app.zoom = 2;
		app.graphZoom = .65f;
		app.graphPan = {0, 0};
		app.locked[0] = app.locked[1] = app.locked[2] = false;
		app.path = output / "workflow" / "project.r2project.json";
		app.images.root = app.path.parent_path();
		app.images.clear();
		app.history = {};
		app.status = "Three workflows: level, placement, module";
		app.paletteSearch.Clear();
		app.newModuleName = "Airlock conversation";
		app.setWorkflow(Workflow::Level);
	}
	click(454, "workflow-help");
	click(462, "workflow-module");
	click(466, "module-create");
	click(474, "add-node-8");
	if (frame == 478) {
		auto &m = app.p.modules[0];
		m.nodes[0].speaker = "Station attendant";
		m.nodes[0].text = "The airlock is ready. Release the outer door?";
		m.nodes[0].choices = {"Yes, release it", "Wait here"};
		m.nodes[0].at = {app.p.rooms[0].id, 96, 216};
		m.nodes[0].graphX = 260;
		m.nodes[0].graphY = 24;
		m.nodes.push_back(makeNode(m, NodeKind::Button, {app.p.rooms[0].id, 64, 216}));
		m.nodes[1].graphX = 24;
		m.nodes[1].graphY = 24;
		m.nodes.push_back(makeNode(m, NodeKind::Animation, {app.p.rooms[0].id, 144, 216}));
		m.nodes[2].graphX = 500;
		m.nodes[2].graphY = 24;
		m.nodes.push_back(makeNode(m, NodeKind::Door, {app.p.rooms[0].id, 192, 216}));
		m.nodes[3].graphX = 740;
		m.nodes[3].graphY = 24;
		m.wires = {{m.nodes[1].id, m.nodes[0].id, Signal::Used, Input::Activate},
		           {m.nodes[0].id, m.nodes[2].id, Signal::Choice1, Input::Activate},
		           {m.nodes[2].id, m.nodes[3].id, Signal::Completed, Input::Open}};
		app.graphNode = 0;
		app.saveAs(app.path);
	}
	click(482, "graph-frame");
	click(488, "module-library-save");
	click(494, "workflow-placement");
	if (frame == 500 || frame == 501)
		click(500, "library-" + app.moduleLibrary.at(0).filename().u8string());
	if (frame == 504) {
		const auto origin = app.testPoints.at("room-origin");
		pointer = {origin.x + 160 * app.zoom, origin.y + 216 * app.zoom};
		io.AddMousePosEvent(pointer.x, pointer.y);
		io.AddMouseButtonEvent(0, true);
	}
	if (frame == 505)
		io.AddMouseButtonEvent(0, false);
	click(510, "workflow-level");
	if (frame == 514) {
		playtestDocument = serialize(app.p);
		io.AddKeyEvent(ImGuiKey_Delete, true); // placement selection must be protected here
		const auto origin = app.testPoints.at("room-origin");
		pointer = {origin.x + 196 * app.zoom, origin.y + 168 * app.zoom};
		io.AddMousePosEvent(pointer.x, pointer.y);
		io.AddMouseButtonEvent(0, true);
	}
	if (frame == 515)
		io.AddKeyEvent(ImGuiKey_Delete, false);
	if (frame == 516)
		pointer.x += 35;
	if (frame == 517)
		io.AddMouseButtonEvent(0, false);
}
void checkWorkflows(int frame, Workspace &app) {
	if (frame == 458) {
		require(app.workflow == Workflow::Help, "Top Help tab did not open");
		screenshot(app.renderer, (output / "workflow-help.bmp").u8string());
	}
	if (frame == 470)
		require(app.workflow == Workflow::Module && app.p.modules.size() == 1 &&
		            app.p.modules[0].name == "Airlock conversation",
		        "Named module creation GUI");
	if (frame == 477)
		require(app.p.modules[0].nodes.size() == 1 &&
		            app.p.modules[0].nodes[0].kind == NodeKind::Dialogue,
		        "Dialogue node palette");
	if (frame == 486)
		screenshot(app.renderer, (output / "dialogue-module.bmp").u8string());
	if (frame == 492)
		require(app.moduleLibrary.size() == 1 && std::filesystem::exists(app.moduleLibrary[0]),
		        "Save to local module library GUI");
	if (frame == 498) {
		require(app.workflow == Workflow::Placement && !app.graphView, "Module Placement tab");
		screenshot(app.renderer, (output / "module-placement.bmp").u8string());
	}
	if (frame == 508)
		require(app.p.modules.size() == 2 && app.p.modules[0].id != app.p.modules[1].id &&
		            app.p.modules[1].nodes[0].choices[0] == "Yes, release it" &&
		            app.p.modules[1].wires.size() == 3,
		        "Place reusable puzzle with intact dialogue/animation wiring");
	if (frame == 520) {
		require(app.workflow == Workflow::Level && serialize(app.p) == playtestDocument,
		        "Level workspace allowed deleting or dragging a placed module");
		screenshot(app.renderer, (output / "level-editing.bmp").u8string());
	}
}
