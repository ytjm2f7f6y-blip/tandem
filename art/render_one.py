"""Render the one-panel Tandem (layout_one, theme "one") with Blender, reusing render.py's parts.

    cd art
    TD_BUILD=build_one python3 render_one.py --px 1.5 --samples 40 --out one.jpg \\
        [--crops "name:x,y,w,h;..." --crop-px 4] [--pages]

The panel is one sheet of powder-coated steel. The paint and the print are modelled the way they
are made: a satin coat with orange peel and faint mottling; inks laid on top, each a little
thicker and glossier than the coat, with ragged edges, uneven coverage, the odd pinhole, and the
second (SHIFT) screen a hair out of register.
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
os.environ.setdefault("TD_LAYOUT", "layout_one")
os.environ.setdefault("TD_THEME", "one")
os.environ.setdefault("TD_BUILD", "build_one")
sys.path.insert(0, HERE)

import bpy  # noqa: E402  (bpy first: it provides bmesh)
import bmesh  # noqa: E402

import render as R  # noqa: E402

L, T = R.L, R.T
BUILD = R.BUILD
Z = R.Z_CASS
R.KNOB_H.update({"ring": 13.0, "trim": 9.0})
CUR = {"box": "osc"}
R.section_of = lambda x: CUR["box"]      # the cap colour follows the box the knob is in


def _arg(name):
    return sys.argv[sys.argv.index(name) + 1] if name in sys.argv else None


CROPS = _arg("--crops")
STATE = _arg("--state")          # None: the mockup. "bare": the plugin's background. "lit": every lamp on.
STRIPS = _arg("--strips")        # "all" or a comma list of strip names: knob filmstrips for the plugin
CROP_PX = float(_arg("--crop-px") or 4)
PAGES = "--pages" in sys.argv
NO_MAIN = "--no-main" in sys.argv


# ---------------------------------------------------------------------------
# Powder coat and screen print
def noise(nt, vec, scale, detail=2.0, rough=0.5):
    n = nt.nodes.new("ShaderNodeTexNoise")
    n.inputs["Scale"].default_value = scale
    n.inputs["Detail"].default_value = detail
    n.inputs["Roughness"].default_value = rough
    nt.links.new(vec, n.inputs["Vector"])
    return n.outputs["Fac"]


def powder_print(name, base, inks, peel_k=1.0):
    """Satin powder coat with printed inks: [(mask path, ink colour (linear), ink roughness, uv offset)]."""
    m, nt, bsdf = R.new_mat(name)
    M = R.math_node
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    pos = geo.outputs["Position"]
    uv = R.canvas_uv(nt)
    # Orange peel: soft undulations a few points across, and a finer ripple on them.
    p1 = noise(nt, pos, 0.34, 1.0, 0.4)
    p2 = noise(nt, pos, 1.25, 0.0, 0.5)
    # Faint mottling across the sheet (the gun's passes), and the odd speck in the coat.
    mott = noise(nt, pos, 0.006, 2.0, 0.5)
    speck = noise(nt, pos, 4.2, 0.0)
    col = R.mix_rgb(nt, mott, (base[0] * 0.975, base[1] * 0.978, base[2] * 0.982, 1), (base[0] * 1.02, base[1] * 1.018, base[2] * 1.012, 1))
    sp = M(nt, "MULTIPLY", M(nt, "SUBTRACT", speck, 0.79), 25.0, clamp=True)
    col = R.mix_rgb(nt, M(nt, "MULTIPLY", sp, 0.35), col, (base[0] * 0.6, base[1] * 0.62, base[2] * 0.66, 1))
    rough = M(nt, "ADD", M(nt, "MULTIPLY", p1, 0.10), 0.37)
    height = M(nt, "ADD", M(nt, "MULTIPLY", p1, 1.0), M(nt, "MULTIPLY", p2, 0.08))
    ink_h = None
    for path, ink, ink_rough, off in inks:
        vec = uv
        if off != (0, 0):
            mp = nt.nodes.new("ShaderNodeMapping")
            mp.inputs["Location"].default_value = (off[0] / L.W, -off[1] / L.H, 0)
            nt.links.new(uv, mp.inputs["Vector"])
            vec = mp.outputs["Vector"]
        t = R.tex(nt, R.img(path), vec)
        msk = t.outputs["Color"]
        # Ragged edges: the mesh and the squeegee leave the ink's edge a little uneven.
        edge = M(nt, "MULTIPLY", M(nt, "SUBTRACT", noise(nt, pos, 2.6, 2.0, 0.6), 0.5), 0.34)
        f = M(nt, "ADD", M(nt, "MULTIPLY", M(nt, "ADD", M(nt, "SUBTRACT", msk, 0.5), edge), 3.2), 0.5, clamp=True)
        # Uneven coverage, and pinholes where the ink pulled away from the coat.
        # (Coverage is mixed in linear light, so a few per cent shows: keep it near solid.)
        cov = M(nt, "ADD", M(nt, "MULTIPLY", noise(nt, pos, 0.05, 2.0), 0.05), 0.955, clamp=True)
        pin = M(nt, "SUBTRACT", 1.0, M(nt, "MULTIPLY", M(nt, "MULTIPLY", M(nt, "SUBTRACT", noise(nt, pos, 2.2, 0.0), 0.83), 25.0, clamp=True), 0.55))
        fac = M(nt, "MULTIPLY", M(nt, "MULTIPLY", f, cov), pin, clamp=True)
        col = R.mix_rgb(nt, fac, col, ink)
        rough = M(nt, "ADD", rough, M(nt, "MULTIPLY", fac, M(nt, "SUBTRACT", ink_rough, rough)))
        ink_h = fac if ink_h is None else M(nt, "MAXIMUM", ink_h, fac)
    if ink_h is not None:          # a layer of ink partly levels the peel under it
        height = M(nt, "MULTIPLY", height, M(nt, "SUBTRACT", 1.0, M(nt, "MULTIPLY", ink_h, 0.8)))
    nt.links.new(col, bsdf.inputs["Base Color"])
    nt.links.new(rough, bsdf.inputs["Roughness"])
    bsdf.inputs["Specular IOR Level"].default_value = 0.5
    b1 = nt.nodes.new("ShaderNodeBump")
    b1.inputs["Strength"].default_value = 0.45 * peel_k
    b1.inputs["Distance"].default_value = 0.7
    nt.links.new(height, b1.inputs["Height"])
    normal = b1.outputs["Normal"]
    if ink_h is not None:          # the ink stands on the coat: a few hundredths of a point
        b2 = nt.nodes.new("ShaderNodeBump")
        b2.inputs["Strength"].default_value = 0.55
        b2.inputs["Distance"].default_value = 0.05
        nt.links.new(ink_h, b2.inputs["Height"])
        nt.links.new(normal, b2.inputs["Normal"])
        normal = b2.outputs["Normal"]
    nt.links.new(normal, bsdf.inputs["Normal"])
    return m


# ---------------------------------------------------------------------------
# Parts
def offset(objs, dx, dy):
    for o in objs:
        o.location.x += dx
        o.location.y += -dy
    return objs


def led_obj(lit, r=1.8):
    M = R.mats()
    c = {"red": (1.0, 0.004, 0.0), "amber": (1.0, 0.20, 0.0)}[T["led"]]
    key = f"led_{lit}"
    if key not in M:
        m, nt, bsdf = R.new_mat(key)
        bsdf.inputs["Base Color"].default_value = (*c, 1) if lit else (c[0] * 0.10, c[1] * 0.06, c[2] * 0.06, 1)
        bsdf.inputs["Roughness"].default_value = 0.08
        bsdf.inputs["Transmission Weight"].default_value = 0.3
        if lit:
            bsdf.inputs["Emission Color"].default_value = (*c, 1)
            bsdf.inputs["Emission Strength"].default_value = 1.1
        M[key] = m
    ring = R.prism("led_ring", R.circle_pts(r + 0.8, 32), 0, 0.8, M["black_plastic"], bevel=0.2)
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=24, v_segments=12, radius=r)
    lens = R.mesh_obj("led", bm, M[key])
    lens.scale = (1, 1, 0.7)
    lens.location = (0, 0, 0.7)
    for p in lens.data.polygons:
        p.use_smooth = True
    return [ring, lens]


def lit_state(mockup_lit):
    """A lamp's state: as the mockup has it, or all off (bare) or all on (lit) for the plugin."""
    return mockup_lit if not STATE else STATE == "lit"


