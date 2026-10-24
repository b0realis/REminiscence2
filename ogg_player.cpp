
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifdef USE_TREMOR
#include <tremor/ivorbisfile.h>
#endif
#ifdef USE_STB_VORBIS
#include "stb_vorbis.c"
#endif
#include "file.h"
#include "mixer.h"
#include "ogg_player.h"
#include "util.h"
#include <array>

#ifdef USE_TREMOR
struct VorbisFile: File {
	uint32_t offset;

	static size_t readHelper(void *ptr, size_t size, size_t nmemb, void *datasource) {
		VorbisFile *vf = (VorbisFile *)datasource;
		if (size != 0 && nmemb != 0) {
			const int n = vf->read(ptr, size * nmemb);
			if (n > 0) {
				vf->offset += n;
				return n / size;
			}
		}
		return 0;
	}
	static int seekHelper(void *datasource, ogg_int64_t offset, int whence) {
		VorbisFile *vf = (VorbisFile *)datasource;
		switch (whence) {
		case SEEK_SET:
			vf->offset = offset;
			break;
		case SEEK_CUR:
			vf->offset += offset;
			break;
		case SEEK_END:
			vf->offset = vf->size() + offset;
			break;
		}
		vf->seek(vf->offset);
		return 0;
	}
	static int closeHelper(void *datasource) {
		VorbisFile *vf = (VorbisFile *)datasource;
		// The datasource is the decoder's embedded File, not a heap allocation.
		return vf->close() ? 0 : -1;
	}
	static long tellHelper(void *datasource) {
		VorbisFile *vf = (VorbisFile *)datasource;
		return vf->offset;
	}
};

struct OggDecoder_impl {
	OggDecoder_impl()
		: _open(false), _readBuf(0), _readBufSize(0) {
	}
	~OggDecoder_impl() {
		free(_readBuf);
		_readBuf = 0;
		if (_open) {
			ov_clear(&_ovf);
		}
	}

	bool load(const char *name, FileSystem *fs, int /*mixerSampleRate*/) {
		if (_open) {
			ov_clear(&_ovf);
			_open = false;
		}
		if (!_f.open(name, "rb", fs)) {
			return false;
		}
		_f.offset = 0;
		ov_callbacks ovcb;
		ovcb.read_func  = VorbisFile::readHelper;
		ovcb.seek_func  = VorbisFile::seekHelper;
		ovcb.close_func = VorbisFile::closeHelper;
		ovcb.tell_func  = VorbisFile::tellHelper;
		if (ov_open_callbacks(&_f, &_ovf, 0, 0, ovcb) < 0) {
			warning("Invalid .ogg file");
			return false;
		}
		_open = true;
		vorbis_info *vi = ov_info(&_ovf, -1);
		if ((vi->channels != 1 && vi->channels != 2) || vi->rate < 8000 || vi->rate > 192000) {
			warning("Unhandled ogg/pcm format ch %d rate %d", vi->channels, vi->rate);
			return false;
		}
		_channels = vi->channels;
		_sampleRate = vi->rate;
		return true;
	}
	int read(int16_t *dst, int samples) {
		if (!_open)
			return 0;
		bool restarted = false;
		int size = samples * _channels * sizeof(int16_t);
		if (size > _readBufSize) {
			_readBufSize = size;
			free(_readBuf);
			_readBuf = (int16_t *)malloc(_readBufSize);
			if (!_readBuf) {
				return 0;
			}
		}
		int count = 0;
		while (size > 0) {
			const int len = ov_read(&_ovf, (char *)_readBuf, size, 0);
			if (len < 0) {
				// error in decoder
				return count;
			} else if (len == 0) {
				// A damaged or empty stream must not spin on the audio thread.
				if (restarted || ov_raw_seek(&_ovf, 0) != 0)
					return count;
				restarted = true;
				continue;
			}
			restarted = false;
			assert((len & 1) == 0);
			switch (_channels) {
			case 2:
				assert((len & 3) == 0);
				for (int i = 0; i < len / 2; i += 2) {
					*dst = ADDC_S16(*dst, _readBuf[i]);
					++dst;
					*dst = ADDC_S16(*dst, _readBuf[i + 1]);
					++dst;
				}
				break;
			case 1:
				for (int i = 0; i < len / 2; ++i) {
					*dst = ADDC_S16(*dst, _readBuf[i]);
					++dst;
					*dst = ADDC_S16(*dst, _readBuf[i]);
					++dst;
				}
				break;
			}
			size -= len;
			count += len / (_channels * sizeof(int16_t));
		}
		assert(size == 0);
		return count;
	}

