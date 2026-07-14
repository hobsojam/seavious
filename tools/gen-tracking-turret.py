#!/usr/bin/env python3
"""Generates assets/sprites/tracking_turret.png (24x24) from an ASCII grid.

Tracking Turret per README roster: a stationary circular mount whose
cannon rotates to lead the player (the barrel itself stays code-drawn so
it can aim). Top-down: the shared ground-family amber waterline ring, a
rust-brown circular deck lit from the upper-left, a bolt ring marking
the rotation bearing, and a dark pivot hub with a pale pin the code
barrel visually rotates around.

The grid was drafted offline with a small rasterizer and curated by eye;
this script holds the baked result so the asset is exactly reproducible
(same convention as the ocean tile).

Run from anywhere:  python3 tools/gen-tracking-turret.py
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
    'm': (126, 78, 34, 255),    # rust deck (matches the old code-drawn disc)
    'l': (158, 102, 48, 255),   # lit arc (upper-left)
    't': (168, 100, 24, 255),   # bearing bolt ring (rotation cue)
    'h': (46, 38, 30, 255),     # pivot hub dark warm
    'c': (255, 200, 120, 255),  # pivot pin pale amber
}

GRID = [
    "........................",
    "........oooooooo........",
    "......ooaaaaaaaaoo......",
    "....ooaaooooooooaaoo....",
    "...ooaoooollllooooaoo...",
    "...oaooolllmmllmoooao...",
    "..oaoollmmmmmmmmmmooao..",
    "..oaoolmmttmmttmmmooao..",
    ".oaoolmmtmmmmmmtmmmooao.",
    ".oaoolmtmmmmmmmmtmmooao.",
    ".oaollmtmmhhhhmmtmmmoao.",
    ".oaolmmmmmhcchmmmmmmoao.",
    ".oaolmmmmmhcchmmmmmmoao.",
    ".oaollmtmmhhhhmmtmmmoao.",
    ".oaoolmtmmmmmmmmtmmooao.",
    ".oaoommmtmmmmmmtmmmooao.",
    "..oaoommmttmmttmmmooao..",
    "..oaoommmmmmmmmmmmooao..",
    "...oaooommmmmmmmoooao...",
    "...ooaoooommmmooooaoo...",
    "....ooaaooooooooaaoo....",
    "......ooaaaaaaaaoo......",
    "........oooooooo........",
    "........................",
]


def main(out_dir=None):
    out_dir = genlib.resolve_out_dir(out_dir, 'assets', 'sprites')
    gridpng.write_grid_png(24, 24, PALETTE, GRID,
                           os.path.join(out_dir, 'tracking_turret.png'))


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else None)
