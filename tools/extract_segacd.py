#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Extract a Flashback Sega CD CUE/BIN into a new, private assets directory.

Uses only Python's standard library. Reads MODE1/2352 or MODE1/2048 ISO9660
data and writes CD audio as lossless 44.1 kHz stereo PCM WAV, omitting pregaps.
The source disc is never modified and an existing output is never overwritten.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass, field
import hashlib
import json
from pathlib import Path, PurePosixPath, PureWindowsPath
import re
import shutil
import sys
import tempfile
import wave

MANIFEST = "reminiscence2-segacd.json"
MAX_FILES = 4096
MAX_BYTES = 1024 * 1024 * 1024
SYNC = b"\0" + b"\xff" * 10 + b"\0"


class DiscError(ValueError):
    """Invalid, unsafe or unsupported disc image."""


def safe_path(name: str) -> PurePosixPath:
    name = name.replace("\\", "/")
    parts = name.split("/")
    if (not name or PureWindowsPath(name).drive or name.startswith("/") or
        any(p in ("", ".", "..") or p.endswith((".", " ")) for p in parts) or
        any(ord(c) < 32 or c in ':*?<>|"' for c in name) or
        any(re.fullmatch(r"CON|PRN|AUX|NUL|COM[1-9¹²³]|LPT[1-9¹²³]",
                         p.split(".", 1)[0], re.I) for p in parts)):
        raise DiscError(f"Unsafe disc path: {name!r}")
    return PurePosixPath(*parts)


def frames(text: str) -> int:
    match = re.fullmatch(r"(\d+):(\d{2}):(\d{2})", text)
    if not match:
        raise DiscError(f"Invalid CUE time: {text}")
    m, s, f = map(int, match.groups())
    if s >= 60 or f >= 75:
        raise DiscError(f"Invalid CUE time: {text}")
    return (m * 60 + s) * 75 + f


@dataclass
class Track:
    number: int
    mode: str
    file: Path
    indexes: dict[int, int] = field(default_factory=dict)
    end: int = 0

    @property
    def sector_size(self) -> int:
        return 2048 if self.mode == "MODE1/2048" else 2352

    @property
    def start(self) -> int:
        return self.indexes[1]

    @property
    def sectors(self) -> int:
        return self.end - self.start


def read_cue(path: Path) -> list[Track]:
    parent = path.resolve().parent
    with path.open("rb") as cue:
        source = cue.read(1024 * 1024 + 1)
    if len(source) > 1024 * 1024:
        raise DiscError("CUE sheet exceeds 1 MiB")
    try:
        lines = source.decode("utf-8-sig").splitlines()
    except UnicodeDecodeError:
        lines = source.decode("cp1252").splitlines()
    tracks: list[Track] = []
    current_file = None
    seen_files: set[Path] = set()
    for line_number, raw in enumerate(lines, 1):
        line = raw.strip()
        if not line:
            continue
        verb = line.split()[0].upper()
        if verb in {"REM", "TITLE", "PERFORMER", "SONGWRITER", "CATALOG", "ISRC"}:
            continue
        if verb == "FILE":
            match = re.fullmatch(r'FILE\s+"([^"\r\n]+)"\s+BINARY', line, re.I)
            if not match:
                raise DiscError(f"Line {line_number}: expected FILE \"name.bin\" BINARY")
            relative = safe_path(match[1])
            current_file = (parent / relative).resolve()
            if parent not in current_file.parents or not current_file.is_file():
                raise DiscError(f"Missing or external BIN file: {relative}")
            if current_file in seen_files:
                raise DiscError("Use one FILE declaration for each BIN")
            seen_files.add(current_file)
        elif verb == "TRACK":
            match = re.fullmatch(r"TRACK\s+(\d+)\s+(AUDIO|MODE1/2352|MODE1/2048)", line, re.I)
            if not match or current_file is None:
                raise DiscError(f"Line {line_number}: unsupported/misplaced TRACK")
            number = int(match[1])
            if not 1 <= number <= 99 or (tracks and number <= tracks[-1].number):
                raise DiscError("Track numbers must increase within 1..99")
            tracks.append(Track(number, match[2].upper(), current_file))
        elif verb == "INDEX":
            match = re.fullmatch(r"INDEX\s+(\d+)\s+(\d+:\d{2}:\d{2})", line, re.I)
            if not match or not tracks or tracks[-1].file != current_file:
                raise DiscError(f"Line {line_number}: misplaced INDEX")
            index = int(match[1])
            if index not in (0, 1) or index in tracks[-1].indexes:
                raise DiscError("Only one INDEX 00 and INDEX 01 per track are supported")
            tracks[-1].indexes[index] = frames(match[2])
        elif verb in {"PREGAP", "POSTGAP"}:
            fields = line.split()
            if len(fields) != 2 or not tracks:
                raise DiscError(f"Line {line_number}: malformed gap")
            frames(fields[1])  # Generated silence, not stored in the BIN.
        else:
            raise DiscError(f"Line {line_number}: unsupported CUE directive {verb}")
    if not tracks or tracks[0].mode == "AUDIO" or sum(t.mode != "AUDIO" for t in tracks) != 1:
        raise DiscError("Expected one data track followed by optional audio tracks")
    for i, track in enumerate(tracks):
        size = track.file.stat().st_size
        if size > MAX_BYTES or size % track.sector_size:
            raise DiscError(f"Truncated or oversized BIN: {track.file.name}")
        if 1 not in track.indexes or track.indexes.get(0, track.start) > track.start:
            raise DiscError(f"Track {track.number}: missing or unordered INDEX")
        track.end = size // track.sector_size
        if i + 1 < len(tracks) and tracks[i + 1].file == track.file:
            following = tracks[i + 1]
            if following.sector_size != track.sector_size or 1 not in following.indexes:
                raise DiscError("Mixed sector sizes or missing INDEX in shared BIN")
            track.end = following.indexes.get(0, following.start)
        if not 0 <= track.indexes.get(0, track.start) <= track.start < track.end <= size // track.sector_size:
            raise DiscError(f"Track {track.number}: overlapping or out-of-file range")
    return tracks


