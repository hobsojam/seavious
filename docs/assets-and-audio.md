# Assets and audio

[Back to the README](../README.md) · [Stage authoring](stage-authoring.md) ·
[Development guide](development.md)

Runtime assets live under `assets/` and are copied beside the executable by
CMake. Most source generation scripts are in `tools/`; keep a generated asset
and the script that made it in sync.

## Terrain art direction

`assets/sprites/stage1_islet.png` and `stage1_islet_ridge.png` are the visual
anchors for terrain. New components should use their apparent pixel density:
rugged olive/ochre ground, clustered brown-grey crags, scrub and scattered
stones, a narrow irregular golden beach, and a consistently thin cyan-white
surf line.

Give each asset a distinct, natural silhouette and feature arrangement. Avoid
broad gradients, sparse or mostly sandy interiors, oversized shore bands,
symmetry, and narrow joins that make one island look glued from two sprites.
Review it at game scale beside at least two existing islets, both alone and in
a joined terrain group. The current set contains compact, crag, crescent,
atoll, ridge, and double-length variants.

Hardpoint pads are functional markers, not generic decoration: they belong
only below an actual land installation. Keep their stonework, shadow, and
scale coherent with the surrounding island.

## Audio

Music is authored as XM modules. Stage 1 and Stage 2 each have a stage and a
boss theme; the title and game-over screen use their own existing cues. Music
changes are hard cuts at title, boss, and game-over state changes rather than
live layers or crossfades. SFX sources live in `assets/audio/sfx/` and their
mix is centralized in `src/audio.c`.

Regenerate and verify derived audio with:

```bash
python3 tests/test_xm_assets.py
python3 tests/test_sfx_assets.py
```

## Asset checks

```bash
python3 tests/test_sprite_assets.py
python3 tests/test_icon_asset.py
```

These tests catch malformed or stale generated files. They complement, rather
than replace, an in-game visual and mix check.
