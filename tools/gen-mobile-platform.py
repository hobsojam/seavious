#!/usr/bin/env python3
"""Generates assets/sprites/mobile_platform.png (36x18) from an ASCII grid.

Mobile Platform per README roster: the heavy, less frequent surface
threat — a wide, flat barge that self-propels slowly across the water
(the one ground unit that moves, so its amber waterline hugs the hull
outline like a vessel instead of sitting as a detached ring), with a
raised rust deckhouse and three pale weapon emitters: one at the bow
tip, two spaced along the deck edges. Wake VFX trail from the stern at
runtime (code-driven, like the player's wake). Nose points left, the
direction it drifts.

The grid was drafted offline with a small rasterizer and curated by eye;
this script holds the baked result so the asset is exactly reproducible
(same convention as the ocean tile).

Run from anywhere:  python3 tools/gen-mobile-platform.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import genlib
import gridpng

PALETTE = {
    '.': (0, 0, 0, 0),          # transparent
    'o': (18, 14, 10, 255),     # outline dark warm
    'a': (232, 148, 44, 255),   # amber energy #e8942c (waterline)
    'h': (46, 38, 30, 255),     # hull dark warm
    's': (34, 28, 22, 255),     # hull shade (lower-right)
    'l': (110, 86, 56, 255),    # lit deck (upper-left)
    'm': (126, 78, 34, 255),    # rust deckhouse trim
    'c': (255, 200, 120, 255),  # weapon emitter pale amber
}

GRID = [
    "....................................",
    "....................................",
    "....................................",
    "...........aaaaaaaaaaaaaaaaaaaa.....",
    ".........aaocoooooooooooocoooooa....",
    "........aoolllllllllllllllllhhhoa...",
    "......aaollllllmmmmmmmmhhhhhhhhoaa..",
    "....aaoolllllllmllllllmhhhhhhhhoaa..",
    "...aaoclhhhhhhhmllllllmhhhhhhhhoaa..",
    "...aaochhhhhhhhmllllllmhhhhhhhhoaa..",
    "....aaoohhhhhhhmllllllmhhssssssoaa..",
    "......aaohhhhhhmmmmmmmmssssssssoaa..",
    "........aoohsssssssssssssssssssoa...",
    ".........aaocoooooooooooocoooooa....",
    "...........aaaaaaaaaaaaaaaaaaaa.....",
    "....................................",
    "....................................",
    "....................................",
]


def main(out_dir=None):
    out_dir = genlib.resolve_out_dir(out_dir, 'assets', 'sprites')
    gridpng.write_grid_png(36, 18, PALETTE, GRID,
                           os.path.join(out_dir, 'mobile_platform.png'))


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else None)
