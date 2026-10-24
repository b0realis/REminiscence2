/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <SDL.h>
#if !SDL_VERSION_ATLEAST(2, 0, 18)
#error REminiscence2 requires SDL 2.0.18 or newer for the settings renderer.
#endif
#include "resource.h"
#include <chrono>
#include <vector>
#include "scaler.h"
#include "screenshot.h"
#include "systemstub.h"
#include "util.h"
#include "settings_overlay.h"
#include <memory>

static const char *kIconBmp = "icon.bmp";

static const int kJoystickCommitValue = 3200;

static const uint32_t kPixelFormat = SDL_PIXELFORMAT_RGB888;

ScalerParameters ScalerParameters::defaults() {
	ScalerParameters params;
	params.type = kScalerTypeInternal;
	params.name[0] = 0;
	params.factor = _internalScaler.factorMin + (_internalScaler.factorMax - _internalScaler.factorMin) / 2;
	return params;
}

struct SystemStub_SDL : SystemStub {
	RuntimeOptions _options;
	SDL_AudioDeviceID _audioDevice = 0;
	int _outputSampleRate = 22050;
	SDL_Window *_window;
	SDL_Renderer *_renderer;
	SDL_Texture *_texture;
	int _texW, _texH;
	SDL_Rect _texRect;
	SDL_GameController *_controller;
	SDL_PixelFormat *_fmt;
	const char *_caption;
	uint32_t *_screenBuffer;
	bool _fullscreen;
	uint8_t _overscanColor;
	uint32_t _rgbPalette[256];
	uint32_t _shadowPalette[256];
	uint32_t _darkPalette[256];
	int _screenW, _screenH;
	SDL_Joystick *_joystick;
	bool _fadeOnUpdateScreen;
	void (*_audioCbProc)(void *, int16_t *, int);
	void *_audioCbData;
	int _screenshot;
	ScalerType _scalerType;
	int _scaleFactor;
	const Scaler *_scaler;
	void *_scalerSo;
	int _widescreenMode;
	SDL_Texture *_widescreenTexture;
	int _wideMargin;
	bool _enableWidescreen;
	settings::Preferences _preferences;
	std::unique_ptr<SettingsOverlay> _overlay;
	settings::Session *_settingsSession = nullptr;
	int _logicalW = 0, _logicalH = 0;
	bool _gameplayActive = false, _pointerVisible = false;
	uint32_t _lastPointer = 0;
	SDL_JoystickID _inputDevice = -1;
	bool _frameDirty = true;
	std::vector<uint32_t> _rgbScratch, _blurScratch, _widePixels;
	int _wideWidth = 0;

	virtual ~SystemStub_SDL() {}
	void configure(const RuntimeOptions &options) override { _options = options; }
	void configurePreferences(const settings::Preferences &preferences) override { _preferences = preferences; }
	void readPreferences(settings::Preferences &preferences) const override;
	bool applyPreferences(const settings::Preferences &preferences, std::string &reason) override;
	void showSettings(settings::Session &session) override;
	void setGameplayActive(bool active) override { _gameplayActive = active; }
	void init(const char *title, int w, int h, bool fullscreen, int widescreenMode, const ScalerParameters *scalerParameters) override;
	void destroy() override;
	virtual bool hasWidescreen() const;
	void setScreenSize(int w, int h) override;
	void setPalette(const uint8_t *pal, int n) override;
	void getPalette(uint8_t *pal, int n) override;
	void setPaletteEntry(int i, const Color *c) override;
	void getPaletteEntry(int i, Color *c) override;
	void setOverscanColor(int i) override;
	void copyRect(int x, int y, int w, int h, const uint8_t *buf, int pitch) override;
	void copyRectRgb24(int x, int y, int w, int h, const uint8_t *rgb) override;
	void zoomRect(int x, int y, int w, int h) override;
	void copyWidescreenLeft(int w, int h, const uint8_t *buf, bool dark = true) override;
	void copyWidescreenRight(int w, int h, const uint8_t *buf, bool dark = true) override;
	void copyWidescreenMirror(int w, int h, const uint8_t *buf) override;
	void copyWidescreenBlur(int w, int h, const uint8_t *buf) override;
	void copyWidescreenCDi(int w, int h, const uint8_t *buf, const uint8_t *pal) override;
	void clearWidescreen() override;
	void enableWidescreen(bool enable) override;
	void fadeScreen() override;
	void updateScreen(int shakeOffset) override;
	void processEvents() override;
	void sleep(int duration) override;
	uint32_t getTimeStamp() override;
	void startAudio(AudioCallback callback, void *param) override;
	void stopAudio() override;
	uint32_t getOutputSampleRate() override;
	void lockAudio() override;
	void unlockAudio() override;

	void setPaletteColor(int color, int r, int g, int b);
	void processEvent(const SDL_Event &ev, bool &paused);
	void openInputDevice();
	void closeInputDevice();
	void restoreTextures();
	void updateWide(const SDL_Rect *area, const uint32_t *pixels, int pitch);
	void prepareGraphics();
	void cleanupGraphics();
	void changeGraphics(bool fullscreen, int scaleFactor);
	void setScaler(const ScalerParameters *parameters);
	void changeScaler(int scalerNum);
	void drawRect(int x, int y, int w, int h, uint8_t color);
	bool drawGameFrame(int shakeOffset);
	SDL_Keycode remapKey(const SDL_KeyboardEvent &event) const;
};

SystemStub *SystemStub_SDL_create() {
	return new SystemStub_SDL();
}

void SystemStub_SDL::init(const char *title, int w, int h, bool fullscreen, int widescreenMode, const ScalerParameters *scalerParameters) {
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) <
	    0) {
		error("Unable to initialize SDL: %s", SDL_GetError());
	}
	_outputSampleRate = _options.outputSampleRate;
	SDL_ShowCursor(SDL_DISABLE);
	_caption = title;
	memset(&_pi, 0, sizeof(_pi));
	_window = 0;
	_renderer = 0;
	_texture = 0;
	_fmt = SDL_AllocFormat(kPixelFormat);
	_screenBuffer = 0;
	_fadeOnUpdateScreen = false;
	_fullscreen = fullscreen;
	_scalerType = kScalerTypeInternal;
	_scaleFactor = CLIP(scalerParameters->factor, _internalScaler.factorMin, _internalScaler.factorMax);
	_scaler = &_internalScaler;
	_scalerSo = 0;
	if (scalerParameters->name[0]) {
		setScaler(scalerParameters);
	}
	memset(_rgbPalette, 0, sizeof(_rgbPalette));
	memset(_shadowPalette, 0, sizeof(_shadowPalette));
	memset(_darkPalette, 0, sizeof(_darkPalette));
	_screenW = _screenH = 0;
	_widescreenMode = widescreenMode;
	_widescreenTexture = 0;
	_wideMargin = 0;
	_enableWidescreen = false;
	setScreenSize(w, h);
	_joystick = 0;
	_controller = 0;
	SDL_GameControllerAddMapping(Resource::_controllerMapping);
	openInputDevice();
}

void SystemStub_SDL::closeInputDevice() {
	if (_controller) SDL_GameControllerClose(_controller);
	if (_joystick) SDL_JoystickClose(_joystick);
	_controller = nullptr; _joystick = nullptr; _inputDevice = -1;
	_pi.dirMask = 0;
	_pi.enter = _pi.space = _pi.shift = _pi.backspace = _pi.escape = false;
}
void SystemStub_SDL::openInputDevice() {
	if (_inputDevice >= 0) return;
	// Prefer SDL-mapped controllers; do not process their duplicate raw events.
	for (int pass = 0; pass < 2; ++pass) for (int i = 0; i < SDL_NumJoysticks(); ++i) {
		if ((SDL_IsGameController(i) == SDL_TRUE) != (pass == 0)) continue;
		if (pass == 0) {
			_controller = SDL_GameControllerOpen(i);
			if (_controller) _inputDevice = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(_controller));
		} else {
			_joystick = SDL_JoystickOpen(i);
			if (_joystick) _inputDevice = SDL_JoystickInstanceID(_joystick);
		}
		if (_inputDevice >= 0) return;
	}
}

