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
- [ ] 4. Confirm 4-directional controls feel right against the new bounds
      and spawn position (needs an actual play-test on Windows)

- [x] Forward (rightward) gun: auto-fire bullets (fixed pool, spawns from
      the nose, 0.15s interval, culled off the right edge)
- [ ] Torpedo: forward-fired (rightward) on second input, travels along
      the surface, lead-targeting logic for ground/surface targets
- [x] One air enemy type (Skimmer Drone): spawns off the right edge, flies
      left on a sine-wave path, dies in one hit to the gun, placeholder
      magenta diamond silhouette (real sprite art still a separate TODO
      item, see Content/Art below)
- [ ] One ground/surface target type (alien platform/installation)
- [ ] Lives / game-over loop (post-MVP, needed for full stage structure)
- [ ] Boss fight structure (post-MVP, end of each stage)
- [ ] Stage/wave definition + sequencing (post-MVP)
- [ ] Implement HUD (reserved 512x32 bottom bar, play area 512x352):
      lives icons left, score center-left, torpedo status right, boss
      health bar far right (boss fights only) — see README for full layout

## Content

- [x] Enemy roster design (air + ground, beyond the first proof-of-concept
      pair), including per-enemy silhouette/visual description — see
      README for the full Stage 1 roster
- [ ] Implement air roster: Skimmer Drone, Interceptor, Wing Formation,
      Gunship
- [ ] Implement ground roster: Turret Platform, Relay Node, Mine, Mobile
      Platform
- [ ] Design Stage 1 boss visuals (Leviathan-class dreadnought) — deferred
      as its own task, separate from the roster above
- [ ] Implement Stage 1 boss: Leviathan-class dreadnought (gun-weak pods +
      torpedo-weak hull sections)
- [ ] Stage list + per-stage boss concepts (beyond Stage 1)
- [ ] Plan each level (enemy wave placement, pacing, terrain/visual variety
      per stage — e.g. open ocean vs. storm vs. near islands)
- [ ] Ground target sprites (alien platforms/installations)
- [ ] Air enemy sprites (drone swarms)

## Art

- [x] Pick art pipeline — LibreSprite, exported as .png, loaded via
      raylib's `LoadTexture`
- [x] Ocean background tile art — first-pass programmatic version at
      `assets/tiles/ocean.png` (32x32, seamless both axes, Bayer-dithered
      bands + dashed foam crests); wired into the game loop in milestone
      step 2 above
- [x] Player skimmer sprite — twin-ski dart silhouette, first-pass
      programmatic version at `assets/sprites/player_ship.png` (48x24),
      wired into the game in milestone step 3 above; refining in
      LibreSprite and wake/spray effect (deferred to the VFX pass) still
      outstanding

## Audio

- [ ] Compose music (OpenMPT, XM/MOD) — first stage track, then boss/menu
      themes as those get scoped
- [ ] Define sound effects — full list of SFX the game needs (gunfire,
      torpedo launch, torpedo impact/splash, explosion, player hit/death,
      UI blips, plus whatever else content design turns up)
- [ ] Generate defined SFX in ChipTone

## Infra

- [ ] Verify the CMake + vcpkg build actually compiles on Windows (untested
      so far — no compiler available in the WSL dev environment)
- [ ] Decide whether/when this repo gets a GitHub remote (relevant if we
      ever move task tracking to Issues, or just want backups/collaborators)
