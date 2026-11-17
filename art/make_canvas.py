"""Writes the mockup canvas's artboards (art/canvas/project) from the uploaded image urls.
python3 make_canvas.py urls.json   (urls.json: {"mockup_ivory.jpg": "/_blob/…", …})"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "canvas", "project")
URLS = json.load(open(sys.argv[1]))

W, H = 1768, 1600

VARIANTS = [
    ("Main.dc.html", "ivory", "direction a", "Ivory and walnut",
     "Cream bakelite knobs with fluted grips, cream push keys with red lamps, navy legends and oiled walnut end cheeks. "
     "The warmest of the three: a 1960s hi-fi in a powder-blue case.",
     [("panel", "baby-blue powder coat"), ("rails", "lighter blue powder coat"), ("knobs", "cream bakelite, fluted"),
      ("keys", "cream, red lamps"), ("display", "green phosphor"), ("ends", "oiled walnut")]),
    ("Machined.dc.html", "machined", "direction b", "Machined and navy",
     "Knurled aluminium knobs with black anodised tops, graphite rubber keys with amber lamps, navy rails and a cyan display. "
     "The most precise of the three: lab equipment in a powder-blue case.",
     [("panel", "baby-blue powder coat"), ("rails", "navy powder coat"), ("knobs", "knurled aluminium, black top"),
      ("keys", "graphite rubber, amber lamps"), ("display", "cyan"), ("ends", "navy")]),
    ("Capped.dc.html", "capped", "direction c", "Colour caps",
     "Black knobs with a coloured cap for each section (orange filter, teal lfo, mustard envelopes, red control), "
     "white keys with red lamps, cream rails and an amber display. The most playful, and the caps map the panel.",
     [("panel", "baby-blue powder coat"), ("rails", "cream powder coat"), ("knobs", "black, colour cap per section"),
      ("keys", "white, red lamps"), ("display", "amber"), ("ends", "cream")]),
]

CLOSE = [("knobs", "filter knobs"), ("control", "display, page keys and patch keys"), ("perf", "arpeggiator keys and clock")]

PAGE = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>Tandem: {title}</title>
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
<div style="display: flex; justify-content: space-between; align-items: flex-end; gap: 48px">
<div style="display: flex; flex-direction: column; gap: 8px; max-width: 760px">
<p style="margin: 0; font-size: 12px; line-height: 16px; font-weight: 500; letter-spacing: 0.02em; color: #5c5b57">{kicker}</p>
<h1 style="margin: 0; font-size: 24px; line-height: 28px; font-weight: 500">{title}</h1>
<p style="margin: 0; font-size: 14px; line-height: 20px; color: #5c5b57">{blurb}</p>
</div>
<dl style="margin: 0; display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); column-gap: 32px; row-gap: 8px; width: 760px">
{specs}
</dl>
</div>
<img src="{full}" alt="Tandem faceplate, {title_lc} finish" style="display: block; width: 1640px; height: 760px; border-radius: 6px; box-shadow: 0 1px 0 #b9b6ae, 0 2px 3px rgba(0, 0, 0, 0.14)">
<div style="display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 28px">
{closes}
</div>
</div>
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

SPEC = """<div style="display: flex; flex-direction: column; gap: 2px">
<dt style="font-size: 12px; line-height: 16px; font-weight: 500; letter-spacing: 0.02em; color: #5c5b57">{k}</dt>
<dd style="margin: 0; font-size: 14px; line-height: 20px">{v}</dd>
</div>"""

FIG = """<figure style="margin: 0; display: flex; flex-direction: column; gap: 8px">
<img src="{src}" alt="Close-up: {cap}" style="display: block; width: 528px; height: 528px; object-fit: cover; border-radius: 6px; box-shadow: 0 1px 0 #b9b6ae, 0 2px 3px rgba(0, 0, 0, 0.14)">
<figcaption style="font-size: 12px; line-height: 16px; color: #5c5b57">{cap}</figcaption>
</figure>"""

boards = {}
for i, (fname, theme, kicker, title, blurb, specs) in enumerate(VARIANTS):
    html = PAGE.format(W=W, H=H, kicker=kicker, title=title, title_lc=title.lower(), blurb=blurb,
                       specs="\n".join(SPEC.format(k=k, v=v) for k, v in specs),
                       full=URLS[f"mockup_{theme}.jpg"],
                       closes="\n".join(FIG.format(src=URLS[f"close_{theme}_{n}.jpg"], cap=c) for n, c in CLOSE))
    open(os.path.join(OUT, fname), "w").write(html)
    boards[fname] = {"x": i * (W + 80), "y": 0, "w": W, "h": H, "title": f"{title}"}
print("wrote", list(boards))
json.dump(boards, open(os.path.join(HERE, "canvas", "boards.json"), "w"), indent=1)
