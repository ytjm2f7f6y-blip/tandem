"""Tandem on one panel: the colour-caps finish (direction C), one powder-coated sheet with printed
section boxes. Points at 100%. Angles: 0 = straight up, clockwise positive; continuous knobs
sweep -150..+150 degrees.

Sized for a 14-inch MacBook: at 100% the panel (1460 x 800) fits beside Logic's own window frame,
and nothing is printed smaller than 8.8 pt. Coordinates inside a box are given from its corner.

The top band names the sound on each layer (the plugin writes it in two windows: click for the
list, the arrows step). Top row: the sound of the layer being edited (the LAYER keys choose which).
Bottom row: playing both layers, and the programmer that reaches everything else.

Switched settings are lamp keys and multi-way choices are encoders ringed with lamps, so a
patch recall can show every setting. Secondary functions are printed in a second ink (oxblood)
and reached with SHIFT. Set-and-forget settings live in a drawer that slides up over the bottom
row (TD_DRAWER=1 renders it pulled out, below the panel, for the plugin to draw over it).
"""
import os

DRAWER_OPEN = os.environ.get("TD_DRAWER") == "1"
W = 1460
H_PANEL = 800
DRAWER_H = 150
H = H_PANEL + (DRAWER_H if DRAWER_OPEN else 0)
SCALE = 2
SWEEP = 150.0
STRIP_FRAMES = 81
CHEEK = 18                      # cream end cheeks
PANEL_X = (CHEEK, W - CHEEK)

KNOBS = {"large": 30, "med": 24, "small": 18, "ring": 19, "trim": 13, "sel": 23}
TYPE = {"head": 10.5, "name": 10.0, "num": 8.8, "sub": 8.8, "lamp": 8.8, "sec": 9.0}    # nothing printed under 8.8 pt
LABEL_R = 15.5
NAME_GAP = 24.0
RING_LED_R = 27.0               # lamps round an encoder
RING_LABEL_R = 38.0
RING_ICON_R = 41.0
LAMP_SP = 24.0                  # lamps over a key, apart
FADER_CAP = (18.0, 28.0)
FADER_BOX = (46.0, 60.0)
FADER_PLATE_W = 14.0
POINTERS = {}
POINTER_COLOURS = {}

HEAD_H = 22                     # the printed header bar on each box
ROW1 = (68, 452)
ROW2 = (462, 784)
# Boxes: id, title, x0, x1, row. The cap colour follows the box (theme "caps").
BOXES = [
    ("osc", "OSCILLATORS", 30, 495, ROW1), ("mix", "MIXER", 505, 605, ROW1), ("vcf", "FILTER", 615, 894, ROW1),
    ("lfo", "LFO", 904, 1078, ROW1), ("env", "ENVELOPES", 1088, 1430, ROW1),
    ("perf", "PERFORMANCE", 30, 268, ROW2), ("arp", "ARPEGGIATOR", 278, 468, ROW2), ("fx", "EFFECTS", 478, 690, ROW2),
    ("prog", "PROGRAMMER", 700, 1152, ROW2), ("lmix", "MIX", 1162, 1430, ROW2),
]
BX = {b[0]: b[2] for b in BOXES}
BX1 = {b[0]: b[3] for b in BOXES}
CASSETTES = [{"id": b[0], "x0": b[2], "x1": b[3]} for b in BOXES]       # for render.section_of (patched there)