	VorbisFile _f;
	OggVorbis_File _ovf;
	int _channels;
	int _sampleRate = 0;
	bool _open;
	int16_t *_readBuf;
	int _readBufSize;
};
#endif

#ifdef USE_STB_VORBIS
static const int kMusicVolume = 192;

struct OggDecoder_impl {
	OggDecoder_impl()
		: _v(0) {
	}
	~OggDecoder_impl() {
		if (_v) {
			stb_vorbis_close(_v);
			_v = 0;
		}
	}
	bool load(const char *name, FileSystem *fs, int /*mixerSampleRate*/) {
		if (_v) {
			stb_vorbis_close(_v);
			_v = nullptr;
		}
		_decodedSamplesLen = _decodedSamplesOffset = 0;
		if (!_f.open(name, "rb", fs)) {
			return false;
		}
		_count = _f.read(_buffer, sizeof(_buffer));
		if (_count > 0) {
			int bytes = 0;
			int error = 0;
			_v = stb_vorbis_open_pushdata(_buffer, _count, &bytes, &error, 0);
			if (_v) {
				_offset = bytes;
				stb_vorbis_info info = stb_vorbis_get_info(_v);
				if ((info.channels != 1 && info.channels != 2) || info.sample_rate < 8000 ||
				    info.sample_rate > 192000 || info.max_frame_size > 8192) {
					warning("Unhandled ogg/pcm format ch %d rate %d", info.channels, info.sample_rate);
					return false;
				}
				_sampleRate = info.sample_rate;
				_decodedSamplesLen = 0;
				_decodedSamplesOffset = 0;
				return true;
			}
		}
		return false;
	}
	int read(int16_t *dst, int samples) {
		int total = 0;
		int emptyLoops = 0;
		if (_decodedSamplesLen != 0) {
			const int len = MIN(_decodedSamplesLen, samples);
			for (int i = 0; i < len; ++i) {
				*dst = ADDC_S16(
				    *dst, ((_decodedSamples[0][_decodedSamplesOffset + i] * kMusicVolume) >> 8));
				++dst;
				*dst = ADDC_S16(
				    *dst, ((_decodedSamples[1][_decodedSamplesOffset + i] * kMusicVolume) >> 8));
				++dst;
			}
			total += len;
			_decodedSamplesLen -= len;
			_decodedSamplesOffset += len;
		}
		while (_v && total < samples) {
			int channels = 0;
			float **outputs;
			int count;
			int bytes = stb_vorbis_decode_frame_pushdata(_v, _buffer + _offset, _count - _offset, &channels, &outputs, &count);
			if (bytes == 0) {
				if (_offset != _count) {
					memmove(_buffer, _buffer + _offset, _count - _offset);
					_offset = _count - _offset;
				} else {
					_offset = 0;
				}
				_count = sizeof(_buffer) - _offset;
				bytes = _f.read(_buffer + _offset, _count);
				if (bytes < 0) {
					break;
				}
				if (bytes == 0) {
					// rewind
					if (++emptyLoops > 1)
						break; // Empty/corrupt streams cannot spin in the audio callback.
					_f.seek(0);
					_count = _f.read(_buffer, sizeof(_buffer));
					stb_vorbis_flush_pushdata(_v);
				} else {
					_count = _offset + bytes;
				}
				_offset = 0;
				continue;
			}
			_offset += bytes;
			if (channels == 1 || channels == 2) {
				if (count > 0)
					emptyLoops = 0;
				const int remain = samples - total;
				const int len = MIN(count, remain);
				for (int i = 0; i < len; ++i) {
					const int l = CLIP(int(outputs[0][i] * 32768), -32768, 32767);
					const int r = CLIP(int(outputs[channels - 1][i] * 32768), -32768, 32767);
					*dst = ADDC_S16(*dst, ((l * kMusicVolume) >> 8));
					++dst;
					*dst = ADDC_S16(*dst, ((r * kMusicVolume) >> 8));
					++dst;
				}
				if (count > remain) {
					_decodedSamplesLen = count - remain;
					_decodedSamplesOffset = 0;
					if (_decodedSamplesLen > 8192)
						return total;
					for (int i = 0; i < _decodedSamplesLen; ++i) {
						_decodedSamples[0][i] =
						    CLIP(int(outputs[0][len + i] * 32768), -32768, 32767);
						_decodedSamples[1][i] =
						    CLIP(int(outputs[channels - 1][len + i] * 32768), -32768, 32767);
					}
					total = samples;
					break;
				}
			} else {
				warning("Invalid decoded data channels %d count %d", channels, count);
			}
			total += count;
		}
		return total;
	}

