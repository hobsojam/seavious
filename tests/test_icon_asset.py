#!/usr/bin/env python3
"""End-to-end test of tools/gen-exe-icon.py and the committed icon assets.

Runs the generator into a temp dir, validates the ICO container (entry
count, per-entry sizes, PNG-compressed blobs with matching IHDR
dimensions) and the 32x32 window PNG, then byte-compares both against
the committed assets/icon files so the generator and the checked-in
assets can't drift apart.

Runs standalone: python3 tests/test_icon_asset.py
"""
import importlib.util
import os
import struct
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSET_DIR = os.path.join(REPO, 'assets', 'icon')
EXPECTED_SIZES = (16, 32, 48, 256)

failures = []


def check(cond, msg):
    if not cond:
        failures.append(msg)
        print('FAIL:', msg)


def load_generator():
    path = os.path.join(REPO, 'tools', 'gen-exe-icon.py')
    spec = importlib.util.spec_from_file_location('gen_exe_icon', path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def validate_ico(data):
    reserved, ico_type, count = struct.unpack('<HHH', data[:6])
    check(reserved == 0 and ico_type == 1, f'bad ICONDIR header ({reserved}, {ico_type})')
    check(count == len(EXPECTED_SIZES), f'{count} entries, want {len(EXPECTED_SIZES)}')
    for i, expected in enumerate(EXPECTED_SIZES):
        entry = data[6 + 16 * i:6 + 16 * (i + 1)]
        w, h, _, _, planes, bpp, size, offset = struct.unpack('<BBBBHHII', entry)
        dir_size = w or 256
        check(dir_size == expected and (h or 256) == expected,
              f'entry {i}: directory says {dir_size}x{h or 256}, want {expected}')
        check(bpp == 32, f'entry {i}: bpp {bpp}, want 32')
        blob = data[offset:offset + size]
        check(blob[:8] == b'\x89PNG\r\n\x1a\n', f'entry {i}: not a PNG blob')
        png_w, png_h = struct.unpack('>II', blob[16:24])
        check(png_w == expected and png_h == expected,
              f'entry {i}: PNG is {png_w}x{png_h}, want {expected}')


def compare_committed(name, generated_path):
    committed_path = os.path.join(ASSET_DIR, name)
    check(os.path.exists(committed_path), f'{name}: missing from assets/icon')
    if not os.path.exists(committed_path):
        return
    with open(generated_path, 'rb') as f:
        generated = f.read()
    with open(committed_path, 'rb') as f:
        committed = f.read()
    check(committed == generated,
          f'{name}: committed asset differs from generator output '
          f'- rerun tools/gen-exe-icon.py and commit the result')


def main():
    gen = load_generator()
    with tempfile.TemporaryDirectory() as tmp:
        gen.main(tmp)

        ico_path = os.path.join(tmp, 'seavious.ico')
        check(os.path.exists(ico_path), 'generator did not write seavious.ico')
        if os.path.exists(ico_path):
            with open(ico_path, 'rb') as f:
                validate_ico(f.read())
            compare_committed('seavious.ico', ico_path)

        window_path = os.path.join(tmp, 'window_icon.png')
        check(os.path.exists(window_path), 'generator did not write window_icon.png')
        if os.path.exists(window_path):
            with open(window_path, 'rb') as f:
                blob = f.read()
            check(blob[:8] == b'\x89PNG\r\n\x1a\n', 'window_icon.png: not a PNG')
            png_w, png_h = struct.unpack('>II', blob[16:24])
            check(png_w == 32 and png_h == 32, f'window_icon.png is {png_w}x{png_h}, want 32x32')
            compare_committed('window_icon.png', window_path)

    if failures:
        print(f'\n{len(failures)} failure(s)')
        return 1
    print('OK: icon assets validated')
    return 0


if __name__ == '__main__':
    sys.exit(main())
