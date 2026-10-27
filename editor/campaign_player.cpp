// SPDX-License-Identifier: GPL-3.0-or-later
#include "campaign.h"
#include "campaign_progress.h"
#include "runtime.h"
#include "cutscene_tool.h"
#include "game.h"
#include "fs.h"
#include "systemstub.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
namespace editor {
// The editor and player use this same raster layout. The default ImGui font is
// engine-owned; authored story screens need no font ripped from original assets.
struct StoryFont {
	ImFontAtlas atlas;
	ImFont *font;
	unsigned char *pixels = nullptr;
	int width = 0, height = 0;
	StoryFont() {
		font = atlas.AddFontDefault();
		atlas.GetTexDataAsAlpha8(&pixels, &width, &height);
	}
};
void storyText(Image &image, const std::string &text, int x, int y, int width, int height, int size,
               uint32_t color, size_t visible) {
	static StoryFont raster;
	auto *font = raster.font;
	const float scale = std::clamp(size, 8, 64) / font->FontSize;
	float cursor = float(x), row = float(y);
	const int right = std::min(image.width, x + width), bottom = std::min(image.height, y + height);
	for (size_t i = 0; i < text.size(); ++i) {
		const unsigned char c = text[i];
		if (c == '\n') {
			cursor = float(x);
			row += size + 4;
			continue;
		}
		if (row >= bottom)
			break;
		const auto *g = font->FindGlyph(c >= 32 && c < 127 ? c : '?');
		if (!g)
			continue;
		if (c != ' ' && (i == 0 || text[i - 1] == ' ' || text[i - 1] == '\n')) {
			float word = 0;
			for (size_t j = i; j < text.size() && text[j] != ' ' && text[j] != '\n'; ++j)
				word += font->GetCharAdvance((unsigned char)text[j]) * scale;
			if (cursor > x && cursor + word > right) {
				cursor = float(x);
				row += size + 4;
			}
		}
		if (cursor + g->AdvanceX * scale > right && cursor > x) {
			cursor = float(x);
			row += size + 4;
		}
		const int gx = int(cursor + g->X0 * scale), gy = int(row + g->Y0 * scale);
		const int gw = std::max(0, int(std::ceil((g->X1 - g->X0) * scale))),
		          gh = std::max(0, int(std::ceil((g->Y1 - g->Y0) * scale)));
		if (i < visible)
			for (int yy = std::max(y, gy); yy < std::min(bottom, gy + gh); ++yy)
				for (int xx = std::max(x, gx); xx < std::min(right, gx + gw); ++xx) {
					const int sx = std::clamp(int(g->U0 * raster.width + (xx - gx) / scale), 0,
					                          raster.width - 1);
					const int sy = std::clamp(int(g->V0 * raster.height + (yy - gy) / scale), 0,
					                          raster.height - 1);
					const unsigned alpha =
					    raster.pixels[sy * raster.width + sx] * (color >> 24) / 255;
					if (xx >= 0 && yy >= 0)
						over(image.pixels[yy * image.width + xx],
						     (color & 0xffffff) | (alpha << 24));
				}
		cursor += g->AdvanceX * scale;
	}
}
Image renderStory(const Stage &stage, Images &images, uint32_t elapsed, bool reveal) {
	const auto &bg = stage.background;
	Image out{640, 360, std::vector<uint32_t>(640 * 360, rgba(bg[0], bg[1], bg[2], bg[3]))};
	for (const auto &e : stage.elements) {
		if (!reveal && elapsed < uint32_t(std::max(0, e.delay)))
			continue;
		const uint32_t age = elapsed - std::min(elapsed, uint32_t(std::max(0, e.delay)));
		const float travel = std::min(1.f, age / float(std::max(1, e.moveDuration)));
		const int x = e.x + int(std::lround(e.moveX * travel));
		const int y = e.y + int(std::lround(e.moveY * travel));
		const float fade = !reveal && e.effect == StoryEffect::Fade
		                       ? std::min(1.f, age / float(std::max(1, e.duration)))
		                       : 1.f;
		const auto color = rgba(e.color[0], e.color[1], e.color[2], int(e.color[3] * fade));
		if (e.kind == StoryKind::Text) {
			const size_t visible =
			    !reveal && e.effect == StoryEffect::Typewriter
			        ? std::min<uint64_t>(e.text.size(), uint64_t(age) * std::max(1, e.rate) / 1000)
			        : e.text.size();
			storyText(out, e.text, x, y, e.width, e.height, e.size, color, visible);
		} else if (!e.file.empty()) {
			const auto &photo = images.get(e.file);
			const float scale =
			    std::min(e.width / float(photo.width), e.height / float(photo.height));
			const int w = std::max(1, int(photo.width * scale)),
			          h = std::max(1, int(photo.height * scale));
			Image layer{640, 360, std::vector<uint32_t>(640 * 360)};
			blit(layer, photo, {e.file, 0, 0, photo.width, photo.height}, x + (e.width - w) / 2,
			     y + (e.height - h) / 2, w, h);
			for (size_t i = 0; i < out.pixels.size(); ++i)
				over(out.pixels[i],
				     (layer.pixels[i] & 0xffffff) |
				         (uint32_t((layer.pixels[i] >> 24) * fade * e.color[3] / 255.f) << 24));
		}
	}
	// Timed cinematic screens fade the fully composited frame, including text.
	// Revealing a caption must not cancel motion or the final fade to black.
	if (stage.advance > 0 && stage.fadeOut > 0 &&
	    elapsed > uint32_t(std::max(0, stage.advance - stage.fadeOut))) {
		const unsigned remaining = elapsed >= uint32_t(stage.advance) ? 0 : stage.advance - elapsed;
		const unsigned strength = std::min(255u, remaining * 255 / unsigned(stage.fadeOut));
		for (auto &pixel : out.pixels)
			pixel = rgba((pixel & 255) * strength / 255,
			             ((pixel >> 8) & 255) * strength / 255,
			             ((pixel >> 16) & 255) * strength / 255);
	}
	return out;
}
static int screen(const Stage &stage, Images &images, const std::vector<Route> &routes) {
	std::unique_ptr<SystemStub> stub(SystemStub_SDL_create());
	try {
		settings::Preferences p;
		p.fullscreen = false;
		p.scale = 1;
		stub->configurePreferences(p);
		auto scaler = ScalerParameters::defaults();
		std::snprintf(scaler.name, sizeof(scaler.name), "point");
		scaler.factor = 1;
		stub->init(stage.name.c_str(), 640, 360, false, kWidescreenNone, &scaler);
		Images local;
		local.root = images.root;
		// Suspended/background time must not finish a timed story screen while
		// the player is away. Media playback uses the same active-time clock.
		const auto began = stub->getTimeStamp() - stub->getPausedTime();
		bool enter = false, reveal = false;
		uint8_t dir = 0;
		int selected = 0, result = -1;
		std::vector<uint8_t> rgb(640 * 360 * 3);
		while (!stub->_pi.quit && !stub->_pi.escape) {
			stub->setGameplayActive(false);
			stub->processEvents();
			if (stub->_pi.quit || stub->_pi.escape)
				break;
			const uint32_t elapsed = stub->getTimeStamp() - stub->getPausedTime() - began;
			const uint8_t next = stub->_pi.dirMask;
			if (routes.size() > 1 && next != dir) {
				if (next & PlayerInput::DIR_DOWN)
					selected = (selected + 1) % int(routes.size());
				if (next & PlayerInput::DIR_UP)
					selected = (selected + int(routes.size()) - 1) % int(routes.size());
			}
			dir = next;
			if (stub->_pi.enter && !enter) {
				if (!reveal)
					reveal = true;
				else {
					result = selected;
					break;
				}
			}
			enter = stub->_pi.enter;
			const bool finished = routes.size() <= 1 && stage.kind != StageKind::Choice &&
			                      stage.advance > 0 && elapsed >= uint32_t(stage.advance);
			auto frame = renderStory(stage, local, finished ? stage.advance : elapsed, reveal);
			if (!stage.advance || stage.kind == StageKind::Choice || routes.size() > 1) {
				const int panel = routes.size() > 1 ? int(routes.size()) * 23 + 24 : 24;
				rectangle(frame, 0, 360 - panel, 640, panel, rgba(6, 14, 22, 235));
				if (routes.size() > 1)
					for (size_t i = 0; i < routes.size(); ++i)
						storyText(frame, (int(i) == selected ? "> " : "  ") + routes[i].label, 16,
						          364 - panel + int(i) * 23, 608, 23, 17,
						          int(i) == selected ? rgba(85, 235, 215) : rgba(220, 230, 238));
				storyText(frame,
				          reveal ? "Enter: continue / choose   Up/Down: route   Esc: leave"
				                 : "Enter: reveal text   Esc: leave",
				          16, 340, 608, 20, 14, rgba(180, 211, 220));
			}
			for (size_t i = 0; i < frame.pixels.size(); ++i)
				for (int c = 0; c < 3; ++c)
					rgb[i * 3 + c] = uint8_t(frame.pixels[i] >> (c * 8));
			stub->copyRectRgb24(0, 0, 640, 360, rgb.data());
			stub->updateScreen(0);
			if (finished) {
				result = 0;
				break;
			}
			stub->sleep(16);
		}
		stub->destroy();
		return result;
	} catch (...) {
		stub->destroy();
		throw;
	}
}
static bool movie(const Stage &stage, const assets::Source &source) {
	std::unique_ptr<SystemStub> stub(SystemStub_SDL_create());
	try {
		FileSystem files(source.root.c_str(), source.excludedRoots);
		std::unique_ptr<Game, GameDeleter> game(new Game(stub.get(), &files, &files, "", 0,
		                                                 ResourceType(source.edition), LANG_EN,
		                                                 kWidescreenNone, false, 0));
		settings::Preferences p;
		p.fullscreen = false;
		p.scale = 3;
		p.skipCutscenesWithKeyboard = true;
		stub->configurePreferences(p);
		game->configurePreferences(p);
		auto scaler = ScalerParameters::defaults();
		scaler.factor = 3;
		stub->init(stage.name.c_str(), 256, 224, false, kWidescreenNone, &scaler);
		game->initCutsceneResources();
		cinematics::Selection selection;
		if (!cinematics::parseSelection(stage.movie.c_str(), selection) ||
		    !cinematics::play(*game, selection))
			throw std::runtime_error("Movie unavailable in selected assets: " + stage.movie);
		const bool ok = !stub->_pi.quit;
		game.reset();
		stub->destroy();
		return ok;
	} catch (...) {
		stub->destroy();
		throw;
	}
}
int playCampaign(const std::filesystem::path &path, const assets::Source &source,
                 const std::string &start, const std::filesystem::path &saves, CampaignMode mode) {
	try {
		const auto c = loadCampaign(path);
		const auto root = std::filesystem::absolute(path).parent_path();
		const auto errors = validateCampaign(c, root, true);
		if (!errors.empty())
			throw std::runtime_error(errors.front());
		Images images;
		images.root = root;
		CampaignRun run;
		run.campaign = c;
		run.progress.identity = campaignIdentity(c, root);
		run.progress.edition = source.edition;
		run.progress.stage = start.empty() ? c.entry : start;
		if (stageIndex(c, run.progress.stage) < 0)
			throw std::runtime_error("Unknown campaign start stage");
		// Path identity keeps distinct copies/test starts separate. The save payload
		// contains content hashes, never the user's absolute project/assets paths.
		uint64_t location = 14695981039346656037ull;
		for (unsigned char b :
		     std::filesystem::absolute(path).lexically_normal().generic_u8string() + start) {
			location ^= b;
			location *= 1099511628211ull;
		}
		run.path =
		    (saves.empty() ? root / ".campaign-saves" : saves) / "campaigns" /
		    (std::to_string(location) + "-" + std::to_string(source.edition) + ".r2save.json");
		const bool exists = std::filesystem::exists(run.path);
		if (!start.empty())
			mode = CampaignMode::New;
		if (exists && mode == CampaignMode::Ask) {
			Stage menu;
			menu.name = c.name;
			menu.kind = StageKind::Choice;
			menu.elements.emplace_back();
			menu.elements[0].text = c.name + "\nCampaign progress is available.";
			const int action =
			    screen(menu, images,
				       {{"complete", "Resume campaign", ""}, {"complete", "New campaign", ""}});
			if (action < 0)
				return 0;
			mode = action == 0 ? CampaignMode::Resume : CampaignMode::New;
		}
		if (mode == CampaignMode::Resume) {
			if (!exists)
				throw std::runtime_error("No campaign save yet. Start a new campaign first.");
			run.progress = loadCampaignProgress(run.path, c, run.progress.identity, source.edition);
			run.resumeLevel = true;
		}
		run.store();
		std::string id = run.progress.stage;
		// Cycles/forks are legal. Every stage waits for play/input or a bounded
		// presentation; a navigation budget prevents accidental infinite loops.
		for (int visits = 0; visits < 4096; ++visits) {
			const int at = stageIndex(c, id);
			if (at < 0)
				throw std::runtime_error("Missing campaign stage: " + id);
			const auto &s = c.stages[at];
			std::string exit = run.progress.exit.empty() ? "complete" : run.progress.exit;
			if (s.kind == StageKind::Level && run.progress.exit.empty()) {
				exit.clear();
				if (playProject(assetPath(root, s.level), source, {}, &exit, &run))
					return 1;
				if (exit.empty())
					return 0;
			} else if (s.kind == StageKind::Movie && run.progress.exit.empty() && !movie(s, source))
				return 0;
			run.progress.exit = s.kind == StageKind::End ? "" : exit;
			run.store();
			auto routes = campaignRoutes(s, exit);
			routes.erase(std::remove_if(routes.begin(), routes.end(),
			                            [&](const Route &r) {
				                            if (r.condition.empty())
					                            return false;
				                            const auto found =
				                                run.progress.values.find("flag:" + r.condition);
				                            return (found != run.progress.values.end() &&
											        found->second) != r.requiredValue;
			                            }),
			             routes.end());
			int chosen = 0;
			if (s.kind == StageKind::Story || s.kind == StageKind::Choice ||
			    s.kind == StageKind::End)
				chosen = screen(s, images, routes);
			else if (routes.size() > 1) {
				Stage choice;
				choice.name = "Choose your route";
				choice.kind = StageKind::Choice;
				chosen = screen(choice, images, routes);
			}
			if (chosen < 0)
				return 0;
			if (s.kind == StageKind::End) {
				run.progress.finished = true;
				run.progress.exit.clear();
				run.store();
				return 0;
			}
			if (routes.empty())
				throw std::runtime_error("Unconnected exit: " + s.name + " / " + exit);
			id = routes.at(chosen).to;
			run.progress.stage = id;
			run.progress.exit.clear();
			run.resumeLevel = false;
			++run.progress.visits;
			run.store();
		}
		throw std::runtime_error("Campaign navigation limit reached (4096 stages)");
	} catch (const std::exception &e) {
		std::fprintf(stderr, "Campaign: %s\n", e.what());
		return 1;
	}
}
} // namespace editor
