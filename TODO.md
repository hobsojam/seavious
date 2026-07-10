# TODO

Working task list. See `README.md` for the design decisions behind these
items.

## Mechanics (current focus: bare mechanical proof)

- [ ] Rework skeleton from vertical to horizontal scroll (pivot decided —
      top-down camera kept, scroll axis rotates to left-to-right, Uridium-
      style not Scramble-style): switch internal resolution from the
      240x320 portrait placeholder to 512x384 (2x upscale to a 1024x768
      window), flip starfield/water scroll axis, move player toward the
      right edge instead of the bottom
- [ ] Forward (rightward) gun: auto-fire, hits air targets
- [ ] Torpedo: forward-fired (rightward) on second input, travels along
      the surface, lead-targeting logic for ground/surface targets
- [ ] One air enemy type (sleek mechanical alien drone)
- [ ] One ground/surface target type (alien platform/installation)
- [ ] Lives / game-over loop (post-MVP, needed for full stage structure)
- [ ] Boss fight structure (post-MVP, end of each stage)
- [ ] Stage/wave definition + sequencing (post-MVP)
- [ ] Implement HUD (reserved 512x32 bottom bar, play area 512x352):
      lives icons left, score center-left, torpedo status right, boss
      health bar far right (boss fights only) — see README for full layout

## Content

- [ ] Enemy roster design (air + ground, beyond the first proof-of-concept
      pair)
- [ ] Stage list + per-stage boss concepts
- [ ] Plan each level (enemy wave placement, pacing, terrain/visual variety
      per stage — e.g. open ocean vs. storm vs. near islands)
- [ ] Player skimmer sprite + wake/spray effect
- [ ] Ground target sprites (alien platforms/installations)
- [ ] Air enemy sprites (drone swarms)

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
