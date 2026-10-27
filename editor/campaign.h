// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "content.h"
#include "image.h"
#include "asset_library.h"
namespace editor {
enum class StageKind { Level, Movie, Story, Choice, End };
enum class StoryKind { Text, Photo };
enum class StoryEffect { None, Typewriter, Fade };
struct StoryElement {
	StoryKind kind = StoryKind::Text;
	std::string text = "A new chapter begins.", file;
	int x = 40, y = 40, width = 560, height = 240, size = 22;
	std::array<int, 4> color{225, 242, 244, 255};
	StoryEffect effect = StoryEffect::None;
	int delay = 0, duration = 1000, rate = 30;
	// Translation is independent of text reveal; signed offsets may leave the canvas.
	int moveX = 0, moveY = 0, moveDuration = 1000;
};
struct Route {
	std::string key = "complete", label = "Continue", to, condition;
	bool requiredValue = true;
};
struct Stage {
	std::string id, name, comments;
	StageKind kind = StageKind::Level;
	int x = 24, y = 24;
	std::string level, movie = "SC1";
	std::array<int, 4> background{8, 18, 28, 255};
	std::vector<StoryElement> elements;
	std::vector<Route> routes;
	bool remember = true;
	int advance = 0; // milliseconds; zero waits for input, choices always wait
	int fadeOut = 0; // final milliseconds of a timed screen; includes every layer
};
struct Campaign {
	int schema = 2;
	bool carryInventory = true, carryHealth = true;
	std::string name = "New campaign", comments, entry;
	std::vector<Stage> stages;
};
std::vector<std::string> listCampaignMovies(const assets::Source &);
const char *stageName(StageKind);
int stageIndex(const Campaign &, const std::string &);
Stage newStage(const Campaign &, StageKind);
std::vector<std::string> validateCampaign(const Campaign &, const std::filesystem::path &root = {},
                                          bool files = false);
std::string serializeCampaign(const Campaign &);
Campaign deserializeCampaign(const std::string &);
Campaign loadCampaign(const std::filesystem::path &);
void saveCampaign(const Campaign &, const std::filesystem::path &);
std::vector<Route> campaignRoutes(const Stage &, const std::string &exit);
// Copies the project and its image dependencies beneath the campaign root.
std::string importCampaignLevel(const std::filesystem::path &, const std::filesystem::path &root);
Image renderStory(const Stage &, Images &, uint32_t elapsed, bool reveal = false);
void storyText(Image &, const std::string &, int x, int y, int width, int height, int size,
               uint32_t color, size_t visible = size_t(-1));
enum class CampaignMode { Ask, New, Resume };
int playCampaign(const std::filesystem::path &, const assets::Source &,
                 const std::string &start = {}, const std::filesystem::path &saves = {},
                 CampaignMode mode = CampaignMode::Ask);
} // namespace editor