void SystemStub_SDL::destroy() {
	stopAudio();
	cleanupGraphics();
	_overlay.reset();
	if (_screenBuffer) {
		free(_screenBuffer);
		_screenBuffer = 0;
	}
	if (_fmt) {
		SDL_FreeFormat(_fmt);
		_fmt = 0;
	}
	if (_scalerSo) {
		SDL_UnloadObject(_scalerSo);
		_scalerSo = 0;
	}
	closeInputDevice();
	SDL_Quit();
}

bool SystemStub_SDL::hasWidescreen() const {
	return _widescreenMode != kWidescreenNone;
}

void SystemStub_SDL::setScreenSize(int w, int h) {
	if (_screenW == w && _screenH == h) {
		return;
	}
	cleanupGraphics();
	if (_screenBuffer) {
		free(_screenBuffer);
		_screenBuffer = 0;
	}
	const int screenBufferSize = w * h * sizeof(uint32_t);
	_screenBuffer = (uint32_t *)calloc(1, screenBufferSize);
	if (!_screenBuffer) {
		error("SystemStub_SDL::setScreenSize() Unable to allocate offscreen buffer, w=%d, h=%d", w, h);
	}
	_screenW = w;
	_screenH = h;
	prepareGraphics();
}

void SystemStub_SDL::setPaletteColor(int color, int r, int g, int b) {
	_rgbPalette[color] = SDL_MapRGB(_fmt, r, g, b);
	_shadowPalette[color] = SDL_MapRGB(_fmt, r / 3, g / 3, b / 3);
	_darkPalette[color] = SDL_MapRGB(_fmt, r / 4, g / 4, b / 4);
}

void SystemStub_SDL::setPalette(const uint8_t *pal, int n) {
	assert(n <= 256);
	for (int i = 0; i < n; ++i) {
		setPaletteColor(i, pal[0], pal[1], pal[2]);
		pal += 3;
	}
}

void SystemStub_SDL::getPalette(uint8_t *pal, int n) {
	assert(n <= 256);
	for (int i = 0; i < n; ++i) {
		SDL_GetRGB(_rgbPalette[i], _fmt, &pal[0], &pal[1], &pal[2]);
		pal += 3;
	}
}

void SystemStub_SDL::setPaletteEntry(int i, const Color *c) {
	setPaletteColor(i, c->r, c->g, c->b);
}

void SystemStub_SDL::getPaletteEntry(int i, Color *c) {
	SDL_GetRGB(_rgbPalette[i], _fmt, &c->r, &c->g, &c->b);
}

void SystemStub_SDL::setOverscanColor(int i) {
	_overscanColor = i;
}

void SystemStub_SDL::copyRect(int x, int y, int w, int h, const uint8_t *buf, int pitch) {
	_frameDirty = true;
	if (x < 0) {
		x = 0;
	} else if (x >= _screenW) {
		return;
	}
	if (y < 0) {
		y = 0;
	} else if (y >= _screenH) {
		return;
	}
	if (x + w > _screenW) {
		w = _screenW - x;
	}
	if (y + h > _screenH) {
		h = _screenH - y;
	}

	uint32_t *p = _screenBuffer + y * _screenW + x;
	buf += y * pitch + x;

	for (int j = 0; j < h; ++j) {
		for (int i = 0; i < w; ++i) {
			p[i] = _rgbPalette[buf[i]];
		}
		p += _screenW;
		buf += pitch;
	}

	if (_pi.dbgMask & PlayerInput::DF_DBLOCKS) {
		drawRect(x, y, w, h, 0xE7);
	}
}

void SystemStub_SDL::copyRectRgb24(int x, int y, int w, int h, const uint8_t *rgb) {
	_frameDirty = true;
	assert(x >= 0 && x + w <= _screenW && y >= 0 && y + h <= _screenH);
	uint32_t *p = _screenBuffer + y * _screenW + x;

	for (int j = 0; j < h; ++j) {
		for (int i = 0; i < w; ++i) {
			p[i] = SDL_MapRGB(_fmt, rgb[0], rgb[1], rgb[2]); rgb += 3;
		}
		p += _screenW;
	}

	if (_pi.dbgMask & PlayerInput::DF_DBLOCKS) {
		drawRect(x, y, w, h, 0xE7);
	}
}

void SystemStub_SDL::zoomRect(int x, int y, int w, int h) {
	if (_pi.dbgMask & PlayerInput::DF_DBLOCKS) {
		drawRect(x, y, w, h, 0xE7);
	}
	_texRect.x = x * _texW / _screenW;
	_texRect.y = y * _texH / _screenH;
	_texRect.w = w * _texW / _screenW;
	_texRect.h = h * _texH / _screenH;
}

static void clearTexture(SDL_Texture *texture, int h, SDL_PixelFormat *fmt) {
	void *dst = 0;
	int pitch = 0;
	if (SDL_LockTexture(texture, 0, &dst, &pitch) == 0) {
		assert((pitch & 3) == 0);
		const uint32_t color = SDL_MapRGB(fmt, 0, 0, 0);
		for (uint32_t i = 0; i < h * pitch / sizeof(uint32_t); ++i) {
			((uint32_t *)dst)[i] = color;
		}
		SDL_UnlockTexture(texture);
	}
}

static void blur_h(int radius, const uint32_t *src, int srcPitch, int w, int h, const SDL_PixelFormat *fmt, uint32_t *dst, int dstPitch) {

	const int count = 2 * radius + 1;

	for (int y = 0; y < h; ++y) {

		uint32_t r = 0;
		uint32_t g = 0;
		uint32_t b = 0;

		uint32_t color;

		for (int x = -radius; x <= radius; ++x) {
			color = src[MAX(x, 0)];
			r += (color & fmt->Rmask) >> fmt->Rshift;
			g += (color & fmt->Gmask) >> fmt->Gshift;
			b += (color & fmt->Bmask) >> fmt->Bshift;
		}
		dst[0] = ((r / count) << fmt->Rshift) | ((g / count) << fmt->Gshift) | ((b / count) << fmt->Bshift);

		for (int x = 1; x < w; ++x) {
			color = src[MIN(x + radius, w - 1)];
			r += (color & fmt->Rmask) >> fmt->Rshift;
			g += (color & fmt->Gmask) >> fmt->Gshift;
			b += (color & fmt->Bmask) >> fmt->Bshift;

			color = src[MAX(x - radius - 1, 0)];
			r -= (color & fmt->Rmask) >> fmt->Rshift;
			g -= (color & fmt->Gmask) >> fmt->Gshift;
			b -= (color & fmt->Bmask) >> fmt->Bshift;

			dst[x] = ((r / count) << fmt->Rshift) | ((g / count) << fmt->Gshift) | ((b / count) << fmt->Bshift);
		}

		src += srcPitch;
		dst += dstPitch;
	}
}

static void blur_v(int radius, const uint32_t *src, int srcPitch, int w, int h, const SDL_PixelFormat *fmt, uint32_t *dst, int dstPitch) {

	const int count = 2 * radius + 1;

	for (int x = 0; x < w; ++x) {

		uint32_t r = 0;
		uint32_t g = 0;
		uint32_t b = 0;

		uint32_t color;

		for (int y = -radius; y <= radius; ++y) {
			color = src[MAX(y, 0) * srcPitch];
			r += (color & fmt->Rmask) >> fmt->Rshift;
			g += (color & fmt->Gmask) >> fmt->Gshift;
			b += (color & fmt->Bmask) >> fmt->Bshift;
		}
		dst[0] = ((r / count) << fmt->Rshift) | ((g / count) << fmt->Gshift) | ((b / count) << fmt->Bshift);

		for (int y = 1; y < h; ++y) {
			color = src[MIN(y + radius, h - 1) * srcPitch];
			r += (color & fmt->Rmask) >> fmt->Rshift;
			g += (color & fmt->Gmask) >> fmt->Gshift;
			b += (color & fmt->Bmask) >> fmt->Bshift;

			color = src[MAX(y - radius - 1, 0) * srcPitch];
			r -= (color & fmt->Rmask) >> fmt->Rshift;
			g -= (color & fmt->Gmask) >> fmt->Gshift;
			b -= (color & fmt->Bmask) >> fmt->Bshift;

			dst[y * dstPitch] = ((r / count) << fmt->Rshift) | ((g / count) << fmt->Gshift) | ((b / count) << fmt->Bshift);
		}

		++src;
		++dst;
	}
}

