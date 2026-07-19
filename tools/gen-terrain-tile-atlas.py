#!/usr/bin/env python3
"""Generate the native-pixel Wang-interface terrain shoreline atlas.

Each 32px visual tile combines four shared land/water corner states with an
explicit crossing position for each edge. A transition crosses at 1/4, 1/2,
or 3/4 along that edge; adjacent tiles derive the shared edge interface from
the same world-grid coordinate, so curved shores tessellate exactly.

The palette and band widths follow stage1_islet.png and
stage1_islet_atoll.png: olive ground, narrow irregular ochre beach, broken
white foam, then cyan wash. No source art is resized into the atlas.

Run from anywhere: python3 tools/gen-terrain-tile-atlas.py [out_dir]
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import genlib
import gridpng
import pngread

TILE = 32
INTERFACE_LEVELS = 3
# Keep enough movement for bays and headlands without allowing one tile to
# pull a crossing all the way from one quarter to the opposite quarter. The
# world selector varies these levels coherently over several tiles.
CROSSING_POSITIONS = (11, 16, 20)
PROFILE_COUNT = INTERFACE_LEVELS ** 4
PHASES = 2
PHASE_COUNT = PHASES ** 2
ATLAS_TILES = 72
ATLAS = TILE * ATLAS_TILES

NW, NE, SE, SW = 1, 2, 4, 8
WATER = (0, 0, 0, 0)
SURF = (102, 224, 228, 255)
FOAM = (224, 248, 240, 255)
WASH = (50, 166, 184, 255)
SAND_DARK = (174, 132, 54, 255)
SAND = (220, 174, 70, 255)
SAND_LIT = (246, 204, 94, 255)
GROUND_DARK = (111, 87, 46, 255)
GROUND = (139, 110, 56, 255)
GROUND_LIT = (177, 142, 74, 255)
SCRUB_DARK = (43, 79, 31, 255)
SCRUB = (65, 112, 38, 255)
ROCK_DARK = (67, 54, 45, 255)
ROCK = (112, 88, 64, 255)
ROCK_LIT = (157, 124, 88, 255)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GROUND_TILE_PATH = os.path.join(ROOT, 'assets', 'tiles', 'terrain_ground_tile.png')
GROUND_TILE_WIDTH, GROUND_TILE_HEIGHT, GROUND_TILE = pngread.read_rgba(GROUND_TILE_PATH)
if (GROUND_TILE_WIDTH, GROUND_TILE_HEIGHT) != (TILE * PHASES, TILE * PHASES):
    raise ValueError(
        f'{GROUND_TILE_PATH}: expected {TILE * PHASES}x{TILE * PHASES} seamless tile')


def hash2(x, y):
    return ((x * 1103515245 + y * 12345) >> 16) & 0x7fff


def decode_profile(profile):
    """Return N, E, S, W crossing levels for one atlas profile."""
    return (profile // 27 % 3, profile // 9 % 3,
            profile // 3 % 3, profile % 3)


def edge_field(first_land, second_land, level, position):
    """Signed field on one edge, with an exact authored crossing."""
    first = 1.0 if first_land else -1.0
    second = 1.0 if second_land else -1.0
    if first_land == second_land:
        return first
    crossing = CROSSING_POSITIONS[level]
    if position <= crossing:
        return first * (crossing - position) / crossing
    return second * (position - crossing) / (TILE - 1 - crossing)


def make_tile(mask, profile, phase_x=0, phase_y=0):
    pixels = [WATER] * (TILE * TILE)
    nw, ne, se, sw = (bool(mask & bit) for bit in (NW, NE, SE, SW))
    north_level, east_level, south_level, west_level = decode_profile(profile)

    field = [[0.0] * TILE for _ in range(TILE)]
    for y in range(TILE):
        for x in range(TILE):
            u = x / (TILE - 1)
            v = y / (TILE - 1)
            north = edge_field(nw, ne, north_level, x)
            east = edge_field(ne, se, east_level, y)
            south = edge_field(sw, se, south_level, x)
            west = edge_field(nw, sw, west_level, y)
            corner_blend = (
                (1.0 if nw else -1.0) * (1.0 - u) * (1.0 - v)
                + (1.0 if ne else -1.0) * u * (1.0 - v)
                + (1.0 if sw else -1.0) * (1.0 - u) * v
                + (1.0 if se else -1.0) * u * v
            )
            # Coons patch: the independently authored edge fields are exact
            # at tile boundaries and blend into one smooth interior field.
            field[y][x] = ((1.0 - v) * north + v * south
                           + (1.0 - u) * west + u * east - corner_blend)

    for y in range(TILE):
        for x in range(TILE):
            i = y * TILE + x
            value = field[y][x]
            # Exact shared edges use the same one-dimensional detail hash on
            # both incident tiles. Interior pixels can vary by signature.
            if (x == 0 or x == TILE - 1) and (y == 0 or y == TILE - 1):
                h = 0
            elif x == 0 or x == TILE - 1:
                h = hash2(y, 71)
            elif y == 0 or y == TILE - 1:
                h = hash2(x, 71)
            else:
                h = hash2(x + profile * 37, y + profile * 19)
            # Field thresholds, rather than a tile-local distance search,
            # make every shore band agree exactly on a shared edge.
            if value < -0.16:
                continue
            if value < -0.09:
                pixels[i] = WASH if h % 5 == 0 else SURF
            elif value < -0.01:
                pixels[i] = SURF if h % 4 == 0 else FOAM
            elif value < 0.07:
                pixels[i] = SAND if h % 5 == 0 else SAND_DARK
            elif value < 0.18:
                pixels[i] = SAND if h % 4 == 0 else SAND_LIT
            elif value < 0.34:
                if h % 11 == 0:
                    pixels[i] = SAND_LIT
                elif h % 5 == 0:
                    pixels[i] = SAND_DARK
                else:
                    pixels[i] = SAND
            else:
                # Four variants are consecutive 32px quarters of one seamless
                # 64px ground texture. Runtime selects the quarter from world
                # grid parity, so detail crosses Wang boundaries without
                # rectangular overlays or tile-local restarts.
                ground_x = (phase_x % PHASES) * TILE + x
                ground_y = (phase_y % PHASES) * TILE + y
                pixels[i] = GROUND_TILE[ground_y * GROUND_TILE_WIDTH + ground_x]
    return pixels


def main(out_dir=None):
    out_dir = genlib.resolve_out_dir(out_dir, 'assets', 'tiles')
    pixels = [WATER] * (ATLAS * ATLAS)
    for mask in range(16):
        for profile in range(PROFILE_COUNT):
            for phase_y in range(PHASES):
                for phase_x in range(PHASES):
                    tile = make_tile(mask, profile, phase_x, phase_y)
                    base_index = mask * PROFILE_COUNT + profile
                    tile_index = base_index * PHASE_COUNT + phase_y * PHASES + phase_x
                    ox = (tile_index % ATLAS_TILES) * TILE
                    oy = (tile_index // ATLAS_TILES) * TILE
                    for y in range(TILE):
                        for x in range(TILE):
                            pixels[(oy + y) * ATLAS + ox + x] = tile[y * TILE + x]
    gridpng.write_rgba_png(ATLAS, ATLAS, pixels,
                           os.path.join(out_dir, 'terrain_tile_atlas.png'))


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else None)
