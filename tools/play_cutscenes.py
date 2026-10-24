#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Browse original Flashback polygon cutscenes with REminiscence2 (Python 3.9+).

Python supplies the terminal interface; the local engine supplies the catalog,
edition decoding, graphics, subtitles and audio. No assets or JS are downloaded.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
from typing import Sequence


REPO_ROOT = Path(__file__).resolve().parent.parent


class PlayerError(Exception):
    """An actionable setup, asset or playback error."""


@dataclass(frozen=True)
class Cutscene:
    name: str
    title: str
    parts: int


def find_engine(explicit: str | None = None) -> Path:
    """Resolve paths before launching, so spaces and caller directories are safe."""
    names = ("reminiscence2.exe", "reminiscence2", "fb.exe", "fb") if os.name == "nt" else ("reminiscence2", "fb")
    if explicit:
        supplied = Path(explicit).expanduser()
        resolved = supplied if supplied.is_file() else Path(shutil.which(explicit) or supplied)
        candidates = [resolved]
    else:
        candidates = [REPO_ROOT / folder / name for folder in ("build/package-arm64", "build/release", "build", ".") for name in names]
        candidates += [Path(found) for name in names if (found := shutil.which(name))]
    for candidate in candidates:
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return candidate.resolve()
    raise PlayerError(
        "REminiscence2 executable not found or not executable. Build it with "
        "'cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release' and "
        "'cmake --build build/release --parallel', or pass --engine=/path/to/reminiscence2."
    )


def decode_catalog(text: str) -> tuple[str, list[Cutscene]]:
    """Require our versioned JSON protocol; an old binary's help is not a catalog."""
    try:
        data = json.loads(text)
        if not isinstance(data, dict) or type(data.get("schema")) is not int or data["schema"] != 1:
            raise ValueError("unsupported catalog schema")
        edition = data["edition"]
        rows = data["cutscenes"]
        if not isinstance(edition, str) or not isinstance(rows, list) or len(rows) > 256:
            raise ValueError("invalid catalog")
        scenes = []
        names = set()
        for row in rows:
            name, title, parts = row["name"], row["title"], row["parts"]
            if (not isinstance(name, str) or not re.fullmatch(r"[a-z0-9_]{1,16}", name)
                    or name in names or not isinstance(title, str) or not title.isprintable()
                    or type(parts) is not int or not 1 <= parts <= 65535):
                raise ValueError("invalid cutscene entry")
            names.add(name)
            scenes.append(Cutscene(name, title, parts))
        return edition, scenes
    except (ValueError, TypeError, KeyError) as exc:
        raise PlayerError("The engine returned an invalid cutscene catalog. Rebuild REminiscence2 from this tree.") from exc


def read_catalog(command: Sequence[str]) -> tuple[str, list[Cutscene]]:
    try:
        result = subprocess.run(
            [*command, "--list-cutscenes"], capture_output=True, text=True,
            encoding="utf-8", errors="replace", timeout=30, check=False,
        )
    except subprocess.TimeoutExpired as exc:
        raise PlayerError("Inspecting the assets timed out after 30 seconds.") from exc
    except OSError as exc:
        raise PlayerError(f"Cannot run the engine: {exc}") from exc
    if result.returncode:
        detail = result.stderr.strip() or result.stdout.strip() or f"exit status {result.returncode}"
        raise PlayerError(f"Cannot inspect the assets:\n{detail}\nRebuild the engine if it does not recognize --list-cutscenes.")
    if result.stderr:
        print(result.stderr.rstrip(), file=sys.stderr)
    return decode_catalog(result.stdout)


def select(text: str, scenes: Sequence[Cutscene], *, numbers: bool = False) -> tuple[Cutscene, int]:
    name, separator, part_text = text.strip().lower().partition(":")
    if numbers and name.isascii() and name.isdecimal():
        index = int(name)
        scene = scenes[index - 1] if 1 <= index <= len(scenes) else None
    else:
        scene = next((entry for entry in scenes if entry.name == name), None)
    if scene is None:
        raise PlayerError("Unknown selection. Choose a listed name or menu number.")
    if separator and (not re.fullmatch(r"[0-9]{1,5}", part_text) or int(part_text) >= scene.parts):
        raise PlayerError(f"{scene.name} has parts 0 through {scene.parts - 1}.")
    return scene, int(part_text) if separator else 0


def print_menu(edition: str, scenes: Sequence[Cutscene]) -> None:
    print(f"\nREminiscence2 cutscene player — {edition}")
    print(f"{'No.':>3}  {'Name':<12} {'Parts':<7} Description")
    for index, scene in enumerate(scenes, 1):
        parts = "0" if scene.parts == 1 else f"0–{scene.parts - 1}"
        print(f"{index:>3}  {scene.name:<12} {parts:<7} {scene.title}")
    print(f"\n{len(scenes)} files, {sum(scene.parts for scene in scenes)} selectable parts.")


