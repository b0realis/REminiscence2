// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "content.h"
#include "graph_runtime.h"
#include "playtest.h"
#include <optional>
#include "image.h"
#include "asset_library.h"
#include "authoring_hooks.h"
#include <map>
namespace editor {
class Scene;
struct CampaignRun;
// One project per native level; sibling level-N.r2project.json files form a
// campaign. Missing siblings continue with original assets. No native IDs or
// script operand tables are regenerated or compacted.
class NativeSession final : public AuthoringHooks {
public:
	NativeSession(const std::filesystem::path &project, const assets::Source &,
	              const std::filesystem::path &control = {});
	~NativeSession();
	void loaded(Game &) override;
	void boundary(Game &) override;
	void beforeDraw(Game &) override;
	void afterDraw(Game &) override;
	void afterActors(Game &) override;
	bool beforeSimulation(Game &) override;
	bool beforeActor(Game &, LivePGE &) override;
	void afterSimulation(Game &) override;
	bool allowRule(Game &, LivePGE &, const Object &) override;
	void ruleExecuted(Game &, LivePGE &, const Object &) override;
	void messageSent(Game &, int, int, int) override;
	int behaviorIdentity(int behavior) const override;
	struct Message {
		int source = 0, target = 0, value = 0;
		uint64_t serial = 0;
	};
	struct ActorState {
		int id = 0, room = 0, x = 0, y = 0, state = 0, life = 0, counter = 0;
		bool active = false;
	};
	struct Monitor {
		std::vector<Message> messages;
		std::vector<ActorState> actors;
	};
	static Monitor readMonitor(const std::filesystem::path &, const NativeCampaign &);
	struct Event {
		int actor = 0, behavior = 0, rule = 0, state = 0, next = 0;
		uint64_t serial = 0;
	};
	static std::map<std::string, GraphState> readGraphs(const std::filesystem::path &);
	static std::vector<Event> readEvents(const std::filesystem::path &,
	                                     const NativeCampaign *expected = nullptr);
	bool stopRequested() const;
	CampaignRun *campaign = nullptr;
	Scene &campaignScene(Game &);
	bool ownsSaves() const override { return campaign != nullptr; }
	bool saveCampaign(Game &, std::string &) override;
	bool loadCampaign(Game &, std::string &) override;
	bool campaignStage = false;
	std::string exitRoute;
	bool levelExit(Game &, int) override;
	void apply(Game &, const Project &, bool restart);
	void playHere(Game &, const Project &, const PlaytestStart &);
	bool present(Game &);
	bool paused() const { return _paused; }
	bool restartRequired() const { return _restartRequired; }
	const std::string &message() const { return _message; }
	const Project &project() const { return _project; }
	static void publish(const std::filesystem::path &control, const std::string &action,
	                    const Project &project, const PlaytestStart *start = nullptr);
	static bool pending(const std::filesystem::path &control);
	static std::string statusText(const std::filesystem::path &control);

private:
	std::filesystem::path _root, _control;
	assets::Source _source;
	Project _project;
	std::map<int, Project> _levels;
	std::map<int, Project> _originals;
	Images _images;
	bool _paused = false, _restartRequired = false, _drawing = false;
	std::string _message = "Native campaign running", _seen;
	std::filesystem::file_time_type _lastCommand{};
	uint32_t _statusTime = 0;
	std::vector<uint8_t> _coverage, _baselinePixels, _actorPixels, _rgb;
	Image _composed;
	std::optional<PlaytestStart> _launchAt;
	std::unique_ptr<Scene> _overlay;
	std::vector<Event> _events;
	std::vector<Message> _messages;
	uint64_t _definitionRevision = 0;
	bool _stepPending = false;
	uint64_t _eventSerial = 0;
	void verify(const Project &, const Project &) const;
	void command(Game &);
	void refreshPaused(Game &);
	void status(Game &);
};
int playNativeProject(const std::filesystem::path &, const assets::Source &,
                      const std::filesystem::path &control = {}, std::string *exitRoute = nullptr,
                      CampaignRun *campaign = nullptr);
} // namespace editor
