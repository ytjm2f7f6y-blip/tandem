"""Checks the panel's printing before anything is rendered: every word, lamp and control must sit inside
its box with room to spare, nothing may overlap anything else, and the names in a row should share
a line. Run:  TD_LAYOUT=layout_one python3 art/lint_layout.py   (exit 1 on a fault)

It places things exactly as textures_one.py prints them, measured with the same fonts."""
import os
import sys

os.environ.setdefault("TD_LAYOUT", "layout_one")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_textures as MT  # noqa: E402

L = MT.L
TEX = MT.TEX
F = MT.font
LEG, MED, BOLD = MT.LEGEND, MT.LEGEND_MED, MT.LEGEND_BOLD
T = L.TYPE
f_name, f_num, f_sub, f_lamp, f_sec, f_shift = F(BOLD, T["name"]), F(LEG, T["num"]), F(MED, T["sub"]), F(MED, T["lamp"]), F(BOLD, T["sec"]), F(BOLD, 9.0)

EDGE_SIDE, EDGE_BOTTOM, EDGE_TOP = 6.0, 9.0, 5.0      # room inside a box (points)
GAP = 1.6                                             # between any two things


def tbox(s, f, x, y, anchor="mm", spacing=0.0):
    """The printed extent of a string, in points."""
    w = (sum(f.getlength(ch) for ch in s) + spacing * f.size * max(0, len(s) - 1)) / TEX
    b = f.getbbox("HXgy0", anchor="lm")
    top, bot = b[1] / TEX, b[3] / TEX
    cap = f.getbbox("H", anchor="lm")
    ch = (cap[3] - cap[1]) / TEX
    if anchor[0] == "m":
        x0 = x - w / 2
    elif anchor[0] == "r":
        x0 = x - w
    else:
        x0 = x
    if anchor[1] == "m":                     # caps centred on y; descenders hang below
        y0, y1 = y - ch / 2, y + ch / 2 + max(0.0, bot - (-top)) * 0.0
    else:
        y0, y1 = y + top, y + bot
    has_desc = any(c in s for c in "gjpqy,")
    if has_desc:
        y1 += ch * 0.25
    return (x0, y0, x0 + w, y1)


def circ(x, y, r):
    return (x - r, y - r, x + r, y + r)


