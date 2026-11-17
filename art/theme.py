"""Tandem: finishes for the faceplate. render.py picks one with TD_THEME (default "family": the
grey hammertone of Sam's Desk and Kontur). Colours are sRGB 0..1."""

BABY_BLUE = (0.60, 0.77, 0.88)

THEMES = {
    "family": {
        "title": "Family grey (stage 1)",
        "panel": (0.72, 0.705, 0.655), "panel_finish": "hammertone",
        "rail": (0.30, 0.315, 0.33), "rail_finish": "hammertone", "cheeks": None,
        "ink": (0.045, 0.042, 0.038), "rail_ink": (0.86, 0.82, 0.70),
        "knob": "family", "key": (0.86, 0.84, 0.78), "key_finish": "plastic", "led": "red",
        "fader": "phenolic", "display": (0.55, 1.0, 0.72),
    },
    # Cream bakelite knobs, walnut end cheeks, cream keys with red LEDs, green phosphor.
    "ivory": {
        "title": "Powder Blue · Ivory & Walnut",
        "panel": BABY_BLUE, "panel_finish": "powder",
        "rail": (0.53, 0.70, 0.82), "rail_finish": "powder", "cheeks": "walnut",
        "ink": (0.07, 0.11, 0.18), "rail_ink": (0.07, 0.11, 0.18),
        "knob": "ivory", "key": (0.90, 0.86, 0.76), "key_finish": "plastic", "led": "red",
        "fader": "ivory", "display": (0.55, 1.0, 0.72),
    },
    # Knurled aluminium knobs with black tops, navy rails, graphite keys, amber LEDs, a cyan display.
    "machined": {
        "title": "Powder Blue · Machined & Navy",
        "panel": BABY_BLUE, "panel_finish": "powder",
        "rail": (0.085, 0.13, 0.22), "rail_finish": "powder", "cheeks": None,
        "ink": (0.06, 0.09, 0.15), "rail_ink": (0.90, 0.93, 0.97),
        "knob": "machined", "key": (0.20, 0.21, 0.23), "key_finish": "rubber", "led": "amber",
        "fader": "machined", "display": (0.45, 0.95, 1.0),
    },
    # Black knobs with a coloured cap for each section, cream rails, white keys, amber display.
    "capped": {
        "title": "Powder Blue · Colour Caps",
        "panel": BABY_BLUE, "panel_finish": "powder",
        "rail": (0.92, 0.89, 0.81), "rail_finish": "powder", "cheeks": None,
        "ink": (0.07, 0.10, 0.17), "rail_ink": (0.10, 0.13, 0.20),
        "knob": "capped", "key": (0.95, 0.94, 0.91), "key_finish": "plastic", "led": "red",
        "fader": "white", "display": (1.0, 0.72, 0.30),
        "caps": {"osc": (0.93, 0.89, 0.78), "vcf": (0.93, 0.45, 0.18), "lfo": (0.20, 0.60, 0.62),
                 "env": (0.92, 0.72, 0.22), "ctl": (0.80, 0.22, 0.18), "play": (0.93, 0.89, 0.78)},
    },
    # Direction C on one panel: cream cheeks, navy print with a red-orange SHIFT screen, red caps
    # on the lower (playing and programming) row.
    "one": {
        "title": "Powder Blue · Colour Caps, one panel",
        "panel": BABY_BLUE, "panel_finish": "powder", "cheek": (0.92, 0.89, 0.81),
        "ink": (0.05, 0.075, 0.13), "shift_ink": (0.80, 0.27, 0.12),
        "knob": "capped", "key": (0.95, 0.94, 0.91), "key_finish": "plastic", "led": "red", "shift_cap": (0.80, 0.25, 0.14),
        "fader": "white", "display": (1.0, 0.72, 0.30),
        "tray": (0.10, 0.14, 0.24), "tray_ink": (0.93, 0.90, 0.80),
        "caps": {"osc": (0.93, 0.89, 0.78), "mix": (0.93, 0.89, 0.78), "vcf": (0.93, 0.45, 0.18), "lfo": (0.20, 0.60, 0.62),
                 "env": (0.92, 0.72, 0.22), "perf": (0.80, 0.22, 0.18), "arp": (0.80, 0.22, 0.18), "fx": (0.80, 0.22, 0.18),
                 "prog": (0.80, 0.22, 0.18), "lmix": (0.80, 0.22, 0.18), "out": (0.80, 0.22, 0.18), "drawer": (0.93, 0.89, 0.78)},
    },
}
