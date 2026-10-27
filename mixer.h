
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef MIXER_H__
#define MIXER_H__

#include "intern.h"
#include "cpc_player.h"
#include "mod_player.h"
#include "ogg_player.h"
#include "sfx_player.h"
#include "music_options.h"
#include <memory>
#include <atomic>
namespace music {
class PrfPlayer;
}

struct MixerChunk {
	const uint8_t *data;
	uint32_t len;

	MixerChunk() : data(0), len(0) {}

	int8_t getPCM(int offset) const {
		if (offset < 0) {
			offset = 0;
		} else if (offset >= (int)len) {
			offset = len - 1;
		}
		return (int8_t)data[offset];
	}
};

enum class MixerTone { Original, Bass, DeepBass };

struct MixerChannel {
	uint8_t active;
	uint8_t volume;
	MixerChunk chunk;
	uint32_t chunkPos;
	uint32_t chunkInc;
	MixerTone tone = MixerTone::Original;
	int lowPass = 0, bassCoefficient = 0;
};

struct FileSystem;
struct SystemStub;

struct Mixer {
	typedef bool (*PremixHook)(void *userData, int16_t *buf, int len);

	enum MusicType {
		MT_NONE,
		MT_MOD,
		MT_OGG,
		MT_SFX,
		MT_CPC,
		MT_PRF,
		MT_CUE,
		MT_SOURCE_PRF,
	};

	enum { MUSIC_TRACK = 1000, NUM_CHANNELS = 4, FRAC_BITS = 12, MAX_VOLUME = 64 };

	using ContentHook = void (*)(void *, int16_t *, int, int, int);
	ContentHook _contentHook = nullptr;
	void *_contentOwner = nullptr;
	void setContentHook(ContentHook, void *);
	void clearContentHook(void *);
	FileSystem *_fs;
	SystemStub *_stub;
	MixerChannel _channels[NUM_CHANNELS];
	PremixHook _premixHook = nullptr;
	void *_premixHookData = nullptr;
	MusicType _backgroundMusicType;
	std::atomic<MusicType> _musicType;
	CpcPlayer _cpc;
	ModPlayer _mod;
	OggPlayer _ogg;
	OggPlayer _cueOgg;
	SfxPlayer _sfx;
	int _musicTrack;
	std::unique_ptr<music::PrfPlayer> _prf;
	std::unique_ptr<music::PrfPlayer> _sourcePrf;
	music::Options _musicOptions;
	FileSystem *_sourceFiles = nullptr;
	int _sourceEdition = -1;
	bool musicIsAmiga(bool nativeAmiga) const {
		return _sourceEdition < 0 ? nativeAmiga : _sourceEdition == kResourceTypeAmiga;
	}
	void selectMusicSource(FileSystem *files, int edition);
	int _masterVolume = 100, _musicVolume = 100, _effectsVolume = 100;
	bool _muted = false;

	Mixer(FileSystem *fs, SystemStub *stub);
	~Mixer();
	void configureMusic(const music::Options &options, FileSystem *gameFiles);
	void setVolumes(int master, int music, int effects, bool muted);
	void init();
	void free();
	void setPremixHook(PremixHook premixHook, void *userData);
	void clearPremixHook(void *owner);
	bool hasPremixHook(void *owner) const;
	void play(const uint8_t *data, uint32_t len, uint16_t freq, uint8_t volume,
	          MixerTone tone = MixerTone::Original);
	bool isPlaying(const uint8_t *data) const;
	uint32_t getSampleRate() const;
	void stopAll();
	void playMusic(int num, int tempo = 0, bool trySource = true);
	bool playSourceMusic(int num, int tempo);
	void playCutsceneMusic(int event, bool nativeAmiga);
	void stopMusic();
	void mix(int16_t *buf, int len);

	static void mixCallback(void *param, int16_t *buf, int len);
};

#endif // MIXER_H__
