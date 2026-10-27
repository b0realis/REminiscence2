#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Generate the playable Module Lab with original schematic art and node recipes."""
import argparse
import copy
import json
from pathlib import Path
from create_editor_example import make, png


def build(root):
    make(root)
    path = root / "project.r2project.json"
    project = json.loads(path.read_text())
    project.update(schema=2, id="reminiscence2-module-lab", name="Module Lab", serial=100)
    room = project["rooms"][0]
    project["rooms"] = []
    for i, name in enumerate(("Switch workshop", "Retrieval office", "Mission control")):
        r = copy.deepcopy(room)
        r.update(id=f"room-{i+1}", name=name, column=i,
                 exits=["", "", f"room-{i+2}" if i < 2 else "", f"room-{i}" if i else ""])
        project["rooms"].append(r)
    empty = {"file": "", "x": 0, "y": 0, "w": 256, "h": 224}

    def clip(file="", x=0, w=16, h=24, frames=1, loop=True):
        return dict(art=dict(file=file, x=x, y=0, w=w, h=h), frames=frames, fps=8, loop=loop)

    def node(i, name, kind, room, x, height=24):
        return dict(id=f"node-{i}", name=name, kind=kind, at=dict(room=room, x=x, y=216),
                    graphX=24 + (i-1)*240, graphY=24, width=16, height=height, layer=1,
                    visuals=[clip(), clip()], enabled=True, initiallyActive=False,
                    once=False, consumeItem=True, requiredItem="", text="", doneText="", goal=1)

    def wire(a, b, signal, action):
        return dict(**{"from": f"node-{a}"}, to=f"node-{b}", signal=signal, input=action)

    def module(i, name):
        anchor = dict(room=f"room-{i}", x=192, y=216)
        return dict(id=f"module-{i}", name=name, kind=4, a=anchor, b=anchor.copy(),
                    initiallyOpen=False, toggle=False, facingLeft=False, health=2,
                    width=16, height=16, sprite=empty.copy(), frames=1, fps=6, layer=1,
                    outputs=[], nodes=[], wires=[])

    def pixels(w, h):
        data = bytearray(w*h*4)
        def rect(x, y, rw, rh, color):
            for yy in range(max(0, y), min(h, y+rh)):
                for xx in range(max(0, x), min(w, x+rw)):
                    data[(yy*w+xx)*4:(yy*w+xx+1)*4] = bytes(color)
        return data, rect

    data, rect = pixels(80, 72)
    for frame in range(5):
        x = frame*16
        rect(x, 0, 2, 72, (116, 186, 191, 255))
        rect(x+14, 0, 2, 72, (116, 186, 191, 255))
        height = 72-frame*18
        rect(x+2, 0, 12, height, (34, 80, 99, 255))
        for y in range(4, height, 8):
            rect(x+3, y, 10, 2, (90, 167, 180, 255))
    png(root/"art/door-strip.png", 80, 72, data)
    data, rect = pixels(32, 24)
    for i, color in enumerate(((244, 166, 73, 255), (84, 237, 169, 255))):
        rect(i*16+2, 0, 12, 24, (39, 64, 78, 255))
        rect(i*16+4, 3, 8, 6, color)
        rect(i*16+5, 14, 6, 2, (166, 194, 203, 255))
    png(root/"art/button-strip.png", 32, 24, data)
    data, rect = pixels(32, 48)
    for i, color in enumerate(((113, 134, 214, 255), (129, 206, 175, 255))):
        x=i*16
        rect(x+5, 0, 7, 10, (220, 192, 153, 255))
        rect(x+3, 10, 11, 23, color)
        rect(x+4, 33, 4, 15, (40, 58, 78, 255))
        rect(x+10, 33, 4, 15, (40, 58, 78, 255))
    png(root/"art/npc-strip.png", 32, 48, data)

    modules=[]
    for i, name in enumerate(("Door circuit", "Technician's lost module", "Power restoration job"), 1):
        m=module(i, name)
        door=node(1, "Access door", 1, f"room-{i}", 192, 72)
        door["visuals"]=[clip("art/door-strip.png", 0, 16, 72),
                         clip("art/door-strip.png", 16, 16, 72, 4, False)]
        m["nodes"].append(door)
        if i==1:
            button=node(2, "Wall button", 0, "room-1", 128)
            button["visuals"]=[clip("art/button-strip.png"),clip("art/button-strip.png",16)]
            m["nodes"].append(button)
            m["wires"]=[wire(2,1,0,3)]
        elif i==2:
            item=node(2,"Signal module",3,"room-2",48,16)
            npc=node(3,"Technician",2,"room-2",128,48)
            npc.update(requiredItem="node-2", text="My signal module is missing. Bring it here and I can unlock the passage.",
                       doneText="That is the missing module. Thank you! I have opened the passage to mission control.")
            npc["visuals"]=[clip("art/npc-strip.png",0,16,48),clip("art/npc-strip.png",16,16,48)]
            m["nodes"] += [item,npc]
            m["wires"]=[wire(3,1,1,1)]
        else:
            terminal=node(2,"Mission terminal",4,"room-3",48)
            terminal.update(text="Restore the power at the orange switch. The exit will unlock when the job is complete.",
                            doneText="Power restored. Access granted. Module Lab complete!")
            terminal["visuals"]=[clip("art/button-strip.png"),clip("art/button-strip.png",16)]
            button=node(3,"Power switch",0,"room-3",128)
            button["visuals"]=copy.deepcopy(terminal["visuals"])
            m["nodes"] += [terminal,button]
            m["wires"]=[wire(3,2,0,4),wire(2,1,1,1)]
        # Put source on the left and receiving door on the right.
        m["nodes"][0]["graphX"] = 504
        m["nodes"][1]["graphX"] = 24
        if len(m["nodes"])>2: m["nodes"][2]["graphX"] = 264
        modules.append(m)
    project["modules"]=modules
    path.write_text(json.dumps(project, indent=2)+"\n")


if __name__ == "__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination",type=Path)
    build(parser.parse_args().destination)
