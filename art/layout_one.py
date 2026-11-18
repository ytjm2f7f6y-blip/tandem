"""Tandem on one panel: the colour-caps finish (direction C), one powder-coated sheet with printed
section boxes. Points at 100%. Angles: 0 = straight up, clockwise positive; continuous knobs
sweep -150..+150 degrees.

Top row: the sound of the layer being edited (the LAYER keys choose which).
Bottom row: playing both layers, and the programmer that reaches everything else.

Switched settings are lamp keys and multi-way choices are encoders ringed with lamps, so a
patch recall can show every setting. Secondary functions are printed in a second ink
(red-orange) and reached by holding SHIFT. Set-and-forget settings live in a drawer under the
front edge (TD_DRAWER=1 renders it pulled out).
"""
import os

DRAWER_OPEN = os.environ.get("TD_DRAWER") == "1"
W = 1640
H_PANEL = 800
DRAWER_H = 150
H = H_PANEL + (DRAWER_H if DRAWER_OPEN else 0)
SCALE = 2
SWEEP = 150.0
STRIP_FRAMES = 81
CHEEK = 22                      # cream end cheeks
PANEL_X = (CHEEK, W - CHEEK)

KNOBS = {"large": 30, "med": 24, "small": 18, "ring": 19, "trim": 13, "sel": 23}
TYPE = {"head": 10.0, "name": 9.4, "num": 8.2, "sub": 7.9, "lamp": 7.2, "sec": 8.4}
LABEL_R = 15.5
NAME_GAP = 23.0
RING_LED_R = 27.0               # lamps round an encoder
RING_LABEL_R = 38.0
RING_ICON_R = 41.0
LAMP_SP = 21.0                  # lamps over a key, apart
FADER_CAP = (16.0, 24.0)
FADER_BOX = (40.0, 52.0)
FADER_PLATE_W = 12.0
POINTERS = {}
POINTER_COLOURS = {}

HEAD_H = 20                     # the printed header bar on each box
ROW1 = (96, 470)
ROW2 = (484, 784)
# Boxes: id, title, x0, x1, row. The cap colour follows the box (theme "caps").
BOXES = [
    ("osc", "OSCILLATORS", 36, 556, ROW1), ("mix", "MIXER", 568, 680, ROW1), ("vcf", "FILTER", 692, 1004, ROW1),
    ("lfo", "LFO", 1016, 1210, ROW1), ("env", "ENVELOPES", 1222, 1604, ROW1),
    ("perf", "PERFORMANCE", 36, 286, ROW2), ("arp", "ARPEGGIATOR", 298, 498, ROW2), ("fx", "EFFECTS", 510, 732, ROW2),
    ("prog", "PROGRAMMER", 744, 1274, ROW2), ("lmix", "LAYER MIX", 1286, 1482, ROW2), ("out", "OUTPUT", 1494, 1604, ROW2),
]
CASSETTES = [{"id": b[0], "x0": b[2], "x1": b[3]} for b in BOXES]       # for render.section_of (patched there)

CONTROLS = []
SUBHEADS = []           # (text, x0, x1, y): a small heading with rules
PRINT = []              # loose printed words: (text, x, y, size, ink, anchor)  ink "navy"|"shift"


def box_of(x, y):
    for b in BOXES:
        if b[2] <= x <= b[3] and b[4][0] <= y <= b[4][1]:
            return b[0]
    return "drawer" if y > H_PANEL else "out"


def labels(pairs):
    return [{"n": n, "t": t} for n, t in pairs]


def ticks(n):
    return [i / n for i in range(n + 1)]


ZERO_TEN = labels([(0, "0"), (1, "10")])
T10 = ticks(10)
BIPOLAR = labels([(0, "–"), (0.5, "0"), (1, "+")])


def knob(pid, kind, x, y, name, lab=ZERO_TEN, tk=T10, sub=None, set_=0.5, shift=None):
    CONTROLS.append({"id": pid, "type": "knob", "kind": kind, "x": x, "y": y, "name": name, "sub": sub, "labels": lab,
                     "ticks": tk, "set": set_, "shift": shift})


