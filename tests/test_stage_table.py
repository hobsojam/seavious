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


def main():
    gen = load_generator()

    with tempfile.TemporaryDirectory() as tmp:
        gen.main(tmp)
        out_path = os.path.join(tmp, 'stage1_data.c')
        check(os.path.exists(out_path), 'generator did not write stage1_data.c')
        with open(out_path, encoding='utf-8') as f:
            generated = f.read()
    with open(COMMITTED, encoding='utf-8') as f:
        committed = f.read()
    check(generated == committed,
          'committed src/stage1_data.c differs from the compiled map - '
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
