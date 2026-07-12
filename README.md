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
  forward shot — the first enemy that shoots back. Elongated, forward-swept
  wings (stealth-fighter-ish), glowing spine stripe showing its weapon.
- *Wing Formation* — not a new sprite, just Skimmer Drones flying a fixed V
  or line formation, testing aim/positioning across a spread.
- *Gunship* (heavier, less frequent) — bigger, tougher, fires a 3-way
  spread. Bulky twin-hull frame (like a small flying catamaran, echoing the
  naval theme even airborne), three visible weapon-emitter nodes.

Ground/surface (amber/orange, torpedo targets):
- *Turret Platform* — stationary, breaches the surface, fires straight
  shots up the player's lane. The baseline ground threat. Low hexagonal
  platform, single rotating cannon on top, amber glow ring at the waterline.
- *Relay Node* — stationary, doesn't attack directly but periodically
  launches Skimmer Drones — high priority since destroying it cuts off
  reinforcements. Spire/tower with a pulsing beacon top instead of a gun
  barrel (reads as "broadcasting," not "shooting"), flashes brighter when
  launching a drone.
- *Mine* — stationary, no attack, but sits in the player's path and
  detonates on contact if not destroyed first — a positioning check rather
  than a threat that shoots. Small spiked-sphere/urchin shape at the
  waterline, kept dim/low-key so it doesn't visually announce itself.
- *Mobile Platform* (heavier, less frequent) — slowly drifts across the
  water, higher HP, wider shot spread. Wider, flatter barge/raft shape,
  several small weapon emitters along its edge, trailing wake.

Stage 1 boss: *Leviathan-class dreadnought*, partially breaching the
surface, with separate gun-weak pods and torpedo-weak hull sections as
destructible parts — the dual-targeting mechanic made literal in one fight.
Visual design deferred to a separate pass (see `TODO.md`).

**Color palette**: No sky rendered — open water scrolls underneath the
top-down camera. Environment is classic flat arcade water in the
Xevious/1942 tradition: solid navy (`#144874`) that reads as flat color at
a glance, with a whisper-subtle darker mottle (`#113e66`, barely one value
step down) and a few sparse white flecks (`#cfeff0`). The deliberate
plainness lets the glow-heavy sprites own the scene, and the navy hue
separates cleanly from the player's cyan. Texture-heavy water was tried
and rejected three times (dithered chevrons, swell bands, dappled
patches) — at this scale any pattern with visible contrast reads as
fabric or stripes rather than water; texture only survives at
near-invisible contrast.
Faction colors double as gameplay readability cues for dual-targeting:
player (skimmer + gun bullets) bright cyan/white/silver (`#e8f8f8` hull,
`#4ce0e8` accents); air enemies (gun targets) glowing magenta/purple
(`#d848c0`); ground/surface targets (torpedo targets) glowing amber/orange
(`#e8942c`) — same mechanical hull language as air enemies, different hue,
so weapon choice reads at a glance. Enemy projectiles are red (`#e83c3c`)
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

**Scoring (first pass)**: Points are awarded once, when a target is
destroyed. The current mechanical proof implements the two live target
types; the remaining values establish the intended arcade weighting for
future enemies.

| Target | Points | Status |
| --- | ---: | --- |
| Skimmer Drone | 100 | Implemented |
| Interceptor | 200 | Planned |
| Turret Platform | 300 | Implemented |
| Relay Node | 400 | Planned |
| Gunship / Mobile Platform | 500 | Planned |
| Mine | 100 | Planned |
| Boss part | 1,000+ each | Planned; boss-clear bonus later |

Basic filler targets are inexpensive, limited-torpedo surface threats pay
more, and heavier or progression-gating threats carry the highest awards.

**Art pipeline**: Sprites and tiles are authored in **LibreSprite** (free,
open-source), exported as `.png`, loaded via raylib's `LoadTexture`. Chosen
over Aseprite (paid) and Piskel (browser-only, less capable) for a
no-cost, desktop-native pixel-art tool with animation/tilemap support.

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

**HUD/UI**: Reserved bottom bar (512x32), carved out of the 512x384 canvas
— play area becomes 512x352. Left: reserve lives, shown as small cyan
ship-silhouette icons matching the player color. A run starts with three
total lives, so the active ship is represented on the playfield and two
reserve ships appear in the HUD. Center-left: score, in a pixel font, the
largest text in the bar. Right: torpedo status icon — bright/amber
when ready, pale while in flight, and dim with a reload meter on cooldown
(torpedo isn't unlimited-fire, unlike the gun). Far right: reserved but
empty outside boss fights, becomes a boss health bar when one is active, so
the layout doesn't need to change later just to add that. Chosen as a
reserved bar rather than an overlay so the HUD never competes with the
glow/bloom-heavy playfield for readability.

**Structure**: Stage-based, with a boss fight at the end of each stage.
Lives-based (die = lose a life, restart at checkpoint; game over on last
life). No roguelike meta-progression between runs.

**Current scope (bare mechanical proof)**: Player movement, gun, bomb, one
air enemy type, one ground target type. No full stage, no boss, no
lives/game-over yet — the goal is just validating that the dual-targeting
feel works before building content on top of it.

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
reload) — the only weapon that can destroy the amber Turret Platforms
drifting with the water; gun bullets pass right over them (the
dual-targeting rule). The torpedo reticle sits far ahead on the current
surface lane and marks maximum range, clamped before the right edge of the
screen. Firing launches a straight torpedo down that lane: after a short
arming distance it explodes on the first surface target it hits, or at the
saved reticle point as it drifts with the water if nothing is hit first.
Hits before arming do only small direct impact damage, with no splash. There's no lives/game-over loop
yet — see `TODO.md` for what's next.

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
- Split `src/main.c` before adding the next feature wave. The current
  single-loop prototype is still readable, but lives, enemy projectiles,
  wave sequencing, HUD state, and boss parts will be easier to add if setup,
  update, collision, and draw code are separated first.

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
- Gun bullets destroy Skimmer Drones and do not affect Turret Platforms.
- Torpedoes destroy Turret Platforms and do not affect air enemies.
- Destroying a Skimmer Drone adds 100 points exactly once.
- Destroying a Turret Platform adds 300 points exactly once, including
  when it is caught in a torpedo explosion.
- Torpedo cooldown and one-in-flight behavior are obvious while playing.
- Torpedo reticle marks max range clearly and does not read as a target lock.
- Armed torpedoes explode on the first surface target or at max range.
- Unarmed close-range torpedo impacts do not create splash damage.
- Missing assets or failed texture/render-target loads fail clearly once load
  checks are added.
- The raylib/vcpkg overlay still prevents the blank-window regression after
  dependency or build-system changes.

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
