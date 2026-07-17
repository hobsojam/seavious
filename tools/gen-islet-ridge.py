#!/usr/bin/env python3
"""Generates assets/sprites/stage1_islet_ridge.png (288x128, top-down).

A long, low island ridge (~2.25:1) for the islet variant set: chained
island groups kept stamping near-square variants side by side, so wide
groups read as "two identical islands next to each other" (playtest
2026-07-17). One genuinely elongated landmass lets the picker cover a
wide segment with a single shape, and alternates with the crescent on
longer chains.

Palette sampled from the existing islet art set (dominant interior
olives, dry/damp sand ring, dark wet coast edge) so the generated
ridge sits in the same family as the painted variants.

Procedurally composed (same precedent as the title logo): an elliptical
coastline perturbed by low-order angular waves, banded into coast, sand
and scrub, with hash-dithered grain and a darker crest along the spine.
Deterministic bytes, regenerate any time.

Run from anywhere:  python3 tools/gen-islet-ridge.py [out_dir]
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import genlib
import gridpng

WIDTH, HEIGHT = 288, 128

PALETTE = {
    '.': (0, 0, 0, 0),           # transparent water
    'O': (80, 64, 48, 255),      # wet coast edge
    'S': (240, 208, 96, 255),    # dry sand
    's': (224, 192, 80, 255),    # damp sand
    'I': (160, 128, 48, 255),    # interior scrub
    'i': (144, 128, 48, 255),    # interior shade
    'R': (176, 144, 64, 255),    # ridge crest highlight
    'D': (64, 52, 40, 255),      # dark speckle (rocks, brush shadow)
}


def _hash01(x, y):
    """Deterministic per-pixel noise in [0,1) - no random module, so the
    output bytes never depend on interpreter version."""
    h = (x * 374761393 + y * 668265263) & 0xffffffff
    h = ((h ^ (h >> 13)) * 1274126177) & 0xffffffff
    return ((h ^ (h >> 16)) & 0xffff) / 65536.0


def build_grid():
    rows = []
    cx, cy = (WIDTH - 1) / 2.0, (HEIGHT - 1) / 2.0
    rx, ry = WIDTH / 2.0 - 3.0, HEIGHT / 2.0 - 3.0
    for y in range(HEIGHT):
        row = []
        for x in range(WIDTH):
            nx, ny = (x - cx) / rx, (y - cy) / ry
            theta = math.atan2(ny, nx)
            # Low-order waves keep the coast organic without breaking the
            # single-landmass silhouette.
            edge = (1.0 + 0.11 * math.sin(3.0 * theta + 1.7)
                    + 0.07 * math.sin(7.0 * theta + 0.4)
                    + 0.045 * math.sin(13.0 * theta + 3.1))
            d = math.hypot(nx, ny) / edge
            n = _hash01(x, y)
            if d > 1.0:
                ch = '.'
            elif d > 0.94:
                ch = 'O'
            elif d > 0.78:
                ch = 'S' if n > 0.35 else 's'
            elif d > 0.72:
                ch = 's' if n > 0.5 else 'I'
            else:
                ch = 'I'
                if n > 0.86:
                    ch = 'i'
                elif n < 0.05:
                    ch = 'D'
                # The crest: a broken highlight band along the long axis
                # gives the ridge its name and a hint of elevation.
                if abs(ny) < 0.20 and d < 0.62 and 0.25 < n < 0.45:
                    ch = 'R'
            row.append(ch)
        rows.append(''.join(row))
    return rows


def main(out_dir=None):
    out_dir = genlib.resolve_out_dir(out_dir, 'assets', 'sprites')
    gridpng.write_grid_png(WIDTH, HEIGHT, PALETTE, build_grid(),
                           os.path.join(out_dir, 'stage1_islet_ridge.png'))


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else None)
