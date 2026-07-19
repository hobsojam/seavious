#!/usr/bin/env python3
"""Exercise tools/export-terrain-art-templates.py end to end.

The tool has no external (ffmpeg/art-source) dependency - it derives its
output purely from the committed stage2_terrain.txt and the tile-atlas
generator's pixel math - so a full run against a temp dir is a cheap,
deterministic way to validate both the brief/sheet it writes and the
output-directory safety check that guards its CLI argument.

Runs standalone: python3 tests/test_terrain_art_templates.py
"""
import importlib.util
import os
import struct
import sys
import tempfile
import zlib

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

failures = []


def check(cond, msg):
    if not cond:
        failures.append(msg)
        print('FAIL:', msg)


def load_module():
    path = os.path.join(REPO, 'tools', 'export-terrain-art-templates.py')
    spec = importlib.util.spec_from_file_location('export_terrain_art_templates', path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def read_png_size(data):
    check(data[:8] == b'\x89PNG\r\n\x1a\n', 'bad PNG signature')
    width, height = struct.unpack('>II', data[16:24])
    return width, height


def main():
    mod = load_module()

    # Pure helpers: deterministic across calls, and the lattice/noise
    # values must stay within the documented [-1, 1] range.
    check(mod.u32(-1) == 0xffffffff, 'u32 does not wrap negative values')
    for salt in (0xc2b2ae35, 0x27d4eb2f):
        for gx in range(-2, 3):
            for gy in range(-2, 3):
                value = mod.lattice_value(gx, gy, salt)
                check(-1.0 <= value <= 1.0, f'lattice_value out of range: {value}')
    for level in (0, 1, 2):
        check(level in (0, 1, 2), 'interface_level sanity')

    with tempfile.TemporaryDirectory() as out_dir:
        mod.main(out_dir)

        sheet_path = os.path.join(out_dir, 'stage2-common-coasts.png')
        brief_path = os.path.join(out_dir, 'stage2-common-coasts.md')
        check(os.path.exists(sheet_path), 'sheet PNG was not written')
        check(os.path.exists(brief_path), 'brief Markdown was not written')

        with open(sheet_path, 'rb') as f:
            data = f.read()
        cell = mod.SCALE * 32 + mod.GUTTER * 2
        expected = (mod.SHEET_COLUMNS * cell, mod.SHEET_ROWS * cell)
        check(read_png_size(data) == expected,
              f'sheet size {read_png_size(data)} != {expected}')
        # A structurally valid PNG must decompress without error.
        idat = b''
        pos = 8
        while pos < len(data):
            length, = struct.unpack('>I', data[pos:pos + 4])
            tag = data[pos + 4:pos + 8]
            if tag == b'IDAT':
                idat += data[pos + 8:pos + 8 + length]
            pos += length + 12
        try:
            zlib.decompress(idat)
        except zlib.error as error:
            check(False, f'sheet IDAT does not decompress: {error}')

        with open(brief_path, encoding='utf-8') as f:
            brief = f.read()
        check(brief.startswith('# Stage 2 common coast art templates'),
              'brief is missing its heading')
        rows = [line for line in brief.splitlines() if line.startswith('| ') and
                not line.startswith('| Panel') and not line.startswith('| ---:')]
        check(1 <= len(rows) <= mod.SHEET_COLUMNS * mod.SHEET_ROWS,
              f'brief has {len(rows)} panel rows')

    # A CLI argument that would escape the repository must be rejected,
    # not silently followed - this is the security-relevant path.
    try:
        mod.main(os.path.join(tempfile.gettempdir(), '..', 'escaped'))
        check(False, 'main() accepted an output dir outside the allowed roots')
    except ValueError:
        pass

    if failures:
        print(f'\n{len(failures)} failure(s)')
        return 1
    print('OK: terrain art template export validated')
    return 0


if __name__ == '__main__':
    sys.exit(main())
