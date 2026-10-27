// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "campaign.h"
#include <map>
namespace editor {
class Scene;
// Shared keys are designer-chosen identities, never native actor slot numbers.
using CampaignValues = std::map<std::string, bool>;
struct CampaignProgress {
	std::string identity, stage, exit;
	int edition = 0, health = 5, visits = 0;
	bool finished = false;
	CampaignValues values;
	std::map<std::string, std::string> levels; // stage ID -> validated Scene snapshot
	std::vector<std::array<int, 5>> inventory; // type, behavior, object kind, icon, quantity
};
std::string campaignIdentity(const Campaign &, const std::filesystem::path &);
std::string serializeProgress(const CampaignProgress &);
CampaignProgress deserializeProgress(const std::string &, const Campaign &,
                                     const std::string &identity, int edition);
CampaignProgress loadCampaignProgress(const std::filesystem::path &, const Campaign &,
                                      const std::string &identity, int edition);
void saveCampaignProgress(const CampaignProgress &, const std::filesystem::path &);
// Owns the in-memory working session. Writes replace a complete bounded document
// atomically, so level state and carried inventory can never come from different saves.
struct CampaignRun {
	Campaign campaign;
	CampaignProgress progress;
	std::filesystem::path path;
	bool resumeLevel = false;
	uint32_t lastSave = 0;
	void enter(Scene &);
	void capture(Scene &);
	bool save(Scene &, std::string &reason);
	bool load(Scene &, std::string &reason);
	void poll(Scene &);
	void store();
};
} // namespace editor