CONTROLS = []
SUBHEADS = []           # (text, x0, x1, y): a small heading with rules
PRINT = []              # loose printed words: (text, x, y, size, ink, anchor)  ink "navy"|"shift"|"glyph"


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
Y1 = ROW1[0]
# ---- OSCILLATORS
X0, X1 = BX["osc"], BX1["osc"]
XC = (X0 + X1) / 2
OSC_WAVES = ["icon:sine", "icon:saw", "icon:square", "icon:pulse", "icon:tri", "icon:noise", "1", "2", "3", "4", "5", "6"]
OSC2_WAVES = ["icon:sine", "icon:saw", "icon:square", "icon:pulse", "icon:tri", "icon:noise", "1", "2", "3", "D1", "D2", "D3"]
SUBHEADS += [("OSC 1", X0 + 14, X0 + 170, Y1 + 36), ("OSC 2", X1 - 170, X1 - 14, Y1 + 36)]
ring("o1_wave", X0 + 86, Y1 + 106, "WAVEFORM", OSC_WAVES, 26.0, set_=1)
ring("o2_wave", X1 - 86, Y1 + 106, "WAVEFORM", OSC2_WAVES, 26.0, set_=2)
key("o_sync", XC, Y1 + 106, "SYNC", lamps=["2›1", "1›2"], lit=())
TUNE = labels([(0, "–7"), (0.5, "0"), (1, "+7")])
FINE = labels([(0, "–50"), (0.5, "0"), (1, "+50")])
knob("o1_tune", "med", X0 + 44, Y1 + 202, "TUNE", TUNE, ticks(14), set_=0.5)
knob("o1_fine", "med", X0 + 126, Y1 + 202, "FINE", FINE, ticks(10), sub="CENTS", set_=0.5)
knob("o2_tune", "med", X1 - 126, Y1 + 202, "TUNE", TUNE, ticks(14), set_=1.0)
knob("o2_fine", "med", X1 - 44, Y1 + 202, "FINE", FINE, ticks(10), sub="CENTS", set_=0.56)
ring("octave", XC, Y1 + 206, "OCTAVE", ["–2", "–1", "0", "+1", "+2"], 30.0, set_=2)
SUBHEADS.append(("MODULATION", X0 + 14, X1 - 14, Y1 + 290))
MY = Y1 + 332
knob("m_detune", "med", X0 + 58, MY, "DETUNE", set_=0.35)
key("m_osc", X0 + 128, MY + 8, "TO OSC", lamps=["1", "2"], lit=(0, 1))
knob("m_wave", "med", X0 + 205, MY, "WAVE / PWM", set_=0.4)
key("m_wsrc", X0 + 275, MY + 8, "SOURCE", lamps=["ENV1", "LFO1"], lit=(1,))
knob("m_pitch", "med", X1 - 113, MY, "PITCH MOD", set_=0.1)
key("m_psrc", X1 - 41, MY + 8, "SOURCE", lamps=["ENV1", "LFO1", "X-MOD"], lit=(1,))

# ---- MIXER (into the filter)
X0, X1 = BX["mix"], BX1["mix"]
XC = (X0 + X1) / 2
knob("x_osc1", "med", XC, Y1 + 92, "OSC 1", set_=0.55)
knob("x_osc2", "med", XC, Y1 + 190, "OSC 2", set_=0.4)
knob("x_ext", "med", XC, Y1 + 288, "EXT IN", set_=0.0)
lamp("odrive", X0 + 22, Y1 + 362, r=4.2, color="red", on=False)
PRINT.append(("O.DRIVE", X0 + 34, Y1 + 362, 8.8, "navy", "lm"))

# ---- FILTER
X0, X1 = BX["vcf"], BX1["vcf"]
XC = (X0 + X1) / 2
knob("f_cutoff", "large", X0 + 72, Y1 + 96, "CUTOFF", set_=0.62)
knob("f_res", "large", X1 - 72, Y1 + 96, "RESONANCE", set_=0.45)
TYPE_WIN = {"id": "f_type", "x": XC, "y": Y1 + 178, "w": 108, "h": 26}
key("f_prev", XC - 72, Y1 + 178, "", w=22, h=19, led=False)
key("f_next", XC + 72, Y1 + 178, "", w=22, h=19, led=False)
PRINT += [("◂", XC - 72, Y1 + 178, 9.0, "glyph", "mm"), ("▸", XC + 72, Y1 + 178, 9.0, "glyph", "mm")]
TYPE_LABEL_Y = Y1 + 205
knob("f_mod", "med", X0 + 64, Y1 + 262, "MODULATION", set_=0.45)
key("f_msrc", XC, Y1 + 270, "SOURCE", lamps=["ENV1", "LFO1", "OSC2"], lit=(0,))
knob("f_delta", "med", X1 - 64, Y1 + 262, "Δ-CUTOFF", BIPOLAR, ticks(10), set_=0.5)
key("f_kt", X0 + 64, Y1 + 352, "KEYTRACK", lamps=["HALF", "FULL"], lit=(0,))
key("f_restype", XC, Y1 + 352, "RES TYPE", lamps=["1", "2"], lit=(0,))
key("f_route", X1 - 64, Y1 + 352, "ROUTING", lamps=["ST", "PAR", "SER"], lit=(0,))

