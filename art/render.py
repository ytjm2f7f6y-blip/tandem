"""Tandem: renders the faceplate with Blender (Cycles). Forked from Kontur and Sam's Desk.

    python3 render.py panel [--draft]     the panel background (JPEG)
    python3 render.py sprites [--draft]   knob filmstrips, toggles, the fader cap

World units are points (1 unit = 1 pt at 100%); canvas x runs right, canvas y
runs down, so Blender Y = -canvas y. Lights are suns plus a studio world, so a
part lights the same wherever it sits and sprites can be rendered at the origin.
"""
import math
import os
import sys

import bpy  # noqa: I001  (bpy first: it puts bmesh and mathutils on the path)
import bmesh
from mathutils import Vector
from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import importlib  # noqa: E402

import theme as TH  # noqa: E402

L = importlib.import_module(os.environ.get("TD_LAYOUT", "layout"))   # layout (stage 1) or layout_final (the mockups)
T = TH.THEMES[os.environ.get("TD_THEME", "family")]

HERE = os.path.dirname(os.path.abspath(__file__))
BUILD = os.path.join(HERE, os.environ.get("TD_BUILD", "build"))
ASSETS = BUILD
DRAFT = "--draft" in sys.argv
PREVIEW = "--preview" in sys.argv


def _arg(name):
    if name in sys.argv:
        return sys.argv[sys.argv.index(name) + 1]
    return None


PX = float(_arg("--px")) if _arg("--px") else None          # pixels per point (default: 1 draft, else 2)
SAMPLES = int(_arg("--samples")) if _arg("--samples") else None
OUT_NAME = _arg("--out")
STATE = _arg("--state")        # None: the mockup. "bare": the plugin's background (no knobs, switches or
                               # needles, nothing lit). "amber" / "green": everything lit, for the lit patches.
FLIP = _arg("--flip")          # a toggle id to render in its other position
CLOSEUPS = _arg("--closeups")  # "name:x,y,w,h;name:x,y,w,h" in points: close-up crops, as JPEGs
BORDER = _arg("--border")      # "x,y,w,h" in points: render only this region, to a PNG

Z_FRAME = 3.0
Z_CASS = 4.5

# Knob geometry per kind: skirt radius = layout radius.
KNOB_H = {"large": 17.0, "med": 15.0, "small": 12.0, "sel": 13.0}


# ---------------------------------------------------------------------------
# Scene
def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    _mats.clear()   # the old scene's materials are gone
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = SAMPLES or (24 if DRAFT else (56 if PREVIEW else 160))
    sc.cycles.use_adaptive_sampling = True
    sc.cycles.adaptive_threshold = 0.02 if DRAFT else 0.006
    sc.cycles.max_bounces = 8
    sc.cycles.glossy_bounces = 4
    sc.cycles.transmission_bounces = 8
    sc.cycles.use_denoising = True
    try:
        sc.cycles.denoiser = "OPENIMAGEDENOISE"
    except Exception:
        pass
    sc.view_settings.view_transform = "AgX"
    try:
        sc.view_settings.look = "AgX - Base Contrast"
    except Exception:
        pass
    sc.view_settings.exposure = 0.0
    sc.render.image_settings.color_depth = "8"
    sc.cycles.seed = 7
    return sc


def look_dir(obj, direction):
    obj.rotation_euler = Vector(direction).normalized().to_track_quat("-Z", "Y").to_euler()


def lights():
    # Key: high from the upper left, soft. Fill: low and weak from the right.
    key = bpy.data.lights.new("key", "SUN")
    key.energy = 3.2
    key.angle = math.radians(9)
    key.color = (1.0, 0.97, 0.92)
    ko = bpy.data.objects.new("key", key)
    bpy.context.scene.collection.objects.link(ko)
    look_dir(ko, (0.55, -0.62, -1.25))
    fill = bpy.data.lights.new("fill", "SUN")
    fill.energy = 0.55
    fill.angle = math.radians(25)
    fill.color = (0.92, 0.95, 1.0)
    fo = bpy.data.objects.new("fill", fill)
    bpy.context.scene.collection.objects.link(fo)
    look_dir(fo, (-0.9, 0.2, -1.0))

    # Studio world: a dim room with a large softbox up and to the left (for reflections).
    w = bpy.data.worlds.new("studio")
    bpy.context.scene.world = w
    w.use_nodes = True
    nt = w.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputWorld")
    bg = nt.nodes.new("ShaderNodeBackground")
    tc = nt.nodes.new("ShaderNodeTexCoord")
    dot = nt.nodes.new("ShaderNodeVectorMath")
    dot.operation = "DOT_PRODUCT"
    kd = Vector((-0.45, 0.55, 1.0)).normalized()
    dot.inputs[1].default_value = kd
    mr = nt.nodes.new("ShaderNodeMapRange")
    mr.inputs["From Min"].default_value = 0.86
    mr.inputs["From Max"].default_value = 0.97
    mr.inputs["To Min"].default_value = 0.0
    mr.inputs["To Max"].default_value = 3.2
    up = nt.nodes.new("ShaderNodeSeparateXYZ")
    amb = nt.nodes.new("ShaderNodeMapRange")
    amb.inputs["From Min"].default_value = -1.0
    amb.inputs["From Max"].default_value = 1.0
    amb.inputs["To Min"].default_value = 0.03
    amb.inputs["To Max"].default_value = 0.30
    add = nt.nodes.new("ShaderNodeMath")
    add.operation = "ADD"
    nt.links.new(tc.outputs["Generated"], dot.inputs[0])
    nt.links.new(dot.outputs["Value"], mr.inputs["Value"])
    nt.links.new(tc.outputs["Generated"], up.inputs[0])
    nt.links.new(up.outputs["Z"], amb.inputs["Value"])
    nt.links.new(mr.outputs["Result"], add.inputs[0])
    nt.links.new(amb.outputs["Result"], add.inputs[1])
    bg.inputs["Color"].default_value = (1.0, 0.96, 0.9, 1)
    nt.links.new(add.outputs["Value"], bg.inputs["Strength"])
    nt.links.new(bg.outputs["Background"], out.inputs["Surface"])


def camera(cx, cy, width, height, px_per_pt):
    cam = bpy.data.cameras.new("cam")
    cam.type = "ORTHO"
    cam.ortho_scale = max(width, height)
    cam.clip_start = 1
    cam.clip_end = 3000
    co = bpy.data.objects.new("cam", cam)
    bpy.context.scene.collection.objects.link(co)
    co.location = (cx, -cy, 1500)
    co.rotation_euler = (0, 0, 0)
    sc = bpy.context.scene
    sc.camera = co
    sc.render.resolution_x = int(round(width * px_per_pt))
    sc.render.resolution_y = int(round(height * px_per_pt))
    sc.render.resolution_percentage = 100
    sc.render.pixel_aspect_x = sc.render.pixel_aspect_y = 1
    return co


# ---------------------------------------------------------------------------
# Materials
def new_mat(name):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    return m, nt, bsdf


def srgb(c):
    def f(v):
        return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4
    return (f(c[0]), f(c[1]), f(c[2]), 1.0)


def img(path, noncolor=True):
    im = bpy.data.images.load(path, check_existing=True)
    if noncolor:
        im.colorspace_settings.name = "Non-Color"
    return im


def canvas_uv(nt):
    """World position -> canvas UV (0..1 across the whole faceplate)."""
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    mp = nt.nodes.new("ShaderNodeMapping")
    mp.inputs["Scale"].default_value = (1.0 / L.W, 1.0 / L.H, 1.0)
    mp.inputs["Location"].default_value = (0.0, 1.0, 0.0)
    nt.links.new(geo.outputs["Position"], mp.inputs["Vector"])
    return mp.outputs["Vector"]


def tex(nt, image, vec, interp="Cubic"):
    t = nt.nodes.new("ShaderNodeTexImage")
    t.image = image
    t.interpolation = interp
    t.extension = "CLIP"
    nt.links.new(vec, t.inputs["Vector"])
    return t


def math_node(nt, op, a, b=None, clamp=False):
    n = nt.nodes.new("ShaderNodeMath")
    n.operation = op
    n.use_clamp = clamp
    for i, v in enumerate((a, b)):
        if v is None:
            continue
        if isinstance(v, (int, float)):
            n.inputs[i].default_value = v
        else:
            nt.links.new(v, n.inputs[i])
    return n.outputs[0]


def mix_rgb(nt, fac, a, b, blend="MIX"):
    n = nt.nodes.new("ShaderNodeMix")
    n.data_type = "RGBA"
    n.blend_type = blend
    if isinstance(fac, (int, float)):
        n.inputs[0].default_value = fac
    else:
        nt.links.new(fac, n.inputs[0])
    for idx, v in ((6, a), (7, b)):
        if isinstance(v, tuple):
            n.inputs[idx].default_value = v
        else:
            nt.links.new(v, n.inputs[idx])
    return n.outputs[2]