def ring(pid, x, y, name, texts, spacing, set_=0, shift=None, name_dy=None):
    """An endless encoder ringed with lamps, one per choice; the choice's word or glyph outside."""
    n = len(texts)
    ang = [(-(n - 1) / 2 + i) * spacing for i in range(n)]
    CONTROLS.append({"id": pid, "type": "knob", "kind": "ring", "x": x, "y": y, "name": name, "steps": ang,
                     "texts": texts, "set": set_, "shift": shift, "name_dy": name_dy})


def key(pid, x, y, label, w=30, h=19, led=True, set_=False, lamps=None, lit=(), shift=None, cap=None, label_side=None):
    """A push key. led: one lamp above. lamps: a row of named lamps above instead (lit: their indices)."""
    CONTROLS.append({"id": pid, "type": "key", "x": x, "y": y, "w": w, "h": h, "label": label, "led": led and not lamps,
                     "set": set_, "lamps": lamps, "lit": list(lit), "shift": shift, "cap": cap, "label_side": label_side})


def fader(pid, x, y, travel, name, scale=False, set_=0.5):
    CONTROLS.append({"id": pid, "type": "fader", "x": x, "y": y, "travel": travel, "name": name, "scale": scale, "set": set_})


def lamp(pid, x, y, r=4.2, color="red", on=False, style="jewel"):
    """A lamp on its own: a jewel (r: its lens), or a small LED like the keys' (style "led")."""
    CONTROLS.append({"id": pid, "type": "lamp", "x": x, "y": y, "r": r, "color": color, "on": on, "style": style})


# ============================================================ TOP ROW: the edited layer
# ---- OSCILLATORS
OSC_WAVES = ["icon:sine", "icon:saw", "icon:square", "icon:pulse", "icon:tri", "icon:noise", "1", "2", "3", "4", "5", "6"]
OSC2_WAVES = ["icon:sine", "icon:saw", "icon:square", "icon:pulse", "icon:tri", "icon:noise", "1", "2", "3", "D1", "D2", "D3"]
SUBHEADS += [("OSC 1", 50, 206, 132), ("OSC 2", 386, 542, 132)]
ring("o1_wave", 128, 196, "WAVEFORM", OSC_WAVES, 26.0, set_=1)
ring("o2_wave", 464, 196, "WAVEFORM", OSC2_WAVES, 26.0, set_=2)
key("o_sync", 296, 196, "SYNC", lamps=["2›1", "1›2"], lit=())
TUNE = labels([(0, "–7"), (0.5, "0"), (1, "+7")])
FINE = labels([(0, "–50"), (0.5, "0"), (1, "+50")])
knob("o1_tune", "med", 86, 288, "TUNE", TUNE, ticks(14), set_=0.5)
knob("o1_fine", "med", 170, 288, "FINE", FINE, ticks(10), sub="CENTS", set_=0.5)
knob("o2_tune", "med", 422, 288, "TUNE", TUNE, ticks(14), set_=1.0)
knob("o2_fine", "med", 506, 288, "FINE", FINE, ticks(10), sub="CENTS", set_=0.56)
ring("octave", 296, 292, "OCTAVE", ["–2", "–1", "0", "+1", "+2"], 30.0, set_=2)
SUBHEADS.append(("MODULATION", 50, 542, 362))
knob("m_detune", "med", 98, 404, "DETUNE", set_=0.35)
key("m_osc", 168, 412, "TO OSC", lamps=["1", "2"], lit=(0, 1))
knob("m_wave", "med", 270, 404, "WAVE / PWM", set_=0.4)
key("m_wsrc", 340, 412, "SOURCE", lamps=["ENV1", "LFO1"], lit=(1,))
knob("m_pitch", "med", 442, 404, "PITCH MOD", set_=0.1)
key("m_psrc", 512, 412, "SOURCE", lamps=["ENV1", "LFO1", "X-MOD"], lit=(1,))

