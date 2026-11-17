"""Tandem: the faceplate layout, in points at 100% (the art is rendered at 2x).

Same conventions as Sam's Desk and Kontur: one source for the Blender render, the
printed legends and the plugin's Layout.h. Angles: 0 = straight up, clockwise
positive; continuous knobs sweep -150..+150 degrees.

`set` on a control is where the mockup render puts it (a 0..1 position for knobs and
faders, a step index for selectors and toggles, True/False for lit buttons).

Toggles here are horizontal, as on the hardware it follows: the lever goes left,
centre (three-way ones) or right, and the positions are printed above it.
"""
import math

W, H = 1400, 740
SCALE = 2
RAIL_TOP = (0, 64)
CASS_Y = (64, 676)
RAIL_BOT = (676, 740)
RAIL_MID = (RAIL_BOT[0] + RAIL_BOT[1]) / 2

SWEEP = 150.0
STRIP_FRAMES = 121

KNOBS = {"large": 30, "med": 24, "small": 18, "sel": 23}

TYPE = {
    "title": 12.5,
    "name": 9.8,
    "num": 8.4,
    "sub": 7.8,
    "toggle": 7.5,
    "section": 9.0,
}
LABEL_R = 15.5
NAME_GAP = 23.0
SUB_GAP = 10.0

CASSETTES = [
    {"id": "osc", "title": "OSCILLATORS", "x0": 24, "x1": 452},
    {"id": "vcf", "title": "FILTER", "x0": 452, "x1": 762},
    {"id": "lfo", "title": "LFO 1", "x0": 762, "x1": 890},
    {"id": "env", "title": "ENVELOPES", "x0": 890, "x1": 1152},
    {"id": "ctl", "title": "CONTROL", "x0": 1152, "x1": 1376},
]

POINTERS = {"f_cutoff": "brick", "f_res": "ochre", "m_detune": "slate", "volume": "brick"}
POINTER_COLOURS = {
    "brick": (0.84, 0.40, 0.33), "slate": (0.56, 0.68, 0.84), "sage": (0.62, 0.78, 0.58),
    "ochre": (0.93, 0.77, 0.44), "terracotta": (0.93, 0.58, 0.40),
}

# Toggles: the lever's sprite box, how far apart the printed positions sit, and where.
TOGGLE_BOX = 44
TOG_SP = 21.5
TOG_LABEL_DY = -19.0
TOG_NAME_DY = 21.0

# Faders: a nickel cover plate with a slot; the cap is a separate sprite the window moves.
FADER_CAP = (16.0, 24.0)          # w, h of the cap's top
FADER_BOX = (30.0, 40.0)          # the cap sprite's box (room for its shadow)
FADER_PLATE_W = 12.0


def cx(i):
    c = CASSETTES[i]
    return (c["x0"] + c["x1"]) / 2


def lin(lo, hi, v):
    return (v - lo) / (hi - lo)


def ticks_lin(lo, hi, step):
    n = int(round((hi - lo) / step))
    return [i / n for i in range(n + 1)]


def labels(pairs):
    return [{"n": n, "t": t} for n, t in pairs]


def steps(n, spacing=30.0):
    return [(-(n - 1) / 2 + i) * spacing for i in range(n)]


CONTROLS = []
SECTIONS = []          # small printed headings with rules: (text, x0, x1, y)


def knob(pid, kind, x, y, name, lab, ticks=None, sub=None, set_=0.5):
    CONTROLS.append({"id": pid, "type": "knob", "kind": kind, "x": x, "y": y, "name": name, "sub": sub,
                     "labels": lab, "ticks": ticks if ticks is not None else [l["n"] for l in lab],
                     "steps": None, "set": set_})


def selector(pid, x, y, name, texts, sub=None, spacing=30.0, set_=0):
    ang = steps(len(texts), spacing)
    CONTROLS.append({"id": pid, "type": "knob", "kind": "sel", "x": x, "y": y, "name": name, "sub": sub,
                     "labels": [{"a": a, "t": t} for a, t in zip(ang, texts)], "ticks": [], "steps": ang, "set": set_})


def toggle(pid, x, y, positions, name=None, set_=0, under=None):
    CONTROLS.append({"id": pid, "type": "toggle", "x": x, "y": y, "pos": positions, "name": name, "set": set_, "under": under})


def fader(pid, x, y, travel, name, scale=False, set_=0.5):
    CONTROLS.append({"id": pid, "type": "fader", "x": x, "y": y, "travel": travel, "name": name, "scale": scale, "set": set_})


def button(pid, x, y, text="IN", w=36, h=20, set_=False, **kw):
    CONTROLS.append({"id": pid, "type": "button", "x": x, "y": y, "w": w, "h": h, "text": text, "set": set_, **kw})


