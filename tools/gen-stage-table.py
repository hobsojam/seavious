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
    cells merge into blocking footprint rectangles (horizontal runs, then
    vertically stacked identical runs). Stage 2 replaces that coarse terrain
    layer with stage2_terrain.txt's 16px mask while retaining this map's
    encounter/hardpoint layer.

Output: src/stage1_data.c and src/stage2_data.c (do not edit by hand);
the type definitions live in the hand-written src/stage_data.h. A drift
test regenerates and byte-compares, same as the audio/sprite assets.

Run from anywhere:  python3 tools/gen-stage-table.py [out_dir]
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import genlib

COLUMN_PX = 32
LANES = 11
TERRAIN_TILE_PX = 16
TERRAIN_LANES = 22
TERRAIN_COLUMNS = 48
TERRAIN_BLOCK_PX = TERRAIN_TILE_PX * TERRAIN_COLUMNS

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


def merge_terrain(cells, cell_px=COLUMN_PX):
    """Merge contiguous terrain cells into rectangles: horizontal runs per
    row first, then vertically stacked identical runs."""
    runs = []  # (px, row, width_px)
    for px, row in sorted(cells, key=lambda c: (c[1], c[0])):
        if runs and runs[-1][1] == row and runs[-1][0] + runs[-1][2] == px:
            runs[-1] = (runs[-1][0], row, runs[-1][2] + cell_px)
        else:
            runs.append((px, row, cell_px))

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


def parse_terrain_mask(path, expected_length_px):
    """Parse a Stage 2-only 16px terrain layer.

    Encounter glyphs deliberately remain in the normal 32px stage map;
    this finer layer owns only coastline/collision geometry.  Keeping the
    concerns separate lets a designer reshape a shore without moving a
    spawn's timing or its hardpoint.
    """
    cells = set()
    block_px = None
    rows_seen = 0
    block_offsets = []

    for lineno, raw in enumerate(open(path, encoding='utf-8'), 1):
        line = raw.rstrip('\n')
        if not line.strip() or line.lstrip().startswith(';'):
            continue
        if line.startswith('@'):
            if block_px is not None and rows_seen != TERRAIN_LANES:
                raise SystemExit(f'{path}:{lineno}: block @{block_px} has '
                                 f'{rows_seen} rows, want {TERRAIN_LANES}')
            block_px = int(line[1:])
            block_offsets.append(block_px)
            rows_seen = 0
            continue
        if block_px is None:
            raise SystemExit(f'{path}:{lineno}: grid row before any @ header')
        if rows_seen >= TERRAIN_LANES:
            raise SystemExit(f'{path}:{lineno}: block @{block_px} has more '
                             f'than {TERRAIN_LANES} rows')
        if any(ch not in '.#' for ch in line):
            raise SystemExit(f'{path}:{lineno}: terrain masks use only . and #')
        if len(line) != TERRAIN_COLUMNS:
            raise SystemExit(f'{path}:{lineno}: row has {len(line)} columns, '
                             f'want {TERRAIN_COLUMNS}')
        for col, ch in enumerate(line):
            if ch == '#': cells.add((block_px + col * TERRAIN_TILE_PX, rows_seen))
        rows_seen += 1

    if block_px is not None and rows_seen != TERRAIN_LANES:
        raise SystemExit(f'{path}: final block @{block_px} has {rows_seen} '
                         f'rows, want {TERRAIN_LANES}')
    if not cells:
        raise SystemExit(f'{path}: terrain mask is empty')
    expected_offsets = list(range(0, expected_length_px, TERRAIN_BLOCK_PX))
    if block_offsets != expected_offsets:
        raise SystemExit(f'{path}: block offsets {block_offsets} do not cover '
                         f'0..{expected_length_px} in {TERRAIN_BLOCK_PX}px blocks')
    return merge_terrain(cells, TERRAIN_TILE_PX), cells