def hammertone(name, base, metal, ink_layers, wear=True):
    """Hammer-finish paint with printed ink layers [(mask image, ink colour, roughness)]."""
    m, nt, bsdf = new_mat(name)
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    vor = nt.nodes.new("ShaderNodeTexVoronoi")
    vor.voronoi_dimensions = "3D"
    vor.feature = "F1"
    vor.inputs["Scale"].default_value = 0.17
    vor.inputs["Randomness"].default_value = 1.0
    nt.links.new(geo.outputs["Position"], vor.inputs["Vector"])
    noi = nt.nodes.new("ShaderNodeTexNoise")
    noi.inputs["Scale"].default_value = 0.9
    noi.inputs["Detail"].default_value = 6
    nt.links.new(geo.outputs["Position"], noi.inputs["Vector"])
    big = nt.nodes.new("ShaderNodeTexNoise")
    big.inputs["Scale"].default_value = 0.012
    big.inputs["Detail"].default_value = 3
    nt.links.new(geo.outputs["Position"], big.inputs["Vector"])

    dist = math_node(nt, "POWER", vor.outputs["Distance"], 0.8)
    bump1 = nt.nodes.new("ShaderNodeBump")
    bump1.inputs["Strength"].default_value = 0.22
    bump1.inputs["Distance"].default_value = 0.6
    nt.links.new(dist, bump1.inputs["Height"])
    bump2 = nt.nodes.new("ShaderNodeBump")
    bump2.inputs["Strength"].default_value = 0.05
    bump2.inputs["Distance"].default_value = 0.2
    nt.links.new(noi.outputs["Fac"], bump2.inputs["Height"])
    nt.links.new(bump1.outputs["Normal"], bump2.inputs["Normal"])

    uv = canvas_uv(nt)
    col = mix_rgb(nt, math_node(nt, "MULTIPLY", vor.outputs["Distance"], 0.25), base, (base[0] * 0.86, base[1] * 0.86, base[2] * 0.86, 1))
    col = mix_rgb(nt, math_node(nt, "MULTIPLY", big.outputs["Fac"], 0.35), col, (base[0] * 1.08, base[1] * 1.06, base[2] * 1.0, 1))
    rough = math_node(nt, "ADD", math_node(nt, "MULTIPLY", noi.outputs["Fac"], 0.12), 0.40)
    wear_fac = None
    if wear:
        wt = tex(nt, img(os.path.join(BUILD, "wear.png")), uv)
        gt = tex(nt, img(os.path.join(BUILD, "grime.png")), uv)
        wear_fac = wt.outputs["Color"]
        col = mix_rgb(nt, math_node(nt, "MULTIPLY", wt.outputs["Color"], 0.35), col, (base[0] * 1.25, base[1] * 1.25, base[2] * 1.22, 1))
        rough = math_node(nt, "SUBTRACT", rough, math_node(nt, "MULTIPLY", wt.outputs["Color"], 0.18))
        col = mix_rgb(nt, math_node(nt, "MULTIPLY", gt.outputs["Color"], 0.55), col, (0.10, 0.085, 0.06, 1))
        rough = math_node(nt, "ADD", rough, math_node(nt, "MULTIPLY", gt.outputs["Color"], 0.2))
    for path, ink, ink_rough in ink_layers:
        it = tex(nt, img(path), uv)
        fac = it.outputs["Color"]
        if wear_fac is not None:
            fac = math_node(nt, "MULTIPLY", fac, math_node(nt, "SUBTRACT", 1.0, math_node(nt, "MULTIPLY", wear_fac, 0.8)), clamp=True)
        col = mix_rgb(nt, fac, col, ink)
        rough = mix_rgb(nt, fac, rough, (ink_rough, ink_rough, ink_rough, 1))
    nt.links.new(col, bsdf.inputs["Base Color"])
    nt.links.new(rough, bsdf.inputs["Roughness"])
    bsdf.inputs["Metallic"].default_value = metal
    nt.links.new(bump2.outputs["Normal"], bsdf.inputs["Normal"])
    return m


def simple(name, color, metal=0.0, rough=0.4, **extra):
    m, nt, bsdf = new_mat(name)
    bsdf.inputs["Base Color"].default_value = color
    bsdf.inputs["Metallic"].default_value = metal
    bsdf.inputs["Roughness"].default_value = rough
    for k, v in extra.items():
        bsdf.inputs[k].default_value = v
    return m


def phenolic():
    """Black bakelite: glossy, with faint micro-scratches."""
    m, nt, bsdf = new_mat("phenolic")
    bsdf.inputs["Base Color"].default_value = (0.012, 0.011, 0.010, 1)
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    noi = nt.nodes.new("ShaderNodeTexNoise")
    noi.inputs["Scale"].default_value = 2.5
    noi.inputs["Detail"].default_value = 8
    nt.links.new(geo.outputs["Position"], noi.inputs["Vector"])
    nt.links.new(math_node(nt, "ADD", math_node(nt, "MULTIPLY", noi.outputs["Fac"], 0.18), 0.17), bsdf.inputs["Roughness"])
    bsdf.inputs["Coat Weight"].default_value = 0.35
    bsdf.inputs["Coat Roughness"].default_value = 0.12
    return m


def brushed_alu():
    m, nt, bsdf = new_mat("alu")
    bsdf.inputs["Base Color"].default_value = (0.80, 0.80, 0.79, 1)
    bsdf.inputs["Metallic"].default_value = 1.0
    bsdf.inputs["Roughness"].default_value = 0.26
    bsdf.inputs["Anisotropic"].default_value = 0.85
    # Radial (spun) brushing: tangent around the knob axis.
    tg = nt.nodes.new("ShaderNodeTangent")
    tg.direction_type = "RADIAL"
    tg.axis = "Z"
    nt.links.new(tg.outputs["Tangent"], bsdf.inputs["Tangent"])
    return m


def paint_white():
    return simple("paint_white", srgb((0.93, 0.92, 0.87)), rough=0.45)


def glass(name, color=(1, 1, 1, 1), rough=0.02):
    m, nt, bsdf = new_mat(name)
    bsdf.inputs["Base Color"].default_value = color
    bsdf.inputs["Transmission Weight"].default_value = 1.0
    bsdf.inputs["Roughness"].default_value = rough
    bsdf.inputs["IOR"].default_value = 1.5
    return m


def powder(name, base, ink_layers, wear=True):
    """Powder coat: an even satin finish with a fine orange-peel texture, printed ink on top."""
    m, nt, bsdf = new_mat(name)
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    peel = nt.nodes.new("ShaderNodeTexNoise")
    peel.inputs["Scale"].default_value = 1.6
    peel.inputs["Detail"].default_value = 3
    nt.links.new(geo.outputs["Position"], peel.inputs["Vector"])
    fine = nt.nodes.new("ShaderNodeTexNoise")
    fine.inputs["Scale"].default_value = 9.0
    fine.inputs["Detail"].default_value = 2
    nt.links.new(geo.outputs["Position"], fine.inputs["Vector"])
    big = nt.nodes.new("ShaderNodeTexNoise")
    big.inputs["Scale"].default_value = 0.01
    big.inputs["Detail"].default_value = 3
    nt.links.new(geo.outputs["Position"], big.inputs["Vector"])
    b1 = nt.nodes.new("ShaderNodeBump")
    b1.inputs["Strength"].default_value = 0.10
    b1.inputs["Distance"].default_value = 0.25
    nt.links.new(peel.outputs["Fac"], b1.inputs["Height"])
    b2 = nt.nodes.new("ShaderNodeBump")
    b2.inputs["Strength"].default_value = 0.03
    b2.inputs["Distance"].default_value = 0.1
    nt.links.new(fine.outputs["Fac"], b2.inputs["Height"])
    nt.links.new(b1.outputs["Normal"], b2.inputs["Normal"])
    uv = canvas_uv(nt)
    col = mix_rgb(nt, math_node(nt, "MULTIPLY", big.outputs["Fac"], 0.4), base, (base[0] * 1.05, base[1] * 1.04, base[2] * 1.02, 1))
    rough = math_node(nt, "ADD", math_node(nt, "MULTIPLY", peel.outputs["Fac"], 0.10), 0.40)
    wear_fac = None
    if wear:
        wt = tex(nt, img(os.path.join(BUILD, "wear.png")), uv)
        gt = tex(nt, img(os.path.join(BUILD, "grime.png")), uv)
        wear_fac = wt.outputs["Color"]
        col = mix_rgb(nt, math_node(nt, "MULTIPLY", wt.outputs["Color"], 0.22), col, (base[0] * 1.12, base[1] * 1.12, base[2] * 1.1, 1))
        rough = math_node(nt, "SUBTRACT", rough, math_node(nt, "MULTIPLY", wt.outputs["Color"], 0.15))
        col = mix_rgb(nt, math_node(nt, "MULTIPLY", gt.outputs["Color"], 0.35), col, (base[0] * 0.55, base[1] * 0.55, base[2] * 0.55, 1))
    for path, ink, ink_rough in ink_layers:
        it = tex(nt, img(path), uv)
        fac = it.outputs["Color"]
        if wear_fac is not None:
            fac = math_node(nt, "MULTIPLY", fac, math_node(nt, "SUBTRACT", 1.0, math_node(nt, "MULTIPLY", wear_fac, 0.7)), clamp=True)
        col = mix_rgb(nt, fac, col, ink)
        rough = mix_rgb(nt, fac, rough, (ink_rough, ink_rough, ink_rough, 1))
    nt.links.new(col, bsdf.inputs["Base Color"])
    nt.links.new(rough, bsdf.inputs["Roughness"])
    bsdf.inputs["Coat Weight"].default_value = 0.08
    nt.links.new(b2.outputs["Normal"], bsdf.inputs["Normal"])
    return m


