#!/usr/bin/env python3
"""Unit test for tools/pngread.py, the stdlib PNG decoder the terrain art
importers use in place of an image-library dependency.

Runs standalone: python3 tests/test_pngread.py
"""
import importlib.util
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

failures = []


def check(cond, msg):
    if not cond:
        failures.append(msg)
        print('FAIL:', msg)


def load_module():
    path = os.path.join(REPO, 'tools', 'pngread.py')
    spec = importlib.util.spec_from_file_location('pngread', path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def main():
    pngread = load_module()

    # A committed native-resolution PNG exercises both the fast IHDR-only
    # size read and the full RGBA decode against the same known dimensions.
    path = os.path.join(REPO, 'assets', 'tiles', 'terrain_ground_tile.png')
    width, height = pngread.read_size(path)
    check((width, height) == (64, 64), f'read_size returned {(width, height)}')

    full_width, full_height, pixels = pngread.read_rgba(path)
    check((full_width, full_height) == (width, height),
          'read_rgba dimensions disagree with read_size')
    check(len(pixels) == full_width * full_height,
          f'read_rgba returned {len(pixels)} pixels, expected {full_width * full_height}')
    check(all(len(pixel) == 4 for pixel in pixels[:16]),
          'read_rgba pixels are not RGBA tuples')

    if failures:
        print(f'\n{len(failures)} failure(s)')
        return 1
    print('OK: pngread validated')
    return 0


if __name__ == '__main__':
    sys.exit(main())
