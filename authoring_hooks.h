// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>
struct Game;
struct LivePGE;
struct Object;
// Optional editor integration. The normal player has no hooks installed.
// Native scripts, inventory, cinematics and transitions remain in Game.
struct AuthoringHooks {
	virtual ~AuthoringHooks() = default;
	virtual void loaded(Game &) = 0;   // Resources decoded, before any actor is initialized.
	virtual void boundary(Game &) = 0; // Between complete simulation frames.
	virtual void beforeDraw(Game &) = 0;
	virtual void afterActors(Game &) {}
	virtual bool beforeSimulation(Game &) { return true; }
	virtual bool beforeActor(Game &, LivePGE &) { return true; }
	// Authored scenes may seal absent room links before native scripts wrap the
	// actor's coordinates. Original campaigns keep their native transition rules.
	virtual void constrainActor(Game &, LivePGE &) {}
	virtual void afterSimulation(Game &) {}
	// -1 delegates hit eligibility to the original script. Custom actors keep
	// the native pistol ray, occlusion and cosmetic impact positions.
	virtual int acceptsHit(const LivePGE &, int) const { return -1; }
	// Return the exact impact X, or -1. Called along the native gun ray after
	// actor/wall tests; custom target feedback is delivered after simulation.
	virtual int gunRayHit(int, int, int, int, int) { return -1; }
	virtual bool drawsNativeActor(const LivePGE &) const { return true; }
	virtual bool allowRule(Game &, LivePGE &, const Object &) { return true; }
	virtual void ruleExecuted(Game &, LivePGE &, const Object &) {}
	virtual void messageSent(Game &, int source, int target, int message) {}
	virtual int behaviorIdentity(int behavior) const { return behavior; }
	virtual bool ownsSaves() const { return false; }
	virtual bool saveCampaign(Game &, std::string &) { return false; }
	virtual bool loadCampaign(Game &, std::string &) { return false; }
	virtual bool levelExit(Game &, int) { return false; }
	virtual void afterDraw(Game &) = 0;
};