def both_endian(data: bytes, at: int, width: int) -> int:
    if at < 0 or at + width * 2 > len(data):
        raise DiscError("Truncated ISO directory record")
    value = int.from_bytes(data[at:at + width], "little")
    if value != int.from_bytes(data[at + width:at + width * 2], "big"):
        raise DiscError("Mismatched ISO little/big-endian fields")
    return value


@dataclass(frozen=True)
class Entry:
    path: PurePosixPath
    sector: int
    size: int


class IsoDisc:
    def __init__(self, track: Track):
        self.track = track
        self.file = track.file.open("rb")
        self.volume_sectors = track.sectors
        self.volume = ""

    def close(self):
        self.file.close()

    def read(self, offset: int, length: int) -> bytes:
        if offset < 0 or length < 0 or offset + length > self.volume_sectors * 2048:
            raise DiscError("ISO extent is outside the data track")
        result = bytearray()
        while length:
            sector, skip = divmod(offset, 2048)
            self.file.seek((self.track.start + sector) * self.track.sector_size)
            data = self.file.read(self.track.sector_size)
            if len(data) != self.track.sector_size:
                raise DiscError("Truncated data sector")
            if self.track.sector_size == 2352:
                if data[:12] != SYNC or data[15] != 1:
                    raise DiscError(f"Invalid MODE1 sector at {sector}")
                data = data[16:2064]
            count = min(length, 2048 - skip)
            result.extend(data[skip:skip + count])
            offset += count
            length -= count
        return bytes(result)

    def entries(self) -> list[Entry]:
        primary = None
        for sector in range(16, min(80, self.volume_sectors)):
            block = self.read(sector * 2048, 2048)
            if block[1:7] != b"CD001\x01":
                raise DiscError("Not an ISO9660 data track")
            if block[0] == 1:
                primary = block
                break
            if block[0] == 255:
                break
        if primary is None or both_endian(primary, 128, 2) != 2048:
            raise DiscError("Missing ISO9660 primary volume or unsupported block size")
        size = both_endian(primary, 80, 4)
        if not 17 <= size <= self.track.sectors:
            raise DiscError("Invalid ISO volume size")
        self.volume_sectors = size
        self.volume = primary[40:72].decode("ascii").strip()
        root = primary[156:156 + primary[156]]
        queue = [(PurePosixPath(), both_endian(root, 2, 4), both_endian(root, 10, 4), 0)]
        visited, names = set(), set()
        result = []
        total = records = 0
        while queue:
            parent, sector, length, depth = queue.pop(0)
            if sector in visited or depth > 16 or length > 4 * 1024 * 1024:
                raise DiscError("Cyclic, oversized or deeply nested ISO directory")
            visited.add(sector)
            data = self.read(sector * 2048, length)
            at = 0
            while at < len(data):
                length = data[at]
                if not length:
                    at = (at // 2048 + 1) * 2048
                    continue
                record = data[at:at + length]
                if length < 34 or len(record) != length or at % 2048 + length > 2048:
                    raise DiscError("Truncated ISO directory entry")
                at += length
                records += 1
                if records > MAX_FILES:
                    raise DiscError("Too many ISO entries")
                if 33 + record[32] > length or record[1] or record[26] or record[27] or record[25] & ~3:
                    raise DiscError("Unsupported ISO directory entry layout")
                raw_name = record[33:33 + record[32]]
                if raw_name in (b"\0", b"\1"):
                    continue
                name = raw_name.decode("ascii").split(";", 1)[0]
                path = parent / safe_path(name)
                key = path.as_posix().casefold()
                if key in names:
                    raise DiscError(f"Duplicate ISO filename: {path}")
                names.add(key)
                extent, count = both_endian(record, 2, 4), both_endian(record, 10, 4)
                if extent * 2048 + count > self.volume_sectors * 2048:
                    raise DiscError(f"File outside ISO volume: {path}")
                if record[25] & 2:
                    queue.append((path, extent, count, depth + 1))
                else:
                    total += count
                    if total > MAX_BYTES:
                        raise DiscError("ISO file total exceeds 1 GiB")
                    result.append(Entry(path, extent, count))
        return sorted(result, key=lambda e: str(e.path))


def extract(cue: Path, destination: Path, *, list_only=False, audio_endian="little") -> dict:
    if audio_endian not in ("little", "big"):
        raise DiscError("Audio byte order must be little or big")
    tracks = read_cue(cue)
    disc = IsoDisc(tracks[0])
    staging = None
    try:
        entries = disc.entries()
        names = {str(e.path).upper() for e in entries}
        if any(n == MANIFEST.upper() or n == "MUSIC" or n.startswith("MUSIC/") for n in names):
            raise DiscError("ISO files collide with generated music or manifest paths")
        if not {"DATA/LEVEL1.LEV", "DATA/VOICE.VCE", "SC1.MCD", "SC2.MCD"} <= names:
            raise DiscError("This disc is not the supported Flashback Sega CD layout")
        report = {"format": 1, "edition": "segacd", "volume": disc.volume,
                  "files": [{"path": str(e.path), "size": e.size} for e in entries],
                  "audio": [{"track": t.number, "seconds": t.sectors / 75,
                             "pregap_sectors": t.start - t.indexes.get(0, t.start)}
                            for t in tracks if t.mode == "AUDIO"]}
        if list_only:
            return report
        if destination.exists() or destination.is_symlink():
            raise DiscError(f"Output already exists; choose a new directory: {destination}")
        destination.parent.mkdir(parents=True, exist_ok=True)
        staging = Path(tempfile.mkdtemp(prefix=".segacd-", dir=destination.parent))
        for entry, metadata in zip(entries, report["files"]):
            path = staging / entry.path
            path.parent.mkdir(parents=True, exist_ok=True)
            digest = hashlib.sha256()
            with path.open("xb") as out:
                for pos in range(0, entry.size, 64 * 2048):
                    data = disc.read(entry.sector * 2048 + pos, min(64 * 2048, entry.size - pos))
                    out.write(data)
                    digest.update(data)
            metadata["sha256"] = digest.hexdigest()
        (staging / "music").mkdir()
        for track, metadata in zip(tracks[1:], report["audio"]):
            name = f"music/track{track.number:02d}.wav"
            with track.file.open("rb") as source, wave.open(str(staging / name), "wb") as out:
                source.seek(track.start * 2352)
                out.setparams((2, 2, 44100, 0, "NONE", "not compressed"))
                remaining = track.sectors * 2352
                while remaining:
                    data = source.read(min(remaining, 2352 * 75))
                    if not data:
                        raise DiscError("Truncated audio track")
                    remaining -= len(data)
                    if audio_endian == "big":
                        swapped = bytearray(len(data))
                        swapped[0::2], swapped[1::2] = data[1::2], data[0::2]
                        data = swapped
                    out.writeframesraw(data)
            metadata["path"] = name
            digest = hashlib.sha256()
            with (staging / name).open("rb") as wav:
                for data in iter(lambda: wav.read(1024 * 1024), b""):
                    digest.update(data)
            metadata["sha256"] = digest.hexdigest()
        report["cue_sha256"] = hashlib.sha256(cue.read_bytes()).hexdigest()
        report["audio_source_byte_order"] = audio_endian
        (staging / MANIFEST).write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        if destination.exists() or destination.is_symlink():
            raise DiscError("Output appeared during extraction; refusing to replace it")
        staging.rename(destination)
        staging = None
        return report
    finally:
        disc.close()
        if staging is not None:
            shutil.rmtree(staging)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("cue", type=Path, help="CUE sheet beside its BIN file(s)")
    parser.add_argument("--output", type=Path, help="New assets directory (required unless --list)")
    parser.add_argument("--list", action="store_true", help="Inspect files/tracks without writing output")
    parser.add_argument("--json", action="store_true", help="Print machine-readable extraction inventory")
    parser.add_argument("--audio-endian", choices=("little", "big"), default="little",
                        help="BIN audio sample byte order; standard CUE/BIN is little-endian")
    args = parser.parse_args()
    if not args.list and args.output is None:
        parser.error("--output is required for extraction")
    try:
        report = extract(args.cue, args.output, list_only=args.list, audio_endian=args.audio_endian)
    except (OSError, ValueError, UnicodeError, wave.Error) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1
    if args.json:
        print(json.dumps(report, indent=2))
    else:
        print(f"Flashback Sega CD: {len(report['files'])} files, {len(report['audio'])} audio tracks")
        if args.list:
            for entry in report["files"]:
                print(f"  {entry['size']:10d}  {entry['path']}")
        for track in report["audio"]:
            print(f"  CD track {track['track']:02d}: {track['seconds']:.2f} s (pregap removed)")
        if not args.list:
            print(f"Extracted to {args.output}; source disc unchanged.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
