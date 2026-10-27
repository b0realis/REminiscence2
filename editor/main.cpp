// SPDX-License-Identifier: GPL-3.0-or-later
#include "content.h"
#include "campaign.h"
#include <cmath>
#include "image.h"
#include "audio.h"
#include "runtime.h"
#include "original.h"
#include "native.h"
#include "native_logic.h"
#include "settings.h"
#include "child_process.h"
#include <optional>
#include "preview_system.h"
#include "game.h"
#include "config.h"
#include "file.h"
#include "version.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include <algorithm>
#include <cstdio>
#include <functional>
#include <stdexcept>
Options g_options;
const char *g_caption = "REminiscence2 Editor";
namespace editor {
namespace fs = std::filesystem;
static bool textField(const char *label, std::string &value, size_t limit = 2047) {
	char b[2048];
	std::snprintf(b, sizeof(b), "%s", value.c_str());
	if (ImGui::InputText(label, b, std::min(sizeof(b), limit + 1))) {
		value = b;
		return true;
	}
	return false;
}
enum class Tool {
	Select,
	Solid,
	Erase,
	LedgeRight,
	LedgeLeft,
	Spawn,
	Door,
	Prop,
	Medkit,
	Crawler,
	Graph,
	Prefab
};
enum class Workflow { Level, Placement, Module, Campaign, Help };
struct Workspace {
	SDL_Renderer *renderer;
	SDL_Texture *texture = nullptr;
	SDL_Texture *clipTexture[2]{};
	std::vector<SDL_Texture *> thumbnails;
	std::string thumbnailVersion;
	Campaign campaign;
	fs::path campaignPath;
	std::string campaignSaved;
	std::vector<std::string> campaignUndo, campaignRedo;
	int campaignStage = -1, storyElement = 0, campaignLink = -1;
	float campaignZoom = 1;
	ImVec2 campaignPan{12, 12};
	bool campaignDrag = false, campaignFrame = false, storyReveal = false;
	uint32_t storyBegan = 0;
	SDL_Texture *storyTexture = nullptr;
	ChildProcess campaignPlayer;
	bool moduleHelpOpen = false, nodeHelpOpen = false, storyWindowOpen = false,
	     storyDragging = false;
	Images storyImages;
	std::vector<std::string> campaignMovieNames;
	std::string pulseContext;
	uint64_t pulseSequence = 0;
	std::vector<std::pair<int, double>> wirePulses;
	std::vector<int> pulseStates;
	std::vector<double> statePulses;
	Project p;
	History history;
	Images images;
	fs::path path;
	std::string saved, status = "Ready";
	assets::Library library;
	int assetIndex = 0;
	int room = 0, selected = -1, part = 0;
	Tool tool = Tool::Select;
	Workflow workflow = Workflow::Level;
	std::vector<fs::path> moduleLibrary;
	fs::path libraryRoot, pendingPrefab;
	std::string newModuleName = "New puzzle";
	bool placingTestStart = false;
	EditorSettings settings;
	bool settingsOpen = false;
	std::map<std::string, PlaytestStart> testStarts;
	bool world = false, collision = true, ledges = true, wires = true,
	     visible[3] = {true, true, true};
	bool graphView = false, graphDrag = false, gestureRecorded = false;
	bool locked[3] = {false, false, false}, collisionLocked = false;
	std::vector<std::string> selection;
	ModuleClipboard clipboard;
	Project groupStart;
	bool boxSelecting = false;
	ImVec2 selectionStart{0, 0};
	float graphZoom = 1;
	ImVec2 graphPan{0, 0};
	bool graphFrameAll = false;
	int graphStartX = 0, graphStartY = 0;
	ImVec2 graphDragOrigin{0, 0};
	std::optional<Issue> pendingIssue;
	std::vector<Issue> validationResults;
	bool validationOpen = false;
	PreviewSystem nativePreview;
	std::unique_ptr<OriginalLevel> nativeReference;
	std::string nativeReferenceKey;
	int nativeActor = -1, referenceLevel = 0;
	bool nativeAllActors = false;
	ChildProcess nativePlayer;
	fs::path nativeControl, nativeSessionRoot;
	std::string nativeStatus;
	uint32_t nativeStatusTime = 0;
	int graphNode = -1, pendingFrom = -1, recipe = 0;
	Signal pendingSignal = Signal::Used;
	GraphState graphDebugState;
	std::string graphDebugModule, graphDebugSignature, graphDebugMessage;
	bool graphDebugOpen = false, graphDebugRunning = false;
	uint64_t graphDebugTick = 0;
	uint32_t graphDebugLast = 0;
	int graphDebugSpeed = 6;
	const GraphState *monitoredGraph = nullptr;
	bool graphMonitorOpen = false;
	int graphMonitorModule = 0;
	bool graphReadOnly = false, nativeExtensionGraph = false;
	int nativePuzzle = 0, nativeRule = 0, nativeRulePage = 0;
	std::vector<NativeSession::Event> nativeEvents;
	std::map<std::string, GraphState> nativeGraphs;
	NativeCampaign nativeDefaults;
	NativeSession::Monitor nativeMonitor;
	bool nativeConnections = true, nativePlacementOpen = false;
	int nativePlacementPuzzle = 0, nativePlacementX = 16, nativePlacementY = 0;
	std::map<int, int> nativePlacementRooms;
	std::string nativePlacementProject;
	bool nativeMonitorGraph = false;
#ifdef REMINISCENCE2_EDITOR_TESTING
	std::map<std::string, ImVec2> testPoints;
#endif
	bool quit = false, closing = false, gesture = false, paused = false;
	float zoom = 2.3f;
	bool resetCanvasScroll = false;
	uint64_t frames = 0;
	uint32_t previousTime = 0;
	double accumulator = 0;
	bool guidelines = false;
	ImGuiTextFilter paletteSearch;
	PreviewSystem referencePreview;
	std::unique_ptr<OriginalLevel> reference;
	int referenceRoom = 0;
	bool referenceActors = true, referenceCollision = false, referenceEmpty = false,
	     referenceAllActors = false;
	struct AudioAudition {
		SDL_AudioDeviceID device = 0;
		~AudioAudition() { stop(); }
		void stop() {
			if (device)
				SDL_CloseAudioDevice(device);
			device = 0;
		}
		void play(const AudioClip &clip, int gain) {
			stop();
			SDL_AudioSpec spec{};
			spec.freq = clip.rate;
			spec.format = AUDIO_S16SYS;
			spec.channels = 2;
			spec.samples = 1024;
			device = SDL_OpenAudioDevice(nullptr, 0, &spec, nullptr, 0);
			if (!device)
				throw std::runtime_error(SDL_GetError());
			auto data = clip.samples;
			for (auto &s : data)
				s = int16_t(int(s) * gain / 100);
			if (SDL_QueueAudio(device, data.data(), Uint32(data.size() * 2))) {
				stop();
				throw std::runtime_error(SDL_GetError());
			}
			SDL_PauseAudioDevice(device, 0);
		}
	} audition;
	PreviewSystem preview;
	std::unique_ptr<Scene> scene;
	struct Picker {
		bool open = false, save = false, folder = false;
		std::string directory, file;
		std::function<void(fs::path)> accept;
	} picker;
	bool sheetOpen = false, rebuild = false;
	std::string sheetFile;
	Slice slice;
	int sheetLayer = 0;