	int _sampleRate = 0;
	uint8_t _buffer[8192];
	std::array<std::array<int16_t, 8192>, 2> _decodedSamples;
	int _decodedSamplesLen = 0, _decodedSamplesOffset = 0;
	uint32_t _offset, _count;
	stb_vorbis *_v;
	File _f;
};
#endif

OggPlayer::OggPlayer(Mixer *mixer, FileSystem *fs)
	: _mix(mixer), _fs(fs) {
	_impl = new OggDecoder_impl;
}

OggPlayer::~OggPlayer() {
	stopTrack();
	delete _impl;
	_impl = 0;
}

// https://www.amigaremix.com/remix/191
static const char *kMenuThemeRemix = "deadly_cookie_-_flashback.ogg";

bool OggPlayer::playTrack(int num) {
	char buf[16];
	snprintf(buf, sizeof(buf), "track%02d.ogg", num);
	return playFile(buf) || (num == 2 && playFile(kMenuThemeRemix));
}

bool OggPlayer::playFile(const char *name) {
	stopTrack();
	if (_impl->load(name, _fs, _mix->getSampleRate())) {
		_inputCount = _inputOffset = 0;
		_phase = 0;
		_primed = false;
		_step = double(_impl->_sampleRate) / _mix->getSampleRate();
		_mix->setPremixHook(mixCallback, this);
		return true;
	}
	return false;
}

void OggPlayer::stopTrack() {
	if (_impl) {
		_mix->clearPremixHook(this);
	}
}

void OggPlayer::pauseTrack() {
	if (_impl) {
		_mix->clearPremixHook(this);
	}
}

void OggPlayer::resumeTrack() {
	if (_impl) {
		_mix->setPremixHook(mixCallback, this);
	}
}

bool OggPlayer::nextFrame(int16_t *frame) {
	if (_inputOffset == _inputCount) {
		_input.fill(0); // Decoder adds to the destination, like the mixer.
		_inputCount = _impl->read(_input.data(), _input.size() / 2);
		_inputOffset = 0;
		if (_inputCount <= 0)
			return false;
	}
	frame[0] = _input[_inputOffset * 2];
	frame[1] = _input[_inputOffset * 2 + 1];
	++_inputOffset;
	return true;
}

bool OggPlayer::mix(int16_t *buf, int len) {
	if (!_impl)
		return false;
	if (_step == 1.0)
		return _impl->read(buf, len) != 0;
	// Stream at the device rate without changing pitch or loading an entire
	// soundtrack into RAM. Keep interpolation state across callback boundaries.
	if (!_primed) {
		if (!nextFrame(_left) || !nextFrame(_right))
			return false;
		_primed = true;
	}
	for (int i = 0; i < len; ++i) {
		for (int channel = 0; channel < 2; ++channel) {
			const int value = int(_left[channel] + (_right[channel] - _left[channel]) * _phase);
			*buf = ADDC_S16(*buf, value);
			++buf;
		}
		_phase += _step;
		while (_phase >= 1.0) {
			_left[0] = _right[0];
			_left[1] = _right[1];
			if (!nextFrame(_right))
				return false;
			_phase -= 1.0;
		}
	}
	return true;
}

bool OggPlayer::mixCallback(void *param, int16_t *buf, int len) {
	return ((OggPlayer *)param)->mix(buf, len);
}
