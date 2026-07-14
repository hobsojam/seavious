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
- [x] Two ground/surface target types: Casemate and Tracking Turret — both
      spawn off the right edge, are anchored to the water (drift left at
      ocean scroll speed), die to one torpedo, and ignore gun bullets
      (dual-targeting rule). The Casemate fires straight left; the Tracking
      Turret leads current player movement before firing.
- [x] Torpedo range-reticle behavior: reticle marks max range, clamped
      before the right edge; baseline shot runs straight down the current
      surface lane, arms after a short distance, explodes on first armed
      surface hit or at max range, and only does direct impact damage before
      arming. Lead-targeting math is preserved in code for a possible future
      upgrade, but is no longer the baseline behavior
- [x] First-pass scoring: award 100 points for a Skimmer Drone, 300 for a
      Casemate, and 400 for a Tracking Turret on destruction; accumulate the run score, and show
      it as minimal text in the reserved HUD bar; wider scoring table is
      documented in README
- [x] Lives / game-over loop (post-MVP, needed for full stage structure):
      enemy contact now costs one life, plays a short player explosion before
      respawning with brief invulnerability, and ends the run after the final
      death effect; full checkpoint structure still belongs with the later
      stage system
- [x] Destruction effects: air targets burst briefly then disappear; destroyed
      surface targets leave inert burnt-out wrecks that drift with the water
- [ ] Boss fight structure (post-MVP, end of each stage)
- [x] Stage/wave definition + sequencing — implemented:
      `tools/gen-stage-table.py` compiles `assets/stages/stage1.txt` into
      the committed `src/stage1_data.c` event table (drift-tested by
      `tests/test_stage_table.py`); `src/stage.c` walks the sorted table
      with a cursor as scroll distance accumulates, fires wave patterns
      (lone drone / line of 3 / V of 5 live in `gameplay.c`), raises the
      boss lock at map end (freezes scroll, water-anchored drift, and
      spawns; raises `bossActive` as a placeholder until the boss fight
      owns it), and replaces the old random spawn timers. Unimplemented
      roster glyphs (i/G/R/m/P) compile into the table and are skipped
      until each enemy lands; terrain footprints (`#`) compile in for
      the future terrain system. Restart rewinds the script. Unit-tested
      in `tests/stage_tests.c`; needs a Windows playtest for beat
      pacing/feel
- [ ] Terrain system: stage-data land footprints drifting at scroll speed —
      non-colliding for ship/gun, blocks torpedoes (armed = detonate at
      land edge, unarmed = fizzle), reticle clamps to the first land edge
      in the lane; ground enemies can anchor to terrain features
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
- [ ] Implement remaining ground roster: Mine and Mobile Platform (both
      designed, see README/Art; the lives system exists so neither is
      blocked). Relay Node is done: spawns from its stage glyph, drifts
      with the water, launches a Skimmer Drone from its own position
      every 2.5s while fully on-screen (capped at 3 of its drones alive;
      a freed slot refills immediately), never attacks directly, dies to
      one torpedo for 400 points leaving its drones alive, code-driven
      beacon flash + broadcast-warble SFX on launch
- [x] Design Stage 1 boss visuals (Leviathan-class dreadnought) — fight
      design done (see README "Stage 1 boss design": part layout with 2
      AA pods + 2 hull sections + indestructible mortar turret + hidden
      core, HP/scoring numbers, emergent fight decay, entrance/defeat/
      salvage sequence) and first-pass sprite set generated at
      `assets/sprites/leviathan_*.png` by `tools/gen-leviathan.py`
      (breached hull base, pod, hull section, faction-colorless mortar
      dome, white-hot core); mortar shell/shadow/blast stay code-drawn
      VFX and part-death scorch reuses the wreck treatment; refining in
      LibreSprite still outstanding; now includes the
      armored (indestructible) mortar turret that lobs arcing shells during
      the fight and is salvaged intact afterward (see README boss entry)
- [ ] Implement Stage 1 boss: Leviathan-class dreadnought (gun-weak pods +
      torpedo-weak hull sections, plus the armored mortar turret hazard)
      and the end-of-stage salvage sequence that awards the mortar
