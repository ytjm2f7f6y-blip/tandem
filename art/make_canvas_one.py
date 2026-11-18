"""Writes the one-panel row of the mockup canvas: the panel and its close-ups, and the hidden options.
python3 make_canvas_one.py urls.json   (urls.json: {"one.jpg": "/_blob/…", …})"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "canvas", "project")
U = json.load(open(sys.argv[1]))
W = 1768

HEAD = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>{title}</title>
<script src="./support.js"></script>
</head>
<body>
<x-dc>
<helmet>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link href="https://fonts.googleapis.com/css2?family=Hanken+Grotesk:wght@400;500;600&amp;display=swap" rel="stylesheet">
<style>
body{{margin:0}}
a{{color:#5c5b57}}a:hover{{color:#1c1c1b}}
</style>
</helmet>
<div style="width: {W}px; height: {H}px; box-sizing: border-box; padding: 64px; background: #eceae5; color: #1c1c1b; font-family: 'Hanken Grotesk', 'Helvetica Neue', Helvetica, Arial, sans-serif; display: flex; flex-direction: column; gap: 32px">
<div style="display: flex; flex-direction: column; gap: 8px; max-width: 960px">
<p style="margin: 0; font-size: 12px; line-height: 16px; font-weight: 500; letter-spacing: 0.02em; color: #5c5b57">{kicker}</p>
<h1 style="margin: 0; font-size: 24px; line-height: 28px; font-weight: 500">{h1}</h1>
<p style="margin: 0; font-size: 14px; line-height: 20px; color: #5c5b57">{blurb}</p>
</div>
"""
TAIL = """</div>
</x-dc>
<script type="text/x-dc" data-dc-script data-props='{{"$preview":{{"width":{W},"height":{H}}}}}'>
class Component extends DCLogic {{
renderVals() {{
return {{}};
}}
}}
</script>
</body>
</html>
"""
SHADOW = "box-shadow: 0 1px 0 #b9b6ae, 0 2px 3px rgba(0, 0, 0, 0.14)"


def fig(src, w, h, cap, alt):
    return (f'<figure style="margin: 0; display: flex; flex-direction: column; gap: 8px; width: {w}px">\n'
            f'<img src="{src}" alt="{alt}" style="display: block; width: {w}px; height: {h}px; object-fit: cover; border-radius: 6px; {SHADOW}">\n'
            f'<figcaption style="font-size: 12px; line-height: 16px; color: #5c5b57">{cap}</figcaption>\n</figure>')


def label(s):
    return f'<h2 style="margin: 0; font-size: 16px; line-height: 22px; font-weight: 600">{s}</h2>'


# ---- the panel
H1 = 1520
body = [f'<img src="{U["one.jpg"]}" alt="Tandem on one powder-coated panel, colour-cap knobs" style="display: block; width: 1640px; height: 800px; border-radius: 6px; {SHADOW}">',
        '<div style="display: flex; gap: 28px; align-items: flex-start">',
        fig(U["one_perf.jpg"], 585, 380, "layer keys, play mode and the arpeggiator. shift legends in red-orange", "Close-up: performance and arpeggiator"),
        fig(U["one_prog.jpg"], 671, 380, "the programmer: four columns on the display, four encoders under them", "Close-up: programmer"),
        fig(U["one_filter.jpg"], 317, 380, "filter: lamp keys show every switched setting", "Close-up: filter"),
        '</div>']
html = HEAD.format(title="Tandem: one panel", W=W, H=H1, kicker="direction c · one panel",
                   h1="One sheet, two rows",
                   blurb="Top row: the sound of the layer you are editing. Bottom row: playing both layers, and the programmer. "
                         "Satin powder coat with orange peel; navy print with titles knocked out of the header bars; a second, "
                         "red-orange screen for the shift functions.")
html += "\n".join(body) + "\n" + TAIL.format(W=W, H=H1)
open(os.path.join(OUT, "OnePanel.dc.html"), "w").write(html)

# ---- the hidden options
H2 = 900
pages = [("one_page_lfo.jpg", "shift + lfo wave: mode, phase, clock sync, user wave"),
         ("one_page_env3.jpg", "shift + vca: a whole third envelope"),
         ("one_page_voice.jpg", "shift + glide: voice mode, unison, drift, spread"),
         ("one_page_zones.jpg", "shift + play mode: split point and key ranges")]
pw = (1640 - 3 * 28) // 4
ph = round(pw * 112 / 320)
body = [label("1 · on the panel: one control for each thing you play"),
        '<p style="margin: -24px 0 0; font-size: 14px; line-height: 20px; color: #5c5b57">Above. Switched settings are lamp keys and multi-way choices are encoders ringed with lamps, so a patch recall shows everything.</p>',
        label("2 · hold shift, touch a control: its page"),
        '<div style="display: flex; gap: 28px; margin-top: -16px">',
        *[fig(U[f], pw, ph, c, "Display page: " + c) for f, c in pages],
        '</div>',
        label("3 · the drawer: set once, forget"),
        fig(U["one_open_drawer.jpg"], 1640, round(1640 * 180 / 1560),
            "tuning, bend range, velocity and pressure response, midi channels per layer, age (drift), quality, panel size. setup opens it; it slides out from under the front edge",
            "The setup drawer pulled out")]
html = HEAD.format(title="Tandem: hidden options", W=W, H=H2, kicker="direction c · advanced options",
                   h1="Three levels deep",
                   blurb="The panel holds what you play. Everything else is one shift away on the display, and the settings you change once a year live in a drawer.")
html += "\n".join(body) + "\n" + TAIL.format(W=W, H=H2)
open(os.path.join(OUT, "Hidden.dc.html"), "w").write(html)
print("wrote OnePanel.dc.html, Hidden.dc.html")
