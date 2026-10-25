// SPDX-License-Identifier: GPL-3.0-or-later
#include "display_modes.h"
#include "scaler.h"
#include "systemstub.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

Options g_options;
const char *g_caption = "Presentation tests";
static void check(bool ok, const char *message) {
	if (!ok) throw std::runtime_error(message);
}
static SDL_Window *gameWindow() {
	for (Uint32 id = 1; id < 4096; ++id)
		if (auto *window = SDL_GetWindowFromID(id)) return window;
	return nullptr;
}
static std::vector<uint32_t> capture(SDL_Renderer *renderer, int width, int height) {
	std::vector<uint32_t> pixels(size_t(width) * height);
	check(SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGB888, pixels.data(), width * 4) == 0,
	      "Could not capture presentation");
	for (auto &pixel : pixels) pixel &= 0xFFFFFF;
	return pixels;
}
static std::vector<uint32_t> reference(const std::vector<uint32_t> &native, int width, int height,
                                     settings::Filter filter, int factor, int outW, int outH) {
	// Scale a native reference image independently, then let SDL sample it at the
	// output resolution. This distinguishes real CPU filtering from plain stretching.
	const Scaler *scaler = filter == settings::Filter::Xbrz ? &scaler_xbr
	                       : filter == settings::Filter::Scale ? &_internalScaler : nullptr;
	const int zoom = scaler ? factor : 1;
	std::vector<uint32_t> scaled(size_t(width) * height * zoom * zoom);
	if (scaler)
		scaler->scale(factor, scaled.data(), width * factor, native.data(), width, width, height);
	else
		scaled = native;
	auto *surface = SDL_CreateRGBSurfaceWithFormat(0, outW, outH, 32, SDL_PIXELFORMAT_RGB888);
	check(surface, "Reference surface unavailable");
	auto *renderer = SDL_CreateSoftwareRenderer(surface);
	check(renderer, "Reference renderer unavailable");
	auto *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STATIC,
	                                  width * zoom, height * zoom);
	check(texture, "Reference texture unavailable");
	SDL_UpdateTexture(texture, nullptr, scaled.data(), width * zoom * 4);
	SDL_SetTextureScaleMode(texture, filter == settings::Filter::Point ? SDL_ScaleModeNearest : SDL_ScaleModeLinear);
	SDL_RenderSetLogicalSize(renderer, width * factor, height * factor);
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, nullptr, nullptr);
	auto result = capture(renderer, outW, outH);
	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_FreeSurface(surface);
	return result;
}
static void scenery(int nativeW) {
	const int nativeH = nativeW * 224 / 256;
	auto platform = std::unique_ptr<SystemStub>(SystemStub_SDL_create());
	settings::Preferences p;
	p.fullscreen = false;
	p.filter = settings::Filter::Point;
	p.scale = 1;
	p.widescreen = kWidescreenAdjacentRooms;
	p.windowWidth = display::canvasWidth(nativeW, nativeH, p.widescreen);
	p.windowHeight = nativeH;
	platform->configurePreferences(p);
	auto scaler = ScalerParameters::defaults();
	strcpy(scaler.name, "point");
	scaler.factor = 1;
	platform->init(g_caption, nativeW, nativeH, false, p.widescreen, &scaler);
	auto *window = gameWindow();
	check(window, "Game window unavailable");
	auto *renderer = SDL_GetRenderer(window);
	std::string reason;
	auto apply = [&] { check(platform->applyPreferences(p, reason), reason.c_str()); };
	auto frame = [&] {
		platform->updateScreen(0);
		return capture(renderer, p.windowWidth, p.windowHeight);
	};
	std::vector<uint8_t> pattern(size_t(nativeW) * nativeH);
	for (int y = 0; y < nativeH; ++y)
		for (int x = 0; x < nativeW; ++x)
			pattern[y * nativeW + x] = (x / 9 + y / 7) % 3 == 0 || std::abs(x % 40 - y % 40) < 3;
	uint8_t palette[768]{};
	palette[3] = 252; palette[4] = 180; palette[5] = 100;
	platform->setPalette(palette, 256);
	for (int mode : {kWidescreenAdjacentRooms, kWidescreenAdjacentRoomsBlur,
	                 kWidescreenMirrorRoom, kWidescreenBlur, kWidescreenCDi}) {
		p.widescreen = mode;
		p.filter = settings::Filter::Point;
		p.scale = 1;
		p.windowWidth = display::canvasWidth(nativeW, nativeH, mode);
		p.windowHeight = nativeH;
		apply();
		platform->enableWidescreen(true);
		const int canvas = p.windowWidth;
		auto supply = [&] {
			if (mode == kWidescreenMirrorRoom)
				platform->copyWidescreenMirror(nativeW, nativeH, pattern.data());
			else if (mode == kWidescreenBlur)
				platform->copyWidescreenBlur(nativeW, nativeH, pattern.data());
			else if (mode == kWidescreenCDi)
				platform->copyWidescreenCDi(kWidescreenBorderCDiW, nativeH, pattern.data(), palette);
			else {
				platform->copyWidescreenLeft(nativeW, nativeH, pattern.data(), mode == kWidescreenAdjacentRooms);
				platform->copyWidescreenRight(nativeW, nativeH, pattern.data(), mode == kWidescreenAdjacentRooms);
			}
		};
		supply();
		const auto native = frame();
		check(std::count_if(native.begin(), native.end(), [](auto c) { return c != 0; }) > 100,
		      "Native scenery is empty");
		std::vector<uint32_t> point;
		for (auto filter : {settings::Filter::Point, settings::Filter::Linear,
		                   settings::Filter::Scale, settings::Filter::Xbrz}) {
			p.filter = filter;
			p.scale = 3;
			p.windowWidth = canvas * 3;
			p.windowHeight = nativeH * 3;
			apply(); // No new room upload: changing the filter must retain scenery.
			const auto actual = frame();
			if (mode != kWidescreenBlur) {
				const auto expected = reference(native, canvas, nativeH, filter, 3, p.windowWidth, p.windowHeight);
				const int margin = (canvas - nativeW) / 2 * 3;
				for (int y = 3; y < p.windowHeight - 3; ++y)
					for (int x = 3; x < margin - 6; ++x) {
						const size_t left = size_t(y) * p.windowWidth + x;
						const size_t right = size_t(y) * p.windowWidth + p.windowWidth - 1 - x;
						check(actual[left] == expected[left] && actual[right] == expected[right],
						      "Side scenery did not use the selected playfield filter");
					}
			}
			if (filter == settings::Filter::Point) point = actual;
			else if (filter == settings::Filter::Xbrz)
				check(actual != point, "xBRZ scenery is still nearest-neighbor stretched");
			check(frame() == actual, "Cached scenery changes between unchanged frames");
			supply();
			check(frame() == actual, "Live filter switch differs from a fresh scenery upload");
			SDL_Event reset{};
			reset.type = SDL_RENDER_DEVICE_RESET;
			SDL_PushEvent(&reset);
			platform->processEvents();
			check(frame() == actual, "Renderer reset lost scenery or changed its filter");
		}
		// Missing neighboring rooms and title/cutscene clears must clear filtered pixels too.
		platform->clearWidescreen();
		const auto blank = frame();
		check(std::all_of(blank.begin(), blank.end(), [](auto c) { return c == 0; }),
		      "Clearing scenery left stale filtered pixels");
	}
	// Exercise output fitting at Full HD and 4K, independently of CPU scaler size.
	for (auto size : {settings::Resolution{1920, 1080}, {3840, 2160}}) {
		p.windowWidth = size.width;
		p.windowHeight = size.height;
		for (auto preset : {settings::Presentation::ClassicPixels, settings::Presentation::WidescreenScenery,
		                    settings::Presentation::SmoothWidescreen}) {
			settings::applyPresentation(p, preset);
			apply();
			platform->copyRect(0, 0, nativeW, nativeH, pattern.data(), nativeW);
			if (platform->hasWidescreen()) {
				platform->copyWidescreenLeft(nativeW, nativeH, pattern.data());
				platform->copyWidescreenRight(nativeW, nativeH, pattern.data());
			}
			const auto pixels = frame();
			check(std::count_if(pixels.begin(), pixels.end(), [](auto c) { return c != 0; }) > 100000,
			      "Full HD/4K preset produced an empty frame");
			int width = 0, height = 0;
			SDL_GetRendererOutputSize(renderer, &width, &height);
			check(width == size.width && height == size.height, "Preset changed chosen output resolution");
			if (preset == settings::Presentation::ClassicPixels) {
				float x = 0, y = 0;
				SDL_RenderGetScale(renderer, &x, &y);
				check(x == std::min(width / nativeW, height / nativeH) && x == y,
				      "Classic preset does not use the largest whole native-pixel multiple");
			}
		}
	}
	platform->destroy();
}
int main() {
	try {
		scenery(256);
		scenery(512);
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
	std::cout << "Side filters, cached changes, renderer resets and 1080p/4K presets passed\n";
}
