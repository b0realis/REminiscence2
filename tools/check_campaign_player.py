#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Run story -> authored named exit -> native movie -> ending through the real player."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("assets", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    project = json.loads((root / "examples/editor/workshop/project.r2project.json").read_text())
    module = json.loads((root / "examples/editor/module-lab/project.r2project.json").read_text())["modules"][0]
    node = module["nodes"][0]
    node.update(id="start", name="On start", kind=21, at=project["spawn"], requiredItem="", initiallyActive=False)
    node["visuals"] = [dict(art=dict(file="", x=0, y=0, w=16, h=24), frames=1, fps=6, loop=True) for _ in range(2)]
    finish = dict(node, id="exit", name="Named exit", kind=20, text="secret")
    module.update(id="exit-module", nodes=[node, finish], assets=[], outputs=[],
                  wires=[dict(from_="start", to="exit", signal=1, input=0)])
    module["wires"][0]["from"] = module["wires"][0].pop("from_")
    for room in project["rooms"]:
        for art in room["art"]:
            art["file"] = ""
    project.update(schema=7, modules=[module])
    with tempfile.TemporaryDirectory(prefix="reminiscence2-campaign-") as folder:
        path = Path(folder)
        (path / "level.r2project.json").write_text(json.dumps(project))
        def route(target, key="complete"):
            return dict(key=key, label="Continue", to=target)
        campaign = dict(schema=1, name="Player regression", entry="intro", stages=[
            dict(id="intro", name="Intro text", kind=2, elements=[dict(text="Arrival.", effect=1)], advance=50, routes=[route("level")]),
            dict(id="level", name="Exit test", kind=0, level="level.r2project.json", routes=[route("movie", "secret")]),
            dict(id="movie", name="Movie test", kind=1, movie="teleport", routes=[route("end")]),
            dict(id="end", name="End text", kind=4, elements=[dict(text="Finished.", effect=2)], advance=50),
        ])
        (path / "campaign.r2campaign.json").write_text(json.dumps(campaign))
        env = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", SDL_RENDER_DRIVER="software")
        result = subprocess.run([str(args.binary.resolve()), f"--campaign={path / 'campaign.r2campaign.json'}", f"--datapath={args.assets.resolve()}", f"--savepath={path / 'saves'}", "--campaign-new"], env=env, capture_output=True, text=True, timeout=40)
        if result.returncode:
            print(result.stdout)
            print(result.stderr)
        else:
            saved = list((path / "saves/campaigns").glob("*.r2save.json"))
            if len(saved) != 1 or not json.loads(saved[0].read_text())["finished"]:
                raise AssertionError("Campaign did not atomically save its ending")
            resumed = subprocess.run([str(args.binary.resolve()), f"--campaign={path / 'campaign.r2campaign.json'}",
                f"--datapath={args.assets.resolve()}", f"--savepath={path / 'saves'}", "--campaign-resume"],
                env=env, capture_output=True, text=True, timeout=10)
            if resumed.returncode:
                raise AssertionError(resumed.stderr)
            print("Story, named exit, movie/audio, atomic completion save and resumed ending passed")
        return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