# ---- MIXER (into the filter)
knob("x_osc1", "med", 624, 182, "OSC 1", set_=0.55)
knob("x_osc2", "med", 624, 280, "OSC 2", set_=0.4)
knob("x_ext", "med", 624, 378, "EXT IN", set_=0.0)
lamp("odrive", 598, 452, r=4.2, color="red", on=False)
PRINT.append(("O.DRIVE", 610, 452, 7.4, "navy", "lm"))

# ---- FILTER
knob("f_cutoff", "large", 776, 182, "CUTOFF", set_=0.62)
knob("f_res", "large", 920, 182, "RESONANCE", set_=0.45)
TYPE_WIN = {"id": "f_type", "x": 848, "y": 266, "w": 112, "h": 26}
key("f_prev", 776, 266, "", w=22, h=19, led=False)
key("f_next", 920, 266, "", w=22, h=19, led=False)
PRINT += [("◂", 776, 266, 9.0, "glyph", "mm"), ("▸", 920, 266, 9.0, "glyph", "mm")]
TYPE_LABEL_Y = 293
knob("f_mod", "med", 770, 350, "MODULATION", set_=0.45)
key("f_msrc", 848, 358, "SOURCE", lamps=["ENV1", "LFO1", "OSC2"], lit=(0,))
knob("f_delta", "med", 926, 350, "Δ-CUTOFF", BIPOLAR, ticks(10), set_=0.5)
key("f_kt", 770, 438, "KEYTRACK", lamps=["HALF", "FULL"], lit=(0,))
key("f_restype", 848, 438, "RES TYPE", lamps=["1", "2"], lit=(0,))
key("f_route", 926, 438, "ROUTING", lamps=["ST", "PAR", "SER"], lit=(0,))

# ---- LFO 1 and LFO 2
LFO_WAVES = ["icon:sine", "icon:saw", "icon:sh", "icon:square", "icon:ramp", "icon:noise", "icon:tri", "icon:nsine", "1", "2", "HF", "TRK"]
SUBHEADS += [("LFO 1", 1026, 1200, 132), ("LFO 2", 1026, 1200, 362)]
ring("l_wave", 1076, 200, "WAVE", LFO_WAVES, 26.0, set_=6)
FADER_Y, FADER_TRAVEL = 244, 136
fader("l_rate", 1152, FADER_Y, FADER_TRAVEL, "RATE", scale=True, set_=0.55)
fader("l_delay", 1186, FADER_Y, FADER_TRAVEL, "DELAY", set_=0.0)
knob("lfo2_rate", "med", 1076, 404, "RATE", set_=0.45)
key("lfo2_trig", 1160, 412, "TRIGGER", lamps=["WHL", "AT", "ON"], lit=(0,))

# ---- ENVELOPES
SUBHEADS += [("ENV 1", 1232, 1402, 132), ("ENV 2", 1424, 1594, 132)]
E1 = [1268, 1298, 1328, 1358, 1388]
E2 = [1464, 1494, 1524, 1554]
for i, (pid, nm) in enumerate(zip(["e1_h", "e1_a", "e1_d", "e1_s", "e1_r"], "HADSR")):
    fader(pid, E1[i], FADER_Y, FADER_TRAVEL, nm, scale=(i == 0), set_=[0.0, 0.08, 0.5, 0.25, 0.35][i])
for i, (pid, nm) in enumerate(zip(["e2_a", "e2_d", "e2_s", "e2_r"], "ADSR")):
    fader(pid, E2[i], FADER_Y, FADER_TRAVEL, nm, scale=(i == 0), set_=[0.02, 0.4, 0.8, 0.3][i])
knob("e1_vel", "med", 1278, 404, "VELOCITY", BIPOLAR, ticks(10), sub="TO ENV 1", set_=0.6)
key("e1_mode", 1368, 412, "MODE", lamps=["INV", "LOOP"], lit=())
knob("e2_vel", "med", 1474, 404, "VELOCITY", BIPOLAR, ticks(10), sub="TO VCA", set_=0.62)
key("vca_src", 1562, 412, "VCA", lamps=["ENV2", "ENV3", "GATE"], lit=(0,), shift="ENV 3")

