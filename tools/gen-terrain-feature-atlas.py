#!/usr/bin/env python3
"""Generate four sparse 64px terrain feature stamps for tiled land.

The shoreline atlas owns only continuous ground and coast. These transparent
overlays supply the larger rock/scrub clusters that make an island feel like
a place rather than a patterned floor. Run from anywhere:
python3 tools/gen-terrain-feature-atlas.py [out_dir]
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import genlib
import gridpng

SIZE = 64
COUNT = 4
TRANSPARENT = (0, 0, 0, 0)
ROCK_DARK = (60, 50, 42, 255)
ROCK = (109, 85, 64, 255)
ROCK_LIT = (157, 124, 88, 255)
SCRUB_DARK = (42, 76, 29, 255)
SCRUB = (64, 109, 37, 255)
SCRUB_LIT = (91, 139, 45, 255)
STONE = (184, 151, 96, 255)


def paint_boulder(pixels, cx, cy, width, height):
    """A stepped, faceted boulder - deliberately not a round icon."""
    profile = (0.35, 0.70, 1.0, 0.95, 0.78, 0.55, 0.25)
    for row, fraction in enumerate(profile):
        y = cy - height // 2 + row
        half = max(1, int(width * fraction / 2))
        for x in range(cx - half, cx + half + 1):
            if not (0 <= x < SIZE and 0 <= y < SIZE):
                continue
            if (x + y * 3) % 9 == 0 and row not in (2, 3):
                continue
            shade = ROCK
            if row <= 1 or x < cx - width // 5:
                shade = ROCK_DARK
            elif row >= 4 or x > cx + width // 5:
                shade = ROCK_LIT
            pixels[y * SIZE + x] = shade


def paint_scrub(pixels, cx, cy):
    # Low, overlapping leaf clumps with a dark underside; their uneven
    # outline reads as vegetation at game scale, unlike smooth green disks.
    for ox, oy, radius in ((-5, 1, 3), (-1, -2, 4), (4, 1, 3), (1, 4, 3)):
        for y in range(cy + oy - radius, cy + oy + radius + 1):
            for x in range(cx + ox - radius, cx + ox + radius + 1):
                if not (0 <= x < SIZE and 0 <= y < SIZE):
                    continue
                if abs(x - (cx + ox)) + abs(y - (cy + oy)) > radius + 1:
                    continue
                shade = SCRUB
                if y >= cy + oy + 1 or x < cx + ox - 1:
                    shade = SCRUB_DARK
                elif (x + y) % 5 == 0:
                    shade = SCRUB_LIT
                pixels[y * SIZE + x] = shade


def make_feature(index):
    pixels = [TRANSPARENT] * (SIZE * SIZE)
    if index == 0:
        paint_boulder(pixels, 27, 35, 10, 7)
        paint_boulder(pixels, 37, 30, 13, 9)
        paint_boulder(pixels, 45, 38, 9, 6)
    elif index == 1:
        paint_scrub(pixels, 32, 32)
        paint_scrub(pixels, 43, 39)
    elif index == 2:
        paint_scrub(pixels, 27, 35)
        paint_boulder(pixels, 41, 29, 12, 8)
    else:
        paint_boulder(pixels, 30, 37, 11, 7)
        paint_scrub(pixels, 42, 30)
    # A few loose pale stones bridge the clusters without becoming a grid.
    for x, y in ((17, 45), (29, 16), (43, 48), (52, 24)):
        x = (x + index * 7) % 56 + 4
        y = (y + index * 11) % 56 + 4
        pixels[y * SIZE + x] = STONE
        pixels[y * SIZE + x + 1] = ROCK_LIT
    return pixels


def main(out_dir=None):
    out_dir = genlib.resolve_out_dir(out_dir, 'assets', 'tiles')
    pixels = [TRANSPARENT] * (SIZE * COUNT * SIZE)
    for index in range(COUNT):
        feature = make_feature(index)
        for y in range(SIZE):
            for x in range(SIZE):
                pixels[y * SIZE * COUNT + index * SIZE + x] = feature[y * SIZE + x]
    gridpng.write_rgba_png(SIZE * COUNT, SIZE, pixels,
                           os.path.join(out_dir, 'terrain_feature_atlas.png'))


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else None)