ZERO_TEN = labels([(0, "0"), (1, "10")])
T10 = ticks_lin(0, 10, 1)
BIPOLAR = labels([(0, "–"), (0.5, "0"), (1, "+")])

# ---- OSCILLATORS
OSC_WAVES = ["icon:sine", "icon:saw", "icon:square", "icon:pulse", "icon:tri", "icon:noise", "1", "2", "3", "4", "5", "6"]
OSC2_WAVES = ["icon:sine", "icon:saw", "icon:square", "icon:pulse", "icon:tri", "icon:noise", "1", "2", "3", "icon:square", "icon:saw", "icon:sine"]
SEL12 = 26.0
X1, X2, XM = 118, 358, 238
SECTIONS += [("OSC 1", X1 - 70, X1 + 70, 120), ("OSC 2", X2 - 70, X2 + 70, 120)]
selector("o1_wave", X1, 196, "WAVEFORM", OSC_WAVES, spacing=SEL12, set_=1)
selector("o2_wave", X2, 196, "WAVEFORM", OSC2_WAVES, spacing=SEL12, set_=2)
DRONE_ARC = {"control": "o2_wave", "from": 9, "to": 11, "text": "KB OFF"}     # a printed bracket over the drone positions
TUNE_LAB = labels([(0, "–7"), (0.5, "0"), (1, "+7")])
FINE_LAB = labels([(0, "–50"), (0.5, "0"), (1, "+50")])
knob("o1_tune", "med", X1 - 34, 302, "TUNE", TUNE_LAB, ticks=ticks_lin(-7, 7, 1), set_=0.5)
knob("o1_fine", "med", X1 + 34, 302, "FINE", FINE_LAB, ticks=ticks_lin(-50, 50, 10), sub="CENTS", set_=0.5)
knob("o2_tune", "med", X2 - 34, 302, "TUNE", TUNE_LAB, ticks=ticks_lin(-7, 7, 1), set_=0.5 + 7 / 14)
knob("o2_fine", "med", X2 + 34, 302, "FINE", FINE_LAB, ticks=ticks_lin(-50, 50, 10), sub="CENTS", set_=0.56)
toggle("o_sync", XM, 196, ["OFF", "2-1", "1-2"], name="SYNC", set_=0)
selector("octave", XM, 302, "OCTAVE", ["–2", "–1", "0", "+1", "+2"], spacing=30.0, set_=2)

SECTIONS.append(("MOD", 40, 436, 384))
MC = [100, 238, 376]
toggle("m_osc", MC[0], 432, ["1", "BOTH", "2"], set_=1)
toggle("m_wsrc", MC[1], 432, ["MAN", "ENV1", "LFO1"], set_=2)
toggle("m_psrc", MC[2], 432, ["ENV1", "LFO1", "CROSS"], set_=1)
knob("m_detune", "med", MC[0], 512, "DETUNE", ZERO_TEN, ticks=T10, set_=0.35)
knob("m_wave", "med", MC[1], 512, "WAVE / PWM", ZERO_TEN, ticks=T10, set_=0.4)
knob("m_pitch", "med", MC[2], 512, "PITCH MOD", ZERO_TEN, ticks=T10, set_=0.1)

# ---- FILTER
c = cx(1)
knob("f_cutoff", "large", c - 72, 190, "CUTOFF", ZERO_TEN, ticks=T10, set_=0.62)
knob("f_res", "large", c + 72, 190, "RESONANCE", ZERO_TEN, ticks=T10, set_=0.45)
toggle("f_kt", c - 84, 300, ["OFF", "HALF", "FULL"], name="KEYTRACK", set_=1)
toggle("f_restype", c - 84, 392, ["1", "2"], name="RES TYPE", set_=0)
knob("f_mod", "med", c, 300, "MODULATION", ZERO_TEN, ticks=T10, set_=0.45)
toggle("f_msrc", c, 392, ["ENV1", "LFO1", "OSC2"], set_=0)
knob("f_delta", "med", c + 84, 300, "Δ-CUTOFF", BIPOLAR, ticks=ticks_lin(-5, 5, 1), set_=0.5)
toggle("f_route", c + 84, 392, ["ST", "PAR", "SER"], set_=0)
# The filter type: a small window the plugin writes in, stepped by two buttons.
TYPE_WIN = {"id": "f_type", "x": c, "y": 454, "w": 112, "h": 26}
button("f_prev", c - 76, 454, "◂", w=22, h=22, small=True)
button("f_next", c + 76, 454, "▸", w=22, h=22, small=True)
TYPE_LABEL_Y = 480
SECTIONS.append(("MIXER", 468, 746, 508))
knob("x_osc1", "med", c - 80, 560, "OSC 1", ZERO_TEN, ticks=T10, set_=0.55)
knob("x_osc2", "med", c - 10, 560, "OSC 2", ZERO_TEN, ticks=T10, set_=0.4)
knob("x_ext", "med", c + 60, 560, "EXT", ZERO_TEN, ticks=T10, set_=0.0)
CONTROLS.append({"id": "odrive", "type": "lamp", "x": c + 122, "y": 560, "r": 5.0, "color": "red"})
ODRIVE_LABEL = (c + 122, 543)

