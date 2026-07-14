#!/usr/bin/env python3
"""Generates assets/sprites/casemate.png (24x24) from an ASCII pixel grid.

Casemate per README roster: the baseline ground threat — a low hexagonal
armored plate breaching the surface, fixed cannon firing straight left
across its row. Top-down: the shared ground-family amber waterline glow
ring (same ring the Relay Node carries), a dark warm hex plate lit from
the upper-left, the fixed barrel crossing the waterline to the left with
a pale-amber muzzle, and a pale emitter core at the hub.

The grid was drafted offline with a small rasterizer and curated by eye;
this script holds the baked result so the asset is exactly reproducible
(same convention as the ocean tile).

Run from anywhere:  python3 tools/gen-casemate.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import genlib
import gridpng

PALETTE = {
    '.': (0, 0, 0, 0),          # transparent
    'o': (18, 14, 10, 255),     # outline dark warm
    'a': (232, 148, 44, 255),   # amber energy #e8942c
    'h': (46, 38, 30, 255),     # hull dark warm
    's': (34, 28, 22, 255),     # hull shade (lower-right)
    'l': (110, 86, 56, 255),    # lit facet (upper-left)
    'b': (70, 58, 46, 255),     # barrel metal
    'c': (255, 200, 120, 255),  # muzzle / emitter core pale amber
}

GRID = [
    "........................",
    "........oooooooo........",
    "......ooaaaaaaaaoo......",
    "....ooaaooo..oooaaoo....",
    "...ooaoo........ooaoo...",
    "...oao............oao...",
    "..oao...oooooooo...oao..",
    "..oao..ollllhhhho..oao..",
    ".oao..ollllhhhhhho..oao.",
    ".oao.ollllhhhhhhhho.oao.",
    ".oaoollllhhhhhhhhhsooao.",
    "ccbbbbllhhhcchhhhsso.ao.",
    "ccbbbblhhhhcchhsssso.ao.",
    ".oaoolhhhhhhhhhssssooao.",
    ".oao.ohhhhhhhhsssso.oao.",
    ".oao..ohhhhhhsssso..oao.",
    "..oao..ohhhhsssso..oao..",
    "..oao...oooooooo...oao..",
    "...oao............oao...",
    "...ooaoo........ooaoo...",
    "....ooaaooo..oooaaoo....",
    "......ooaaaaaaaaoo......",
    "........oooooooo........",
    "........................",
]


def main(out_dir=None):
    out_dir = genlib.resolve_out_dir(out_dir, 'assets', 'sprites')
    gridpng.write_grid_png(24, 24, PALETTE, GRID,
                           os.path.join(out_dir, 'casemate.png'))


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else None)
