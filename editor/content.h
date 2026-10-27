// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace editor {
constexpr int RoomWidth = 256, RoomHeight = 224, RoomStrideY = 216;
constexpr int MaxRooms = 64, MaxModules = 64;
enum class Layer { Back, Playfield, Front };
enum class Kind { Door, Prop, Medkit, Crawler, Graph };
struct Art {
	std::string file;
	int x = 0, y = 0, w = 256, h = 224;
};
struct Ledge {
	int column = 4, row = 4, facing = 1;
};
struct Room {
	std::string id, name;
	int column = 0, row = 0;
	std::array<Art, 3> art;
	std::array<uint8_t, 112> collision{};
	std::vector<Ledge> ledges;
	std::array<std::string, 4> exits; // up, down, right, left; empty = sealed
};
struct Anchor {
	std::string room;
	int x = 48, y = 214;
};
// Node identities are scoped to their owning module. Copying a module preserves
// its internal connections without ever sharing puzzle state with the original.
enum class NodeKind {
	Button,
	Door,
	Npc,
	Item,
	Computer,
	Zone,
	Relay,
	Prop,
	Dialogue,
	Animation,
	PickupAnimation,
	Timer,
	State,
	Actor,
	HealthPickup,
	Damage,
	Counter,
	ItemCondition,
	Checkpoint,
	Teleport,
	LevelExit,
	Start,
	CampaignSet,
	CampaignTest,
	Music,
	Sound,
	Count
};
enum class Signal {
	Used,
	Completed,
	Opened,
	Closed,
	Entered,
	Started,
	Stopped,
	Choice1,
	Choice2,
	Exited,
	Hurt,
	Died,
	Reached,
	StateChanged
};
enum class Input {
	Activate,
	Open,
	Close,
	Toggle,
	Progress,
	Reset,
	Enable,
	Disable,
	Stop,
	Friendly,
	Hostile,
	Patrol,
	Hold
};
struct Animation {
	Art art;
	int frames = 1, fps = 6;
	bool loop = true;
};
struct Node {
	std::string id, name, comments;
	NodeKind kind = NodeKind::Button;
	Anchor at;
	int graphX = 24, graphY = 24;
	int width = 16, height = 24;
	Layer layer = Layer::Playfield;
	std::array<Animation, 2> visuals; // idle and active/completed, horizontal strips
	bool enabled = true, initiallyActive = false, once = false, consumeItem = true;
	std::string requiredItem, text, doneText, campaignKey;
	int goal = 1;
	std::string speaker;
	std::array<std::string, 2> choices; // both empty = continue; otherwise two branches
	int duration = 30;                  // timer ticks at 30 Hz
	bool friendly = false, patrol = false;
	int nativeActor = -1, nativeRule = -1, message = 1, messageFrom = 0;
	int patrolLeft = 32, patrolRight = 208, speed = 1, health = 2;
	std::string audioFile;
	int audioVolume = 70;
	bool audioLoop = false;
	std::string stateGroup = "main"; // mutually exclusive states, local to one module
};
struct Wire {
	std::string from, to;
	Signal signal = Signal::Used;
	Input input = Input::Open;
};
// A door's two anchors are owned by ONE instance. There is no global switch ID
// to accidentally retain when duplicating a module. Other modules use part A.
struct Module {
	std::string id, name, comments;
	Kind kind = Kind::Door;
	Anchor a, b;
	bool initiallyOpen = false, toggle = false, facingLeft = false;
	int health = 2, width = 16, height = 72;
	Art sprite;
	int frames = 1, fps = 6;
	Layer layer = Layer::Playfield;
	std::vector<std::string> outputs; // activated -> open another door module
	std::vector<Art> assets;          // module-owned image library; clips refer to these files
	std::vector<Node> nodes;
	std::vector<Wire> wires;
};
// Stable original slots retain campaign identity. Copies carry the original actor
// template ID; untouched sprite/animation/resource metadata comes from that template.
struct NativeActor {
	int id = 0, room = 0, x = 0, y = 0, life = 0;
	bool facingLeft = false;
	int type = 0, behavior = 0, objectKind = 0;
	std::array<int, 4> data{};
	int source = -1; // -1: original slot; otherwise pristine actor template in this level.
	std::string comments;
	bool removed = false; // tombstone: keep slot/reference identity for undo and restoration
};
// Lossless native state-machine rows. IDs and row numbers remain stable so
// native actor, message and inventory references keep their original meaning.
struct NativeRule {
	int type = 0, nextType = 0, next = 0, dx = 0, dy = 0, flags = 0;
	std::array<int, 3> opcode{}, argument{};
	bool enabled = true;
	std::string comments;
};
struct NativeProgram {
	std::vector<NativeRule> rules;
	int source = -1; // -1: original behavior slot; otherwise original behavior vocabulary.
};
struct NativePuzzle {
	std::string name;
	std::vector<int> actors;
	std::string comments;
};
struct NativeCampaign {
	bool enabled = false;
	int level = 0, edition = 0;
	std::string fingerprint;
	std::vector<NativeActor> actors;
	std::vector<NativeProgram> programs;
	std::vector<NativePuzzle> puzzles;
};
struct Project {
	int schema = 9;
	std::string id, name = "New campaign";
	uint32_t serial = 1;
	std::vector<Room> rooms;
	std::vector<Module> modules;
	Anchor spawn;
	bool spawnLeft = false;
	NativeCampaign native;
};
struct Issue {
	bool error;
	std::string message;
	std::string room, module, node;
	int actor = -1, layer = -1;
};
struct Slice {
	int width = 256, height = 224, strideX = 256, strideY = 216;
};
Project makeProject();
Room &addRoom(Project &p, int column, int row);
std::string newId(Project &p, const char *prefix);
int roomIndex(const Project &, const std::string &id);
Module makeModule(Project &, Kind, const Anchor &);
Module duplicateModule(Project &, const Module &);
// Clipboard owns complete modules and imported images. Pasting remaps links
// inside the copied group and drops external links; the operation is atomic.
struct ModuleClipboard {
	std::vector<Module> modules;
	std::filesystem::path sourceRoot;
	std::string originRoom;
};
ModuleClipboard copyModules(const Project &, const std::vector<std::string> &,
                            const std::filesystem::path &, const std::string &room);