def elements():
    """(owner, what, (x0, y0, x1, y1)) for everything printed or mounted on the panel."""
    out = []
    for c in L.CONTROLS:
        t, x, y, cid = c["type"], c["x"], c["y"], c["id"]
        add = lambda what, b: out.append((cid, what, b))  # noqa: E731
        if t == "knob" and c.get("encoder"):
            add("body", circ(x, y, L.KNOBS[c["kind"]] + 6))
            continue
        if t == "knob" and c["kind"] == "ring":
            r = L.KNOBS["ring"]
            add("body", circ(x, y, r + 2))
            for a, s in zip(c["steps"], c["texts"]):
                lx, ly = MT.polar(x, y, L.RING_LED_R, a)
                add("lamp", circ(lx, ly, 2.2))
                if s.startswith("icon:"):
                    ix, iy = MT.polar(x, y, L.RING_ICON_R, a)
                    add("icon", (ix - MT.ICON_W / 2, iy - MT.ICON_H / 2, ix + MT.ICON_W / 2, iy + MT.ICON_H / 2))
                else:
                    tx, ty = MT.polar(x, y, L.RING_LABEL_R, a)
                    add("num", tbox(s, f_num, tx, ty))
            ny = y + (c["name_dy"] or (r + L.NAME_GAP + 6))
            add("name", tbox(c["name"], f_name, x, ny, spacing=0.05))
            if c.get("shift"):
                add("shift", tbox(c["shift"], f_shift, x, ny + 10.5, spacing=0.08))
            continue
        if t == "knob":
            r = L.KNOBS[c["kind"]]
            add("body", circ(x, y, r + 7.5))
            for lab in c["labels"]:
                tx, ty = MT.polar(x, y, r + L.LABEL_R, L.norm_angle(lab["n"]))
                add("num", tbox(lab["t"], f_num, tx, ty))
            ny = y + (r + 19 if c["kind"] == "trim" else r + L.NAME_GAP)
            add("name", tbox(c["name"], f_name, x, ny, spacing=0.05))
            if c.get("sub"):
                ny += 10
                add("sub", tbox(c["sub"], f_sub, x, ny))
            if c.get("shift"):
                add("shift", tbox(c["shift"], f_shift, x, ny + 10.5, spacing=0.08))
        elif t == "fader":
            tr = c["travel"]
            top, bot = y - tr / 2, y + tr / 2
            ph = tr + L.FADER_CAP[1] + 12
            add("body", (x - max(L.FADER_PLATE_W, L.FADER_CAP[0]) / 2, y - ph / 2, x + max(L.FADER_PLATE_W, L.FADER_CAP[0]) / 2, y + ph / 2))
            if c["scale"]:
                for k, s in ((0, "0"), (5, "5"), (10, "10")):
                    yy = bot - (bot - top) * k / 10
                    add("num", tbox(s, f_num, x - L.FADER_PLATE_W / 2 - 7.5, yy, anchor="rm"))
            add("name", tbox(c["name"], f_name, x, bot + 28))
        elif t == "key":
            w, h = c["w"], c["h"]
            add("body", (x - w / 2, y - h / 2, x + w / 2, y + h / 2))
            if c.get("lamps"):
                ly = L.lamp_y(c)
                for lx, s in zip(L.lamp_xs(c), c["lamps"]):
                    add("lamp", circ(lx, ly, 2.2))
                    add("lamptext", tbox(s, f_lamp, lx, ly - 8.6, spacing=0.04))
            elif c.get("led"):
                add("lamp", circ(x, y - h / 2 - 7.5, 2.2))
            lab = c.get("label", "")
            if lab.startswith("icon:"):
                add("icon", (x - 12, y + h / 2 + 12.5 - 7, x + 12, y + h / 2 + 12.5 + 7))
            elif lab:
                if c.get("label_side") == "right":
                    add("label", tbox(lab, f_sub, x + w / 2 + 7, y + 0.4, anchor="lm", spacing=0.06))
                elif c.get("cap") == "shift":
                    add("shift", tbox(lab, f_shift, x, y + h / 2 + 9.5, spacing=0.08))
                else:
                    add("label", tbox(lab, f_sub if len(lab) <= 6 else f_lamp, x, y + h / 2 + 9.5, spacing=0.06))
            if c.get("shift"):
                add("shift", tbox(c["shift"], f_shift, x, y + h / 2 + 19.5, spacing=0.08))
        elif t == "lamp":
            add("lamp", circ(x, y, c["r"] + 0.8))
    for i, (s, x, y, size, ink, anchor) in enumerate(L.PRINT):
        if ink == "glyph":
            continue
        out.append(("print:" + s, "print", tbox(s, F(BOLD if ink == "shift" else MED, size), x, y, anchor=anchor, spacing=0.08 if ink != "shift" else 0.06)))
    for s, x0, x1, y in L.SUBHEADS:
        out.append(("sub:" + s, "subhead", tbox(s, f_sec, (x0 + x1) / 2, y, spacing=0.12)))
        out.append(("sub:" + s, "rule", (x0, y - 3, x1, y + 0.5)))
    tw = L.TYPE_WIN
    out.append(("f_type", "body", (tw["x"] - tw["w"] / 2, tw["y"] - tw["h"] / 2, tw["x"] + tw["w"] / 2, tw["y"] + tw["h"] / 2)))
    out.append(("f_type", "name", tbox("FILTER TYPE", f_name, tw["x"], L.TYPE_LABEL_Y, spacing=0.05)))
    for i, x in enumerate(L.MIXC):
        out.append((f"mixhead{i}", "name", tbox("LAYER " + str(i + 1), f_name, x, L.MIX_HEAD_Y, spacing=0.08)))
    lx, ly = L.PLAY_LEGEND_XY
    for i, s in enumerate(L.PLAY_MODES):
        yy = ly + i * L.PLAY_LEGEND_STEP
        out.append(("legend", "print", tbox(str(i + 1) + "  " + s, F(MED, 8.8), lx, yy, anchor="lm")))
    d = L.DISPLAY
    out.append(("disp", "body", (d["x"] - d["w"] / 2, d["y"] - d["h"] / 2, d["x"] + d["w"] / 2, d["y"] + d["h"] / 2)))
    for pw in L.PATCH_WINS:
        out.append((pw["id"], "body", (pw["x"] - pw["w"] / 2, pw["y"] - pw["h"] / 2, pw["x"] + pw["w"] / 2, pw["y"] + pw["h"] / 2)))
    if hasattr(L, "ARP_WIN"):
        a = L.ARP_WIN
        out.append(("arp_win", "body", (a["x"] - a["w"] / 2, a["y"] - a["h"] / 2, a["x"] + a["w"] / 2, a["y"] + a["h"] / 2)))
    for i, (x, y) in enumerate(L.SCREWS):
        out.append((f"screw{i}", "body", circ(x, y, 5.0)))
    for extra in getattr(L, "LINT_EXTRA", []):
        out.append(extra)
    return out


