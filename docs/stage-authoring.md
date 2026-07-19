# Stage authoring

[Back to the README](../README.md) · [Game design](game-design.md) ·
[Assets and terrain guidance](assets-and-audio.md)

The editable maps are [Stage 1](../assets/stages/stage1.txt),
[Stage 2](../assets/stages/stage2.txt), and
[Stage 3](../assets/stages/stage3.txt). `tools/gen-stage-table.py` compiles
each `assets/stages/stageN.txt` it finds into `src/stageN_data.c`; commit
both the source map and its generated table together.

```bash
python3 tools/gen-stage-table.py src
python3 tests/test_stage_table.py
```

## Map format

A map consists of `@<scroll-pixels>` blocks followed by exactly eleven grid
rows. One cell is 32 pixels. Columns represent scroll distance and rows are
top-to-bottom lanes in the 352-pixel play area. Blocks must be contiguous.
Lines beginning with `;` are comments; `#` is terrain, not a comment.

| Glyph | Meaning |
| --- | --- |
| `d`, `w`, `W` | Skimmer Drone solo, line of three, V of five |
| `i`, `G` | Interceptor, Gunship |
| `C`, `T`, `R`, `m`, `P` | Casemate, Tracking Turret, Relay Node, Mine, Mobile Platform |
| `B`, `K` | Mortar Battery, Drone Bunker; each also creates terrain and a hardpoint |
| `#` | Terrain footprint |
| `H` | Explicit terrain hardpoint; use only where something actually mounts |
| `~` | Rogue Wave (Stage 3): dodge-only hazard, no target of any kind |

Events are sorted by scroll distance and played once by `src/stage.c`. Terrain
rectangles are grouped and rendered from that same compiled data, so map
authoring controls both collision and presentation.

## Terrain and hardpoints

Use multi-cell terrain for land installations and inset them from the
coastline. A single-cell islet makes a building appear to overhang the beach.
Use `B` or `K` for an installed pad; an empty `H` should be exceptional, not
a visual filler.

Joined terrain should read as one island, not as copied sprites touching.
The renderer groups touching footprints and composes overlapping,
aspect-matched islet variants rather than stretching one small component over
the whole group. Check a map change in-game at the actual 512×384 canvas,
including chains and overlaps.

## Adding a stage

The stage descriptor in `src/stage.c` is the runtime registration point. Add
the map, generate its table, expose its generated symbols in `stage_data.h`,
and add the descriptor row with its length and award. Update the stage tests
at the same time; `MAX_STAGE_TERRAIN_RECTS` is a deliberate safety limit for
the renderer's fixed scratch storage.