def play(command: Sequence[str], scene: Cutscene, part: int) -> int:
    print(f"\nPlaying {scene.name}:{part} — {scene.title}", flush=True)
    print("Backspace / Tab / Escape: return to menu. Space / A-Z: skip (enabled by default). Enter / arrows: advance panels.", flush=True)
    try:
        # An argument vector deliberately avoids shell interpolation of paths.
        result = subprocess.run([*command, f"--play-cutscene={scene.name}:{part}"], check=False)
    except OSError as exc:
        raise PlayerError(f"Cannot start playback: {exc}") from exc
    if result.returncode:
        print(f"Playback exited with status {result.returncode}.", file=sys.stderr)
    return 0 if result.returncode == 0 else 1


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(
        description="List and play original Flashback polygon cutscenes in a terminal menu.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""Examples:
  python3 tools/play_cutscenes.py /path/to/prepared-amiga
  python3 tools/play_cutscenes.py /path/to/DOS/DATA --list
  python3 tools/play_cutscenes.py /path/to/assets --play intro
  python3 tools/play_cutscenes.py /path/to/assets --play chute:1 --fullscreen

Requirements: Python 3.9+ (standard library only) and this project's compiled
REminiscence2 executable. Game files stay local and are read without modification.
For Amiga WHDLoad LHA archives, first run tools/extract_amiga.py. DOS assets use
CMD/POL pairs; Macintosh assets need a FLASHBACK.BIN or FLASHBACK.RSRC resource
fork. Uses the engine's archive support and edition detection. Supplied GOG DOS data has
regression coverage; Macintosh CD assets still need real-asset validation. SEQ movies, SET animations, 3DO videos,
disk images and installers are outside this polygon player's scope.

Interactive use: choose a number or name, optionally followed by :PART; part
numbers start at zero. A multi-part file prompts for its part if omitted.
After each part you return to the menu. Type q or press Ctrl+C to quit.
During playback Backspace, Tab or Escape skips the part. Space or A-Z also skips
by default; saved Enhancements options control logos and other scenes separately.
Closing the playback window also returns to the menu. Enter and arrow keys advance
interactive panels such as the journal. --play runs once and exits instead.
""",
    )
    result.add_argument("assets", type=Path, help="extracted game folder or an edition library (subfolders are searched)")
    result.add_argument("--asset-set", help="edition ID from the engine --list-assets output, or an unambiguous shorthand")
    result.add_argument("--music-source", help="independent soundtrack ID, or native for this edition")
    result.add_argument("--engine", help="compiled REminiscence2 path; otherwise search the repository builds and PATH")
    mode = result.add_mutually_exclusive_group()
    mode.add_argument("--list", action="store_true", help="print the catalog and exit without opening a window")
    mode.add_argument("--json", action="store_true", help="print a machine-readable catalog and exit")
    mode.add_argument("--play", metavar="NAME[:PART]", help="play one named part and exit; default part is 0")
    result.add_argument("--filter", default="", metavar="TEXT", help="show names or descriptions containing TEXT (ignores case)")
    result.add_argument("--language", type=str.upper, choices=("EN", "FR", "DE", "SP", "IT", "JP", "RU"), help="subtitle language; default: engine auto-detection")
    result.add_argument("--tunepath", type=Path, help="explicit music folder; default: only the selected edition")
    result.add_argument("--config", type=Path, help="fb.cfg compatibility file; default: repository fb.cfg")
    result.add_argument("--fullscreen", action="store_true", help="play full screen; default: a window")
    result.add_argument("--scale", type=int, choices=range(1, 7), default=3, metavar="1..6", help="nearest-neighbor window scale; default: 3 (Mac native size is already doubled)")
    return result


def main(argv: Sequence[str] | None = None) -> int:
    arguments = parser()
    args = arguments.parse_args(argv)
    assets = args.assets.expanduser().resolve()
    if not assets.is_dir():
        arguments.error(f"Asset folder does not exist: {assets}")
    try:
        engine = find_engine(args.engine)
        command = [str(engine), f"--datapath={assets}"]
        if args.tunepath:
            tunes = args.tunepath.expanduser().resolve()
            if not tunes.is_dir():
                raise PlayerError(f"Music folder does not exist: {tunes}")
            command.append(f"--tunepath={tunes}")
        if args.asset_set:
            command.append(f"--asset-set={args.asset_set}")
        if args.music_source:
            command.append(f"--music-source={args.music_source}")
        config = args.config.expanduser().resolve() if args.config else REPO_ROOT / "fb.cfg"
        if args.config or config.is_file():
            command.append(f"--config={config}")
        if args.language:
            command.append(f"--language={args.language}")
        edition, scenes = read_catalog(command)
        scenes = [scene for scene in scenes if args.filter.casefold() in f"{scene.name} {scene.title}".casefold()]
        if args.json:
            print(json.dumps({"schema": 1, "edition": edition, "cutscenes": [scene.__dict__ for scene in scenes]}, ensure_ascii=True))
            return 0
        if not scenes:
            raise PlayerError("No matching polygon cutscenes found. Check the asset folder and --filter.")
        if args.list:
            print_menu(edition, scenes)
            return 0
        command.append(f"--scaler=point@{args.scale}")
        if not args.fullscreen:
            command.append("--window")
        if args.play:
            scene, part = select(args.play, scenes)
            return play(command, scene, part)
        if not sys.stdin.isatty():
            raise PlayerError("Interactive selection needs a terminal. Use --list, --json or --play NAME[:PART].")
        while True:
            print_menu(edition, scenes)
            answer = input("\nChoose a number or name[:part], or q to quit: ").strip()
            if answer.lower() in ("q", "quit", "exit"):
                return 0
            if not answer:
                continue
            try:
                scene, part = select(answer, scenes, numbers=True)
                if ":" not in answer and scene.parts > 1:
                    answer_part = input(f"Part for {scene.name} (0–{scene.parts - 1}, Enter = 0, q = menu): ").strip()
                    if answer_part.lower() == "q":
                        continue
                    scene, part = select(f"{scene.name}:{answer_part or '0'}", scenes)
                play(command, scene, part)
            except PlayerError as exc:
                print(f"\n{exc}", file=sys.stderr)
    except (KeyboardInterrupt, EOFError):
        print("\nCutscene player closed.")
        return 0
    except PlayerError as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
