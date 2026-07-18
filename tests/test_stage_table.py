#!/usr/bin/env python3
"""Drift test for the compiled stage table.

Recompiles assets/stages/stage1.txt with tools/gen-stage-table.py into a
temp dir and byte-compares against the committed src/stage1_data.c, so
the map and the table the engine actually consumes can't drift apart.
Also validates map/table invariants the C tests can't see (glyph census
against the map source).

Runs standalone: python3 tests/test_stage_table.py
"""
import importlib.util
import os
import re
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAP_PATH = os.path.join(REPO, 'assets', 'stages', 'stage1.txt')
COMMITTED = os.path.join(REPO, 'src', 'stage1_data.c')
STAGE2_MAP_PATH = os.path.join(REPO, 'assets', 'stages', 'stage2.txt')
STAGE2_TERRAIN_PATH = os.path.join(REPO, 'assets', 'stages', 'stage2_terrain.txt')
STAGE2_COMMITTED = os.path.join(REPO, 'src', 'stage2_data.c')

failures = []


def check(cond, msg):
    if not cond:
        failures.append(msg)
        print('FAIL:', msg)


def load_generator():
    path = os.path.join(REPO, 'tools', 'gen-stage-table.py')
    spec = importlib.util.spec_from_file_location('gen_stage_table', path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def map_glyph_census():
    census = {}
    for line in open(MAP_PATH, encoding='utf-8'):
        line = line.rstrip('\n')
        if not line.strip() or line.lstrip().startswith(';') or line.startswith('@'):
            continue
        for ch in line:
            if ch != '.':
                census[ch] = census.get(ch, 0) + 1
    return census


def stage2_blocks():
    """Return each Stage 2 block's scroll offset and eleven source rows."""
    blocks = []
    offset = None
    rows = []
    with open(STAGE2_MAP_PATH, encoding='utf-8') as stage2:
        for line in stage2:
            line = line.rstrip('\n')
            if line.startswith('@'):
                if offset is not None:
                    blocks.append((offset, rows))
                offset = int(line[1:])
                rows = []
            elif offset is not None and line and not line.startswith(';'):
                rows.append(line)
    if offset is not None:
        blocks.append((offset, rows))
    return blocks


def check_stage2_installations_are_inset():
    terrain = set('#BKH')
    for offset, rows in stage2_blocks():
        check(len(rows) == 11, f'Stage 2 block @{offset} has {len(rows)} rows')
        for y, row in enumerate(rows):
            for x, glyph in enumerate(row):
                if glyph not in 'BK':
                    continue
                if y == 0 or y == len(rows) - 1 or x == 0 or x == len(row) - 1:
                    check(False, f'{glyph} at @{offset}, row {y}, col {x} is on the map edge')
                    continue
                neighbours = [
                    rows[y + dy][x + dx]
                    for dy in (-1, 0, 1)
                    for dx in (-1, 0, 1)
                    if dx != 0 or dy != 0
                ]
                check(all(cell in terrain for cell in neighbours),
                      f'{glyph} at @{offset}, row {y}, col {x} is not inset in terrain')


def check_stage2_fine_terrain_pads(gen):
    _, _, hardpoints, length_px = gen.parse_map(STAGE2_MAP_PATH)
    _, terrain_cells = gen.parse_terrain_mask(STAGE2_TERRAIN_PATH, length_px)
    try:
        gen.validate_hardpoint_terrain(hardpoints, terrain_cells)
    except SystemExit as error:
        check(False, str(error))


def check_stage2_emitted_hardpoints(generated):
    """The 16px source rows must not be emitted at the old 32px pitch."""
    terrain_text = generated.split('STAGE2_TERRAIN[]')[1].split('};', 1)[0]
    rects = [tuple(map(int, match)) for match in re.findall(
        r'\{ (\d+), (\d+), (\d+), (\d+) \},', terrain_text)]
    gen = load_generator()
    _, _, hardpoints, _ = gen.parse_map(STAGE2_MAP_PATH)
    for px, row in hardpoints:
        y = row * 32
        # A B/K cell must be land at all four corners of its 32px pad.
        for probe_x in (px, px + 16):
            for probe_y in (y, y + 16):
                check(any(x <= probe_x < x + width and top <= probe_y < top + height
                          for x, top, width, height in rects),
                      f'emitted Stage 2 terrain misses hardpoint at ({px}, {y})')


def main():
    gen = load_generator()

    with tempfile.TemporaryDirectory() as tmp:
        gen.main(tmp)
        out_path = os.path.join(tmp, 'stage1_data.c')
        check(os.path.exists(out_path), 'generator did not write stage1_data.c')
        with open(out_path, encoding='utf-8') as f:
            generated = f.read()
        with open(os.path.join(tmp, 'stage2_data.c'), encoding='utf-8') as f:
            generated_stage2 = f.read()
    with open(COMMITTED, encoding='utf-8') as f:
        committed = f.read()
    check(generated == committed,
          'committed src/stage1_data.c differs from the compiled map - '
          'rerun tools/gen-stage-table.py and commit the result')
    with open(STAGE2_COMMITTED, encoding='utf-8') as f:
        committed_stage2 = f.read()
    check(generated_stage2 == committed_stage2,
          'committed src/stage2_data.c differs from the compiled map - '
          'rerun tools/gen-stage-table.py and commit the result')

    # The table must account for every glyph in the map. Land glyphs
    # (B/K) are spawn events AND terrain cells: they count on both sides.
    census = map_glyph_census()
    spawn_glyphs = sum(n for ch, n in census.items() if ch not in '#H')
    event_count = len(re.findall(r'STAGE_SPAWN_\w+ }', generated))
    check(event_count == spawn_glyphs,
          f'table has {event_count} events, map has {spawn_glyphs} spawn glyphs')

    length = int(re.search(r'STAGE1_LENGTH_PX = (\d+);', generated).group(1))
    check(length == 7200, f'stage length {length} != 7200')
    stage2_length = int(re.search(r'STAGE2_LENGTH_PX = (\d+);', generated_stage2).group(1))
    check(stage2_length == 7680, f'Stage 2 length {stage2_length} != 7680')
    check_stage2_installations_are_inset()
    check_stage2_fine_terrain_pads(gen)
    check_stage2_emitted_hardpoints(generated_stage2)

    # Terrain rects must cover exactly the map's land cell area.
    rects = re.findall(r'\{ (\d+), (\d+), (\d+), (\d+) \},',
                       generated.split('STAGE1_TERRAIN[]')[1])
    rect_area = sum(int(w) * int(h) for _, _, w, h in rects)
    terrain_cells = sum(census.get(ch, 0) for ch in '#HBK')
    check(rect_area == terrain_cells * 32 * 32,
          f'terrain rect area {rect_area} != {terrain_cells} cells')

    # Land installation glyphs compile to all three tables at once: spawn
    # event + terrain cell + hardpoint pad in the same cell (the
    # installation brings its mounting pad with it). Stage 1 authors
    # none, so this runs against a synthetic map.
    with tempfile.TemporaryDirectory() as tmp:
        map_path = os.path.join(tmp, 'map.txt')
        rows = ['....'] * 11
        rows[1] = '.B..'
        rows[6] = '..K.'
        with open(map_path, 'w', encoding='utf-8') as f:
            f.write('@0\n' + '\n'.join(rows) + '\n')
        events, terrain, hardpoints, _ = gen.parse_map(map_path)
    check(sorted(e[2] for e in events)
          == ['STAGE_SPAWN_DRONE_BUNKER', 'STAGE_SPAWN_MORTAR_BATTERY'],
          f'land glyphs compiled to events {events}')
    check((32, 1) in hardpoints and (64, 6) in hardpoints,
          f'land glyphs missing their pads: {hardpoints}')
    check(len(terrain) == 2
          and (32, 1, 32, 1) in terrain and (64, 6, 32, 1) in terrain,
          f'land glyphs missing their terrain cells: {terrain}')

    if failures:
        print(f'\n{len(failures)} failure(s)')
        return 1
    print('OK: stage table matches the map '
          f'({event_count} events, {len(rects)} terrain footprints)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
