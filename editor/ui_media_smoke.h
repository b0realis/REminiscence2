// SPDX-License-Identifier: GPL-3.0-or-later
// Exercise the real audio import picker and audition controls with a synthetic tone.
void inputMedia(int frame, Workspace &app) {
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
	if (frame == 640) {
		app.storyWindowOpen = app.nodeHelpOpen = app.moduleHelpOpen = false;
		app.scene.reset();
		app.p = makeProject();
		app.room = 0;
		app.selection.clear();
		app.paletteSearch.Clear();
		app.path = output / "media" / "project.r2project.json";
		app.images.root = app.path.parent_path();
		fs::create_directories(app.images.root);
		File file;
		file.open("tone.wav", "wb", app.images.root.u8string().c_str());
		file.write("RIFF", 4);
		file.writeUint32LE(16036);
		file.write("WAVEfmt ", 8);
		file.writeUint32LE(16);
		file.writeUint16LE(1);
		file.writeUint16LE(1);
		file.writeUint32LE(8000);
		file.writeUint32LE(16000);
		file.writeUint16LE(2);
		file.writeUint16LE(16);
		file.write("data", 4);
		file.writeUint32LE(16000);
		for (int i = 0; i < 8000; ++i)
			file.writeUint16LE((i % 80 < 40) ? 2000 : -2000);
		file.close();
		auto m = makeGraph(app.p, 5, app.p.spawn);
		m.name = "Sound and animation workshop";
		m.nodes.push_back(makeNode(m, NodeKind::Music, app.p.spawn));
		app.p.modules.push_back(m);
		app.selected = 0;
		app.part = 0;
		app.graphNode = 0;
		app.openGraph();
		app.graphZoom = .7f;
		app.graphPan = {0, 0};
	}
	click(644, "audio-import");
	if (frame == 647) {
		app.picker.directory = app.images.root.u8string();
		app.picker.file = "tone.wav";
	}
	click(648, "picker-accept");
	click(654, "audio-listen");
	click(658, "audio-stop");
	if (frame == 663) {
		auto &m = app.p.modules[0];
		auto n = makeNode(m, NodeKind::Animation, app.p.spawn);
		Image image{32, 16, std::vector<uint32_t>(32 * 16, rgba(87, 220, 202))};
		writePNG(image, app.images.root / "strip.png");
		n.visuals[0] = {{"strip.png", 0, 0, 16, 16}, 2, 8, true};
		m.nodes.push_back(n);
		app.graphNode = 1;
	}
	click(666, "clip-idle");
}
void checkMedia(int frame, Workspace &app) {
	if (frame == 651) {
		const auto &n = app.p.modules[0].nodes[0];
		require(!app.picker.open && !n.audioFile.empty() && n.duration == 30,
		        "Audio picker did not import duration/file");
		require(fs::is_regular_file(assetPath(app.images.root, n.audioFile)),
		        "Audio import dependency missing");
	}
	if (frame == 656) {
		require(app.audition.device != 0, "Audio Listen did not open preview");
		screenshot(app.renderer, (output / "audio-node.bmp").u8string());
	}
	if (frame == 661) {
		require(app.audition.device == 0, "Stop listening left the audio device open");
		app.saveAs(app.path);
		require(audioIssues(loadProject(app.path), app.images.root).empty(),
		        "Saved audio module is invalid");
	}
	if (frame == 670) {
		require(app.clipTexture[0] != nullptr, "Sprite animation preview did not render");
		screenshot(app.renderer, (output / "animation-node.bmp").u8string());
		app.quit = true;
	}
}