def inside(b, outer, side, top, bottom):
    return b[0] >= outer[0] + side and b[2] <= outer[2] - side and b[1] >= outer[1] + top and b[3] <= outer[3] - bottom


def overlap(a, b, gap):
    return a[0] < b[2] + gap and b[0] < a[2] + gap and a[1] < b[3] + gap and b[1] < a[3] + gap


def lint(verbose=True):
    els = elements()
    faults, notes = [], []
    boxes = {b[0]: (b[2], b[4][0], b[3], b[4][1]) for b in L.BOXES}
    owner_box = {}
    for c in L.CONTROLS:
        owner_box[c["id"]] = L.box_of(c["x"], c["y"])
    for s, x0, x1, y in L.SUBHEADS:
        owner_box["sub:" + s] = L.box_of((x0 + x1) / 2, y)
    for s, x, y, *_ in L.PRINT:
        owner_box["print:" + s] = L.box_of(x, y)
    owner_box["f_type"] = "vcf"
    owner_box["legend"] = "perf"
    owner_box["disp"] = "prog"
    owner_box["arp_win"] = "arp"
    for i in range(len(L.MIXC)):
        owner_box[f"mixhead{i}"] = "lmix"
    # 1. Inside its box, with room.
    for o, what, b in els:
        bid = owner_box.get(o)
        if bid not in boxes:
            continue
        x0, y0, x1, y1 = boxes[bid]
        outer = (x0, y0 + L.HEAD_H, x1, y1)
        if not inside(b, outer, EDGE_SIDE, EDGE_TOP, EDGE_BOTTOM):
            room = min(b[0] - outer[0], outer[2] - b[2], b[1] - outer[1], outer[3] - b[3])
            faults.append(f"{o} {what}: {room:.1f} pt from the edge of {bid} (wants {EDGE_SIDE:.0f}-{EDGE_BOTTOM:.0f})")
    # 2. Nothing overlaps anything of another owner (a control's own parts may touch).
    for i in range(len(els)):
        for j in range(i + 1, len(els)):
            oa, wa, a = els[i]
            ob, wb, b = els[j]
            if oa == ob:
                continue
            if wa == "rule" and wb == "rule":
                continue
            if overlap(a, b, GAP):
                faults.append(f"{oa} {wa} overlaps {ob} {wb}")
    # 3. Names in a row share a line: controls in one box whose bodies overlap vertically.
    names = {}
    for o, what, b in els:
        if what in ("name", "label"):
            names.setdefault(o, (what, b))
    ctl = {c["id"]: c for c in L.CONTROLS}
    seen = set()
    for a in names:
        for b in names:
            if a >= b or a not in ctl or b not in ctl or owner_box.get(a) != owner_box.get(b):
                continue
            ca, cb = ctl[a], ctl[b]
            if abs(ca["y"] - cb["y"]) > 14 or ca["type"] == "lamp" or cb["type"] == "lamp":
                continue
            ya, yb = (names[a][1][1] + names[a][1][3]) / 2, (names[b][1][1] + names[b][1][3]) / 2
            if 2.0 < abs(ya - yb) < 40 and (a, b) not in seen:
                seen.add((a, b))
                notes.append(f"{a} '{ca.get('name') or ca.get('label')}' and {b} '{cb.get('name') or cb.get('label')}': captions {abs(ya - yb):.1f} pt apart in height")
    if verbose:
        for f in faults:
            print("FAULT", f)
        for n in notes:
            print("row  ", n)
        print(f"{len(faults)} faults, {len(notes)} uneven rows")
    return faults, notes


if __name__ == "__main__":
    faults, _ = lint()
    sys.exit(1 if faults else 0)
