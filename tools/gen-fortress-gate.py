#!/usr/bin/env python3
"""Downsample the Fortress Atoll channel gate to its native footprint."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import genlib
import gridpng

SOURCE = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                      'assets', 'sprites', 'fortress_gate_source.png')
WIDTH = 10
HEIGHT = 28
CROP_LEFT = 290
CROP_TOP = 220
CROP_WIDTH = 360
CROP_HEIGHT = 1120


def build():
    source_width, _source_height, source = gridpng.read_png_rgba(SOURCE)
    pixels = []
    for y in range(HEIGHT):
        sy = CROP_TOP + y * CROP_HEIGHT // HEIGHT
        for x in range(WIDTH):
            sx = CROP_LEFT + x * CROP_WIDTH // WIDTH
            r, g, b, a = source[sy * source_width + sx]
            if r > 210 and g < 100 and b > 160:
                a = 0
            # Keep the sprite's bounding corners transparent even if the
            # nearest sample catches a one-pixel outline at the source edge.
            if (x == 0 or x == WIDTH - 1) and (y == 0 or y == HEIGHT - 1):
                a = 0
            pixels.append((r, g, b, a))
    return WIDTH, HEIGHT, pixels


def main(out_dir=None):
    out_dir = genlib.resolve_out_dir(out_dir, 'assets', 'sprites')
    width, height, pixels = build()
    gridpng.write_rgba_png(width, height, pixels,
                           os.path.join(out_dir, 'fortress_gate.png'))


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else None)
