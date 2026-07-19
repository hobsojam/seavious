#!/usr/bin/env python3
"""Create the small seamless ground texture baked into Stage 2 Wang tiles.

The reviewed terrain_ground.png is intentionally kept as the art source. This
manual importer reduces it to native pixel scale, then softly reconciles the
opposite edges so the 64px result can repeat without a visible join.
"""
import os
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import genlib
import gridpng
import pngread

SIZE = 64
BLEND = 1


def lerp(a, b, amount):
    return int(round(a + (b - a) * amount))


def reconcile_edges(pixels):
    pixels = list(pixels)
    for y in range(SIZE):
        left = pixels[y * SIZE]
        right = pixels[y * SIZE + SIZE - 1]
        shared = tuple((left[c] + right[c]) // 2 for c in range(4))
        for depth in range(BLEND):
            keep = depth / BLEND
            for x in (depth, SIZE - 1 - depth):
                original = pixels[y * SIZE + x]
                pixels[y * SIZE + x] = tuple(
                    lerp(shared[c], original[c], keep) for c in range(4))

    for x in range(SIZE):
        top = pixels[x]
        bottom = pixels[(SIZE - 1) * SIZE + x]
        shared = tuple((top[c] + bottom[c]) // 2 for c in range(4))
        for depth in range(BLEND):
            keep = depth / BLEND
            for y in (depth, SIZE - 1 - depth):
                original = pixels[y * SIZE + x]
                pixels[y * SIZE + x] = tuple(
                    lerp(shared[c], original[c], keep) for c in range(4))
    return pixels


def main(out_dir=None):
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    source = os.path.join(root, 'assets', 'tiles', 'terrain_ground.png')
    out_dir = genlib.resolve_out_dir(out_dir, 'assets', 'tiles')
    destination = os.path.join(out_dir, 'terrain_ground_tile.png')
    ffmpeg = shutil.which('ffmpeg')
    if not ffmpeg:
        raise RuntimeError('ffmpeg is required for this manual high-resolution art import')

    with tempfile.TemporaryDirectory() as temporary:
        reduced = os.path.join(temporary, 'terrain-ground-64.png')
        subprocess.run([
            ffmpeg, '-loglevel', 'error', '-y', '-i', source,
            '-vf', f'scale={SIZE}:{SIZE}:flags=neighbor', reduced,
        ], check=True)
        width, height, pixels = pngread.read_rgba(reduced)
    if (width, height) != (SIZE, SIZE):
        raise ValueError(f'reduced ground is {width}x{height}, expected {SIZE}x{SIZE}')
    gridpng.write_rgba_png(SIZE, SIZE, reconcile_edges(pixels), destination)


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else None)
