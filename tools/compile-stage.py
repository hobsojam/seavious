#!/usr/bin/env python3
"""Compile the Stage 1 map (assets/stages/stage1.txt) into its C spawn table.

Map format (documented in the map file and README "Stage authoring
format"): `;` comment lines, `@<px>` block headers, then exactly 11 grid
rows per block (1 char = 32 px, rows top to bottom across the 352px play
area). Blocks must be contiguous from @0; the map's end is the stage
length, where the boss lock fires.

Emits stage1_data.c (structs in src/stage_data.h): every glyph becomes a
StageSpawnEvent at its cell center, sorted by x; contiguous `#` cells
merge into StageTerrainRect land footprints (horizontal runs per row,
then vertically joined where adjacent rows share the exact same span).

Explicit and stage-specific by convention, like the other generators.
Rerun and commit the result whenever the map changes —
tests/test_stage_assets.py fails if map and table drift.

Run from anywhere:  python3 tools/compile-stage.py [out_dir]
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import genlib

STAGE = 'stage1'
CELL = 32
ROWS = 11

GLYPHS = {
    'd': 'STAGE_SPAWN_SKIMMER_DRONE',
    'w': 'STAGE_SPAWN_DRONE_LINE3',
    'W': 'STAGE_SPAWN_DRONE_V5',
    'i': 'STAGE_SPAWN_INTERCEPTOR',
    'G': 'STAGE_SPAWN_GUNSHIP',
    'C': 'STAGE_SPAWN_CASEMATE',
    'T': 'STAGE_SPAWN_TRACKING_TURRET',
    'R': 'STAGE_SPAWN_RELAY_NODE',
    'm': 'STAGE_SPAWN_MINE',
    'P': 'STAGE_SPAWN_MOBILE_PLATFORM',
}
LAND = '#'
EMPTY = '.'


class MapError(ValueError):
    pass


def parse_map(text):
    """Parse map text into (grid, length_px), where grid is ROWS strings
    spanning the whole stage (blocks concatenated), so land masses can
    cross block boundaries."""
    grid = [''] * ROWS
    expected_offset = 0
    block_rows = None
    block_header = None

    def close_block():
        if block_header is None:
            return 0
        if len(block_rows) != ROWS:
            raise MapError(f'@{block_header}: {len(block_rows)} rows, want {ROWS}')
        widths = {len(r) for r in block_rows}
        if len(widths) != 1:
            raise MapError(f'@{block_header}: ragged row widths {sorted(widths)}')
        for i in range(ROWS):
            grid[i] += block_rows[i]
        return widths.pop() * CELL

    for lineno, line in enumerate(text.splitlines(), 1):
        if not line.strip() or line.lstrip().startswith(';'):
            continue
        if line.startswith('@'):
            expected_offset += close_block()
            try:
                offset = int(line[1:])
            except ValueError:
                raise MapError(f'line {lineno}: bad block header {line!r}')
            if offset != expected_offset:
                raise MapError(f'line {lineno}: block @{offset} not contiguous, '
                               f'expected @{expected_offset}')
            block_header = offset
            block_rows = []
            continue
        if block_header is None:
            raise MapError(f'line {lineno}: grid row before any @ header')
        bad = set(line) - set(GLYPHS) - {LAND, EMPTY}
        if bad:
            raise MapError(f'line {lineno}: unknown glyphs {sorted(bad)}')
        block_rows.append(line)

    length = expected_offset + close_block()
    if length == 0:
        raise MapError('map has no blocks')
    return grid, length


def collect_events(grid):
    events = []
    for row, cells in enumerate(grid):
        for col, cell in enumerate(cells):
            if cell in GLYPHS:
                events.append((col * CELL + CELL / 2,
                               row * CELL + CELL / 2,
                               GLYPHS[cell]))
    events.sort(key=lambda e: (e[0], e[1], e[2]))
    return events


def collect_terrain(grid):
    """Merge `#` cells into rects: horizontal runs per row, joined
    vertically while adjacent rows repeat the exact same column span."""
    rects = []
    active = {}  # (start_col, end_col) -> rect index still growing
    for row, cells in enumerate(grid):
        spans = []
        col = 0
        while col < len(cells):
            if cells[col] == LAND:
                start = col
                while col < len(cells) and cells[col] == LAND:
                    col += 1
                spans.append((start, col - 1))
            else:
                col += 1
        next_active = {}
        for span in spans:
            if span in active and rects[active[span]]['end_row'] == row - 1:
                rects[active[span]]['end_row'] = row
                next_active[span] = active[span]
            else:
                rects.append({'span': span, 'start_row': row, 'end_row': row})
                next_active[span] = len(rects) - 1
        active = next_active

    out = []
    for r in rects:
        start, end = r['span']
        out.append((start * CELL,
                    r['start_row'] * CELL,
                    (end - start + 1) * CELL,
                    (r['end_row'] - r['start_row'] + 1) * CELL))
    out.sort()
    return out


def emit_c(events, terrain, length):
    lines = [
        f'/* Generated by tools/compile-stage.py from assets/stages/{STAGE}.txt.',
        ' * Do not edit: change the map and rerun the compiler. */',
        '#include "stage_data.h"',
        '',
        f'static const StageSpawnEvent {STAGE}Events[] = {{',
    ]
    for x, y, spawn_type in events:
        lines.append(f'    {{ {x:.1f}f, {y:.1f}f, {spawn_type} }},')
    lines += ['};', '']
    if terrain:
        lines.append(f'static const StageTerrainRect {STAGE}Terrain[] = {{')
        for x, y, w, h in terrain:
            lines.append(f'    {{ {x:.1f}f, {y:.1f}f, {w:.1f}f, {h:.1f}f }},')
        lines += ['};', '']
        terrain_ref = f'{STAGE}Terrain'
        terrain_count = f'(int)(sizeof {STAGE}Terrain / sizeof {STAGE}Terrain[0])'
    else:
        terrain_ref, terrain_count = '0', '0'  # an all-water stage
    lines += [
        f'const StageData {STAGE}Data = {{',
        f'    {STAGE}Events,',
        f'    (int)(sizeof {STAGE}Events / sizeof {STAGE}Events[0]),',
        f'    {terrain_ref},',
        f'    {terrain_count},',
        f'    {length:.1f}f,',
        '};',
        '',
    ]
    return '\n'.join(lines)


def compile_stage(map_path):
    with open(map_path, encoding='utf-8') as f:
        grid, length = parse_map(f.read())
    events = collect_events(grid)
    terrain = collect_terrain(grid)
    if not events:
        raise MapError('map spawns nothing')
    return emit_c(events, terrain, length)


def main(out_dir=None):
    out_dir = genlib.resolve_out_dir(out_dir, 'src')
    map_path = os.path.join(genlib.repo_root(), 'assets', 'stages', f'{STAGE}.txt')
    out_path = os.path.join(out_dir, f'{STAGE}_data.c')
    source = compile_stage(map_path)
    with open(out_path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(source)
    print(f'wrote {out_path}')


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else None)