def walnut():
    """Oiled walnut: dark streaky grain running the length of the cheek."""
    m, nt, bsdf = new_mat("walnut")
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    mp = nt.nodes.new("ShaderNodeMapping")
    mp.inputs["Scale"].default_value = (0.6, 0.025, 0.6)
    nt.links.new(geo.outputs["Position"], mp.inputs["Vector"])
    grain = nt.nodes.new("ShaderNodeTexWave")
    grain.wave_type = "BANDS"
    grain.inputs["Scale"].default_value = 2.2
    grain.inputs["Distortion"].default_value = 7.0
    grain.inputs["Detail"].default_value = 4.0
    nt.links.new(mp.outputs["Vector"], grain.inputs["Vector"])
    pores = nt.nodes.new("ShaderNodeTexNoise")
    pores.inputs["Scale"].default_value = 3.0
    pores.inputs["Detail"].default_value = 10
    nt.links.new(mp.outputs["Vector"], pores.inputs["Vector"])
    col = mix_rgb(nt, grain.outputs["Fac"], srgb((0.17, 0.09, 0.05)), srgb((0.36, 0.21, 0.11)))
    col = mix_rgb(nt, math_node(nt, "MULTIPLY", pores.outputs["Fac"], 0.35), col, srgb((0.08, 0.04, 0.02)))
    nt.links.new(col, bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.42
    bsdf.inputs["Coat Weight"].default_value = 0.35
    bsdf.inputs["Coat Roughness"].default_value = 0.2
    bp = nt.nodes.new("ShaderNodeBump")
    bp.inputs["Strength"].default_value = 0.08
    nt.links.new(pores.outputs["Fac"], bp.inputs["Height"])
    nt.links.new(bp.outputs["Normal"], bsdf.inputs["Normal"])
    return m


def plastic(name, c, rough=0.32, sss=0.08):
    m = simple(name, srgb(c), rough=rough)
    bsdf = m.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Coat Weight"].default_value = 0.25
    bsdf.inputs["Coat Roughness"].default_value = 0.15
    bsdf.inputs["Subsurface Weight"].default_value = sss
    bsdf.inputs["Subsurface Radius"].default_value = (1.0, 0.8, 0.6)
    return m


# ---------------------------------------------------------------------------
# Geometry helpers
def link(obj):
    bpy.context.scene.collection.objects.link(obj)
    return obj


def mesh_obj(name, bm, mat=None):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    o = link(bpy.data.objects.new(name, me))
    if mat:
        o.data.materials.append(mat)
    return o


def box(name, x0, y0, x1, y1, z0, z1, mat, bevel=1.0):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    o = mesh_obj(name, bm, mat)
    o.scale = (x1 - x0, y1 - y0, z1 - z0)
    o.location = ((x0 + x1) / 2, -(y0 + y1) / 2, (z0 + z1) / 2)
    bpy.context.view_layer.objects.active = o
    o.select_set(True)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    o.select_set(False)
    if bevel > 0:
        b = o.modifiers.new("bevel", "BEVEL")
        b.width = bevel
        b.segments = 4
        b.limit_method = "ANGLE"
    return o


def prism(name, outline, z0, z1, mat, bevel=0.0, taper=1.0, smooth=True):
    """Extrude a closed 2D outline (points in local xy) from z0 to z1."""
    bm = bmesh.new()
    vs = [bm.verts.new((x, y, z0)) for x, y in outline]
    f = bm.faces.new(vs)
    ext = bmesh.ops.extrude_face_region(bm, geom=[f])
    top = [e for e in ext["geom"] if isinstance(e, bmesh.types.BMVert)]
    for v in top:
        v.co.z = z1
        v.co.x *= taper
        v.co.y *= taper
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    o = mesh_obj(name, bm, mat)
    if smooth:
        for p in o.data.polygons:
            p.use_smooth = True
    if bevel > 0:
        b = o.modifiers.new("bevel", "BEVEL")
        b.width = bevel
        b.segments = 5
        b.limit_method = "ANGLE"
        b.angle_limit = math.radians(40)
        o.modifiers.new("wn", "WEIGHTED_NORMAL")
    return o


def circle_pts(r, n=96, flutes=0, depth=0.0, phase=0.0):
    pts = []
    for i in range(n):
        a = 2 * math.pi * i / n + phase
        rr = r
        if flutes:
            rr = r - depth * (0.5 - 0.5 * math.cos(flutes * a)) ** 3
        pts.append((rr * math.cos(a), rr * math.sin(a)))
    return pts


def rounded_rect_pts(w, h, rad, n=8):
    pts = []
    corners = [(w / 2 - rad, h / 2 - rad, 0), (-w / 2 + rad, h / 2 - rad, 90), (-w / 2 + rad, -h / 2 + rad, 180), (w / 2 - rad, -h / 2 + rad, 270)]
    for cx_, cy_, a0 in corners:
        for i in range(n + 1):
            a = math.radians(a0 + 90 * i / n)
            pts.append((cx_ + rad * math.cos(a), cy_ + rad * math.sin(a)))
    return pts


def place(objs, x, y, z=0.0, rot_deg=0.0):
    for o in objs:
        o.location.x += x
        o.location.y += -y
        o.location.z += z
        if rot_deg:
            # rotate about the part's axis at (x, -y)
            o.rotation_euler.z += math.radians(-rot_deg)
    return objs


def parent_to(objs, name="part"):
    root = link(bpy.data.objects.new(name, None))
    for o in objs:
        o.parent = root
    return root


# ---------------------------------------------------------------------------
# Parts. Each builder returns objects centred on (0, 0) sitting on z = 0.
_mats = {}


def mats():
    if not _mats:
        _mats["phen"] = phenolic()
        _mats["alu"] = brushed_alu()
        _mats["white"] = paint_white()
        _mats["nickel"] = simple("nickel", srgb((0.78, 0.76, 0.72)), metal=1.0, rough=0.24)
        _mats["chrome"] = simple("chrome", srgb((0.86, 0.86, 0.86)), metal=1.0, rough=0.10)
        _mats["dark"] = simple("slot", (0.01, 0.01, 0.01, 1), rough=0.8)
        _mats["black_plastic"] = simple("black_plastic", (0.02, 0.02, 0.022, 1), rough=0.35)
    return _mats


def pointer_mat(colour):
    key = "ptr_" + (colour or "white")
    M = mats()
    if key not in M:
        if colour is None:
            M[key] = M["white"]
        else:
            mul = L.POINTER_COLOURS[colour]
            base = (0.93, 0.92, 0.87)
            M[key] = simple(key, srgb((base[0] * mul[0], base[1] * mul[1], base[2] * mul[2])), rough=0.45)
    return M[key]


def section_of(x):
    for c in L.CASSETTES:
        if c["x0"] <= x < c["x1"]:
            return c["id"]
    return "ctl"


def knob(kind, pointer=None, x=0.0):
    """Knob pointing at 12 o'clock (canvas up = +Y), in the theme's style."""
    style = T["knob"]
    if style == "family":
        return knob_family(kind, pointer)
    M = mats()
    r = L.KNOBS[kind]
    h = KNOB_H[kind]
    line_col = L.POINTER_COLOURS[pointer] if pointer else None
    if style == "ivory":
        cream = M.setdefault("cream", plastic("cream", (0.90, 0.86, 0.74)))
        ink = M.setdefault("ink_black", simple("ink_black", (0.02, 0.02, 0.02, 1), rough=0.5))
        lm = simple("ivline_" + str(pointer), srgb(tuple(0.25 * v for v in line_col)) if line_col else (0.02, 0.02, 0.02, 1), rough=0.5)
        if kind == "sel":
            objs = [prism("boss", circle_pts(r * 0.62, 64), 0, 4.0, cream, bevel=0.8)]
            tip, tail, wid = r * 0.98, r * 0.80, r * 0.56
            body = sorted_ccw([(-wid / 2, -tail + wid * 0.45), (wid / 2, -tail + wid * 0.45), (wid * 0.32, tip - 4), (wid * 0.12, tip), (-wid * 0.12, tip),
                               (-wid * 0.32, tip - 4), (-wid * 0.45, -tail + 1), (wid * 0.45, -tail + 1), (0, -tail - 1)])
            objs.append(prism("bar", body, 3.5, h, cream, bevel=1.6, taper=0.94))
            objs.append(prism("line", [(-0.8, 2.0), (0.8, 2.0), (0.8, tip - 2.5), (-0.8, tip - 2.5)], h - 0.6, h + 0.12, ink, smooth=False))
            return objs
        objs = [prism("skirt", circle_pts(r, 128), 0, 2.8, cream, bevel=0.9, taper=0.97),
                prism("index_skirt", [(-0.9, r * 0.76), (0.9, r * 0.76), (0.9, r * 0.97), (-0.9, r * 0.97)], 2.5, 2.85, lm, smooth=False)]
        br = r * 0.72
        objs.append(prism("body", circle_pts(br, 240, flutes=18, depth=r * 0.05), 2.6, h, cream, bevel=1.6, taper=0.94))
        objs.append(prism("dome", circle_pts(br * 0.84, 96), h - 0.4, h + 0.6, cream, bevel=0.7))
        objs.append(prism("index_top", [(-0.75, br * 0.15), (0.75, br * 0.15), (0.75, br * 0.80), (-0.75, br * 0.80)], h + 0.35, h + 0.7, lm, smooth=False))
        return objs
    if style == "machined":
        anod = M.setdefault("anod", simple("anod", (0.018, 0.018, 0.022, 1), metal=0.7, rough=0.33))
        white = M["white"]
        lm = pointer_mat(pointer)
        if kind == "sel":
            objs = [prism("boss", circle_pts(r * 0.62, 64), 0, 4.0, anod, bevel=0.8)]
            tip, wid = r * 0.98, r * 0.5
            body = [(-wid / 2, -r * 0.7), (wid / 2, -r * 0.7), (wid * 0.4, tip - 3), (wid * 0.15, tip), (-wid * 0.15, tip), (-wid * 0.4, tip - 3)]
            objs.append(prism("bar", sorted_ccw(body), 3.5, h, M["alu"], bevel=1.2, taper=0.95))
            objs.append(prism("line", [(-0.8, 2.0), (0.8, 2.0), (0.8, tip - 2.5), (-0.8, tip - 2.5)], h - 0.5, h + 0.12, anod, smooth=False))
            return objs
        objs = [prism("base", circle_pts(r, 128), 0, 1.6, anod, bevel=0.4),
                prism("index_base", [(-0.85, r * 0.84), (0.85, r * 0.84), (0.85, r * 0.98), (-0.85, r * 0.98)], 1.4, 1.65, white, smooth=False)]
        kr = r * 0.82
        objs.append(prism("knurl", circle_pts(kr, 288, flutes=72, depth=0.45), 1.4, h - 0.8, M["alu"], bevel=0.5, taper=0.97))
        objs.append(prism("top", circle_pts(kr * 0.86, 96), h - 1.2, h + 0.15, anod, bevel=0.35))
        objs.append(prism("index_top", [(-0.7, kr * 0.15), (0.7, kr * 0.15), (0.7, kr * 0.80), (-0.7, kr * 0.80)], h + 0.05, h + 0.3, lm, smooth=False))
        return objs
    # capped: black knob, a coloured cap for its section
    blk = M.setdefault("softblack", simple("softblack", (0.025, 0.025, 0.028, 1), rough=0.55))
    capc = T["caps"][section_of(x)]
    capm = M.setdefault("cap_" + section_of(x), plastic("cap_" + section_of(x), capc, rough=0.25, sss=0.05))
    dark = M.setdefault("ink_black", simple("ink_black", (0.02, 0.02, 0.02, 1), rough=0.5))
    if kind == "sel":
        objs = [prism("boss", circle_pts(r * 0.62, 64), 0, 4.0, blk, bevel=0.8)]
        tip, wid = r * 0.98, r * 0.52
        body = [(-wid / 2, -r * 0.72), (wid / 2, -r * 0.72), (wid * 0.36, tip - 3), (wid * 0.14, tip), (-wid * 0.14, tip), (-wid * 0.36, tip - 3)]
        objs.append(prism("bar", sorted_ccw(body), 3.5, h, blk, bevel=1.4, taper=0.94))
        objs.append(prism("line", [(-1.0, 1.0), (1.0, 1.0), (1.0, tip - 2.5), (-1.0, tip - 2.5)], h - 0.6, h + 0.15, capm, smooth=False))
        return objs
    objs = [prism("skirt", circle_pts(r, 128), 0, 3.0, blk, bevel=0.9, taper=0.97),
            prism("index_skirt", [(-0.9, r * 0.77), (0.9, r * 0.77), (0.9, r * 0.97), (-0.9, r * 0.97)], 2.7, 3.05, M["white"], smooth=False)]
    br = r * 0.74
    objs.append(prism("body", circle_pts(br, 240, flutes=24, depth=r * 0.05), 3.0, h, blk, bevel=1.5, taper=0.95))
    objs.append(prism("cap", circle_pts(br * 0.86, 96), h - 0.6, h + 0.9, capm, bevel=0.8))
    objs.append(prism("index_top", [(-0.7, br * 0.12), (0.7, br * 0.12), (0.7, br * 0.80), (-0.7, br * 0.80)], h + 0.55, h + 0.95, dark, smooth=False))
    return objs


def knob_family(kind, pointer=None):
    """Knob pointing at 12 o'clock (canvas up = +Y)."""
    M = mats()
    PM = pointer_mat(pointer)
    r = L.KNOBS[kind]
    h = KNOB_H[kind]
    objs = []
    if kind == "sel":
        # A bar ("chicken-head") knob on a round boss.
        objs.append(prism("boss", circle_pts(r * 0.62, 64), 0, 4.0, M["phen"], bevel=0.8))
        tip, tail, wid = r * 0.98, r * 0.80, r * 0.56
        out = []
        n = 10
        for i in range(n + 1):           # rounded tail
            a = math.pi + math.pi * i / n
            out.append((wid / 2 * math.cos(a) * 1.0, -tail + wid / 2 * 0.9 + wid / 2 * math.sin(a) * 0.9))
        out = [(x, y) for x, y in out]
        out = [(wid / 2, -tail + wid * 0.45)] + out[::-1] if False else out
        body = [(-wid / 2, -tail + wid * 0.45)] + [(p[0], p[1]) for p in out] + [(wid / 2, -tail + wid * 0.45), (wid * 0.32, tip - 4), (wid * 0.12, tip), (-wid * 0.12, tip), (-wid * 0.32, tip - 4)]
        body = sorted_ccw(body)
        objs.append(prism("bar", body, 3.5, h, M["phen"], bevel=1.6, taper=0.94))
        objs.append(prism("line", [(-0.85, 2.0), (0.85, 2.0), (0.85, tip - 2.5), (-0.85, tip - 2.5)], h - 0.6, h + 0.12, M["white"], smooth=False))
        return objs
    # Round knobs: skirt, fluted body, spun aluminium cap, white index.
    objs.append(prism("skirt", circle_pts(r, 128), 0, 3.6, M["phen"], bevel=0.9, taper=0.98))
    objs.append(prism("index_skirt", [(-0.95, r * 0.77), (0.95, r * 0.77), (0.95, r * 0.975), (-0.95, r * 0.975)], 3.3, 3.62, PM, smooth=False))
    body_r = r * 0.74
    objs.append(prism("body", circle_pts(body_r, 240, flutes=24, depth=r * 0.06), 3.4, h, M["phen"], bevel=1.5, taper=0.95))
    cap_r = body_r * 0.66
    if kind != "small":
        objs.append(prism("cap", circle_pts(cap_r, 96), h - 0.6, h + 0.25, M["alu"], bevel=0.35))
        objs.append(prism("index_top", [(-0.8, cap_r + 0.6), (0.8, cap_r + 0.6), (0.8, body_r * 0.95 - 1.2), (-0.8, body_r * 0.95 - 1.2)], h - 0.3, h + 0.05, PM, smooth=False))
    else:
        objs.append(prism("index_top", [(-0.8, 1.5), (0.8, 1.5), (0.8, body_r * 0.95 - 1.2), (-0.8, body_r * 0.95 - 1.2)], h - 0.3, h + 0.05, PM, smooth=False))
    return objs


def sorted_ccw(pts):
    cxp = sum(p[0] for p in pts) / len(pts)
    cyp = sum(p[1] for p in pts) / len(pts)
    return sorted(pts, key=lambda p: math.atan2(p[1] - cyp, p[0] - cxp))


def screw(rot):
    M = mats()
    objs = [prism("screw_head", circle_pts(4.0, 64), 0, 1.4, M["nickel"], bevel=0.7)]
    s = prism("slot", [(-3.6, -0.45), (3.6, -0.45), (3.6, 0.45), (-3.6, 0.45)], 0.9, 1.5, M["dark"], smooth=False)
    s.rotation_euler.z = math.radians(rot)
    objs.append(s)
    return objs


def toggle(pos):
    """A bat-lever toggle that moves left and right: pos is 'l', 'c' or 'r'."""
    M = mats()
    hexpts = [(6.2 * math.cos(math.radians(30 + 60 * i)), 6.2 * math.sin(math.radians(30 + 60 * i))) for i in range(6)]
    objs = [prism("washer", circle_pts(7.6, 64), 0, 0.6, M["nickel"], bevel=0.2),
            prism("nut", hexpts, 0.6, 2.8, M["nickel"], bevel=0.4, smooth=False),
            prism("bush", circle_pts(3.9, 48), 2.8, 5.5, M["nickel"], bevel=0.3)]
    lev = prism("lever", circle_pts(1.5, 32), 0, 15.5, M["chrome"], bevel=0.4, taper=1.55)
    tipb = prism("lever_tip", circle_pts(2.3, 32), 15.0, 16.6, M["chrome"], bevel=0.8)
    ang = {"l": -27.0, "c": 0.0, "r": 27.0}[pos]
    for o in (lev, tipb):
        o.location.z = 4.6
        o.rotation_euler.y = math.radians(ang)
    objs += [lev, tipb]
    return objs


def push_key(w, h, lit, led=True):
    """A square-shouldered push key in a dark well, an LED lens above it."""
    M = mats()
    fin = T["key_finish"]
    capm = M.setdefault("keycap", plastic("keycap", T["key"], rough=0.62 if fin == "rubber" else 0.34, sss=0.0 if fin == "rubber" else 0.06))
    objs = [prism("key_well", rounded_rect_pts(w + 3.4, h + 3.4, 2.6), -0.2, 0.4, M["black_plastic"], bevel=0.3),
            prism("key_cap", rounded_rect_pts(w, h, 2.2), 0.3, 6.2, capm, bevel=1.2, taper=0.92)]
    if led:
        c = {"red": (1.0, 0.004, 0.0), "amber": (1.0, 0.20, 0.0)}[T["led"]]
        m, nt, bsdf = new_mat(f"led_{T['led']}_{lit}")
        bsdf.inputs["Base Color"].default_value = (*c, 1) if lit else (c[0] * 0.10, c[1] * 0.06, c[2] * 0.06, 1)
        bsdf.inputs["Roughness"].default_value = 0.08
        bsdf.inputs["Transmission Weight"].default_value = 0.3
        if lit:
            bsdf.inputs["Emission Color"].default_value = (*c, 1)
            bsdf.inputs["Emission Strength"].default_value = 1.1
        ly = h / 2 + 5.8
        objs.append(prism("led_ring", circle_pts(2.6, 32), 0, 0.8, M["black_plastic"], bevel=0.2))
        objs[-1].location.y = ly
        bm = bmesh.new()
        bmesh.ops.create_uvsphere(bm, u_segments=24, v_segments=12, radius=1.8)
        lens = mesh_obj("led", bm, m)
        lens.scale = (1, 1, 0.7)
        lens.location = (0, ly, 0.7)
        for p in lens.data.polygons:
            p.use_smooth = True
        objs.append(lens)
    return objs


def fader_plate(travel):
    """The fader's cover: a nickel plate with a slot down it, the slot dark inside."""
    M = mats()
    w = L.FADER_PLATE_W
    h = travel + L.FADER_CAP[1] + 6
    plate = frame_curve("fader_plate", rounded_rect_pts(w, h, 2.5), rounded_rect_pts(2.6, travel + 6, 1.2), 0.5, 0.25, M["nickel"])
    plate.location.z = 0.05
    slot = prism("fader_slot", rounded_rect_pts(3.2, travel + 7, 1.3), -0.4, 0.2, M["dark"])
    return [plate, slot]


def fader_cap():
    """A broadcast-desk fader knob: a black block with a dished top, grip ridges and a white line."""
    M = mats()
    cw, ch = L.FADER_CAP
    style = T["fader"]
    body = {"ivory": lambda: M.setdefault("cream", plastic("cream", (0.90, 0.86, 0.74))),
            "white": lambda: M.setdefault("fwhite", plastic("fwhite", (0.94, 0.93, 0.90))),
            "machined": lambda: M.setdefault("anod", simple("anod", (0.018, 0.018, 0.022, 1), metal=0.7, rough=0.33))}.get(style, lambda: M["phen"])()
    line = M["white"] if style in ("phenolic", "machined") else M.setdefault("ink_black", simple("ink_black", (0.02, 0.02, 0.02, 1), rough=0.5))
    objs = [prism("cap_stem", rounded_rect_pts(4.0, 8.0, 1.0), 0, 4.0, M["nickel"], bevel=0.3)]
    objs.append(prism("cap_body", rounded_rect_pts(cw, ch, 3.0), 3.5, 11.0, body, bevel=1.4, taper=0.92))
    for k in (-1, 1):       # ridges across the top, front and back, for the finger
        for j in range(3):
            y = k * (ch / 2 - 3.2 - j * 1.8)
            objs.append(prism("ridge", [(-cw * 0.36, y - 0.35), (cw * 0.36, y - 0.35), (cw * 0.36, y + 0.35), (-cw * 0.36, y + 0.35)],
                              10.6, 11.25, body, bevel=0.15, smooth=False))
    objs.append(prism("cap_line", [(-cw * 0.42, -0.55), (cw * 0.42, -0.55), (cw * 0.42, 0.55), (-cw * 0.42, 0.55)], 10.7, 11.12, line, smooth=False))
    return objs


def display_box(w, h, name, show=None):
    """A readout: a phenolic bezel, a nickel mask, and dark green-black glass the plugin writes behind."""
    M = mats()
    objs = []
    inset = 9.0 if w > 150 else 4.5
    fw, fh = w - 2 * inset, h - 2 * inset
    bez = frame_curve(name + "_bezel", rounded_rect_pts(w, h, 8.0 if w > 150 else 4.0), rounded_rect_pts(fw + 3.0, fh + 3.0, 5.0 if w > 150 else 2.5),
                      2.4 if w > 150 else 1.4, 1.0 if w > 150 else 0.6, M["phen"])
    bez.location.z = 2.0
    objs.append(bez)
    mask = frame_curve(name + "_mask", rounded_rect_pts(fw + 4.0, fh + 4.0, 5.5), rounded_rect_pts(fw - 0.6, fh - 0.6, 4.0), 0.3, 0.2, M["nickel"])
    mask.location.z = 1.2
    objs.append(mask)
    m, nt, bsdf = new_mat(name + "_face")
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    grain = nt.nodes.new("ShaderNodeTexNoise")
    grain.inputs["Scale"].default_value = 3.0
    grain.inputs["Detail"].default_value = 2
    nt.links.new(geo.outputs["Position"], grain.inputs["Vector"])
    base = mix_rgb(nt, math_node(nt, "MULTIPLY", grain.outputs["Fac"], 0.5), (0.008, 0.020, 0.014, 1), (0.013, 0.028, 0.020, 1))
    nt.links.new(base, bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.05
    bsdf.inputs["Coat Weight"].default_value = 0.7
    bsdf.inputs["Coat Roughness"].default_value = 0.02
    if show and os.path.exists(os.path.join(BUILD, show)):
        uvn = nt.nodes.new("ShaderNodeTexCoord")
        st = tex(nt, img(os.path.join(BUILD, show)), uvn.outputs["UV"], "Linear")
        tint = srgb(T["display"])
        nt.links.new(mix_rgb(nt, st.outputs["Color"], (0, 0, 0, 1), tint), bsdf.inputs["Emission Color"])
        bsdf.inputs["Emission Strength"].default_value = 2.2
    face = prism(name + "_glass", rounded_rect_pts(fw + 1.0, fh + 1.0, 4.0), 0.0, 1.0, m, bevel=0.3)
    uv_plane_box(face, fw + 1.0, fh + 1.0)
    objs.append(face)
    return objs


def label_texture(text, w, h, path, dark=True):
    S = 24
    im = Image.new("L", (int(w * S), int(h * S)), 0)
    d = ImageDraw.Draw(im)
    if text in ("◂", "▸"):     # arrows are drawn: the legend font has no triangles
        cx_, cy_, a = im.size[0] / 2, im.size[1] / 2, min(w, h) * S * 0.26
        sgn = -1 if text == "◂" else 1
        d.polygon([(cx_ + sgn * a, cy_), (cx_ - sgn * a * 0.7, cy_ - a), (cx_ - sgn * a * 0.7, cy_ + a)], fill=255)
    else:
        f = ImageFont.truetype(os.path.join(HERE, "fonts", "BarlowSemiCondensed-SemiBold.ttf"), int(min(h * 0.62, 9) * S))
        d.text((im.size[0] / 2, im.size[1] / 2), text, font=f, fill=255, anchor="mm")
    im.save(path)
    return path


def lens_mat(name, text_path, lit, small=False, tint="amber"):
    m, nt, bsdf = new_mat(name)
    uvn = nt.nodes.new("ShaderNodeTexCoord")
    t = nt.nodes.new("ShaderNodeTexImage")
    t.image = img(text_path)
    t.extension = "CLIP"
    nt.links.new(uvn.outputs["UV"], t.inputs["Vector"])
    if small and not lit:
        base = (0.03, 0.03, 0.03, 1)
        ink = srgb((0.9, 0.9, 0.86))
    else:
        # An opal lens: cream when dark, warm amber with the lamp behind it.
        lit_base = srgb((0.80, 0.97, 0.80)) if tint == "green" else srgb((0.99, 0.79, 0.50))
        base = lit_base if lit else srgb((0.88, 0.84, 0.72))
        ink = srgb((0.12, 0.10, 0.08))
    col = mix_rgb(nt, t.outputs["Color"], base, ink)
    nt.links.new(col, bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.32
    bsdf.inputs["Subsurface Weight"].default_value = 0.0 if (small and not lit) else 0.4
    bsdf.inputs["Subsurface Radius"].default_value = (2.0, 1.2, 0.6)
    if lit:
        glow = srgb((0.35, 1.0, 0.55)) if tint == "green" else srgb((1.0, 0.50, 0.12))
        em = mix_rgb(nt, t.outputs["Color"], glow, (0.02, 0.012, 0.004, 1))
        nt.links.new(em, bsdf.inputs["Emission Color"])
        bsdf.inputs["Emission Strength"].default_value = 1.4
    return m


def uv_plane_box(o, w, h):
    """UVs from local xy over a w x h rectangle (top faces)."""
    me = o.data
    uvl = me.uv_layers.new(name="UVMap")
    for poly in me.polygons:
        for li in poly.loop_indices:
            v = me.vertices[me.loops[li].vertex_index].co
            uvl.data[li].uv = (v.x / w + 0.5, v.y / h + 0.5)


def button(w, h, text, lit, small=False, tint="amber"):
    M = mats()
    objs = [prism("bezel", rounded_rect_pts(w, h, 2.5), 0, 2.2, M["chrome"] if not small else M["black_plastic"], bevel=0.8)]
    lw, lh = w - 6, h - 6
    tp = label_texture(text, lw, lh, os.path.join(BUILD, f"lab_{abs(hash((text, w, h)))}.png"))
    lens = prism("lens", rounded_rect_pts(lw, lh, 1.6), 1.5, 4.2, lens_mat(f"lens_{text}_{lit}_{small}_{tint}_{w}", tp, lit, small, tint), bevel=0.7)
    uv_plane_box(lens, lw, lh)
    objs.append(lens)
    return objs


def jewel(color, lit, r=6.0):
    M = mats()
    objs = [prism("lamp_ring", circle_pts(r + 1.6, 64), 0, 2.0, M["chrome"], bevel=0.9)]
    m, nt, bsdf = new_mat(f"jewel_{color}_{lit}")
    c = {"red": (1.0, 0.05, 0.02), "amber": (1.0, 0.28, 0.02)}[color]
    bsdf.inputs["Base Color"].default_value = (*c, 1)
    bsdf.inputs["Transmission Weight"].default_value = 0.6
    bsdf.inputs["Roughness"].default_value = 0.05
    if lit:
        bsdf.inputs["Emission Color"].default_value = (*c, 1)
        bsdf.inputs["Emission Strength"].default_value = 0.9
    else:
        bsdf.inputs["Base Color"].default_value = (c[0] * 0.35, c[1] * 0.3, c[2] * 0.3, 1)
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=2, radius=r)
    j = mesh_obj("jewel", bm, m)
    j.scale = (1, 1, 0.55)
    j.location.z = 1.0
    objs.append(j)
    return objs


def tape(spec, alpha_name, crepe_name, ink_name, lift=1.6):
    pad = 6
    w, h = spec["w"] + 2 * pad, spec["h"] + 2 * pad
    bm = bmesh.new()
    bmesh.ops.create_grid(bm, x_segments=90, y_segments=16, size=0.5)
    for v in bm.verts:
        x, y = v.co.x, v.co.y
        v.co.x, v.co.y = x * w, y * h
        # Right end lifts a little; small undulations elsewhere.
        t = max(0.0, (x + 0.5 - 0.84) / 0.16)
        v.co.z = 0.22 + lift * t * t + 0.05 * math.sin(x * 9.0 + y * 2.0)
    me = bpy.data.meshes.new("tape")
    bm.to_mesh(me)
    bm.free()
    o = link(bpy.data.objects.new("tape", me))
    for p in me.polygons:
        p.use_smooth = True
    uvl = me.uv_layers.new(name="UVMap")
    for poly in me.polygons:
        for li in poly.loop_indices:
            v = me.vertices[me.loops[li].vertex_index].co
            uvl.data[li].uv = (v.x / w + 0.5, v.y / h + 0.5)
    m, nt, bsdf = new_mat("tape_" + alpha_name)
    uvn = nt.nodes.new("ShaderNodeTexCoord")
    ta = tex(nt, img(os.path.join(BUILD, alpha_name)), uvn.outputs["UV"], "Linear")
    tc = tex(nt, img(os.path.join(BUILD, crepe_name)), uvn.outputs["UV"], "Linear")
    paper = srgb((0.86, 0.80, 0.63))
    col = mix_rgb(nt, math_node(nt, "MULTIPLY", tc.outputs["Color"], 0.25), paper, srgb((0.95, 0.90, 0.74)))
    rough = 0.82
    if ink_name:
        ti = tex(nt, img(os.path.join(BUILD, ink_name)), uvn.outputs["UV"], "Linear")
        col = mix_rgb(nt, ti.outputs["Color"], col, srgb((0.07, 0.07, 0.10)))
        rough = mix_rgb(nt, ti.outputs["Color"], (0.82, 0.82, 0.82, 1), (0.5, 0.5, 0.5, 1))
    nt.links.new(col, bsdf.inputs["Base Color"])
    if isinstance(rough, float):
        bsdf.inputs["Roughness"].default_value = rough
    else:
        nt.links.new(rough, bsdf.inputs["Roughness"])
    nt.links.new(ta.outputs["Color"], bsdf.inputs["Alpha"])
    bsdf.inputs["Subsurface Weight"].default_value = 0.15
    bsdf.inputs["Subsurface Radius"].default_value = (1.0, 0.8, 0.5)
    bp = nt.nodes.new("ShaderNodeBump")
    bp.inputs["Strength"].default_value = 0.35
    bp.inputs["Distance"].default_value = 0.15
    nt.links.new(tc.outputs["Color"], bp.inputs["Height"])
    nt.links.new(bp.outputs["Normal"], bsdf.inputs["Normal"])
    o.data.materials.append(m)
    o.location = (spec["x"], -spec["y"], Z_FRAME + 0.05)
    o.rotation_euler.z = math.radians(-spec["angle"])
    return o


def sticker_obj():
    """The inspection sticker: paper on the cassette, its edges just lifting."""
    st = L.STICKER
    w, h = st["w"], st["h"]
    bm = bmesh.new()
    bmesh.ops.create_grid(bm, x_segments=40, y_segments=20, size=0.5)
    for v in bm.verts:
        x, y = v.co.x, v.co.y
        v.co.x, v.co.y = x * w, y * h
        edge = max(abs(x), abs(y)) * 2           # 0 centre .. 1 edge
        lift = 0.25 * max(0.0, (x + 0.5 - 0.88) / 0.12) ** 2 if y > 0.3 else 0.0   # one corner coming up
        v.co.z = 0.12 + 0.05 * edge ** 6 + lift
    me = bpy.data.meshes.new("sticker")
    bm.to_mesh(me)
    bm.free()
    o = link(bpy.data.objects.new("sticker", me))
    for poly in me.polygons:
        poly.use_smooth = True
    uvl = me.uv_layers.new(name="UVMap")
    for poly in me.polygons:
        for li in poly.loop_indices:
            v = me.vertices[me.loops[li].vertex_index].co
            uvl.data[li].uv = (v.x / w + 0.5, v.y / h + 0.5)
    m, nt, bsdf = new_mat("sticker")
    uvn = nt.nodes.new("ShaderNodeTexCoord")
    tc = tex(nt, img(os.path.join(BUILD, "sticker_color.png"), noncolor=False), uvn.outputs["UV"], "Linear")
    ta = tex(nt, img(os.path.join(BUILD, "sticker_alpha.png")), uvn.outputs["UV"], "Linear")
    nt.links.new(tc.outputs["Color"], bsdf.inputs["Base Color"])
    nt.links.new(ta.outputs["Color"], bsdf.inputs["Alpha"])
    bsdf.inputs["Roughness"].default_value = 0.42
    bsdf.inputs["Coat Weight"].default_value = 0.15   # a varnished paper label
    o.data.materials.append(m)
    o.location = (st["x"], -st["y"], Z_CASS)
    o.rotation_euler.z = math.radians(-st["angle"])
    return o


def serial_plate():
    """The maker's plate: satin aluminium, engraved and filled, two rivets."""
    p = L.SERIAL_PLATE
    w, h = p["w"], p["h"]
    M = mats()
    body = box("plate", p["x"] - w / 2, p["y"] - h / 2, p["x"] + w / 2, p["y"] + h / 2, Z_FRAME, Z_FRAME + 0.9,
               simple("plate_edge", srgb((0.74, 0.73, 0.70)), metal=1.0, rough=0.35), bevel=0.5)
    bm = bmesh.new()
    bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=0.5)
    face = mesh_obj("plate_face", bm)
    face.scale = (w - 1.0, h - 1.0, 1)
    face.location = (p["x"], -p["y"], Z_FRAME + 0.92)
    face.data.uv_layers.new(name="UVMap")
    uvl = face.data.uv_layers[0]
    for poly in face.data.polygons:
        for li in poly.loop_indices:
            v = face.data.vertices[face.data.loops[li].vertex_index].co
            uvl.data[li].uv = (v.x + 0.5, v.y + 0.5)
    m, nt, bsdf = new_mat("plate_face")
    uvn = nt.nodes.new("ShaderNodeTexCoord")
    ti = tex(nt, img(os.path.join(BUILD, "plate_ink.png")), uvn.outputs["UV"], "Linear")
    noi = nt.nodes.new("ShaderNodeTexNoise")
    noi.inputs["Scale"].default_value = 140.0
    noi.inputs["Detail"].default_value = 4
    mp = nt.nodes.new("ShaderNodeMapping")
    mp.inputs["Scale"].default_value = (0.04, 1.0, 1.0)   # stretched along x: brushed
    nt.links.new(uvn.outputs["UV"], mp.inputs["Vector"])
    nt.links.new(mp.outputs["Vector"], noi.inputs["Vector"])
    metal_col = mix_rgb(nt, math_node(nt, "MULTIPLY", noi.outputs["Fac"], 0.25), srgb((0.80, 0.79, 0.76)), srgb((0.62, 0.61, 0.58)))
    col = mix_rgb(nt, ti.outputs["Color"], metal_col, srgb((0.05, 0.045, 0.04)))
    nt.links.new(col, bsdf.inputs["Base Color"])
    nt.links.new(math_node(nt, "SUBTRACT", 1.0, ti.outputs["Color"]), bsdf.inputs["Metallic"])
    bsdf.inputs["Roughness"].default_value = 0.34
    face.data.materials.append(m)
    for sx in (-1, 1):
        bm = bmesh.new()
        bmesh.ops.create_uvsphere(bm, u_segments=24, v_segments=12, radius=2.0)
        r = mesh_obj("rivet", bm, M["nickel"])
        r.scale = (1, 1, 0.45)
        r.location = (p["x"] + sx * (w / 2 - 5.0), -p["y"], Z_FRAME + 0.9)
        for poly in r.data.polygons:
            poly.use_smooth = True
    return body


def nameplate():
    """The model plate: black anodised aluminium, the name engraved through to bright metal,
    polished bevel, two domed rivets."""
    p = L.NAMEPLATE
    w, h = p["w"], p["h"]
    M = mats()
    body = box("nameplate", p["x"] - w / 2, p["y"] - h / 2, p["x"] + w / 2, p["y"] + h / 2, Z_FRAME, Z_FRAME + 1.1,
               simple("nameplate_edge", srgb((0.70, 0.69, 0.66)), metal=1.0, rough=0.22), bevel=0.7)
    bm = bmesh.new()
    bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=0.5)
    face = mesh_obj("nameplate_face", bm)
    face.scale = (w - 1.6, h - 1.6, 1)
    face.location = (p["x"], -p["y"], Z_FRAME + 1.12)
    face.data.uv_layers.new(name="UVMap")
    uvl = face.data.uv_layers[0]
    for poly in face.data.polygons:
        for li in poly.loop_indices:
            v = face.data.vertices[face.data.loops[li].vertex_index].co
            uvl.data[li].uv = (v.x + 0.5, v.y + 0.5)
    m, nt, bsdf = new_mat("nameplate_face")
    uvn = nt.nodes.new("ShaderNodeTexCoord")
    ti = tex(nt, img(os.path.join(BUILD, "nameplate_ink.png")), uvn.outputs["UV"], "Linear")
    noi = nt.nodes.new("ShaderNodeTexNoise")
    noi.inputs["Scale"].default_value = 160.0
    noi.inputs["Detail"].default_value = 4
    mp = nt.nodes.new("ShaderNodeMapping")
    mp.inputs["Scale"].default_value = (0.04, 1.0, 1.0)   # brushed along x
    nt.links.new(uvn.outputs["UV"], mp.inputs["Vector"])
    nt.links.new(mp.outputs["Vector"], noi.inputs["Vector"])
    anod = mix_rgb(nt, math_node(nt, "MULTIPLY", noi.outputs["Fac"], 0.35), srgb((0.030, 0.030, 0.032)), srgb((0.060, 0.059, 0.062)))
    bright = mix_rgb(nt, math_node(nt, "MULTIPLY", noi.outputs["Fac"], 0.3), srgb((0.86, 0.85, 0.82)), srgb((0.70, 0.69, 0.66)))
    col = mix_rgb(nt, ti.outputs["Color"], anod, bright)
    nt.links.new(col, bsdf.inputs["Base Color"])
    nt.links.new(math_node(nt, "ADD", math_node(nt, "MULTIPLY", ti.outputs["Color"], 0.55), 0.45), bsdf.inputs["Metallic"])
    nt.links.new(math_node(nt, "SUBTRACT", 0.42, math_node(nt, "MULTIPLY", ti.outputs["Color"], 0.14)), bsdf.inputs["Roughness"])
    face.data.materials.append(m)
    for sx in (-1, 1):
        bm = bmesh.new()
        bmesh.ops.create_uvsphere(bm, u_segments=24, v_segments=12, radius=2.1)
        rv = mesh_obj("nameplate_rivet", bm, M["nickel"])
        rv.scale = (1, 1, 0.45)
        rv.location = (p["x"] + sx * (w / 2 - 6.5), -p["y"], Z_FRAME + 1.1)
        for poly in rv.data.polygons:
            poly.use_smooth = True
    return body