# ---- LFO 1 and LFO 2
X0, X1 = BX["lfo"], BX1["lfo"]
LFO_WAVES = ["icon:sine", "icon:saw", "icon:sh", "icon:square", "icon:ramp", "icon:noise", "icon:tri", "icon:nsine", "1", "2", "HF", "TRK"]
SUBHEADS += [("LFO 1", X0 + 10, X1 - 10, Y1 + 36), ("LFO 2", X0 + 10, X1 - 10, Y1 + 280)]
ring("l_wave", X0 + 56, Y1 + 120, "WAVE", LFO_WAVES, 26.0, set_=6)
FADER_Y, FADER_TRAVEL = Y1 + 164, 132
fader("l_rate", X0 + 122, FADER_Y, FADER_TRAVEL, "RATE", scale=True, set_=0.55)
fader("l_delay", X0 + 154, FADER_Y, FADER_TRAVEL, "DELAY", set_=0.0)
knob("lfo2_rate", "med", X0 + 52, Y1 + 322, "RATE", set_=0.45)
key("lfo2_trig", X0 + 128, Y1 + 330, "TRIGGER", lamps=["WHL", "AT", "ON"], lit=(0,))

# ---- ENVELOPES
X0, X1 = BX["env"], BX1["env"]
SUBHEADS += [("ENV 1", X0 + 10, X0 + 174, Y1 + 36), ("ENV 2", X0 + 192, X1 - 10, Y1 + 36)]
E1 = [X0 + 34 + 32 * i for i in range(5)]
E2 = [X0 + 222 + 32 * i for i in range(4)]
for i, (pid, nm) in enumerate(zip(["e1_h", "e1_a", "e1_d", "e1_s", "e1_r"], "HADSR")):
    fader(pid, E1[i], FADER_Y, FADER_TRAVEL, nm, scale=(i == 0), set_=[0.0, 0.08, 0.5, 0.25, 0.35][i])
for i, (pid, nm) in enumerate(zip(["e2_a", "e2_d", "e2_s", "e2_r"], "ADSR")):
    fader(pid, E2[i], FADER_Y, FADER_TRAVEL, nm, scale=(i == 0), set_=[0.02, 0.4, 0.8, 0.3][i])
knob("e1_vel", "med", X0 + 44, Y1 + 316, "VELOCITY", BIPOLAR, ticks(10), sub="TO ENV 1", set_=0.6)
key("e1_mode", X0 + 126, Y1 + 324, "MODE", lamps=["INV", "LOOP"], lit=())
knob("e2_vel", "med", X0 + 226, Y1 + 316, "VELOCITY", BIPOLAR, ticks(10), sub="TO VCA", set_=0.62)
key("vca_src", X1 - 40, Y1 + 324, "VCA", lamps=["ENV2", "ENV3", "GATE"], lit=(0,), shift="ENV 3")

