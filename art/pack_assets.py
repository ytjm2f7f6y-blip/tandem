"""Packs the renders in art/build into the plugin's assets.

    python3 art/pack_assets.py

- panel.jpg            the bare faceplate (render.py panel --state bare)
- lit_<group>.png      the lit patches (render.py panel --state amber)
- knob_sel.png         the selector at each angle it can rest at (render.py knob_sel)
- knob_small.jpg/_mask the encoders, all the way round (render.py knob_small)
- toggle_l/c/r.png, fader_cap.png   (render.py parts)
The round knobs' strips (knob_large, knob_med) are the same parts as Sam's Desk's and Kontur's.
"""
import os

import numpy as np
from PIL import Image

import layout as L

HERE = os.path.dirname(os.path.abspath(__file__))
BUILD = os.path.join(HERE, "build")
ASSETS = os.path.join(HERE, "..", "plugin", "assets")


def clean_catcher(im, frames=1):
    """Sprites rendered on a shadow catcher pick up a faint, even tint across the whole box.
    Take that level out, and fade the last few pixels to nothing, so the box never shows."""
    a = np.asarray(im.convert("RGBA")).astype(np.float32).copy()
    h = a.shape[0] // frames
    for f in range(frames):
        fr = a[f * h:(f + 1) * h]
        al = fr[..., 3]
        border = np.concatenate([al[0, :], al[-1, :], al[:, 0], al[:, -1]])
        bgl = float(np.median(border))
        if bgl > 0:
            fr[..., 3] = np.clip((al - bgl) / max(1e-3, 255 - bgl) * 255, 0, 255)
        n = 6
        ramp = np.minimum(np.minimum(np.arange(fr.shape[0])[:, None], np.arange(fr.shape[0])[::-1, None]),
                          np.minimum(np.arange(fr.shape[1])[None, :], np.arange(fr.shape[1])[None, ::-1]))
        fr[..., 3] *= np.clip(ramp / n, 0, 1)
        a[f * h:(f + 1) * h] = fr
    return Image.fromarray(np.clip(a + 0.5, 0, 255).astype(np.uint8), "RGBA")


def round_strip(src, name):
    """A round knob's strip as the plugin loads it: colour premultiplied on black (JPEG), one shared mask."""
    a = np.asarray(Image.open(src).convert("RGBA")).astype(np.float32)
    fw = a.shape[1]
    n = a.shape[0] // fw
    alpha = a[..., 3:4] / 255.0
    rgb = a[..., :3] * alpha
    Image.fromarray(np.clip(rgb + 0.5, 0, 255).astype(np.uint8), "RGB").save(os.path.join(ASSETS, name + ".jpg"), quality=90)
    mask = a[..., 3].reshape(n, fw, fw).max(axis=0)
    Image.fromarray(np.clip(mask + 0.5, 0, 255).astype(np.uint8), "L").save(os.path.join(ASSETS, name + "_mask.png"), optimize=True)
    print(name, n, "frames")


if __name__ == "__main__":
    os.makedirs(ASSETS, exist_ok=True)
    for name in ("bare_panel.jpg", "bare_panel_preview.jpg", "bare_panel_draft.jpg"):
        p = os.path.join(BUILD, name)
        if os.path.exists(p):
            Image.open(p).convert("RGB").save(os.path.join(ASSETS, "panel.jpg"), quality=90, subsampling=0, optimize=True)
            print("panel from", name)
            break
    for g in L.LIT_GROUPS:
        Image.open(os.path.join(BUILD, "lit", f"amber_{g}.png")).convert("RGB").save(os.path.join(ASSETS, f"lit_{g}.png"), optimize=True)
    print("lit patches:", ", ".join(L.LIT_GROUPS))
    strip = Image.open(os.path.join(BUILD, "strips", "knob_sel.png"))
    n = strip.size[1] // strip.size[0]
    assert n == len(L.SEL_ANGLES), (n, len(L.SEL_ANGLES))
    clean_catcher(strip, n).save(os.path.join(ASSETS, "knob_sel.png"), optimize=True)
    print("knob_sel:", n, "frames")
    round_strip(os.path.join(BUILD, "strips", "knob_small.png"), "knob_small")
    for t in ("toggle_l", "toggle_c", "toggle_r", "fader_cap"):
        clean_catcher(Image.open(os.path.join(BUILD, "sprites", t + ".png"))).save(os.path.join(ASSETS, t + ".png"), optimize=True)
    print("toggles and fader cap")