def arrow(direction):
    M = R.mats()
    s = 3.2
    pts = [(-s * 0.6, -s), (s * 0.7, 0), (-s * 0.6, s)] if direction == ">" else [(s * 0.6, s), (-s * 0.7, 0), (s * 0.6, -s)]
    return [R.prism("arrow", pts, 5.9, 6.45, M["black_plastic"], smooth=False)]


def key_obj(c):
    M = R.mats()
    shift = c.get("cap") == "shift"
    nm = "keycap_shift" if shift else "keycap"
    if nm not in M:
        M[nm] = R.plastic(nm, T["shift_cap"] if shift else T["key"], rough=0.34, sss=0.05)
    capm = M[nm]
    w, h = c["w"], c["h"]
    objs = [R.prism("key_well", R.rounded_rect_pts(w + 3.4, h + 3.4, 2.6), -0.2, 0.4, M["black_plastic"], bevel=0.3),
            R.prism("key_cap", R.rounded_rect_pts(w, h, 2.2), 0.3, 6.2, capm, bevel=1.2, taper=0.92)]
    if c.get("led"):
        objs += offset(led_obj(lit_state(bool(c["set"]))), 0, -(h / 2 + 5.8))
    if c.get("lamps"):
        for i, lx in enumerate(L.lamp_xs(c)):
            objs += offset(led_obj(lit_state(i in c["lit"]), r=1.6), lx - c["x"], L.lamp_y(c) - c["y"])
    return objs


