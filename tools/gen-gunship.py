#!/usr/bin/env python3
"""Generates assets/sprites/gunship.png (32x24) from an ASCII pixel grid.

Gunship per README roster: the heavy, less frequent air threat — a bulky
twin-hull frame like a small flying catamaran (echoing the naval theme
even airborne), noses left. Air-family language shared with the Skimmer
Drone and Interceptor: dark gunmetal-violet hulls, a magenta spine
stripe along each hull with a bright/dark split, and three pale weapon
emitters — one at each hull nose plus a 2x2 block on the connecting
deck's leading edge (the 3-way spread's center emitter).

The grid was drafted offline with a small rasterizer and curated by eye;
this script holds the baked result so the asset is exactly reproducible
(same convention as the ocean tile).

Run from anywhere:  python3 tools/gen-gunship.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import genlib
import gridpng

PALETTE = {
    '.': (0, 0, 0, 0),          # transparent
    'O': (20, 16, 32, 255),     # outline dark violet-navy
    'H': (58, 52, 72, 255),     # hull gunmetal violet
    'S': (42, 36, 56, 255),     # hull shade
    'M': (216, 72, 192, 255),   # magenta energy #d848c0
    'm': (150, 50, 134, 255),   # magenta energy, dark half
    'W': (255, 216, 248, 255),  # glowing core near-white pink
}

GRID = [
    "................................",
    "................................",
    "...........OOOOOOOOOOOOOOOO.....",
    ".........OOHHHHHHHHHHHHHHHHO....",
    "......OOOHHHHHHHHHHHHHHHHHHHO...",
    "....OOSSSSSSSSSSSSSSSSSSSSSSSO..",
    "...OWWSMMMMMMMMMMMMMmmmmmmmSSS..",
    "....OOSSSSSSSSSSSSSSSSSSSSSSSO..",
    "......OOOSSSSSSSSSSSSSSSSSSSO...",
    ".........OOSSSSSSSSSSSSSSSSO....",
    "...........OOOOMOOOOOOOOOOO.....",
    "............OWWMHHHHHHHHHHO.....",
    "............OWWMSSSSSSSSSSO.....",
    "............OSSMSSSSSSSSSSO.....",
    "..........OOOOOOOOOOOOOOOOO.....",
    "........OOHHHHHHHHHHHHHHHHHO....",
    ".....OOOHHHHHHHHHHHHHHHHHHHHO...",
    "...OOSSSSSSSSSSSSSSSSSSSSSSSSO..",
    "...OWWSMMMMMMMMMMMMMmmmmmmmSSO..",
    ".....OOOSSSSSSSSSSSSSSSSSSSSO...",
    "........OOSSSSSSSSSSSSSSSSSO....",
    "..........OOOOOOOOOOOOOOOOO.....",
    "................................",
    "................................",
]


def main(out_dir=None):
    out_dir = genlib.resolve_out_dir(out_dir, 'assets', 'sprites')
    gridpng.write_grid_png(32, 24, PALETTE, GRID,
                           os.path.join(out_dir, 'gunship.png'))


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else None)