# ---- LFO 1
c = cx(2)
toggle("l_mode", c, 152, ["FREE", "ONCE", "RST"], set_=0, under=["", "OSC1", "OSC2"])
FADER_Y, FADER_TRAVEL = 330, 150
fader("l_rate", c - 26, FADER_Y, FADER_TRAVEL, "RATE", scale=True, set_=0.55)
fader("l_delay", c + 26, FADER_Y, FADER_TRAVEL, "DELAY", set_=0.0)
LFO_WAVES = ["icon:sine", "icon:saw", "icon:sh", "icon:square", "icon:ramp", "icon:noise", "icon:tri", "icon:nsine", "1", "2", "HF", "TRK"]
selector("l_wave", c, 522, "WAVE", LFO_WAVES, spacing=SEL12, set_=6)

# ---- ENVELOPES
c = cx(3)
E1 = [914, 938, 962, 986, 1010]
E2 = [1052, 1076, 1100, 1124]
SECTIONS += [("ENV 1", E1[0] - 10, E1[-1] + 10, 120), ("ENV 2", E2[0] - 10, E2[-1] + 10, 120)]
toggle("e1_mode", E1[2], 166, ["INV", "LOOP", "NORM"], set_=2)
toggle("vca_src", (E2[1] + E2[2]) / 2, 166, ["ENV2", "ENV3", "ON"], name=None, set_=0)
for i, (pid, nm) in enumerate(zip(["e1_h", "e1_a", "e1_d", "e1_s", "e1_r"], ["H", "A", "D", "S", "R"])):
    fader(pid, E1[i], FADER_Y, FADER_TRAVEL, nm, scale=(i == 0), set_=[0.0, 0.08, 0.5, 0.25, 0.35][i])
for i, (pid, nm) in enumerate(zip(["e2_a", "e2_d", "e2_s", "e2_r"], ["A", "D", "S", "R"])):
    fader(pid, E2[i], FADER_Y, FADER_TRAVEL, nm, scale=(i == 0), set_=[0.02, 0.4, 0.8, 0.3][i])
knob("e1_vel", "med", E1[2], 530, "VELOCITY", BIPOLAR, ticks=ticks_lin(-5, 5, 1), sub="TO ENV 1", set_=0.6)
knob("e2_vel", "med", (E2[1] + E2[2]) / 2, 530, "VELOCITY", BIPOLAR, ticks=ticks_lin(-5, 5, 1), sub="TO VCA", set_=0.62)

# ---- CONTROL: the display, its four encoders, the menu buttons, then the output.
c = cx(4)
DISPLAY = {"id": "disp", "x": c, "y": 152, "w": 204, "h": 84}
ENC_Y = 236
ENC_X = [c - 75, c - 25, c + 25, c + 75]
for i, k in enumerate("abcd"):
    CONTROLS.append({"id": f"enc_{k}", "type": "knob", "kind": "small", "x": ENC_X[i], "y": ENC_Y, "name": k.upper(), "sub": None,
                     "labels": [], "ticks": [], "steps": None, "set": [0.1, 0.4, 0.7, 0.25][i], "encoder": True})
button("menu_voice", c - 62, 292, "VOICE", w=50, h=20)
button("menu_mod", c, 292, "MOD", w=50, h=20, set_=True)
button("menu_fx", c + 62, 292, "FX", w=50, h=20)
SECTIONS.append(("OUTPUT", 1166, 1362, 330))
knob("level", "med", c - 54, 382, "LEVEL", ZERO_TEN, ticks=T10, set_=0.75)
knob("pan", "med", c + 54, 382, "PAN", labels([(0, "L"), (0.5, "·"), (1, "R")]), ticks=ticks_lin(-5, 5, 1), set_=0.5)
knob("fx_send", "med", c - 54, 482, "FX SEND", ZERO_TEN, ticks=T10, set_=0.3)
knob("glide", "med", c + 54, 482, "PORTAMENTO", ZERO_TEN, ticks=T10, set_=0.0)
knob("volume", "large", c, 588, "VOLUME", ZERO_TEN, ticks=T10, set_=0.8)

