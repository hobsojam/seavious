# TODO

Working task list. See `README.md` for the design decisions behind these
items.

## Mechanics (current focus: bare mechanical proof)

Milestone — scrolling background + player sprite + 4-directional controls:
- [x] 1. Rework skeleton orientation: internal canvas 240x320 → 512x384
      (2x to 1024x768 window), move player default position from
      bottom-center to left-middle, reclamp movement bounds to the
      512x352 play area, HUD bar placeholder carved out at the bottom
- [x] 2. Implement scrolling background: `assets/tiles/ocean.png` loaded
      with `TEXTURE_WRAP_REPEAT`, drawn in one `DrawTexturePro` call with
      an oversized source rect (real-time tiling via GPU wrap, not manual
      tile loop), offset scrolls right-to-left and wraps modulo tile width
- [x] 3. Implement player sprite rendering: `LoadTexture` the ship sprite,
      replace the placeholder `DrawTriangle` with a textured draw call;
      movement bounds now clamp to the sprite's actual 48x24 half-size
      instead of the old triangle's 8px margin; CMake copies `assets/` next
      to the built exe (POST_BUILD step) so the relative load path works
- [x] 4. Confirm 4-directional controls feel right against the new bounds
      and spawn position — play-tested on Windows, confirmed working

- [x] Forward (rightward) gun: auto-fire bullets (fixed pool, spawns from
      the nose, 0.15s interval, culled off the right edge)
- [x] Torpedo: forward-fired (Space), single-in-flight + 1.5s reload
      cooldown so it isn't unlimited-fire like the gun, travels level
      along the surface to a saved max-range reticle
- [x] One air enemy type (Skimmer Drone): spawns off the right edge, flies
      left on a sine-wave path, dies in one hit to the gun; first-pass
      sprite replaced the placeholder magenta diamond (see Art below)
- [x] One ground/surface target type: Turret Platform — spawns off the
      right edge, anchored to the water (drifts left at ocean scroll
      speed), dies to one torpedo, gun bullets pass over it (dual-targeting
      rule); placeholder amber hexagon + glow ring + cannon stub. Doesn't
      fire back yet — blocked on the lives/damage system existing
- [x] Torpedo range-reticle behavior: reticle marks max range, clamped
      before the right edge; baseline shot runs straight down the current
      surface lane, arms after a short distance, explodes on first armed
      surface hit or at max range, and only does direct impact damage before
      arming. Lead-targeting math is preserved in code for a possible future
      upgrade, but is no longer the baseline behavior
- [x] First-pass scoring: award 100 points for a Skimmer Drone and 300 for
      a Turret Platform on destruction, accumulate the run score, and show
      it as minimal text in the reserved HUD bar; wider scoring table is
      documented in README
- [ ] Lives / game-over loop (post-MVP, needed for full stage structure)
- [ ] Boss fight structure (post-MVP, end of each stage)
- [ ] Stage/wave definition + sequencing (post-MVP)
- [x] Implement HUD (reserved 512x32 bottom bar, play area 512x352):
      two reserve-life icons for the initial three-life count, score
      center-left, live torpedo ready/flight/reload status right, and space
      reserved at far right for the future boss health bar. Losing lives and
      game-over behavior remain part of the separate lives-system task

## Content

- [x] Enemy roster design (air + ground, beyond the first proof-of-concept
      pair), including per-enemy silhouette/visual description — see
      README for the full Stage 1 roster
- [ ] Implement air roster: Skimmer Drone, Interceptor, Wing Formation,
      Gunship — Interceptor sprite + behavior numbers are designed (see
      README/Art); its implementation is blocked on the lives/damage
      system because it shoots back
