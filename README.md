# Seavious

Retro top-down scrolling shooter, built in C with [raylib](https://www.raylib.com/).

Renders internally at 512x384 to a `RenderTexture2D` with point
(nearest-neighbor) filtering, then upscales 2x to a 1024x768 window — the
standard trick for a crisp pixel-art look at native resolution.

## Design

**Aesthetic**: Modern pixel-art revival. 256+ colors, soft glow/bloom on
effects (explosions, trails), 32px+ sprites with subtle animation/lighting.
More visual punch than strict 80s authenticity, while staying on a visible
pixel grid. Audio: synthwave-adjacent chiptune hybrid.

**Setting**: An ocean world under invasion by a sleek, mechanical alien
force — "1942, but the WWII planes are aliens." The player flies a low
skimmer craft that hugs the wave tops (wake/spray effects), not a
high-altitude fighter. Air targets are alien drone/fighter craft; ground
targets are alien platforms/installations on or breaching the water surface.

**Camera & scroll direction**: Top-down (bird's-eye) camera, like
Xevious/1942 — but scrolling **horizontally**, left-to-right, rather than
vertically. Closer to Uridium than to Xevious in framing, and explicitly
not a side-view/profile game like Scramble (no terrain-following, no
crashing into silhouetted terrain). Chosen partly because a horizontal-scroll
top-down playfield is naturally landscape-shaped, so it fits a normal
desktop window without the letterboxing/side-panel problem a portrait
playfield would have on most monitors.

**Player ship ("Skimmer")**: Low, wide-stance dart silhouette — forward-
pointed nose cone, twin ski-like pontoons at the rear that visibly sit on/
kiss the water surface. This is the one design element no enemy shares:
air units are fully airborne (aircraft silhouettes), ground units are
stationary platforms, only the player visibly touches the water while
moving. A raised centerline spine between the two skis carries the glowing
accent line (same "glow stripe = active tech" visual grammar as the
Interceptor, in the ally color). Color: `#e8f8f8` hull (near-white/silver),
`#4ce0e8` cyan accents on the spine, engine glow, and ski trim. Wake VFX
emit from the two rear ski-points (first-pass foam-puff trail is in;
spray and glow polish deferred to the VFX design pass).

**Enemy design language**: Sleek mechanical alien tech — clean hulls,
glowing energy lines, geometric silhouettes. Applies to both the air drone
swarms and the surface installations, for readability at small sprite sizes.

**Enemy roster (Stage 1 sized)**:

Silhouette rule, on top of the color coding: air units read as *aircraft*
(swept/pointed shapes, implies motion), ground units read as *platforms*
(flat-bottomed, sits at a visible waterline, implies stationary bulk) — so
shape alone hints at which weapon to use, before color even registers.

Air (magenta/purple, gun targets):
- *Skimmer Drone* — weakest, most common filler. Sine-wave flight, dies in
  1-2 hits. Tiny dart/diamond shape, one glowing core, minimal wing area —
  deliberately plain so it parses instantly in swarms. Sprite (first-pass
  programmatic, 24x16, nose left): dark gunmetal-violet hull — inverting the
  player's white-hull-plus-accent scheme — so the full-length magenta energy
  stripe and single pale-pink core carry the color read; at swarm scale each
  drone reads as a magenta light moving over the water. The core's brightness
  pulses in code with a per-drone phase offset (no extra animation frames),
  so swarms shimmer rather than strobe in unison. The near-symmetric diamond
  is kept deliberately plain — swept/elongated shapes are reserved for the
  Interceptor, extra emitter detail for the Gunship.
- *Interceptor* — faster, flies straight at/past the player, fires a single
  forward shot — the first enemy that shoots back. Elongated,
  stealth-fighter-ish silhouette with a glowing spine stripe showing its
  weapon. Sprite (first-pass programmatic, 32x16, nose left): same air-family
  language as the drone (dark gunmetal-violet hull, magenta energy) but
  clearly elongated with swept-back wings (the original brief said
  forward-swept; back-swept read cleaner at this sprite size), a full-length
  spine stripe with a bright top and dark bottom half, wingtip glints, and a
  pale weapon core at the nose marking it as armed. Implemented: 140 px/s
  straight flight along its spawn row (vs the drone's 60 px/s sine), 1 HP,
  fires once when it crosses two-thirds of the screen, 200 points.
  Its shot uses the universal enemy projectile: code-drawn red (`#e83c3c`)
  diamond with a white-hot single-pixel center, identical for every enemy
  that shoots (the diamond visual landed with the air roster, replacing
  the placeholder circle). Its speed stays at the 160 px/s the surface
  roster shipped and playtested with, rather than the 180 px/s this entry
  originally sketched — one shared speed for every enemy shot wins.
- *Wing Formation* — not a new sprite, just Skimmer Drones flying a fixed V
  or line formation, testing aim/positioning across a spread.
- *Gunship* (heavier, less frequent) — bigger, tougher, fires a 3-way
  spread. Bulky twin-hull frame (like a small flying catamaran, echoing the
  naval theme even airborne), three visible weapon-emitter nodes. Sprite
  (first-pass programmatic, 32x24, noses left): two parallel gunmetal-violet
  fuselages joined by a recessed deck, each hull carrying the family's
  magenta spine stripe with the bright/dark split, and the three emitters
  as pale cores — one at each hull nose plus a 2x2 block on the deck's
  leading edge (the spread's center emitter). Implemented: 70 px/s
  straight flight along its spawn row (half the
  Interceptor's speed — the bulk should read in motion), 3 HP to the gun
  (the first multi-hit air target), fires a 3-way spread aimed at the
  player (±16°) every 2.4s while fully on-screen — the timer only runs
  on-screen, so the first spread comes one interval after entry, same
  courtesy as the Mobile Platform — 500 points, and a slightly larger
  destruction burst than the darts to match its bulk.

Ground/surface (amber/orange, torpedo targets):
- *Casemate* — stationary, breaches the surface, and fires straight left
  across its own row. The baseline ground threat: a low hexagonal platform
  with a fixed cannon and amber glow ring at the waterline. Sprite
  (first-pass programmatic, 24x24, top-down): the shared ground-family
  amber waterline ring, a dark warm hex plate lit from the upper-left,
  the fixed barrel baked into the sprite crossing the waterline to the
  left with a pale muzzle (it never rotates, so nothing stays
  code-drawn), and a small pale emitter core at the hub.
- *Tracking Turret* — stationary circular mount with a rotating cannon. It
  leads the player's current movement before firing, so changing direction
  after a shot is the reliable evasion response. The lead is deliberately
  soft: only a tenth of the player's velocity goes into the intercept
  solve (`TRACKING_TURRET_LEAD_FACTOR`), and the predicted point is
  clamped to the play area, so the shot nudges toward the direction of
  travel and never aims where the player can't be. A perfect intercept
  was tried first and read as psychic — while
  flying up it aimed at the top edge, where the player *would* be seconds
  later. Sprite (first-pass programmatic, 24x24, top-down): same amber
  waterline ring, a rust-brown circular deck lit from the upper-left, a
  bolt ring marking the rotation bearing (the "this rotates" cue
  distinguishing it from the Casemate's fixed plate), and a dark pivot
  hub — the aiming barrel stays code-drawn on top so it can lead the
  player.

Both are water designs on purpose. Land-based emplacements are *not*
reskins of these: land blocks torpedoes, so an amber (torpedo-class)
target on land would be unkillable — land emplacements belong to the
green mortar-target class (Stage 2+, see Color palette) and will be
designed as their own roster entries sharing the silhouette family.
Boat-hulled variants were also considered and rejected: boats imply
drift, breaking the ground-family "stationary bulk" silhouette rule, and
the drifting-vessel niche already belongs to the Mobile Platform.
- *Relay Node* — stationary, doesn't attack directly but periodically
  launches Skimmer Drones — high priority since destroying it cuts off
  reinforcements. Spire/tower with a pulsing beacon top instead of a gun
  barrel (reads as "broadcasting," not "shooting"), flashes brighter when
  launching a drone. Sprite (first-pass programmatic, 24x24, top-down):
  amber waterline glow ring echoing the Casemate (the shared
  ground-family cue), dark warm platform disc lit from the upper-left,
  three radial masts at 120° reading as broadcast structure rather than a
  gun, and a 2x2 pale beacon core whose launch flash is code-driven.
  Behavior numbers (first pass): anchored to the water (drifts at scroll
  speed), 1 HP to a torpedo, launches a Skimmer Drone every 2.5s while
  fully on-screen with at most 3 of its drones alive at once.
  Implemented: spawns from its stage glyph, launched drones start at the
  relay itself (not the screen edge) and count against its cap so a
  freed slot refills immediately, destroying it scores 400 and strands
  its surviving drones, and each launch fires the code-driven beacon
  flash plus a broadcast-warble SFX.
- *Mine* — stationary, no attack, but sits in the player's path and
  detonates on contact if not destroyed first — a positioning check rather
  than a threat that shoots. Small spiked-sphere/urchin shape at the
  waterline, kept dim/low-key so it doesn't visually announce itself.
  Sprite (first-pass programmatic, 14x14, top-down): eight dark-rust spikes
  around a dark warm body with a small dim amber core — deliberately the
  least luminous object on the water. Behavior numbers (first pass):
  anchored to the water, 1 HP to a torpedo (gun passes over it), detonates
  on player contact. Implemented: spawns from its stage glyph; a torpedo
  kill scores 100 like any surface kill, while a contact detonation costs
  the ship and deliberately scores nothing (hitting a mine is never worth
  points); either death is a blast with no wreck — the one surface target
  that detonates to nothing — with its own sharp-crack SFX on contact; an
  invulnerable (respawning) player passes over mines without setting them
  off.
- *Mobile Platform* (heavier, less frequent) — slowly drifts across the
  water, higher HP, wider shot spread. Wider, flatter barge/raft shape,
  several small weapon emitters along its edge, trailing wake. Sprite
  (first-pass programmatic, 36x18, bow left): the one ground unit that
  moves, so its amber waterline hugs the hull outline like a vessel
  instead of sitting as the anchored units' detached ring — pointed bow
  in the drift direction, dark warm deck lit upper-left, raised rust
  deckhouse, and three pale emitters (bow tip + two along the deck
  edges). Wake trails from the stern at runtime (code VFX, like the
  player's). Behavior numbers (first pass): self-propelled at 60 px/s
  leftward (1.5x scroll speed, visibly moving relative to the water),
  2 HP to torpedoes (the first multi-torpedo surface target), fires a
  3-shot fan aimed at the player (±14°) every 3.0s while on-screen,
  500 points. Implemented: spawns from its stage glyph; the fire timer
  only runs while the hull is fully on-screen, so the first fan comes one
  full interval after it appears instead of greeting the player at the
  edge; the stern wake reuses the player's wake pool (puffs drop at the
  stern and drift at water speed while the hull pulls away at 1.5x); its
  self-propulsion shares the scroll clock, so the boss lock freezes it
  with the rest of the water traffic.

Stage 1 boss: *Leviathan-class dreadnought*, partially breaching the
surface, with separate gun-weak pods and torpedo-weak hull sections as
destructible parts — the dual-targeting mechanic made literal in one fight.
It also carries one part the player *can't* destroy: an armored **mortar
turret** that lobs arcing shells (rising/falling shadow, area blast on
landing) throughout the fight, shaping the dodging as persistent pressure
— and teaching the mortar's visual language as an enemy weapon before the
player owns it. When the boss core dies the turret powers down intact, and
the end-of-stage sequence has the skimmer salvage it: from Stage 2 onward
it's fitted to the player as the third weapon (see Core mechanic).

**Stage 1 boss design (Leviathan)**:

*Body & layout* — a long broadside hull, bow left (toward the player),
parked on the right ~40% of the screen once the boss lock stops the
scroll; the player owns the left side of the arena. Overall footprint
~200x120 on the 512x352 play area, drawn as one dark breached-hull base
sprite (the "wreck-to-be") with the five interactive parts as separate
sprites layered on top, each with its own hitbox and HP, following the
weapon-class colors exactly:

- 2x *AA pods* (magenta, gun-weak, ~20x20): raised turret bulbs on the
  hull spine, fore and aft. Same air-family visual language as the drone
  swarms — gunmetal-violet housings with magenta energy lines — because
  magenta = "shoot it." 12 gun hits each; 1,000 points. Each fires
  turret-style aimed red shots every ~2.0s.
- 2x *hull sections* (amber, torpedo-weak, ~40x24): armored casemate
  plates at the waterline, fore and aft, with the ground-family amber
  waterline glow. 2 torpedoes each; 1,000 points. Each fires straight
  lane shots (casemate-style) every ~2.5s.
- 1x *mortar turret* (indestructible, ~24x24): armored dome aft-center,
  deliberately *colorless* in weapon-class terms — bare dark steel with
  no amber/magenta glow, the visual grammar for "no weapon works on
  this." Lobs an arcing shell every ~4.0s at the player's position: a
  launch puff, a shrinking-then-growing shadow at the landing point
  (~1.2s of air time — the dodge window), then an area blast. Red
  accents only on the shell/blast (universal "this kills you").
- 1x *core* (initially hidden): amidships beneath a breach in the base
  hull, revealed once both hull sections are destroyed — a white-hot
  pale opening with escaping glow (the same white-hot palette as
  explosion centers, reading as "the inside of the machine"). Weak to
  *both* weapons once exposed (4 torpedoes or ~24 gun hits; mixing
  works), so the endgame rewards whichever weapon the player has left
  free. 2,000 points + stage-clear bonus later.

*Fight flow* — no scripted phases; the structure emerges from the parts:
pods and hull sections all fire from the start (staggered timers so
volleys interleave rather than wall), the mortar lobs throughout, and
every destroyed part permanently silences its gun — the fight naturally
decays from "bullet storm + mortar" to "just the mortar's rhythm" as the
player dismantles the ship. Destroying a part leaves a burnt socket on
the base sprite (the wreck look assembling in place). The mortar's
cadence quickens slightly (~4.0s → ~2.8s) once the core is exposed, so
the final push stays under pressure. Boss HP bar (reserved HUD slot)
shows the sum of remaining destructible-part HP.

*Sprites* — first-pass programmatic set at `assets/sprites/leviathan_*.png`
(generated by `tools/gen-leviathan.py`, five sprites: the 200x120
breached hull base with plate seams, spine ridge, and the jagged breach;
the AA pod dome with its magenta energy ring; the amber-edged hull
section plate with two dark gun ports facing the player; the bare-steel
mortar dome — verified faction-colorless by the asset test; and the
white-hot core opening). Mortar shell/shadow/blast are code-drawn VFX
per the game's convention, not sprites, and destroyed-part scorch marks
reuse the existing code-drawn wreck treatment rather than baked socket
states. Refining in LibreSprite still outstanding, same as the roster.

*Entrance & defeat* — the boss slides in from the right over ~3s under
the boss-lock (scroll stopped, spawns stopped, "hammer" theme
hard-cut in). On core death: chain of part-explosions along the hull
(white-hot → orange, biggest VFX in the game), the wreck settles a few
pixels lower in the water, music hard-cuts back to the stage theme, and
the mortar turret's red accents fade to dark — powered down, intact.
The salvage beat is deliberately simple: the skimmer auto-pilots
alongside, the turret dome lifts off the wreck and docks onto the
skimmer's spine (a few seconds of sprite animation, no input), a pickup
jingle plays, and the stage-clear flow takes over. Stage 2 then opens
with the mortar equipped.

**Color palette**: No sky rendered — open water scrolls underneath the
top-down camera. Environment is classic flat arcade water in the
Xevious/1942 tradition: solid navy (`#144874`) that reads as flat color at
a glance, with a whisper-subtle darker mottle (`#113e66`, barely one value
step down) and nothing else. The deliberate plainness lets the glow-heavy
sprites own the scene, and the navy hue separates cleanly from the
player's cyan. Texture-heavy water was tried and rejected three times
(dithered chevrons, swell bands, dappled patches) — at this scale any
pattern with visible contrast reads as fabric or stripes rather than
water; texture only survives at near-invisible contrast. Distinct baked
marks were also tried and rejected (sparse white flecks): any landmark
feature makes the tile repeat trackable. Foam glints instead live on a
separate parallax overlay (`assets/tiles/ocean_overlay.png`, 160x96,
3 dim `#cfeff0` marks on transparency) that scrolls slightly faster
than the base water (`OCEAN_OVERLAY_SPEED_SCALE`) — the glints drift
relative to the water body like propagating wavelets, and since 160x96
never aligns with the 128x64 base tile, the combined repeat period is
far longer than either texture alone. The overlay is part of the water
surface in draw order: immediately above the base ocean, below
everything that sits on the water (wake, surface targets, future land),
so glints never draw over solid objects.
Faction colors double as gameplay readability cues for dual-targeting:
player (skimmer + gun bullets) bright cyan/white/silver (`#e8f8f8` hull,
`#4ce0e8` accents); air enemies (gun targets) glowing magenta/purple
(`#d848c0`); ground/surface targets (torpedo targets) glowing amber/orange
(`#e8942c`) — same mechanical hull language as air enemies, different hue,
so weapon choice reads at a glance; land targets (mortar targets, Stage 2+)
glowing green (`#48d858`, first-pass value) completing the
weapon-class-color triad. Enemy projectiles are red (`#e83c3c`)
regardless of source (universal "this kills you" convention). Explosions/VFX
are white-hot fading to orange (`#fff6d8` → `#e8942c`) with bloom.

**Core mechanic**: Xevious-style air/ground dual targeting, reskinned for
the setting. Forward (rightward) gun auto-fires at air enemies; a separate
torpedo hits ground/surface targets. The torpedo is forward-fired and runs straight along the water lane toward
a visible max-range reticle rather than dropped/arced from altitude (the
skimmer flies too low for a falling bomb to make sense). The reticle marks
range, not lock-on: once fired, that max-range point is anchored to the
scrolling surface and drifts left with the water. The torpedo explodes on
the first armed surface hit or at max range, while very close unarmed
impacts only deal direct hit damage.
Lead-targeting is kept as a future upgrade idea, not the baseline weapon.

From Stage 2 the grammar extends to a third weapon/target class: the
**scavenged mortar**, taken from the Stage 1 boss's armored turret after
the fight (see the boss entry) — an upgrade the run earns, not standard
kit, so Stage 1 stays the pure dual-targeting tutorial. The mortar is a
lobbed shot (the "skimmer flies too low" objection was to *dropping* from
altitude; lobbing up-and-forward from sea level reads fine top-down:
projectile scales up then down over a drifting shadow) that **arcs over
land**, with its own shorter fixed-range in-lane reticle that — unlike
the torpedo's — does not clamp at land edges, an area blast where it
lands, its own manual-fire key, and its own cooldown/HUD icon. Class
mapping stays strict in all three lanes: gun→air, torpedo→water surface,
mortar→land targets only — arcade readability beats blast physics, the
same rule that lets gun bullets pass over surface targets. Land targets
(possible from Stage 2, since only the mortar can touch them) get the
third faction color to complete the triad.

**Scoring (first pass)**: Points are awarded once, when a target is
destroyed. The current mechanical proof implements the two live target
types; the remaining values establish the intended arcade weighting for
future enemies.

| Target | Points | Status |
| --- | ---: | --- |
| Skimmer Drone | 100 | Implemented |
| Interceptor | 200 | Implemented |
| Casemate | 300 | Implemented |
| Tracking Turret | 400 | Implemented |
| Relay Node | 400 | Implemented |
| Gunship | 500 | Implemented |
| Mobile Platform | 500 | Implemented |
| Mine | 100 | Implemented (torpedo kill only; contact detonation scores nothing) |
| Boss part | 1,000+ each | Planned; boss-clear bonus later |

Basic filler targets are inexpensive, limited-torpedo surface threats pay
more, and heavier or progression-gating threats carry the highest awards.

**Art pipeline**: Sprites and tiles are authored in **LibreSprite** (free,
open-source), exported as `.png`, loaded via raylib's `LoadTexture`. Chosen
over Aseprite (paid) and Piskel (browser-only, less capable) for a
no-cost, desktop-native pixel-art tool with animation/tilemap support.
The ASCII-grid generator scripts in `tools/` stay intentionally explicit and
asset-specific; only the low-level bitmap write step is shared so the sprite
shape, palette, and output path stay easy to read in each file.

**Audio**: Music is tracker-composed (XM/MOD) in **OpenMPT** (free/open
source), played back via raylib's built-in `raudio` module, which supports
XM/MOD/IT natively with no extra dependencies. OpenMPT was chosen over
Furnace/MilkyTracker for reliable .XM/.MOD format compatibility with
raylib's parser and the largest tutorial/example base of the three.

SFX (gunfire, torpedo launch/splash, explosions, UI, hit/death) are
generated in **ChipTone** (free, sfxr-family) and exported as `.wav`,
played via `raudio`'s `Sound` API. A dedicated SFX synth is a better fit
than the tracker for one-shot sounds, and WAV has no compatibility caveats
the way XM/MOD does.

First-pass SFX are programmatic (`tools/gen-sfx.py` → `assets/audio/sfx/`,
nine square/sine/noise one-shots at 22050 Hz mono; refine in ChipTone
later): gun shot (every auto-fire shot, deliberately tiny and quiet),
torpedo launch, torpedo splash (unarmed direct hit), explosion (armed
boom), air pop (drone kill), player death, relay drone launch
(broadcast warble), mine detonation (sharp contact crack, distinct from
the torpedo boom), and a UI blip on run restart.
They're triggered through the `GameEventQueue`: the update code emits
events (weapon fired, impact, death, restart) and `audio.c` maps events
to sounds — the same pattern the destruction VFX already use, so gameplay
code never touches the audio API. Surface-target destruction deliberately
has no dedicated sound: it only ever happens inside a torpedo explosion,
whose boom covers the moment.

**Music structure**: Every track is three layers authored together from one
shared OpenMPT template — a 2-bar drum loop that never changes, a 4-bar
walking bass loop outlining a fixed chord skeleton (`I–vi–IV–V` in A major),
and a lead tune loop (8 bars to start, 16 as a stretch) swapped per context.
152 BPM, 4/4 throughout. Reuse is a composition-workflow trick, not engine
code: the drum/bass patterns get copied unchanged into each new track's
project file, so only the lead pattern differs between, say, the stage 1
theme and the stage 2 theme. Boss fights hard-cut to a separate `.xm` built
from the same template but with the bass reharmonized to the parallel minor
(`i–VI–iv–v`, same A–F–D–E root motion, so only the bass's color notes
change) — a straight track swap via `LoadMusicStream`/`PlayMusicStream` at
the moment a boss fight starts, not a live crossfade. A hard cut was chosen
over a seamless transition specifically to avoid needing synced simultaneous
music streams or per-channel muting in `raudio`, neither of which is
confirmed to work cleanly.

Current tracks (first pass, generated by `tools/gen-stage1-xm.py` since
OpenMPT isn't reachable from the WSL dev environment; auditioned and
confirmed playable): `stage1_theme_a.xm` ("anthem" — long held notes,
mostly stepwise, resolves onto A at the loop) is the stage 1 gameplay
theme. `boss1_theme_b.xm` ("hammer" — relentless straight-8ths arpeggio
grind over the parallel-minor backing) is the stage 1 boss theme, wired
behind a `bossActive` flag that the future boss-fight structure will
drive. `boss1_theme_a.xm` ("siren" — a high wailing descent, also over
the minor backing) doubles as a lament on the game-over screen; the
per-life death (0.6s) is deliberately too short to swap music, so only
the final death changes the track. `stage1_theme_b.xm` ("driver") is
reserved for future modal screens (menus, high-score entry).
`stage1_drums_bass.xm`/`boss1_drums_bass.xm` are the authoring
templates the tunes were composed over; they aren't played in-game.

**HUD/UI**: Reserved bottom bar (512x32), carved out of the 512x384 canvas
— play area becomes 512x352. Left: reserve lives, shown as small cyan
ship-silhouette icons matching the player color. A run starts with three
total lives, so the active ship is represented on the playfield and two
reserve ships appear in the HUD. Center-left: score, in a pixel font, the
largest text in the bar. Right: torpedo status icon — bright/amber
when ready, pale while in flight, and dim with a reload meter on cooldown
(torpedo isn't unlimited-fire, unlike the gun); once the mortar is
scavenged (Stage 2+) a second, green status icon joins it with the same
ready/flight/cooldown states. Far right: reserved but
empty outside boss fights, becomes a boss health bar when one is active, so
the layout doesn't need to change later just to add that. Chosen as a
reserved bar rather than an overlay so the HUD never competes with the
glow/bloom-heavy playfield for readability.

**Structure**: Stage-based, with a boss fight at the end of each stage.
Lives-based: enemy contact costs one life, the ship explodes briefly, then
respawns with brief invulnerability if any lives remain; game over triggers
after the final death effect. Destroyed air targets burst and disappear;
destroyed surface targets leave inert, scrolling burnt-out wrecks. Checkpoints
still come later with the stage system. No roguelike meta-progression between
runs.

Open question, deliberately deferred to the terrain system build: should
wrecks block torpedoes the way land will? A live surface target already
blocks its lane (the torpedo hits it), so killing it either opens the lane
(wrecks inert, current behavior) or leaves debris blocking it (wrecks act
like drifting land: armed shots detonate on the wreck — whose splash can
still reach a target tucked behind it, an emergent detonator trick —
unarmed shots fizzle, and the reticle clamps to the first wreck in the
lane). Both are coherent; blocking wrecks would preview the terrain rules
before islands appear and share their implementation, which is why the
call belongs with that task rather than now.

**Level & stage design**: Stages are scripted timelines — a flat,
deterministic table of spawn events triggered by scroll distance traveled,
in the Gradius/R-Type tradition, not a procedural wave director. Events
reference reusable parameterized wave patterns (e.g. "V of 5 Skimmer Drones
entering at lane Y") so the stage table stays compact and readable.
Deterministic stages are memorizable (where shmup mastery lives),
hand-tunable beat by beat, and unit-testable in the pure gameplay layer.
Scroll distance — not wall-clock time — is the trigger currency, so
scroll-speed changes stay possible later and the boss lock (scroll stops)
halts stage spawns with no special casing. The wave script keeps running
through the 0.6s death pause (freezing it mid-wave would create
half-spawned states for little benefit). Checkpoints, when they arrive, are
continue-after-game-over restart points, not Gradius-style per-death
rollback — in-place respawn per death stays.

Stage 1 doubles as the tutorial: each roster enemy debuts alone (or nearly
so), then folds into combinations with everything already introduced. Beat
chart, ~15–20s per beat (~3 minutes at the current 40 px/s scroll ≈ 7,200 px
≈ 14 screen-widths): (1) lone Skimmer Drones — gun + scroll; (2) Wing
Formations — aiming across a spread; (3) first Casemates — forces the
torpedo; (4) mixed air + surface — the dual-targeting juggle; (5)
Interceptors — first return fire; (6) Tracking Turrets in the mix —
move after they fire; (7) Relay Node set-piece — priority targeting;
(8) mine belt — positioning and torpedo economy; (9) Gunship + Mobile
Platform finale; (10) empty-water breather, then boss lock.

**Stage authoring format**: Stages are designed as ASCII maps — a text
grid where columns are scroll distance and rows are lanes, a faithful 1:1
picture of the horizontal ocean strip. Resolution: 1 character = 32 px,
giving 11 rows for the 352px play area, 16 columns per screen-width, and
~0.8s of scroll per column. The map is split into per-beat blocks, each
headed by its scroll offset (`@4200`), so beats can be re-paced without
redrawing. Glyphs are *pattern instances, not individual enemies* (`W` =
"V of 5 drones, leader anchored here"); a formation's internal shape and
timing live in its reusable pattern function, the map only places and
times it. One unified map holds everything — terrain footprints (`#`,
contiguous runs merge into one blocking footprint), world-anchored ground
enemies, and air spawn triggers — so lane pressure between air and ground
stays visible while designing. The two glyph semantics share the grid
cleanly: ground glyphs and land are world positions on the water, air
glyphs fire their spawn when their column reaches the right screen edge —
the same column meaning either way. Pipeline follows the repo's
generator idiom (XM tracks, sprites): the map (`assets/stages/stage1.txt`)
is compiled by a `tools/` script into a committed C spawn-event table the
engine consumes directly — no text parsing at runtime — with a drift test
that regenerates and compares, same as `tests/test_xm_assets.py`.

**Terrain**: Non-colliding for flight — the skimmer and its gun ignore land
entirely (reaffirming the "not Scramble, no terrain crashes" stance) — but
land **blocks torpedoes**: an armed torpedo detonates against the first
land edge in its lane (splash can still catch shoreline targets — a
deliberate tactic), an unarmed one fizzles with no splash, and the range
reticle clamps to that land edge so a blocked lane reads before firing.
Ground enemies can anchor to terrain features (a Casemate on a shoreline,
a Relay Node on a reef), so islands create natural set-pieces — and since
terrain and targets drift together, a target shielded behind an island is
*permanently* blocked from its own row's approach: the counter-play is
positional, not patience — fly forward past the island and fire from
beyond it (deliberately risky), catch the target with edge-splash if it
hugs the shoreline, or detonate a max-range shot in an adjacent clear
lane and let splash reach. Terrain is therefore
stage data (blocking footprints drifting at scroll speed in the timeline),
not just background art. Stage 1 is open ocean with sparse islets; heavier
terrain flavor (storms, archipelagos) is per-stage variety for later
stages. From Stage 2, land also *holds* destructible targets — green
mortar-class installations built on terrain (see Core mechanic) — so
Stage 2's opening beats teach the scavenged mortar the way Stage 1's
beat 3 taught the torpedo: a land target the other weapons visibly can't
touch. Boss-yields-an-upgrade is a candidate pattern for later stages'
progression spine, not yet a commitment beyond Stage 1.

**Current scope**: Player movement, gun, torpedo, lives/game-over loop,
music + SFX, and the Stage 1 script driving all spawning: the committed
map (`assets/stages/stage1.txt`) compiles to a C event table
(`src/stage1_data.c`) that the engine walks by scroll distance — the
full Stage 1 roster (Skimmer Drone solo/line/V formations, Interceptor,
Gunship, Casemate, Tracking Turret, Relay Node, Mine, Mobile Platform)
spawns at its authored beats, and the boss lock at
map end freezes the scroll (raising the boss music as a placeholder for
the future fight). Remaining for a completable Stage 1: the terrain
system and the boss fight itself.

## Building on Windows (MSVC + vcpkg)

Prerequisites:
- Visual Studio 2022 (Desktop development with C++ workload) or the standalone
  Build Tools
- [vcpkg](https://github.com/microsoft/vcpkg), with `VCPKG_ROOT` set

This project is on the WSL filesystem. Open it from Windows via the UNC path
(`\\wsl$\<distro>\home\james\dev\seavious` or `\\wsl.localhost\...`), or clone
it to a native Windows path for faster builds — either works with CMake.

```powershell
cmake --preset default    # or: cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
.\build\Release\seavious-dev.exe   # temp name, see CMakeLists.txt / TODO.md
```

**Note — `vcpkg-overlays/raylib`**: the project overrides the stock vcpkg
raylib port. Stock vcpkg builds raylib 6.0 with `-DCUSTOMIZE_BUILD=ON`,
which trips a raylib CMake bug (`ParseConfigHeader.cmake` reads
`#define SUPPORT_X 0` as ON) that silently enables
`SUPPORT_CUSTOM_FRAME_CONTROL` — with it, `EndDrawing()` never presents a
frame and the window stays permanently blank at uncapped fps. The overlay
port forces that flag (and `SUPPORT_BUSY_WAIT_LOOP`) off. The preset wires
it in via `VCPKG_OVERLAY_PORTS`; don't remove it until the upstream
port/raylib is fixed.

Or just open the folder in Visual Studio 2022 — it detects `CMakeLists.txt`
and `vcpkg.json` automatically (manifest mode) and configures itself.

## Controls

Arrow keys / WASD to move, within the 512x352 play area (the bottom 32px
is reserved HUD space). Gun auto-fires forward and downs the Skimmer
Drones (dark dart-diamond craft with a pulsing magenta core) that fly in
from the right on a sine-wave path. Space fires a torpedo (one in flight at a time, 1.5s
reload) — the only weapon that can destroy the amber surface threats
drifting with the water; gun bullets pass right over them (the
dual-targeting rule). The torpedo reticle sits far ahead on the current
surface lane and marks maximum range, clamped before the right edge of the
screen. Firing launches a straight torpedo down that lane: after a short
arming distance it explodes on the first surface target it hits, or at the
saved reticle point as it drifts with the water if nothing is hit first.
Hits before arming do only small direct impact damage, with no splash.
Casemates fire straight red lane shots; rotating turrets gently lead the
player's current movement before firing.
Enemy contact costs one life, triggers a brief ship explosion, then respawns
the ship briefly invulnerable if any remain; game over follows the final
explosion.

## Technical Follow-ups

Items found in the current prototype review:

- Guard the Windows-only discrete-GPU exports in `src/main.c` with
  `#if defined(_WIN32)` so the otherwise portable C/raylib app can still
  build cleanly on non-Windows targets later.
- Make asset lookup independent of the process working directory. CMake
  copies `assets/` next to the executable, but `LoadTexture("assets/...")`
  still resolves relative to the current working directory when the app is
  launched.
- Add explicit failure checks after `LoadRenderTexture` and `LoadTexture`
  calls so missing assets or graphics-resource failures fail clearly instead
  of producing confusing blank/placeholder behavior.
- Cap or sanitize unusually large `dt` values after stalls/debug pauses, and
  wrap ocean scrolling with true modulo behavior rather than subtracting one
  tile width once.
- Normalize player movement input so diagonal movement is not faster than
  horizontal or vertical movement.
- ~~Split `src/main.c` before adding the next feature wave.~~ Done: state,
  update, render, assets, and audio now live in their own modules (see
  `CLAUDE.md` Structure); `main.c` is the init + frame-loop shell.

## Testing Notes

For now, keep rendering/feel checks manual and put automated coverage around
small gameplay-rule functions once they are split out of `src/main.c`.

Recommended first test framework: [Unity](https://github.com/ThrowTheSwitch/Unity).
It is a small C test framework with simple assertions, easy CMake integration,
and little ceremony. Plain `assert()` is fine for the very first extracted
function, but Unity will give clearer failures once there are multiple rule
tests.

Manual smoke checklist:

- Window opens at 1024x768 and presents frames normally.
- Pixel art remains crisp with nearest-neighbor scaling.
- Ocean scrolls continuously without seams or jumps.
- Player movement stays within the 512x352 play area and never enters the HUD.
- Diagonal movement does not become faster once movement normalization is added.
- Gun bullets destroy Skimmer Drones and do not affect surface threats.
- Torpedoes destroy Casemates and Tracking Turrets and do not affect air enemies.
- Destroying a Skimmer Drone adds 100 points exactly once.
- Destroying a Casemate adds 300 points, and a Tracking Turret adds 400 points, exactly once, including
  when it is caught in a torpedo explosion.
- Air-target explosions are brief and disappear; destroyed surface targets
  leave inert burnt-out wrecks that drift with the ocean.
- A player death shows an explosion, disables control during the short death
  delay, then respawns or reaches game over as appropriate.
- Torpedo cooldown and one-in-flight behavior are obvious while playing.
- Torpedo reticle marks max range clearly and does not read as a target lock.
- Armed torpedoes explode on the first surface target or at max range.
- Unarmed close-range torpedo impacts do not create splash damage.
- Missing assets or failed texture/render-target loads fail clearly once load
  checks are added.
- The raylib/vcpkg overlay still prevents the blank-window regression after
  dependency or build-system changes.

The XM music assets are covered by `tests/test_xm_assets.py` (run
standalone with `python3 tests/test_xm_assets.py`): it regenerates all
tracks via `tools/gen-stage1-xm.py` into a temp dir, validates each file
structurally against the XM spec, byte-compares against the committed
`assets/audio/` files so generator and assets can't drift, and — where
libopenmpt is installed (CI does) — loads each file with the reference
engine and checks duration, pattern layout, and that rendered audio is
audible.

Good first automated tests after extracting pure gameplay logic:

- Player input normalization produces equal cardinal and diagonal speed.
- Player clamping respects sprite half-size and the reserved HUD bar.
- Ocean scroll wrapping handles large `dt` values.
- Fixed-range torpedo reticle clamps before the right edge and never behind the launch point.
- Torpedo arming distance separates direct impact from splash explosion.
- Lead-targeting math remains isolated for a future upgrade path.
- Collision rules preserve the dual-targeting split: gun-vs-air only,
  torpedo-vs-surface only.

## License

MIT. See `LICENSE`.
