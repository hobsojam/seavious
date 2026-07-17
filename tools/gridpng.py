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


def _write_png(width, height, raw, out_path):
    """Assemble and write an 8-bit RGBA PNG from raw (filtered) scanline
    bytes - the shared tail of every writer below."""
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


def write_grid_png(width, height, palette, grid, out_path):
    """palette: dict char -> (r, g, b, a). Grid rows shorter than width are
    padded with the '.' entry (conventionally transparent)."""
    raw = bytearray()
    for y in range(height):
        row = grid[y] if y < len(grid) else ''
        raw.append(0)  # filter type: None
        for x in range(width):
            ch = row[x] if x < len(row) else '.'
            raw += bytes(palette[ch])
    _write_png(width, height, raw, out_path)


def write_rgba_png(width, height, pixels, out_path):
    """pixels: flat list of (r, g, b, a) in row-major order, for
    generators that compose true-color output from existing assets."""
    raw = bytearray()
    for y in range(height):
        raw.append(0)  # filter type: None
        for x in range(width):
            raw += bytes(pixels[y * width + x])
    _write_png(width, height, raw, out_path)


def _unfilter_scanline(filter_type, line, prev, bpp):
    """Reverse one scanline's PNG filter in place (spec section 9)."""
    for x in range(len(line)):
        a = line[x - bpp] if x >= bpp else 0
        b = prev[x]
        c = prev[x - bpp] if x >= bpp else 0
        if filter_type == 1:
            recon = a
        elif filter_type == 2:
            recon = b
        elif filter_type == 3:
            recon = (a + b) // 2
        elif filter_type == 4:
            p = a + b - c
            pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
            recon = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
        else:
            recon = 0
        line[x] = (line[x] + recon) & 255


def read_png_rgba(path):
    """Minimal stdlib PNG reader for the generators' own source assets:
    8-bit RGB/RGBA, non-interlaced (what the art exports and our own
    writers produce). Returns (width, height, flat [(r,g,b,a)] list)."""
    data = open(path, 'rb').read()
    assert data[:8] == b'\x89PNG\r\n\x1a\n', f'{path}: not a PNG'
    pos, width, height, ctype, idat = 8, 0, 0, None, b''
    while pos < len(data):
        length, = struct.unpack('>I', data[pos:pos + 4])
        tag = data[pos + 4:pos + 8]
        payload = data[pos + 8:pos + 8 + length]
        if tag == b'IHDR':
            width, height, depth, ctype = struct.unpack('>IIBB', payload[:10])
            assert depth == 8 and ctype in (2, 6), \
                f'{path}: unsupported depth={depth} color={ctype}'
        elif tag == b'IDAT':
            idat += payload
        pos += 12 + length
    bpp = 4 if ctype == 6 else 3
    stride = width * bpp
    raw = zlib.decompress(idat)
    out = []
    prev = bytearray(stride)
    i = 0
    for _y in range(height):
        filter_type = raw[i]
        line = bytearray(raw[i + 1:i + 1 + stride])
        i += 1 + stride
        _unfilter_scanline(filter_type, line, prev, bpp)
        prev = line
        for x in range(width):
            px = line[x * bpp:x * bpp + bpp]
            out.append((px[0], px[1], px[2], px[3] if bpp == 4 else 255))
    return width, height, out