# ---- rails
BRAND = {"text": "RUMPUS ROOM RECORDS", "x": 44, "y": 30, "size": 21}
TYPE_PLATE = {"text": "TYPE DS 8", "x": 44, "y": 51, "size": 8.5}
NAMEPLATE = {"x": 900, "y": 32, "w": 300, "h": 46, "name": "TANDEM", "sub": "BI-TIMBRAL SYNTHESIZER  ·  8 VOICES"}
CONTROLS.append({"id": "power", "type": "lamp", "x": 1352, "y": 32, "r": 7, "color": "amber"})
PRESET_TAPE = {"x": 560, "y": RAIL_MID, "w": 460, "h": 34, "angle": 0.4}
button("preset_prev", 302, RAIL_MID, "◂", w=22, h=22, small=True)
button("preset_next", 818, RAIL_MID, "▸", w=22, h=22, small=True)
button("ab_a", 900, RAIL_MID - 4, "A", w=26, h=22, small=True, set_=True)
button("ab_b", 934, RAIL_MID - 4, "B", w=26, h=22, small=True)
button("menu", 996, RAIL_MID, "SET", w=40, h=22, small=True)
RAIL_LEGENDS = [
    {"text": "PRESET", "x": 246, "y": RAIL_MID, "size": 9.0},
    {"text": "COMPARE", "x": 917, "y": RAIL_MID + 16, "size": 7.4},
    {"text": "POWER", "x": 1318, "y": 32, "size": 7.4},
]
PRESET_TEXT = "init"

# ---- character
SERIAL_PLATE = {"x": 1222, "y": RAIL_MID, "w": 168, "h": 32,
                "lines": ["RUMPUS ROOM RECORDS", "TANDEM  TYPE DS 8  No. 0212"]}
STICKER = {"x": 404, "y": 628, "w": 84, "h": 46, "angle": -2.0,
           "printed": ["INSPECTED", "No. 0212"], "hand": "14.3.68  RK"}
COFFEE = {"x": 1196, "y": 30, "r": 21}
GREASE = {"control": "f_res", "label": "10", "note": "sings"}

SCREWS = [(12, 32), (1388, 32), (12, RAIL_MID), (1388, RAIL_MID)]
for cs in CASSETTES:
    SCREWS += [(cs["x0"] + 9, 72), (cs["x1"] - 9, 72), (cs["x0"] + 9, 668), (cs["x1"] - 9, 668)]
DISPLAY_SCREWS = [(DISPLAY["x"] + sx * (DISPLAY["w"] / 2 - 7), DISPLAY["y"] + sy * (DISPLAY["h"] / 2 - 7)) for sx in (-1, 1) for sy in (-1, 1)]

SEL_ANGLES = sorted({round(a, 3) for c in CONTROLS if c["type"] == "knob" and c["kind"] == "sel" for a in c["steps"]})
CAP_R = 0.74 * 0.66            # the cap's radius, as a fraction of the knob's (see render.knob)


def lit_rect(c):
    """The patch of panel that changes when a button or lamp lights, in whole points."""
    if c["type"] == "lamp":
        m = c["r"] + 9
        return (int(c["x"] - m), int(c["y"] - m), int(2 * m) + 1, int(2 * m) + 1)
    mx, my = 7, 7
    x0, y0 = int(c["x"] - c["w"] / 2 - mx), int(c["y"] - c["h"] / 2 - my)
    return (x0, y0, int(c["x"] + c["w"] / 2 + mx) + 1 - x0, int(c["y"] + c["h"] / 2 + my) + 1 - y0)


# Groups of lit controls rendered together (art/render.py lit): name -> control ids.
LIT_GROUPS = {
    "menu": ["menu_voice", "menu_mod", "menu_fx"],
    "odrive": ["odrive"],
    "power": ["power"],
    "ab": ["ab_a", "ab_b"],
}


def control(pid):
    return next(c for c in CONTROLS if c["id"] == pid)


def set_angle(ctl):
    """The angle the mockup puts a knob at."""
    if ctl["kind"] == "sel":
        return ctl["steps"][ctl["set"]]
    return -SWEEP + 2 * SWEEP * ctl["set"]


def toggle_label_x(ctl, i):
    n = len(ctl["pos"])
    sp = TOG_SP if n == 3 else 15.0
    return ctl["x"] + (i - (n - 1) / 2) * sp


def toggle_lever(ctl, idx):
    """Which sprite: 'l', 'c' or 'r'."""
    n = len(ctl["pos"])
    if n == 2:
        return "l" if idx == 0 else "r"
    return "lcr"[idx]
