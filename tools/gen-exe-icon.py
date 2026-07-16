#!/usr/bin/env python3
"""Generates the application icon from the player ship sprite.

Outputs (into assets/icon/):
  seavious.ico     multi-size Windows icon (16/32/48/256, PNG-compressed
                   entries) embedded into the exe via src/seavious.rc
  window_icon.png  32x32 copy of the same art for SetWindowIcon at
                   runtime, so the running window/taskbar matches the exe

Composition: the 48x24 ship sprite centered on a solid ocean-navy square
(the game's water color) with a darker 1px border. Nearest-neighbour
scaling only - native at 48, integer 5x up for 256, and straight NN down
for 32/16 - keeping the pixel-art edges hard. Deterministic bytes, same
convention as every other generated asset.

Run from anywhere:  python3 tools/gen-exe-icon.py [out_dir]
"""
import os
import struct
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import genlib

NAVY = (20, 72, 116)     # ocean base #144874
BORDER = (13, 48, 80)    # darker rim
SIZES = (16, 32, 48, 256)


def load_ship():
    path = os.path.join(genlib.repo_root(), 'assets', 'sprites', 'player_ship.png')
    with open(path, 'rb') as f:
        data = f.read()
    pos = 8
    width = height = None
    idat = b''
    while pos < len(data):
        length, = struct.unpack('>I', data[pos:pos + 4])
        tag = data[pos + 4:pos + 8]
        if tag == b'IHDR':
            width, height, depth, color = struct.unpack('>IIBB', data[pos + 8:pos + 18])
            if depth != 8 or color != 6:
                raise SystemExit('player_ship.png: expected 8-bit RGBA')
        elif tag == b'IDAT':
            idat += data[pos + 8:pos + 8 + length]
        pos += 12 + length
    raw = zlib.decompress(idat)
    stride = width * 4 + 1
    pixels = []
    for y in range(height):
        row = raw[y * stride:(y + 1) * stride]
        if row[0] != 0:
            raise SystemExit('player_ship.png: expected filter-0 rows')
        pixels.append([tuple(row[1 + x * 4:5 + x * 4]) for x in range(width)])
    return width, height, pixels


def compose(size, ship_w, ship_h, ship):
    # Ship width per canvas: native at 48, integer 5x for 256, NN down
    # for the small sizes.
    target_w = 240 if size == 256 else size
    target_h = target_w * ship_h // ship_w
    x0 = (size - target_w) // 2
    y0 = (size - target_h) // 2

    canvas = []
    for y in range(size):
        row = []
        for x in range(size):
            edge = x == 0 or y == 0 or x == size - 1 or y == size - 1
            base = BORDER if edge else NAVY
            pixel = (base[0], base[1], base[2], 255)
            if x0 <= x < x0 + target_w and y0 <= y < y0 + target_h:
                sx = (x - x0) * ship_w // target_w
                sy = (y - y0) * ship_h // target_h
                r, g, b, a = ship[sy][sx]
                if a > 0:
                    pixel = ((r * a + base[0] * (255 - a)) // 255,
                             (g * a + base[1] * (255 - a)) // 255,
                             (b * a + base[2] * (255 - a)) // 255,
                             255)
            row.append(pixel)
        canvas.append(row)
    return canvas


def png_bytes(size, canvas):
    raw = bytearray()
    for row in canvas:
        raw.append(0)
        for r, g, b, a in row:
            raw += bytes((r, g, b, a))
    def chunk(tag, payload):
        return (struct.pack('>I', len(payload)) + tag + payload
                + struct.pack('>I', zlib.crc32(tag + payload) & 0xffffffff))
    ihdr = struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', ihdr)
            + chunk(b'IDAT', zlib.compress(bytes(raw), 9)) + chunk(b'IEND', b''))


def main(out_dir=None):
    out_dir = genlib.resolve_out_dir(out_dir, 'assets', 'icon')
    os.makedirs(out_dir, exist_ok=True)
    ship_w, ship_h, ship = load_ship()

    images = [(size, png_bytes(size, compose(size, ship_w, ship_h, ship))) for size in SIZES]

    # ICO container: ICONDIR header, one ICONDIRENTRY per image, then the
    # PNG blobs back to back. A size byte of 0 means 256.
    ico = struct.pack('<HHH', 0, 1, len(images))
    offset = 6 + 16 * len(images)
    for size, blob in images:
        size_byte = 0 if size == 256 else size
        ico += struct.pack('<BBBBHHII', size_byte, size_byte, 0, 0, 1, 32, len(blob), offset)
        offset += len(blob)
    for _, blob in images:
        ico += blob

    ico_path = os.path.join(out_dir, 'seavious.ico')
    with open(ico_path, 'wb') as f:
        f.write(ico)
    print(f'wrote {ico_path} ({len(images)} sizes)')

    window_path = os.path.join(out_dir, 'window_icon.png')
    with open(window_path, 'wb') as f:
        f.write(dict(images)[32])
    print(f'wrote {window_path} (32x32)')


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else None)
