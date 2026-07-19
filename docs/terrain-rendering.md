# Terrain rendering: experiments and direction

[Back to the README](../README.md) · [Stage authoring](stage-authoring.md) ·
[Assets and audio](assets-and-audio.md)

This is the decision record for reusable terrain. Read it before changing the
terrain renderer or generating another atlas: several plausible approaches
have already been tested in-game and produced the same problems in different
forms.

The fixed, one-off Fortress Atoll may continue using a dedicated sprite. This
document concerns reusable islands, banks, coves, and straits.

## Non-negotiable visual and gameplay constraints

- Work at native pixel resolution. Do not resize a small island or shoreline
  to cover a larger footprint; the resulting pixel density does not match the
  player, weapons, or established island art.
- Use `stage1_islet.png` and `stage1_islet_ridge.png` as the style reference:
  irregular golden beach, thin cyan-white surf, olive/ochre ground, clustered
  crags and scrub, and a natural asymmetric silhouette.
- Rendered land and collision must come from the same authored geometry. A
  torpedo must not hit apparent water, and a land target must not sit at sea.
- Keep a solid 64px mounting area around every B/K hardpoint. Hardpoint art and
  the installed weapon must share the exact same centre.
- Joined pieces must read as one landform. Avoid visible seams, repeated edge
  motifs, one-cell fingers, and shapes that look like rectangles glued
  together.
- Review at the actual 512×384 game canvas. A technically seamless atlas can
  still look mechanical at game scale.

## What has been tried

### Resized component sprites

Stretching existing island components covers arbitrary rectangles, but it
changes their apparent pixel size and distorts beaches, rocks, and mountains.
Large footprints look soft or coarse beside native-resolution sprites. This
remains unsuitable for reusable terrain.

### Mirrored island variants

Horizontal mirroring can provide limited variation. Vertical mirroring makes
directional features—especially mountains, shadows, and beaches—look upside
down. Do not treat arbitrary flips as free variants; create intentional art or
restrict transformations to assets designed to support them.

### Overlapping complete island components

Placing painted islands together can hide some stretching, but their
shorelines and feature densities rarely agree at the join. The result reads as
several sprites pasted together and makes reliable collision awkward.

### Binary square autotiles

A native 32px atlas selected by north/east/south/west land neighbours removed
internal beach seams. It also exposed the underlying grid: shores became long
straight lines and the land read as rectangular platforms.

Subdividing the authored geometry to 16px improved contour resolution and
kept collision aligned. It did not solve the fundamental limitation: a binary
edge is still either entirely coast or entirely joined, so large banks remain
box-like.

### Interior texture and feature overlays

Per-tile grass, stones, and scrub quickly became repeated visual noise. Making
them sparse left large empty fields. Larger 64px feature stamps are more
promising, but decoration cannot rescue a mechanical silhouette. Establish
the coastline first, then place a small number of cross-tile crag/scrub
clusters away from hardpoints.

### Automatic coastline carving

Randomly removing 16px edge cells produced square notches, thin fingers,
detached fragments, and artificial holes. Symmetric procedural coves were
cleaner but still looked cut from rectangular platforms. Do not repeat this as
the primary silhouette generator. Authored coastline intent must be present in
the tile selection or terrain mask.

### Separate Stage 2 terrain mask

Separating Stage 2's 16px terrain/collision mask from its 32px encounter map is
useful and should be retained: coastlines can change without moving scripted
events. A compiler bug initially emitted the 16px row indices at a 32px pitch,
placing land targets in apparent water. The generator and regression test now
verify that every emitted B/K hardpoint remains covered.

## Agreed direction: shoreline interface tiles

The next atlas should treat a shoreline as a path through a tile, not as a
straight tile boundary. This is a Wang-tile-style interface contract.

Each tile edge receives a code describing how terrain meets that edge:

