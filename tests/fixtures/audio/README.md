<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Synthetic Ogg fixture

`tone.ogg` contains an original 440 Hz sine wave: 1,600 mono samples at
16,000 Hz, with signed 16-bit amplitude `round(4096 * sin(2*pi*440*n/16000))`.
It was encoded with libsndfile 1.2.2 using `SF_FORMAT_OGG | SF_FORMAT_VORBIS`.
It contains no game or demo audio. The editor decoder test checks that the
decoded clip has samples, a valid rate and nonzero audio.
