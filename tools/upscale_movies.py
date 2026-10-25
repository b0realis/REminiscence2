#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Export and upscale local Sega CD MCD movies with Real-CUGAN Pro (Python 3.9+)."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import wave

ROOT = Path(__file__).resolve().parent.parent


def digest(path: Path) -> str:
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()


def movies(assets: Path) -> dict[str, Path]:
    result = {}
    for path in sorted(assets.iterdir(), key=lambda p: p.name.casefold()):
        if path.is_file() and path.suffix.lower() == ".mcd":
            name = path.stem.lower()
            if not re.fullmatch(r"[a-z0-9_]{1,16}", name) or name in result:
                raise ValueError(f"Unsafe or ambiguous movie name: {path.name}")
            result[name] = path.resolve()
    return result


def read_timing(path: Path) -> list[tuple[int, int, int]]:
    rows = []
    tick = 0
    for index, line in enumerate(path.read_text(encoding="ascii").splitlines()):
        fields = tuple(map(int, line.split()))
        if len(fields) != 3 or fields[0] != index or fields[1] != tick or not 1 <= fields[2] <= 7:
            raise ValueError("Exported movie has invalid frame timing")
        rows.append(fields)
        tick += fields[2]
    if not rows or len(rows) > 100000:
        raise ValueError("Exported movie has an invalid frame count")
    return rows


def output_stem(name: str, scale: int, denoise: int) -> str:
    suffix = "" if denoise == 3 else ("_conservative" if denoise == -1 else "_n0")
    return f"{name}_cugan_pro_{scale}x{suffix}"


def executable(value: str | None, candidates: list[Path], label: str) -> Path:
    if value:
        candidates = [Path(shutil.which(value) or value).expanduser()]
    for path in candidates:
        if path.is_file() and os.access(path, os.X_OK):
            return path.resolve()
    raise ValueError(f"{label} not found. See tools/MOVIE_UPSCALING.md for setup.")


def run(command: list[str], log: Path):
    with log.open("wb") as stream:
        result = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT, check=False)
    if result.returncode:
        raise ValueError(f"Command failed ({result.returncode}); see {log}")


def publish(source: Path, destination: Path):
    # Stage on the destination filesystem, then publish without ever replacing
    # an existing movie, even if another renderer finished during this run.
    fd, name = tempfile.mkstemp(prefix=".upscale-", suffix=destination.suffix, dir=destination.parent)
    temporary = Path(name)
    try:
        with os.fdopen(fd, "wb") as target, source.open("rb") as original:
            shutil.copyfileobj(original, target)
            target.flush()
            os.fsync(target.fileno())
        os.link(temporary, destination)
    finally:
        temporary.unlink(missing_ok=True)


def encode(ffmpeg: Path, frames: Path, rows, audio: Path, destination: Path, size, restoration=None):
    from PIL import Image
    command = [str(ffmpeg), "-hide_banner", "-loglevel", "error", "-y", "-f", "rawvideo",
               "-pixel_format", "rgb24", "-video_size", f"{size[0]}x{size[1]}", "-framerate", "60",
               "-i", "pipe:0", "-i", str(audio), "-map", "0:v:0", "-map", "1:a:0",
               "-c:v", "libx264", "-preset", "veryfast", "-crf", "18", "-pix_fmt", "yuv420p",
               "-c:a", "aac", "-b:a", "128k", "-ar", "48000", "-movflags", "+faststart", str(destination)]
    log = destination.with_suffix(".encode.log")
    with log.open("wb") as stream:
        p = subprocess.Popen(command, stdin=subprocess.PIPE, stderr=stream)
        try:
            for index, _, ticks in rows:
                with Image.open(frames / f"{index:06}.png") as source:
                    image = source.convert("RGB")
                if image.size != size:
                    raise ValueError(f"Wrong upscale dimensions at frame {index}")
                if restoration:
                    image = restoration.apply(index, image)
                data = image.tobytes()
                # Preserve the exact 1/60-second holds from the MCD index.
                for _ in range(ticks):
                    p.stdin.write(data)
            p.stdin.close()
            if p.wait() != 0:
                raise ValueError(f"FFmpeg encoding failed; see {log}")
        finally:
            if p.poll() is None:
                p.terminate()
                p.wait()
    result = subprocess.run([str(ffmpeg), "-hide_banner", "-loglevel", "error", "-xerror",
                             "-i", str(destination), "-map", "0:v:0", "-map", "0:a:0",
                             "-progress", "pipe:1", "-f", "null", "-"], capture_output=True, text=True, check=True)
    counts = [int(line.split("=", 1)[1]) for line in result.stdout.splitlines() if line.startswith("frame=")]
    if not counts or counts[-1] != sum(row[2] for row in rows) or result.stderr.strip():
        raise ValueError("Encoded movie failed frame-count or full-decode validation")


