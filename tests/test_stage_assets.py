#!/usr/bin/env python3
"""End-to-end test of tools/compile-stage.py and the committed stage table.

Compiles assets/stages/stage1.txt into a temp dir, validates the parsed
stage structurally, checks the map-format error handling and the
output-dir guard, and byte-compares against the committed
src/stage1_data.c so the map and its compiled table can't drift apart.

Runs standalone: python3 tests/test_stage_assets.py
"""
import importlib.util
import os
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAP_PATH = os.path.join(REPO, 'assets', 'stages', 'stage1.txt')
COMMITTED = os.path.join(REPO, 'src', 'stage1_data.c')

PLAY_HEIGHT = 352
CELL = 32
ROWS = 11

failures = []


def check(cond, msg):
    if not cond:
        failures.append(msg)
        print('FAIL:', msg)


def load_compiler():
    path = os.path.join(REPO, 'tools', 'compile-stage.py')
    spec = importlib.util.spec_from_file_location('compile_stage', path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def check_rejects(comp, name, text):
    try:
        comp.parse_map(text)
        check(False, f'parse_map accepted {name}')
    except comp.MapError:
        pass


def block(offset, rows):
    return f'@{offset}\n' + '\n'.join(rows) + '\n'


def main():
    comp = load_compiler()

    # The output-dir guard must reject paths outside the repo/system temp.
    try:
        comp.genlib.validated_out_dir('/definitely/not/allowed')
        check(False, 'validated_out_dir accepted a path outside allowed roots')
    except ValueError:
        pass

    # Malformed maps must be rejected, not silently mis-compiled.
    ok_rows = ['.' * 4] * ROWS
    check_rejects(comp, 'a map with no blocks', '; only a comment\n')
    check_rejects(comp, 'a grid row before any header', '....\n')
    check_rejects(comp, 'a non-numeric header', block('x', ok_rows))
    check_rejects(comp, 'a non-contiguous block', block(64, ok_rows))
    check_rejects(comp, 'a short block', block(0, ok_rows[:-1]))
    check_rejects(comp, 'ragged row widths', block(0, ['.' * 4] * (ROWS - 1) + ['.' * 5]))
    check_rejects(comp, 'an unknown glyph', block(0, ['...x'] + ['.' * 4] * (ROWS - 1)))
    with tempfile.TemporaryDirectory() as tmp:
        empty_map = os.path.join(tmp, 'empty.txt')
        with open(empty_map, 'w', encoding='utf-8') as f:
            f.write(block(0, ok_rows))
        try:
            comp.compile_stage(empty_map)
            check(False, 'compile_stage accepted a map that spawns nothing')
        except comp.MapError:
            pass

    # Structural checks on the real Stage 1 map.
    with open(MAP_PATH, encoding='utf-8') as f:
        grid, length = comp.parse_map(f.read())
    events = comp.collect_events(grid)
    terrain = comp.collect_terrain(grid)

    check(length == 7200, f'stage length {length}, want 7200')
    check(len(events) > 0, 'stage spawns nothing')
    xs = [e[0] for e in events]
    check(xs == sorted(xs), 'events not sorted by x')
    for x, y, spawn_type in events:
        check(0 <= x <= length, f'{spawn_type} at x={x} outside the stage')
        check(0 <= y <= PLAY_HEIGHT, f'{spawn_type} at y={y} outside the play area')
    for x, y, w, h in terrain:
        check(x >= 0 and x + w <= length, f'terrain x [{x}, {x + w}] outside the stage')
        check(y >= 0 and y + h <= PLAY_HEIGHT, f'terrain y [{y}, {y + h}] outside the play area')
        check(w % CELL == 0 and h % CELL == 0, f'terrain rect {w}x{h} not cell-aligned')

    # Design landmarks (update when the map is deliberately retuned): the
    # beat 7 islet is three rects, with the Relay Node on its western shore.
    check(len(terrain) == 3, f'{len(terrain)} terrain rects, want the 3 islet rects')
    relay = [(x, y) for x, y, t in events if t == 'STAGE_SPAWN_RELAY_NODE']
    check(relay == [(4400.0, 144.0)], f'relay node at {relay}, want [(4400.0, 144.0)]')
    if terrain:
        west_edge = min(x for x, _, _, _ in terrain)
        check(relay and relay[0][0] < west_edge,
              'relay node is not west of the islet - torpedo lane blocked')

    # Drift check: regenerating must reproduce the committed table exactly.
    with tempfile.TemporaryDirectory() as tmp:
        comp.main(tmp)
        with open(os.path.join(tmp, 'stage1_data.c'), 'rb') as f:
            generated = f.read()
    check(os.path.exists(COMMITTED), 'src/stage1_data.c missing')
    if os.path.exists(COMMITTED):
        with open(COMMITTED, 'rb') as f:
            committed = f.read()
        check(committed == generated,
              'src/stage1_data.c differs from compiler output '
              '- rerun tools/compile-stage.py and commit the result')

    if failures:
        print(f'\n{len(failures)} failure(s)')
        return 1
    print(f'OK: stage1 map compiled, {len(events)} events, '
          f'{len(terrain)} terrain rects, length {length}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
