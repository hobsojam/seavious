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

## Run structure

A run begins with three lives in Stage 1. Enemy contact and hostile shots cost
a life; the player briefly respawns invulnerable while lives remain. A game
over forfeits score and upgrades. Clearing a stage preserves them and advances
to the next stage; after Stage 2, the current two-stage run wraps to Stage 1.

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

Stage maps, encounter beats, and glyphs are documented in
[Stage authoring](stage-authoring.md). Future work belongs in [TODO.md](../TODO.md),
not in this design record.
