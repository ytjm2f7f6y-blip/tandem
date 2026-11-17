"""Print masks for the one-panel Tandem (layout_one): what the screen printer puts down.

Navy ink: box outlines, the header bars (titles knocked out to the bare powder coat), every legend.
Shift ink (red-orange, its own screen, a hair out of register): the SHIFT functions.
Stripe inks: the five bars by the name, one screen each, in the cap colours.
Drawer ink (cream, on the navy tray): the set-and-forget settings.

    TD_LAYOUT=layout_one TD_BUILD=build_one python3 textures_one.py
"""
import os
import sys

os.environ.setdefault("TD_LAYOUT", "layout_one")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_textures as MT  # noqa: E402
from PIL import Image, ImageChops, ImageDraw, ImageFilter  # noqa: E402

L = MT.L
TEX = MT.TEX
W, H = MT.W, MT.H
OUT = MT.OUT
os.makedirs(OUT, exist_ok=True)
F = MT.font
LEG, MED, BOLD = MT.LEGEND, MT.LEGEND_MED, MT.LEGEND_BOLD
SHIFT_OFFSET = (0.16, -0.10)          # the second screen's misregistration, points


def P(v):
    return v * TEX


def text(d, xy, s, f, anchor="mm", spacing=0.0, fill=255):
    if anchor == "mm":
        MT.text_centered(d, xy, s, f, fill=fill, spacing=spacing)
        return
    x, y = P(xy[0]), P(xy[1])
    if spacing == 0:
        d.text((x, y), s, font=f, fill=fill, anchor=anchor)
        return
    widths = [f.getlength(ch) for ch in s]
    extra = spacing * f.size
    total = sum(widths) + extra * (len(s) - 1)
    cur = x - total if anchor[0] == "r" else x
    for ch, w in zip(s, widths):
        d.text((cur, y), ch, font=f, fill=fill, anchor="l" + anchor[1])
        cur += w + extra


def spaced_width(s, f, spacing):
    return (sum(f.getlength(ch) for ch in s) + spacing * f.size * (len(s) - 1)) / TEX


def rrect(d, x0, y0, x1, y1, r, **kw):
    d.rounded_rectangle([P(x0), P(y0), P(x1), P(y1)], radius=P(r), **kw)