- [ ] Implement ground roster: Turret Platform, Relay Node, Mine, Mobile
      Platform — Relay Node and Mine sprites + behavior numbers are
      designed (see README/Art); Relay Node is implementable now (it never
      attacks directly, so it doesn't need the lives system) and is the
      natural next enemy to wire in; Mine contact damage is blocked on
      lives/damage
- [ ] Design Stage 1 boss visuals (Leviathan-class dreadnought) — deferred
      as its own task, separate from the roster above
- [ ] Implement Stage 1 boss: Leviathan-class dreadnought (gun-weak pods +
      torpedo-weak hull sections)
- [ ] Stage list + per-stage boss concepts (beyond Stage 1)
- [ ] Plan each level (enemy wave placement, pacing, terrain/visual variety
      per stage — e.g. open ocean vs. storm vs. near islands)
- [ ] Ground target sprites (alien platforms/installations) — Relay Node
      and Mine first passes done (see Art below); Turret Platform (still a
      placeholder hexagon in-game) and Mobile Platform still to design
- [ ] Air enemy sprites (drone swarms) — Skimmer Drone and Interceptor
      first passes done (see Art below); Gunship still to design

## Art

- [x] Pick art pipeline — LibreSprite, exported as .png, loaded via
      raylib's `LoadTexture`
- [x] Ocean background tile art — fourth-pass programmatic version at
      `assets/tiles/ocean.png` (128x64, seamless both axes): classic flat
      arcade water (Xevious/1942 style) — solid navy with a whisper-subtle
      darker mottle and nothing else — generated by
      `tools/gen-ocean-tile.ps1`. Passes 1-3 (dithered chevrons, swell
      bands, dappled patches) all read as fabric/stripes/pattern rather
      than water, and baked white flecks were also tried and rejected
      (landmark features make the tile repeat trackable); lessons recorded
      in the README palette section
- [x] Foam-glint parallax overlay — first pass in:
      `assets/tiles/ocean_overlay.png` (160x96, 3 dim foam marks on
      transparency, generated by `tools/gen-ocean-overlay.ps1`; reduced
      from 14 after playtest feedback) scrolls at
      1.15x base water speed, drawn above the base ocean and below all
      surface objects. Tune `OCEAN_OVERLAY_SPEED_SCALE` / mark density to
      taste after Windows playtest
- [ ] Ocean back-pocket idea if the water ever needs still more life:
      multiple edge-compatible tile variants placed by a positional hash
      (prototyped: variants sharing noise-lattice borders tile seamlessly
      in any arrangement)
- [x] Player skimmer sprite — twin-ski dart silhouette, first-pass
      programmatic version at `assets/sprites/player_ship.png` (48x24),
      wired into the game in milestone step 3 above; refining in
      LibreSprite still outstanding
- [x] Skimmer Drone sprite — dark-hull dart/diamond with magenta energy
      stripe and code-pulsed core, first-pass programmatic version at
      `assets/sprites/skimmer_drone.png` (24x16, nose left), generated by
      `tools/gen-skimmer-drone.ps1`; design rationale in the README roster
      entry; refining in LibreSprite still outstanding
- [x] Interceptor sprite — elongated swept-back-wing dart with two-tone
      magenta spine stripe and pale nose weapon core, first-pass
      programmatic version at `assets/sprites/interceptor.png` (32x16,
      nose left), generated by `tools/gen-interceptor.ps1`; not yet wired
      into the game (wiring is part of the air-roster implementation task);
      refining in LibreSprite still outstanding
- [x] Relay Node sprite — amber waterline ring, dark platform disc,
      tri-mast + pale beacon core (launch flash is code-driven), first-pass
      programmatic version at `assets/sprites/relay_node.png` (24x24),
      generated by `tools/gen-relay-node.ps1`; not yet wired into the game;
      refining in LibreSprite still outstanding
- [x] Mine sprite — dim spiked urchin with small amber core, first-pass
      programmatic version at `assets/sprites/mine.png` (14x14), generated
      by `tools/gen-mine.ps1`; not yet wired into the game; refining in
      LibreSprite still outstanding
- [x] Wake effect — first-pass particle trail: foam-colored puffs emitted
      from the two rear ski-points, anchored to the water (drift left at
      scroll speed), fade/shrink out over ~1.1s; spray and any bloom/glow
      polish stays with the VFX pass

## Audio

- [ ] Compose music (OpenMPT, XM/MOD) — first stage track, then boss/menu
      themes as those get scoped
- [ ] Define sound effects — full list of SFX the game needs (gunfire,
      torpedo launch, torpedo impact/splash, explosion, player hit/death,
      UI blips, plus whatever else content design turns up)
- [ ] Generate defined SFX in ChipTone

## Infra

- [x] Verify the CMake + vcpkg build actually compiles on Windows — builds
      and runs (first playable confirmation 2026-07-12)
- [ ] Consider reporting the raylib 6.0 bug upstream: with
      `-DCUSTOMIZE_BUILD=ON` (which the vcpkg port passes),
      `cmake/ParseConfigHeader.cmake` treats every uncommented
      `#define SUPPORT_X 0` in `config.h` as default ON, silently enabling
      `SUPPORT_CUSTOM_FRAME_CONTROL` — `EndDrawing()` then never calls
      `SwapScreenBuffer()`/`PollInputEvents()`/frame pacing, so every app
      built on vcpkg's raylib 6.0 shows a permanently blank window at
      uncapped fps. Worked around locally via `vcpkg-overlays/raylib`
      (forces the flag OFF); affects raylib upstream and the vcpkg port
