"""Small stdlib PNG reader for deterministic art-source importers."""
import struct
import zlib


def read_size(path):
    with open(path, 'rb') as source:
        header = source.read(24)
    if (header[:8] != b'\x89PNG\r\n\x1a\n'
            or header[12:16] != b'IHDR'):
        raise ValueError(f'{path}: invalid PNG header')
    return struct.unpack('>II', header[16:24])


def _paeth(a, b, c):
    estimate = a + b - c
    da, db, dc = abs(estimate - a), abs(estimate - b), abs(estimate - c)
    if da <= db and da <= dc:
        return a
    return b if db <= dc else c


def read_rgba(path):
    with open(path, 'rb') as source:
        data = source.read()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError(f'{path}: invalid PNG signature')
    pos = 8
    width = height = color_type = None
    compressed = bytearray()
    while pos < len(data):
        length, = struct.unpack('>I', data[pos:pos + 4])
        tag = data[pos + 4:pos + 8]
        payload = data[pos + 8:pos + 8 + length]
        if tag == b'IHDR':
            width, height, depth, color_type = struct.unpack('>IIBB', payload[:10])
            if depth != 8 or color_type not in (2, 6):
                raise ValueError(f'{path}: expected 8-bit RGB/RGBA PNG')
        elif tag == b'IDAT':
            compressed.extend(payload)
        elif tag == b'IEND':
            break
        pos += length + 12

    channels = 3 if color_type == 2 else 4
    packed = zlib.decompress(compressed)
    stride = width * channels
    previous = bytearray(stride)
    pixels = []
    cursor = 0
    for _ in range(height):
        filter_type = packed[cursor]
        cursor += 1
        encoded = packed[cursor:cursor + stride]
        cursor += stride
        row = bytearray(stride)
        for i, value in enumerate(encoded):
            left = row[i - channels] if i >= channels else 0
            above = previous[i]
            upper_left = previous[i - channels] if i >= channels else 0
            if filter_type == 0:
                decoded = value
            elif filter_type == 1:
                decoded = value + left
            elif filter_type == 2:
                decoded = value + above
            elif filter_type == 3:
                decoded = value + (left + above) // 2
            elif filter_type == 4:
                decoded = value + _paeth(left, above, upper_left)
            else:
                raise ValueError(f'{path}: unsupported PNG filter {filter_type}')
            row[i] = decoded & 0xff
        for x in range(width):
            start = x * channels
            alpha = row[start + 3] if channels == 4 else 255
            pixels.append((row[start], row[start + 1], row[start + 2], alpha))
        previous = row
    return width, height, pixels
