#!/usr/bin/env python3
"""End-to-end test of the Python sprite generators and their committed
PNG assets (Casemate, Tracking Turret).

Runs each generator into a temp dir, parses the PNG structurally (pure
stdlib: chunk layout, IHDR dimensions, IDAT decode), checks the pixel
content is meaningful (opaque hull pixels, amber family color present,
transparent corners), and byte-compares against the committed
assets/sprites files so the generators and assets can't drift apart.

Runs standalone: python3 tests/test_sprite_assets.py
"""
import importlib.util
import os
import struct
import sys
import tempfile
import zlib

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSET_DIR = os.path.join(REPO, 'assets', 'sprites')

AMBER = (232, 148, 44)      # ground family
MAGENTA = (216, 72, 192)    # air family
WHITE_HOT = (255, 246, 216) # explosion-center palette (boss core)
CYAN = (76, 224, 232)       # player faction (ship tech, title wordmark)
GREEN = (108, 224, 96)      # land family (Stage 2 installations)
# family color None = deliberately faction-less (the boss mortar: bare
# steel means "no weapon works on this", so both faction colors must be
# absent, not merely unrequired)

# generator script -> [(output file, width, height, family color), ...]
# A fifth tuple element names the assets/ subdirectory (default 'sprites').
GENERATORS = {
    'gen-casemate.py': [('casemate.png', 24, 24, AMBER)],
    'gen-islet-ridge.py': [('stage1_islet_ridge.png', 288, 128, None)],
    'gen-terrain-hardpoint.py': [
        ('terrain_island_hardpoint.png', 32, 32, GREEN, 'tiles')],
    'gen-tracking-turret.py': [('tracking_turret.png', 24, 24, AMBER)],
    'gen-mobile-platform.py': [('mobile_platform.png', 36, 18, AMBER)],
    'gen-gunship.py': [('gunship.png', 32, 24, MAGENTA)],
    'gen-title-logo.py': [('title_logo.png', 384, 68, CYAN)],
    'gen-mortar-battery.py': [('mortar_battery.png', 24, 24, GREEN)],
    'gen-drone-bunker.py': [('drone_bunker.png', 24, 24, GREEN)],
    'gen-leviathan.py': [
        ('leviathan_hull.png', 200, 120, AMBER),
        ('leviathan_pod.png', 20, 20, MAGENTA),
        ('leviathan_hull_section.png', 40, 24, AMBER),
        ('leviathan_mortar.png', 24, 24, None),
        ('leviathan_core.png', 32, 24, WHITE_HOT),
    ],
}

failures = []


def check(cond, msg):
    if not cond:
        failures.append(msg)
        print('FAIL:', msg)


def load_tool(script):
    path = os.path.join(REPO, 'tools', script)
    name = script[:-3].replace('-', '_')
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def parse_png(name, data):
    check(data[:8] == b'\x89PNG\r\n\x1a\n', f'{name}: bad PNG signature')
    pos = 8
    width = height = None
    idat = b''
    saw_end = False
    while pos < len(data):
        length, = struct.unpack('>I', data[pos:pos + 4])
        tag = data[pos + 4:pos + 8]
        payload = data[pos + 8:pos + 8 + length]
        crc, = struct.unpack('>I', data[pos + 8 + length:pos + 12 + length])
        check(crc == zlib.crc32(tag + payload) & 0xffffffff,
              f'{name}: bad CRC on {tag!r} chunk')
        if tag == b'IHDR':
            width, height, depth, color = struct.unpack('>IIBB', payload[:10])
            check(depth == 8 and color == 6,
                  f'{name}: expected 8-bit RGBA, got depth={depth} color={color}')
        elif tag == b'IDAT':
            idat += payload
        elif tag == b'IEND':
            saw_end = True
        pos += 12 + length
    check(saw_end, f'{name}: missing IEND chunk')
    raw = zlib.decompress(idat)
    stride = width * 4 + 1
    check(len(raw) == stride * height,
          f'{name}: IDAT decodes to {len(raw)} bytes, want {stride * height}')
    pixels = []
    for y in range(height):
        row = raw[y * stride:(y + 1) * stride]
        check(row[0] == 0, f'{name}: row {y} uses filter {row[0]}, want None')
        pixels.append([tuple(row[1 + x * 4:5 + x * 4]) for x in range(width)])
    return width, height, pixels


def validate_sprite(name, data, exp_w, exp_h, family_color):
    width, height, pixels = parse_png(name, data)
    check(width == exp_w and height == exp_h,
          f'{name}: {width}x{height}, want {exp_w}x{exp_h}')

    flat = [p for row in pixels for p in row]
    opaque = [p for p in flat if p[3] == 255]
    check(len(opaque) > len(flat) * 0.3,
          f'{name}: only {len(opaque)} opaque pixels - sprite mostly empty?')
    if family_color is None:
        for banned in (AMBER, MAGENTA):
            check(not any(p[:3] == banned for p in opaque),
                  f'{name}: faction-less sprite contains faction color {banned}')
    else:
        check(any(p[:3] == family_color for p in opaque),
              f'{name}: family color {family_color} missing')
    for cx, cy in ((0, 0), (exp_w - 1, 0), (0, exp_h - 1), (exp_w - 1, exp_h - 1)):
        check(pixels[cy][cx][3] == 0,
              f'{name}: corner ({cx},{cy}) not transparent')


def main():
    total = 0
    for script, outputs in GENERATORS.items():
        gen = load_tool(script)
        with tempfile.TemporaryDirectory() as tmp:
            gen.main(tmp)
            for output in outputs:
                out_name, exp_w, exp_h, family_color = output[:4]
                subdir = output[4] if len(output) > 4 else 'sprites'
                asset_dir = os.path.join(REPO, 'assets', subdir)
                total += 1
                path = os.path.join(tmp, out_name)
                check(os.path.exists(path), f'{script} did not write {out_name}')
                if not os.path.exists(path):
                    continue
                with open(path, 'rb') as f:
                    data = f.read()
                validate_sprite(out_name, data, exp_w, exp_h, family_color)

                committed_path = os.path.join(asset_dir, out_name)
                check(os.path.exists(committed_path),
                      f'{out_name}: missing from assets/{subdir}')
                if os.path.exists(committed_path):
                    with open(committed_path, 'rb') as f:
                        committed = f.read()
                    check(committed == data,
                          f'{out_name}: committed asset differs from generator '
                          f'output - rerun tools/{script} and commit the result')

    if failures:
        print(f'\n{len(failures)} failure(s)')
        return 1
    print(f'OK: {total} sprite assets validated')
    return 0


if __name__ == '__main__':
    sys.exit(main())