# ============================================================ BOTTOM ROW: playing and programming
# ---- PERFORMANCE
SUBHEADS.append(("EDIT", 52, 176, 524))
key("layer_1", 82, 556, "LAYER 1", w=44, h=26, set_=True, shift="SWAP")
key("layer_2", 146, 556, "LAYER 2", w=44, h=26, shift="COPY")
knob("glide", "med", 238, 552, "GLIDE", set_=0.15)
# Each layer's voice mode, in a printed grid: a key per layer steps it, a lamp under each mode.
SUBHEADS.append(("VOICE MODE", 52, 208, 602))
VOICE_MODES = ["POLY", "SOLO", "LEGATO", "UNISON", "POLY 2"]
VM_X = [100, 128, 156, 184, 212]
VM_Y = [630, 654]
for i, s_ in enumerate(VOICE_MODES):
    PRINT.append((s_, VM_X[i], 616, 6.9, "navy", "mm"))
for n, yy in zip((1, 2), VM_Y):
    PRINT.append((str(n), 48, yy, 8.4, "navy", "mm"))
    key(f"vmode_{n}", 72, yy, "", w=26, h=16, led=False)
    for k, xx in enumerate(VM_X):
        lamp(f"vm{n}_{k}", xx, yy, r=1.6, style="led", on=(k == (0 if n == 1 else 3)))
PLAY_MODES = ["SINGLE", "DUAL", "SPLIT", "ONE-TWO", "ROTATE", "RANDOM", "CHAOS", "CASCADE"]
ring("play_mode", 96, 720, "PLAY MODE", [str(i + 1) for i in range(8)], 36.0, set_=2, shift="ZONES", name_dy=44)
PLAY_LEGEND_XY = (154, 678)
PLAY_LEGEND_STEP = 12.6

# ---- ARPEGGIATOR
for i, (pid, lab) in enumerate([("arp_on", "ON"), ("arp_hold", "HOLD"), ("arp_mode", "MODE"), ("arp_range", "RANGE")]):
    key(pid, 329 + i * 46, 556, lab, set_=(pid == "arp_on"))
key("tap", 334, 640, "TAP", w=34, h=24, shift="DIVISION")
knob("clock", "med", 404, 634, "TEMPO", labels([(0, "40"), (1, "240")]), T10, set_=0.45)
key("arp_sync", 470, 640, "HOST", set_=True)
# Variation: DICE rolls a new set of small differences, note by note; VARY sets how much of it plays.
key("dice", 352, 726, "icon:dice", w=34, h=22)                 # labelled with a printed pair of dice
knob("vary", "small", 446, 722, "VARY", set_=0.3)

# ---- EFFECTS (the edited layer's)
knob("fx_drive", "small", 556, 558, "DRIVE", set_=0.3)
key("fx_chorus", 621, 564, "CHORUS", set_=True)
knob("fx_bal", "small", 686, 558, "BALANCE", labels([(0, "DLY"), (1, "REV")]), ticks(10), set_=0.4)
knob("fx_time", "small", 556, 652, "DELAY", set_=0.5)
knob("fx_fb", "small", 621, 652, "FEEDBACK", set_=0.35)
knob("fx_size", "small", 686, 652, "REVERB", set_=0.6)
key("fx_freeze", 621, 742, "FREEZE", w=34, h=22)

# ---- PROGRAMMER
DISPLAY = {"id": "disp", "x": 1009, "y": 566, "w": 300, "h": 92}
ENC_X = [898, 972, 1046, 1120]
ENC_Y = 662
for i, x in enumerate(ENC_X):
    CONTROLS.append({"id": f"enc_{i}", "type": "knob", "kind": "small", "x": x, "y": ENC_Y, "name": None, "sub": None,
                     "labels": [], "ticks": [], "set": [0.1, 0.4, 0.7, 0.25][i], "encoder": True, "shift": None})
