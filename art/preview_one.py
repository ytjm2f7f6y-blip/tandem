"""A quick flat preview of the one-panel layout (no Blender): the print masks over the powder coat,
with plain shapes for the knobs, keys, faders, lamps and windows. For checking a layout in seconds.

    TD_LAYOUT=layout_one TD_BUILD=/tmp/tx python3 textures_one.py && python3 preview_one.py /tmp/tx out.png [scale]
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
os.environ.setdefault("TD_LAYOUT", "layout_one")
import layout_one as L  # noqa: E402
import theme  # noqa: E402
from PIL import Image, ImageDraw, ImageFont  # noqa: E402

src, out = sys.argv[1], sys.argv[2]
S = float(sys.argv[3]) if len(sys.argv) > 3 else 2.0
T = theme.THEMES["one"]


def rgb(c):
    return tuple(int(round(255 * (v ** (1 / 2.2)))) for v in c[:3])


W, H = int(L.W * S), int(L.H * S)
im = Image.new("RGB", (W, H), rgb(T["panel"]))
d = ImageDraw.Draw(im)
d.rectangle([0, 0, L.CHEEK * S, H], fill=rgb(T["cheek"]))
d.rectangle([W - L.CHEEK * S, 0, W, H], fill=rgb(T["cheek"]))
inks = [("legend_ink.png", rgb(T["ink"])), ("shift_ink.png", rgb(T["shift_ink"]))]
inks += [(f"stripe_{b}.png", rgb(T["caps"][b])) for b in L.STRIPE_ORDER]
for name, col in inks:
    p = os.path.join(src, name)
    if os.path.exists(p):
        m = Image.open(p).convert("L").resize((W, H), Image.LANCZOS)
        im.paste(Image.new("RGB", (W, H), col), (0, 0), m)
font = ImageFont.truetype(os.path.join(os.path.dirname(os.path.abspath(__file__)), "fonts", "BarlowSemiCondensed-SemiBold.ttf"), int(11 * S))


def P(x, y):
    return x * S, y * S


def circle(x, y, r, fill, outline=None):
    d.ellipse([(x - r) * S, (y - r) * S, (x + r) * S, (y + r) * S], fill=fill, outline=outline)


def window(spec, text=None):
    x, y, w, h = spec["x"], spec["y"], spec["w"], spec["h"]
    d.rounded_rectangle([(x - w / 2) * S, (y - h / 2) * S, (x + w / 2) * S, (y + h / 2) * S], radius=4 * S, fill=(26, 24, 22))
    d.rounded_rectangle([(x - w / 2 + 4) * S, (y - h / 2 + 4) * S, (x + w / 2 - 4) * S, (y + h / 2 - 4) * S], radius=3 * S, fill=(12, 26, 18))
    if text:
        d.text(P(x, y), text, fill=(255, 190, 90), font=font, anchor="mm")


caps = {k: rgb(v) for k, v in T["caps"].items()}
key_col = rgb(T["key"])
for c in L.CONTROLS:
    t, x, y = c["type"], c["x"], c["y"]
    if y > L.H_PANEL:
        continue
    box = L.box_of(x, y)
    if t == "knob":
        r = L.KNOBS[c["kind"]]
        circle(x + 1.5, y + 2, r, (60, 80, 95))
        circle(x, y, r, (24, 24, 26))
        circle(x, y, r * (0.5 if c["kind"] == "ring" else 0.62), caps.get(box, (200, 60, 50)))
        a = math.radians(L.set_angle(c))
        d.line([P(x + math.sin(a) * r * 0.3, y - math.cos(a) * r * 0.3), P(x + math.sin(a) * r * 0.95, y - math.cos(a) * r * 0.95)],
               fill=(245, 245, 240), width=max(1, int(1.6 * S)))
        if c["kind"] == "ring":
            for i, ang in enumerate(c["steps"]):
                aa = math.radians(ang)
                circle(x + math.sin(aa) * L.RING_LED_R, y - math.cos(aa) * L.RING_LED_R, 1.6, (230, 40, 30) if i == c["set"] else (70, 20, 20))
    elif t == "key":
        w, h = c["w"], c["h"]
        fill = rgb(T["shift_cap"]) if c.get("cap") == "shift" else key_col
        d.rounded_rectangle([(x - w / 2 + 1) * S, (y - h / 2 + 1.5) * S, (x + w / 2 + 1) * S, (y + h / 2 + 1.5) * S], radius=3 * S, fill=(60, 80, 95))
        d.rounded_rectangle([(x - w / 2) * S, (y - h / 2) * S, (x + w / 2) * S, (y + h / 2) * S], radius=3 * S, fill=fill)
        if c.get("led"):
            circle(x, y - h / 2 - 7.5, 1.6, (230, 40, 30) if c["set"] else (70, 20, 20))
        if c.get("lamps"):
            for i, lx in enumerate(L.lamp_xs(c)):
                circle(lx, L.lamp_y(c), 1.6, (230, 40, 30) if i in c["lit"] else (70, 20, 20))
    elif t == "fader":
        tr = c["travel"]
        pw = L.FADER_PLATE_W
        d.rounded_rectangle([(x - pw / 2) * S, (y - tr / 2 - 16) * S, (x + pw / 2) * S, (y + tr / 2 + 16) * S], radius=2 * S, fill=(180, 184, 186))
        d.rounded_rectangle([(x - 1.6) * S, (y - tr / 2 - 3) * S, (x + 1.6) * S, (y + tr / 2 + 3) * S], radius=1 * S, fill=(20, 20, 20))
        cw, ch = L.FADER_CAP
        ty = y + tr / 2 - tr * c["set"]
        d.rounded_rectangle([(x - cw / 2) * S, (ty - ch / 2) * S, (x + cw / 2) * S, (ty + ch / 2) * S], radius=2.5 * S, fill=(28, 28, 30))
        d.rectangle([(x - cw / 2 + 2) * S, (ty - ch / 2 + 2) * S, (x + cw / 2 - 2) * S, (ty - ch / 2 + 9) * S], fill=caps.get(box))
        d.line([P(x - cw / 2 + 2, ty), P(x + cw / 2 - 2, ty)], fill=(250, 250, 250), width=max(1, int(1.2 * S)))
    elif t == "lamp":
        circle(x, y, c["r"], (230, 40, 30) if c["on"] else (70, 20, 20))
window(L.DISPLAY, "MOD  MATRIX 1        LFO 2 > Pitch")
window(L.TYPE_WIN, L.TYPE_SHOW)
for spec, s in zip(getattr(L, "PATCH_WINS", []), getattr(L, "PATCH_SHOW", [])):
    window(spec, "◂   " + s + "   ▸")
im.save(out)
print("wrote", out, im.size)
