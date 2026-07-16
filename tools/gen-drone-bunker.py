#!/usr/bin/env python3
"""Generates assets/sprites/drone_bunker.png (24x24, top-down).

Drone Bunker per README Stage 2 design: the Relay Node's land cousin —
a low blockhouse hatching Skimmer Drones from behind shorelines. Land
family language (green accents on dark stonework): rectangular stone
blockhouse, a dark central hatch rimmed in green (where the drones
emerge), and a green marker stripe along the downstream edge.

Procedurally composed (same precedent as the title logo): deterministic
bytes, regenerate any time.

Run from anywhere:  python3 tools/gen-drone-bunker.py [out_dir]
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import genlib
import gridpng

SIZE = 24

PALETTE = {
    '.': (0, 0, 0, 0),          # transparent
    'O': (14, 20, 16, 255),     # outline
    'S': (52, 56, 50, 255),     # stonework
    's': (38, 42, 36, 255),     # stonework shade
    'G': (108, 224, 96, 255),   # land-family green, lit
    'g': (56, 130, 52, 255),    # land-family green, dark half
    'M': (18, 22, 20, 255),     # hatch opening
}


def build_grid():
    grid = [['.'] * SIZE for _ in range(SIZE)]
    left, right, top, bottom = 2, 21, 5, 19
    for y in range(top, bottom + 1):
        for x in range(left, right + 1):
            edge = x in (left, right) or y in (top, bottom)
            if edge:
                grid[y][x] = 'O'
            elif y <= top + 2 or x <= left + 2:
                grid[y][x] = 'S'  # lit roof edge (light from upper-left)
            else:
                grid[y][x] = 's'
    # Hatch: dark opening with a green rim, offset toward the launch side.
    for y in range(8, 15):
        for x in range(8, 16):
            rim = x in (8, 15) or y in (8, 14)
            grid[y][x] = ('G' if y < 11 or x < 10 else 'g') if rim else 'M'
    # Downstream marker stripe along the lower face.
    for x in range(left + 2, right - 1):
        grid[17][x] = 'g' if x % 4 else 'G'
    return [''.join(row) for row in grid]


def main(out_dir=None):
    out_dir = genlib.resolve_out_dir(out_dir, 'assets', 'sprites')
    gridpng.write_grid_png(SIZE, SIZE, PALETTE, build_grid(),
                           os.path.join(out_dir, 'drone_bunker.png'))


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else None)
