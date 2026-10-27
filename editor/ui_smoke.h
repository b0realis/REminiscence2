// SPDX-License-Identifier: GPL-3.0-or-later
// Included only in test builds, after Workspace. Drive the actual ImGui widgets
// through input events, then check the document and playtest state. Coordinates
// describe the fixed 1280x850 regression window, not an external desktop.
struct UiSmoke {
	std::filesystem::path output;
	size_t originalModules = 0;
	ImVec2 pointer{0, 0};
	int graphX = 0, graphY = 0;
	ImVec2 previousPan{};
	int focusIndex = -1;
	std::string playtestDocument, playtestRoom;
	void require(bool condition, const char *message) {
		if (!condition)
			throw std::runtime_error(std::string("Editor UI regression: ") + message);
	}
	void prepare(Workspace &app) {
		require(!std::filesystem::exists(output / "project.r2project.json"),
		        "use a new output directory");
		app.saveAs(output / "project.r2project.json");
		originalModules = app.p.modules.size();
		app.setWorkflow(Workflow::Placement);
	}
	void input(int frame, Workspace &app) {
		auto &io = ImGui::GetIO();
		auto click = [&](int at, float x, float y) {
			if (frame == at) {
				pointer = {x, y};
				io.AddMousePosEvent(x, y);
				io.AddMouseButtonEvent(0, true);
			}
			if (frame == at + 1)
				io.AddMouseButtonEvent(0, false);
		};
		if (frame == 4 || frame == 5 || frame == 8 || frame == 9) {
			const auto at = app.testPoints.at("world-view");
			click(frame < 8 ? 4 : 8, at.x, at.y);
		} // World / room views.
		if (frame == 12 || frame == 13) {
			const auto where = app.testPoints.at("door-palette");
			click(12, where.x, where.y);
		}
		if (frame == 15 || frame == 16) {
			const auto at = app.testPoints.at("room-origin");
			click(15, at.x + 96 * app.zoom, at.y + 214 * app.zoom);
		} // Door palette -> room.
		click(20, 247, 24);
		if (frame == 22 || frame == 23 || frame == 28 || frame == 29) {
			const auto at = app.testPoints.at("room-origin");
			click(frame < 28 ? 22 : 28, at.x + 196 * app.zoom, at.y + 166 * app.zoom);
		} // Selection must preserve redo history.
		click(24, 291, 24); // Undo / redo placement.

		click(32, 123, 24); // Select original panel, save.
		if (!app.library.games.empty()) {
			click(36, 544, 24); // Play.
			if (frame == 40)
				io.AddKeyEvent(ImGuiKey_RightArrow, true);
			if (frame == 70)
				io.AddKeyEvent(ImGuiKey_RightArrow, false);
			if (frame >= 40 && frame <= 70)
				app.accumulator += 34;
			click(74, 110, 24); // Stop.
		}
		if (frame == 81) {
			auto m = makeGraph(app.p, 0, {app.p.rooms[0].id, 192, 216});
			m.wires.clear();
			app.p.modules.push_back(m);
			app.selected = int(app.p.modules.size()) - 1;
			app.openGraph();
			graphX = app.p.modules.back().nodes[1].graphX;
			graphY = app.p.modules.back().nodes[1].graphY;
		}
		if (frame >= 84 && frame < 180) {
			auto outputPort = app.testPoints.at("port-node-2-1-0");
			auto doorPort = app.testPoints.at("port-node-1-0-0");
			click(86, outputPort.x, outputPort.y);
			click(90, doorPort.x, doorPort.y);
			if (frame == 96) {
				pointer = app.testPoints.at("header-node-2");
				io.AddMousePosEvent(pointer.x, pointer.y);
				io.AddMouseButtonEvent(0, true);
			}
			if (frame == 98) {
				pointer.x += 30;
				pointer.y += 25;
			}
			if (frame == 99)
				io.AddMouseButtonEvent(0, false);
		}
		click(106, 247, 24);
		click(110, 291, 24);
		if (frame == 114)
			io.AddKeyEvent(ImGuiKey_F1, true);
		if (frame == 115)
			io.AddKeyEvent(ImGuiKey_F1, false);
		if (frame == 119)
			io.AddKeyEvent(ImGuiKey_Escape, true);
		if (frame == 120)
			io.AddKeyEvent(ImGuiKey_Escape, false);
		if (frame >= 126 && frame <= 131) {
			const auto fit = app.testPoints.at("fit"), one = app.testPoints.at("one-x");
			click(126, fit.x, fit.y);
			click(130, one.x, one.y);
		}
		if (frame == 134 || frame == 135) {
			const auto search = app.testPoints.at("search");
			click(134, search.x, search.y);
		}
		if (frame == 136)
			io.AddInputCharactersUTF8("door");
		if (frame == 140 || frame == 141) {
			const auto guide = app.testPoints.at("guidelines");
			click(140, guide.x, guide.y);
		}
		if (frame == 144)
			io.AddKeyEvent(ImGuiKey_Escape, true);
		if (frame == 145)
			io.AddKeyEvent(ImGuiKey_Escape, false);
		if (!app.library.games.empty() && (frame == 148 || frame == 149)) {
			const auto original = app.testPoints.at("original");
			click(148, original.x, original.y);
		}
		if (!app.library.games.empty()) {
			if (frame == 156 || frame == 157) {
				const auto point = app.testPoints.at("scenery-copy");
				click(156, point.x, point.y);
			}
			if (frame == 160 || frame == 161) {
				const auto point = app.testPoints.at("picker-accept");
				click(160, point.x, point.y);
			}
			if (frame == 166)
				io.AddKeyEvent(ImGuiKey_F1, true);
			if (frame == 167)
				io.AddKeyEvent(ImGuiKey_F1, false);
			if (frame == 172)
				io.AddKeyEvent(ImGuiKey_Escape, true);
			if (frame == 173)
				io.AddKeyEvent(ImGuiKey_Escape, false);
		}
		inputExtras(frame, app);
		inputPlaytest(frame, app);
		inputSettings(frame, app);
		inputWorkflows(frame, app);
		inputStatePlayback(frame, app);
		inputCampaign(frame, app);
		inputMedia(frame, app);
		io.AddMousePosEvent(pointer.x, pointer.y);
	}
	void check(int frame, Workspace &app) {
		auto capture = [&](const char *name) {
			screenshot(app.renderer, (output / name).u8string());
		};
		if (frame == 7) {
			require(app.world, "World view button");
			capture("world.bmp");
		}
		if (frame == 11)
			require(!app.world, "Return to room view");
		if (frame == 18) {
			require(app.p.modules.size() == originalModules + 1, "Place paired module");
			require(app.p.modules.back().a.y == 216, "Placement must retain the clicked floor");
		}
		if (frame == 23)
			require(app.p.modules.size() == originalModules, "Undo placement");
		if (frame == 27)
			require(app.p.modules.size() == originalModules + 1, "Redo placement");
		if (frame == 31) {
			require(app.selected == 0, "Select tall door by panel area");
			capture("workspace.bmp");
		}
		if (frame == 35)
			require(loadProject(app.path).modules.size() == originalModules + 1, "Save button");
		if (!app.library.games.empty()) {
			if (frame == 39)
				require(bool(app.scene), "Play button");
			if (frame == 72) {
				require(app.scene && app.scene->game()._pgeLive[0].pos_x > 32,
				        "Playtest keyboard movement");
				capture("playtest.bmp");
			}
			if (frame == 77)
				require(!app.scene && app.p.modules.size() == originalModules + 1,
				        "Stop restores editor document");
		}
		if (frame == 94) {
			auto &m = app.p.modules.back();
			require(m.wires.size() == 1 && m.wires[0].from == "node-2" &&
			            m.wires[0].input == Input::Open,
			        "Connect actual graph ports");
		}
		if (frame == 102) {
			const auto &n = app.p.modules.back().nodes[1];
			require(n.graphX == graphX + 30 && n.graphY == graphY + 25, "Drag graph node header");
			capture("module-graph.bmp");
			const int savedX = n.graphX;
			app.saveAs(app.path);
			auto loaded = loadProject(app.path);
			require(loaded.modules.back().wires.size() == 1 &&
			            loaded.modules.back().nodes[1].graphX == savedX,
			        "Save graph layout and connections");
		}
		if (frame == 109) {
			require(app.graphView && app.selected == int(app.p.modules.size()) - 1 &&
			            app.p.modules.back().nodes[1].graphX == graphX,
			        "Undo keeps the selected graph open");
		}
		if (frame == 113)
			require(app.graphView && app.p.modules.back().nodes[1].graphX == graphX + 30,
			        "Redo keeps the selected graph open");
		if (frame == 118) {
			require(app.guidelines, "F1 opens guidelines");
			capture("guidelines.bmp");
		}
		if (frame == 123) {
			require(!app.guidelines && app.graphView,
			        "Escape closes guidelines without leaving graph");
			app.setWorkflow(Workflow::Placement);
			// Drops must load projects, not treat JSON as an image; modal drops are blocked.
			const auto filename = app.path;
			app.dropFile(filename);
			app.setWorkflow(Workflow::Placement);
			require(!app.sheetOpen && app.path == filename, "Project-file drop dispatch");
			app.guidelines = true;
			app.dropFile(filename.parent_path() / "missing.png");
			require(!app.sheetOpen, "Modal blocks file drops");
			app.guidelines = false;
		}
		if (frame == 129)
			require(app.zoom > 1 && app.zoom < 3, "Fit zoom matches the actual room canvas");
		if (frame == 133)
			require(app.zoom == 1, "1x zoom button");
		if (frame == 139)
			require(app.paletteSearch.IsActive() &&
			            app.paletteSearch.PassFilter("Door + activator") &&
			            !app.paletteSearch.PassFilter("Alien crawler"),
			        "Palette search input");
		if (frame == 143)
			require(app.guidelines, "Guidelines toolbar button");
		if (frame == 153) {
			if (!app.library.games.empty()) {
				require(bool(app.reference) && app.referenceRoom == 27,
				        "Original L1 toolbar button");
				capture("original-reference.bmp");
			}
		}
		if (frame == 165) {
			if (!app.library.games.empty()) {
				require(!app.reference && app.p.rooms.size() == 64 && app.room == 27 &&
				            app.p.modules.empty(),
				        "Scenery-copy button exports and opens a new project");
				app.play(false);
			}
		}
		if (frame == 170 && !app.library.games.empty())
			require(app.scene && app.paused && app.guidelines, "F1 pauses playtest");
		if (frame == 177 && !app.library.games.empty()) {
			require(app.scene && app.paused && !app.guidelines,
			        "Closing guidelines retains paused playtest");
			app.scene.reset();
		}
		checkExtras(frame, app);
		checkPlaytest(frame, app);
		checkSettings(frame, app);
		checkWorkflows(frame, app);
		checkStatePlayback(frame, app);
		checkCampaign(frame, app);
		checkMedia(frame, app);
		if (app.quit) {
			std::puts("Editor UI regression passed");
		}
	}
#include "ui_qol_smoke.h"
#include "ui_playtest_smoke.h"
#include "ui_settings_smoke.h"
#include "ui_workflow_smoke.h"
#include "ui_state_smoke.h"
#include "ui_campaign_smoke.h"
#include "ui_media_smoke.h"
};
