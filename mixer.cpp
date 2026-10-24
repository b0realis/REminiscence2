
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "mixer.h"
#include "music_prf.h"
#include "systemstub.h"
#include "util.h"

Mixer::Mixer(FileSystem *fs, SystemStub *stub)
    : _fs(fs), _stub(stub), _musicType(MT_NONE), _cpc(this, fs), _mod(this, fs), _ogg(this, fs),
      _sfx(this) {
	_musicTrack = -1;
	_backgroundMusicType = MT_NONE;
}

Mixer::~Mixer() {
	free();
}
void Mixer::configureMusic(const music::Options &options, FileSystem *gameFiles) {
	_prf = std::make_unique<music::PrfPlayer>(this, gameFiles, _fs, options);
}

void Mixer::setVolumes(int master, int music, int effects, bool muted) {
	LockAudioStack lock(_stub);
	_masterVolume = CLIP(master, 0, 100);
	_musicVolume = CLIP(music, 0, 100);
	_effectsVolume = CLIP(effects, 0, 100);
	_muted = muted;
}

void Mixer::init() {
	for (int i = 0; i < NUM_CHANNELS; ++i) {
		_channels[i].active = false;
	}
	_premixHook = 0;
	_stub->startAudio(Mixer::mixCallback, this);
}

void Mixer::free() {
	setPremixHook(0, 0);
	_mod.stop();
	_sfx.stop();
	_ogg.stopTrack();
	_cpc.stopTrack();
	if (_prf)
		_prf->stop();
	_musicType = _backgroundMusicType = MT_NONE;
	_musicTrack = -1;
	stopAll();
	_stub->stopAudio();
}

void Mixer::setPremixHook(PremixHook premixHook, void *userData) {
	debug(DBG_SND, "Mixer::setPremixHook()");
	LockAudioStack las(_stub);
	_premixHook = premixHook;
	_premixHookData = userData;
}

void Mixer::play(const uint8_t *data, uint32_t len, uint16_t freq, uint8_t volume) {
	debug(DBG_SND, "Mixer::play(%d, %d)", freq, volume);
	if (!data || len == 0 || freq == 0) {
		return;
	}
	volume = MIN<uint8_t>(volume, MAX_VOLUME);
	LockAudioStack las(_stub);
	MixerChannel *ch = 0;
	for (int i = 0; i < NUM_CHANNELS; ++i) {
		MixerChannel *cur = &_channels[i];
		if (cur->active) {
			if (cur->chunk.data == data) {
				cur->chunkPos = 0;
				cur->volume = volume;
				return;
			}
		} else {
			// Keep searching: an active copy may follow this free channel.
			if (!ch) {
				ch = cur;
			}
		}
	}
	if (ch) {
		ch->active = true;
		ch->volume = volume;
		ch->chunk.data = data;
		ch->chunk.len = len;
		ch->chunkPos = 0;
		ch->chunkInc = (freq << FRAC_BITS) / _stub->getOutputSampleRate();
	}
}

bool Mixer::hasPremixHook(void *owner) const {
	LockAudioStack lock(_stub);
	return _premixHook && _premixHookData == owner;
}

void Mixer::clearPremixHook(void *owner) {
	LockAudioStack lock(_stub);
	if (_premixHookData == owner) {
		_premixHook = nullptr;
		_premixHookData = nullptr;
	}
}

bool Mixer::isPlaying(const uint8_t *data) const {
	debug(DBG_SND, "Mixer::isPlaying");
	LockAudioStack las(_stub);
	for (int i = 0; i < NUM_CHANNELS; ++i) {
		const MixerChannel *ch = &_channels[i];
		if (ch->active && ch->chunk.data == data) {
			return true;
		}
	}
	return false;
}

uint32_t Mixer::getSampleRate() const {
	return _stub->getOutputSampleRate();
}

void Mixer::stopAll() {
	debug(DBG_SND, "Mixer::stopAll()");
	LockAudioStack las(_stub);
	for (uint8_t i = 0; i < NUM_CHANNELS; ++i) {
		_channels[i].active = false;
	}
}

static bool isMusicSfx(int num) {
	return (num >= 68 && num <= 75);
}