def build():
    sc = R.reset()
    R.lights()
    R.camera(L.W / 2, L.H / 2, L.W, L.H, R.PX or 1.5)
    R.box("floor", -60, -60, L.W + 60, L.H + 60, -8, -6, R.simple("floor", (0.004, 0.004, 0.004, 1), rough=0.9), bevel=0)

    B = lambda f: os.path.join(BUILD, f)  # noqa: E731
    inks = [(B("legend_ink.png"), R.srgb(T["ink"]), 0.36, (0, 0)),
            (B("shift_ink.png"), R.srgb(T["shift_ink"]), 0.36, (0, 0))]
    inks += [(B(f"stripe_{bid}.png"), R.srgb(T["caps"][bid]), 0.36, (0, 0)) for bid in L.STRIPE_ORDER]
    panel = powder_print("panel", R.srgb(T["panel"]), inks)
    R.box("panel", L.CHEEK, 0, L.W - L.CHEEK, L.H_PANEL, 0, Z, panel, bevel=1.8)
    cheek = powder_print("cheek", R.srgb(T["cheek"]), [], peel_k=1.2)
    R.box("cheek_l", 0, -1, L.CHEEK + 0.6, L.H_PANEL + 1, 0, Z + 5.0, cheek, bevel=3.0)
    R.box("cheek_r", L.W - L.CHEEK - 0.6, -1, L.W, L.H_PANEL + 1, 0, Z + 5.0, cheek, bevel=3.0)
    for i, (x, y) in enumerate(L.SCREWS):
        R.place(R.screw((i * 47) % 180), x, y, Z)

    zc = Z
    if L.DRAWER_OPEN:          # the drawer, pulled out from under the front edge
        dr = L.DRAWER
        tray = powder_print("tray", R.srgb(T["tray"]), [(B("drawer_ink.png"), R.srgb(T["tray_ink"]), 0.32, (0, 0))], peel_k=0.8)
        R.box("tray", dr["x0"], L.H_PANEL - 30, dr["x1"], dr["y1"], -3.0, -0.8, tray, bevel=1.0)
        R.box("tray_lip", dr["x0"], dr["y1"] - 12, dr["x1"], dr["y1"] + 2, -3.0, 3.0, tray, bevel=1.6)

    D = L.DISPLAY
    R.place(R.display_box(D["w"], D["h"], "disp", show=None if STATE else "display_show.png"), D["x"], D["y"], Z)
    for i, (x, y) in enumerate(L.DISPLAY_SCREWS):
        R.place(R.screw((i * 61 + 20) % 180), x, y, Z + 4.6)
    TW = L.TYPE_WIN
    R.place(R.display_box(TW["w"], TW["h"], "ftype", show=None if STATE else "type_show.png"), TW["x"], TW["y"], Z)

    glyphs = {(round(x), round(y)): s for s, x, y, size, ink, a in L.PRINT if ink == "glyph"}
    for c in L.CONTROLS:
        t = c["type"]
        x, y = c["x"], c["y"]
        z = -0.8 if y > L.H_PANEL else zc
        CUR["box"] = L.box_of(x, y)
        if t == "knob":
            if not STATE:
                R.place(R.knob(c["kind"], None, x), x, y, z, rot_deg=L.set_angle(c))
            if c["kind"] == "ring":
                for i, a in enumerate(c["steps"]):
                    lx = x + math.sin(math.radians(a)) * L.RING_LED_R
                    ly = y - math.cos(math.radians(a)) * L.RING_LED_R
                    R.place(led_obj(lit_state(i == c["set"]), r=1.5), lx, ly, z)
        elif t == "key":
            objs = key_obj(c)
            g = glyphs.get((round(x), round(y)))
            if g:
                objs += arrow(">" if g == "▸" else "<")
            R.place(objs, x, y, z)
        elif t == "fader":
            R.place(R.fader_plate(c["travel"]), x, y, z)
            if not STATE:
                ty = y + c["travel"] / 2 - c["travel"] * c["set"]
                R.place(R.fader_cap(), x, ty, z + 0.6)
        elif t == "lamp":
            on = c["on"] if not STATE else (STATE == "lit" or c["id"] == "power")
            R.place(R.jewel(c["color"], lit=on, r=c["r"]), x, y, z)
    return sc


