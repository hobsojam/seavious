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
- [x] Shared enemy activation line: every acting enemy (Casemate, Tracking
      Turret, Relay Node, Mobile Platform, Gunship) approaches fully
      pre-armed and takes its first action right at 10% into the play
      area (`ENEMY_ACTIVATION_X`) — playtest found the old "fully
      on-screen plus a first full interval" gating let enemies get far
      too deep before reacting; the Interceptor's mid-screen sniper
      trigger stays the deliberate exception
- [x] Boss fight structure (post-MVP, end of each stage) — landed with the
      Stage 1 boss: the stage script raises the boss lock, `src/boss.c`
      owns the fight from there (entrance, part-driven fight, death
      chain, salvage, stage-clear overlay) including the `bossActive`
      music flag; stage clear now advances the run through the stage
      descriptor (wrapping to Stage 1 until Stage 2 content ships)
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
- [x] Terrain system: stage-data land footprints drifting at scroll speed —
      non-colliding for ship/gun, blocks torpedoes (armed = detonate at
      land edge with splash that can still catch shoreline targets,
      unarmed = fizzle), reticle clamps to the first land edge in the
      lane. No entity pool: footprint screen rects derive purely from
      scroll distance, so the boss lock freezes land for free and ground
      enemies anchored to islets (beat 7's Relay Node) just work by map
      placement. Deferred wreck question decided: wrecks stay inert and
      do NOT block torpedoes (see README Structure for the rationale).
      Second-pass code-drawn islet look (playtest: the flat rectangles
      read too square): organic coastline with concentric rings —
      shallows, surf foam, wet beach around every waterline, dry
      interior with sparse grain; footprint seams read as coves. Needs
      a Windows playtest for readability/colors
- [x] Implement HUD (reserved 512x32 bottom bar, play area 512x352):
      two reserve-life icons for the initial three-life count, score
      center-left, live torpedo ready/flight/reload status right, and space
      reserved at far right for the future boss health bar. Losing lives and
      game-over behavior remain part of the separate lives-system task

## Content

- [x] Enemy roster design (air + ground, beyond the first proof-of-concept
      pair), including per-enemy silhouette/visual description — see
      README for the full Stage 1 roster
- [x] Implement air roster: Interceptor, Gunship — both spawn from their
      stage glyphs (`i`/`G`) now:
      - Interceptor: 140 px/s straight lane flight, 1 HP, fires one shot
        aimed at the player at mid-screen at
        double the shared projectile speed (playtest: the original
        straight 1x lane shot at two-thirds crossed read as meaningless),
        200 points
      - Gunship: 70 px/s straight lane flight, 3 gun HP (first multi-hit
        air target), 3-way spread aimed at the player (±16°) every 2.4s
        (first spread at the shared activation line),
        500 points, bigger destruction burst
      - the shared enemy shot now draws as the designed red diamond with
        a white-hot center (was a placeholder circle), kept at 160 px/s
        (README roster note)
- [x] Implement remaining ground roster — all three done:
      - Relay Node: spawns from its stage glyph, drifts with the water,
        launches a Skimmer Drone from its own position every 2.5s once
        past the activation line (capped at 3 of its drones alive; a freed
        slot refills immediately), never attacks directly, dies to one torpedo
        for 400 points leaving its drones alive, code-driven beacon flash
        + broadcast-warble SFX on launch
      - Mine: water-anchored, never shoots, dies to one torpedo for 100;
        player contact detonates it (costs the ship, scores nothing, own
        sharp-crack SFX); either death is a blast with no wreck; a
        respawning (invulnerable) player passes over mines harmlessly
      - Mobile Platform: self-propelled at 60 px/s (frozen by the boss
        lock with the rest of the water), 2 torpedo HP, 3-shot fan aimed
        at the player (±14°) every 3.0s — first fan at the shared
        activation line — 500
        points, stern wake reusing the player's wake pool
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
- [x] Implement Stage 1 boss: Leviathan-class dreadnought (gun-weak pods +
      torpedo-weak hull sections, plus the armored mortar turret hazard)
      and the end-of-stage salvage sequence that awards the mortar —
      implemented per the README design (`src/boss.c`, unit-tested in
      `tests/boss_tests.c`): 2 pods (12 gun hits), 2 hull sections
      (2 torpedoes), hidden core (4 torpedoes / 24 gun hits, mixing
      works), staggered part fire that decays as parts die, arcing
      mortar shells with shadow dodge-windows (cadence 4.0s→2.8s once
      the core shows), lethal hull contact while the boss lives, boss
      HP bar in the reserved HUD slot, death chain → wreck settle →
      autopilot salvage → STAGE 1 CLEAR overlay. Playtest revision
      (2026-07-15): the parked broadside let torpedoes fly through the
      ship to reach far-side parts — the hull is now solid armor
      (blocks torpedoes under the land rules, reticle clamps to it)
      and the ship patrols vertically with a 180° turn at each end, so
      each heading exposes exactly one broadside hull section and
      killing both takes at least one full turn; blowing both tears
      the breach open as the sole torpedo lane to the core. Second
      revision: the hull sections are SAM batteries (the player is a
      flyer, so surface-to-air is the right class, and open launch
      cells are the literal gaps in the armor) — the facing battery
      launches gun-shootable homing missiles (130 px/s, 140°/s turn
      cap, 4s fuel, 50 pts for a shootdown) instead of straight lane
      shots. Needs another Windows playtest for pacing (sail speed,
      turn time, missile turn rate/fuel).
      Placeholder SFX: mortar lob
      reuses torpedo-launch, salvage jingle reuses the UI blip (see
      Audio tasks)
- [x] Scavenged mortar weapon (see README Core mechanic) — first pass in:
      lobbed arc that ignores land/armor blockers, shorter fixed-range
      green reticle (120px), area blast on landing, Left Shift/X fire key,
      2.5s cooldown, green HUD dome icon, dome sprite riding the ship.
      The salvage sets `hasMortar`; a stage-clear restart carries it into
      the replay (the stand-in for Stage 2), game over forfeits it.
      Remaining follow-ups below:
- [x] Playtest the mortar's water-class damage — settled: the mortar is
      fine as is. It keeps damaging surface targets + boss parts
      permanently (harder to aim than the torpedo earns the overlap),
      and the tuning stands (120px range / 2.5s cooldown / 22px blast)
- [x] Green land-target faction color + mortar-only reachability —
      folded into the "Implement Stage 2 land roster" item below (the
      mortar additionally keeps its water-class damage per the playtest
      decision above)
- [x] Pause menu + settings (see README "Pause menu & settings"): P opens
      Resume / Options / Controls / Restart Run / Quit over the frozen
      frame; Options = Music/SFX volume in 0–10 steps scaling the
      authored mix, applied live and persisted to `settings.cfg`
      (key=value, next to the exe, `src/settings.c`); Controls screen
      renders from the central action→keys table (`src/input.c`) that
      gameplay now reads all input through
- [x] Fullscreen (see README "Fullscreen"): borderless windowed via F11
      or the options-screen FULLSCREEN row, persisted to `settings.cfg`;
      presentation integer-scales and centers the 512x384 canvas on
      whatever the output is (`src/present.c`, unit-tested) — 2x window
      unchanged, 1080p/1440p/4K at 2x/3x/5x letterboxed. Fractional-fill
      kept as a deliberate playtest option
- [ ] Key remapping UI — deferred (thin payoff with five dual-bound
      actions); the `input.c` binding table is the hook: make it mutable,
      add a capture flow + conflict rule, persist to `settings.cfg`
- [ ] Gamepad support (raylib gamepad API) — likely worth more than key
      remapping for a shmup; hooks into the same `input.c` action table
      as a second binding column
- [x] Title screen (see README "Title screen"): boots to the SEAVIOUS
      wordmark logo (`tools/gen-title-logo.py` → `title_logo.png`, wave
      cut through the letters) over live scrolling ocean, the 2.5x ship
      banked 12° drifting under it with a real wake trail, Theme B
      "driver" playing, and a Start / Options / Controls / Quit menu
      sharing the pause menu's sub-screens. Menu/title state machines
      are pure over an injected MenuInput struct and unit-tested in
      `tests/title_tests.c` (navigation, options adjust/clamp, back-out
      cursor restore, ambient scroll/wake simulation, pause results)
- [x] Link the options/controls screens from the title screen — done
      with the title screen above (shared sub-screen functions in
      `menu.c`)
- [x] Hardpoint pads only where something mounts (decision): an empty
      "could hold a turret" pad is visual noise, so the map only authors
      `H` under an actual installation — Stage 1's two decorative pads
      reverted to plain terrain (`#`, identical footprints), leaving the
      glyph/tooling/render path in place for Stage 2 land targets
- [x] Design Stage 2 (see README "Stage 2 design", agreed in full):
      three-lane exam in an archipelago; land roster = Mortar Battery
      (~600) + Drone Bunker (~500), green, on `H` pads, mortar-only
      (flak emplacement deferred); strait-run set-piece; fortress-atoll
      boss with sea gates needing all three weapons; targeting-computer
      salvage enabling lead torpedoes; ~7,600–8,000 px; Stage 3 slot
      pencilled for the storm concept
- [x] Stage descriptor refactor — engine reads the current stage through
      `GameState.stageNumber` + `GetStageDescriptor()` (`stage.h`)
      instead of naming `STAGE1_*`; stage clear advances the run via
      `BeginStage` (score/lives/mortar carry, pools/scroll rewind,
      wraps to Stage 1 until Stage 2 exists), game over forfeits via
      `ResetRunState`; stage-clear overlay reads the stage number and
      says CONTINUE. Unit-tested in `stage_tests.c`
- [x] Implement Stage 2 land roster — engine-side complete (awaits
      `stage2.txt` for real placement): LandTarget pool, mortar-only
      damage (no gun/torpedo path exists structurally), no contact-kill;
      Mortar Battery (600 pts, enemy MortarShell lob + red shadow
      telegraph, one shell in flight, shells outlive their battery) and
      Drone Bunker (500 pts, land Relay Node, cap 3, owner ids offset
      past the surface pool); green faction color + green destruction
      burst (pad stays as the marker, no wreck); sprites via generators
      (`gen-mortar-battery.py`, `gen-drone-bunker.py`); `B`/`K` map
      glyphs compile cell as terrain + pad + spawn in one. Unit-tested
      in `gameplay_tests.c`, glyph compilation in `test_stage_table.py`,
      smoke-run injection covers update/render paths. Balance numbers
      (3.5s/3.0s cadences, 1 HP each) need the Stage 2 playtest
- [ ] Author `assets/stages/stage2.txt` (~10 beats per the README beat
      intent: mortar tutorial opening, three-lane escalation, strait
      run, breather, fortress-atoll boss lock) + generator/table wiring
      as a second descriptor row
- [ ] Fortress-atoll boss (Stage 2): AA pods (gun), ring batteries
      (mortar), cycling sea gates (torpedo), core; salvage beat awards
      the targeting computer → lead-aimed torpedoes (`FireLeadTorpedo`
      already in `gameplay.c`)
- [ ] Compose Stage 2 theme (+ boss variant) over the shared
      drum/bass template
- [ ] Plan remaining levels beyond Stage 2 (Stage 3 pencilled: storm)
- [x] Ground target sprites (alien platforms/installations) — Relay Node,
      Mine, Casemate, Tracking Turret, and Mobile Platform first passes
      all done (see Art below). Land-based emplacement variants
      deliberately deferred to the Stage 2 mortar-target class (see
      README roster note) rather than reskins of the water designs
- [ ] Island/islet terrain art (Stage 1: sparse islets on open ocean) —
      unblocked now that the terrain-footprint system is in; would
      replace the code-drawn rounded-coastline rings in `game_render.c`
      (second pass: shallows/foam/beach/interior layers) with real
      tiles or per-island generated sprites, if the code-drawn look
      ever falls short. Note: an unused cell-autotile prototype (ground
      material + shore edges + per-cell hardpoint tiles) was removed
      from `game_render.c` as dead code during the stage-descriptor
      refactor — recover it from git history if this second pass happens
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
      nose left), generated by `tools/gen-interceptor.ps1` and wired into
      the game (sprite only — the nose weapon core is baked in);
      refining in LibreSprite still outstanding