# ---------------------------------------------------------------------------
def frame_curve(name, outer, inner, extrude, bevel, mat):
    """A flat frame (outer outline with an inner hole), extruded and bevelled."""
    cu = bpy.data.curves.new(name, "CURVE")
    cu.dimensions = "2D"
    cu.fill_mode = "BOTH"
    cu.extrude = extrude
    cu.bevel_depth = bevel
    cu.bevel_resolution = 4
    for pts in (outer, inner):
        sp = cu.splines.new("POLY")
        sp.points.add(len(pts) - 1)
        for i, (x, y) in enumerate(pts):
            sp.points[i].co = (x, y, 0, 1)
        sp.use_cyclic_u = True
    o = link(bpy.data.objects.new(name, cu))
    o.data.materials.append(mat)
    return o


def build_panel(regions=None):
    """The whole faceplate as the mockup shows it: every knob, switch and lamp in place."""
    sc = reset()
    lights()
    camera(L.W / 2, L.H / 2, L.W, L.H, PX or (1 if DRAFT else L.SCALE))
    box("floor", -50, -50, L.W + 50, L.H + 50, -4, -2, simple("floor", (0.004, 0.004, 0.004, 1), rough=0.9), bevel=0)

    rail_inks = [(os.path.join(BUILD, "brand_ink.png"), srgb(T["rail_ink"]), 0.5),
                 (os.path.join(BUILD, "rail_ink.png"), srgb(T["rail_ink"]), 0.5),
                 (os.path.join(BUILD, "coffee.png"), srgb((0.42, 0.29, 0.16)), 0.3)]
    if T["rail_finish"] == "powder":
        rail_inks = rail_inks[:2]         # a fresh finish: no coffee ring
        rail = powder("rail", srgb(T["rail"]), rail_inks, wear=False)
    else:
        rail = hammertone("rail", srgb(T["rail"]), 0.35, rail_inks, wear=False)
    ct, cb = L.CASS_Y
    wood = T.get("cheeks") == "walnut"
    xr0, xr1 = (24, L.W - 24) if wood else (0, L.W)
    box("rail_top", xr0, 0, xr1, ct, 0, Z_FRAME, rail, bevel=1.2)
    box("rail_bot", xr0, cb, xr1, L.H, 0, Z_FRAME, rail, bevel=1.2)
    if wood:        # walnut end cheeks, standing a little proud of the panel
        wm = walnut()
        box("cheek_l", 0, 0, 24, L.H, 0, Z_FRAME + 4.0, wm, bevel=2.2)
        box("cheek_r", L.W - 24, 0, L.W, L.H, 0, Z_FRAME + 4.0, wm, bevel=2.2)
    else:
        box("side_l", 0, ct - 1, 24, cb + 1, 0, Z_FRAME - 0.4, rail, bevel=0.8)
        box("side_r", L.W - 24, ct - 1, L.W, cb + 1, 0, Z_FRAME - 0.4, rail, bevel=0.8)

    inks = [(os.path.join(BUILD, "legend_ink.png"), srgb(T["ink"]), 0.32),
            (os.path.join(BUILD, "grease_ink.png"), srgb((0.66, 0.11, 0.07)), 0.55)]
    if T["panel_finish"] == "powder":
        cass = powder("cassette", srgb(T["panel"]), inks)
    else:
        cass = hammertone("cassette", srgb(T["panel"]), 0.22, inks)
    for c in L.CASSETTES:
        box("cass_" + c["id"], c["x0"] + 1.0, ct + 1.0, c["x1"] - 1.0, cb - 1.0, 0, Z_CASS, cass, bevel=1.4)

    sticker_obj()
    serial_plate()

    for i, (x, y) in enumerate(L.SCREWS):
        if wood and (x < 24 or x > L.W - 24):
            continue
        z = Z_FRAME if (y < ct or y > cb or x < 24 or x > L.W - 24) else Z_CASS
        place(screw((i * 47) % 180), x, y, z)
    for i, (x, y) in enumerate(L.DISPLAY_SCREWS):
        place(screw((i * 61 + 20) % 180), x, y, Z_CASS + 4.6)

    D = L.DISPLAY
    place(display_box(D["w"], D["h"], "disp", show=None if STATE else "display_show.png"), D["x"], D["y"], Z_CASS)
    TW = L.TYPE_WIN
    place(display_box(TW["w"], TW["h"], "ftype", show=None if STATE else "type_show.png"), TW["x"], TW["y"], Z_CASS)

    for ctl in L.CONTROLS:
        t = ctl["type"]
        if t == "fader":
            place(fader_plate(ctl["travel"]), ctl["x"], ctl["y"], Z_CASS)
            if not STATE:
                ty = ctl["y"] + ctl["travel"] / 2 - ctl["travel"] * ctl["set"]
                place(fader_cap(), ctl["x"], ty, Z_CASS + 0.6)
            continue
        if STATE and t in ("knob", "toggle"):
            continue
        if t == "knob":
            ang = L.set_angle(ctl) if not ctl.get("encoder") else 360.0 * ctl["set"]
            place(knob(ctl["kind"], L.POINTERS.get(ctl["id"]), ctl["x"]), ctl["x"], ctl["y"], Z_CASS, rot_deg=ang)
        elif t == "key":
            lit = bool(ctl.get("set")) if not STATE else STATE != "bare"
            place(push_key(ctl["w"], ctl["h"], lit, ctl.get("led", True)), ctl["x"], ctl["y"], Z_CASS)
        elif t == "button":
            small = ctl.get("small", False)
            z = Z_FRAME if ctl["y"] > L.CASS_Y[1] else Z_CASS
            lit = bool(ctl.get("set"))
            if STATE:
                lit = STATE != "bare" and not small
            if small and ctl["id"] in ("ab_a", "ab_b") and STATE:
                lit = STATE != "bare"
            place(button(ctl["w"], ctl["h"], ctl["text"], lit=lit, small=small and ctl["id"] not in ("ab_a", "ab_b")),
                  ctl["x"], ctl["y"], z)
        elif t == "lamp":
            z = Z_FRAME if ctl["y"] < L.CASS_Y[0] else Z_CASS
            on = (STATE in ("amber", "green")) if STATE else ctl["id"] == "power"
            place(jewel(ctl["color"], lit=on, r=ctl["r"]), ctl["x"], ctl["y"], z)
        elif t == "toggle":
            place(toggle(L.toggle_lever(ctl, ctl["set"])), ctl["x"], ctl["y"], Z_CASS)

    nameplate()
    ink = "tape_preset_ink.png"
    if STATE:   # the plugin writes the preset's name
        Image.new("L", Image.open(os.path.join(BUILD, ink)).size, 0).save(os.path.join(BUILD, "tape_blank_ink.png"))
        ink = "tape_blank_ink.png"
    tape(L.PRESET_TAPE, "tape_preset_alpha.png", "tape_preset_crepe.png", ink, lift=0.6)

    if CLOSEUPS:
        sc.render.use_border = True
        sc.render.use_crop_to_border = True
        sc.render.image_settings.file_format = "JPEG"
        sc.render.image_settings.quality = 93
        for item in CLOSEUPS.split(";"):
            nm, rect = item.split(":")
            x, y, w, h = [float(v) for v in rect.split(",")]
            sc.render.border_min_x, sc.render.border_max_x = x / L.W, (x + w) / L.W
            sc.render.border_min_y, sc.render.border_max_y = 1.0 - (y + h) / L.H, 1.0 - y / L.H
            sc.render.filepath = os.path.join(BUILD, f"{OUT_NAME or 'close'}_{nm}.jpg")
            bpy.ops.render.render(write_still=True)
            print("wrote", sc.render.filepath)
        return
    if BORDER:
        # Only a small region (for a switch's other position), cropped, as a PNG.
        x, y, w, h = [float(v) for v in BORDER.split(",")]
        sc.render.use_border = True
        sc.render.use_crop_to_border = True
        sc.render.border_min_x, sc.render.border_max_x = x / L.W, (x + w) / L.W
        sc.render.border_min_y, sc.render.border_max_y = 1.0 - (y + h) / L.H, 1.0 - y / L.H
        sc.render.image_settings.file_format = "PNG"
        sc.render.image_settings.color_mode = "RGB"
        sc.render.filepath = os.path.join(BUILD, f"flip_{FLIP}.png")
        bpy.ops.render.render(write_still=True)
        print("wrote", sc.render.filepath)
        return
    if STATE in ("amber", "green"):
        # The lit patches: one crop per group of controls.
        os.makedirs(os.path.join(BUILD, "lit"), exist_ok=True)
        sc.render.use_border = True
        sc.render.use_crop_to_border = True
        sc.render.image_settings.file_format = "PNG"
        sc.render.image_settings.color_mode = "RGB"
        for name, ids in L.LIT_GROUPS.items():
            rects = [L.lit_rect(L.control(i)) for i in ids]
            x0, y0 = min(r[0] for r in rects), min(r[1] for r in rects)
            x1, y1 = max(r[0] + r[2] for r in rects), max(r[1] + r[3] for r in rects)
            sc.render.border_min_x, sc.render.border_max_x = x0 / L.W, x1 / L.W
            sc.render.border_min_y, sc.render.border_max_y = 1.0 - y1 / L.H, 1.0 - y0 / L.H
            sc.render.filepath = os.path.join(BUILD, "lit", f"{STATE}_{name}.png")
            bpy.ops.render.render(write_still=True)
            print("wrote", sc.render.filepath, (x0, y0, x1 - x0, y1 - y0))
        return
    sc.render.image_settings.file_format = "JPEG"
    sc.render.image_settings.quality = 92
    name = OUT_NAME or ("panel_draft.jpg" if DRAFT else ("panel_preview.jpg" if PREVIEW else "panel.jpg"))
    if STATE == "bare":
        name = "bare_" + name
    sc.render.filepath = os.path.join(BUILD, name)
    bpy.ops.render.render(write_still=True)
    print("wrote", sc.render.filepath)


