# Stage authoring

[Back to the README](../README.md) · [Game design](game-design.md) ·
[Assets and terrain guidance](assets-and-audio.md) ·
[Terrain rendering decision record](terrain-rendering.md)

The editable encounter maps are [Stage 1](../assets/stages/stage1.txt) and
[Stage 2](../assets/stages/stage2.txt). Stage 2 also has a dedicated
[16px terrain mask](../assets/stages/stage2_terrain.txt), so its coastline can
be shaped independently of encounter timing. `tools/gen-stage-table.py`
compiles them into `src/stage1_data.c` and `src/stage2_data.c`; commit sources
and their generated table together.

```bash
python3 tools/gen-stage-table.py
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

Events are sorted by scroll distance and played once by `src/stage.c`. Stage 1
terrain comes directly from this map. Stage 2's glyph map controls events and
hardpoint locations, while `stage2_terrain.txt` controls collision and the
rendered shoreline.

## Terrain and hardpoints

Use multi-cell terrain for land installations and inset them from the
coastline. A single-cell islet makes a building appear to overhang the beach.
Use `B` or `K` for an installed pad; an empty `H` should be exceptional, not
a visual filler.

For Stage 2, use `stage2_terrain.txt`: ten `@<px>` blocks of 22 rows, with
48 `.`/`#` cells per row. Each cell is 16px. Carve coves, rounded headlands,
and narrow joins there without altering spawns. Every `B`/`K` keeps a required
64px terrain pad, so retain the solid area around installations. Check a map
change in-game at the actual 512×384 canvas.

## Adding a stage

The stage descriptor in `src/stage.c` is the runtime registration point. Add
the map, generate its table, expose its generated symbols in `stage_data.h`,
and add the descriptor row with its length and award. Update the stage tests
at the same time; `MAX_STAGE_TERRAIN_RECTS` is a deliberate safety limit for
the compiled terrain table.