# ============================================================ BOTTOM ROW: playing and programming
Y2 = ROW2[0]
# ---- PERFORMANCE
X0, X1 = BX["perf"], BX1["perf"]
SUBHEADS.append(("EDIT", X0 + 14, X0 + 148, Y2 + 40))
key("layer_1", X0 + 46, Y2 + 74, "LAYER 1", w=46, h=26, set_=True, shift="SWAP")
key("layer_2", X0 + 112, Y2 + 74, "LAYER 2", w=46, h=26, shift="COPY")
knob("glide", "med", X1 - 48, Y2 + 70, "GLIDE", set_=0.15)
# Each layer's voice mode, in a printed grid: a key per layer steps it, a lamp under each mode.
SUBHEADS.append(("VOICE MODE", X0 + 14, X1 - 14, Y2 + 130))
VOICE_MODES = ["POLY", "SOLO", "LEGATO", "UNISON", "POLY 2"]
VM_X = [X0 + 60 + 36 * i for i in range(5)]
VM_Y = [Y2 + 172, Y2 + 196]
for i, s_ in enumerate(VOICE_MODES):
    PRINT.append((s_, VM_X[i], Y2 + 150, 8.8, "navy", "mm"))
for n, yy in zip((1, 2), VM_Y):
    PRINT.append((str(n), X0 + 14, yy, 9.2, "navy", "mm"))
    key(f"vmode_{n}", X0 + 36, yy, "", w=26, h=16, led=False)
    for k, xx in enumerate(VM_X):
        lamp(f"vm{n}_{k}", xx, yy, r=1.6, style="led", on=(k == (0 if n == 1 else 3)))
PLAY_MODES = ["SINGLE", "DUAL", "SPLIT", "ONE-TWO", "ROTATE", "RANDOM", "CHAOS", "CASCADE"]
ring("play_mode", X0 + 66, Y2 + 258, "PLAY MODE", [str(i + 1) for i in range(8)], 36.0, set_=2, shift="ZONES", name_dy=44)
PLAY_LEGEND_XY = (X0 + 126, Y2 + 212)
PLAY_LEGEND_STEP = 12.6

# ---- ARPEGGIATOR
X0, X1 = BX["arp"], BX1["arp"]
for i, (pid, lab) in enumerate([("arp_on", "ON"), ("arp_hold", "HOLD"), ("arp_mode", "MODE"), ("arp_range", "RANGE")]):
    key(pid, X0 + 30 + 44 * i, Y2 + 74, lab, set_=(pid == "arp_on"))
key("tap", X0 + 36, Y2 + 160, "TAP", w=34, h=24, shift="DIVISION")
knob("clock", "med", X0 + 100, Y2 + 154, "TEMPO", labels([(0, "40"), (1, "240")]), T10, set_=0.45)
key("arp_sync", X1 - 30, Y2 + 160, "HOST", set_=True)
# Variation: DICE rolls a new set of small differences, note by note; VARY sets how much of it plays.
key("dice", X0 + 70, Y2 + 252, "icon:dice", w=34, h=22)                 # labelled with a printed pair of dice
knob("vary", "small", X0 + 146, Y2 + 246, "VARY", set_=0.3)

# ---- EFFECTS (the edited layer's)
X0, X1 = BX["fx"], BX1["fx"]
XC = (X0 + X1) / 2
knob("fx_drive", "small", X0 + 46, Y2 + 96, "DRIVE", set_=0.3)
key("fx_chorus", XC, Y2 + 102, "CHORUS", set_=True)
knob("fx_bal", "small", X1 - 46, Y2 + 96, "BALANCE", labels([(0, "DLY"), (1, "REV")]), ticks(10), set_=0.4)
knob("fx_time", "small", X0 + 46, Y2 + 190, "DELAY", set_=0.5)
knob("fx_fb", "small", XC, Y2 + 190, "FEEDBACK", set_=0.35)
knob("fx_size", "small", X1 - 46, Y2 + 190, "REVERB", set_=0.6)
key("fx_freeze", XC, Y2 + 278, "FREEZE", w=34, h=22)

