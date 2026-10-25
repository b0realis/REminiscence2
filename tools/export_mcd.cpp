// SPDX-License-Identifier: GPL-3.0-or-later
// Lossless offline export through the engine's canonical Sega CD decoder.
#include "config.h"
#include "fs.h"
#include "mcd_player.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

Options g_options;
const char *g_caption = "REminiscence2 MCD export";
namespace fs = std::filesystem;

static void require(bool ok, const char *why) {
    if (!ok) throw std::runtime_error(why);
}
static void le16(std::ostream &out, uint16_t n) {
    const char b[] = {char(n), char(n >> 8)};
    out.write(b, sizeof(b));
}
static void le32(std::ostream &out, uint32_t n) {
    le16(out, n); le16(out, n >> 16);
}

int main(int argc, char **argv) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::cout << "Usage: reminiscence2-export-mcd INPUT.MCD NEW_OUTPUT_DIRECTORY\n"
                     "Export native PPM frames, frames.tsv (index/start/duration in 1/60 s),\n"
                     "and original mono 30720 Hz PCM16 audio.wav. Uses game playback limits.\n"
                     "The destination must not exist. Source assets are read-only.\n";
        return 0;
    }
    if (argc != 3) {
        std::cerr << "Use --help for usage.\n";
        return 1;
    }
    try {
        const fs::path input = fs::absolute(argv[1]), output = fs::absolute(argv[2]);
        require(fs::is_regular_file(input), "Input movie does not exist");
        std::string name = input.filename().string();
        for (char &c : name) if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
        require(name.size() > 4 && name.substr(name.size() - 4) == ".MCD", "Expected an MCD file");
        FileSystem files(input.parent_path().string().c_str());
        mcd::Stream movie;
        require(movie.open(input.filename().string().c_str(), &files), "Unsupported or malformed MCD stream");
        const auto count = movie.playbackFrames(name.c_str());
        require(count > 0, "Movie has no playback frames");
        uint32_t ticks = 0;
        for (size_t i = 0; i < count; ++i) {
            require(movie.frames[i].sector == ticks, "Non-contiguous movie timing");
            ticks += movie.frames[i].sectors;
        }
        const uint64_t samples = uint64_t(ticks) * 512;
        require(samples <= movie.audioSamples && samples * 2 <= UINT32_MAX - 36,
                "Movie audio is shorter than playback or exceeds WAV limits");
        // create_directory atomically refuses an existing output, including a
        // symlink. A failed export retains its partial directory for inspection.
        require(fs::create_directory(output), "Output directory already exists");
        fs::create_directory(output / "frames");
        std::ofstream timings(output / "frames.tsv");
        require(bool(timings), "Cannot create timing table");
        for (size_t i = 0; i < count; ++i) {
            std::array<uint8_t, 32768> pixels{};
            std::array<uint8_t, 768> palette{};
            require(movie.frame(i, pixels.data(), palette.data()), "Frame decoding failed");
            char filename[32];
            snprintf(filename, sizeof(filename), "%06zu.ppm", i);
            std::ofstream image(output / "frames" / filename, std::ios::binary);
            image << "P6\n256 128\n255\n";
            for (uint8_t pixel : pixels)
                image.write(reinterpret_cast<const char *>(palette.data() + pixel * 3), 3);
            image.close();
            require(bool(image), "Cannot write exported frame");
            timings << i << '\t' << movie.frames[i].sector << '\t' << movie.frames[i].sectors << '\n';
        }
        timings.close();
        require(bool(timings), "Cannot write timing table");
        std::ofstream wav(output / "audio.wav", std::ios::binary);
        wav.write("RIFF", 4); le32(wav, uint32_t(samples * 2 + 36));
        wav.write("WAVEfmt ", 8); le32(wav, 16); le16(wav, 1); le16(wav, 1);
        le32(wav, 30720); le32(wav, 61440); le16(wav, 2); le16(wav, 16);
        wav.write("data", 4); le32(wav, uint32_t(samples * 2));
        std::array<uint8_t, 2048> block{};
        uint64_t remaining = samples;
        for (uint32_t i = 0; remaining; ++i) {
            movie.file.seek(i * 10240 + 8192);
            require(movie.file.read(block.data(), block.size()) == block.size() && !movie.file.ioErr(),
                    "Cannot read movie PCM block");
            const auto take = std::min(remaining, uint64_t(block.size()));
            for (size_t j = 0; j < take; ++j) {
                // Sega CD PCM is sign-magnitude, not unsigned/two's complement.
                const int sample = (block[j] & 128 ? -int(block[j] & 127) : int(block[j])) * 256;
                le16(wav, uint16_t(int16_t(sample)));
            }
            remaining -= take;
        }
        wav.close();
        require(bool(wav), "Cannot write exported audio");
        std::cout << count << " frames, " << ticks << " ticks, " << samples << " PCM samples\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "MCD export: " << e.what() << '\n';
        return 1;
    }
}
