#!/usr/bin/env python3
"""Generates assets/sprites/stage1_islet_ridge.png - a long island ridge
(~2.3:1) composited from the painted islet art.

Chained island groups kept stamping near-square variants side by side,
so wide groups read as "two identical islands next to each other"
(playtest 2026-07-17). A first, banded-noise procedural ridge didn't
fit the painted aesthetic (same playtest, next round) - so this version
builds the wide landmass out of stage1_islet.png itself: three upright copies
(plain, mirrored, plain) overlap along the long axis with a slight
vertical stagger. Where copies overlap, the pixel that sits deeper
inside its own island wins, so the inner coastlines disappear and only
the true outer coast survives; a hash dither over the near-tie zone
hides the seams inside the painted grain. Every output pixel comes from
the painted source, so the style matches by construction.

Deterministic bytes, regenerate any time.

Run from anywhere:  python3 tools/gen-islet-ridge.py [out_dir]
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import genlib
import gridpng

SOURCE = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                      'assets', 'sprites', 'stage1_islet.png')

TILE_HEIGHT = 256      # working scale per copy (output stays crisp at the
                       # renderer's 128px draw height)
OVERLAP_FRAC = 0.42    # how deep neighboring copies merge
STAGGER = (12, 0, 20)  # per-copy vertical offsets: an organic waterline
DITHER_BAND = 0.08     # |d| tie zone where the seam dithers


def _hash01(x, y):
    h = (x * 374761393 + y * 668265263) & 0xffffffff
    h = ((h ^ (h >> 13)) * 1274126177) & 0xffffffff
    return ((h ^ (h >> 16)) & 0xffff) / 65536.0


def _crop_and_scale(width, height, pixels):
    """Crop to the alpha bounding box, then NN-scale to TILE_HEIGHT."""
    min_x, min_y, max_x, max_y = width, height, -1, -1
    for y in range(height):
        for x in range(width):
            if pixels[y * width + x][3] > 25:
                if x < min_x: min_x = x
                if x > max_x: max_x = x
                if y < min_y: min_y = y
                if y > max_y: max_y = y
    src_w, src_h = max_x - min_x + 1, max_y - min_y + 1
    out_h = TILE_HEIGHT
    out_w = round(src_w * out_h / src_h)
    tile = []
    for y in range(out_h):
        sy = min_y + y * src_h // out_h
        for x in range(out_w):
            sx = min_x + x * src_w // out_w
            tile.append(pixels[sy * width + sx])
    return out_w, out_h, tile


def build():
    src_w, src_h, src = gridpng.read_png_rgba(SOURCE)
    tile_w, tile_h, tile = _crop_and_scale(src_w, src_h, src)

    def sample(copy, x, y):
        """Copies alternate left/right only; terrain details stay upright."""
        if copy % 2 == 1:
            x = tile_w - 1 - x
        if 0 <= x < tile_w and 0 <= y < tile_h:
            return tile[y * tile_w + x]
        return (0, 0, 0, 0)

    step = tile_w - int(tile_w * OVERLAP_FRAC)
    placements = [(i * step, STAGGER[i]) for i in range(3)]
    out_w = placements[-1][0] + tile_w
    out_h = tile_h + max(STAGGER)

    # Interior-ness: normalized elliptical distance from each copy's own
    # center. The smaller d wins an overlap, which is exactly "the pixel
    # deeper inside its island" - inner coasts lose to the neighbor's
    # interior and vanish.
    def depth(x, y):
        nx = (x - (tile_w - 1) / 2.0) / (tile_w / 2.0)
        ny = (y - (tile_h - 1) / 2.0) / (tile_h / 2.0)
        return math.hypot(nx, ny)

    def candidates(x, y):
        """Opaque source pixels under (x, y) as (depth, pixel), sorted so
        the copy whose interior lies deepest under the point comes first."""
        found = []
        for copy, (ox, oy) in enumerate(placements):
            lx, ly = x - ox, y - oy
            if not (0 <= lx < tile_w and 0 <= ly < tile_h):
                continue
            px = sample(copy, lx, ly)
            if px[3] > 25:
                found.append((depth(lx, ly), px))
        found.sort(key=lambda entry: entry[0])
        return found

    def resolve(x, y):
        found = candidates(x, y)
        if not found:
            return (0, 0, 0, 0)
        best = found[0][1]
        # Near-ties dither between the two copies, hiding the seam
        # inside the painted grain.
        if len(found) > 1 and found[1][0] - found[0][0] < DITHER_BAND \
                and _hash01(x, y) < 0.5:
            best = found[1][1]
        # The sprite tests treat terrain as faction-less: nudge any
        # painted pixel that lands exactly on a faction color.
        if best[:3] in ((232, 148, 44), (216, 72, 192)):
            best = (best[0], best[1], best[2] ^ 1, best[3])
        return best

    pixels = [resolve(x, y) for y in range(out_h) for x in range(out_w)]
    return out_w, out_h, pixels


def main(out_dir=None):
    out_dir = genlib.resolve_out_dir(out_dir, 'assets', 'sprites')
    out_w, out_h, pixels = build()
    gridpng.write_rgba_png(out_w, out_h, pixels,
                           os.path.join(out_dir, 'stage1_islet_ridge.png'))


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else None)
