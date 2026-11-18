"""Writes the Additions artboard of the mockup canvas: the research's items 3, 4, 11 and 12 as the plugin
draws them (snapshots of its window, not renders).
python3 make_canvas_additions.py urls.json   (urls.json: {"add_panel.jpg": "/_blob/…", …})"""
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "canvas", "project")
U = json.load(open(sys.argv[1]))
src = open(os.path.join(HERE, "make_canvas_one.py")).read()
HEAD = re.search(r'HEAD = """(.*?)"""', src, re.S).group(1)
TAIL = re.search(r'TAIL = """(.*?)"""', src, re.S).group(1)
SHADOW = "box-shadow: 0 1px 0 #b9b6ae, 0 2px 3px rgba(0, 0, 0, 0.14)"
W, H = 1768, 1900


def fig(src, w, h, cap, alt):
    return (f'<figure style="margin: 0; display: flex; flex-direction: column; gap: 8px; width: {w}px">\n'
            f'<img src="{src}" alt="{alt}" style="display: block; width: {w}px; height: {h}px; object-fit: cover; border-radius: 6px; {SHADOW}">\n'
            f'<figcaption style="font-size: 12px; line-height: 16px; color: #5c5b57">{cap}</figcaption>\n</figure>')


body = [f'<img src="{U["add_panel.jpg"]}" alt="The plugin window with the new controls, playing the dice keys patch" '
        f'style="display: block; width: 1640px; height: 800px; border-radius: 6px; {SHADOW}">',
        '<div style="display: flex; gap: 28px; align-items: flex-start">',
        fig(U["add_perf.jpg"], 317, 380, "voice mode per layer: a key steps it, a lamp shows it. layer 2's row waits for stage 2", "Close-up: voice mode"),
        fig(U["add_arp.jpg"], 253, 380, "dice rolls a pattern of note-to-note differences; vary sets how much of it plays", "Close-up: dice and vary"),
        fig(U["add_lmix.jpg"], 248, 380, "voice lamps between the layer columns: one per voice, lit by its level", "Close-up: voice lamps"),
        fig(U["add_arc.jpg"], 581, 380, "a modulated knob: an amber dot at the foot of its scale, always; while touched or assigning, an arc for its range and a tick where the newest voice has it", "Close-up: modulation arc on the cutoff"),
        '</div>',
        '<div style="display: flex; gap: 28px; align-items: flex-start">',
        fig(U["add_assign.jpg"], 528, 299, "assign lit: turn a knob and a free slot opens for it, the turn setting the amount", "Programmer with ASSIGN lit"),
        fig(U["add_slot.jpg"], 528, 299, "one slot a page: source, destination, amount and via. here the wheel brings in a vibrato", "Programmer showing a matrix slot with VIA"),
        fig(U["add_curve.jpg"], 528, 299, "shift: the amount column becomes the slot's curve (linear, exp, steps, smooth)", "Programmer with SHIFT showing the curve"),
        '</div>']
html = HEAD.format(title="Tandem: additions", W=W, H=H, kicker="direction c · from the research: items 3, 4, 11 and 12",
                   h1="Modulation you can see, voices you can see, and a pair of dice",
                   blurb="As built: these are the plugin's own window, not renders. A twelve-slot matrix with VIA and curves, "
                         "an ASSIGN key, dots and arcs on modulated knobs; each layer's voice mode and voices on the panel; "
                         "DICE and VARY; and NOTE RND, a per-note random source.")
html += "\n".join(body) + "\n" + TAIL.format(W=W, H=H)
open(os.path.join(OUT, "Additions.dc.html"), "w").write(html)
print("wrote Additions.dc.html")