def patch_regions(sc, out, regions):
    import numpy as np
    from PIL import ImageFilter
    base = Image.open(out).convert("RGB")
    sc.render.image_settings.file_format = "PNG"
    sc.render.image_settings.color_mode = "RGBA"
    sc.render.use_border = True
    sc.render.use_crop_to_border = False
    tmp = os.path.join(BUILD, "patch.png")
    for (x, y, w, h) in regions:
        sc.render.border_min_x = max(0.0, x / L.W)
        sc.render.border_max_x = min(1.0, (x + w) / L.W)
        sc.render.border_min_y = max(0.0, 1.0 - (y + h) / L.H)
        sc.render.border_max_y = min(1.0, 1.0 - y / L.H)
        sc.render.filepath = tmp
        bpy.ops.render.render(write_still=True)
        patch = Image.open(tmp).convert("RGBA")
        # Feathered edges, so the patch blends into the render around it.
        S = L.SCALE
        box = (int(x * S), int(y * S), int((x + w) * S), int((y + h) * S))
        m = Image.new("L", base.size, 0)
        ImageDraw.Draw(m).rectangle((box[0] + 10, box[1] + 10, box[2] - 10, box[3] - 10), fill=255)
        m = m.filter(ImageFilter.GaussianBlur(4))
        a = np.asarray(patch)[..., 3:4] > 0
        mm = (np.asarray(m)[..., None] / 255.0) * a
        blended = np.asarray(base) * (1 - mm) + np.asarray(patch.convert("RGB")) * mm
        base = Image.fromarray(np.clip(blended + 0.5, 0, 255).astype("uint8"), "RGB")
        print("patched", (x, y, w, h))
    base.save(out, quality=90)