def render(source: Path, args, exporter: Path, runner: Path, models: Path, ffmpeg: Path):
    from PIL import Image
    name = output_stem(source.stem.lower(), args.scale, args.denoise)
    output, sidecar = args.output / (name + ".mp4"), args.output / (name + ".json")
    if output.exists() or sidecar.exists() or output.is_symlink() or sidecar.is_symlink():
        raise ValueError(f"Output already exists: {output}. Choose another --output folder.")
    source_hash = digest(source)
    work = Path(tempfile.mkdtemp(prefix=name + "-", dir=args.work_dir))
    print(f"{source.name}: exporting original frames/audio (work: {work})", flush=True)
    try:
        export = work / "export"
        run([str(exporter), str(source), str(export)], work / "export.log")
        rows = read_timing(export / "frames.tsv")
        ticks = sum(row[2] for row in rows)
        with wave.open(str(export / "audio.wav"), "rb") as audio:
            if (audio.getframerate(), audio.getnchannels(), audio.getsampwidth(), audio.getnframes()) != (30720, 1, 2, ticks * 512):
                raise ValueError("Exported audio does not match the movie timing")
        native, upscaled = work / "native", work / "upscaled"
        native.mkdir(); upscaled.mkdir()
        for index, _, _ in rows:
            with Image.open(export / "frames" / f"{index:06}.ppm") as image:
                if image.size != (256, 128):
                    raise ValueError("Unexpected native movie dimensions")
                image.save(native / f"{index:06}.png")
        size = (256 * args.scale, 128 * args.scale)
        restoration = None
        if args.restoration:
            from restore_movie_region import Restoration
            restoration = Restoration(args.restoration, args.reference, source_hash, size, len(rows))
        print(f"{source.name}: CUGAN Pro {args.scale}x, denoise {args.denoise}, {len(rows)} frames", flush=True)
        run([str(runner), "-i", str(native), "-o", str(upscaled), "-m", str(models),
             "-s", str(args.scale), "-n", str(args.denoise), "-t", "256", "-c", "1", "-j", "1:1:1"], work / "cugan.log")
        expected = {f"{i:06}.png" for i, _, _ in rows}
        if {p.name for p in upscaled.iterdir()} != expected:
            raise ValueError("Upscaler did not produce exactly the expected frames")
        print(f"{source.name}: encoding and validating audio/video", flush=True)
        staged = work / (name + ".mp4")
        encode(ffmpeg, upscaled, rows, export / "audio.wav", staged, size, restoration)
        if digest(source) != source_hash:
            raise ValueError("Source movie changed during rendering; refusing to publish")
        model = f"up{args.scale}x-" + ({-1: "conservative", 0: "no-denoise", 3: "denoise3x"}[args.denoise])
        metadata = {"schema": 1, "playlist_name": source.stem.lower(), "source": source.name,
                    "source_sha256": source_hash, "method": "cugan_pro", "scale": args.scale,
                    "denoise": args.denoise, "tile_size": 256, "sync_gap_mode": 1,
                    "source_frames": len(rows), "video_ticks": ticks, "duration_seconds": ticks / 60,
                    "size": list(size), "frame_interpolation": False, "audio": "Original PCM encoded as AAC",
                    "output": output.name, "output_sha256": digest(staged),
                    "runner_sha256": digest(runner), "exporter_sha256": digest(exporter),
                    "model_sha256": digest(models / (model + ".bin")),
                    "model_param_sha256": digest(models / (model + ".param")),
                    "restoration": None if not restoration else {"recipe_sha256": digest(args.restoration),
                        "reference_sha256": digest(args.reference), "text": restoration.text,
                        "frames": sorted(restoration.matrices)}, "validated_full_decode": True}
        staged_json = work / (name + ".json")
        staged_json.write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
        publish(staged, output); publish(staged_json, sidecar)
        print(f"Saved {output}", flush=True)
    except BaseException:
        print(f"Unfinished work retained at {work}", file=sys.stderr)
        raise
    if not args.keep_work:
        shutil.rmtree(work)  # Only the unique directory created by this call.


