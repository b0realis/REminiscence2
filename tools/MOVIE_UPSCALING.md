<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Offline movie upscaling

## Tool files

All reusable source and documentation live in `tools/`:

- [upscale_movies.py](upscale_movies.py): movie listing, export, Real-CUGAN processing,
  encoding and validation.
- [upscale-movies-local.sh](upscale-movies-local.sh): launcher for this checkout's
  prepared assets and Python environment.
- [export_mcd.cpp](export_mcd.cpp): headless exporter using the engine's MCD decoder.
- [restore_movie_region.py](restore_movie_region.py): optional tracked-region repairs.
- [requirements-upscaling.txt](requirements-upscaling.txt): Python dependencies.
- [movie_recipes/sc1_dock25.json](movie_recipes/sc1_dock25.json): the intro sign repair.

Run the examples below from the repository root. The root-level
`upscale-movies-local.sh` remains a convenience wrapper for the launcher in `tools/`.

## Overview

`tools/upscale_movies.py` turns extracted Sega CD `.MCD` movies into MP4s with
Real-CUGAN Pro. It reuses the engine's verified decoder for all three MCD codecs.
Enhanced MP4 playback in the game is not implemented. DOS/Amiga polygon movies,
Macintosh movies, 3DO videos and arbitrary input videos are outside this tool.

Names follow the movie player's **Name** column, not its menu number:
`SC1.MCD` / playlist `sc1` becomes `upscaled/sc1_cugan_pro_3x.mp4`.
Defaults are Pro, 3×, denoise 3, full-frame 256-pixel tiles and accurate sync.
Native 256×128 frames become 768×384. The JSON beside the MP4 records settings,
source/model hashes, timing and any restoration recipe.

## This checkout

The final, lettering-corrected intro is saved at
`upscaled/sc1_cugan_pro_3x.mp4`, with its original audio and timing. This folder
is Git-ignored. Runner/models are in `.local/movie-tools/realcugan`, Python
dependencies in `.local/movie-tools/venv`, and the private sign reference and
generation prompt in `.local/movie-restoration/sc1`.

```sh
./tools/upscale-movies-local.sh --list
./tools/upscale-movies-local.sh --movie bridge
./tools/upscale-movies-local.sh --movie sc2
# Several movies, processed sequentially:
./tools/upscale-movies-local.sh --movie spy --movie memory
```

No arguments lists the movies. The launcher also works from another current
directory. Rendering needs time and temporary disk space, especially for the
ending. Success removes only the run's own temporary directory unless
`--keep-work` is supplied; failures retain frames and logs. Existing MP4/JSON
files are never overwritten. Choose another `--output` folder for comparisons.

## Setup elsewhere

Install the project's build dependencies from the root README, then:

```sh
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --target reminiscence2-export-mcd --parallel
python3 -m venv .local/movie-tools/venv
.local/movie-tools/venv/bin/python -m pip install -r tools/requirements-upscaling.txt
```

On Windows use `.local\movie-tools\venv\Scripts\python.exe`. Obtain the host's
official [Real-CUGAN ncnn Vulkan executable and models](https://github.com/nihui/realcugan-ncnn-vulkan/releases).
This checkout was tested with the `20220728` macOS distribution on Apple M4.
Keep its license notices. The repository does not bundle models or the runner.
That runner uses a Vulkan backend; this workflow does not require CUDA.

Extract it into `.local/movie-tools/realcugan`, or supply explicit paths:

```sh
python tools/upscale_movies.py /path/to/extracted/segacd --movie sc1 \
  --exporter build/release/reminiscence2-export-mcd \
  --cugan /path/to/realcugan-ncnn-vulkan --models /path/to/models-pro \
  --output /path/to/private/upscaled
```

Use the virtual environment's Python for that command. FFmpeg is found on PATH
or supplied by `imageio-ffmpeg`; `--ffmpeg` selects an explicit binary.
`--help` describes every switch. Pro supports `--scale 2` or `3`; denoise choices
are `-1` (conservative), `0`, and `3`. Non-default noise variants receive an extra
filename suffix. `--list` needs only Python's standard library. Keep original
MCD names: the engine selects some codecs and playback limits by filename.

## Timing and audio

`tools/export_mcd.cpp` emits native PPM frames, `frames.tsv`, and `audio.wav`
without opening a game window. It calls `mcd::Stream` and `playbackFrames`, using
the game's event lengths and SC2 ending limit instead of trailing disc padding.
The timing table stores source index, start tick and duration at **60 ticks/s**.
MP4 repeats those exact frame holds at 60fps, with no interpolated motion.

Audio is demultiplexed from every fifth 2048-byte sector, converted from
sign-magnitude PCM to mono 16-bit 30720 Hz WAV, and trimmed to the video span
(512 samples per tick). MP4 encodes AAC at 48 kHz. Both completed tracks are fully
decoded again and the exact video frame count is checked before publication.
Original game files are never modified.

## Tracked repairs

`tools/restore_movie_region.py` applies an optional reference image using a JSON
recipe. It verifies source/reference hashes, dimensions and frame ranges, then
follows saved perspective transforms and matches wall illumination. Pixels
outside the feathered polygon remain identical in lossless frames; normal
H.264 recompression can still affect other pixels in the final MP4.

`movie_recipes/sc1_dock25.json` restores the confirmed **RESTRICTED AREA - DANGER ZONE** text
in both sign shots. It is specific to the tested SC1 pressing and 3× output.
Its private reference was repaired once using built-in image generation, then
tracked through the shots. Reproduce it into a new folder with:

```sh
./tools/upscale-movies-local.sh --movie sc1 \
  --restoration tools/movie_recipes/sc1_dock25.json \
  --reference .local/movie-restoration/sc1/repaired-sign-reference.png \
  --output upscaled/sc1-check
```

For another movie, render normally first. Add a repair recipe only after
checking the wording/design, camera tracking, cuts, lighting and foreground
occlusions. Automatic occlusion tracking is not implemented. The lighting model
is intended for a wall/sign, not every kind of surface. Do not apply the SC1
recipe to unrelated footage.

Recipes specify schema, source/reference SHA-256, output size, polygon, feather
parameters and non-overlapping `first`/`last` frame segments. Each segment has a
3×3 matrix mapping reference coordinates into the current frame. Recipes contain
no executable code. Keep reference art and its creation prompt privately.

## Checks

```sh
REMINISCENCE2_MCD_EXPORTER="$PWD/build/release/reminiscence2-export-mcd" \
  python3 -m unittest discover -s tests -p test_upscale_movies.py -v
python3 tools/check_release_docs.py
```

Synthetic tests exercise all three codecs, RGB output, variable frame holds,
PCM sign conversion/interleaving/trimming, malformed input and output protection.
Review AI results for invented detail, flicker and lettering distortion.
Enhanced game artwork/audio remain local; the engine/tool license does not
relicense those assets.