std::vector<std::string> pasteModules(Project &, const ModuleClipboard &,
                                      const std::filesystem::path &, const std::string &room);
void translateModules(Project &, const std::vector<std::string> &, int dx, int dy);
void connectRooms(Project &, int room, int direction, int target, bool reciprocal);
std::vector<Issue> validate(const Project &);
std::array<int8_t, 7424> compileCollision(const Project &);
std::string serialize(const Project &);
uint64_t moduleRevision(const Module &);
uint64_t nativeRevision(const NativeCampaign &);
Project deserialize(const std::string &);
Project loadProject(const std::filesystem::path &);
void saveProject(const Project &, const std::filesystem::path &);
bool safeAssetName(const std::string &);
bool validCampaignKey(const std::string &);
std::filesystem::path assetPath(const std::filesystem::path &root, const std::string &relative);
std::string importImage(const std::filesystem::path &source, const std::filesystem::path &root);
void sliceImage(Project &, const std::string &image, int imageWidth, int imageHeight, const Slice &,
                Layer, bool rebuildRooms);
void savePrefab(const Module &, const std::filesystem::path &projectRoot,
                const std::filesystem::path &destination);
Module loadPrefab(Project &, const Anchor &, const std::filesystem::path &source,
                  const std::filesystem::path &projectRoot);
const char *kindName(Kind);
const char *layerName(Layer);
const char *nodeName(NodeKind);
const char *nodeHelp(NodeKind);
const char *nodeCategory(NodeKind);
const char *signalName(Signal);
const char *inputName(Input);
std::vector<Signal> nodeSignals(NodeKind);
std::vector<Input> nodeInputs(NodeKind);
int nodeIndex(const Module &, const std::string &);
Node makeNode(const Module &, NodeKind, const Anchor &);
Module makeGraph(Project &, int recipe, const Anchor &);
const char *recipeName(int);
void removeNode(Module &, size_t);
void forEachArt(Module &, const std::function<void(Art &)> &);
void validateGraph(const Project &, const Module &, std::vector<Issue> &);

// One history entry per completed gesture, with a bounded memory budget.
struct History {
	std::vector<std::string> undoStack, redoStack;
	void remember(const Project &);
	bool undo(Project &);
	bool redo(Project &);
};
} // namespace editor