void SystemStub_SDL::copyWidescreenLeft(int w, int h, const uint8_t *buf, bool dark) {
	assert(w >= _wideMargin);
	_rgbScratch.resize(size_t(w) * h);
	uint32_t *rgb = _rgbScratch.data();
	if (rgb) {
		if (buf) {
			for (int i = 0; i < w * h; ++i) {
				rgb[i] = dark ? _darkPalette[buf[i]] : _shadowPalette[buf[i]];
			}
		} else {
			const uint32_t color = SDL_MapRGB(_fmt, 0, 0, 0);
			for (int i = 0; i < w * h; ++i) {
				rgb[i] = color;
			}
		}

		if (!dark) {
			_blurScratch.resize(size_t(w) * h);
			uint32_t *tmp = _blurScratch.data();

			if (rgb && tmp) {
				static const int radius = 2;
				blur_h(radius, rgb, w, w, h, _fmt, tmp, w);
				blur_v(radius, tmp, w, w, h, _fmt, rgb, w);
			}

		}

		const int xOffset = w - _wideMargin;
		SDL_Rect r;
		r.x = 0;
		r.y = 0;
		r.w = _wideMargin;
		r.h = h;
		updateWide(&r, rgb + xOffset, w * sizeof(uint32_t));
	}
}

void SystemStub_SDL::copyWidescreenRight(int w, int h, const uint8_t *buf, bool dark) {
	assert(w >= _wideMargin);
	_rgbScratch.resize(size_t(w) * h);
	uint32_t *rgb = _rgbScratch.data();
	if (rgb) {
		if (buf) {
			for (int i = 0; i < w * h; ++i) {
				rgb[i] = dark ? _darkPalette[buf[i]] : _shadowPalette[buf[i]];
			}
		} else {
			const uint32_t color = SDL_MapRGB(_fmt, 0, 0, 0);
			for (int i = 0; i < w * h; ++i) {
				rgb[i] = color;
			}
		}

		if (!dark) {
			_blurScratch.resize(size_t(w) * h);
			uint32_t *tmp = _blurScratch.data();

			if (rgb && tmp) {
				static const int radius = 2;
				blur_h(radius, rgb, w, w, h, _fmt, tmp, w);
				blur_v(radius, tmp, w, w, h, _fmt, rgb, w);
			}

		}

		const int xOffset = 0;
		SDL_Rect r;
		r.x = _wideMargin + _screenW;
		r.y = 0;
		r.w = _wideMargin;
		r.h = h;
		updateWide(&r, rgb + xOffset, w * sizeof(uint32_t));
	}
}

void SystemStub_SDL::copyWidescreenMirror(int w, int h, const uint8_t *buf) {
	assert(w >= _wideMargin);
	_rgbScratch.resize(size_t(w) * h);
	uint32_t *rgb = _rgbScratch.data();
	if (rgb) {
		for (int i = 0; i < w * h; ++i) {
			rgb[i] = _darkPalette[buf[i]];
		}
		void *dst = 0;
		int pitch = 0;
		if (SDL_LockTexture(_widescreenTexture, 0, &dst, &pitch) == 0) {
			assert((pitch & 3) == 0);
			uint32_t *p = (uint32_t *)dst;
			for (int y = 0; y < h; ++y) {
				for (int x = 0; x < _wideMargin; ++x) {
					// left side
					const int xLeft = _wideMargin - 1 - x;
					p[x] = rgb[y * w + xLeft];
					// right side
					const int xRight = w - 1 - x;
					p[_wideMargin + _screenW + x] = rgb[y * w + xRight];
				}
				p += pitch / sizeof(uint32_t);
			}
			updateWide(nullptr, static_cast<uint32_t *>(dst), pitch);
			SDL_UnlockTexture(_widescreenTexture);
		}
	}
}

void SystemStub_SDL::copyWidescreenBlur(int w, int h, const uint8_t *buf) {
	assert(w == _screenW && h == _screenH);
	void *ptr = 0;
	int pitch = 0;
	if (SDL_LockTexture(_widescreenTexture, 0, &ptr, &pitch) == 0) {
		assert((pitch & 3) == 0);

		_rgbScratch.resize(size_t(w) * h);
		uint32_t *src = _rgbScratch.data();
		_blurScratch.resize(size_t(w) * h);
			uint32_t *tmp = _blurScratch.data();
		uint32_t *dst = (uint32_t *)ptr;

		if (src && tmp) {
			for (int i = 0; i < w * h; ++i) {
				src[i] = _rgbPalette[buf[i]];
			}
			static const int radius = 8;
			blur_h(radius, src, w, w, h, _fmt, tmp, w);
			blur_v(radius, tmp, w, w, h, _fmt, dst, pitch / sizeof(uint32_t));
		}


		updateWide(nullptr, static_cast<uint32_t *>(ptr), pitch);
		SDL_UnlockTexture(_widescreenTexture);
	}
}

void SystemStub_SDL::copyWidescreenCDi(int w, int h, const uint8_t *buf, const uint8_t *pal) {
	_rgbScratch.resize(size_t(w) * h);
	uint32_t *rgb = _rgbScratch.data();
	if (rgb) {
		for (int i = 0; i < w * h; ++i) {
			const uint8_t *p = pal + buf[i] * 3;
			const uint32_t color = SDL_MapRGB(_fmt, p[0], p[1], p[2]);
			rgb[i] = color;
		}
		SDL_Rect r;
		r.y = 0;
		r.w = w;
		r.h = h;
		// left border
		r.x = 0;
		updateWide(&r, rgb, w * sizeof(uint32_t));
		// right border
		r.x = _screenW + w;
		updateWide(&r, rgb, w * sizeof(uint32_t));
	}
}

void SystemStub_SDL::updateWide(const SDL_Rect *area, const uint32_t *pixels, int pitch) {
	if (!_widescreenTexture) return;
	int width, height;
	SDL_QueryTexture(_widescreenTexture, nullptr, nullptr, &width, &height);
	_wideWidth = width;
	_widePixels.resize(size_t(width) * height);
	const SDL_Rect r = area ? *area : SDL_Rect{0, 0, width, height};
	for (int y = 0; y < r.h; ++y)
		memcpy(_widePixels.data() + (r.y + y) * width + r.x,
		       reinterpret_cast<const uint8_t *>(pixels) + size_t(y) * pitch, r.w * sizeof(uint32_t));
	if (area) SDL_UpdateTexture(_widescreenTexture, area, pixels, pitch);
}
void SystemStub_SDL::restoreTextures() {
	SDL_DestroyTexture(_texture);
	_texture = SDL_CreateTexture(_renderer, kPixelFormat, SDL_TEXTUREACCESS_STREAMING, _texW, _texH);
	if (!_texture) error("Cannot restore game texture: %s", SDL_GetError());
	_frameDirty = true;
	if (_widescreenTexture) {
		SDL_DestroyTexture(_widescreenTexture);
		_widescreenTexture = SDL_CreateTexture(_renderer, kPixelFormat, SDL_TEXTUREACCESS_STREAMING, _wideWidth, _screenH);
		if (!_widescreenTexture) error("Cannot restore side texture: %s", SDL_GetError());
		if (!_widePixels.empty()) SDL_UpdateTexture(_widescreenTexture, nullptr, _widePixels.data(), _wideWidth * sizeof(uint32_t));
	}
}

