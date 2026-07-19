#!/usr/bin/env python3
"""Export the most-used Stage 2 Wang coast tiles as an art-task sheet.

The runtime atlas contains every mask/profile combination, but Stage 2 uses a
much smaller subset. This tool reproduces the runtime selector, ranks the
coastline tiles by use, and exports the common signatures at nearest-neighbour
scale. The generated Markdown is the edge-contract brief for an art pass.

Run from anywhere: python3 tools/export-terrain-art-templates.py [out_dir]
"""
from collections import Counter
import importlib.util
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import genlib
import gridpng

TERRAIN_CELL = 16
VISUAL_TILE = 32
PLAY_HEIGHT = 352
SHEET_COLUMNS = 4
SHEET_ROWS = 3
SCALE = 6
GUTTER = 16
SHEET_BG = (24, 28, 31, 255)


def load_tile_generator():
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        'gen-terrain-tile-atlas.py')
    spec = importlib.util.spec_from_file_location('terrain_tile_generator', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def u32(value):
    return value & 0xffffffff


def lattice_value(grid_x, grid_y, salt):
    value = u32(u32(grid_x) * 0x9e3779b9
                + u32(grid_y) * 0x85ebca6b + salt)
    value ^= value >> 16
    value = u32(value * 0x7feb352d)
    value ^= value >> 15
    return (value & 0xffff) / 32767.5 - 1.0


def smooth_noise(x, y, salt):
    x0 = math.floor(x)
    y0 = math.floor(y)
    tx = x - x0
    ty = y - y0
    tx = tx * tx * (3.0 - 2.0 * tx)
    ty = ty * ty * (3.0 - 2.0 * ty)
    top_left = lattice_value(x0, y0, salt)
    top = top_left + (lattice_value(x0 + 1, y0, salt) - top_left) * tx
    bottom_left = lattice_value(x0, y0 + 1, salt)
    bottom = bottom_left + (
        lattice_value(x0 + 1, y0 + 1, salt) - bottom_left) * tx
    return top + (bottom - top) * ty


def interface_level(grid_x, grid_y, vertical):
    noise = smooth_noise(
        grid_x * 0.28 + (7.25 if vertical else 0.0),
        grid_y * 0.28 + (3.75 if vertical else 0.0),
        0xc2b2ae35 if vertical else 0x27d4eb2f)
    if noise < -0.24:
        return 0
    if noise > 0.24:
        return 2
    return 1


def read_terrain_cells(path):
    cells = set()
    offset = None
    row = 0
    with open(path, encoding='utf-8') as source:
        for raw in source:
            line = raw.strip()
            if not line or line.startswith(';'):
                continue
            if line.startswith('@'):
                offset = int(line[1:])
                row = 0
                continue
            if offset is None:
                raise ValueError('terrain rows must follow an @offset line')
            for column, marker in enumerate(line):
                if marker == '#':
                    cells.add((offset // TERRAIN_CELL + column, row))
            row += 1
    return cells


def has_cell(cells, px, y):
    return (px // TERRAIN_CELL, y // TERRAIN_CELL) in cells


def vertex_land(cells, px, y):
    land = sum((
        has_cell(cells, px - 16, y - 16),
        has_cell(cells, px, y - 16),
        has_cell(cells, px - 16, y),
        has_cell(cells, px, y),
    ))
    return land >= 2


def used_coast_tiles(cells):
    counts = Counter()
    max_cell_x = max(x for x, _ in cells)
    for y in range(-VISUAL_TILE, PLAY_HEIGHT, VISUAL_TILE):
        for px in range(0, (max_cell_x + 2) * TERRAIN_CELL, VISUAL_TILE):
            mask = 0
            if vertex_land(cells, px, y):
                mask |= 1
            if vertex_land(cells, px + VISUAL_TILE, y):
                mask |= 2
            if vertex_land(cells, px + VISUAL_TILE, y + VISUAL_TILE):
                mask |= 4
            if vertex_land(cells, px, y + VISUAL_TILE):
                mask |= 8
            if mask in (0, 15):
                continue
            grid_x = px // VISUAL_TILE
            grid_y = y // VISUAL_TILE
            north = interface_level(grid_x, grid_y, False)
            east = interface_level(grid_x + 1, grid_y, True)
            south = interface_level(grid_x, grid_y + 1, False)
            west = interface_level(grid_x, grid_y, True)
            profile = north * 27 + east * 9 + south * 3 + west
            counts[(mask, profile)] += 1
    return counts


def write_sheet(tile_gen, selected, out_path):
    cell_size = tile_gen.TILE * SCALE + GUTTER * 2
    width = SHEET_COLUMNS * cell_size
    height = SHEET_ROWS * cell_size
    pixels = [SHEET_BG] * (width * height)
    for index, ((mask, profile), _) in enumerate(selected):
        source = tile_gen.make_tile(mask, profile)
        cell_x = index % SHEET_COLUMNS * cell_size + GUTTER
        cell_y = index // SHEET_COLUMNS * cell_size + GUTTER
        for y in range(tile_gen.TILE):
            for x in range(tile_gen.TILE):
                color = source[y * tile_gen.TILE + x]
                for sy in range(SCALE):
                    dest_y = cell_y + y * SCALE + sy
                    for sx in range(SCALE):
                        dest_x = cell_x + x * SCALE + sx
                        pixels[dest_y * width + dest_x] = color
    gridpng.write_rgba_png(width, height, pixels, out_path)


def write_brief(tile_gen, selected, out_path):
    lines = [
        '# Stage 2 common coast art templates',
        '',
        'The PNG is ordered left-to-right, top-to-bottom. Each panel is one',
        '32px native tile enlarged 6× with nearest-neighbour sampling.',
        '',
        'Art constraints:',
        '',
        '- Keep the outermost two native pixels unchanged; they are the seam contract.',
        '- Keep transparent water transparent.',
        '- Preserve land/water polarity and the visible crossing on every edge.',
        '- Use the `stage1_islet.png` palette and pixel density as the style reference.',
        '- No resampling, antialiasing, gradients, text, or arbitrary vertical flips.',
        '- Interior details may cross a tile only through a separately authored metatile.',
        '',
        '| Panel | Uses | Mask | North | East | South | West |',
        '| ---: | ---: | ---: | ---: | ---: | ---: | ---: |',
    ]
    for index, ((mask, profile), count) in enumerate(selected, 1):
        north, east, south, west = tile_gen.decode_profile(profile)
        lines.append(
            f'| {index} | {count} | `{mask:04b}` | {north} | {east} | {south} | {west} |')
    lines.extend((
        '',
        'The generated atlas remains the fallback. Only validated painted variants',
        'should replace or overlay these signatures at runtime.',
        '',
    ))
    with open(out_path, 'w', encoding='utf-8', newline='\n') as output:
        output.write('\n'.join(lines))


def main(out_dir=None):
    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out_dir = genlib.resolve_out_dir(out_dir, 'assets', 'tiles', 'art-templates')
    os.makedirs(out_dir, exist_ok=True)
    cells = read_terrain_cells(os.path.join(repo, 'assets', 'stages',
                                            'stage2_terrain.txt'))
    tile_gen = load_tile_generator()
    selected = used_coast_tiles(cells).most_common(SHEET_COLUMNS * SHEET_ROWS)
    write_sheet(tile_gen, selected,
                os.path.join(out_dir, 'stage2-common-coasts.png'))
    write_brief(tile_gen, selected,
                os.path.join(out_dir, 'stage2-common-coasts.md'))
    print(f'wrote {len(selected)} common coast templates to {out_dir}')


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else None)