def navy_and_shift():
    nv = Image.new("L", (W, H), 0)
    sh = Image.new("L", (W, H), 0)
    d, ds = ImageDraw.Draw(nv), ImageDraw.Draw(sh)
    T = L.TYPE
    f_head = F(BOLD, T["head"])
    f_name = F(BOLD, T["name"])
    f_num = F(LEG, T["num"])
    f_sub = F(MED, T["sub"])
    f_lamp = F(MED, T["lamp"])
    f_sec = F(BOLD, T["sec"])
    f_shift = F(BOLD, 7.4)

    # Boxes: a fine outline, a solid header bar with the title knocked out.
    for bid, title, x0, x1, (y0, y1) in L.BOXES:
        rrect(d, x0, y0, x1, y1, 5, outline=255, width=max(1, int(0.9 * TEX)))
        rrect(d, x0, y0, x1, y0 + L.HEAD_H, 5, fill=255)
        d.rectangle([P(x0), P(y0 + L.HEAD_H - 6), P(x1), P(y0 + L.HEAD_H)], fill=255)
        text(d, ((x0 + x1) / 2, y0 + L.HEAD_H / 2 + 0.6), title, f_head, spacing=0.22, fill=0)
    for t, x0, x1, y in L.SUBHEADS:
        MT.section(d, t, x0, x1, y, f_sec)

    def shift_word(xy, s):
        text(ds, (xy[0] + SHIFT_OFFSET[0], xy[1] + SHIFT_OFFSET[1]), s, f_shift, spacing=0.08)

    for c in L.CONTROLS:
        t = c["type"]
        x, y = c["x"], c["y"]
        if t == "knob" and c.get("encoder"):
            r = L.KNOBS[c["kind"]]
            for k in range(24):
                p = MT.polar(x, y, r + 5.0, k * 15)
                rr = 0.55
                d.ellipse([P(p[0] - rr), P(p[1] - rr), P(p[0] + rr), P(p[1] + rr)], fill=255)
            continue
        if t == "knob" and c["kind"] == "ring":
            r = L.KNOBS["ring"]
            for a, s in zip(c["steps"], c["texts"]):
                if s.startswith("icon:"):
                    MT.wave_icon(d, MT.polar(x, y, L.RING_ICON_R, a), s[5:])
                else:
                    text(d, MT.polar(x, y, L.RING_LABEL_R, a), s, f_num)
            ny = y + (c["name_dy"] or (r + L.NAME_GAP + 6))
            text(d, (x, ny), c["name"], f_name, spacing=0.05)
            if c.get("shift"):
                shift_word((x, ny + 10.5), c["shift"])
            continue
        if t == "knob":
            r = L.KNOBS[c["kind"]]
            for n in c["ticks"]:
                a = L.norm_angle(n)
                p0, p1 = MT.polar(x, y, r + 3, a), MT.polar(x, y, r + 7.5, a)
                d.line([(P(p0[0]), P(p0[1])), (P(p1[0]), P(p1[1]))], fill=255, width=int(0.9 * TEX))
            for lab in c["labels"]:
                text(d, MT.polar(x, y, r + L.LABEL_R, L.norm_angle(lab["n"])), lab["t"], f_num)
            ny = y + r + L.NAME_GAP
            if c["kind"] == "trim":
                ny = y + r + 19
            text(d, (x, ny), c["name"], f_name, spacing=0.05)
            if c.get("sub"):
                ny += 10
                text(d, (x, ny), c["sub"], f_sub)
            if c.get("shift"):
                shift_word((x, ny + 10.5), c["shift"])
        elif t == "fader":
            tr = c["travel"]
            top, bot = y - tr / 2, y + tr / 2
            for k in range(11):
                yy = bot - (bot - top) * k / 10
                ln = 4.5 if k in (0, 5, 10) else 2.6
                xr = x - L.FADER_PLATE_W / 2 - 1.5
                d.line([(P(xr - ln), P(yy)), (P(xr), P(yy))], fill=255, width=int(0.7 * TEX))
            if c["scale"]:
                for k, s in ((0, "0"), (5, "5"), (10, "10")):
                    yy = bot - (bot - top) * k / 10
                    d.text((P(x - L.FADER_PLATE_W / 2 - 7.5), P(yy)), s, font=f_num, fill=255, anchor="rm")
            text(d, (x, bot + 22), c["name"], f_name)
        elif t == "key":
            if c.get("lamps"):
                ly = L.lamp_y(c)
                for lx, s in zip(L.lamp_xs(c), c["lamps"]):
                    text(d, (lx, ly - 8.6), s, f_lamp, spacing=0.04)
            if c.get("label"):
                if c.get("label_side") == "right":
                    text(d, (x + c["w"] / 2 + 7, y + 0.4), c["label"], f_sub, anchor="lm", spacing=0.06)
                elif c.get("cap") == "shift":
                    shift_word((x, y + c["h"] / 2 + 9.5), c["label"])
                else:
                    f = f_sub if len(c["label"]) <= 6 else f_lamp
                    text(d, (x, y + c["h"] / 2 + 9.5), c["label"], f, spacing=0.06)
            if c.get("shift"):
                shift_word((x, y + c["h"] / 2 + 19.5), c["shift"])

    for s, x, y, size, ink, anchor in L.PRINT:
        if ink == "glyph":
            continue            # the arrows are moulded into their keys
        f = F(BOLD if ink == "shift" else MED, size)
        if ink == "shift":
            text(ds, (x + SHIFT_OFFSET[0], y + SHIFT_OFFSET[1]), s, f, anchor=anchor, spacing=0.06)
        else:
            text(d, (x, y), s, f, anchor=anchor, spacing=0.08)

    # The filter type window's label; the layer mixer's columns; the play modes' list.
    text(d, (L.TYPE_WIN["x"], L.TYPE_LABEL_Y), "FILTER TYPE", f_name, spacing=0.05)
    for i, x in enumerate(L.MIXC):
        text(d, (x, L.MIX_HEAD_Y), "LAYER " + str(i + 1), f_name, spacing=0.08)
    lx, ly = L.PLAY_LEGEND_XY
    f_leg, f_legn = F(MED, 8.0), F(BOLD, 8.0)
    for i, s in enumerate(L.PLAY_MODES):
        yy = ly + i * L.PLAY_LEGEND_STEP
        d.text((P(lx), P(yy)), str(i + 1), font=f_legn, fill=255, anchor="lm")
        d.text((P(lx + 10), P(yy)), s, font=f_leg, fill=255, anchor="lm")
    d.line([(P(lx - 5), P(ly - 6)), (P(lx - 5), P(ly + 7 * L.PLAY_LEGEND_STEP + 6))], fill=255, width=int(0.7 * TEX))

    # The top band: the maker, the type, the name.
    b = L.BRAND
    fb = F("Jost.ttf", b["size"], weight=520)
    text(d, (b["x"], b["y"]), b["text"], fb, anchor="lm", spacing=0.20)
    brand_right = b["x"] + spaced_width(b["text"], fb, 0.20)
    tp = L.TYPE_PLATE
    ft = F("Jost.ttf", tp["size"], weight=480)
    text(d, (tp["x"], tp["y"]), tp["text"], ft, anchor="lm", spacing=0.28)
    tx = tp["x"] + spaced_width(tp["text"], ft, 0.28) + 6
    d.line([(P(tx), P(tp["y"])), (P(brand_right), P(tp["y"]))], fill=255, width=int(0.55 * TEX))
    wm = L.WORDMARK
    fw = F("Jost.ttf", wm["size"], weight=600)
    text(d, (wm["x"], wm["y"]), wm["text"], fw, anchor="rm", spacing=0.16)
    wm_left = wm["x"] - spaced_width(wm["text"], fw, 0.16)
    sl = L.SUBLINE
    text(d, (sl["x"], sl["y"]), sl["text"], F(MED, sl["size"]), anchor="rm", spacing=0.14)
    nv.save(os.path.join(OUT, "legend_ink.png"))
    sh.save(os.path.join(OUT, "shift_ink.png"))
    return wm_left