void Mixer::playMusic(int num, int tempo) {
	debug(DBG_SND, "Mixer::playMusic(%d, %d)", num, tempo);
	int trackNum = -1;
	if (num == 1) { // menu screen
		trackNum = 2;
	} else if (num > MUSIC_TRACK) {
		trackNum = num - MUSIC_TRACK;
	}
	if (trackNum != -1 && trackNum != _musicTrack) {
		if (_ogg.playTrack(trackNum)) {
			_backgroundMusicType = _musicType = MT_OGG;
			_musicTrack = trackNum;
			return;
		}
		if (_cpc.playTrack(trackNum)) {
			_backgroundMusicType = _musicType = MT_CPC;
			_musicTrack = trackNum;
			return;
		}
	}
	if ((_musicType == MT_OGG || _musicType == MT_CPC) && isMusicSfx(num)) { // do not play level action music with background music
		return;
	}
	if (isMusicSfx(num)) { // level action sequence
		_sfx.play(num);
		if (_sfx._playing) {
			_musicType = MT_SFX;
		}
	} else { // cutscene
		_mod.play(num, tempo);
		if (_mod._playing) {
			_musicType = MT_MOD;
			return;
		}
		if (g_options.use_prf_music && _prf && _prf->play(num)) {
			_musicType = MT_PRF;
			return;
		}
	}
}

void Mixer::stopMusic() {
	debug(DBG_SND, "Mixer::stopMusic()");
	switch (_musicType) {
	case MT_NONE:
		break;
	case MT_MOD:
		_mod.stop();
		break;
	case MT_OGG:
		_ogg.pauseTrack();
		break;
	case MT_SFX:
		_sfx.stop();
		break;
	case MT_PRF:
		if (_prf) {
			_prf->stop();
		}
		break;
	case MT_CPC:
		_cpc.pauseTrack();
		break;
	}
	_musicType = MT_NONE;
	if (_musicTrack > 2) { // do not resume menu music
		switch (_backgroundMusicType) {
		case MT_OGG:
			_ogg.resumeTrack();
			_musicType = MT_OGG;
			break;
		case MT_CPC:
			_cpc.resumeTrack();
			_musicType = MT_CPC;
			break;
		default:
			break;
		}
	} else {
		_musicTrack = -1;
	}
}

static const bool kUseNr = false;

static void nr(int16_t *buf, int len) {
	static int prev = 0;
	for (int i = 0; i < len; ++i) {
		const int vnr = buf[i] >> 1;
		buf[i] = vnr + prev;
		prev = vnr;
	}
}

void Mixer::mix(int16_t *out, int len) {
	memset(out, 0, sizeof(int16_t) * len * 2);
	if (_premixHook) {
		if (!_premixHook(_premixHookData, out, len)) {
			_premixHook = 0;
			_premixHookData = 0;
		}
	}
	// Scale the music contribution before adding effects. Muting still advances
	// both playback paths, avoiding a jump back to stale audio when unmuted.
	const int musicGain = _musicType == MT_SFX ? _effectsVolume : _musicVolume;
	for (int i = 0; i < len * 2; ++i) {
		out[i] = out[i] * musicGain / 100;
	}
	for (uint8_t i = 0; i < NUM_CHANNELS; ++i) {
		MixerChannel *ch = &_channels[i];
		if (ch->active) {
			for (int pos = 0; pos < len; ++pos) {
				const uint32_t cpos = ch->chunkPos >> FRAC_BITS;
				if (cpos >= ch->chunk.len) {
					ch->active = false;
					break;
				}
				const int sample = S8_to_S16(ch->chunk.getPCM(cpos)) * ch->volume / Mixer::MAX_VOLUME * _effectsVolume / 100;
				out[2 * pos]     = ADDC_S16(out[2 * pos],     sample);
				out[2 * pos + 1] = ADDC_S16(out[2 * pos + 1], sample);
				ch->chunkPos += ch->chunkInc;
			}
		}
	}
	if (kUseNr) {
		nr(out, len * 2); // stereo
	}
	const int masterGain = _muted ? 0 : _masterVolume;
	for (int i = 0; i < len * 2; ++i) {
		out[i] = out[i] * masterGain / 100;
	}
}

void Mixer::mixCallback(void *param, int16_t *buf, int len) {
	((Mixer *)param)->mix(buf, len);
}