key("shift", 798, 548, "SHIFT", w=40, h=24, led=False, cap="shift")
PRINT += [("HOLD + ANY CONTROL", 798, 584, 6.9, "shift", "mm"), ("OPENS ITS PAGE", 798, 593, 6.9, "shift", "mm")]
key("assign", 798, 640, "ASSIGN", w=40, h=22)
PRINT.append(("THEN TURN A KNOB", 798, 670, 6.9, "navy", "mm"))
key("compare", 778, 742, "COMPARE", shift="INIT")
key("write", 822, 742, "WRITE", shift="NAME")
key("page_prev", 1196, 548, "", w=30, h=19, led=False)
key("page_next", 1240, 548, "", w=30, h=19, led=False)
PRINT += [("◂", 1196, 548, 9.0, "glyph", "mm"), ("▸", 1240, 548, 9.0, "glyph", "mm"), ("PAGE", 1218, 567, 7.6, "navy", "mm")]
for i, (pid, lab) in enumerate([("pg_perf", "PERF"), ("pg_voice", "VOICE"), ("pg_mod", "MOD"), ("pg_arp", "ARP"), ("pg_fx", "FX")]):
    key(pid, 1197, 606 + i * 38, lab, w=28, h=19, set_=(pid == "pg_mod"), label_side="right")
SUBHEADS.append(("PATCH", 865, 1153, 712))
BANKS = "ABCDEFGH"
for i in range(8):
    key(f"patch_{i + 1}", 1009 + (i - 3.5) * 38, 746, str(i + 1), w=28, h=19, set_=(i == 3), shift=BANKS[i])

# ---- LAYER MIX
MIXC = [1332, 1436]
MIX_HEAD_Y = 524
for n, x in zip((1, 2), MIXC):
    knob(f"level{n}", "small", x, 562, "LEVEL", set_=[0.75, 0.6][n - 1])
    knob(f"pan{n}", "small", x, 638, "PAN", labels([(0, "L"), (1, "R")]), ticks(10), set_=[0.38, 0.64][n - 1])
    knob(f"send{n}", "small", x, 714, "FX SEND", set_=[0.3, 0.45][n - 1])
# Each layer's voices, a ladder of lamps between the columns: lit while a voice sounds.
LADDER_X = [1372, 1396]
LADDER_Y0, LADDER_STEP = 744, 12.6
for n, x in zip((1, 2), LADDER_X):
    for v in range(16):
        lamp(f"v{n}_{v + 1}", x, LADDER_Y0 - v * LADDER_STEP, r=1.5, style="led", on=(v < [3, 2][n - 1]))
PRINT += [("1", 1384, LADDER_Y0, 6.6, "navy", "mm"), ("16", 1384, LADDER_Y0 - 15 * LADDER_STEP, 6.6, "navy", "mm"),
          ("VOICES", 1384, 766, 7.4, "navy", "mm")]

# ---- OUTPUT
knob("volume", "large", 1549, 576, "VOLUME", set_=0.8)
key("setup", 1549, 712, "SETUP", w=40, h=22, led=False)

# ============================================================ The top band
BRAND = {"text": "RUMPUS ROOM RECORDS", "x": 52, "y": 34, "size": 16.5}
TYPE_PLATE = {"text": "TYPE DS 8", "x": 52, "y": 55, "size": 8.0}
WORDMARK = {"text": "TANDEM", "x": 1530, "y": 44, "size": 40}          # right edge
SUBLINE = {"text": "BI-TIMBRAL SYNTHESIZER  ·  2 × 4 VOICES", "x": 1530, "y": 74, "size": 8.2}
STRIPES = {"x1": 1214, "y": 44, "w": 34, "h": 9, "gap": 6}             # five bars in the cap colours, ending at x1
STRIPE_ORDER = ["osc", "vcf", "lfo", "env", "prog"]
lamp("power", 1588, 40, r=5.5, color="red", on=True)
PRINT.append(("POWER", 1588, 60, 7.0, "navy", "mm"))
SCREWS = [(36, 16), (W / 2, 16), (W - 36, 16), (36, H_PANEL - 10), (W / 2, H_PANEL - 10), (W - 36, H_PANEL - 10)]
DISPLAY_SCREWS = [(DISPLAY["x"] + sx * (DISPLAY["w"] / 2 - 7), DISPLAY["y"] + sy * (DISPLAY["h"] / 2 - 7)) for sx in (-1, 1) for sy in (-1, 1)]

