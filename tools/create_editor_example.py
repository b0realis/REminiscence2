#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Generate the original, redistributable editor workshop (no game assets)."""
import argparse
import json
from pathlib import Path
import struct
import zlib


def png(path, width, height, pixels):
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    raw = b"".join(b"\0" + bytes(pixels[y * width * 4:(y + 1) * width * 4]) for y in range(height))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def make(root):
    width, height = 512, 224
    back = bytearray(width * height * 4)
    front = bytearray(width * height * 4)

    def rect(pixels, x, y, w, h, color):
        for yy in range(max(0, y), min(height, y + h)):
            for xx in range(max(0, x), min(width, x + w)):
                pixels[(yy * width + xx) * 4:(yy * width + xx + 1) * 4] = bytes(color)

    rect(back, 0, 0, width, height, (13, 25, 38, 255))
    for x in range(0, width, 32):
        rect(back, x, 15, 30, 188, (20, 40, 53, 255))
        rect(back, x + 2, 18, 26, 4, (35, 66, 78, 255))
        rect(back, x + 4, 54, 22, 92, (12, 31, 43, 255))
        rect(back, x + 4, 151, 22, 38, (26, 46, 59, 255))
        rect(back, x + 8, 60, 12, 2, (48, 117, 124, 255))
    for x in (28, 284):
        rect(back, x, 33, 100, 60, (6, 19, 32, 255))
        rect(back, x + 4, 37, 92, 52, (15, 45, 65, 255))
        rect(back, x + 8, 43, 46, 2, (60, 174, 187, 255))
        rect(back, x + 8, 50, 65, 2, (50, 126, 145, 255))
    rect(back, 0, 216, width, 8, (55, 86, 99, 255))
    rect(back, 0, 216, width, 1, (143, 193, 188, 255))
    rect(back, 384, 144, 128, 72, (34, 58, 69, 255))
    rect(back, 384, 144, 128, 2, (136, 193, 181, 255))
    for x in range(392, 512, 16):
        rect(back, x, 154, 8, 48, (21, 39, 53, 255))
    # This foreground railing is deliberately visual only.
    rect(front, 67, 183, 43, 4, (114, 170, 172, 255))
    rect(front, 68, 187, 3, 37, (46, 96, 114, 255))
    rect(front, 106, 187, 3, 37, (46, 96, 114, 255))
    png(root / "art/workshop-back.png", width, height, back)
    png(root / "art/workshop-front.png", width, height, front)
    empty = {"file": "", "x": 0, "y": 0, "w": 256, "h": 224}
    rooms = []
    for i, name in enumerate(("Access hall", "Climb chamber")):
        grid = [0] * 112
        grid[96:112] = [1] * 16
        if i:
            grid[4 * 16 + 8:5 * 16] = [1] * 8
        rooms.append({"id": f"room-{i+1}", "name": name, "column": i, "row": 0,
                      "art": [{**empty, "file": "art/workshop-back.png", "x": i * 256}, empty.copy(),
                              {**empty, "file": "art/workshop-front.png", "x": i * 256}],
                      "collision": grid, "ledges": [{"column": 8, "row": 4, "facing": 1}] if i else [],
                      "exits": ["", "", "room-2" if not i else "", "room-1" if i else ""]})

    def module(ident, kind, room, x, y):
        return {"id": ident, "name": ("Access door", "Signal beacon", "Health pickup", "Crawler")[kind],
                "kind": kind, "a": {"room": room, "x": x, "y": y},
                "b": {"room": room, "x": max(8, x-64), "y": y},
                "initiallyOpen": False, "toggle": False, "facingLeft": True,
                "health": 2, "width": 16, "height": 72 if kind == 0 else 16,
                "sprite": empty.copy(), "frames": 1, "fps": 6, "layer": 1, "outputs": []}
    project = {"schema": 1, "id": "reminiscence2-editor-workshop", "name": "Editor workshop", "serial": 20,
               "rooms": rooms, "modules": [module("module-1", 0, "room-1", 192, 216),
               module("module-2", 1, "room-1", 224, 144), module("module-3", 2, "room-2", 64, 216)],
               "spawn": {"room": "room-1", "x": 32, "y": 214}, "spawnLeft": False}
    (root / "project.r2project.json").write_text(json.dumps(project, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    make(args.destination)