void SystemStub_SDL::clearWidescreen() {
	std::fill(_widePixels.begin(), _widePixels.end(), 0);
	clearTexture(_widescreenTexture, _screenH, _fmt);
}

void SystemStub_SDL::enableWidescreen(bool enable) {
	_enableWidescreen = enable;
}

void SystemStub_SDL::fadeScreen() {
	_fadeOnUpdateScreen = true;
}

bool SystemStub_SDL::drawGameFrame(int shakeOffset) {
	if (_frameDirty) {
	if (_texW != _screenW || _texH != _screenH) {
		void *dst = 0;
		int pitch = 0;
		if (SDL_LockTexture(_texture, 0, &dst, &pitch) == 0) {
			assert((pitch & 3) == 0);
			_scaler->scale(_scaleFactor, (uint32_t *)dst, pitch / sizeof(uint32_t), _screenBuffer, _screenW, _screenW, _screenH);
			SDL_UnlockTexture(_texture);
		}
	} else {
		SDL_UpdateTexture(_texture, 0, _screenBuffer, _screenW * sizeof(uint32_t));
	}
	_frameDirty = false;
	}
	SDL_RenderClear(_renderer);
	if (_widescreenMode != kWidescreenNone) {
		if (_enableWidescreen) {
			// borders / background screen
			SDL_RenderCopy(_renderer, _widescreenTexture, 0, 0);
		}
		// game screen
		SDL_Rect r;
		r.y = shakeOffset * _scaleFactor;
		SDL_RenderGetLogicalSize(_renderer, &r.w, &r.h);
		if (_scalerType == kScalerTypeLinear || _scalerType == kScalerTypePoint) {
			r.x = (r.w - _texW * _scaleFactor) / 2;
			r.w = _texW * _scaleFactor;
		} else {
			r.x = (r.w - _texW) / 2;
			r.w = _texW;
		}
		SDL_RenderCopy(_renderer, _texture, &_texRect, &r);
	} else {
		if (_fadeOnUpdateScreen) {
			SDL_SetRenderDrawBlendMode(_renderer, SDL_BLENDMODE_BLEND);
			SDL_Rect r;
			r.x = r.y = 0;
			SDL_RenderGetLogicalSize(_renderer, &r.w, &r.h);
			for (int i = 1; i <= 16; ++i) {
				SDL_SetRenderDrawColor(_renderer, 0, 0, 0, 256 - i * 16);
				SDL_RenderCopy(_renderer, _texture, 0, 0);
				SDL_RenderFillRect(_renderer, &r);
				SDL_RenderPresent(_renderer);
				SDL_Delay(30);
			}
			_fadeOnUpdateScreen = false;
			SDL_SetRenderDrawBlendMode(_renderer, SDL_BLENDMODE_NONE);
			return false;
		}
		SDL_Rect r;
		r.x = 0;
		r.y = shakeOffset * _scaleFactor;
		SDL_RenderGetLogicalSize(_renderer, &r.w, &r.h);
		SDL_RenderCopy(_renderer, _texture, &_texRect, &r);
	}
	_texRect.x = 0;
	_texRect.y = 0;
	_texRect.w = _texW;
	_texRect.h = _texH;
	return true;
}

void SystemStub_SDL::updateScreen(int shakeOffset) {
	if (!drawGameFrame(shakeOffset)) return;
	if ((_preferences.showFps || (_gameplayActive && _preferences.showMenuButton)) && _overlay && !_settingsSession) {
		SDL_RenderSetLogicalSize(_renderer, 0, 0);
		_overlay->beginFrame(_preferences);
		if (_overlay->drawHud(_preferences, _gameplayActive)) _pi.escape = true;
		_overlay->render();
	}
	SDL_RenderPresent(_renderer);
	SDL_RenderSetLogicalSize(_renderer, _logicalW, _logicalH);
	if (!_settingsSession && _pointerVisible && SDL_GetTicks() - _lastPointer > 2000) {
		SDL_ShowCursor(SDL_DISABLE);
		_pointerVisible = false;
	}
}

void SystemStub_SDL::processEvents() {
	bool paused = false;
	while (true) {
		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			processEvent(ev, paused);
			if (_pi.quit) {
				return;
			}
		}
		if (!paused) {
			break;
		}
		SDL_Delay(100);
	}
}

// only used for the protection codes and level passwords
static void setAsciiChar(PlayerInput &pi, const SDL_Keysym *key) {
	if (key->sym >= SDLK_0 && key->sym <= SDLK_9) {
		pi.lastChar = '0' + key->sym - SDLK_0;
	} else if (key->sym >= SDLK_a && key->sym <= SDLK_z) {
		pi.lastChar = 'A' + key->sym - SDLK_a;
	} else if (key->scancode == SDL_SCANCODE_0) {
		pi.lastChar = '0';
	} else if (key->scancode >= SDL_SCANCODE_1 && key->scancode <= SDL_SCANCODE_9) {
		pi.lastChar = '1' + key->scancode - SDL_SCANCODE_1;
	} else if (key->sym == SDLK_SPACE || key->sym == SDLK_KP_SPACE) {
		pi.lastChar = ' ';
	} else {
		pi.lastChar = 0;
	}
}