# ============================================================ The drawer: set and forget
DRAWER = {"x0": 60, "x1": W - 60, "y0": H_PANEL + 4, "y1": H_PANEL + DRAWER_H - 8}
if DRAWER_OPEN:
    dy = H_PANEL + 62
    knob("d_tune", "trim", 150, dy, "MASTER TUNE", FINE, ticks(10), set_=0.5)
    knob("d_trans", "trim", 300, dy, "TRANSPOSE", labels([(0, "–12"), (0.5, "0"), (1, "+12")]), ticks(12), set_=0.5)
    key("d_vel", 456, dy + 6, "VELOCITY", lamps=["SOFT", "NORM", "HARD"], lit=(1,))
    key("d_press", 612, dy + 6, "PRESSURE", lamps=["OFF", "NORM", "HARD"], lit=(1,))
    knob("d_ch1", "trim", 762, dy, "MIDI CH  L1", labels([(0, "ALL"), (1, "16")]), ticks(16), set_=0.0)
    knob("d_ch2", "trim", 892, dy, "MIDI CH  L2", labels([(0, "ALL"), (1, "16")]), ticks(16), set_=0.0)
    knob("d_age", "trim", 1036, dy, "AGE", ZERO_TEN, T10, set_=0.5)
    key("d_pedal", 1180, dy + 6, "PEDAL", lamps=["NORM", "INV"], lit=(0,))
    key("d_size", 1324, dy + 6, "SIZE", lamps=["80", "100", "125"], lit=(0,))
    key("d_close", 1500, dy + 6, "CLOSE", w=40, h=22, led=False)

# What the display and the filter-type window show on the mockup.
DISPLAY_SHOW = {"title": "MOD  MATRIX 1", "page": "1 / 19",
                "cols": [("SOURCE", "LFO 2", 0.25), ("DEST", "Pitch", 0.85), ("AMOUNT", "+6", 0.55), ("VIA", "Mod Wheel", 0.9)]}
TYPE_SHOW = "4PLP"
# Other pages, for the canvas: what holding SHIFT and touching a control opens.
DISPLAY_PAGES = {
    "page_lfo": {"title": "LFO 1", "page": "SHIFT + WAVE", "cols": [("MODE", "Free", 0.0), ("PHASE", "90°", 0.25), ("CLOCK", "1/8", 0.55), ("USER WAVE", "Choir", 0.7)]},
    "page_env3": {"title": "ENV 3", "page": "SHIFT + VCA", "cols": [("ATTACK", "12 ms", 0.1), ("DECAY", "1.4 s", 0.55), ("SUSTAIN", "6.0", 0.6), ("RELEASE", "800 ms", 0.45)]},
    "page_voice": {"title": "VOICE", "page": "SHIFT + GLIDE", "cols": [("MODE", "Unison", 0.75), ("VOICES", "2", 0.25), ("DRIFT", "3.0", 0.3), ("SPREAD", "60 %", 0.6)]},
    "page_zones": {"title": "ZONES", "page": "SHIFT + PLAY MODE", "cols": [("MODE", "Split", 0.3), ("POINT", "C3", 0.45), ("LAYER 1", "Lower", 0.0), ("LAYER 2", "+1 oct", 0.62)]},
}


def control(pid):
    return next(c for c in CONTROLS if c["id"] == pid)


def norm_angle(n):
    return -SWEEP + 2 * SWEEP * n


def set_angle(ctl):
    if ctl["kind"] == "ring":
        return ctl["steps"][ctl["set"]]
    if ctl.get("encoder"):
        return 360.0 * ctl["set"]
    return norm_angle(ctl["set"])


def lamp_xs(ctl):
    n = len(ctl["lamps"])
    return [ctl["x"] + (i - (n - 1) / 2) * LAMP_SP for i in range(n)]


def lamp_y(ctl):
    return ctl["y"] - ctl["h"] / 2 - 7.5