# ---------------------------------------------------------------------------
# Knob filmstrips for the plugin: one per kind and cap colour, each frame with its own shadow.
SHADOW_MARGIN = 14


def strip_specs():
    """name -> (kind, cap box, angles). Continuous knobs sweep 121 frames; encoders turn all the way;
    a ring encoder only ever rests pointing at one of its lamps."""
    specs = {}
    for c in L.CONTROLS:
        if c["type"] != "knob":
            continue
        box = L.box_of(c["x"], c["y"])
        cap = T["caps"][box]
        cname = next(k for k, v in COLOURS.items() if v == cap)
        if c.get("encoder"):
            specs[f"enc_{cname}"] = (c["kind"], box, [4.0 * i for i in range(90)])
        elif c["kind"] == "ring":
            nm = f"ring_{cname}"
            prev = specs.get(nm, (None, None, []))[2]
            specs[nm] = ("ring", box, sorted(set(prev) | {round(a, 3) for a in c["steps"]}))
        else:
            n = L.STRIP_FRAMES
            specs[f"{c['kind']}_{cname}"] = (c["kind"], box, [-L.SWEEP + 2 * L.SWEEP * i / (n - 1) for i in range(n)])
    return specs


COLOURS = {"cream": (0.93, 0.89, 0.78), "orange": (0.93, 0.45, 0.18), "teal": (0.20, 0.60, 0.62),
           "mustard": (0.92, 0.72, 0.22), "red": (0.80, 0.22, 0.18)}


