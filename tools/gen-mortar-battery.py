#!/usr/bin/env python3
"""Generates assets/sprites/mortar_battery.png (24x24, top-down).

Mortar Battery per README Stage 2 design: the land family's flagship —
the enemy counterpart of the player's scavenged mortar. Round stone
emplacement with the land-family green pad ring at its edge, a steel
dome echoing the Leviathan's mortar turret, a dark mortar mouth, and a
pale armed-core pixel at the center (the shared "this shoots" mark).
Land family language: green accents on dark stonework — installations
built on terrain, distinct from the amber water platforms.

Procedurally composed from distance fields (same precedent as the title
logo): deterministic bytes, regenerate any time.

Run from anywhere:  python3 tools/gen-mortar-battery.py [out_dir]
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import genlib
import gridpng

SIZE = 24

PALETTE = {
    '.': (0, 0, 0, 0),          # transparent
    'O': (14, 20, 16, 255),     # outline, near-black green-cast
    'S': (52, 56, 50, 255),     # stonework
    's': (38, 42, 36, 255),     # stonework shade
    'G': (108, 224, 96, 255),   # land-family green, lit
    'g': (56, 130, 52, 255),    # land-family green, dark half
    'D': (122, 128, 124, 255),  # dome steel, lit
    'd': (86, 92, 88, 255),     # dome steel, shade
    'M': (18, 22, 20, 255),     # mortar mouth
    'W': (232, 248, 240, 255),  # pale armed core
}


def build_grid():
    grid = [['.'] * SIZE for _ in range(SIZE)]
    cx = cy = (SIZE - 1) / 2.0
    for y in range(SIZE):
        for x in range(SIZE):
            dx, dy = x - cx, y - cy
            r2 = dx * dx + dy * dy
            lit = dx + dy < 0  # light from the upper-left
            if r2 <= 1.4:
                grid[y][x] = 'W'
            elif r2 <= 7.5:
                grid[y][x] = 'M'
            elif r2 <= 42.0:
                grid[y][x] = 'D' if lit else 'd'
            elif r2 <= 52.0:
                grid[y][x] = 'O'
            elif r2 <= 79.0:
                grid[y][x] = 'S' if lit else 's'
            elif r2 <= 100.0:
                grid[y][x] = 'G' if lit else 'g'
            elif r2 <= 121.0:
                grid[y][x] = 'O'
    return [''.join(row) for row in grid]


def main(out_dir=None):
    out_dir = genlib.resolve_out_dir(out_dir, 'assets', 'sprites')
    gridpng.write_grid_png(SIZE, SIZE, PALETTE, build_grid(),
                           os.path.join(out_dir, 'mortar_battery.png'))


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else None)
