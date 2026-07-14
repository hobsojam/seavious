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

    # The table must account for every glyph in the map.
    census = map_glyph_census()
    spawn_glyphs = sum(n for ch, n in census.items() if ch != '#')
    event_count = len(re.findall(r'STAGE_SPAWN_\w+ }', generated))
    check(event_count == spawn_glyphs,
          f'table has {event_count} events, map has {spawn_glyphs} spawn glyphs')

    length = int(re.search(r'STAGE1_LENGTH_PX = (\d+);', generated).group(1))
    check(length == 7200, f'stage length {length} != 7200')

    # Terrain rects must cover exactly the map's # cell area.
    rects = re.findall(r'\{ (\d+), (\d+), (\d+), (\d+) \},',
                       generated.split('STAGE1_TERRAIN[]')[1])
    rect_area = sum(int(w) * int(h) for _, _, w, h in rects)
    check(rect_area == census.get('#', 0) * 32 * 32,
          f'terrain rect area {rect_area} != {census.get("#", 0)} cells')

    if failures:
        print(f'\n{len(failures)} failure(s)')
        return 1
    print('OK: stage table matches the map '
          f'({event_count} events, {len(rects)} terrain footprints)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
