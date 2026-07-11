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
`#4ce0e8` cyan accents on the spine, engine glow, and ski trim. Wake/spray
VFX (deferred to the VFX design pass) will emit from the two rear
ski-points.

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
  deliberately plain so it parses instantly in swarms.
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
top-down camera. Environment is deep teal-to-navy ocean
(`#0a2530` → `#134a5c`) with pale cyan-white foam/wave crests (`#cfeff0`).
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
torpedo hits ground/surface targets. The torpedo is forward-fired and
travels along the surface toward a leading target rather than dropped/arced
from altitude (the skimmer flies too low for a falling bomb to make sense)
— still just a second fire input, no extra control axis needed.

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
— play area becomes 512x352. Left: lives, shown as small cyan
ship-silhouette icons matching the player color. Center-left: score, in a
pixel font, the largest text in the bar. Right: torpedo status icon —
bright/amber when ready, dims or shows a small reload meter on cooldown
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
.\build\Release\seavious.exe
```

Or just open the folder in Visual Studio 2022 — it detects `CMakeLists.txt`
and `vcpkg.json` automatically (manifest mode) and configures itself.

## Controls

Arrow keys / WASD to move, within the 512x352 play area (the bottom 32px
is reserved HUD space). No shooting yet, and the scrolling background is
still a placeholder star field until the ocean tile art exists — see
`TODO.md`.
