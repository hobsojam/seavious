# Game design and progression

[Back to the README](../README.md) · [Stage authoring](stage-authoring.md) ·
[Development guide](development.md)

## Core rule: match the weapon to the target

Seavious is built around readable target classes rather than universal damage.
The gun destroys airborne threats; torpedoes destroy surface threats; the
mortar destroys land installations and can also damage water targets. Terrain
does not collide with the player or bullets, but it blocks torpedoes and
clamps their reticle in that lane. An armed torpedo detonates at a shore;
an unarmed one fizzles.

The torpedo normally follows the player's current horizontal lane to its
reticle. It arms after a short distance, then explodes on the first surface
target, boss blocker, shore, or reticle point it reaches. Before arming it
only applies direct impact damage. The Stage 2 targeting computer changes the
torpedo from a fixed lane shot to a lead solution against a suitable surface
target. If no valid target exists it falls back to the ordinary fixed-lane
shot. Land targets are never selected: a torpedo cannot hit them.

After the Stage 1 salvage, the mortar lobs from the player to a shorter green
reticle. Its arcing shell passes over terrain and armour, and its blast affects
land targets, surface targets, and the relevant boss parts. Its ground shadow
moves with the shell; the persistent green impact ring telegraphs the landing
position.

Stage 3 introduces wind drift: a persistent current that pushes the player and
in-flight torpedoes off their aimed line. The player must continuously
counter-steer to hold a lane; a fixed-lane shot fired across a crosswind drifts
off target the same way it misses a moving surface target, so the Stage 2
lead-torpedo solve — already active from Stage 2 onward — is extended to net
out drift as well. The Stage 3 salvage instead removes drift outright: it does
not change how the shot is aimed, it removes the current the aim was
correcting for. Direction reverses on a timer (currently every ~30 seconds,
`WIND_DIRECTION_CHANGE_INTERVAL`) rather than pushing one way for the whole
run — playtest feedback was that a single constant direction read as broken
gravity, not weather. Magnitude is still fixed per stage, and the reversal
isn't yet telegraphed visually (still open, see TODO.md) — the player only
learns the new direction by feel.

## Threat roster

| Class | Targets | Counter |
| --- | --- | --- |
| Air | Skimmer Drone, Interceptor, Gunship | Gun |
| Surface | Casemate, Tracking Turret, Relay Node, Mine, Mobile Platform | Torpedo (or mortar blast) |
| Land | Mortar Battery, Drone Bunker | Mortar |

Skimmer Drones form the basic sine-wave swarm. Interceptors make a single
fast aimed shot, while Gunships withstand several gun hits and fire spreads.
Casemates fire straight lane shots; Tracking Turrets lead the player. Relay
Nodes launch drones, mines create proximity blast zones, and Mobile Platforms
move and fire fans.

Mortar Batteries return lobbed shells with a landing telegraph. Drone Bunkers
launch drones from terrain pads. Every land installation is authored on a
hardpoint; empty decorative hardpoints are deliberately avoided.

Rogue waves, introduced in Stage 3, are the one deliberate exception to the
class rule above: a telegraphed swell that surges into a wall sweeping the
entire play width, with no weapon counter and no target to destroy - a real,
sustained obstacle rather than a point hazard confined to one spot on screen.
One lane-band around the spawn lane stays passable as the front sweeps
through; everywhere else in its path is a hit. The existing air/surface/land
roster is not replaced in Stage 3; it returns under drift and rogue-wave
pressure rather than as new enemy types.

## Run structure

A run begins with three lives in Stage 1. Enemy contact and hostile shots cost
a life; the player briefly respawns invulnerable while lives remain. A game
over forfeits score and upgrades. Clearing a stage preserves them and advances
to the next stage; after Stage 3, the run wraps to Stage 1.

### Stage 1 — open-ocean tutorial

Stage 1 introduces gun-versus-air and torpedo-versus-surface combat on sparse
islets. Its Leviathan-class dreadnought has gun-vulnerable AA pods,
torpedo-vulnerable hull sections, homing SAMs, an armoured mortar dome, and a
core revealed by destroying the hull sections. The boss patrols vertically;
only the player-facing broadside is torpedo-reachable. Defeat salvages its
mortar dome, unlocking the player mortar for the rest of the run.

### Stage 2 — archipelago exam

Stage 2 turns shorelines into tactical lanes. It begins by teaching exposed
Mortar Batteries and Drone Bunkers, builds into a strait run, then ends at the
fortress atoll. Its boss combines gun-vulnerable pods, mortar-only ring
batteries, cycling sea gates, and a protected core. Its salvage is the
targeting computer: the luminous core module lifts out and docks onto the
player ship, enabling lead torpedoes.

### Stage 3 — the squall

Stage 3 turns the sea itself into the opponent: wind drift and rogue waves
demand continuous, active piloting on top of the target-class combat already
learned, while the existing air/surface/land roster returns under those
harder conditions rather than as new enemy types. It ends at the Storm
Warden, a fixed weather-control installation that cycles between a STORM
phase (intensified drift and waves, its parts shielded) and a brief CALM
phase (parts vulnerable) — the fortress atoll's gate-cycle pattern reused as
weather instead of a physical gate, so the skill the stage spent teaching
becomes the boss's own mechanic. Its salvage is the stabilizer, which cancels
wind drift and steadies the lead-torpedo solve for the rest of the run.

Stage maps, encounter beats, and glyphs are documented in
[Stage authoring](stage-authoring.md). Future work belongs in [TODO.md](../TODO.md),
not in this design record.