void SystemStub_SDL::processEvent(const SDL_Event &raw, bool &paused) {
	if (raw.type == SDL_JOYDEVICEADDED || raw.type == SDL_CONTROLLERDEVICEADDED) { openInputDevice(); return; }
	if (raw.type == SDL_JOYDEVICEREMOVED || raw.type == SDL_CONTROLLERDEVICEREMOVED) {
		if (raw.jdevice.which == _inputDevice) { closeInputDevice(); openInputDevice(); }
		return;
	}
	if (raw.type == SDL_RENDER_DEVICE_RESET || raw.type == SDL_RENDER_TARGETS_RESET) { restoreTextures(); return; }
	if (raw.type == SDL_APP_WILLENTERBACKGROUND || raw.type == SDL_APP_DIDENTERFOREGROUND) {
		paused = raw.type == SDL_APP_WILLENTERBACKGROUND;
		_pi.dirMask = 0; _pi.enter = _pi.shift = _pi.space = _pi.backspace = false;
		if (_audioDevice) SDL_PauseAudioDevice(_audioDevice, paused);
		return;
	}

	if (_settingsSession && _overlay) {
		_overlay->processEvent(raw, _settingsSession);
		// Window/focus/quit events still belong to the platform. Every gameplay
		// input is otherwise consumed by the overlay, including Ctrl+S/L.
		if (raw.type != SDL_QUIT && raw.type != SDL_WINDOWEVENT) return;
	}
	if (!_settingsSession && _gameplayActive && _preferences.showMenuButton && _overlay &&
	    (raw.type == SDL_MOUSEMOTION || raw.type == SDL_MOUSEBUTTONDOWN || raw.type == SDL_MOUSEBUTTONUP)) {
		SDL_Event pointer = raw;
		// SDL maps game mouse events to renderer logical coordinates. The HUD is
		// drawn in window coordinates, including letterboxing and HiDPI scaling.
		int x, y;
		if (raw.type == SDL_MOUSEMOTION) {
			SDL_RenderLogicalToWindow(_renderer, raw.motion.x, raw.motion.y, &x, &y);
			pointer.motion.x = x; pointer.motion.y = y;
		} else {
			SDL_RenderLogicalToWindow(_renderer, raw.button.x, raw.button.y, &x, &y);
			pointer.button.x = x; pointer.button.y = y;
		}
		_overlay->processEvent(pointer, nullptr);
		if (raw.type == SDL_MOUSEBUTTONUP && raw.button.button == SDL_BUTTON_LEFT &&
		    _overlay->shortcutContains(x, y)) _pi.escape = true;
		_lastPointer = SDL_GetTicks();
		_pointerVisible = true;
		SDL_ShowCursor(SDL_ENABLE);
		return;
	}
	SDL_Event ev = raw;
	if ((ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP) && !(ev.key.keysym.mod & (KMOD_CTRL | KMOD_ALT))) {
		ev.key.keysym.sym = remapKey(raw.key);
	}
	switch (ev.type) {
	case SDL_QUIT:
		_pi.quit = true;
		break;
	case SDL_WINDOWEVENT:
		if (_window && ev.window.windowID && ev.window.windowID != SDL_GetWindowID(_window)) break;
		switch (ev.window.event) {
		case SDL_WINDOWEVENT_FOCUS_GAINED:
		case SDL_WINDOWEVENT_FOCUS_LOST:
			paused = (ev.window.event == SDL_WINDOWEVENT_FOCUS_LOST) && _preferences.pauseOnFocusLoss;
			if (_audioDevice) {
				SDL_PauseAudioDevice(_audioDevice, paused);
			}
			if (ev.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
				// Lost key-up events must not leave Conrad running on return.
				_pi.dirMask = 0;
				_pi.shift = _pi.enter = _pi.space = _pi.backspace = false;
			}
			break;
		}
		break;
	case SDL_JOYHATMOTION:
		if (_joystick && ev.jhat.which == _inputDevice) {
			_pi.dirMask = 0;
			if (ev.jhat.value & SDL_HAT_UP) {
				_pi.dirMask |= PlayerInput::DIR_UP;
			}
			if (ev.jhat.value & SDL_HAT_DOWN) {
				_pi.dirMask |= PlayerInput::DIR_DOWN;
			}
			if (ev.jhat.value & SDL_HAT_LEFT) {
				_pi.dirMask |= PlayerInput::DIR_LEFT;
			}
			if (ev.jhat.value & SDL_HAT_RIGHT) {
				_pi.dirMask |= PlayerInput::DIR_RIGHT;
			}
		}
		break;
	case SDL_JOYAXISMOTION:
		if (_joystick && ev.jaxis.which == _inputDevice) {
			switch (ev.jaxis.axis) {
			case 0:
				_pi.dirMask &= ~(PlayerInput::DIR_RIGHT | PlayerInput::DIR_LEFT);
				if (ev.jaxis.value > kJoystickCommitValue) {
					_pi.dirMask |= PlayerInput::DIR_RIGHT;
				} else if (ev.jaxis.value < -kJoystickCommitValue) {
					_pi.dirMask |= PlayerInput::DIR_LEFT;
				}
				break;
			case 1:
				_pi.dirMask &= ~(PlayerInput::DIR_UP | PlayerInput::DIR_DOWN);
				if (ev.jaxis.value > kJoystickCommitValue) {
					_pi.dirMask |= PlayerInput::DIR_DOWN;
				} else if (ev.jaxis.value < -kJoystickCommitValue) {
					_pi.dirMask |= PlayerInput::DIR_UP;
				}
				break;
			}
		}
		break;
	case SDL_JOYBUTTONDOWN:
	case SDL_JOYBUTTONUP:
		if (_joystick && ev.jbutton.which == _inputDevice) {
			const bool pressed = (ev.jbutton.state == SDL_PRESSED);
			switch (ev.jbutton.button) {
			case 0:
				_pi.space = pressed;
				break;
			case 1:
				_pi.shift = pressed;
				break;
			case 2:
				_pi.enter = pressed;
				break;
			case 3:
				_pi.backspace = pressed;
				break;
			case 9:
				_pi.escape = pressed;
			}
		}
		break;
	case SDL_CONTROLLERAXISMOTION:
		if (_controller && ev.caxis.which == _inputDevice) {
			switch (ev.caxis.axis) {
			case SDL_CONTROLLER_AXIS_LEFTX:
			case SDL_CONTROLLER_AXIS_RIGHTX:
				if (ev.caxis.value < -kJoystickCommitValue) {
					_pi.dirMask |= PlayerInput::DIR_LEFT;
				} else {
					_pi.dirMask &= ~PlayerInput::DIR_LEFT;
				}
				if (ev.caxis.value > kJoystickCommitValue) {
					_pi.dirMask |= PlayerInput::DIR_RIGHT;
				} else {
					_pi.dirMask &= ~PlayerInput::DIR_RIGHT;
				}
				break;
			case SDL_CONTROLLER_AXIS_LEFTY:
			case SDL_CONTROLLER_AXIS_RIGHTY:
				if (ev.caxis.value < -kJoystickCommitValue) {
					_pi.dirMask |= PlayerInput::DIR_UP;
				} else {
					_pi.dirMask &= ~PlayerInput::DIR_UP;
				}
				if (ev.caxis.value > kJoystickCommitValue) {
					_pi.dirMask |= PlayerInput::DIR_DOWN;
				} else {
					_pi.dirMask &= ~PlayerInput::DIR_DOWN;
				}
				break;
			}
		}
		break;
	case SDL_CONTROLLERBUTTONDOWN:
	case SDL_CONTROLLERBUTTONUP:
		if (_controller && ev.cbutton.which == _inputDevice) {
			const bool pressed = (ev.cbutton.state == SDL_PRESSED);
			switch (ev.cbutton.button) {
			case SDL_CONTROLLER_BUTTON_A:
				_pi.enter = pressed;
				break;
			case SDL_CONTROLLER_BUTTON_B:
				_pi.space = pressed;
				break;
			case SDL_CONTROLLER_BUTTON_X:
				_pi.shift = pressed;
				break;
			case SDL_CONTROLLER_BUTTON_Y:
				_pi.backspace = pressed;
				break;
			case SDL_CONTROLLER_BUTTON_BACK:
			case SDL_CONTROLLER_BUTTON_START:
				_pi.escape = pressed;
				break;
			case SDL_CONTROLLER_BUTTON_DPAD_UP:
				if (pressed) {
					_pi.dirMask |= PlayerInput::DIR_UP;
				} else {
					_pi.dirMask &= ~PlayerInput::DIR_UP;
				}
				break;
			case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
				if (pressed) {
					_pi.dirMask |= PlayerInput::DIR_DOWN;
				} else {
					_pi.dirMask &= ~PlayerInput::DIR_DOWN;
				}
				break;
			case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
				if (pressed) {
					_pi.dirMask |= PlayerInput::DIR_LEFT;
				} else {
					_pi.dirMask &= ~PlayerInput::DIR_LEFT;
				}
				break;
			case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
				if (pressed) {
					_pi.dirMask |= PlayerInput::DIR_RIGHT;
				} else {
					_pi.dirMask &= ~PlayerInput::DIR_RIGHT;
				}
				break;
			}
		}
		break;
	case SDL_KEYUP:
		if (ev.key.keysym.mod & KMOD_ALT) {
			switch (ev.key.keysym.sym) {
			case SDLK_RETURN:
				changeGraphics(!_fullscreen, _scaleFactor);
				break;
			case SDLK_KP_PLUS:
			case SDLK_PAGEUP:
				if (_scalerType == kScalerTypeInternal || _scalerType == kScalerTypeExternal) {
					changeGraphics(_fullscreen, _scaleFactor + 1);
				}
				break;
			case SDLK_KP_MINUS:
			case SDLK_PAGEDOWN:
				if (_scalerType == kScalerTypeInternal || _scalerType == kScalerTypeExternal) {
					changeGraphics(_fullscreen, _scaleFactor - 1);
				}
				break;
			case SDLK_s: {
					const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
					    std::chrono::system_clock::now().time_since_epoch()).count();
					char name[64];
					snprintf(name, sizeof(name), "screenshot-%lld.bmp", static_cast<long long>(timestamp));
					saveBMP(name, (const uint8_t *)_screenBuffer, 0, _screenW, _screenH);
				    info("Written '%s'", name);
			    }
				break;
			case SDLK_x:
				_pi.quit = true;
				break;
			}
			break;
		} else if (ev.key.keysym.mod & KMOD_CTRL) {
			switch (ev.key.keysym.sym) {
			case SDLK_f:
				_pi.dbgMask ^= PlayerInput::DF_FASTMODE;
				break;
			case SDLK_b:
				_pi.dbgMask ^= PlayerInput::DF_DBLOCKS;
				break;
			case SDLK_i:
				_pi.dbgMask ^= PlayerInput::DF_SETLIFE;
				break;
			case SDLK_s:
				_pi.save = true;
				break;
			case SDLK_l:
				_pi.load = true;
				break;
			case SDLK_r:
				_pi.rewind = true;
				break;
			case SDLK_g:
				_pi.dbgMask ^= PlayerInput::DF_AUTOZOOM;
				info("Auto zoom %s",
				     (_pi.dbgMask & PlayerInput::DF_AUTOZOOM) ? "enabled" : "disabled");
				break;
			case SDLK_KP_PLUS:
			case SDLK_PAGEUP:
				_pi.stateSlot = 1;
				break;
			case SDLK_KP_MINUS:
			case SDLK_PAGEDOWN:
				_pi.stateSlot = -1;
				break;
			}
			break;
		}
		setAsciiChar(_pi, &raw.key.keysym);
		switch (ev.key.keysym.sym) {
		case SDLK_LEFT:
		case SDLK_KP_4:
			_pi.dirMask &= ~PlayerInput::DIR_LEFT;
			break;
		case SDLK_RIGHT:
		case SDLK_KP_6:
			_pi.dirMask &= ~PlayerInput::DIR_RIGHT;
			break;
		case SDLK_UP:
		case SDLK_KP_8:
			_pi.dirMask &= ~PlayerInput::DIR_UP;
			break;
		case SDLK_DOWN:
		case SDLK_KP_2:
			_pi.dirMask &= ~PlayerInput::DIR_DOWN;
			break;
		case SDLK_SPACE:
			_pi.space = false;
			break;
		case SDLK_RSHIFT:
		case SDLK_LSHIFT:
			_pi.shift = false;
			break;
		case SDLK_RETURN:
			_pi.enter = false;
			break;
		case SDLK_ESCAPE:
			_pi.escape = false;
			break;
		case SDLK_F1:
		case SDLK_F2:
		case SDLK_F3:
		case SDLK_F4:
		case SDLK_F5:
		case SDLK_F6:
		case SDLK_F7:
		case SDLK_F8:
			changeScaler(ev.key.keysym.sym - SDLK_F1);
			break;
		default:
			break;
		}
		break;
	case SDL_KEYDOWN:
		if (ev.key.keysym.mod & (KMOD_ALT | KMOD_CTRL)) {
			break;
		}
		switch (ev.key.keysym.sym) {
		case SDLK_LEFT:
		case SDLK_KP_4:
			_pi.dirMask |= PlayerInput::DIR_LEFT;
			break;
		case SDLK_RIGHT:
		case SDLK_KP_6:
			_pi.dirMask |= PlayerInput::DIR_RIGHT;
			break;
		case SDLK_UP:
		case SDLK_KP_8:
			_pi.dirMask |= PlayerInput::DIR_UP;
			break;
		case SDLK_DOWN:
		case SDLK_KP_2:
			_pi.dirMask |= PlayerInput::DIR_DOWN;
			break;
		case SDLK_BACKSPACE:
		case SDLK_TAB:
			_pi.backspace = true;
			break;
		case SDLK_SPACE:
			_pi.space = true;
			break;
		case SDLK_RSHIFT:
		case SDLK_LSHIFT:
			_pi.shift = true;
			break;
		case SDLK_RETURN:
			_pi.enter = true;
			break;
		case SDLK_ESCAPE:
			_pi.escape = true;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
}

void SystemStub_SDL::sleep(int duration) {
	SDL_Delay(duration);
}

uint32_t SystemStub_SDL::getTimeStamp() {
	return SDL_GetTicks();
}

static void mixAudioS16(void *param, uint8_t *buf, int len) {
	SystemStub_SDL *stub = (SystemStub_SDL *)param;
	memset(buf, 0, len);
	assert((len & 3) == 0);
	stub->_audioCbProc(stub->_audioCbData, (int16_t *)buf, len / (sizeof(int16_t) * 2));
}

void SystemStub_SDL::startAudio(AudioCallback callback, void *param) {
	stopAudio();
	SDL_AudioSpec desired;
	memset(&desired, 0, sizeof(desired));
	desired.freq = _options.outputSampleRate;
	desired.format = AUDIO_S16SYS;
	desired.channels = 2;
	// Approximately 23 ms at the requested rate, rounded to a power of two.
	// The simulation clock is independent of this device buffer size.
	desired.samples = 256;
	while (desired.samples < desired.freq / 43 && desired.samples < 4096) {
		desired.samples *= 2;
	}
	desired.callback = mixAudioS16;
	desired.userdata = this;
	SDL_AudioSpec obtained = {};
	_audioCbProc = callback;
	_audioCbData = param;
	_audioDevice =
	    SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
	if (!_audioDevice) {
		error("Unable to open sound device: %s", SDL_GetError());
	}
	_outputSampleRate = obtained.freq;
	info("Audio output: %d Hz, stereo, %d frames", obtained.freq, obtained.samples);
	SDL_PauseAudioDevice(_audioDevice, 0);
}

void SystemStub_SDL::stopAudio() {
	if (_audioDevice) {
		SDL_CloseAudioDevice(_audioDevice);
		_audioDevice = 0;
	}
	_audioCbProc = nullptr;
	_audioCbData = nullptr;
}

uint32_t SystemStub_SDL::getOutputSampleRate() {
	return _outputSampleRate;
}

void SystemStub_SDL::lockAudio() {
	if (_audioDevice) {
		SDL_LockAudioDevice(_audioDevice);
	}
}

void SystemStub_SDL::unlockAudio() {
	if (_audioDevice) {
		SDL_UnlockAudioDevice(_audioDevice);
	}
}


void SystemStub_SDL::prepareGraphics() {
	_frameDirty = true;
	_texW = _screenW;
	_texH = _screenH;
	switch (_scalerType) {
	case kScalerTypePoint:
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0"); // nearest pixel sampling
		break;
	case kScalerTypeLinear:
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1"); // linear filtering
		break;
	case kScalerTypeInternal:
	case kScalerTypeExternal:
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
		_texW *= _scaleFactor;
		_texH *= _scaleFactor;
		break;
	}
	_texRect.x = 0;
	_texRect.y = 0;
	_texRect.w = _texW;
	_texRect.h = _texH;
	int windowW = _screenW * _scaleFactor;
	int windowH = _screenH * _scaleFactor;
	int flags = SDL_WINDOW_ALLOW_HIGHDPI;
	if (_fullscreen) {
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	} else {
		flags |= SDL_WINDOW_RESIZABLE;
		if (_options.maximized) {
			flags |= SDL_WINDOW_MAXIMIZED;
		}
	}
	if (_widescreenMode == kWidescreenCDi) {
		windowW = (_screenW + kWidescreenBorderCDiW * 2) * _scaleFactor;
	} else if (_widescreenMode != kWidescreenNone) {
		windowW = windowH * 16 / 9;
	}
	_logicalW = windowW;
	_logicalH = windowH;
	_window = SDL_CreateWindow(_caption, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowW, windowH, flags);
	if (!_window) error("Cannot create game window: %s", SDL_GetError());
	SDL_Surface *icon = SDL_LoadBMP(kIconBmp);
	if (icon) {
		SDL_SetWindowIcon(_window, icon);
		SDL_FreeSurface(icon);
	}
	_renderer = SDL_CreateRenderer(_window, -1, SDL_RENDERER_ACCELERATED);
	if (!_renderer) _renderer = SDL_CreateRenderer(_window, -1, SDL_RENDERER_SOFTWARE);
	if (!_renderer) error("Cannot create renderer: %s", SDL_GetError());
	SDL_RenderSetLogicalSize(_renderer, _logicalW, _logicalH);
	if (SDL_RenderSetVSync(_renderer, _preferences.vsync ? 1 : 0) != 0 && _preferences.vsync) {
		warning("VSync unavailable on this renderer: %s", SDL_GetError());
		_preferences.vsync = false;
	}
	_texture = SDL_CreateTexture(_renderer, kPixelFormat, SDL_TEXTUREACCESS_STREAMING, _texW, _texH);
	if (!_texture) error("Cannot create game texture: %s", SDL_GetError());
	if (_widescreenMode != kWidescreenNone) {
		int w = _screenH * 16 / 9;
		// in blur mode, the background texture has the same dimensions as the game texture
		// SDL stretches the texture to 16:9
		if (_widescreenMode == kWidescreenBlur) {
			w = _screenW;
		} else if (_widescreenMode == kWidescreenCDi) {
			w = _screenW + kWidescreenBorderCDiW * 2;
		}
		_widescreenTexture = SDL_CreateTexture(_renderer, kPixelFormat, SDL_TEXTUREACCESS_STREAMING, w, _screenH);
		clearTexture(_widescreenTexture, _screenH, _fmt);
		_wideWidth = w;
		_widePixels.assign(size_t(w) * _screenH, 0);

		// left and right borders
		_wideMargin = (w - _screenW) / 2;
	}
	if (!_overlay) _overlay = std::make_unique<SettingsOverlay>();
	if (!_overlay->attach(_window, _renderer)) error("Cannot initialize settings overlay");
}

void SystemStub_SDL::cleanupGraphics() {
	if (_overlay) _overlay->detach();
	if (_texture) {
		SDL_DestroyTexture(_texture);
		_texture = 0;
	}
	if (_widescreenTexture) {
		SDL_DestroyTexture(_widescreenTexture);
		_widescreenTexture = 0;
	}
	if (_renderer) {
		SDL_DestroyRenderer(_renderer);
		_renderer = 0;
	}
	if (_window) {
		SDL_DestroyWindow(_window);
		_window = 0;
	}
}

void SystemStub_SDL::readPreferences(settings::Preferences &p) const {
    p.fullscreen = _fullscreen;
    p.vsync = _preferences.vsync;
    p.pauseOnFocusLoss = _preferences.pauseOnFocusLoss;
    p.showFps = _preferences.showFps;
    p.showMenuButton = _preferences.showMenuButton;
    p.menuLayout = _preferences.menuLayout;
    p.uiScale = _preferences.uiScale;
    p.keys = _preferences.keys;
    p.filter = _scalerType == kScalerTypePoint ? settings::Filter::Point :
               _scalerType == kScalerTypeLinear ? settings::Filter::Linear : settings::Filter::Scale;
    p.scale = _scaleFactor;
    if (p.filter == settings::Filter::Scale) p.scale = CLIP(p.scale, 2, 4);
    p.widescreen = _widescreenMode;
}

bool SystemStub_SDL::applyPreferences(const settings::Preferences &p, std::string &reason) {
    if (!settings::validate(p, reason)) return false;
    const ScalerType type = p.filter == settings::Filter::Point ? kScalerTypePoint :
                            p.filter == settings::Filter::Linear ? kScalerTypeLinear : kScalerTypeInternal;
    const bool displayChanged = p.fullscreen != _fullscreen || type != _scalerType ||
                                p.scale != _scaleFactor || p.widescreen != _widescreenMode;
    SDL_Texture *gameTexture = nullptr, *wideTexture = nullptr;
    const int texW = _screenW * (type == kScalerTypeInternal ? p.scale : 1);
    const int texH = _screenH * (type == kScalerTypeInternal ? p.scale : 1);
    int wideW = _screenH * 16 / 9;
    if (p.widescreen == kWidescreenBlur) wideW = _screenW;
    else if (p.widescreen == kWidescreenCDi) wideW = _screenW + 2 * kWidescreenBorderCDiW;
    if (displayChanged) {
        // Allocate replacement game textures first. The window, renderer and
        // ImGui resources stay alive, including keyboard focus and font atlas.
        gameTexture = SDL_CreateTexture(_renderer, kPixelFormat, SDL_TEXTUREACCESS_STREAMING, texW, texH);
        if (p.widescreen != kWidescreenNone)
            wideTexture = SDL_CreateTexture(_renderer, kPixelFormat, SDL_TEXTUREACCESS_STREAMING, wideW, _screenH);
        if (!gameTexture || (p.widescreen != kWidescreenNone && !wideTexture)) {
            reason = std::string("Display change could not allocate textures: ") + SDL_GetError();
            if (gameTexture) SDL_DestroyTexture(gameTexture);
            if (wideTexture) SDL_DestroyTexture(wideTexture);
            return false;
        }
        SDL_SetTextureScaleMode(gameTexture, type == kScalerTypePoint ? SDL_ScaleModeNearest : SDL_ScaleModeLinear);
        if (wideTexture) clearTexture(wideTexture, _screenH, _fmt);
        if (p.fullscreen != _fullscreen && SDL_SetWindowFullscreen(_window, p.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) != 0) {
            reason = std::string("Fullscreen change was rejected: ") + SDL_GetError();
            SDL_DestroyTexture(gameTexture);
            if (wideTexture) SDL_DestroyTexture(wideTexture);
            return false;
        }
    }
    if (p.vsync != _preferences.vsync && SDL_RenderSetVSync(_renderer, p.vsync ? 1 : 0) != 0) {
        reason = std::string("This renderer cannot change VSync: ") + SDL_GetError();
        if (p.fullscreen != _fullscreen)
            SDL_SetWindowFullscreen(_window, _fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
        if (gameTexture) SDL_DestroyTexture(gameTexture);
        if (wideTexture) SDL_DestroyTexture(wideTexture);
        return false;
    }
    if (displayChanged) {
        SDL_DestroyTexture(_texture);
        if (_widescreenTexture) SDL_DestroyTexture(_widescreenTexture);
        _texture = gameTexture;
        _frameDirty = true;
        _widescreenTexture = wideTexture;
        _wideWidth = wideW;
        _widePixels.assign(size_t(wideW) * _screenH, 0);
        _fullscreen = p.fullscreen;
        _scalerType = type;
        _scaler = type == kScalerTypeInternal ? &_internalScaler : nullptr;
        _scaleFactor = p.scale;
        _widescreenMode = p.widescreen;
        _enableWidescreen = p.widescreen != kWidescreenNone;
        _wideMargin = (wideW - _screenW) / 2;
        _texW = texW;
        _texH = texH;
        _texRect = {0, 0, _texW, _texH};
        _logicalH = _screenH * p.scale;
        _logicalW = _screenW * p.scale;
        if (p.widescreen == kWidescreenCDi) _logicalW = (_screenW + 2 * kWidescreenBorderCDiW) * p.scale;
        else if (p.widescreen != kWidescreenNone) _logicalW = _logicalH * 16 / 9;
        if (!_fullscreen) SDL_SetWindowSize(_window, _logicalW, _logicalH);
        SDL_RenderSetLogicalSize(_renderer, _settingsSession ? 0 : _logicalW, _settingsSession ? 0 : _logicalH);
    }
    _preferences = p;
    reason.clear();
    return true;
}

SDL_Keycode SystemStub_SDL::remapKey(const SDL_KeyboardEvent &event) const {
    const int key = event.keysym.scancode ? int(event.keysym.scancode) : int(SDL_GetScancodeFromKey(event.keysym.sym));
    if (key == SDL_SCANCODE_F10) return SDLK_ESCAPE;
    static const SDL_Keycode canonical[] = {SDLK_LEFT, SDLK_RIGHT, SDLK_UP, SDLK_DOWN,
        SDLK_LSHIFT, SDLK_RETURN, SDLK_SPACE, SDLK_TAB, SDLK_ESCAPE};
    for (size_t i = 0; i < settings::BindingCount; ++i) {
        if (_preferences.keys[i] == key) return canonical[i];
    }
    const settings::Preferences defaults;
    if (key == SDL_SCANCODE_RSHIFT && _preferences.keys[4] == defaults.keys[4]) return SDLK_LSHIFT;
    if (key == SDL_SCANCODE_BACKSPACE && _preferences.keys[7] == defaults.keys[7]) return SDLK_TAB;
    const int keypad[] = {SDL_SCANCODE_KP_4, SDL_SCANCODE_KP_6, SDL_SCANCODE_KP_8, SDL_SCANCODE_KP_2};
    for (size_t i = 0; i < 4; ++i) {
        if (key == keypad[i]) return _preferences.keys[i] == defaults.keys[i] ? canonical[i] : SDLK_UNKNOWN;
    }
    for (size_t i = 0; i < settings::BindingCount; ++i) {
        if (key == defaults.keys[i]) return SDLK_UNKNOWN;
    }
    return event.keysym.sym;
}

void SystemStub_SDL::showSettings(settings::Session &s) {
    auto applied = s.values;
    _settingsSession = &s;
    _pi.dirMask = 0;
    _pi.enter = _pi.shift = _pi.space = _pi.backspace = _pi.escape = false;
    SDL_RenderSetLogicalSize(_renderer, 0, 0);
    _overlay->open(s.values);
    while (!_pi.quit && s.action != settings::Action::Resume && s.action != settings::Action::Title && s.action != settings::Action::Quit) {
        processEvents();
        if (_pi.quit) break;
        _overlay->beginFrame(s.values);
        _overlay->draw(s);
        SDL_RenderSetLogicalSize(_renderer, _logicalW, _logicalH);
        drawGameFrame(0);
        SDL_RenderSetLogicalSize(_renderer, 0, 0);
        _overlay->render();
        SDL_RenderPresent(_renderer);
        if (s.applyRequested) {
            std::string reason;
            if (s.apply && s.apply(s.values, reason)) {
                applied = s.values;
                s.status = "Applied to this session. Save settings to keep changes.";
                s.statusError = false;
            } else {
                s.values = applied;
                s.status = reason.empty() ? "This setting could not be applied." : reason;
                s.statusError = true;
            }
            s.applyRequested = false;
        }
        if (s.saveRequested) {
            s.statusError = !s.persist || !s.persist(applied, s.status);
            s.saveRequested = false;
        }
        if (s.cheatsRequested) {
            if (s.applyCheats) s.applyCheats(s.cheats, s.debugFlags);
            s.cheatsRequested = false;
            s.status = "Session cheats updated. Saved preferences are unchanged.";
            s.statusError = false;
        }
        if (s.refreshRequested) {
            if (s.refreshSaves) s.refreshSaves();
            s.refreshRequested = false;
        }
        if (s.action == settings::Action::SaveGame || s.action == settings::Action::LoadGame ||
            s.action == settings::Action::Rewind || s.action == settings::Action::Heal ||
            s.action == settings::Action::Screenshot || s.action == settings::Action::StartLevel) {
            const auto command = s.action;
            const int slot = s.commandSlot >= 0 ? s.commandSlot : s.saveSlot;
            s.commandSlot = -1;
            const bool success = s.command && s.command(command, slot, s.status);
            s.statusError = !success;
            const bool resume = command == settings::Action::LoadGame || command == settings::Action::Rewind || command == settings::Action::StartLevel;
            s.action = success && resume ? settings::Action::Resume : settings::Action::None;
        }
        SDL_Delay(16);
    }
    _settingsSession = nullptr;
    SDL_RenderSetLogicalSize(_renderer, _logicalW, _logicalH);
    SDL_ShowCursor(SDL_DISABLE);
}

void SystemStub_SDL::changeGraphics(bool fullscreen, int scaleFactor) {
	const int factor = _scaler ? CLIP(scaleFactor, _scaler->factorMin, _scaler->factorMax) : scaleFactor;
	if (fullscreen == _fullscreen && factor == _scaleFactor) {
		// no change
		return;
	}
	_fullscreen = fullscreen;
	_scaleFactor = factor;
	cleanupGraphics();
	prepareGraphics();
}

void SystemStub_SDL::setScaler(const ScalerParameters *parameters) {
	static const struct {
		const char *name;
		int type;
		const Scaler *scaler;
	} scalers[] = {
		{ "point", kScalerTypePoint, 0 },
		{ "linear", kScalerTypeLinear, 0 },
		{ "scale", kScalerTypeInternal, &_internalScaler },
#ifdef USE_STATIC_SCALER
		{ "nearest", kScalerTypeInternal, &scaler_nearest },
		{ "tv2x", kScalerTypeInternal, &scaler_tv2x },
		{ "xbr", kScalerTypeInternal, &scaler_xbr },
#endif
		{ 0, -1, 0 }
	};
	bool found = false;
	for (int i = 0; scalers[i].name; ++i) {
		if (strcmp(scalers[i].name, parameters->name) == 0) {
			_scalerType = (ScalerType)scalers[i].type;
			_scaler = scalers[i].scaler;
			found = true;
			break;
		}
	}
	if (!found) {
#ifdef _WIN32
		static const char *libSuffix = "dll";
#else
		static const char *libSuffix = "so";
#endif
		char libname[64];
		snprintf(libname, sizeof(libname), "./scaler_%s.%s", parameters->name, libSuffix);
		_scalerSo = SDL_LoadObject(libname);
		if (!_scalerSo) {
			warning("Scaler '%s' not found: %s\nUsing default scaler", libname, SDL_GetError());
		} else {
			static const char *kSoSym = "getScaler";
			void *symbol = SDL_LoadFunction(_scalerSo, kSoSym);
			if (!symbol) {
				warning("Symbol '%s' not found in '%s': %s", kSoSym, libname, SDL_GetError());
			} else {
				typedef const Scaler *(*GetScalerProc)();
				const Scaler *scaler = ((GetScalerProc)symbol)();
				const int tag = scaler ? scaler->tag : 0;
				if (tag != SCALER_TAG) {
					warning("Unexpected tag %d for scaler '%s'", tag, libname);
				} else {
					_scalerType = kScalerTypeExternal;
					_scaler = scaler;
				}
			}
		}
	}
	_scaleFactor = _scaler ? CLIP(parameters->factor, _scaler->factorMin, _scaler->factorMax) : parameters->factor;
}

void SystemStub_SDL::changeScaler(int scalerNum) {
	ScalerType type = kScalerTypeInternal;
	const Scaler *scaler = 0;
	switch (scalerNum) {
	case 0:
		type = kScalerTypePoint;
		break;
	case 1:
		type = kScalerTypeLinear;
		break;
	case 2:
		type = kScalerTypeInternal;
		scaler = &_internalScaler;
		break;
#ifdef USE_STATIC_SCALER
	case 3:
		type = kScalerTypeInternal;
		scaler = &scaler_nearest;
		break;
	case 4:
		type = kScalerTypeInternal;
		scaler = &scaler_tv2x;
		break;
	case 5:
		type = kScalerTypeInternal;
		scaler = &scaler_xbr;
		break;
#endif
	default:
		return;
	}
	if (_scalerType != type || scaler != _scaler) {
		_scalerType = type;
		_scaler = scaler;
		if (_scalerType == kScalerTypeInternal || _scalerType == kScalerTypeExternal) {
			_scaleFactor = CLIP(_scaleFactor, _scaler->factorMin, _scaler->factorMax);
		} else {
			_scaleFactor = 1;
		}
		cleanupGraphics();
		prepareGraphics();
	}
}

void SystemStub_SDL::drawRect(int x, int y, int w, int h, uint8_t color) {
	const int x1 = x;
	const int y1 = y;
	const int x2 = x + w - 1;
	const int y2 = y + h - 1;
	assert(x1 >= 0 && x2 < _screenW && y1 >= 0 && y2 < _screenH);
	for (int i = x1; i <= x2; ++i) {
		*(_screenBuffer + y1 * _screenW + i) = *(_screenBuffer + y2 * _screenW + i) = _rgbPalette[color];
	}
	for (int j = y1; j <= y2; ++j) {
		*(_screenBuffer + j * _screenW + x1) = *(_screenBuffer + j * _screenW + x2) = _rgbPalette[color];
	}
}
