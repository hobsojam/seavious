#!/usr/bin/env python3
"""Generates assets/tiles/terrain_island_hardpoint.png (32x32, top-down).

The terrain hardpoint pad - the mounting footing drawn under every land
installation (Mortar Battery, Drone Bunker). The previous asset was a
raw 1254x1254 magenta-keyed art export that scaled down to a flat green
square (playtest 2026-07-17: "one of the buildings is just a green
square"). This replaces it with a designed pad in the land-family
language: chamfered stone footing with paving grain, a dark mounting
socket ring at the center, and green corner brackets marking it as a
Stage 2 installation site.

Procedurally composed (same precedent as the title logo): deterministic
bytes, regenerate any time.

Run from anywhere:  python3 tools/gen-terrain-hardpoint.py [out_dir]
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import genlib
import gridpng

SIZE = 32
CHAMFER = 6  # x+y distance below which a corner cell is cut away

PALETTE = {
    '.': (0, 0, 0, 0),          # transparent (cut corners)
    'O': (14, 20, 16, 255),     # outline, near-black green-cast
    'S': (52, 56, 50, 255),     # stonework
    's': (38, 42, 36, 255),     # stonework shade
    'G': (108, 224, 96, 255),   # land-family green, lit
    'g': (56, 130, 52, 255),    # land-family green, dark half
    'M': (24, 28, 24, 255),     # mounting socket ring
}


def build_grid():
    grid = [['.'] * SIZE for _ in range(SIZE)]
    last = SIZE - 1

    def corner_distance(x, y):
        return min(x + y, (last - x) + y, x + (last - y), (last - x) + (last - y))

    for y in range(SIZE):
        for x in range(SIZE):
            corner = corner_distance(x, y)
            if corner < CHAMFER:
                continue  # chamfered corner stays water
            edge = min(x, y, last - x, last - y)
            if edge == 0 or corner == CHAMFER:
                grid[y][x] = 'O'
            else:
                # Coarse paving checker keeps the slab from reading flat.
                grid[y][x] = 'S' if (x // 4 + y // 4) % 2 == 0 else 's'

    # Central mounting socket: a dark ring where the installation sits,
    # slightly recessed stone inside it.
    c = last / 2.0
    for y in range(SIZE):
        for x in range(SIZE):
            if grid[y][x] not in ('S', 's'):
                continue
            r = math.hypot(x - c, y - c)
            if 8.5 <= r < 10.5:
                grid[y][x] = 'M'
            elif r < 8.5:
                grid[y][x] = 's'

    # Green corner brackets along the chamfer edges: the land-family
    # marker that says "installation site" even before anything spawns.
    for k in range(CHAMFER, CHAMFER + 4):
        for x, y in ((k, CHAMFER + 1), (CHAMFER + 1, k),
                     (last - k, CHAMFER + 1), (last - CHAMFER - 1, k),
                     (k, last - CHAMFER - 1), (CHAMFER + 1, last - k),
                     (last - k, last - CHAMFER - 1), (last - CHAMFER - 1, last - k)):
            if grid[y][x] != '.':
                grid[y][x] = 'G' if k < CHAMFER + 2 else 'g'

    return [''.join(row) for row in grid]


def main(out_dir=None):
    out_dir = genlib.resolve_out_dir(out_dir, 'assets', 'tiles')
    gridpng.write_grid_png(SIZE, SIZE, PALETTE, build_grid(),
                           os.path.join(out_dir, 'terrain_island_hardpoint.png'))


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else None)