- [x] Relay Node sprite — amber waterline ring, dark platform disc,
      tri-mast + pale beacon core (launch flash is code-driven), first-pass
      programmatic version at `assets/sprites/relay_node.png` (24x24),
      generated by `tools/gen-relay-node.ps1` and wired into the game
      (with the code-driven launch flash); refining in LibreSprite still
      outstanding
- [x] Mine sprite — dim spiked urchin with small amber core, first-pass
      programmatic version at `assets/sprites/mine.png` (14x14), generated
      by `tools/gen-mine.ps1` and wired into the game (sprite only, nothing
      code-drawn — it stays deliberately unluminous); refining in
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
      `tools/gen-mobile-platform.py` and wired into the game (the stern
      wake is emitted at runtime into the shared wake pool); refining in
      LibreSprite still outstanding
- [x] Gunship sprite — twin-hull catamaran frame with per-hull magenta
      spine stripes and three pale emitter cores (hull noses + deck
      leading edge), first-pass programmatic version at
      `assets/sprites/gunship.png` (32x24, noses left), generated by
      `tools/gen-gunship.py` and wired into the game (sprite only — the
      three emitter cores are baked in); refining in LibreSprite still
      outstanding
- [x] Wake effect — first-pass particle trail: foam-colored puffs emitted
      from the two rear ski-points, anchored to the water (drift left at
      scroll speed), fade/shrink out over ~1.1s; spray and any bloom/glow
      polish stays with the VFX pass