def stripes(wm_left):
    s = L.STRIPES
    n = len(L.STRIPE_ORDER)
    x1 = wm_left - 18
    for i, bid in enumerate(L.STRIPE_ORDER):
        im = Image.new("L", (W, H), 0)
        d = ImageDraw.Draw(im)
        xr = x1 - (n - 1 - i) * (s["w"] + s["gap"])
        d.rectangle([P(xr - s["w"]), P(s["y"] - s["h"] / 2), P(xr), P(s["y"] + s["h"] / 2)], fill=255)
        im.save(os.path.join(OUT, f"stripe_{bid}.png"))


def drawer_ink():
    im = Image.new("L", (W, H), 0)
    if L.DRAWER_OPEN:
        d = ImageDraw.Draw(im)
        dr = L.DRAWER
        f_name, f_num, f_lamp, f_head = F(BOLD, 8.6), F(LEG, 7.6), F(MED, 6.9), F(BOLD, 8.4)
        text(d, (dr["x0"] + 20, dr["y0"] + 18), "SETUP  ·  SET ONCE, FORGET", f_head, anchor="lm", spacing=0.16)
        d.line([(P(dr["x0"] + 20), P(dr["y0"] + 27)), (P(dr["x1"] - 20), P(dr["y0"] + 27))], fill=255, width=int(0.6 * TEX))
        for c in L.CONTROLS:
            if c["y"] < L.H_PANEL:
                continue
            x, y = c["x"], c["y"]
            if c["type"] == "knob":
                r = L.KNOBS[c["kind"]]
                for n in c["ticks"]:
                    a = L.norm_angle(n)
                    p0, p1 = MT.polar(x, y, r + 2.5, a), MT.polar(x, y, r + 6, a)
                    d.line([(P(p0[0]), P(p0[1])), (P(p1[0]), P(p1[1]))], fill=255, width=int(0.8 * TEX))
                for lab in c["labels"]:
                    text(d, MT.polar(x, y, r + 13, L.norm_angle(lab["n"])), lab["t"], f_num)
                text(d, (x, y + r + 19), c["name"], f_name, spacing=0.05)
            elif c["type"] == "key":
                if c.get("lamps"):
                    for lx, s in zip(L.lamp_xs(c), c["lamps"]):
                        text(d, (lx, L.lamp_y(c) - 8.6), s, f_lamp, spacing=0.04)
                text(d, (x, y + c["h"] / 2 + 9.5), c["label"], f_name if len(c["label"]) <= 8 else f_lamp, spacing=0.06)
    im.save(os.path.join(OUT, "drawer_ink.png"))


