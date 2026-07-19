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
SURF = (102, 224, 228)      # terrain-tile shoreline cyan
SAND = (220, 174, 70)       # terrain-tile beach ochre
GROUND = (139, 110, 56)     # terrain-ground transition base
# family color None = deliberately faction-less (the boss mortar: bare
# steel means "no weapon works on this", so both faction colors must be
# absent, not merely unrequired)

# generator script -> [(output file, width, height, family color), ...]
# A fifth tuple element names the assets/ subdirectory (default 'sprites').
GENERATORS = {
    'gen-casemate.py': [('casemate.png', 24, 24, AMBER)],
    'gen-islet-ridge.py': [('stage1_islet_ridge.png', 595, 276, None)],
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

# Hand-authored/generated terrain components have no deterministic generator,
# but still need structural checks: the runtime crops by alpha and a stray
# opaque magenta matte would turn an entire terrain group pink.
TERRAIN_COMPONENTS = {
    'stage1_islet_crescent.png': (1774, 887),
    'stage1_islet_atoll.png': (1448, 1086),
    'stage1_islet_long.png': (1774, 887),
}

TILE_GENERATORS = {
    'gen-terrain-tile-atlas.py': ('terrain_tile_atlas.png', 2304, 2304),
}

FEATURE_ASSETS = {
    'terrain_feature_atlas.png': (1024, 128),
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


def validate_terrain_component(name, data, exp_w, exp_h):
    width, height, pixels = parse_png(name, data)
    check((width, height) == (exp_w, exp_h),
          f'{name}: {width}x{height}, want {exp_w}x{exp_h}')
    flat = [p for row in pixels for p in row]
    opaque = [p for p in flat if p[3] == 255]
    check(len(opaque) > len(flat) * 0.15,
          f'{name}: only {len(opaque)} opaque pixels - terrain mostly empty?')
    check(not any(p[0] > 160 and p[1] < 160 and p[2] > 160
                  and p[0] + p[2] - 2 * p[1] > 120
                  for p in opaque),
          f'{name}: opaque chroma-key magenta remains')
    for cx, cy in ((0, 0), (width - 1, 0),
                   (0, height - 1), (width - 1, height - 1)):
        check(pixels[cy][cx][3] == 0,
              f'{name}: corner ({cx},{cy}) not transparent')


def validate_tile_atlas(name, data, exp_w, exp_h):
    width, height, pixels = parse_png(name, data)
    check((width, height) == (exp_w, exp_h),
          f'{name}: {width}x{height}, want {exp_w}x{exp_h}')
    flat = [p for row in pixels for p in row]
    opaque = [p for p in flat if p[3] == 255]
    check(len(opaque) > len(flat) * 0.40,
          f'{name}: only {len(opaque)} opaque pixels - atlas is unexpectedly sparse')
    check(any(p[:3] == SURF for p in opaque), f'{name}: surf color missing')
    check(any(p[:3] == SAND for p in opaque), f'{name}: beach color missing')
    ground_colors = {
        p[:3] for p in opaque
        if p[0] > p[1] and p[1] > p[2] and 50 < p[2] < 150
    }
    check(len(ground_colors) > 24,
          f'{name}: baked ground texture has too little colour variation')


def validate_wang_interfaces(gen):
    """Shared crossings must join through land, beach, surf, and water."""
    def profile(north, east, south, west):
        return north * 27 + east * 9 + south * 3 + west

    def band(pixel):
        if pixel[3] == 0:
            return 'water'
        if pixel in (gen.SURF, gen.FOAM, gen.WASH):
            return 'surf'
        if pixel in (gen.SAND_DARK, gen.SAND, gen.SAND_LIT):
            return 'beach'
        return 'land'

    horizontal_mask = gen.NW | gen.NE  # land above, water below
    vertical_mask = gen.NW | gen.SW    # land left, water right
    for level in range(3):
        left = gen.make_tile(horizontal_mask, profile(0, level, 2, 1))
        right = gen.make_tile(horizontal_mask, profile(2, 0, 1, level))
        for y in range(gen.TILE):
            check(band(left[y * gen.TILE + gen.TILE - 1])
                  == band(right[y * gen.TILE]),
                  f'Wang east/west seam differs at level {level}, y={y}')

        upper = gen.make_tile(vertical_mask, profile(0, 2, level, 1))
        lower = gen.make_tile(vertical_mask, profile(level, 1, 0, 2))
        for x in range(gen.TILE):
            check(band(upper[(gen.TILE - 1) * gen.TILE + x])
                  == band(lower[x]),
                  f'Wang north/south seam differs at level {level}, x={x}')

    silhouettes = {tuple(gen.make_tile(gen.NW, profile(level, level, level, level)))
                   for level in range(3)}
    check(len(silhouettes) == 3,
          'Wang interface levels do not produce distinct shoreline depths')

    full_land = gen.NW | gen.NE | gen.SE | gen.SW
    phases = {
        tuple(gen.make_tile(full_land, 0, phase_x, phase_y))
        for phase_y in range(gen.PHASES)
        for phase_x in range(gen.PHASES)
    }
    check(len(phases) == gen.PHASE_COUNT,
          'Wang ground phases do not produce four distinct texture quarters')


def validate_feature_atlas(name, data, exp_w, exp_h):
    width, height, pixels = parse_png(name, data)
    check((width, height) == (exp_w, exp_h),
          f'{name}: {width}x{height}, want {exp_w}x{exp_h}')
    flat = [p for row in pixels for p in row]
    opaque = [p for p in flat if p[3] == 255]
    check(len(opaque) > len(flat) * 0.01,
          f'{name}: only {len(opaque)} opaque pixels - feature atlas is unexpectedly sparse')
    transparent = [p for p in flat if p[3] == 0]
    check(len(transparent) > len(flat) * 0.45,
          f'{name}: metatile atlas lacks transparent negative space')
    check(any(p[0] > p[1] and p[1] > p[2] and p[0] > 80 for p in opaque),
          f'{name}: rock family missing')
    foliage = [p for p in opaque
               if p[1] >= p[0] * 0.78 and p[2] < p[1] * 0.65]
    check(len(foliage) > 150,
          f'{name}: substantial foliage clusters are missing')
    for tile in range(8):
        tile_opaque = sum(
            pixels[y][x][3] == 255
            for y in range(128)
            for x in range(tile * 128, tile * 128 + 128)
        )
        check(tile_opaque > 20,
              f'{name}: metatile {tile} has too little feature art')
        tile_foliage = sum(
            pixels[y][x][3] == 255
            and pixels[y][x][1] >= pixels[y][x][0] * 0.78
            and pixels[y][x][2] < pixels[y][x][1] * 0.65
            for y in range(128)
            for x in range(tile * 128, tile * 128 + 128)
        )
        if tile < 4:
            check(tile_foliage == 0,
                  f'{name}: mountain metatile {tile} contains foliage')
        else:
            check(tile_foliage > 20,
                  f'{name}: forest metatile {tile} lacks a readable foliage cluster')
        pad_is_clear = all(
            pixels[y][x][3] == 0
            for y in range(50, 78)
            for x in range(tile * 128 + 50, tile * 128 + 78)
        )
        check(pad_is_clear,
              f'{name}: metatile {tile} obstructs its hardpoint pad')


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

    for name, dimensions in TERRAIN_COMPONENTS.items():
        total += 1
        path = os.path.join(ASSET_DIR, name)
        check(os.path.exists(path), f'{name}: missing from assets/sprites')
        if os.path.exists(path):
            with open(path, 'rb') as f:
                validate_terrain_component(name, f.read(), *dimensions)

    for script, (name, exp_w, exp_h) in TILE_GENERATORS.items():
        total += 1
        gen = load_tool(script)
        with tempfile.TemporaryDirectory() as tmp:
            gen.main(tmp)
            path = os.path.join(tmp, name)
            check(os.path.exists(path), f'{script} did not write {name}')
            if not os.path.exists(path):
                continue
            with open(path, 'rb') as f:
                data = f.read()
            validate_tile_atlas(name, data, exp_w, exp_h)
            validate_wang_interfaces(gen)
            committed_path = os.path.join(REPO, 'assets', 'tiles', name)
            check(os.path.exists(committed_path), f'{name}: missing from assets/tiles')
            if os.path.exists(committed_path):
                with open(committed_path, 'rb') as f:
                    committed = f.read()
                check(committed == data,
                      f'{name}: committed asset differs from generator output '
                      f'- rerun tools/{script} and commit the result')

    # Feature metatiles are reviewed bitmap art rather than procedurally
    # authored sprites. Validate the committed native atlas directly; the
    # high-resolution source importer is intentionally a manual art tool.
    for name, (exp_w, exp_h) in FEATURE_ASSETS.items():
        total += 1
        path = os.path.join(REPO, 'assets', 'tiles', name)
        check(os.path.exists(path), f'{name}: missing from assets/tiles')
        if os.path.exists(path):
            with open(path, 'rb') as f:
                validate_feature_atlas(name, f.read(), exp_w, exp_h)

    if failures:
        print(f'\n{len(failures)} failure(s)')
        return 1
    print(f'OK: {total} sprite assets validated')
    return 0


if __name__ == '__main__':
    sys.exit(main())
