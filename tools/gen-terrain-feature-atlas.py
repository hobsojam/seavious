#!/usr/bin/env python3
"""Import the reviewed terrain-metatile sheet at native game resolution.

The image-art source uses a flat magenta removal key. This generator removes
that key with no image-library dependency, downsamples each quadrant to a
128px transparent overlay, and clears a central 28px hardpoint pad.

Run from anywhere: python3 tools/gen-terrain-feature-atlas.py [out_dir]
"""
import os
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import genlib
import gridpng
import pngread

SIZE = 128
SOURCE_VARIANTS = 4
FAMILY_COUNT = 2
COUNT = SOURCE_VARIANTS * FAMILY_COUNT
SOURCE_COLUMNS = 2
SOURCE_ROWS = 2
TRANSPARENT = (0, 0, 0, 0)


def is_chroma(pixel):
    red, green, blue = pixel[:3]
    # The source promises no purple subject colours, so remove darker edge
    # pixels as well as the exact #ff00ff field. This prevents a pink fringe
    # surviving the reduction to native resolution.
    return (red > 50 and blue > 50
            and green < red * 0.75 and green < blue * 0.85)


def is_scrub(pixel):
    red, green, blue = pixel[:3]
    return (green >= red * 0.78 and blue < green * 0.65)


def substantial_scrub(sampled):
    """Keep authored shrub masses, but reject isolated downsampled specks."""
    scrub = [is_scrub(pixel) for pixel in sampled]
    keep = [False] * len(scrub)
    visited = [False] * len(scrub)
    for start, present in enumerate(scrub):
        if not present or visited[start]:
            continue
        component = []
        stack = [start]
        visited[start] = True
        while stack:
            index = stack.pop()
            component.append(index)
            x = index % SIZE
            y = index // SIZE
            for nx, ny in ((x - 1, y), (x + 1, y),
                           (x, y - 1), (x, y + 1)):
                if nx < 0 or nx >= SIZE or ny < 0 or ny >= SIZE:
                    continue
                neighbour = ny * SIZE + nx
                if scrub[neighbour] and not visited[neighbour]:
                    visited[neighbour] = True
                    stack.append(neighbour)
        if len(component) >= 20:
            for index in component:
                keep[index] = True
    return scrub, keep


def read_source(path):
    """Use ffmpeg for a quick manual art import, with a stdlib fallback."""
    ffmpeg = shutil.which('ffmpeg')
    if not ffmpeg:
        return pngread.read_rgba(path)
    width, height = pngread.read_size(path)
    result = subprocess.run(
        [ffmpeg, '-loglevel', 'error', '-i', path, '-f', 'rawvideo',
         '-pix_fmt', 'rgba', 'pipe:1'],
        check=True, stdout=subprocess.PIPE)
    raw = result.stdout
    if len(raw) != width * height * 4:
        raise ValueError(f'{path}: ffmpeg returned an unexpected byte count')
    pixels = [tuple(raw[offset:offset + 4])
              for offset in range(0, len(raw), 4)]
    return width, height, pixels


def sample_quadrant(source, index, x, y):
    width, height, pixels = source
    cell_width = width // SOURCE_COLUMNS
    cell_height = height // SOURCE_ROWS
    column = index % SOURCE_COLUMNS
    row = index // SOURCE_COLUMNS
    source_x = column * cell_width + min(
        cell_width - 1, int((x + 0.5) * cell_width / SIZE))
    source_y = row * cell_height + min(
        cell_height - 1, int((y + 0.5) * cell_height / SIZE))
    return pixels[source_y * width + source_x]


def make_metatile(source, index, forest):
    pixels = [TRANSPARENT] * (SIZE * SIZE)
    sampled = [sample_quadrant(source, index, x, y)
               for y in range(SIZE) for x in range(SIZE)]
    scrub, keep_scrub = substantial_scrub(sampled)
    for y in range(SIZE):
        for x in range(SIZE):
            pixel = sampled[y * SIZE + x]
            if is_chroma(pixel):
                continue
            # Split the reviewed composition into mountain and forest families.
            # Tiny sampled scrub fragments remain excluded from both: those
            # were the regular olive "beads" rejected in the first playtest.
            nearby_scrub = [sy * SIZE + sx
                            for sy in range(max(0, y - 1), min(SIZE, y + 2))
                            for sx in range(max(0, x - 1), min(SIZE, x + 2))
                            if scrub[sy * SIZE + sx]]
            near_foliage = any(keep_scrub[nearby] for nearby in nearby_scrub)
            if nearby_scrub and not near_foliage:
                continue
            if forest != near_foliage:
                continue
            # Runtime draws the visible pad at 24px. Preserve a two-pixel
            # margin without cutting a conspicuous square out of nearby art.
            if 50 <= x < 78 and 50 <= y < 78:
                continue
            pixels[y * SIZE + x] = (pixel[0], pixel[1], pixel[2], 255)
    return pixels


def main(out_dir=None):
    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    source_path = os.path.join(repo, 'assets', 'tiles', 'art-sources',
                               'terrain-metatiles-v2-source.png')
    source = read_source(source_path)
    if source[0] % SOURCE_COLUMNS or source[1] % SOURCE_ROWS:
        raise ValueError('metatile source must divide into a 2x2 grid')
    pixels = [TRANSPARENT] * (SIZE * COUNT * SIZE)
    for index in range(COUNT):
        source_index = index % SOURCE_VARIANTS
        forest = index >= SOURCE_VARIANTS
        metatile = [TRANSPARENT] * (SIZE * SIZE)
        placements = ((source_index, 0, 0),
                      ((source_index + 1) % SOURCE_VARIANTS, 12, -10),
                      ((source_index + 2) % SOURCE_VARIANTS, -10, 10))
        # One filtered family was too sparse at game scale. Three complementary
        # reviewed layouts, slightly offset, form a main mass plus supporting
        # details distributed across the island rather than one dense knot.
        for layout, offset_x, offset_y in placements:
            layer = make_metatile(source, layout, forest)
            for y in range(SIZE):
                target_y = y + offset_y
                if target_y < 0 or target_y >= SIZE:
                    continue
                for x in range(SIZE):
                    target_x = x + offset_x
                    if target_x < 0 or target_x >= SIZE:
                        continue
                    pixel = layer[y * SIZE + x]
                    if pixel[3]:
                        metatile[target_y * SIZE + target_x] = pixel
        for y in range(50, 78):
            for x in range(50, 78):
                metatile[y * SIZE + x] = TRANSPARENT
        for y in range(SIZE):
            for x in range(SIZE):
                pixels[y * SIZE * COUNT + index * SIZE + x] = (
                    metatile[y * SIZE + x])
    out_dir = genlib.resolve_out_dir(out_dir, 'assets', 'tiles')
    gridpng.write_rgba_png(SIZE * COUNT, SIZE, pixels,
                           os.path.join(out_dir, 'terrain_feature_atlas.png'))


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else None)