| Edge interface | Meaning |
| --- | --- |
| `water` | The complete edge is water |
| `land` | The complete edge is land |
| `cross-1`, `cross-2`, `cross-3` | Shoreline crosses at roughly 1/4, 1/2, or 3/4 along the edge |

A crossing also needs polarity: which side of the crossing is land. Adjacent
tiles are compatible only when their shared edge codes agree after accounting
for opposite edge orientation. The artwork inside a compatible tile may then
connect its edge interfaces with a curve, shallow bay, headland, diagonal, or
S-bend. Several art variants may share the same four interface codes.

This allows a coastline to move inward and outward across several tiles while
every join remains exact. It is more expressive than the current binary
north/east/south/west mask and avoids requiring a perfectly straight shore at
every tile boundary.

## Suggested implementation sequence

1. Define and test edge-interface compatibility independently of rendering.
2. Make a small diagnostic atlas using `water`, `land`, and centre crossings;
   draw interface IDs in a test scene so incorrect joins are obvious.
3. Add quarter and three-quarter crossings plus polarity once the selector is
   reliable.
4. Replace diagnostics with native-pixel shore art and add two or three visual
   variants per commonly used interface combination.
5. Derive or validate the collision mask from the chosen tile geometry.
6. Add large, sparse terrain feature stamps only after coastline screenshots
   look natural across islets, long banks, concave coves, and straits.

The current three-level vertex prototype is compact enough to precompute in
full. If it grows into explicit crossing positions, polarities, and artwork
variants, do not build that larger combinatorial atlas upfront. Start with the
combinations required by one representative Stage 2 encounter, validate
missing combinations, and expand from observed needs.

## Prototype history and current implementation

The first Wang prototype used three shoreline depths at each 16px grid vertex.
It proved exact joins, but the underlying binary cell outline remained visible:
the islands looked like cogs or clouds with one square lobe per collision cell.
Shared endpoints alone were not enough to produce a broad curve.

The second cut used 32px visual tiles. Four shared corner states define
the tile topology, and each N/E/S/W edge has an explicit crossing at 1/4, 1/2,
or 3/4. A deterministic hash of each world-grid edge selected that interface,
so both incident tiles agreed. Inside the tile, a Coons-style pixel field connects
the four independently authored edges into a continuous curve. Field bands
produce ground, beach, foam, wash, and transparency, which makes the complete
shore treatment—not only its alpha silhouette—continuous at each seam.

This proved tessellation, but independent edge hashes were visually wrong:
neighbouring crossings frequently jumped from 1/4 to 3/4, producing pointed
fish-tail headlands, pinched waists, and star-like islands. Broad, uniform
field thresholds also made the cyan surf and golden beach read as heavy
ribbons around a large, empty olive fill.

The current third cut retains the tested interface contract while selecting
crossings from a low-frequency world-space noise field. Interfaces now drift
coherently over several tiles, and the three crossing positions are pulled
closer to the centre. Shore bands are narrower, while larger rock and scrub
stamps are evaluated on a world-aligned 64px grid rather than per 16px terrain
run. This preserves exact joins without asking random per-edge variation to
create the whole island silhouette.

The 16px Stage 2 mask remains the collision and authoring source. A majority
sample of its four cells around each 32px visual-grid vertex supplies the
shared corner states, smoothing one-cell teeth without moving encounter or
hardpoint coordinates. Asset tests exercise horizontal and vertical joins for
all three crossing positions.

This remains a prototype. Its next evaluation is visual: check whether the
coherent interfaces remove the pointed outline without making collision and
apparent shore diverge noticeably. Multiple art variants for a given
interface tuple should wait until that geometry is accepted.

## Painted tile workflow

`tools/export-terrain-art-templates.py` ranks the coast signatures actually
used by Stage 2 and exports the twelve most common as an enlarged art-task
sheet with a Markdown edge-contract brief. The first reviewed sheet lives in
`assets/tiles/art-sources/`; it is source material, not a runtime texture.