- [ ] Scavenged mortar weapon (Stage 2+, see README Core mechanic): lobbed
      arc over land blockers, shorter fixed-range in-lane reticle that
      ignores land edges, area blast on landing, strict land-only damage
      class, own fire key + cooldown + green HUD status icon — blocked on
      the Stage 1 boss (it's the salvage reward)
- [ ] Design Stage 2 land-target roster (green mortar-class installations
      built on terrain) — the Stage 2 counterpart of the Stage 1 roster
      task above
- [ ] Stage list + per-stage boss concepts (beyond Stage 1)
- [ ] Plan each level (enemy wave placement, pacing, terrain/visual variety
      per stage — e.g. open ocean vs. storm vs. near islands) — Stage 1
      beat chart designed (10 teaching beats, ~3 min at 40 px/s ≈ 7,200 px;
      see README Level & stage design) and its first map draft committed
      at `assets/stages/stage1.txt`; expect placement/density tuning once
      the wave-script system makes it playable
- [x] Ground target sprites (alien platforms/installations) — Relay Node,
      Mine, Casemate, Tracking Turret, and Mobile Platform first passes
      all done (see Art below). Land-based emplacement variants
      deliberately deferred to the Stage 2 mortar-target class (see
      README roster note) rather than reskins of the water designs
- [ ] Island/islet terrain art (Stage 1: sparse islets on open ocean) —
      needs the terrain-footprint system from Mechanics before it's more
      than scenery
- [x] Air enemy sprites (drone swarms) — Skimmer Drone, Interceptor, and
      Gunship first passes all done (see Art below)

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
- [x] Casemate sprite — hex plate with baked fixed left barrel, amber
      waterline ring, pale muzzle + emitter core, first-pass programmatic
      version at `assets/sprites/casemate.png` (24x24, top-down),
      generated by `tools/gen-casemate.py` and wired into the game
      (replaces the placeholder hexagon entirely — nothing code-drawn);
      refining in LibreSprite still outstanding
- [x] Tracking Turret sprite — circular deck with bearing bolt ring and
      pivot hub, same waterline ring, first-pass programmatic version at
      `assets/sprites/tracking_turret.png` (24x24, top-down), generated
      by `tools/gen-tracking-turret.py` and wired into the game (the
      aiming barrel stays code-drawn so it can lead the player);
      refining in LibreSprite still outstanding
- [x] Mobile Platform sprite — vessel-style barge with hull-hugging amber
      waterline, pointed bow left, rust deckhouse, three pale edge
      emitters, first-pass programmatic version at
      `assets/sprites/mobile_platform.png` (36x18), generated by
      `tools/gen-mobile-platform.py`; behavior numbers designed (see
      README roster); not yet wired into the game (part of the
      ground-roster implementation task); refining in LibreSprite still
      outstanding
- [x] Gunship sprite — twin-hull catamaran frame with per-hull magenta
      spine stripes and three pale emitter cores (hull noses + deck
      leading edge), first-pass programmatic version at
      `assets/sprites/gunship.png` (32x24, noses left), generated by
      `tools/gen-gunship.py`; behavior numbers designed (see README
      roster); not yet wired into the game (part of the air-roster
      implementation task); refining in LibreSprite still outstanding
- [x] Wake effect — first-pass particle trail: foam-colored puffs emitted
      from the two rear ski-points, anchored to the water (drift left at
      scroll speed), fade/shrink out over ~1.1s; spray and any bloom/glow
      polish stays with the VFX pass

## Audio

- [x] Build the shared drum+bass template: 2-bar drum loop (kick on beats
      1/3, snare backbeat with a 16th-note fill every 2nd bar, 8th-note
      hihats) + 4-bar A-major walking bass loop (`I–vi–IV–V`, root/3rd/5th
      with a passing tone into each next root) — see README Music structure.
      First-pass programmatic version at `assets/audio/stage1_drums_bass.xm`
      (152 BPM, 4/4, linear frequency table), generated by
      `tools/gen-stage1-xm.py` since OpenMPT itself isn't reachable from
      this dev environment; structurally validated byte-for-byte but not
      yet heard — auditioning/refining in OpenMPT (tone/levels/panning,
      maybe a less synthetic kick/snare) still outstanding
- [x] Compose first stage lead tune (8 bars) over the template — two
      candidates generated and auditioned; Theme A ("anthem",
      `assets/audio/stage1_theme_a.xm`) picked as the stage 1 theme and
      wired in. Theme B ("driver", `assets/audio/stage1_theme_b.xm`) is
      kept in the repo, reserved for future modal screens (menus,
      high-score entry) — wire it when those screens exist
- [x] Wire music into the game: Theme A loops during gameplay, hard-cut
      to the "siren" lament (`boss1_theme_a.xm`) on the game-over screen,
      back to Theme A on restart, with the "hammer" boss theme loaded and
      selected whenever the future `bossActive` flag goes true; guarded so
      headless CI runs (no audio device) play nothing instead of crashing
      — needs a Windows build/playtest to confirm audio comes through
- [x] Build the boss variant: minor backing done
      (`assets/audio/boss1_drums_bass.xm`: identical drums and bass
      contour, only the color notes flattened to `i–VI–iv–v`); two boss
      tunes auditioned and both kept — "hammer" (`boss1_theme_b.xm`,
      relentless straight-8ths grind) picked as the boss theme and wired
      behind a `bossActive` flag in `main.c` that the boss fight
      structure will drive when it exists; "siren" (`boss1_theme_a.xm`,
      high wailing descent) repurposed as the game-over lament, replacing
      the backing-only template there (per-life deaths at 0.6s are too
      short for a music cut, so only the final death swaps the track)
- [ ] Compose menu theme
- [x] Define sound effects — seven for the current game: gun shot,
      torpedo launch, torpedo splash (unarmed hit), explosion (armed
      boom), air pop (drone kill), player death, UI blip (restart);
      surface-kill sound deliberately omitted (always inside an
      explosion); more will come with new enemies (Interceptor shot,
      Relay Node launch, Mine detonation) and menu screens
- [x] First-pass SFX generated programmatically (`tools/gen-sfx.py` →
      `assets/audio/sfx/*.wav`, validated by `tests/test_sfx_assets.py`)
      and wired through the GameEventQueue in `audio.c`; volume balance
      and sound character need a Windows playtest — refine individual
      sounds in ChipTone if the synth versions fall short

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
