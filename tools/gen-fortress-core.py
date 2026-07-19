#!/usr/bin/env python3
"""Downsample the approved Fortress Atoll targeting-core sprite."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import genlib
import gridpng

SOURCE = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                      'assets', 'sprites', 'fortress_core_source.png')
SIZE = 32
# Centred source crop: the core itself fills the resulting native-size tile,
# while the matte border is kept transparent.
CROP_LEFT = 230
CROP_TOP = 210
CROP_SIZE = 800


def build():
    width, _height, source = gridpng.read_png_rgba(SOURCE)
    pixels = []
    for y in range(SIZE):
        sy = CROP_TOP + y * CROP_SIZE // SIZE
        for x in range(SIZE):
            sx = CROP_LEFT + x * CROP_SIZE // SIZE
            r, g, b, a = source[sy * width + sx]
            if r > 210 and g < 100 and b > 160:
                a = 0
            pixels.append((r, g, b, a))
    return SIZE, SIZE, pixels


def main(out_dir=None):
    out_dir = genlib.resolve_out_dir(out_dir, 'assets', 'sprites')
    width, height, pixels = build()
    gridpng.write_rgba_png(width, height, pixels,
                           os.path.join(out_dir, 'fortress_core.png'))


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else None)
