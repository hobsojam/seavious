#!/usr/bin/env python3
"""Generates assets/sprites/title_logo.png (384x68), the SEAVIOUS wordmark.

Title-screen logo per README "Title screen": the wordmark carries the
logo (arcade-correct — the lettering is the identity, the ship is only
an accent drawn live by the title screen code). Big chunky 5x7 blocky
capitals scaled 8x in the player-faction cyan, with a pale highlight
across the top row, and a wave cutting through the lower third of the
letters — foam line on top, deeper cyan below the waterline — making
the SEA in Seavious literal.

The wave uses an integer step table (one step per 8px column), not
trig, so the output is bit-exact everywhere and the cut reads as
chunky pixels rather than an anti-aliased curve.

Run from anywhere:  python3 tools/gen-title-logo.py [out_dir]
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import genlib
import gridpng

PALETTE = {
    '.': (0, 0, 0, 0),          # transparent
    'O': (12, 32, 52, 255),     # outline dark navy
    'C': (76, 224, 232, 255),   # player cyan fill (above the wave)
    'P': (232, 248, 248, 255),  # pale highlight, top letter row
    'F': (250, 252, 252, 255),  # foam line riding the wave cut
    'D': (40, 150, 168, 255),   # deep cyan below the waterline
    'S': (8, 14, 20, 150),      # soft drop shadow
}

# Blocky square-cornered 5x7 capitals: chunkier at 8x than rounded forms.
FONT = {
    'S': ['#####', '#....', '#....', '#####', '....#', '....#', '#####'],
    'E': ['#####', '#....', '#....', '####.', '#....', '#....', '#####'],
    'A': ['.###.', '#...#', '#...#', '#####', '#...#', '#...#', '#...#'],
    'V': ['#...#', '#...#', '#...#', '#...#', '#...#', '.#.#.', '..#..'],
    'I': ['#####', '..#..', '..#..', '..#..', '..#..', '..#..', '#####'],
    'O': ['#####', '#...#', '#...#', '#...#', '#...#', '#...#', '#####'],
    'U': ['#...#', '#...#', '#...#', '#...#', '#...#', '#...#', '#####'],
}

WORD = 'SEAVIOUS'
SCALE = 8
MARGIN_X = 4
MARGIN_TOP = 4
# 8 letters of 5 cols + 7 one-col gaps, scaled, plus side margins = 384.
WIDTH = MARGIN_X * 2 + (len(WORD) * 5 + (len(WORD) - 1)) * SCALE
HEIGHT = 68

# Waterline y per 8px column step; letters span y 4..59, so the wave cuts
# through their lower third.
WAVE = [0, 2, 3, 4, 3, 2, 0, -2, -3, -4, -3, -2]
WAVE_BASE_Y = 40


def build_mask():
    mask = [[False] * WIDTH for _ in range(HEIGHT)]
    x0 = MARGIN_X
    for ch in WORD:
        glyph = FONT[ch]
        for gy in range(7):
            for gx in range(5):
                if glyph[gy][gx] != '#':
                    continue
                for yy in range(SCALE):
                    for xx in range(SCALE):
                        mask[MARGIN_TOP + gy * SCALE + yy][x0 + gx * SCALE + xx] = True
        x0 += 6 * SCALE
    return mask


def near_mask(mask, x, y, distance):
    for dy in range(-distance, distance + 1):
        for dx in range(-distance, distance + 1):
            ny, nx = y + dy, x + dx
            if 0 <= ny < HEIGHT and 0 <= nx < WIDTH and mask[ny][nx]:
                return True
    return False


def build_grid():
    mask = build_mask()
    grid = [['.'] * WIDTH for _ in range(HEIGHT)]
    for y in range(HEIGHT):
        for x in range(WIDTH):
            if mask[y][x]:
                wave_y = WAVE_BASE_Y + WAVE[(x // 8) % len(WAVE)]
                if y < MARGIN_TOP + SCALE:
                    grid[y][x] = 'P'
                elif wave_y - 1 <= y <= wave_y:
                    grid[y][x] = 'F'
                elif y > wave_y:
                    grid[y][x] = 'D'
                else:
                    grid[y][x] = 'C'
            elif near_mask(mask, x, y, 2):
                grid[y][x] = 'O'
            elif 0 <= y - 3 < HEIGHT and 0 <= x - 3 < WIDTH and mask[y - 3][x - 3]:
                grid[y][x] = 'S'
    return [''.join(row) for row in grid]


def main(out_dir=None):
    out_dir = genlib.resolve_out_dir(out_dir, 'assets', 'sprites')
    gridpng.write_grid_png(WIDTH, HEIGHT, PALETTE, build_grid(),
                           os.path.join(out_dir, 'title_logo.png'))


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else None)