def display_mask(spec, path):
    """A display page as a glow mask (the render tints it): title, page, four columns over four encoders."""
    S = 8
    D = L.DISPLAY
    inset = 9.0
    fw, fh = D["w"] - 2 * inset, D["h"] - 2 * inset
    im = Image.new("L", (int(fw * S), int(fh * S)), 0)
    d = ImageDraw.Draw(im)
    f_t, f_l, f_v = MT.font_px(BOLD, 10.0 * S), MT.font_px(MED, 7.8 * S), MT.font_px(BOLD, 12.5 * S)
    d.text((6 * S, 9 * S), spec["title"], font=f_t, fill=255, anchor="lm")
    d.text(((fw - 6) * S, 9 * S), spec["page"], font=f_l, fill=190, anchor="rm")
    d.line([(6 * S, 17 * S), ((fw - 6) * S, 17 * S)], fill=80, width=int(0.7 * S))
    # Columns centred over the encoders below.
    x0 = D["x"] - fw / 2
    for (lab, val, v), ex in zip(spec["cols"], L.ENC_X):
        cxp = ex - x0
        d.text((cxp * S, 27 * S), lab, font=f_l, fill=170, anchor="mm")
        d.text((cxp * S, 41 * S), val, font=f_v, fill=255, anchor="mm")
        bw = 56.0
        for k in range(10):
            on = k / 10 < v
            bx = cxp - bw / 2 + k * bw / 10
            d.rectangle([(bx + 0.6) * S, (fh - 13) * S, (bx + bw / 10 - 0.6) * S, (fh - 8) * S], fill=255 if on else 40)
    im = im.filter(ImageFilter.GaussianBlur(0.5 * S / 4))
    glow = im.filter(ImageFilter.GaussianBlur(2.2 * S / 4)).point(lambda v: int(v * 0.5))
    ImageChops.lighter(im, glow).save(path)


def type_mask():
    S = 8
    T = L.TYPE_WIN
    tw, th = T["w"] - 9, T["h"] - 9
    im = Image.new("L", (int(tw * S), int(th * S)), 0)
    ImageDraw.Draw(im).text((tw * S / 2, th * S / 2), L.TYPE_SHOW, font=MT.font_px(BOLD, 12.5 * S), fill=255, anchor="mm")
    im = im.filter(ImageFilter.GaussianBlur(0.5 * S / 4))
    ImageChops.lighter(im, im.filter(ImageFilter.GaussianBlur(2 * S / 4)).point(lambda v: int(v * 0.5))).save(os.path.join(OUT, "type_show.png"))


if __name__ == "__main__":
    wl = navy_and_shift()
    stripes(wl)
    drawer_ink()
    display_mask(L.DISPLAY_SHOW, os.path.join(OUT, "display_show.png"))
    for name, spec in L.DISPLAY_PAGES.items():
        display_mask(spec, os.path.join(OUT, f"display_{name}.png"))
    type_mask()
    # Previews of the print, for checking: navy, shift and stripes over powder blue.
    base = Image.new("RGB", (W, H), (153, 196, 224))
    for name, col in (("legend_ink.png", (18, 28, 46)), ("shift_ink.png", (199, 66, 26))):
        m = Image.open(os.path.join(OUT, name))
        base.paste(Image.new("RGB", (W, H), col), (0, 0), m)
    base.resize((W // 4, H // 4), Image.LANCZOS).save(os.path.join(OUT, "print_preview.png"))
    print("print masks written to", OUT)
