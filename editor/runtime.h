// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "content.h"
#include "image.h"
#include "audio.h"
#include "graph_runtime.h"
#include "authoring_hooks.h"
#include "asset_library.h"
#include <memory>
#include <optional>
struct Game;
struct FileSystem;
struct SystemStub;
namespace editor {
struct CampaignRun;
struct ModuleState {
	bool open = false, consumed = false;
	int actor = -1;
	GraphState graph;
	std::vector<int> actors, lastLife;
};
struct GameDeleter {
	void operator()(Game *) const;
};
// Initialize once per Game; reset reuses globals for a fresh level simulation.
void initializeOriginalLevel(Game &);
void resetOriginalLevel(Game &);
class Scene : public AuthoringHooks {
public:
	Scene(Project project, const std::filesystem::path &root, const assets::Source &, SystemStub *);
	Scene(Game &, Project, const std::filesystem::path &); // native extension, borrowed world
	~Scene();
	bool bindsActor(int) const;
	void nativeRuleFired(int actor, int rule);
	void renderOverlay(Image &);
	void tick();
	void loaded(Game &) override {}
	void boundary(Game &) override {}
	void beforeDraw(Game &) override {}
	void afterDraw(Game &) override {}
	bool beforeActor(Game &, LivePGE &) override;
	void afterSimulation(Game &) override;
	Image render();
	void restart();
	void activate(size_t module, bool onlyOutputs = false);
	int currentRoom() const;
	bool dead() const;
	bool paused() const { return _paused; }
	Game &game();
	const std::vector<ModuleState> &states() const { return _states; }
	const Project &project() const { return _project; }
	const std::string &exitRoute() const { return _exitRoute; }
	std::string saveSession() const;
	void restoreSession(const std::string &, bool revisit = false);
	bool canSave() const;
	bool checkpointRequested() const { return _checkpointDue; }
	void acknowledgeCheckpoint() { _checkpointDue = false; }
	void bindCampaign(CampaignRun *);
	void message(const std::string &text) {
		_notice = text;
		_noticeUntil = _tick + 90;
	}
	bool dialogueOpen() const { return !_dialogue.empty(); }

private:
	Project _project;
	std::map<std::string, bool> _localValues;
	std::map<std::string, bool> *_values = &_localValues;
	Images _images;
	assets::Source _source;
	SystemStub *_stub;
	std::unique_ptr<FileSystem> _files;
	std::unique_ptr<Game, GameDeleter> _ownedGame;
	Game *_game = nullptr;
	std::unique_ptr<SceneAudio> _audio; // detached before releasing the game/mixer
	void syncAudio(bool seek = false);
	bool _borrowed = false;
	std::vector<std::pair<int, int>> _nativeEvents;
	std::vector<ModuleState> _states;
	std::array<int8_t, 7424> _baseCollision;
	std::vector<uint8_t> _coverage;
	bool _previousUse = false;
	bool _paused = false;
	uint8_t _previousDirection = 0;
	uint64_t _tick = 0;
	std::vector<std::string> _dialogue;
	size_t _page = 0;
	struct DialogueRequest {
		size_t module = 0;
		int node = -1;
		uint64_t generation = 0;
		std::string text;
	};
	std::deque<DialogueRequest> _dialogues;
	int _choice = 0;
	std::string _exitRoute;
	std::optional<Anchor> _checkpoint;
	std::string _checkpointSnapshot; // standalone recovery preserves the entire puzzle
	bool _checkpointDue = false;
	std::string _notice;
	uint64_t _noticeUntil = 0;
	void updateCollision();
	bool canClose(const Anchor &, int width, int height) const;
	bool graphTick(bool use);
	void showDialogue(const std::string &);
	void graphFeedback(GraphMachine &, size_t module);
	void advanceActions();
	void syncDialogue();
	void finishDialogue();
	const Node *dialogueNode() const;
	void drawMessages(Image &);
	void finishTick();
	struct MotionStart {
		int x = 0, y = 0, room = -1;
	};
	std::array<MotionStart, 256> _motion;
	void enforceDoors();
};
// CLI player uses the regular platform/input layer, with no original saves,
// story hooks or persistent preferences modified by a custom-scene playtest.
int playProject(const std::filesystem::path &, const assets::Source &,
                const std::filesystem::path &control = {}, std::string *exitRoute = nullptr,
                CampaignRun *campaign = nullptr);
} // namespace editor