# ---- PROGRAMMER: the display, its menus, SHIFT and ASSIGN, and the edit keys
X0, X1 = BX["prog"], BX1["prog"]
DISPLAY = {"id": "disp", "x": X0 + 261, "y": Y2 + 96, "w": 346, "h": 116}
_face_x0 = DISPLAY["x"] - DISPLAY["w"] / 2 + 9
_face_w = DISPLAY["w"] - 18
ENC_X = [_face_x0 + _face_w * (i + 0.5) / 4 for i in range(4)]       # the display's four columns
key("shift", X0 + 42, Y2 + 64, "SHIFT", w=42, h=26, led=False, cap="shift")
PRINT += [("THEN TOUCH", X0 + 42, Y2 + 98, 9.0, "shift", "mm"), ("ANY CONTROL", X0 + 42, Y2 + 109, 9.0, "shift", "mm"),
          ("FOR ITS PAGE", X0 + 42, Y2 + 120, 9.0, "shift", "mm")]
key("assign", X0 + 42, Y2 + 170, "ASSIGN", w=42, h=24)
PRINT += [("THEN TURN", X0 + 42, Y2 + 200, 8.8, "navy", "mm"), ("A KNOB", X0 + 42, Y2 + 211, 8.8, "navy", "mm")]
for i, (pid, lab) in enumerate([("pg_perf", "PERF"), ("pg_voice", "VOICE"), ("pg_mod", "MOD"), ("pg_arp", "ARP"), ("pg_fx", "FX")]):
    key(pid, X0 + 124 + 69 * i, Y2 + 190, lab, w=34, h=19, set_=(pid == "pg_mod"))
key("compare", X0 + 42, Y2 + 276, "COMPARE", w=34, h=20, shift="INIT")
key("write", X0 + 112, Y2 + 276, "WRITE", w=34, h=20, shift="NAME")
key("page_prev", X0 + 213, Y2 + 276, "", w=34, h=20, led=False)
key("page_next", X0 + 268, Y2 + 276, "", w=34, h=20, led=False)
PRINT += [("◂", X0 + 213, Y2 + 276, 9.0, "glyph", "mm"), ("▸", X0 + 268, Y2 + 276, 9.0, "glyph", "mm"),
          ("PAGE", X0 + 240.5, Y2 + 295.5, 8.8, "navy", "mm")]
key("undo", X1 - 90, Y2 + 276, "UNDO", w=34, h=20, led=False)
key("redo", X1 - 36, Y2 + 276, "REDO", w=34, h=20, led=False)

# ---- MIX: each layer's level, pan and send, their voices, and the master volume
X0, X1 = BX["lmix"], BX1["lmix"]
MIXC = [X0 + 40, X0 + 140]
MIX_HEAD_Y = Y2 + 42
for n, x in zip((1, 2), MIXC):
    knob(f"level{n}", "small", x, Y2 + 82, "LEVEL", set_=[0.75, 0.6][n - 1])
    knob(f"pan{n}", "small", x, Y2 + 160, "PAN", labels([(0, "L"), (1, "R")]), ticks(10), set_=[0.38, 0.64][n - 1])
    knob(f"send{n}", "small", x, Y2 + 238, "FX SEND", set_=[0.3, 0.45][n - 1])
# Each layer's voices, a ladder of lamps between the columns: lit while a voice sounds.
LADDER_X = [X0 + 78, X0 + 102]
LADDER_Y0, LADDER_STEP = Y2 + 290, 12.6
for n, x in zip((1, 2), LADDER_X):
    for v in range(16):
        lamp(f"v{n}_{v + 1}", x, LADDER_Y0 - v * LADDER_STEP, r=1.5, style="led", on=(v < [3, 2][n - 1]))
PRINT += [("1", X0 + 90, LADDER_Y0, 8.8, "navy", "mm"), ("16", X0 + 90, LADDER_Y0 - 15 * LADDER_STEP, 8.8, "navy", "mm"),
          ("VOICES", X0 + 90, Y2 + 310, 8.8, "navy", "mm")]
PRINT.append(("MASTER", X1 - 46, MIX_HEAD_Y, 10.0, "navy", "mm"))
knob("volume", "large", X1 - 46, Y2 + 100, "VOLUME", set_=0.8)
key("setup", X1 - 46, Y2 + 250, "SETUP", w=44, h=22, led=False)

