// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMINISCENCE2_SETTINGS_OVERLAY_H
#define REMINISCENCE2_SETTINGS_OVERLAY_H
#include "settings.h"

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;
union SDL_Event;
struct ImGuiContext;

// UI presentation owns no game resources. The host applies queued changes only
// after rendering a frame, so renderer recreation cannot invalidate draw data.
class SettingsOverlay {
	ImGuiContext *_context = nullptr;
	SDL_Renderer *_renderer = nullptr;
	struct CachedPreview {
		SDL_Texture *texture;
		std::shared_ptr<const progress::Preview> owner;
		int frame;
	};
	std::vector<CachedPreview> _previews;
	bool _firstDraw = true;
	void clearPreview();
	void drawPreview(std::shared_ptr<const progress::Preview> preview);
	bool _attached = false, _closeArmed = false;
	bool _compact = false, _native = false;
	int _captureBinding = -1, _tab = 0;
	int _capturePad = -1;
	bool _captureRaw = false;
	int _requestedTab = -1;
	char _search[96] = {};
	settings::Preferences _opening;
	std::array<float, 4> _shortcut{};

public:
	SettingsOverlay();
	~SettingsOverlay();
	bool attach(SDL_Window *window, SDL_Renderer *renderer);
	void detach();
	void resetRenderer();
	void open(const settings::Preferences &preferences);
	void beginPadCapture(size_t action, bool raw);
	static constexpr int PageCount = 13;
	void selectPage(int index) { _requestedTab = index >= 0 && index < PageCount ? index : 0; }
	void processEvent(const SDL_Event &event, settings::Session *session);
	void beginFrame(const settings::Preferences &preferences);
	void draw(settings::Session &session);
	// Returns true when the on-screen QoL shortcut was clicked.
	bool drawHud(const settings::Preferences &preferences, bool gameplay, float journalOpacity = 0,
	             const std::string &contextHint = {}, const std::string &notice = {},
	             bool noticeError = false, float noticeOpacity = 1);
	bool shortcutContains(float x, float y) const;
	void render();
};
#endif
