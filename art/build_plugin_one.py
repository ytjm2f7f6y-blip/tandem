"""The one-panel Tandem, for the plugin: packs the renders into plugin/assets and writes
plugin/Layout.h and plugin/HelpText.h from layout_one, so the window draws everything exactly
where the renders put it.

    TD_DRAWER=1 python3 art/build_plugin_one.py           (after art/render_plugin_art.sh)
    TD_DRAWER=1 python3 art/build_plugin_one.py --header  (Layout.h and HelpText.h only)

Assets:
  panel.jpg             the bare panel, every lamp off (but POWER)
  drawer.jpg            the setup drawer, pulled out, lamps off
  leds.png              an atlas of every lamp lit, cut from the lit renders, edges feathered
  knob_<kind>_<colour>.jpg / _mask.png   the knob strips (colour on black, one shared outline)
  fader_cap_<box>.png   a fader cap in each box's colour
"""
import math
import os
import sys

os.environ.setdefault("TD_LAYOUT", "layout_one")
os.environ["TD_DRAWER"] = "1"                 # the drawer's controls are part of the window
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import layout_one as L  # noqa: E402
import help_one as HELP  # noqa: E402

ROOT = os.path.join(HERE, "..")
ASSETS = os.path.join(ROOT, "plugin", "assets")
B_PANEL = os.path.join(HERE, "build_plugin")
B_OPEN = os.path.join(HERE, "build_plugin_open")
S = L.SCALE
STRIP_ORDER = ["large_orange", "large_red", "med_cream", "med_orange", "med_teal", "med_mustard", "med_red",
               "small_red", "ring_cream", "ring_teal", "ring_red", "trim_cream"]
FADER_CAPS = sorted({L.box_of(c["x"], c["y"]) for c in L.CONTROLS if c["type"] == "fader"})      # a cap per box colour
COLOURS = {"cream": (0.93, 0.89, 0.78), "orange": (0.93, 0.45, 0.18), "teal": (0.20, 0.60, 0.62),
           "mustard": (0.92, 0.72, 0.22), "red": (0.80, 0.22, 0.18)}
CAPS = {"osc": "cream", "mix": "cream", "vcf": "orange", "lfo": "teal", "env": "mustard", "perf": "red", "arp": "red",
        "fx": "red", "prog": "red", "lmix": "red", "out": "red", "drawer": "cream"}
KINDS = ["large", "med", "small", "ring", "trim"]
SHADOW_MARGIN = 14


def strip_name(c):
    col = CAPS[L.box_of(c["x"], c["y"])]
    if c.get("encoder"):
        return f"enc_{col}"
    return f"{c['kind']}_{col}"


def ring_angles():
    """Each ring strip's frames: the sorted angles of every ring that uses it (as render_one makes them)."""
    out = {}
    for c in L.CONTROLS:
        if c["type"] == "knob" and c["kind"] == "ring":
            nm = strip_name(c)
            out[nm] = sorted(set(out.get(nm, [])) | {round(a, 3) for a in c["steps"]})
    return out


def leds():
    """Every lamp: (control id, x, y, half size of its lit patch, in the drawer?)."""
    out = []
    for c in L.CONTROLS:
        d = c["y"] > L.H_PANEL
        if c["type"] == "key":
            if c.get("led"):
                out.append((c["id"], c["x"], c["y"] - c["h"] / 2 - 5.8, 5.0, d))
            for lx in (L.lamp_xs(c) if c.get("lamps") else []):
                out.append((c["id"], lx, L.lamp_y(c), 4.6, d))
        elif c["type"] == "knob" and c["kind"] == "ring":
            for a in c["steps"]:
                out.append((c["id"], c["x"] + math.sin(math.radians(a)) * L.RING_LED_R, c["y"] - math.cos(math.radians(a)) * L.RING_LED_R, 4.4, d))
        elif c["type"] == "lamp" and c["id"] != "power":
            out.append((c["id"], c["x"], c["y"], 4.4 if c.get("style") == "led" else c["r"] + 6.0, d))
    return out


def atlas_layout(items, width_px=1024):
    """Shelf-pack the lit patches: (ax, ay) in atlas pixels for each."""
    pos, x, y, row = [], 0, 0, 0
    for _, _, _, half, _ in items:
        s = int(math.ceil(2 * half * S))
        if x + s > width_px:
            x, y, row = 0, y + row, 0
        pos.append((x, y))
        x += s
        row = max(row, s)
    return pos, (width_px, y + row)