# ============================================================ The top band: the name, and each layer's sound
BRAND = {"text": "RUMPUS ROOM RECORDS", "x": 40, "y": 26, "size": 15.0}
TYPE_PLATE = {"text": "TYPE DS 8", "x": 40, "y": 46, "size": 8.8}
# The two layers' patch windows: the plugin writes the sound's name, with arrows at each end.
PATCH_WINS = [{"id": "pw1", "x": 580, "y": 34, "w": 236, "h": 30}, {"id": "pw2", "x": 880, "y": 34, "w": 236, "h": 30}]
PRINT += [("1", 448, 34, 11.0, "navy", "mm"), ("2", 748, 34, 11.0, "navy", "mm")]
WORDMARK = {"text": "TANDEM", "x": 1360, "y": 28, "size": 34}          # right edge
SUBLINE = {"text": "BI-TIMBRAL SYNTHESIZER  ·  2 × 16 VOICES", "x": 1360, "y": 52, "size": 8.8}
STRIPES = {"x1": 1166, "y": 28, "w": 24, "h": 8, "gap": 5}             # five bars in the cap colours, ending at x1
STRIPE_ORDER = ["osc", "vcf", "lfo", "env", "prog"]
lamp("power", 1404, 24, r=5.5, color="red", on=True)
PRINT.append(("POWER", 1404, 44, 8.8, "navy", "mm"))
SCREWS = [(32, 10), (W / 2, 10), (W - 32, 10), (32, H_PANEL - 8), (W / 2, H_PANEL - 8), (W - 32, H_PANEL - 8)]
DISPLAY_SCREWS = [(DISPLAY["x"] + sx * (DISPLAY["w"] / 2 - 7), DISPLAY["y"] + sy * (DISPLAY["h"] / 2 - 7)) for sx in (-1, 1) for sy in (-1, 1)]

# ============================================================ The drawer: set and forget
DRAWER = {"x0": 50, "x1": W - 50, "y0": H_PANEL + 4, "y1": H_PANEL + DRAWER_H - 8}
if DRAWER_OPEN:
    dy = H_PANEL + 62
    knob("d_tune", "trim", 130, dy, "MASTER TUNE", FINE, ticks(10), set_=0.5)
    knob("d_trans", "trim", 262, dy, "TRANSPOSE", labels([(0, "–12"), (0.5, "0"), (1, "+12")]), ticks(12), set_=0.5)
    key("d_vel", 400, dy + 6, "VELOCITY", lamps=["SOFT", "NORM", "HARD"], lit=(1,))
    key("d_press", 540, dy + 6, "PRESSURE", lamps=["OFF", "NORM", "HARD"], lit=(1,))
    knob("d_ch1", "trim", 672, dy, "MIDI CH  L1", labels([(0, "ALL"), (1, "16")]), ticks(16), set_=0.0)
    knob("d_ch2", "trim", 790, dy, "MIDI CH  L2", labels([(0, "ALL"), (1, "16")]), ticks(16), set_=0.0)
    knob("d_age", "trim", 916, dy, "AGE", ZERO_TEN, T10, set_=0.5)
    key("d_pedal", 1046, dy + 6, "PEDAL", lamps=["NORM", "INV"], lit=(0,))
    key("d_size", 1180, dy + 6, "SIZE", lamps=["80", "100", "125"], lit=(1,))
    key("d_close", 1340, dy + 6, "CLOSE", w=40, h=22, led=False)

# What the display and the windows show on the mockup.
DISPLAY_SHOW = {"title": "MOD  MATRIX 1", "page": "2 / 20",
                "cols": [("SOURCE", "LFO 2", 0.25), ("DEST", "Pitch", 0.85), ("AMOUNT", "+6", 0.55), ("VIA", "Mod Wheel", 0.9)]}
TYPE_SHOW = "4PLP"
PATCH_SHOW = ["A2  twin saws", "B4  resonant bass"]
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