def parser():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    p.add_argument("assets", type=Path, help="extracted Sega CD folder containing MCD files")
    mode = p.add_mutually_exclusive_group(required=True)
    mode.add_argument("--list", action="store_true", help="list available original movie names without loading models")
    mode.add_argument("--movie", action="append", metavar="NAME", help="playlist name, e.g. sc1; repeat for multiple movies")
    p.add_argument("--output", type=Path, default=ROOT / "upscaled", help="final MP4/JSON folder; existing results are never replaced")
    p.add_argument("--work-dir", type=Path, default=ROOT / ".local/movie-work", help="parent for isolated temporary render directories")
    p.add_argument("--keep-work", action="store_true", help="keep frames, lossless audio and logs after success; failures always keep them")
    p.add_argument("--exporter", help="path to reminiscence2-export-mcd (build this CMake target first)")
    p.add_argument("--cugan", help="path to the official realcugan-ncnn-vulkan executable")
    p.add_argument("--models", type=Path, help="models-pro directory; defaults beside the CUGAN executable")
    p.add_argument("--ffmpeg", help="FFmpeg executable; otherwise PATH or imageio-ffmpeg")
    p.add_argument("--scale", type=int, choices=(2, 3), default=3, help="Pro model enlargement")
    p.add_argument("--denoise", type=int, choices=(-1, 0, 3), default=3, help="-1 conservative, 0 no denoise, 3 strong")
    p.add_argument("--restoration", type=Path, help="optional tracked-region JSON recipe; requires exactly one movie and --reference")
    p.add_argument("--reference", type=Path, help="private repaired reference image matching the recipe hash")
    return p


def main(argv=None):
    p = parser(); args = p.parse_args(argv)
    try:
        assets = args.assets.expanduser().resolve()
        if not assets.is_dir(): raise ValueError(f"Asset folder does not exist: {assets}")
        available = movies(assets)
        if not available: raise ValueError("No MCD movies found in this folder")
        if args.list:
            for i, name in enumerate(available, 1): print(f"{i:3}  {name:<12} {available[name].name}")
            return 0
        names = [n.lower().removesuffix(".mcd") for n in args.movie]
        if len(set(names)) != len(names) or any(n not in available for n in names):
            raise ValueError("Choose unique names from --list")
        if bool(args.restoration) != bool(args.reference) or (args.restoration and len(names) != 1):
            raise ValueError("A restoration requires one movie, --restoration and --reference")
        suffix = ".exe" if os.name == "nt" else ""
        exporter = executable(args.exporter, [ROOT / folder / ("reminiscence2-export-mcd" + suffix)
                              for folder in ("build/package-arm64", "build/release", "build")], "MCD exporter")
        runner = executable(args.cugan, [ROOT / ".local/movie-tools/realcugan" / ("realcugan-ncnn-vulkan" + suffix)], "Real-CUGAN")
        models = (args.models or runner.parent / "models-pro").expanduser().resolve()
        model = f"up{args.scale}x-" + {-1: "conservative", 0: "no-denoise", 3: "denoise3x"}[args.denoise]
        if not all((models / (model + ext)).is_file() for ext in (".bin", ".param")):
            raise ValueError(f"Missing Real-CUGAN Pro weights: {models / model}")
        ffmpeg_name = args.ffmpeg or shutil.which("ffmpeg")
        if not ffmpeg_name:
            import imageio_ffmpeg
            ffmpeg_name = imageio_ffmpeg.get_ffmpeg_exe()
        ffmpeg = executable(ffmpeg_name, [], "FFmpeg")
        args.output = args.output.expanduser().resolve(); args.output.mkdir(parents=True, exist_ok=True)
        args.work_dir = args.work_dir.expanduser().resolve(); args.work_dir.mkdir(parents=True, exist_ok=True)
        if args.restoration:
            args.restoration = args.restoration.expanduser().resolve()
            args.reference = args.reference.expanduser().resolve()
        for name in names: render(available[name], args, exporter, runner, models, ffmpeg)
        return 0
    except (OSError, ValueError, ImportError, subprocess.SubprocessError, wave.Error) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("Movie processing interrupted.", file=sys.stderr)
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