- [x] Application icon: ship on ocean navy, generated multi-size
      (16/32/48/256) `assets/icon/seavious.ico` by `tools/gen-exe-icon.py`
      from the player sprite, embedded via `src/seavious.rc` on Windows;
      the running window sets the same art with `SetWindowIcon`
      (`window_icon.png`); drift-tested by `tests/test_icon_asset.py`

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
      wired in. Lead instrument level dropped 44→34 (XM 0–64 scale, all
      four theme files) after the first Windows playtest found the tune
      overpowering the drums+bass backing, then 34→22 with the bass
      rebalanced (25%→40% pulse for a stronger fundamental, volume 48→64)
      after the second playtest still heard the tune dominating and no
      audible bass — needs another audition. Theme B ("driver", `assets/audio/stage1_theme_b.xm`) is
      kept in the repo, reserved for future modal screens (menus,
      high-score entry) — wire it when those screens exist
- [x] Wire music into the game: Theme A loops during gameplay, hard-cut
      to the "siren" lament (`boss1_theme_a.xm`) on the game-over screen,
      back to Theme A on restart, with the "hammer" boss theme loaded and
      selected whenever the future `bossActive` flag goes true; guarded so
      headless CI runs (no audio device) play nothing instead of crashing
      — confirmed working on Windows; all music now plays at half volume
      (`MUSIC_VOLUME` in `audio.c`) after the third playtest found the
      whole music bed too loud against the game even with the in-file
      mix fixed
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
- [x] Compose menu theme — resolved by wiring Theme B ("driver") as the
      title/menu theme, the exact reservation it was kept for; composing
      a dedicated menu tune stays optional polish