# ---------------------------------------------------------------------------
def pack():
    import numpy as np
    from PIL import Image

    os.makedirs(ASSETS, exist_ok=True)
    for f in os.listdir(ASSETS):                       # the old faceplate's art goes
        if f.startswith(("knob_", "toggle_", "lit_")) or f in (".placeholders",):
            os.remove(os.path.join(ASSETS, f))
    Image.open(os.path.join(B_PANEL, "panel_bare.jpg")).convert("RGB").save(os.path.join(ASSETS, "panel.jpg"), quality=88, subsampling=0, optimize=True)
    Image.open(os.path.join(B_OPEN, "bare_drawer.jpg")).convert("RGB").save(os.path.join(ASSETS, "drawer.jpg"), quality=88, subsampling=0, optimize=True)
    lit_panel = np.asarray(Image.open(os.path.join(B_PANEL, "panel_lit.jpg")).convert("RGB")).astype(np.float32)
    lit_drawer = np.asarray(Image.open(os.path.join(B_OPEN, "lit_drawer.jpg")).convert("RGB")).astype(np.float32)
    items = leds()
    pos, (aw, ah) = atlas_layout(items)
    atlas = np.zeros((ah, aw, 4), np.float32)
    for (cid, x, y, half, drawer), (ax, ay) in zip(items, pos):
        src, oy = (lit_drawer, L.H_PANEL) if drawer else (lit_panel, 0)
        s = int(math.ceil(2 * half * S))
        x0, y0 = int(round((x - half) * S)), int(round((y - oy - half) * S))
        patch = src[y0:y0 + s, x0:x0 + s]
        yy, xx = np.mgrid[0:s, 0:s]
        d = np.hypot(xx + 0.5 - s / 2, yy + 0.5 - s / 2) / S          # points from the lamp's centre
        alpha = np.clip((half - d) / (half * 0.45), 0, 1)
        atlas[ay:ay + s, ax:ax + s, :3] = patch
        atlas[ay:ay + s, ax:ax + s, 3] = alpha * 255
    Image.fromarray(np.clip(atlas + 0.5, 0, 255).astype(np.uint8), "RGBA").save(os.path.join(ASSETS, "leds.png"), optimize=True)
    print("leds:", len(items), "lamps,", aw, "x", ah)

    def clean_catcher(a):
        """Sprites on a shadow catcher pick up a faint even tint across the box: take it out, fade the edge."""
        fr = a.copy()
        al = fr[..., 3]
        border = np.concatenate([al[0, :], al[-1, :], al[:, 0], al[:, -1]])
        bgl = float(np.median(border))
        if bgl > 0:
            fr[..., 3] = np.clip((al - bgl) / max(1e-3, 255 - bgl) * 255, 0, 255)
        n = 5
        h, w = fr.shape[:2]
        ramp = np.minimum(np.minimum(np.arange(h)[:, None], np.arange(h)[::-1, None]), np.minimum(np.arange(w)[None, :], np.arange(w)[None, ::-1]))
        fr[..., 3] *= np.clip(ramp / n, 0, 1)
        return fr

    for nm in STRIP_ORDER:
        a = np.asarray(Image.open(os.path.join(B_PANEL, "strips", nm + ".png")).convert("RGBA")).astype(np.float32)
        fw = a.shape[1]
        n = a.shape[0] // fw
        frames = [clean_catcher(a[i * fw:(i + 1) * fw]) for i in range(n)]
        a = np.concatenate(frames, axis=0)
        alpha = a[..., 3:4] / 255.0
        Image.fromarray(np.clip(a[..., :3] * alpha + 0.5, 0, 255).astype(np.uint8), "RGB").save(os.path.join(ASSETS, f"knob_{nm}.jpg"), quality=88)
        mask = a[..., 3].reshape(n, fw, fw).max(axis=0)
        Image.fromarray(np.clip(mask + 0.5, 0, 255).astype(np.uint8), "L").save(os.path.join(ASSETS, f"knob_{nm}_mask.png"), optimize=True)
        print(f"knob_{nm}:", n, "frames")
    for f_ in os.listdir(ASSETS):
        if f_.startswith("fader_cap"):
            os.remove(os.path.join(ASSETS, f_))
    for box in FADER_CAPS:
        fc = np.asarray(Image.open(os.path.join(B_PANEL, "sprites", f"fader_cap_{box}.png")).convert("RGBA")).astype(np.float32)
        Image.fromarray(np.clip(clean_catcher(fc) + 0.5, 0, 255).astype(np.uint8), "RGBA").save(os.path.join(ASSETS, f"fader_cap_{box}.png"), optimize=True)
        print("fader cap", box)


