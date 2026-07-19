#!/usr/bin/env python3
"""Unit test for tools/gen-terrain-ground-tile.py's edge-reconciliation logic.

main() is a thin, ffmpeg-dependent manual art import (the same deliberately
untested convention documented in test_sprite_assets.py for this generator);
`lerp` and `reconcile_edges` are the pure, deterministic pixel math and are
exercised here directly against a small synthetic tile.

Runs standalone: python3 tests/test_terrain_ground_tile.py
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
    path = os.path.join(REPO, 'tools', 'gen-terrain-ground-tile.py')
    spec = importlib.util.spec_from_file_location('gen_terrain_ground_tile', path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def main():
    mod = load_module()

    check(mod.lerp(0, 10, 0.5) == 5, 'lerp midpoint is wrong')
    check(mod.lerp(0, 10, 0.0) == 0, 'lerp at amount=0 must return a')
    check(mod.lerp(0, 10, 1.0) == 10, 'lerp at amount=1 must return b')

    size = mod.SIZE
    # A left edge of one colour, a right edge of another: reconciliation
    # must blend them toward the same shared value on both sides so the
    # tile repeats without a visible seam, and must not touch the interior.
    left = (200, 40, 40, 255)
    right = (40, 40, 200, 255)
    interior = (10, 200, 10, 255)
    pixels = []
    for y in range(size):
        row = [interior] * size
        row[0] = left
        row[size - 1] = right
        pixels.extend(row)
    reconciled = mod.reconcile_edges(pixels)
    check(len(reconciled) == size * size, 'reconcile_edges changed pixel count')
    row0_left = reconciled[0]
    row0_right = reconciled[size - 1]
    check(row0_left == row0_right,
          f'left/right edge did not converge: {row0_left} vs {row0_right}')
    check(reconciled[size + 5] == interior, 'reconcile_edges touched the interior')

    col_top = reconciled[0]
    col_bottom = reconciled[(size - 1) * size]
    check(col_top == col_bottom,
          f'top/bottom edge did not converge: {col_top} vs {col_bottom}')

    if failures:
        print(f'\n{len(failures)} failure(s)')
        return 1
    print('OK: terrain ground tile helpers validated')
    return 0


if __name__ == '__main__':
    sys.exit(main())