- [x] Enemy-fire SFX — decided and implemented: **one shared shot sound**
      for every bullet-firing enemy (`enemy_shot.wav`, dark falling
      square matching the shared red-diamond projectile), pushed as one
      `GAME_EVENT_ENEMY_FIRED` per shot/volley (a Gunship or Mobile
      Platform fan is one sound, not three) and only when a bullet
      actually spawned; boss pods included. The SAM launch got its own
      whoosh (`sam_launch.wav`, `GAME_EVENT_SAM_LAUNCHED`, gated on a
      free cell so the sound and door flash always pair), and the mortar
      blast got its own deep crump (`mortar_blast.wav`) instead of
      borrowing the explosion boom. The mortar lob and salvage jingle
      already had real sounds (`mortar_fire.wav`, `mortar_salvage.wav`).
      Kept as-is deliberately: the SAM shootdown reuses the air pop (a
      downed missile is just another small air kill). Volume balance
      (enemy shot 0.30 base) needs the usual Windows playtest
- [x] Define sound effects — nine for the current game: gun shot,
      torpedo launch, torpedo splash (unarmed hit), explosion (armed
      boom), air pop (drone kill), player death, relay drone launch
      (broadcast warble), mine detonation (contact crack), UI blip
      (restart); surface-kill sound deliberately omitted (always inside
      an explosion); more will come with new enemies (Interceptor shot)
      and menu screens
- [x] First-pass SFX generated programmatically (`tools/gen-sfx.py` →
      `assets/audio/sfx/*.wav`, validated by `tests/test_sfx_assets.py`)
      and wired through the GameEventQueue in `audio.c`; volume balance
      and sound character need a Windows playtest — refine individual
      sounds in ChipTone if the synth versions fall short. First such
      refinement done: player death regenerated explosion-first (hard
      burst + rolling noise + deep sub, faint power-down dive) after
      the playtest heard the original square-dive as a beep

## Infra

- [x] Verify the CMake + vcpkg build actually compiles on Windows — builds
      and runs (first playable confirmation 2026-07-12)
- [x] Consider reporting the raylib 6.0 bug upstream — decided against:
      not confident enough in the diagnosis (equally likely a local
      misunderstanding of the build config); the local workaround
      stands and the notes below stay for context. Original notes: with
      `-DCUSTOMIZE_BUILD=ON` (which the vcpkg port passes),
      `cmake/ParseConfigHeader.cmake` treats every uncommented
      `#define SUPPORT_X 0` in `config.h` as default ON, silently enabling
      `SUPPORT_CUSTOM_FRAME_CONTROL` — `EndDrawing()` then never calls
      `SwapScreenBuffer()`/`PollInputEvents()`/frame pacing, so every app
      built on vcpkg's raylib 6.0 shows a permanently blank window at
      uncapped fps. Worked around locally via `vcpkg-overlays/raylib`
      (forces the flag OFF); affects raylib upstream and the vcpkg port
