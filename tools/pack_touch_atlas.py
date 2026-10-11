#!/usr/bin/env python3
"""Packs the phone's touch-control icons (Kenney's Input Prompts, Nintendo Switch set, CC0 - assets/icons/touch/)
into atlases of 64, 128 and 256 pixel cells (assets/runtime/icons/touch_atlas_<cell>.png), 4 columns, in the order
TouchControls.cpp's TouchIcon enum lists them.

The icons are 64x64 with their shape in the middle 48: each is cropped to its shape and fitted to its cell, the bigger
cells upscaled with their edges kept sharp (the shapes are flat - white with see-through letters). The D-pad's arms are
masks of one arm (from switch_dpad_up's highlighted one, turned for the others), cropped like the D-pad itself so they
line up with it; TouchControls tints them when pressed.

Usage: python tools/pack_touch_atlas.py"""
from pathlib import Path
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "assets" / "icons" / "touch"
OUT = ROOT / "assets" / "runtime" / "icons"
CELLS = (64, 128, 256)
COLUMNS = 4


def load(name):
    return np.asarray(Image.open(SRC / name).convert("RGBA")).astype(np.float32) / 255.0


def bbox(a):
    ys, xs = np.nonzero(a[..., 3] > 0)
    return xs.min(), ys.min(), xs.max() + 1, ys.max() + 1


dpad = load("switch_dpad.png")
up = load("switch_dpad_up.png")
# the highlighted arm: how red a pixel is (white 255 green -> 0, the red ~50 green -> 1), as a white mask
redness = np.clip((1.0 - up[..., 1]) / (1.0 - 50 / 255.0), 0, 1) * up[..., 3]
arm = np.zeros_like(up); arm[..., :3] = 1.0; arm[..., 3] = redness
dbox = bbox(dpad)
ICONS = [("Dpad", dpad, dbox), ("ArmUp", arm, dbox), ("ArmDown", np.rot90(arm, 2), dbox),
         ("ArmLeft", np.rot90(arm, 1), dbox), ("ArmRight", np.rot90(arm, -1), dbox)]
for name, file in [("A", "switch_button_a.png"), ("B", "switch_button_b.png"), ("L", "switch_button_l.png"),
                   ("R", "switch_button_r.png"), ("Minus", "switch_button_minus.png"), ("Plus", "switch_button_plus.png"),
                   ("Home", "switch_button_home.png")]:
    a = load(file); ICONS.append((name, a, bbox(a)))


def fit(a, box, size):
    x0, y0, x1, y1 = box
    crop = a[y0:y1, x0:x1]
    w, h = x1 - x0, y1 - y0
    s = size / max(w, h)
    tw, th = max(1, round(w * s)), max(1, round(h * s))
    alpha = Image.fromarray((crop[..., 3] * 255).astype(np.uint8)).resize((tw, th), Image.LANCZOS)
    al = np.asarray(alpha).astype(np.float32) / 255.0
    if s > 1.2:                      # upscaled: sharpen the edges back to about a pixel
        al = np.clip((al - 0.5) * min(3.0, s * 0.75) + 0.5, 0, 1)
    out = np.zeros((th, tw, 4), np.float32); out[..., :3] = 1.0; out[..., 3] = al
    return out


rows = (len(ICONS) + COLUMNS - 1) // COLUMNS
for cell in CELLS:
    pad = cell // 16
    atlas = np.zeros((rows * cell, COLUMNS * cell, 4), np.float32)
    for i, (name, a, box) in enumerate(ICONS):
        img = fit(a, box, cell - 2 * pad)
        h, w = img.shape[:2]
        x = (i % COLUMNS) * cell + (cell - w) // 2
        y = (i // COLUMNS) * cell + (cell - h) // 2
        atlas[y:y + h, x:x + w] = img
    OUT.mkdir(parents=True, exist_ok=True)
    Image.fromarray((atlas * 255).round().astype(np.uint8), "RGBA").save(OUT / f"touch_atlas_{cell}.png", optimize=True)
    print("wrote", OUT / f"touch_atlas_{cell}.png", atlas.shape[1], "x", atlas.shape[0], [n for n, _, _ in ICONS])