# ---------------------------------------------------------------------------
def sprite_scene(box_w, box_h, catcher=False):
    sc = reset()
    lights()
    camera(0, 0, box_w, box_h, L.SCALE)
    sc.render.film_transparent = True
    sc.cycles.film_transparent_glass = True
    sc.render.image_settings.file_format = "PNG"
    sc.render.image_settings.color_mode = "RGBA"
    sc.render.image_settings.compression = 90
    if catcher:
        bm = bmesh.new()
        bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=box_w * 2)
        c = mesh_obj("catcher", bm, simple("catch", (0.5, 0.5, 0.5, 1)))
        c.is_shadow_catcher = True
    return sc


def render_to(path):
    sc = bpy.context.scene
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)


def knob_box(kind):
    """Sprite size in points (selectors have room for their own shadow)."""
    return 2 * L.KNOBS[kind] + (20 if kind == "sel" else 12)


def knob_strip(kind):
    box_pt = knob_box(kind)
    sc = sprite_scene(box_pt, box_pt, catcher=(kind == "sel"))
    sc.cycles.samples = 16 if DRAFT else 96
    parts = knob(kind)
    root = parent_to(parts, "knob")
    frames = []
    tmp = os.path.join(BUILD, "frames_" + kind)
    os.makedirs(tmp, exist_ok=True)
    n = 9 if DRAFT else L.STRIP_FRAMES
    angles = [-L.SWEEP + 2 * L.SWEEP * i / (n - 1) for i in range(n)]
    if kind == "sel":
        angles = list(L.SEL_ANGLES)   # a selector only ever rests at these
    if kind == "small":
        angles = [3.0 * i for i in range(120)]   # the encoders turn all the way round
    for i, ang in enumerate(angles):
        root.rotation_euler.z = math.radians(-ang)
        p = os.path.join(tmp, f"{i:03d}.png")
        render_to(p)
        frames.append(p)
    ims = [Image.open(p) for p in frames]
    fw, fh = ims[0].size
    strip = Image.new("RGBA", (fw, fh * len(ims)))
    for i, im in enumerate(ims):
        strip.paste(im, (0, i * fh))
    os.makedirs(os.path.join(BUILD, "strips"), exist_ok=True)
    out = os.path.join(BUILD, "strips", f"knob_{kind}.png")
    strip.save(out, optimize=True)
    print("wrote", out, strip.size)


def sprites():
    for kind in ("sel", "small"):
        knob_strip(kind)
    small_sprites()


def small_sprites():
    for pos in ("l", "c", "r"):
        # Rendered on the catcher (z = 0), so the shadow falls where the cassette face would be.
        sprite_scene(L.TOGGLE_BOX, L.TOGGLE_BOX, catcher=True)
        place(toggle(pos), 0, 0, 0)
        render_to(os.path.join(ASSETS, f"toggle_{pos}.png"))
    sprite_scene(*L.FADER_BOX, catcher=True)
    place(fader_cap(), 0, 0, 0.6)
    render_to(os.path.join(ASSETS, "fader_cap.png"))


if __name__ == "__main__":
    what = sys.argv[1] if len(sys.argv) > 1 else "panel"
    if what == "panel":
        build_panel()
    elif what in ("glass", "parts"):
        ASSETS = os.path.join(BUILD, "sprites")
        os.makedirs(ASSETS, exist_ok=True)
        small_sprites()
    elif what.startswith("knob_"):
        knob_strip(what[5:])
