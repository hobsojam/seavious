#!/usr/bin/env python3
"""Extract the approved Fortress Atoll defense art into game-size sprites."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import genlib
import gridpng

SOURCE = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                      'assets', 'sprites', 'fortress_weapon_art_source.png')
SIZE = 30

# Square atlas cells, deliberately including a little matte border so the
# deliberately chunky native-pixel silhouettes retain breathing room.
CELLS = {
    'fortress_gun.png': (180, 150, 620),
    'fortress_mortar.png': (1010, 140, 600),
}


def build_cell(source, source_width, cell):
    left, top, span = cell
    pixels = []
    for y in range(SIZE):
        sy = top + y * span // SIZE
        for x in range(SIZE):
            sx = left + x * span // SIZE
            r, g, b, a = source[sy * source_width + sx]
            if r > 210 and g < 100 and b > 160:
                a = 0
            pixels.append((r, g, b, a))
    return pixels


def main(out_dir=None):
    out_dir = genlib.resolve_out_dir(out_dir, 'assets', 'sprites')
    width, _height, source = gridpng.read_png_rgba(SOURCE)
    for name, cell in CELLS.items():
        gridpng.write_rgba_png(SIZE, SIZE, build_cell(source, width, cell),
                               os.path.join(out_dir, name))


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else None)
