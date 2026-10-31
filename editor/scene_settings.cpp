// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtime.h"
#include "campaign_progress.h"
#include "game.h"
#include "systemstub.h"
#include <algorithm>

namespace editor {
bool Scene::handleMenuInput(CampaignRun *campaign, const std::filesystem::path &directory) {
	if (_stub->_pi.quit || (!_stub->_pi.escape && !_stub->consumePauseRequest()))
		return false;
	showSettings(campaign, directory);
	return true;
}
void Scene::present() {
	const auto frame = render();
	_presentRgb.resize(frame.pixels.size() * 3);
	for (size_t i = 0; i < frame.pixels.size(); ++i)
		for (int c = 0; c < 3; ++c)
			_presentRgb[i * 3 + c] = uint8_t(frame.pixels[i] >> (c * 8));
	_stub->copyRectRgb24(0, 0, frame.width, frame.height, _presentRgb.data());
}

void Scene::showSettings(CampaignRun *campaign, const std::filesystem::path &directory) {
	auto &g = *_game;
	settings::Session s;
	s.authoredScene = s.campaign = true;
	s.values = g._preferences;
	_stub->readPreferences(s.values);
	s.edition = _project.name;
	s.language = "English";
	s.room = currentRoom();
	s.health = g._pgeLive[0].life;
	s.score = g._score;
	s.canSave = canSave();
	s.canLoad = campaign ? std::filesystem::exists(campaign->path) : !_checkpointSnapshot.empty();
	s.levelAvailable.fill(false);
	s.experimentalPistolAvailable = true;
	s.outputRate = _stub->getOutputSampleRate();
	s.preferencePath = (directory / "authored-preferences.cfg").u8string();
	s.diagnostics = {"Authored project: " + _project.name,
	                 "Room links, collisions and puzzles come from the editor project.",
	                 "Esc / F10: QoL menu. Resume continues this exact scene."};
	s.apply = [&](const settings::Preferences &values, std::string &reason) {
		if (values.widescreen != kWidescreenNone) {
			reason = "Authored rooms use their designed playfield; window size and pixel filters "
			         "are available.";
			return false;
		}
		if (!settings::validate(values, reason) || !_stub->applyPreferences(values, reason))
			return false;
		// Game::applyPreferences rebuilds native room resources. Custom RGB rooms
		// own their compositor, so apply shared input/audio/cosmetics directly.
		g._preferences = values;
		g._preferences.applyCompatibility(g_options);
		g._vid._conradShirt = values.conradShirt;
		g._vid.refreshConradShirtPalette();
		g._weaponFx.clear();
		g._mix.setVolumes(values.masterVolume, values.musicVolume, values.effectsVolume,
		                  values.muted);
		present();
		return true;
	};
	s.persist = [&](const settings::Preferences &values, std::string &reason) {
		std::error_code error;
		std::filesystem::create_directories(directory, error);
		if (error) {
			reason = error.message();
			return false;
		}
		return settings::save(s.preferencePath, values, reason);
	};
	s.applyCheats = [&](uint32_t cheats, uint8_t flags) {
		g._cheats = cheats;
		_stub->_pi.dbgMask = flags & (PlayerInput::DF_FASTMODE | PlayerInput::DF_SETLIFE);
	};
	s.cheats = g._cheats;
	s.debugFlags = _stub->_pi.dbgMask;
	s.command = [&](settings::Action action, int, std::string &reason) {
		try {
			bool success = false;
			if (action == settings::Action::SaveGame) {
				if (campaign)
					success = campaign->save(*this, reason);
				else {
					_checkpointSnapshot = saveSession();
					reason = "Playtest checkpoint stored for this session.";
					success = true;
				}
				if (success)
					s.canLoad = true;
			} else if (action == settings::Action::LoadGame) {
				if (campaign)
					success = campaign->load(*this, reason);
				else if (!_checkpointSnapshot.empty()) {
					restoreSession(_checkpointSnapshot);
					reason = "Playtest checkpoint restored.";
					success = true;
				} else
					reason = "No playtest checkpoint has been stored yet.";
			} else if (action == settings::Action::Heal && canSave()) {
				g._pgeLive[0].life = std::max<int>(5, g._pgeLive[0].life);
				reason = "Health restored.";
				success = true;
			} else if (action == settings::Action::Screenshot) {
				std::filesystem::create_directories(directory);
				static unsigned serial = 0;
				const auto file = directory / ("scene-" + std::to_string(_stub->getTimeStamp()) +
				                               "-" + std::to_string(++serial) + ".png");
				writePNG(render(), file);
				reason = "Screenshot saved: " + file.u8string();
				success = true;
			} else
				reason = "Use the authored campaign routes or choose a test room in the editor.";
			s.health = g._pgeLive[0].life;
			s.room = currentRoom();
			s.canSave = canSave();
			present();
			return success;
		} catch (const std::exception &e) {
			reason = e.what();
			return false;
		}
	};
	const bool wasPaused = _paused;
	_paused = false; // Render the frozen game rather than a second pause banner.
	present();
	_stub->_pi.escape = false;
	_stub->setGameplayActive(false);
	_stub->showSettings(s);
	const bool quit = _stub->_pi.quit || s.action == settings::Action::Quit;
	const auto flags = _stub->_pi.dbgMask;
	_stub->_pi = {};
	_stub->_pi.quit = quit;
	_stub->_pi.dbgMask = flags;
	_previousUse = _previousAdvance = false;
	_previousDirection = 0;
	_paused = wasPaused && s.action != settings::Action::Resume;
	g._frameTimestamp = _stub->getTimeStamp();
}
} // namespace editor