# ---------------------------------------------------------------------------
LAMP_MODES = {   # how a key's lamps show its setting: 0 one lamp per value; 1 the first value has none; 2 "both" lights both
    "o_sync": 1, "m_osc": 2, "m_wsrc": 1, "f_kt": 1, "e1_mode": 1,
}


def f(v):
    return f"{float(v):.4f}f"


def header():
    items = leds()
    pos, (aw, ah) = atlas_layout(items)
    first = {}
    for i, it in enumerate(items):
        first.setdefault(it[0], i)
    count = {}
    for it in items:
        count[it[0]] = count.get(it[0], 0) + 1
    rings = ring_angles()
    lines = []
    w = lines.append
    w("// Generated by art/build_plugin_one.py from art/layout_one.py: do not edit by hand.")
    w("// Points at 100%; the art is rendered at 2x.")
    w("#pragma once")
    w("")
    w("namespace layout {")
    w("")
    w(f"constexpr float W = {f(L.W)}, H = {f(L.H_PANEL)}, kDrawerH = {f(L.DRAWER_H)};   // the drawer slides up over the bottom kDrawerH")
    w(f"constexpr int kArtScale = {S};")
    w(f"constexpr float kSweep = {f(L.SWEEP)};     // continuous knobs: +-degrees")
    w("enum Kind { kLarge, kMed, kSmall, kRing, kTrim };")
    w(f"constexpr float kKnobRadius[5] = {{{', '.join(f(L.KNOBS[k]) for k in KINDS)}}};")
    w(f"constexpr float kShadowMargin = {f(SHADOW_MARGIN)};   // a knob sprite is 2 r + 2 margins square")
    w("enum Type { Knob, Key, Fader, Lamp };")
    w("")
    w(f"constexpr int kNumFaderCaps = {len(FADER_CAPS)};")
    w("inline const char* const kFaderCapNames[kNumFaderCaps] = {" + ", ".join(f'"{n}"' for n in FADER_CAPS) + "};")
    w(f"constexpr int kNumStrips = {len(STRIP_ORDER)};")
    w("inline const char* const kStripNames[kNumStrips] = {" + ", ".join(f'"{n}"' for n in STRIP_ORDER) + "};")
    w("")
    w("struct Control {")
    w("    const char* id;")
    w("    Type type;")
    w("    int kind;            // knobs: Kind")
    w("    int strip;           // knobs: index into kStripNames; faders: into kFaderCapNames")
    w("    float x, y, w, h;    // centre; keys: size; lamps: diameter")
    w("    int nSteps;          // rings: positions")
    w("    float steps[12];     // rings: angles (degrees)")
    w("    int stepFrame[12];   // rings: the strip frame for each position")
    w("    bool encoder;        // the programmer's endless encoders")
    w("    bool drawer;         // in the setup drawer")
    w("    bool shiftCap;       // the SHIFT key")
    w("    bool labelSide;      // a key labelled at its right (the page keys)")
    w("    int led, nLeds;      // its lamps in kLeds")
    w("    int lampMode;        // keys with lamps: 0 one per value, 1 the first value lights none, 2 the middle value lights both")
    w("    float travel;        // faders")
    w("};")
    w("")
    w("inline const Control kControls[] = {")
    for c in L.CONTROLS:
        t = {"knob": "Knob", "key": "Key", "fader": "Fader", "lamp": "Lamp"}[c["type"]]
        kind = KINDS.index(c["kind"]) if c["type"] == "knob" else 0
        strip = STRIP_ORDER.index(strip_name(c)) if c["type"] == "knob" else (FADER_CAPS.index(L.box_of(c["x"], c["y"])) if c["type"] == "fader" else -1)
        cw, ch = (c.get("w", 0), c.get("h", 0)) if c["type"] == "key" else ((2 * c["r"], 2 * c["r"]) if c["type"] == "lamp" else (0, 0))
        st = (c.get("steps") or []) if (c["type"] == "knob" and c["kind"] == "ring") else []
        frames = [rings[strip_name(c)].index(round(a, 3)) for a in st] if st else []
        steps = ", ".join(f(a) for a in (st + [0] * 12)[:12])
        sf = ", ".join(str(v) for v in (frames + [0] * 12)[:12])
        led = first.get(c["id"], -1)
        w(f'    {{"{c["id"]}", {t}, {kind}, {strip}, {f(c["x"])}, {f(c["y"])}, {f(cw)}, {f(ch)}, {len(st)}, {{{steps}}}, {{{sf}}}, '
          f'{"true" if c.get("encoder") else "false"}, {"true" if c["y"] > L.H_PANEL else "false"}, {"true" if c.get("cap") == "shift" else "false"}, '
          f'{"true" if c.get("label_side") else "false"}, {led}, {count.get(c["id"], 0)}, {LAMP_MODES.get(c["id"], 0)}, {f(c.get("travel", 0))}}},')
    w("};")
    w("constexpr int kNumControls = int(sizeof(kControls) / sizeof(kControls[0]));")
    w("")
    w("// Every lamp: where it is, the half-size of its lit patch, and where that patch sits in leds.png (pixels).")
    w("struct Led { float x, y, half; int ax, ay; };")
    w("inline const Led kLeds[] = {")
    for (cid, x, y, half, d), (ax, ay) in zip(items, pos):
        w(f"    {{{f(x)}, {f(y)}, {f(half)}, {ax}, {ay}}},   // {cid}")
    w("};")
    w(f"constexpr int kNumLeds = {len(items)};")
    w("")
    fw_, fh_ = L.FADER_BOX
    w(f"constexpr float kFaderBoxW = {f(fw_)}, kFaderBoxH = {f(fh_)}, kFaderCapH = {f(L.FADER_CAP[1])};")

    def face(spec):
        inset = 9.0 if spec["w"] > 150 and spec["h"] > 60 else 4.5
        return spec["x"] - spec["w"] / 2 + inset, spec["y"] - spec["h"] / 2 + inset, spec["w"] - 2 * inset, spec["h"] - 2 * inset
    dx, dy, dw, dh = face(L.DISPLAY)
    w("// The display's glass and the filter type window's, where the window writes.")
    w(f"constexpr float kDispX = {f(dx)}, kDispY = {f(dy)}, kDispW = {f(dw)}, kDispH = {f(dh)};")
    w(f"constexpr float kEncX[4] = {{{', '.join(f(x) for x in L.ENC_X)}}};   // the display's columns sit over these")
    tx, ty, tw, th = face(L.TYPE_WIN)
    w(f"constexpr float kTypeX = {f(tx)}, kTypeY = {f(ty)}, kTypeW = {f(tw)}, kTypeH = {f(th)};")
    w("// Each layer's patch window (its glass).")
    pw = [face(p_) for p_ in L.PATCH_WINS]
    w("constexpr float kPatchWin[2][4] = {" + ", ".join("{" + ", ".join(f(v) for v in r) + "}" for r in pw) + "};")
    w("")
    w("}  // namespace layout")
    w("")
    open(os.path.join(ROOT, "plugin", "Layout.h"), "w").write("\n".join(lines))
    print("wrote plugin/Layout.h:", len(L.CONTROLS), "controls,", len(items), "lamps")

    def esc(s):
        return s.replace("\\", "\\\\").replace('"', '\\"')
    ids = {c["id"] for c in L.CONTROLS} | {"f_type", "disp"}
    missing = sorted(i for i in ids if i not in HELP.HELP and i.rsplit("_", 1)[0] not in HELP.HELP)   # a family: vm1_0 -> vm1
    if missing:
        print("no help for:", ", ".join(missing))
    out = ["// Generated by art/build_plugin_one.py from art/help_one.py: do not edit by hand.", "#pragma once", "",
           "namespace help {", "struct Entry { const char* id; const char* name; const char* text; };", "inline const Entry kEntries[] = {"]
    for cid, (name, text) in HELP.HELP.items():
        out.append(f'    {{"{cid}", "{esc(name)}", "{esc(text)}"}},')
    out += ["};", "constexpr int kNumEntries = int(sizeof(kEntries) / sizeof(kEntries[0]));", "}  // namespace help", ""]
    open(os.path.join(ROOT, "plugin", "HelpText.h"), "w", encoding="utf-8").write("\n".join(out))
    print("wrote plugin/HelpText.h,", len(HELP.HELP), "entries")


if __name__ == "__main__":
    if "--header" not in sys.argv:
        pack()
    header()