def render_strip(name, kind, box_id, angles, samples):
    from PIL import Image
    r = L.KNOBS[kind]
    box = 2 * r + 2 * SHADOW_MARGIN
    sc = R.sprite_scene(box, box, catcher=True)
    sc.cycles.samples = samples
    CUR["box"] = box_id
    root = R.parent_to(R.knob(kind, None, 0.0), "knob")
    tmp = os.path.join(BUILD, "frames", name)
    os.makedirs(tmp, exist_ok=True)
    frames = []
    for i, ang in enumerate(angles):
        root.rotation_euler.z = math.radians(-ang)
        p = os.path.join(tmp, f"{i:03d}.png")
        R.render_to(p)
        frames.append(p)
    ims = [Image.open(p) for p in frames]
    fw, fh = ims[0].size
    out = Image.new("RGBA", (fw, fh * len(ims)))
    for i, im in enumerate(ims):
        out.paste(im, (0, i * fh))
    os.makedirs(os.path.join(BUILD, "strips"), exist_ok=True)
    path = os.path.join(BUILD, "strips", f"{name}.png")
    out.save(path, optimize=True)
    print("wrote", path, out.size, len(angles), "frames")


def render_fader_cap(samples):
    sc = R.sprite_scene(*L.FADER_BOX, catcher=True)
    sc.cycles.samples = samples
    R.place(R.fader_cap(), 0, 0, 0.6)
    os.makedirs(os.path.join(BUILD, "sprites"), exist_ok=True)
    R.render_to(os.path.join(BUILD, "sprites", "fader_cap.png"))
    print("wrote fader_cap")


def render_crop(sc, x, y, w, h, path, fmt="JPEG"):
    sc.render.use_border = True
    sc.render.use_crop_to_border = True
    sc.render.border_min_x, sc.render.border_max_x = x / L.W, (x + w) / L.W
    sc.render.border_min_y, sc.render.border_max_y = 1.0 - (y + h) / L.H, 1.0 - y / L.H
    sc.render.image_settings.file_format = fmt
    sc.render.image_settings.quality = 93
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)
    print("wrote", path)


if __name__ == "__main__":
    if STRIPS:
        specs = strip_specs()
        names = list(specs) if STRIPS == "all" else STRIPS.split(",")
        for nm in names:
            if nm == "fader_cap":
                render_fader_cap(R.SAMPLES or 64)
                continue
            kind, box_id, angles = specs[nm]
            render_strip(nm, kind, box_id, angles, R.SAMPLES or 64)
        sys.exit(0)
    sc = build()
    if not NO_MAIN:
        sc.render.image_settings.file_format = "JPEG"
        sc.render.image_settings.quality = 92
        sc.render.filepath = os.path.join(BUILD, R.OUT_NAME or "one.jpg")
        bpy.ops.render.render(write_still=True)
        print("wrote", sc.render.filepath)
    if CROPS or PAGES:
        sc.render.resolution_x = int(round(L.W * CROP_PX))
        sc.render.resolution_y = int(round(L.H * CROP_PX))
        stem = (R.OUT_NAME or "one.jpg").rsplit(".", 1)[0]
        for item in (CROPS.split(";") if CROPS else []):
            nm, rect = item.split(":")
            x, y, w, h = [float(v) for v in rect.split(",")]
            render_crop(sc, x, y, w, h, os.path.join(BUILD, f"{stem}_{nm}.jpg"))
        if PAGES:      # the display, showing each SHIFT page in turn
            mat = bpy.data.materials["disp_face"]
            node = next(n for n in mat.node_tree.nodes if n.type == "TEX_IMAGE")
            D = L.DISPLAY
            for name in L.DISPLAY_PAGES:
                node.image = R.img(os.path.join(BUILD, f"display_{name}.png"))
                render_crop(sc, D["x"] - D["w"] / 2 - 10, D["y"] - D["h"] / 2 - 10, D["w"] + 20, D["h"] + 20,
                            os.path.join(BUILD, f"{stem}_{name}.jpg"))
