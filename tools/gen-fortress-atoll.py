#!/usr/bin/env python3
"""Build the Stage 2 Fortress Atoll base from the approved round layout.

The source has a flat magenta matte, so this stdlib-only generator removes it
deterministically. The runtime gets a compact transparent sprite; live
defenses and the sea gate remain renderer overlays for readable fight state.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import genlib
import gridpng

SOURCE = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                      'assets', 'sprites', 'fortress_atoll_source.png')
WIDTH = 192
HEIGHT = 192


def build():
    width, height, source = gridpng.read_png_rgba(SOURCE)
    pixels = []
    for y in range(HEIGHT):
        sy = y * height // HEIGHT
        for x in range(WIDTH):
            r, g, b, a = source[sy * width + x * width // WIDTH]
            if r > 210 and g < 100 and b > 160:
                a = 0
            pixels.append((r, g, b, a))
    return WIDTH, HEIGHT, pixels


def main(out_dir=None):
    out_dir = genlib.resolve_out_dir(out_dir, 'assets', 'sprites')
    width, height, pixels = build()
    gridpng.write_rgba_png(width, height, pixels,
                           os.path.join(out_dir, 'fortress_atoll.png'))


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else None)