The first in-game import applied the sheet's painted ground inside selected
coast tiles. Although the outer edge contract remained intact, those tiles
appeared as rectangular moss patches beside generated/full-land neighbours.
Small amounts of black preview water also became dark opaque wedges where the
art draft drifted inside the authoritative alpha mask. That runtime import was
removed. Its source sheet and templates remain as an experiment record; do not
repeat the approach. Ground, crags, scrub, and soil variation must use larger
metatiles so they cross the 32px grid and cannot reveal which coastline
signature was selected.

Rotation and mirroring should reduce the final authored set, but only for
direction-neutral coast material. Transform the mask and all four edge
interfaces together and test the transformed seams. Do not freely rotate or
vertically mirror mountains, cast shadows, or other directional scenery;
those belong in intentionally authored metatile variants.

## Terrain metatiles

Stage 2 uses the Wang atlas for topology, shoreline, and ground. Each shoreline
signature has four phase variants: the four 32px quarters of the seamless 64px
`terrain_ground_tile.png`. Runtime selects the quarter from the parity of the
world-space Wang-grid coordinate. The ground texture therefore continues over
tile boundaries, but remains baked inside the Wang shoreline silhouette; there
are no rectangular interior overlays that can expose the grid.

`terrain_ground.png` remains the reviewed high-resolution source. Run
`python3 tools/gen-terrain-ground-tile.py` only when deliberately re-importing
that art, then regenerate the Wang atlas. The importer reduces it to native
pixel scale and reconciles only the opposite edge pixels. The
committed 64px tile keeps routine atlas generation deterministic and fast.

The inland part of every coastal Wang tile samples that same phased texture.
Its opaque silhouette and interface positions remain unchanged, while the sand
band is wider and mottled and the foam and cyan wash use broken colour patterns.
This prevents the shoreline from reading as uniform yellow and cyan ribbons
without weakening the exact edge contract.

A separate reviewed atlas, `assets/tiles/terrain_feature_atlas.png`, contains
four transparent 128px mountain compositions and four forest compositions.
A stable hash of each Stage 2 hardpoint position chooses a family and variant,
so an island remains visually identical every run without every island sharing
the same scenery. Each variant combines three offset, complementary reviewed layouts so
the selected family forms a main visual mass plus supporting detail instead of
one isolated object. Rendering centres the composition on the installation before
drawing the hardpoint itself. Every variant has a transparent 28px central pad:
the visible mounting pad is 24px, leaving a two-pixel safety margin.
Forest-family islands also receive the larger reviewed
`terrain_brush_cluster.png` at a hashed side of the installation. A smaller
fraction of either family receives `terrain_tide_pool.png` on the opposite
side. These are stable per-island landmarks, not per-tile noise, and both are
positioned outside the hardpoint clearance area.

The current high-resolution 2x2 art sheet is retained at
`assets/tiles/art-sources/terrain-metatiles-v2-source.png`; `v1` records the
rejected dense import whose magenta edge pixels survived in game. Run
`python3 tools/gen-terrain-feature-atlas.py` after deliberately changing that
source. The importer removes the magenta matte, samples each composition at
native resolution, rejects isolated olive specks that collapsed into repeated
beads during the first Windows playtest, preserves only substantial asymmetric
foliage clusters, and restores the clear hardpoint pad. The importer uses `ffmpeg` when
available for fast decoding and has a Python standard-library fallback, but
importing the large source is intentionally not part of routine tests. Tests
validate the committed native atlas's dimensions, transparency, palette
families, and all four central pads instead.

Metatiles may cross visual tile boundaries, but they must remain transparent
outside their feature mass. Use rotation or horizontal mirroring only for
direction-neutral scrub and stones. Ridges, cliffs, lighting, and cast shadows
need intentional variants. In-game review must check that a metatile neither
spills visibly into water nor makes the collision shoreline appear to move.
