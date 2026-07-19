#!/usr/bin/env python3
"""Unit test for tools/gen-terrain-feature-atlas.py's pixel-processing logic.

The tool's only external dependency is the ffmpeg-based high-resolution art
import (`read_source`) - a thin, deliberately untested manual-import step,
the same convention test_sprite_assets.py documents for the sibling ground
importer. Everything downstream of that read (chroma removal, scrub-mass
filtering, quadrant sampling, metatile compositing, the atlas write) is
pure and deterministic, so it is exercised here against a small synthetic
source image instead of the real multi-megabyte art source.

Runs standalone: python3 tests/test_terrain_feature_atlas.py
"""
import importlib.util
import os
import struct
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

failures = []


def check(cond, msg):
    if not cond:
        failures.append(msg)
        print('FAIL:', msg)


def load_module():
    path = os.path.join(REPO, 'tools', 'gen-terrain-feature-atlas.py')
    spec = importlib.util.spec_from_file_location('gen_terrain_feature_atlas', path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def synthetic_source(mod):
    """A 64x64 stand-in for the 2x2 reviewed source grid: each quadrant
    gets a magenta chroma corner, a substantial scrub mass (kept), a
    scattering of single-pixel scrub specks (rejected), and a plain
    ground fill everywhere else."""
    width = height = 64
    magenta = (255, 0, 255, 255)
    scrub_green = (40, 180, 60, 255)
    ground = (150, 130, 90, 255)
    pixels = [ground] * (width * height)

    def put(x, y, color):
        pixels[y * width + x] = color

    for cell_row in range(2):
        for cell_col in range(2):
            base_x = cell_col * 32
            base_y = cell_row * 32
            for x in range(base_x, base_x + 4):
                for y in range(base_y, base_y + 4):
                    put(x, y, magenta)
            for x in range(base_x + 10, base_x + 16):
                for y in range(base_y + 10, base_y + 16):
                    put(x, y, scrub_green)
            put(base_x + 24, base_y + 24, scrub_green)

    return width, height, pixels


def read_png_size(data):
    check(data[:8] == b'\x89PNG\r\n\x1a\n', 'bad PNG signature')
    return struct.unpack('>II', data[16:24])


def main():
    mod = load_module()

    magenta = (255, 0, 255)
    ground = (150, 130, 90)
    scrub = (40, 180, 60)
    check(mod.is_chroma(magenta), 'magenta must read as chroma')
    check(not mod.is_chroma(ground), 'ground fill must not read as chroma')
    check(mod.is_scrub(scrub), 'scrub green must read as scrub')
    check(not mod.is_scrub(ground), 'ground fill must not read as scrub')

    source = synthetic_source(mod)
    sample = mod.sample_quadrant(source, 0, 0, 0)
    check(len(sample) == 4, 'sample_quadrant did not return an RGBA pixel')

    for forest in (False, True):
        metatile = mod.make_metatile(source, 0, forest)
        check(len(metatile) == mod.SIZE * mod.SIZE,
              f'make_metatile returned {len(metatile)} pixels')
        # The hardpoint pad stays clear regardless of family.
        check(metatile[64 * mod.SIZE + 64] == mod.TRANSPARENT,
              'hardpoint pad centre was not left transparent')
        # Every opaque pixel must have come from the ground/scrub fill,
        # never the chroma-keyed source colour.
        check(all(pixel[:3] != magenta for pixel in metatile if pixel[3]),
              'chroma colour leaked into a metatile')

    mod.read_source = lambda path: synthetic_source(mod)
    with tempfile.TemporaryDirectory() as out_dir:
        mod.main(out_dir)
        out_path = os.path.join(out_dir, 'terrain_feature_atlas.png')
        check(os.path.exists(out_path), 'terrain_feature_atlas.png was not written')
        with open(out_path, 'rb') as f:
            size = read_png_size(f.read())
        expected = (mod.SIZE * mod.COUNT, mod.SIZE)
        check(size == expected, f'atlas size {size} != {expected}')

    if failures:
        print(f'\n{len(failures)} failure(s)')
        return 1
    print('OK: terrain feature atlas helpers validated')
    return 0


if __name__ == '__main__':
    sys.exit(main())
