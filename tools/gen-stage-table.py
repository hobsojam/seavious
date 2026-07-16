#!/usr/bin/env python3
"""Compiles an ASCII stage map (assets/stages/*.txt) into the committed C
spawn-event table the engine consumes directly — no text parsing at
runtime (see README "Stage authoring format").

Map semantics:
  - 1 character = 32 px; columns are scroll distance, rows are lanes
    (11 rows over the 352px play area, lane centers at row*32+16).
  - Blocks start with `@<px>` headers and hold exactly 11 grid rows;
    map length = the furthest block edge (px + widest row * 32).
  - Air glyphs fire their spawn when their column reaches the right
    screen edge; ground glyphs are world positions entering there — the
    engine treats both as "fire at scrollDistance >= px".
  - `#` cells are terrain and `H` cells are terrain hardpoints; contiguous
    cells merge into blocking
    footprint rectangles (horizontal runs, then vertically stacked
    identical runs).

Output: src/stage1_data.c (do not edit by hand); the type definitions
live in the hand-written src/stage_data.h. A drift test regenerates and
byte-compares, same as the audio/sprite assets.

Run from anywhere:  python3 tools/gen-stage-table.py [out_dir]
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import genlib

COLUMN_PX = 32
LANES = 11

GLYPH_KINDS = {
    'd': 'STAGE_SPAWN_DRONE',
    'w': 'STAGE_SPAWN_DRONE_LINE3',
    'W': 'STAGE_SPAWN_DRONE_V5',
    'i': 'STAGE_SPAWN_INTERCEPTOR',
    'G': 'STAGE_SPAWN_GUNSHIP',
    'C': 'STAGE_SPAWN_CASEMATE',
    'T': 'STAGE_SPAWN_TRACKING_TURRET',
    'R': 'STAGE_SPAWN_RELAY_NODE',
    'm': 'STAGE_SPAWN_MINE',
    'P': 'STAGE_SPAWN_MOBILE_PLATFORM',
    'B': 'STAGE_SPAWN_MORTAR_BATTERY',
    'K': 'STAGE_SPAWN_DRONE_BUNKER',
}

# Land installations bring their mounting pad with them: the glyph's cell
# compiles as terrain + hardpoint + spawn event, so a pad can never be
# authored empty and an installation can never float off land.
LAND_GLYPHS = frozenset('BK')


def parse_map(path):
    events = []          # (px, lane_row, kind)
    terrain_cells = set()  # (col_px, lane_row)
    hardpoints = []        # (col_px, lane_row)
    length_px = 0

    block_px = None
    rows_seen = 0
    for lineno, raw in enumerate(open(path, encoding='utf-8'), 1):
        line = raw.rstrip('\n')
        if not line.strip() or line.lstrip().startswith(';'):
            continue
        if line.startswith('@'):
            if block_px is not None and rows_seen != LANES:
                raise SystemExit(f'{path}:{lineno}: block @{block_px} has '
                                 f'{rows_seen} rows, want {LANES}')
            block_px = int(line[1:])
            rows_seen = 0
            continue
        if block_px is None:
            raise SystemExit(f'{path}:{lineno}: grid row before any @ header')
        if rows_seen >= LANES:
            raise SystemExit(f'{path}:{lineno}: block @{block_px} has more '
                             f'than {LANES} rows')
        row = rows_seen
        rows_seen += 1
        length_px = max(length_px, block_px + len(line) * COLUMN_PX)
        for col, ch in enumerate(line):
            if ch == '.':
                continue
            px = block_px + col * COLUMN_PX
            if ch == '#':
                terrain_cells.add((px, row))
            elif ch == 'H':
                terrain_cells.add((px, row))
                hardpoints.append((px, row))
            elif ch in LAND_GLYPHS:
                terrain_cells.add((px, row))
                hardpoints.append((px, row))
                events.append((px, row, GLYPH_KINDS[ch]))
            elif ch in GLYPH_KINDS:
                events.append((px, row, GLYPH_KINDS[ch]))
            else:
                raise SystemExit(f'{path}:{lineno}: unknown glyph {ch!r}')
    if block_px is not None and rows_seen != LANES:
        raise SystemExit(f'{path}: final block @{block_px} has {rows_seen} '
                         f'rows, want {LANES}')

    events.sort(key=lambda e: (e[0], e[1]))
    return events, merge_terrain(terrain_cells), sorted(hardpoints), length_px


def merge_terrain(cells):
    """Merge contiguous terrain cells into rectangles: horizontal runs per
    row first, then vertically stacked identical runs."""
    runs = []  # (px, row, width_px)
    for px, row in sorted(cells, key=lambda c: (c[1], c[0])):
        if runs and runs[-1][1] == row and runs[-1][0] + runs[-1][2] == px:
            runs[-1] = (runs[-1][0], row, runs[-1][2] + COLUMN_PX)
        else:
            runs.append((px, row, COLUMN_PX))

    rects = []  # (px, row, width_px, rows)
    for px, row, width in sorted(runs, key=lambda r: (r[1], r[0])):
        merged = False
        for i, (rpx, rrow, rwidth, rrows) in enumerate(rects):
            if rpx == px and rwidth == width and rrow + rrows == row:
                rects[i] = (rpx, rrow, rwidth, rrows + 1)
                merged = True
                break
        if not merged:
            rects.append((px, row, width, 1))
    return sorted(rects)


def emit(events, terrain, hardpoints, length_px, out_path, source_rel):
    lines = [
        '// Generated by tools/gen-stage-table.py from ' + source_rel,
        '// - do not edit by hand; regenerate and commit instead. A drift',
        '// test (tests/test_stage_table.py) keeps this in sync with the map.',
        '#include "stage_data.h"',
        '',
        'const StageSpawnEvent STAGE1_EVENTS[] = {',
    ]
    for px, row, kind in events:
        lane_y = row * COLUMN_PX + COLUMN_PX // 2
        lines.append(f'    {{ {px}, {lane_y}.0f, {kind} }},')
    lines += [
        '};',
        '',
        'const int STAGE1_EVENT_COUNT = '
        'sizeof(STAGE1_EVENTS) / sizeof(STAGE1_EVENTS[0]);',
        '',
        'const StageTerrainFootprint STAGE1_TERRAIN[] = {',
    ]
    for px, row, width, rows in terrain:
        lines.append(f'    {{ {px}, {row * COLUMN_PX}, {width}, {rows * COLUMN_PX} }},')
    lines += [
        '};',
        '',
        'const int STAGE1_TERRAIN_COUNT = '
        'sizeof(STAGE1_TERRAIN) / sizeof(STAGE1_TERRAIN[0]);',
        '',
        'const StageTerrainHardpoint STAGE1_TERRAIN_HARDPOINTS[] = {',
    ]
    if hardpoints:
        for px, row in hardpoints:
            lines.append(f'    {{ {px}, {row * COLUMN_PX} }},')
        count_expr = ('sizeof(STAGE1_TERRAIN_HARDPOINTS) / '
                      'sizeof(STAGE1_TERRAIN_HARDPOINTS[0])')
    else:
        # An empty initializer list isn't valid C; keep one zeroed row
        # and pin the count to 0 so nothing ever reads it.
        lines.append('    { 0, 0 }, // placeholder: no hardpoints authored')
        count_expr = '0'
    lines += [
        '};',
        '',
        f'const int STAGE1_TERRAIN_HARDPOINT_COUNT = {count_expr};',
        '',
        f'const int STAGE1_LENGTH_PX = {length_px};',
        '',
    ]
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))
    print(f'wrote {out_path} ({len(events)} events, {len(terrain)} terrain '
          f'footprints, length {length_px}px)')


def main(out_dir=None):
    out_dir = genlib.resolve_out_dir(out_dir, 'src')
    map_path = os.path.join(genlib.repo_root(), 'assets', 'stages', 'stage1.txt')
    events, terrain, hardpoints, length_px = parse_map(map_path)
    emit(events, terrain, hardpoints, length_px,
         os.path.join(out_dir, 'stage1_data.c'), 'assets/stages/stage1.txt')


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else None)