	Workspace(SDL_Renderer *r, const fs::path &initial, const std::string &assets)
	    : renderer(r), path(fs::absolute(initial)) {
		p = fs::exists(path) ? loadProject(path) : makeProject();
		if (p.native.enabled)
			room = roomIndex(p, p.spawn.room);
		images.root = path.parent_path();
		campaignPath = images.root / "campaign.r2campaign.json";
		campaignSaved = serializeCampaign(campaign);
		saved = fs::exists(path) ? serialize(p) : "";
		if (!assets.empty())
			library = assets::discover(assets);
		readSettings();
		texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING,
		                            256, 224);
		if (!texture)
			throw std::runtime_error(SDL_GetError());
		SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);
	}
	~Workspace() {
		try {
			if (nativePlayer.running())
				NativeSession::publish(nativeControl, "stop", p);
		} catch (...) {
		}
		scene.reset();
		for (auto *t : clipTexture)
			if (t)
				SDL_DestroyTexture(t);
		for (auto *t : thumbnails)
			SDL_DestroyTexture(t);
		if (storyTexture)
			SDL_DestroyTexture(storyTexture);
		if (texture)
			SDL_DestroyTexture(texture);
	}
	bool dirty() const { return serialize(p) != saved || campaignDirty(); }
	void readSettings() {
		settings = {};
		try {
			settings = loadEditorSettings(path.parent_path());
		} catch (const std::exception &e) {
			status = std::string("Using default editor settings: ") + e.what();
		}
	}
	void edit() { history.remember(p); }
	void mark(const char *id) {
#ifdef REMINISCENCE2_EDITOR_TESTING
		const auto a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
		testPoints[id] = {(a.x + b.x) / 2, (a.y + b.y) / 2};
#else
		(void)id;
#endif
	}
	void restoreHistory(bool redo) {
		const auto roomId = p.rooms[room].id;
		const auto moduleId =
		    selected >= 0 && selected < int(p.modules.size()) ? p.modules[selected].id : "";
		const int chosenNode = graphView ? graphNode : part;
		const auto nodeId = selected >= 0 && selected < int(p.modules.size()) && chosenNode >= 0 &&
		                            chosenNode < int(p.modules[selected].nodes.size())
		                        ? p.modules[selected].nodes[chosenNode].id
		                        : "";
		if (!(redo ? history.redo(p) : history.undo(p)))
			return;
		room = roomIndex(p, roomId);
		if (room < 0)
			room = 0;
		selected = -1;
		for (int i = 0; i < int(p.modules.size()); ++i)
			if (p.modules[i].id == moduleId)
				selected = i;
		graphNode = selected >= 0 ? nodeIndex(p.modules[selected], nodeId) : -1;
		if (!nodeId.empty())
			part = std::max(0, graphNode);
		graphView = graphView && selected >= 0 && p.modules[selected].kind == Kind::Graph;
		pendingFrom = -1;
		gesture = graphDrag = gestureRecorded = false;
		reconcileSelection();
		status = redo ? "Redone" : "Undone";
	}
	void dropFile(const fs::path &file) {
		if (scene || reference || guidelines || settingsOpen || picker.open || sheetOpen ||
		    closing) {
			status = "Return to project editing before dropping a file";
			return;
		}
		if (file.extension() == ".json")
			open(file);
		else {
			readImage(file); // Reject unsupported files before opening a slicing dialog.
			setWorkflow(Workflow::Level);
			sheetFile = file.u8string();
			sheetOpen = true;
		}
	}
	template <class F> void attempt(F f) {
		try {
			f();
		} catch (const std::exception &e) {
			status = e.what();
		}
	}
	void choose(std::function<void(fs::path)> cb, bool save = false, bool folder = false,
	            const char *name = "") {
		picker = {true, save, folder, path.parent_path().u8string(), name, std::move(cb)};
	}
	void saveAs(const fs::path &destination) {
		Project copy = p;
		const auto dest = fs::absolute(destination);
		if (nativePlayer.running() && dest.parent_path() != nativeSessionRoot)
			throw std::runtime_error("Stop the native preview before saving into another folder");
		if (dest.parent_path() != path.parent_path()) {
			std::map<std::string, std::string> moved;
			auto move = [&](Art &a) {
				if (!a.file.empty()) {
					if (!moved.count(a.file))
						moved[a.file] =
						    importImage(assetPath(images.root, a.file), dest.parent_path());
					a.file = moved[a.file];
				}
			};
			for (auto &r : copy.rooms)
				for (auto &a : r.art)
					move(a);
			for (auto &m : copy.modules) {
				forEachArt(m, move);
				copyModuleAudio(m, images.root, dest.parent_path());
			}
		}
		saveProject(copy, dest);
		if (dest.parent_path() != path.parent_path())
			history = {};
		p = std::move(copy);
		path = dest;
		images.root = path.parent_path();
		images.clear();
		thumbnailVersion.clear();
		saved = serialize(p);
		status = "Project saved";
		saveEditorSettings(path.parent_path(), settings);
	}
	void open(const fs::path &file) {
		setWorkflow(Workflow::Level);
		auto next = loadProject(file);
		if (nativePlayer.running() &&
		    (!next.native.enabled || fs::absolute(file).parent_path() != nativeSessionRoot))
			throw std::runtime_error("Stop the native preview before opening another project");
		if (dirty())
			saveProject(p, path.parent_path() / "recovery.r2project.json");
		p = std::move(next);
		testStarts.clear();
		placingTestStart = false;
		path = fs::absolute(file);
		images.root = path.parent_path();
		images.clear();
		history = {};
		room = 0;
		selected = -1;
		selection.clear();
		nativeActor = -1;
		nativeReference.reset();
		nativeReferenceKey.clear();
		tool = Tool::Select;
		if (p.native.enabled)
			room = roomIndex(p, p.spawn.room);
		part = 0;
		thumbnailVersion.clear();
		saved = serialize(p);
		status = "Project opened";
		readSettings();
	}
	PlaytestStart testStart() {
		int slot = room;
		const Anchor *near = nullptr;
		if (graphView && selected >= 0 && selected < int(p.modules.size()) && graphNode >= 0 &&
		    graphNode < int(p.modules[selected].nodes.size())) {
			near = &p.modules[selected].nodes[graphNode].at;
			slot = roomIndex(p, near->room);
		}
		if (slot < 0 || slot >= int(p.rooms.size()))
			throw std::runtime_error("Choose a valid room to test");
		const auto &id = p.rooms[slot].id;
		if (testStarts.count(id))
			return testStarts.at(id);
		return roomPlaytestStart(p, id, near);
	}
	void play(bool here) {
		audition.stop();
		std::optional<PlaytestStart> start;
		if (here) {
			start = testStart();
			validatePlaytestStart(p, *start);
		}
		if (p.native.enabled) {
			ensureNativeReference();
			if (start && !nativeReference->hasMap(roomIndex(p, start->at.room)))
				throw std::runtime_error("This native slot has no map; choose a playable room");
			launchNative(start ? &*start : nullptr);
			return;
		}
		if (library.games.empty())
			throw std::runtime_error("Choose your original game assets folder before Play");
		auto snapshot = p;
		if (start) {
			snapshot.spawn = start->at;
			snapshot.spawnLeft = start->facingLeft;
		}
		preview._pi = {};
		scene =
		    std::make_unique<Scene>(snapshot, images.root, library.games.at(assetIndex), &preview);
		paused = false;
		accumulator = 0;
		previousTime = SDL_GetTicks();
		status = "Playing the current edits; TEST marker and authored campaign start are separate";
	}

	void pickerUI() {
		if (!picker.open)
			return;
		ImGui::OpenPopup("Choose file or folder");
		ImGui::SetNextWindowSize({650, 480}, ImGuiCond_Appearing);
		if (ImGui::BeginPopupModal("Choose file or folder", nullptr, ImGuiWindowFlags_NoCollapse)) {
			textField("Folder", picker.directory);
			if (ImGui::Button("Parent"))
				picker.directory = fs::u8path(picker.directory).parent_path().u8string();
			ImGui::SameLine();
			ImGui::TextUnformatted("Double-click a folder to enter it.");
			ImGui::BeginChild("files", {0, 295}, ImGuiChildFlags_Borders);
			std::error_code ec;
			std::vector<fs::directory_entry> entries;
			for (fs::directory_iterator it(fs::u8path(picker.directory), ec), end; !ec && it != end;
			     it.increment(ec))
				entries.push_back(*it);
			std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) {
				return a.path().filename() < b.path().filename();
			});
			for (const auto &e : entries) {
				const auto name = e.path().filename().u8string();
				if (name.empty() || name[0] == '.')
					continue;
				const bool dir = e.is_directory(ec);
				const auto label = (dir ? "[Folder] " : "") + name;
				if (ImGui::Selectable(label.c_str(), picker.file == name,
				                      ImGuiSelectableFlags_AllowDoubleClick)) {
					if (dir && ImGui::IsMouseDoubleClicked(0))
						picker.directory = e.path().u8string();
					else if (!dir)
						picker.file = name;
				}
			}
			if (ec)
				ImGui::TextWrapped("%s", ec.message().c_str());
			ImGui::EndChild();
			if (!picker.folder)
				textField("File", picker.file);
			if (ImGui::Button(picker.folder ? "Use folder" : picker.save ? "Save here" : "Open")) {
				auto chosen = fs::u8path(picker.directory);
				if (!picker.folder)
					chosen /= fs::u8path(picker.file);
				if (picker.folder || !picker.file.empty()) {
					auto cb = picker.accept;
					picker.open = false;
					ImGui::CloseCurrentPopup();
					attempt([&] { cb(chosen); });
				}
			}
			mark("picker-accept");
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) {
				picker.open = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}
	void sheetUI() {
		if (!sheetOpen)
			return;
		ImGui::OpenPopup("Slice level artwork");
		ImGui::SetNextWindowSize({560, 0}, ImGuiCond_Appearing);
		if (ImGui::BeginPopupModal("Slice level artwork", nullptr,
		                           ImGuiWindowFlags_AlwaysAutoResize)) {
			textField("Source image", sheetFile);
			ImGui::Combo("Layer", &sheetLayer, "Back\0Playfield\0Front\0");
			ImGui::InputInt("Block width", &slice.width);
			ImGui::InputInt("Block height", &slice.height);
			ImGui::InputInt("Horizontal stride", &slice.strideX);
			ImGui::InputInt("Vertical stride", &slice.strideY);
			if (ImGui::Button("Classic overlap (256 x 224, stride 216)"))
				slice = {};
			if (ImGui::Button("Sprite slicing: no overlap")) {
				slice.strideX = slice.width;
				slice.strideY = slice.height;
			}
			ImGui::Checkbox("Rebuild rooms and modules", &rebuild);
			ImGui::TextWrapped(
			    rebuild
			        ? "Replaces existing rooms, collision and modules. Undo restores the previous "
			          "layout."
					: "Updates only this art layer. The sheet must match the existing room grid.");
			if (ImGui::Button("Slice image"))
				attempt([&] {
					if (locked[sheetLayer])
						throw std::runtime_error("Unlock this art layer before slicing");
					if (p.native.enabled && rebuild)
						throw std::runtime_error("Native room slots cannot be rebuilt; import room "
						                         "art in the inspector");
					const auto image = readImage(fs::u8path(sheetFile));
					auto next = p;
					sliceImage(next, "art/check.png", image.width, image.height, slice,
					           Layer(sheetLayer), rebuild);
					const auto file = importImage(fs::u8path(sheetFile), images.root);
					next = p;
					sliceImage(next, file, image.width, image.height, slice, Layer(sheetLayer),
					           rebuild);
					edit();
					p = std::move(next);
					room = 0;
					selected = -1;
					images.clear();
					sheetOpen = false;
					status = "Artwork sliced into " + std::to_string(p.rooms.size()) + " rooms";
					ImGui::CloseCurrentPopup();
				});
			ImGui::SameLine();
			if (ImGui::Button("Cancel slicing")) {
				sheetOpen = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::TextWrapped("%s", status.c_str());
			ImGui::EndPopup();
		}
	}
	void toolbar() {
		if (ImGui::Button("New"))
			attempt([&] {
				if (nativePlayer.running())
					throw std::runtime_error(
					    "Stop the native preview before creating another project");
				if (dirty())
					saveProject(p, path.parent_path() / "recovery.r2project.json");
				p = makeProject();
				testStarts.clear();
				placingTestStart = false;
				selection.clear();
				nativeActor = -1;
				nativeReference.reset();
				tool = Tool::Select;
				setWorkflow(Workflow::Level);
				pendingFrom = -1;
				path = path.parent_path() / ("Untitled-" + p.id.substr(p.id.size() - 8)) /
				       "project.r2project.json";
				images.root = path.parent_path();
				images.clear();
				thumbnailVersion.clear();
				history = {};
				room = 0;
				selected = -1;
				part = 0;
				saved = "";
				status = "New project in its own folder; unsaved previous work retained in its "
				         "recovery.r2project.json";
			});
		ImGui::SameLine();
		if (ImGui::Button("Open"))
			choose([&](auto f) { open(f); });
		ImGui::SameLine();
		if (ImGui::Button("Save"))
			attempt([&] {
				if (workflow == Workflow::Campaign)
					saveCampaignDocument();
				else
					saveAs(path);
			});
		ImGui::SameLine();
		if (ImGui::Button("Save as"))
			choose([&](auto f) { saveAs(f); }, true, false, "project.r2project.json");
		ImGui::SameLine();
		if (ImGui::Button("Undo"))
			attempt([&] { restoreHistory(false); });
		ImGui::SameLine();
		if (ImGui::Button("Redo"))
			attempt([&] { restoreHistory(true); });
		ImGui::SameLine();
		if (ImGui::Button("Slice image...")) {
			setWorkflow(Workflow::Level);
			choose([&](auto f) {
				sheetFile = f.u8string();
				sheetOpen = true;
			});
		}
		ImGui::SameLine();
		if (ImGui::Button("Validate"))
			validateProject();
		ImGui::SameLine();
		const char *playLabel = settings.authoredStart
		                            ? (p.native.enabled ? "PLAY CAMPAIGN" : "PLAY FROM START")
		                            : "PLAY HERE";
		if (ImGui::Button(playLabel))
			attempt([&] { play(!settings.authoredStart); });
		mark("play-here");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(
			    "F5: use Editor Settings start mode. Right-click for an explicit start.");
		if (ImGui::BeginPopupContextItem("play_origin")) {
			if (ImGui::MenuItem("Play here (Ctrl/Cmd+F5)"))
				attempt([&] { play(true); });
			if (ImGui::MenuItem("Play from authored start (Shift+F5)"))
				attempt([&] { play(false); });
			ImGui::EndPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Guidelines (F1)"))
			guidelines = true;
		mark("guidelines");
		ImGui::SameLine();
		if (ImGui::Button("Original campaign"))
			attempt([&] { openOriginal(); });
		mark("original");
		ImGui::SameLine();
		if (ImGui::Button("Settings"))
			settingsOpen = true;
		mark("editor-settings");
		ImGui::SameLine();
		ImGui::Text("%s%s", p.name.c_str(), dirty() ? " *" : "");
		nativeToolbar();
		nativePlacementDialog();
	}
	void palette() {
		if (workflow == Workflow::Placement && p.native.enabled)
			nativePalette();
		else if (workflow == Workflow::Placement) {
			ImGui::SeparatorText("MODULE PALETTE");
			paletteSearch.Draw("Search", 135);
			mark("search");
			const std::pair<const char *, Tool> entries[] = {{"Select / move", Tool::Select},
			                                                 {"Door + activator", Tool::Door},
			                                                 {"Animated prop", Tool::Prop},
			                                                 {"Health pickup", Tool::Medkit},
			                                                 {"Alien crawler", Tool::Crawler}};
			for (const auto &e : entries) {
				if (!paletteSearch.PassFilter(e.first))
					continue;
				if (ImGui::Selectable(e.first, tool == e.second))
					tool = e.second;
				if (e.second == Tool::Door)
					mark("door-palette");
			}
			ImGui::TextWrapped(
			    "Pick a module, then click to place it. Doors include their own activator.");
			if (ImGui::Button("Import module..."))
				choose([&](auto f) {
					pendingPrefab = f;
					tool = Tool::Prefab;
					status = "Click a room to place the imported module";
				});
			libraryPalette();
			ImGui::SeparatorText("PUZZLE MODULES");
			for (int i = 0; i < 9; ++i)
				if (paletteSearch.PassFilter(recipeName(i)) &&
				    ImGui::Selectable(recipeName(i), tool == Tool::Graph && recipe == i)) {
					tool = Tool::Graph;
					recipe = i;
				}
			ImGui::TextWrapped("Each module owns a connected puzzle: parts, wires and state. "
			                   "Original campaign puzzles are not automatically converted.");
			ImGui::SeparatorText("SELECTION");
			if (ImGui::Button("Copy"))
				copySelection();
			ImGui::SameLine();
			if (ImGui::Button("Paste"))
				attempt([&] { pasteSelection(); });
			ImGui::SameLine();
			if (ImGui::Button("Delete"))
				attempt([&] { deleteSelection(); });
			ImGui::TextWrapped("Ctrl/Cmd-click adds a module. Drag empty space for box selection. "
			                   "Ctrl/Cmd+C/V copies complete modules and their internal links.");
		}
		if (workflow == Workflow::Level) {
			ImGui::SeparatorText("LEVEL GEOMETRY");
			if (ImGui::Selectable("Inspect room", tool == Tool::Select))
				tool = Tool::Select;
			if (ImGui::Selectable("Player start", tool == Tool::Spawn))
				tool = Tool::Spawn;
			ImGui::SeparatorText("COLLISION / LEDGES");
			const std::pair<const char *, Tool> brushes[] = {
			    {"Paint blocking cells", Tool::Solid},
			    {"Erase collision", Tool::Erase},
			    {"Ledge: climb right", Tool::LedgeRight},
			    {"Ledge: climb left", Tool::LedgeLeft}};
			for (const auto &e : brushes)
				if (ImGui::Selectable(e.first, tool == e.second))
					tool = e.second;
			ImGui::TextWrapped("Floor edges: Y 72, 144, 216. Right-click a ledge to remove it.");
			ImGui::Checkbox("Lock collision", &collisionLocked);
		}
		ImGui::Checkbox("Collision overlay", &collision);
		ImGui::Checkbox("Ledge overlay", &ledges);
		ImGui::Checkbox("Virtual wires", &wires);
		ImGui::SeparatorText("VISUAL LAYERS");
		for (int i = 2; i >= 0; --i) {
			ImGui::PushID(i);
			ImGui::Checkbox(layerName(Layer(i)), &visible[i]);
			ImGui::SameLine();
			ImGui::Checkbox("Lock", &locked[i]);
			ImGui::PopID();
		}
		ImGui::SeparatorText("PLAYTEST ASSETS");
		if (ImGui::Button("Choose asset folder..."))
			choose(
			    [&](auto f) {
				    library = assets::discover(f.u8string());
				    assetIndex = 0;
				    status = std::to_string(library.games.size()) + " editions found";
			    },
			    false, true);
		if (!library.games.empty() &&
		    ImGui::BeginCombo("Edition", library.games[assetIndex].label.c_str())) {
			for (int i = 0; i < int(library.games.size()); ++i)
				if (ImGui::Selectable(library.games[i].label.c_str(), assetIndex == i))
					assetIndex = i;
			ImGui::EndCombo();
		}
		ImGui::TextWrapped(
		    "Edit without game assets. Play with your Amiga, DOS/GOG or Sega CD files.");
	}
	void canvas() {
		auto &r = p.rooms[room];
		Image frame{256, 224, std::vector<uint32_t>(256 * 224, rgba(12, 20, 28))};
		attempt([&] {
			if (p.native.enabled) {
				ensureNativeReference();
				if (visible[0])
					frame = nativeReference->room(room).scenery;
			}
			for (int layer = 0; layer < 3; ++layer)
				if (visible[layer]) {
					const auto &a = r.art[layer];
					if (!a.file.empty())
						blit(frame, images.get(a.file), a, 0, 0, 256, 224);
					if (p.native.enabled && layer == 1) {
						auto actors = nativeReference->actorOverlay(p, room);
						blit(frame, actors, {"", 0, 0, 256, 224}, 0, 0, 256, 224);
						if (visible[2] && r.art[2].file.empty())
							blit(frame, nativeReference->room(room).foreground,
							     {"", 0, 0, 256, 224}, 0, 0, 256, 224);
					}
					for (const auto &m : p.modules) {
						for (const auto &n : m.nodes)
							if (int(n.layer) == layer && n.at.room == r.id)
								drawNode(frame, n, n.initiallyActive, frames, images, true);
						if (int(m.layer) == layer) {
							if (m.a.room == r.id)
								drawModule(frame, m, 0, m.initiallyOpen, frames, images);
							if (m.kind == Kind::Door && m.b.room == r.id)
								drawModule(frame, m, 1, m.initiallyOpen, frames, images);
						}
					}
				}
		});
		SDL_UpdateTexture(texture, nullptr, frame.pixels.data(), 256 * 4);
		ImGui::SetNextItemWidth(140);
		ImGui::SliderFloat("Zoom", &zoom, 0.5f, 5, "%.2fx");
		ImGui::SameLine();
		if (ImGui::Button("Fit")) {
			resetCanvasScroll = true;
			const auto available = ImGui::GetContentRegionAvail();
			zoom = std::clamp(std::min((ImGui::GetWindowContentRegionMax().x -
			                            ImGui::GetWindowContentRegionMin().x - 28) /
			                               256,
			                           (available.y - 65) / 224),
			                  0.5f, 5.f);
		}
		mark("fit");
		ImGui::SameLine();
		if (ImGui::Button("1x"))
			zoom = 1;
		mark("one-x");
		ImGui::SameLine();
		if (ImGui::Button("2x"))
			zoom = 2;
		ImGui::SameLine();
		if (ImGui::Button(placingTestStart ? "Click test position..." : "Place TEST marker"))
			placingTestStart = !placingTestStart;
		mark("test-marker");
		if (testStarts.count(r.id)) {
			ImGui::SameLine();
			if (ImGui::SmallButton("Auto"))
				testStarts.erase(r.id);
		}
		ImGui::Text("%s | 256 x 224 | %zu %s", r.name.c_str(),
		            p.native.enabled ? p.native.actors.size() : p.modules.size(),
		            p.native.enabled ? "native actors" : "modules");
		ImGui::BeginChild("room_canvas", {0, 0}, ImGuiChildFlags_Borders,
		                  ImGuiWindowFlags_HorizontalScrollbar);
		if (resetCanvasScroll) {
			ImGui::SetScrollX(0);
			ImGui::SetScrollY(0);
			resetCanvasScroll = false;
		}
		const auto origin = ImGui::GetCursorScreenPos();
#ifdef REMINISCENCE2_EDITOR_TESTING
		testPoints["room-origin"] = origin;
#endif
		ImGui::Image((ImTextureID)(intptr_t)texture, {256 * zoom, 224 * zoom});
		const bool hovered = ImGui::IsItemHovered();
		auto *draw = ImGui::GetWindowDrawList();
		auto at = [&](float x, float y) {
			return ImVec2(origin.x + x * zoom, origin.y + y * zoom);
		};
		if (collision)
			for (int y = 0; y < 7; ++y)
				for (int x = 0; x < 16; ++x) {
					const int value = r.collision[y * 16 + x];
					const float top = float(std::max(0, y * 36 - 8)),
					            bottom = float(std::min(224, y * 36 + 28));
					if (value)
						draw->AddRectFilled(at(float(x * 16), top), at(float(x * 16 + 16), bottom),
						                    IM_COL32(248, 166, 64, 65));
					draw->AddRect(at(float(x * 16), top), at(float(x * 16 + 16), bottom),
					              IM_COL32(170, 200, 210, 35));
					if (value && y > 0 && y % 2 == 0)
						draw->AddLine(at(float(x * 16), float(y * 36)),
						              at(float(x * 16 + 16), float(y * 36)),
						              IM_COL32(255, 190, 80, 255), 2);
				}
		if (ledges)
			for (const auto &l : r.ledges) {
				const float x = float((l.column + (l.facing < 0)) * 16), y = float(l.row * 36);
				draw->AddCircle(at(x, y), 5 * zoom, IM_COL32(78, 236, 211, 255), 0, 2);
				draw->AddLine(at(x, y), at(x + l.facing * 15, y - 12), IM_COL32(78, 236, 211, 255),
				              2);
			}
		for (size_t i = 0; i < p.modules.size(); ++i) {
			const auto &m = p.modules[i];
			if (m.kind == Kind::Graph) {
				if (m.nodes.empty() && m.a.room == r.id) {
					draw->AddCircle(at(float(m.a.x), float(m.a.y - 12)), 8 * zoom,
					                IM_COL32(110, 240, 210, 255));
					draw->AddText(at(float(m.a.x + 12), float(m.a.y - 16)), IM_COL32_WHITE,
					              "Empty module");
				}
				for (size_t n = 0; n < m.nodes.size(); ++n) {
					const auto &node = m.nodes[n];
					if (node.at.room != r.id)
						continue;
					draw->AddRect(at(float(node.at.x), float(node.at.y - node.height)),
					              at(float(node.at.x + node.width), float(node.at.y)),
					              (int(i) == selected && int(n) == part) ||
					                      std::find(selection.begin(), selection.end(), m.id) !=
					                          selection.end()
					                  ? IM_COL32(255, 226, 130, 255)
									  : IM_COL32(104, 203, 193, 140),
					              0, 0, 2);
					if (wires)
						draw->AddText(at(float(node.at.x), float(node.at.y - node.height - 8)),
						              IM_COL32(222, 241, 237, 255), node.name.c_str());
				}
				if (wires)
					for (const auto &w : m.wires) {
						int a = nodeIndex(m, w.from), b = nodeIndex(m, w.to);
						if (a < 0 || b < 0)
							continue;
						const auto &x = m.nodes[a], &y = m.nodes[b];
						if (x.at.room == r.id && y.at.room == r.id)
							draw->AddLine(
							    at(float(x.at.x + x.width / 2), float(x.at.y - x.height / 2)),
							    at(float(y.at.x + y.width / 2), float(y.at.y - y.height / 2)),
							    IM_COL32(95, 220, 210, 210), 1.5f);
					}
				continue;
			}
			if (wires && m.kind == Kind::Door && m.a.room == r.id && m.b.room == r.id)
				draw->AddLine(at(float(m.a.x + m.width / 2), float(m.a.y - m.height / 2)),
				              at(float(m.b.x), float(m.b.y - 16)), IM_COL32(97, 215, 230, 210),
				              1.5f);
			for (int n = 0; n < (m.kind == Kind::Door ? 2 : 1); ++n) {
				const auto &a = n ? m.b : m.a;
				if (a.room != r.id)
					continue;
				draw->AddCircleFilled(at(float(a.x), float(a.y)), 3 * zoom,
				                      IM_COL32(110, 240, 210, 255));
				if (int(i) == selected ||
				    std::find(selection.begin(), selection.end(), m.id) != selection.end())
					draw->AddRect(at(float(a.x - 6), float(a.y - (n ? 24 : m.height))),
					              at(float(a.x + (n ? 8 : m.width)), float(a.y + 3)),
					              IM_COL32(255, 226, 130, 255), 0, 0, 2);
			}
		}
		if (p.spawn.room == r.id) {
			auto s = at(float(p.spawn.x + 8), float(p.spawn.y + 2));
			draw->AddTriangleFilled({s.x - 6, s.y - 8}, {s.x + 6, s.y - 8}, s,
			                        IM_COL32(120, 255, 185, 255));
			draw->AddText({s.x + 8, s.y - 20}, IM_COL32(180, 255, 220, 255), "START");
		}
		try {
			const auto target =
			    testStarts.count(r.id) ? testStarts.at(r.id) : roomPlaytestStart(p, r.id);
			const auto q = at(float(target.at.x + 8), float(target.at.y + 2));
			draw->AddLine({q.x - 8, q.y}, {q.x + 8, q.y}, IM_COL32(255, 210, 90, 255), 2);
			draw->AddLine({q.x, q.y - 8}, {q.x, q.y + 8}, IM_COL32(255, 210, 90, 255), 2);
			draw->AddText({q.x + 9, q.y - 20}, IM_COL32(255, 210, 90, 255), "TEST");
		} catch (...) { /* Invalid drafts remain editable; Play reports the reason. */
		}
		if (p.native.enabled && visible[1] && settings.nativeMarkers)
			for (const auto &a : p.native.actors)
				if (a.room == room) {
					auto q = at(float(a.x + 8), float(a.y));
					draw->AddCircle(q, 5 * zoom,
					                a.id == nativeActor ? IM_COL32(255, 220, 90, 255)
					                                    : IM_COL32(110, 225, 220, 210),
					                0, 2);
					draw->AddText({q.x + 7, q.y - 12}, IM_COL32_WHITE,
					              std::to_string(a.id).c_str());
				}
		const auto mouse = ImGui::GetIO().MousePos;
		const int x = int((mouse.x - origin.x) / zoom), y = int((mouse.y - origin.y) / zoom);
		const bool brushLocked = (tool == Tool::Solid || tool == Tool::Erase ||
		                          tool == Tool::LedgeRight || tool == Tool::LedgeLeft) &&
		                         collisionLocked;
		const bool placeTest = hovered && placingTestStart && ImGui::IsMouseClicked(0);
		if (placeTest) {
			testStarts[r.id] = {
			    {r.id, std::clamp(x - 8, 0, 240), std::clamp((y + 36) / 72 * 72 - 2, 70, 214)},
			    p.spawnLeft};
			placingTestStart = false;
			gesture = boxSelecting = false;
			status = "TEST marker placed. F5 plays here; authored start unchanged.";
		}
		if (ImGui::IsKeyPressed(ImGuiKey_Escape))
			placingTestStart = false;
		if (hovered && x >= 0 && x < 256 && y >= 0 && y < 224 && !brushLocked && !placeTest &&
		    !placingTestStart) {
			if (ImGui::IsMouseClicked(0)) {
				selectionStart = {float(x), float(y)};
				groupStart = p;
				boxSelecting = false;
				gestureRecorded = tool != Tool::Select;
				if (gestureRecorded)
					edit();
				gesture = true;
				if (tool == Tool::Spawn && !locked[1]) {
					p.spawn = {r.id, std::clamp(x - 8, 0, 248),
					           std::clamp(((y + 36) / 72) * 72 - 2, 0, 214)};
					if (p.native.enabled) {
						auto &a = p.native.actors[0];
						a.room = room;
						a.x = p.spawn.x;
						a.y = p.spawn.y;
					}
				} else if (tool >= Tool::Door && !locked[1] && !p.native.enabled &&
				           workflow == Workflow::Placement)
					attempt([&] {
						const auto kind = tool == Tool::Door     ? Kind::Door
						                  : tool == Tool::Prop   ? Kind::Prop
						                  : tool == Tool::Medkit ? Kind::Medkit
						                                         : Kind::Crawler;
						if (tool == Tool::Prefab) {
							auto next = p;
							auto placed = loadPrefab(
							    next, {r.id, x / 16 * 16, std::clamp((y + 36) / 72 * 72, 72, 216)},
							    pendingPrefab, images.root);
							if (moduleLocked(placed))
								throw std::runtime_error("Unlock module layers before placement");
							p.serial = next.serial;
							p.modules.push_back(std::move(placed));
						} else
							p.modules.push_back(tool == Tool::Graph
							                        ? makeGraph(p, recipe, {r.id, x, y})
													: makeModule(p, kind, {r.id, x, y}));
						selected = int(p.modules.size()) - 1;
						selection = {p.modules.back().id};
						part = 0;
						tool = Tool::Select;
					});
				else if (tool == Tool::LedgeRight || tool == Tool::LedgeLeft) {
					const int row = std::clamp(((y + 36) / 72) * 2, 2, 6),
					          col = std::clamp(x / 16, 1, 14);
					r.ledges.erase(
					    std::remove_if(r.ledges.begin(), r.ledges.end(),
						               [&](auto l) { return l.row == row && l.column == col; }),
					    r.ledges.end());
					r.ledges.push_back({col, row, tool == Tool::LedgeRight ? 1 : -1});
				} else if (tool == Tool::Select && workflow == Workflow::Placement) {
					selected = -1;
					int nearest = 38;
					for (int i = 0; i < int(p.modules.size()); ++i)
						for (int n = 0; n < (p.modules[i].kind == Kind::Graph
						                         ? std::max(1, int(p.modules[i].nodes.size()))
						                     : p.modules[i].kind == Kind::Door ? 2
						                                                       : 1);
						     ++n) {
							const auto &m = p.modules[i];
							if (moduleLocked(m) || !visible[int(m.layer)])
								continue;
							const auto &a = m.kind == Kind::Graph && !m.nodes.empty()
							                    ? m.nodes[n].at
							                : n ? m.b
							                    : m.a;
							const int left = a.x - (n ? 6 : 4),
							          right = a.x + (m.kind == Kind::Graph && !m.nodes.empty()
									                     ? m.nodes[n].width
									                 : n ? 8
									                     : m.width),
							          top = a.y - (m.kind == Kind::Graph && !m.nodes.empty()
									                   ? m.nodes[n].height
									               : n ? 24
									                   : m.height);
							const int d = std::max({left - x, 0, x - right}) +
							              std::max({top - y, 0, y - a.y});
							if (a.room == r.id && d < nearest) {
								nearest = d;
								selected = i;
								part = n;
							}
						}
					const bool add = ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeySuper ||
					                 ImGui::GetIO().KeyShift;
					if (selected >= 0) {
						const auto id = p.modules[selected].id;
						const auto found = std::find(selection.begin(), selection.end(), id);
						if (add) {
							if (found == selection.end())
								selection.push_back(id);
							else {
								selection.erase(found);
								selected = -1;
							}
							gesture = false;
						} else if (found == selection.end())
							selection = {id};
					} else {
						if (!add)
							selection.clear();
						boxSelecting = !p.native.enabled;
					}
					if (p.native.enabled && !locked[1] && visible[1] && settings.nativeMarkers) {
						nativeActor = -1;
						int nearestActor = 24;
						for (const auto &a : p.native.actors)
							if (a.room == room) {
								int d = std::abs(x - (a.x + 8)) + std::abs(y - a.y);
								if (d < nearestActor) {
									nativeActor = a.id;
									nearestActor = d;
								}
							}
					}
				}
			}
			if (gesture && ImGui::IsMouseDown(0)) {
				if (tool == Tool::Solid || tool == Tool::Erase)
					r.collision[std::clamp((y + 8) / 36, 0, 6) * 16 + x / 16] =
					    tool == Tool::Solid ? 1 : 0;
				if (workflow == Workflow::Placement && tool == Tool::Select && p.native.enabled &&
				    nativeActor >= 0 && nativeActor < int(p.native.actors.size()) && !locked[1] &&
				    visible[1] && settings.nativeMarkers &&
				    p.native.actors[nativeActor].room == room && ImGui::IsMouseDragging(0, 2)) {
					if (!gestureRecorded) {
						edit();
						gestureRecorded = true;
					}
					auto &a = p.native.actors[nativeActor];
					a.x = std::clamp(x - 8, 0, 248);
					a.y = std::clamp(y, 0, 215);
					if (a.id == 0) {
						p.spawn = {p.rooms[a.room].id, a.x, a.y};
						p.spawnLeft = a.facingLeft;
					}
				}
				if (workflow == Workflow::Placement && tool == Tool::Select && selected >= 0 &&
				    !moduleLocked(p.modules[selected]) && ImGui::IsMouseDragging(0, 2)) {
					if (!gestureRecorded) {
						edit();
						gestureRecorded = true;
					}
					if (selection.size() > 1) {
						attempt([&] {
							for (const auto &m : p.modules)
								if (moduleLocked(m) && std::find(selection.begin(), selection.end(),
								                                 m.id) != selection.end())
									throw std::runtime_error(
									    "Unlock selected layers before moving the group");
							auto next = groupStart;
							translateModules(next, selection, int(x - selectionStart.x) / 16 * 16,
							                 int(y - selectionStart.y));
							p.modules = std::move(next.modules);
						});
					} else {
						auto &m = p.modules[selected];
						if (m.kind == Kind::Graph && part >= 0 && part < int(m.nodes.size())) {
							auto &n = m.nodes[part];
							n.at.x = std::clamp(x / 16 * 16, 0, 256 - n.width);
							n.at.y = n.kind == NodeKind::Door
							             ? std::clamp((y + 36) / 72 * 72, n.height, 216)
							             : std::clamp(y, 0, 216);
						} else {
							auto &a = part ? m.b : m.a;
							a.x = std::clamp((x / 16) * 16, 0, part ? 248 : 256 - m.width);
							a.y = m.kind == Kind::Prop
							          ? std::clamp(y, 0, 216)
							          : std::clamp(((y + 36) / 72) * 72,
							                       m.kind == Kind::Door && !part ? m.height : 72,
							                       216);
						}
					}
				}
			}
			if (workflow == Workflow::Level && ImGui::IsMouseClicked(1) && !collisionLocked) {
				auto hit = [&](const Ledge &l) {
					return std::abs(x - (l.column + (l.facing < 0)) * 16) < 18 &&
					       std::abs(y - l.row * 36) < 20;
				};
				if (std::any_of(r.ledges.begin(), r.ledges.end(), hit)) {
					edit();
					r.ledges.erase(std::remove_if(r.ledges.begin(), r.ledges.end(), hit),
					               r.ledges.end());
				}
			}
		}
		if (boxSelecting) {
			const float x1 = std::min(selectionStart.x, float(x)),
			            x2 = std::max(selectionStart.x, float(x));
			const float y1 = std::min(selectionStart.y, float(y)),
			            y2 = std::max(selectionStart.y, float(y));
			draw->AddRectFilled(at(x1, y1), at(x2, y2), IM_COL32(70, 200, 200, 35));
			draw->AddRect(at(x1, y1), at(x2, y2), IM_COL32(90, 235, 220, 255));
			if (ImGui::IsMouseReleased(0)) {
				for (const auto &m : p.modules)
					if (!moduleLocked(m) && visible[int(m.layer)]) {
						auto hit = [&](const Anchor &a, int w, int h) {
							return a.room == p.rooms[room].id && a.x <= x2 && a.x + w >= x1 &&
							       a.y >= y1 && a.y - h <= y2;
						};
						bool matches =
						    m.kind == Kind::Graph
						        ? std::any_of(m.nodes.begin(), m.nodes.end(),
						                      [&](auto &n) { return hit(n.at, n.width, n.height); })
						        : hit(m.a, m.width, m.height) ||
						              (m.kind == Kind::Door && hit(m.b, 8, 24));
						if (matches &&
						    std::find(selection.begin(), selection.end(), m.id) == selection.end())
							selection.push_back(m.id);
					}
				if (!selection.empty())
					for (int i = 0; i < int(p.modules.size()); ++i)
						if (p.modules[i].id == selection.front())
							selected = i;
				boxSelecting = false;
			}
		}
		if (ImGui::IsMouseReleased(0))
			gesture = false;
		ImGui::EndChild();
	}
	void inspector() {
		ImGui::SeparatorText("INSPECTOR");
		if (selected >= int(p.modules.size()))
			selected = -1;
		if (workflow == Workflow::Placement && p.native.enabled && nativeActor >= 0) {
			nativeInspector();
			return;
		}
		if (workflow == Workflow::Placement && selected >= 0) {
			ImGui::BeginDisabled(moduleLocked(p.modules[selected]));
			auto before = p;
			auto &m = p.modules[selected];
			if (ImGui::Button("Module help / comments"))
				moduleHelpOpen = true;
			if (m.kind != Kind::Door && m.kind != Kind::Graph)
				part = 0;
			bool changed = textField("Name", m.name, 160);
			ImGui::TextWrapped("%s", kindName(m.kind));
			if (m.kind == Kind::Door) {
				ImGui::RadioButton("Door", &part, 0);
				ImGui::SameLine();
				ImGui::RadioButton("Activator", &part, 1);
			}
			if (m.kind == Kind::Graph) {
				if (ImGui::Button("EDIT MODULE GRAPH", {-1, 30}))
					openGraph();
				ImGui::TextWrapped("%zu nodes / %zu connections. Select and drag parts in the "
				                   "room, or use their graph inspector.",
				                   m.nodes.size(), m.wires.size());
				if (part >= 0 && part < int(m.nodes.size()))
					ImGui::TextWrapped("Part: %s", m.nodes[part].name.c_str());
			} else {
				auto &a = part ? m.b : m.a;
				if (ImGui::BeginCombo("Part room", a.room.c_str())) {
					for (const auto &r : p.rooms)
						if (ImGui::Selectable((r.name + "##" + r.id).c_str(), a.room == r.id)) {
							a.room = r.id;
							changed = true;
						}
					ImGui::EndCombo();
				}
				changed |= ImGui::SliderInt("X", &a.x, 0, part ? 248 : 256 - m.width);
				changed |= ImGui::SliderInt("Feet Y", &a.y, 0, 216);
				if (m.kind == Kind::Door) {
					a.x = (a.x / 16) * 16;
					a.y = std::clamp(((a.y + 36) / 72) * 72, part ? 72 : m.height, 216);
					changed |= ImGui::Checkbox("Initially open", &m.initiallyOpen);
					changed |= ImGui::Checkbox("Toggle", &m.toggle);
					ImGui::TextWrapped(
					    "Enter near the activator opens this door. Both parts are owned "
					    "by this module.");
					ImGui::SeparatorText("CHAIN OUTPUTS");
					for (const auto &other : p.modules)
						if ((other.kind == Kind::Door ||
						     (other.kind == Kind::Graph &&
						      std::any_of(
						          other.nodes.begin(), other.nodes.end(),
						          [](const auto &n) { return n.kind == NodeKind::Door; }))) &&
						    other.id != m.id) {
							bool link = std::find(m.outputs.begin(), m.outputs.end(), other.id) !=
							            m.outputs.end();
							if (ImGui::Checkbox((other.name + "##" + other.id).c_str(), &link)) {
								changed = true;
								if (link)
									m.outputs.push_back(other.id);
								else
									m.outputs.erase(
									    std::remove(m.outputs.begin(), m.outputs.end(), other.id),
									    m.outputs.end());
							}
						}
				}
				if (m.kind == Kind::Crawler) {
					changed |= ImGui::Checkbox("Face left", &m.facingLeft);
					ImGui::TextWrapped(
					    "Original crawler behavior: shoot while crouching when it exposes itself.");
				}
				if (m.kind == Kind::Medkit)
					changed |= ImGui::SliderInt("Health restored", &m.health, 1, 5);
				if (m.kind == Kind::Prop) {
					int layer = int(m.layer);
					if (ImGui::Combo("Layer", &layer, "Back\0Playfield\0Front\0")) {
						changed = true;
						m.layer = Layer(layer);
					}
					changed |= ImGui::SliderInt("Width", &m.width, 1, 128);
					changed |= ImGui::SliderInt("Height", &m.height, 1, 128);
					if (ImGui::Button("Import sprite sheet...")) {
						const int index = selected;
						choose([&, index](auto f) {
							auto img = readImage(f);
							auto file = importImage(f, images.root);
							edit();
							auto &target = p.modules.at(index);
							target.sprite = {file, 0, 0, img.width, img.height};
							target.frames = 1;
							images.clear();
						});
					}
					changed |= ImGui::InputInt("Frame width", &m.sprite.w);
					changed |= ImGui::InputInt("Frame height", &m.sprite.h);
					changed |= ImGui::SliderInt("Frames across", &m.frames, 1, 32);
					changed |= ImGui::SliderInt("Frames/sec", &m.fps, 1, 30);
					m.sprite.w = std::clamp(m.sprite.w, 1, 16384);
					m.sprite.h = std::clamp(m.sprite.h, 1, 16384);
					m.a.x = std::clamp(m.a.x, 0, 256 - m.width);
				}
			}
			if (changed)
				history.remember(before);
			if (m.kind == Kind::Door && ImGui::Button("Convert to node graph"))
				convertDoor();
			if (ImGui::Button("Duplicate module"))
				attempt([&] {
					if (p.modules.size() >= MaxModules)
						throw std::runtime_error("Maximum 64 modules");
					edit();
					auto copy = duplicateModule(p, p.modules[selected]);
					p.modules.push_back(copy);
					selected = int(p.modules.size()) - 1;
				});
			if (ImGui::Button("Save module...")) {
				auto copy = p.modules[selected];
				choose(
				    [&, copy](auto f) {
					    savePrefab(copy, images.root, f);
					    status = "Module saved with owned parts";
				    },
				    true, false, "module.r2module.json");
			}
			if (ImGui::Button("Delete module")) {
				edit();
				auto id = p.modules[selected].id;
				p.modules.erase(p.modules.begin() + selected);
				for (auto &other : p.modules)
					other.outputs.erase(std::remove(other.outputs.begin(), other.outputs.end(), id),
					                    other.outputs.end());
				selected = -1;
				selection.clear();
			}
			ImGui::EndDisabled();
		} else if (workflow == Workflow::Level) {
			auto before = p;
			auto &r = p.rooms[room];
			bool changed = textField("Room name", r.name, 160);
			ImGui::SeparatorText("SCREEN TRANSITIONS");
			const char *names[] = {"Up", "Down", "Right", "Left"};
			for (int d = 0; d < 4; ++d) {
				int dest = roomIndex(p, r.exits[d]);
				if (ImGui::BeginCombo(names[d],
				                      dest < 0 ? "No exit" : p.rooms[dest].name.c_str())) {
					if (ImGui::Selectable("No exit", dest < 0)) {
						connectRooms(p, room, d, -1, true);
						changed = true;
					}
					for (int i = 0; i < int(p.rooms.size()); ++i)
						if (i != room &&
						    ImGui::Selectable((p.rooms[i].name + "##" + p.rooms[i].id).c_str(),
						                      i == dest)) {
							connectRooms(p, room, d, i, true);
							changed = true;
						}
					ImGui::EndCombo();
				}
			}
			if (changed)
				history.remember(before);
			if (p.spawn.room == r.id) {
				auto old = p;
				ImGui::BeginDisabled(locked[1]);
				if (ImGui::Checkbox("Player starts facing left", &p.spawnLeft)) {
					if (p.native.enabled)
						p.native.actors[0].facingLeft = p.spawnLeft;
					history.remember(old);
				}
				ImGui::EndDisabled();
			}
			ImGui::SeparatorText("ROOM ARTWORK");
			for (int layer = 0; layer < 3; ++layer) {
				ImGui::PushID(layer);
				ImGui::BeginDisabled(locked[layer]);
				ImGui::TextUnformatted(layerName(Layer(layer)));
				ImGui::TextWrapped("%s", r.art[layer].file.empty() ? "No image"
				                                                   : r.art[layer].file.c_str());
				if (ImGui::Button("Import image")) {
					const int index = room;
					choose([&, index, layer](auto f) {
						auto img = readImage(f);
						auto file = importImage(f, images.root);
						edit();
						p.rooms.at(index).art[layer] = {file, 0, 0, img.width, img.height};
						images.clear();
					});
				}
				ImGui::SameLine();
				if (ImGui::Button("Clear")) {
					edit();
					r.art[layer] = Art{};
				}
				ImGui::EndDisabled();
				ImGui::PopID();
			}
			if (ImGui::Button("Play this room"))
				attempt([&] { play(true); });
		}
		if (workflow == Workflow::Placement && selected < 0)
			ImGui::TextWrapped(
			    "Select a placed module to edit its instance, or pick a puzzle from the palette "
			    "and click the room. Room artwork and geometry live in Level Editing.");
		ImGui::SeparatorText("VALIDATION");
		auto issues = validate(p);
		if (issues.empty())
			ImGui::TextUnformatted("No structural issues");
		issueList(issues);
	}
	void worldUI() {
		ImGui::TextWrapped("Click a room to edit. Green lines: screen transitions. Cyan lines: "
		                   "door/activator pairs spanning rooms.");
		ImGui::BeginDisabled(workflow != Workflow::Level || p.native.enabled ||
		                     p.rooms.size() == MaxRooms);
		if (ImGui::Button("Add room"))
			attempt([&] {
				edit();
				int column = 0;
				while (std::any_of(p.rooms.begin(), p.rooms.end(),
				                   [&](auto &r) { return r.column == column && r.row == 0; }))
					++column;
				addRoom(p, column, 0);
				room = int(p.rooms.size()) - 1;
				selected = -1;
				world = false;
			});
		ImGui::EndDisabled();
		const auto revision = serialize(p);
		if (thumbnailVersion != revision)
			attempt([&] {
				for (auto *t : thumbnails)
					SDL_DestroyTexture(t);
				thumbnails.clear();
				if (p.native.enabled)
					ensureNativeReference();
				for (int i = 0; i < int(p.rooms.size()); ++i) {
					Image image{256, 224, std::vector<uint32_t>(256 * 224, rgba(12, 20, 28))};
					if (p.native.enabled)
						image = nativeReference->room(i).scenery;
					for (int layer = 0; layer < 3; ++layer) {
						const auto &art = p.rooms[i].art[layer];
						if (!art.file.empty())
							blit(image, images.get(art.file), art, 0, 0, 256, 224);
						for (const auto &m : p.modules) {
							if (int(m.layer) == layer) {
								if (m.a.room == p.rooms[i].id)
									drawModule(image, m, 0, m.initiallyOpen, 0, images);
								if (m.kind == Kind::Door && m.b.room == p.rooms[i].id)
									drawModule(image, m, 1, m.initiallyOpen, 0, images);
							}
							for (const auto &n : m.nodes)
								if (int(n.layer) == layer && n.at.room == p.rooms[i].id)
									drawNode(image, n, n.initiallyActive, 0, images, true);
						}
					}
					auto *t = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
					                            SDL_TEXTUREACCESS_STATIC, 256, 224);
					if (!t)
						throw std::runtime_error(SDL_GetError());
					thumbnails.push_back(t);
					SDL_UpdateTexture(t, nullptr, image.pixels.data(), 256 * 4);
					SDL_SetTextureScaleMode(t, SDL_ScaleModeNearest);
				}
				thumbnailVersion = revision;
			});
		int minX = 128, minY = 128, maxX = -128, maxY = -128;
		for (const auto &r : p.rooms) {
			minX = std::min(minX, r.column);
			maxX = std::max(maxX, r.column);
			minY = std::min(minY, r.row);
			maxY = std::max(maxY, r.row);
		}
		ImGui::BeginChild("world_map", {0, 0}, ImGuiChildFlags_Borders,
		                  ImGuiWindowFlags_HorizontalScrollbar);
		const auto origin = ImGui::GetCursorScreenPos();
		auto *draw = ImGui::GetWindowDrawList();
		auto pos = [&](int i) {
			return ImVec2(origin.x + (p.rooms[i].column - minX) * 172.f + 12,
			              origin.y + (p.rooms[i].row - minY) * 154.f + 12);
		};
		auto center = [&](int i) {
			auto q = pos(i);
			return ImVec2(q.x + 64, q.y + 56);
		};
		for (int i = 0; i < int(p.rooms.size()); ++i)
			for (const auto &link : p.rooms[i].exits) {
				int j = roomIndex(p, link);
				if (j >= 0)
					draw->AddLine(center(i), center(j), IM_COL32(76, 192, 138, 220), 2);
			}
		for (int i = 0; i < int(p.rooms.size()); ++i) {
			auto q = pos(i);
			ImGui::SetCursorScreenPos(q);
			ImGui::PushID(i);
			ImGui::InvisibleButton("room", {128, 132});
			if (ImGui::IsItemClicked()) {
				room = i;
				selected = -1;
				selection.clear();
				nativeActor = -1;
				world = false;
			}
			if (i < int(thumbnails.size()))
				draw->AddImage((ImTextureID)(intptr_t)thumbnails[i], q, {q.x + 128, q.y + 112});
			draw->AddRect(q, {q.x + 128, q.y + 112},
			              i == room ? IM_COL32(244, 208, 115, 255) : IM_COL32(90, 145, 155, 255), 0,
			              0, 2);
			draw->AddText({q.x, q.y + 116}, IM_COL32(220, 232, 237, 255), p.rooms[i].name.c_str());
			ImGui::PopID();
		}
		if (wires)
			for (const auto &m : p.modules) {
				auto connect = [&](const std::string &from, const std::string &to, ImU32 color) {
					int a = roomIndex(p, from), b = roomIndex(p, to);
					if (a >= 0 && b >= 0 && a != b)
						draw->AddLine(center(a), center(b), color, 2);
				};
				if (m.kind == Kind::Door)
					connect(m.a.room, m.b.room, IM_COL32(95, 220, 241, 220));
				for (const auto &w : m.wires) {
					int a = nodeIndex(m, w.from), b = nodeIndex(m, w.to);
					if (a >= 0 && b >= 0)
						connect(m.nodes[a].at.room, m.nodes[b].at.room,
						        IM_COL32(95, 220, 241, 220));
				}
				for (const auto &n : m.nodes) {
					int item = nodeIndex(m, n.requiredItem);
					if (item >= 0)
						connect(m.nodes[item].at.room, n.at.room, IM_COL32(235, 187, 80, 220));
				}
			}
		ImGui::SetCursorScreenPos(origin);
		ImGui::Dummy({float((maxX - minX + 1) * 172), float((maxY - minY + 1) * 154)});
		ImGui::EndChild();
	}
	void playUI() {
		if (ImGui::Button("STOP / return to editor") ||
		    (!guidelines && ImGui::IsKeyPressed(ImGuiKey_Escape))) {
			scene.reset();
			preview._pi = {};
			status = "Playtest stopped; project unchanged";
			return;
		}
		ImGui::SameLine();
		if (ImGui::Button(paused ? "Resume" : "Pause"))
			paused = !paused;
		ImGui::SameLine();
		if (ImGui::Button("Restart"))
			attempt([&] {
				try {
					scene->restart();
				} catch (...) {
					scene.reset();
					throw;
				}
			});
		if (!scene)
			return;
		ImGui::SameLine();
		bool step = ImGui::Button("One tick");
		ImGui::SameLine();
		if (ImGui::Button("Inspect running graph")) {
			graphMonitorOpen = true;
			paused = true;
		}
		if (graphMonitorOpen) {
			ImGui::SetNextWindowSize({1000, 700}, ImGuiCond_FirstUseEver);
			if (ImGui::Begin("Running puzzle / graph monitor", &graphMonitorOpen)) {
				ImGui::Checkbox("Pause game", &paused);
				ImGui::SameLine();
				if (ImGui::Button("Step gameplay tick"))
					step = true;
				const auto &modules = scene->project().modules;
				if (!modules.empty()) {
					graphMonitorModule = std::clamp(graphMonitorModule, 0, int(modules.size()) - 1);
					if (ImGui::BeginCombo("Module", modules[graphMonitorModule].name.c_str())) {
						for (int i = 0; i < int(modules.size()); ++i)
							if (ImGui::Selectable(modules[i].name.c_str(), i == graphMonitorModule))
								graphMonitorModule = i;
						ImGui::EndCombo();
					}
					auto copy = modules[graphMonitorModule];
					monitoredGraph = &scene->states()[graphMonitorModule].graph;
					graphReadOnly = true;
					graphCanvas(copy);
					graphReadOnly = false;
					monitoredGraph = nullptr;
				}
			}
			ImGui::End();
		}
		const auto &io = ImGui::GetIO();
		preview._pi = {};
		if (!io.WantTextInput) {
			if (ImGui::IsKeyDown(ImGuiKey_LeftArrow))
				preview._pi.dirMask |= PlayerInput::DIR_LEFT;
			if (ImGui::IsKeyDown(ImGuiKey_RightArrow))
				preview._pi.dirMask |= PlayerInput::DIR_RIGHT;
			if (ImGui::IsKeyDown(ImGuiKey_UpArrow))
				preview._pi.dirMask |= PlayerInput::DIR_UP;
			if (ImGui::IsKeyDown(ImGuiKey_DownArrow))
				preview._pi.dirMask |= PlayerInput::DIR_DOWN;
			preview._pi.shift = io.KeyShift;
			preview._pi.space = ImGui::IsKeyDown(ImGuiKey_Space);
			preview._pi.enter = ImGui::IsKeyDown(ImGuiKey_Enter);
		}
		auto now = SDL_GetTicks();
		accumulator += std::min(100u, now - previousTime);
		previousTime = now;
		if (paused)
			accumulator = 0;
		if (step)
			scene->tick();
		while (!paused && accumulator >= 1000.0 / 30) {
			scene->tick();
			accumulator -= 1000.0 / 30;
		}
		const auto frame = scene->render();
		SDL_UpdateTexture(texture, nullptr, frame.pixels.data(), 256 * 4);
		float scale = std::max(1.f, std::min(ImGui::GetContentRegionAvail().x / 256,
		                                     (ImGui::GetContentRegionAvail().y - 75) / 224));
		ImGui::Image((ImTextureID)(intptr_t)texture, {256 * scale, 224 * scale});
		ImGui::TextWrapped("Room %d | Health %d | Arrows move / climb | Shift action / fire | "
		                   "Space pistol | Enter activator",
		                   scene->currentRoom() + 1, scene->game()._pgeLive[0].life);
		if (scene->dead())
			ImGui::TextUnformatted("Conrad died or left the room graph. Restart to try again.");
	}
#include "help_ui.h"
#include "campaign_ui.h"
#include "workflow_ui.h"
#include "workspace_qol.h"
#include "native_module_ui.h"
#include "native_ui.h"
#include "native_graph_ui.h"
#include "graph_debug_ui.h"
#include "graph_ui.h"
#include "guidelines_ui.h"
#include "original_ui.h"
#include "settings_ui.h"
	void frame() {
		++frames;
		const auto &io = ImGui::GetIO();
		ImGui::SetNextWindowPos({0, 0});
		ImGui::SetNextWindowSize(io.DisplaySize);
		ImGui::Begin("REminiscence2 Editor", nullptr,
		             ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
		                 ImGuiWindowFlags_NoSavedSettings);
		if (ImGui::IsKeyPressed(ImGuiKey_F1) && !picker.open && !sheetOpen && !closing &&
		    !settingsOpen) {
			guidelines = true;
			if (scene)
				paused = true;
		}
		if (!scene && !reference && !guidelines && !settingsOpen && !closing && !io.WantTextInput &&
		    !picker.open && !sheetOpen) {
			if (ImGui::IsKeyPressed(ImGuiKey_F5))
				attempt([&] {
					if (workflow == Workflow::Campaign)
						launchCampaign();
					else
						play(!io.KeyShift &&
						     (io.KeyCtrl || io.KeySuper || !settings.authoredStart));
				});
			const bool mod = io.KeyCtrl || io.KeySuper;
			if (mod && ImGui::IsKeyPressed(ImGuiKey_S))
				attempt([&] {
					if (workflow == Workflow::Campaign)
						saveCampaignDocument();
					else
						saveAs(path);
				});
			if (workflow == Workflow::Placement && !p.native.enabled) {
				if (mod && ImGui::IsKeyPressed(ImGuiKey_C))
					copySelection();
				if (mod && ImGui::IsKeyPressed(ImGuiKey_V))
					attempt([&] { pasteSelection(); });
				if (ImGui::IsKeyPressed(ImGuiKey_Delete))
					attempt([&] { deleteSelection(); });
			}
			if (mod && ImGui::IsKeyPressed(ImGuiKey_Z))
				attempt([&] {
					if (workflow == Workflow::Campaign)
						campaignHistory(io.KeyShift);
					else
						restoreHistory(io.KeyShift);
				});
		}
		if (scene)
			playUI();
		else if (reference)
			originalUI();
		else {
			toolbar();
			workflowTabs();
			ImGui::Separator();
			if (workflow == Workflow::Help)
				helpWorkspace();
			else if (workflow == Workflow::Campaign)
				campaignUI();
			else if (workflow == Workflow::Module)
				moduleGraphUI();
			else if (ImGui::BeginTable("workspace", 3,
			                           ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
				ImGui::TableSetupColumn("Palette", ImGuiTableColumnFlags_WidthFixed, 205);
				ImGui::TableSetupColumn("Canvas", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthFixed, 260);
				ImGui::TableNextColumn();
				ImGui::BeginChild("palette", {0, -35});
				palette();
				ImGui::EndChild();
				ImGui::TableNextColumn();
				if (ImGui::Button(world ? "Room view" : "World view"))
					world = !world;
				mark("world-view");
				ImGui::SameLine();
				if (ImGui::BeginCombo("Room", p.rooms[room].name.c_str())) {
					for (int i = 0; i < int(p.rooms.size()); ++i)
						if (ImGui::Selectable((p.rooms[i].name + "##" + p.rooms[i].id).c_str(),
						                      room == i)) {
							room = i;
							selected = -1;
							selection.clear();
							nativeActor = -1;
						}
					ImGui::EndCombo();
				}
				ImGui::BeginChild("canvas_container", {0, -35});
				if (world)
					worldUI();
				else
					canvas();
				ImGui::EndChild();
				ImGui::TableNextColumn();
				ImGui::BeginChild("inspector", {0, -35});
				inspector();
				ImGui::EndChild();
				ImGui::EndTable();
			}
		}
		ImGui::TextWrapped("%s", status.c_str());
		if (closing)
			ImGui::OpenPopup("Unsaved project");
		if (ImGui::BeginPopupModal("Unsaved project", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::TextUnformatted("Save your project before closing?");
			if (ImGui::Button("Save and close"))
				attempt([&] {
					saveAs(path);
					if (campaignDirty())
						saveCampaignDocument();
					quit = true;
				});
			ImGui::SameLine();
			if (ImGui::Button("Discard and close"))
				quit = true;
			ImGui::SameLine();
			if (ImGui::Button("Keep editing")) {
				closing = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
		helpWindows();
		storyWindow();
		sheetUI();
		pickerUI();
		guidelinesUI();
		settingsUI();
		validationUI();
		if (pendingIssue) {
			focusIssue(*pendingIssue);
			pendingIssue.reset();
		}
		ImGui::End();
	}
};
static void screenshot(SDL_Renderer *renderer, const std::string &path) {
	int w, h;
	SDL_GetRendererOutputSize(renderer, &w, &h);
	auto *surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 24, SDL_PIXELFORMAT_RGB24);
	if (!surface)
		throw std::runtime_error(SDL_GetError());
	int result = SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGB24, surface->pixels,
	                                  surface->pitch);
	if (!result)
		result = SDL_SaveBMP(surface, path.c_str());
	SDL_FreeSurface(surface);
	if (result)
		throw std::runtime_error("Could not capture editor workspace");
}
#ifdef REMINISCENCE2_EDITOR_TESTING
#include "ui_smoke.h"
#endif
} // namespace editor
int main(int argc, char **argv) {
	using namespace editor;
	std::string project = ".local/editor-projects/Untitled/project.r2project.json", assets, shot,
	            smoke, module, exportCampaign, campaignFile;
	bool validateOnly = false, showGuidelines = false, showOriginal = false;
	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		if (arg == "--help" || arg == "-h") {
			std::puts(
			    "REminiscence2 Editor\nUsage: reminiscence2-editor [project.r2project.json] "
			    "[--assets=FOLDER]\nImport PNG/JPEG/BMP sheets, slice rooms, paint "
			    "collision/ledges, place linked\nmodules, save projects and play with your "
			    "original game assets.\n--screenshot=FILE.bmp captures the actual workspace "
			    "and exits.\n--campaign=FILE opens a campaign document (also works with "
			    "--validate).\n--module=ID opens a graph module.\n--validate checks project "
			    "and all image/audio files without opening a window.\n--guidelines opens design "
			    "help.\n--original-level opens the campaign reference; select Edit full "
			    "native campaign for live editing.\n--export-native-campaign=NEW_FOLDER "
			    "creates all seven modular levels from\n"
			    "the selected assets and exits without a window. Existing folders are never "
			    "overwritten.\n");
			return 0;
		}
		if (arg == "--guidelines")
			showGuidelines = true;
		else if (arg == "--original-level")
			showOriginal = true;
		else if (arg.rfind("--export-native-campaign=", 0) == 0)
			exportCampaign = arg.substr(std::string("--export-native-campaign=").size());
		else if (arg == "--validate")
			validateOnly = true;
		else if (arg.rfind("--campaign=", 0) == 0)
			campaignFile = arg.substr(11);
		else if (arg.rfind("--module=", 0) == 0)
			module = arg.substr(9);
		else if (arg.rfind("--assets=", 0) == 0)
			assets = arg.substr(9);
		else if (arg.rfind("--screenshot=", 0) == 0)
			shot = arg.substr(13);
#ifdef REMINISCENCE2_EDITOR_TESTING
		else if (arg.rfind("--smoke-test=", 0) == 0)
			smoke = arg.substr(13);
#endif
		else if (arg.rfind("--", 0) == 0) {
			std::fprintf(stderr, "Unknown option: %s\n", arg.c_str());
			return 2;
		} else
			project = arg;
	}
	if (!exportCampaign.empty()) {
		try {
			setDefaultOptions(g_options);
			const auto library = assets::discover(assets);
			if (library.games.empty())
				throw std::runtime_error("Choose extracted original assets with --assets=FOLDER");
			exportNativeCampaign(library.games.front(), fs::u8path(exportCampaign));
			std::printf("Exported seven modular levels from %s to %s\n",
			            library.games.front().label.c_str(), exportCampaign.c_str());
			return 0;
		} catch (const std::exception &e) {
			std::fprintf(stderr, "Campaign import: %s\n", e.what());
			return 1;
		}
	}
	if (validateOnly) {
		try {
			if (!campaignFile.empty()) {
				const auto file = fs::absolute(fs::u8path(campaignFile));
				const auto errors = validateCampaign(loadCampaign(file), file.parent_path(), true);
				for (const auto &e : errors)
					std::fprintf(stderr, "%s\n", e.c_str());
				return errors.empty() ? 0 : 1;
			}
			auto p = loadProject(fs::u8path(project));
			Images images;
			images.root = fs::absolute(fs::u8path(project)).parent_path();
			validateImages(p, images);
			std::puts("Project, image and audio dependencies validated");
			return 0;
		} catch (const std::exception &e) {
			std::fprintf(stderr, "Editor validation: %s\n", e.what());
			return 1;
		}
	}
	setDefaultOptions(g_options);
	g_options.enable_password_menu = false;
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) {
		std::fprintf(stderr, "SDL: %s\n", SDL_GetError());
		return 1;
	}
	auto *window = SDL_CreateWindow("REminiscence2 Editor " REMINISCENCE2_VERSION,
	                                SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 850,
	                                SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	auto *renderer =
	    window
	        ? SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC)
	        : nullptr;
	if (window && !renderer)
		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	if (!renderer) {
		std::fprintf(stderr, "Editor window: %s\n", SDL_GetError());
		if (window)
			SDL_DestroyWindow(window);
		SDL_Quit();
		return 1;
	}
	SDL_SetWindowMinimumSize(window, 960, 640);
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui::GetIO().IniFilename = nullptr;
	auto &style = ImGui::GetStyle();
	style.FrameRounding = 3;
	style.GrabRounding = 3;
	style.WindowPadding = {12, 12};
	style.FramePadding = {7, 5};
	style.Colors[ImGuiCol_Button] = {0.12f, 0.29f, 0.34f, 1};
	style.Colors[ImGuiCol_Header] = {0.14f, 0.37f, 0.41f, 1};
	ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
	ImGui_ImplSDLRenderer2_Init(renderer);
	int result = 0;
	try {
		Workspace app(renderer, fs::u8path(project), assets);
		if (!campaignFile.empty()) {
			app.openCampaignDocument(fs::u8path(campaignFile));
			if (app.saved.empty())
				app.saved = serialize(app.p);
		}
		app.guidelines = showGuidelines;
		if (showOriginal)
			app.openOriginal();
		if (!module.empty()) {
			for (int i = 0; i < int(app.p.modules.size()); ++i)
				if (app.p.modules[i].id == module && app.p.modules[i].kind == Kind::Graph)
					app.selected = i;
			if (app.selected < 0)
				throw std::runtime_error("Requested graph module was not found");
			app.openGraph();
		}
		int frame = 0;
#ifdef REMINISCENCE2_EDITOR_TESTING
		UiSmoke test;
		test.output = smoke;
		if (!smoke.empty())
			test.prepare(app);
#endif
		while (!app.quit) {
			SDL_Event event;
			while (SDL_PollEvent(&event)) {
				ImGui_ImplSDL2_ProcessEvent(&event);
				if (event.type == SDL_QUIT) {
					if (app.dirty())
						app.closing = true;
					else
						app.quit = true;
				}
				if (event.type == SDL_DROPFILE) {
					const auto dropped = fs::u8path(event.drop.file);
					SDL_free(event.drop.file);
					app.attempt([&] { app.dropFile(dropped); });
				}
				if (event.type == SDL_WINDOWEVENT &&
				    event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
					app.preview._pi = {};
					app.paused = true;
				}
			}
			ImGui_ImplSDLRenderer2_NewFrame();
			ImGui_ImplSDL2_NewFrame();
#ifdef REMINISCENCE2_EDITOR_TESTING
			if (!smoke.empty())
				test.input(frame, app);
#endif
			ImGui::NewFrame();
			app.frame();
			ImGui::Render();
			SDL_SetRenderDrawColor(renderer, 12, 18, 24, 255);
			SDL_RenderClear(renderer);
			ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
#ifdef REMINISCENCE2_EDITOR_TESTING
			if (!smoke.empty())
				test.check(frame, app);
#endif
			if (!shot.empty() && frame >= 3) {
				screenshot(renderer, shot);
				app.quit = true;
			}
			SDL_RenderPresent(renderer);
			++frame;
		}
	} catch (const std::exception &e) {
		std::fprintf(stderr, "Editor: %s\n", e.what());
		result = 1;
	}
	ImGui_ImplSDLRenderer2_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return result;
}