def validate_hardpoint_terrain(hardpoints, terrain_cells):
    """Require a 64px square of fine terrain around every B/K mounting pad."""
    for px, row in hardpoints:
        y = row * COLUMN_PX
        for cell_y in range(y - TERRAIN_TILE_PX, y + 3 * TERRAIN_TILE_PX,
                            TERRAIN_TILE_PX):
            for cell_x in range(px - TERRAIN_TILE_PX, px + 3 * TERRAIN_TILE_PX,
                                TERRAIN_TILE_PX):
                if (cell_x, cell_y // TERRAIN_TILE_PX) not in terrain_cells:
                    raise SystemExit('hardpoint at '
                                     f'({px}, {y}) lacks a 64px terrain pad')


def emit(events, terrain, hardpoints, length_px, out_path, source_rel, stage_name,
         terrain_cell_px=COLUMN_PX):
    symbol = stage_name.upper()
    lines = [
        '// Generated by tools/gen-stage-table.py from ' + source_rel,
        '// - do not edit by hand; regenerate and commit instead. A drift',
        '// test (tests/test_stage_table.py) keeps this in sync with the map.',
        '#include "stage_data.h"',
        '',
        f'const StageSpawnEvent {symbol}_EVENTS[] = {{',
    ]
    for px, row, kind in events:
        lane_y = row * COLUMN_PX + COLUMN_PX // 2
        lines.append(f'    {{ {px}, {lane_y}.0f, {kind} }},')
    lines += [
        '};',
        '',
        f'const int {symbol}_EVENT_COUNT = '
        f'sizeof({symbol}_EVENTS) / sizeof({symbol}_EVENTS[0]);',
        '',
        f'const StageTerrainFootprint {symbol}_TERRAIN[] = {{',
    ]
    for px, row, width, rows in terrain:
        lines.append(f'    {{ {px}, {row * terrain_cell_px}, {width}, '
                     f'{rows * terrain_cell_px} }},')
    lines += [
        '};',
        '',
        f'const int {symbol}_TERRAIN_COUNT = '
        f'sizeof({symbol}_TERRAIN) / sizeof({symbol}_TERRAIN[0]);',
        '',
        f'const StageTerrainHardpoint {symbol}_TERRAIN_HARDPOINTS[] = {{',
    ]
    if hardpoints:
        for px, row in hardpoints:
            lines.append(f'    {{ {px}, {row * COLUMN_PX} }},')
        count_expr = (f'sizeof({symbol}_TERRAIN_HARDPOINTS) / '
                      f'sizeof({symbol}_TERRAIN_HARDPOINTS[0])')
    else:
        # An empty initializer list isn't valid C; keep one zeroed row
        # and pin the count to 0 so nothing ever reads it.
        lines.append('    { 0, 0 }, // placeholder: no hardpoints authored')
        count_expr = '0'
    lines += [
        '};',
        '',
        f'const int {symbol}_TERRAIN_HARDPOINT_COUNT = {count_expr};',
        '',
        f'const int {symbol}_LENGTH_PX = {length_px};',
        '',
    ]
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))
    print(f'wrote {out_path} ({len(events)} events, {len(terrain)} terrain '
          f'footprints, length {length_px}px)')


def main(out_dir=None):
    out_dir = genlib.resolve_out_dir(out_dir, 'src')
    for stage_name in ('stage1', 'stage2'):
        map_path = os.path.join(genlib.repo_root(), 'assets', 'stages', stage_name + '.txt')
        if not os.path.exists(map_path):
            continue
        events, terrain, hardpoints, length_px = parse_map(map_path)
        source_rel = 'assets/stages/' + stage_name + '.txt'
        terrain_cell_px = COLUMN_PX
        if stage_name == 'stage2':
            mask_path = os.path.join(genlib.repo_root(), 'assets', 'stages',
                                     'stage2_terrain.txt')
            terrain, terrain_cells = parse_terrain_mask(mask_path, length_px)
            validate_hardpoint_terrain(hardpoints, terrain_cells)
            source_rel += ' + assets/stages/stage2_terrain.txt'
            terrain_cell_px = TERRAIN_TILE_PX
        emit(events, terrain, hardpoints, length_px,
             os.path.join(out_dir, stage_name + '_data.c'), source_rel,
             stage_name, terrain_cell_px)


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else None)
