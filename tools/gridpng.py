"""Shared ASCII-grid -> PNG writer for the Python sprite generators.

Python twin of gen-grid-image.ps1 (used by the PowerShell sprite
generators): each generator stays explicit and asset-specific — grid,
palette, and output path all readable in the generator file — and only
this mechanical bitmap write step is shared. Pure stdlib (zlib) so it
runs anywhere Python does.
"""
import os
import struct
import zlib


def _chunk(tag, payload):
    return (struct.pack('>I', len(payload)) + tag + payload
            + struct.pack('>I', zlib.crc32(tag + payload) & 0xffffffff))


def write_grid_png(width, height, palette, grid, out_path):
    """palette: dict char -> (r, g, b, a). Grid rows shorter than width are
    padded with the '.' entry (conventionally transparent)."""
    raw = bytearray()
    for y in range(height):
        row = grid[y] if y < len(grid) else ''
        raw.append(0)  # filter type: None
        for x in range(width):
            ch = row[x] if x < len(row) else '.'
            r, g, b, a = palette[ch]
            raw += bytes((r, g, b, a))

    ihdr = struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)  # 8-bit RGBA
    png = (b'\x89PNG\r\n\x1a\n'
           + _chunk(b'IHDR', ihdr)
           + _chunk(b'IDAT', zlib.compress(bytes(raw), 9))
           + _chunk(b'IEND', b''))
    out_dir = os.path.dirname(out_path)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    with open(out_path, 'wb') as f:
        f.write(png)
    print(f'wrote {out_path} ({width}x{height})')
