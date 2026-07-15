#ifndef SEAVIOUS_BOSS_H
#define SEAVIOUS_BOSS_H

#include "game_state.h"

// Stage 1 boss: Leviathan-class dreadnought (see README "Stage 1 boss
// design"). UpdateBossFight owns the whole arc: it starts the fight when
// the stage script raises the boss lock (taking over bossActive for the
// music hard-cuts), runs the vertical patrol and part-driven fight, the
// death chain, and the salvage sequence that ends in stageClear.
// Player-weapon collisions against parts follow the strict class
// mapping: gun bullets touch only pods (and the exposed core), torpedoes
// only the player-facing hull section (and the exposed core through the
// breach gap) - the armored hull itself blocks torpedoes under the same
// rules as land (BossHullBlockers), the mortar dome has no weapon
// interaction at all, and the settled wreck is inert like every wreck.
void UpdateBossFight(GameState *state, float dt);

// The armored hull's screen-space torpedo blockers: one rect while the
// armor holds, two (split at the breach gap) once the core is exposed,
// none once the boss is a wreck. Also the contact-damage shape.
int BossHullBlockers(const BossState *boss, Rectangle out[2]);

// True while the broadside section is on the player's side of the ship
// (torpedo-reachable, and allowed to fire).
bool BossSectionFacesPlayer(const BossState *boss, BossPartId part);

// True from the salvage autopilot onward: input, weapons, and the reticle
// are the sequence's, not the player's.
bool BossSequenceOwnsPlayer(const GameState *state);

// Player hazards. Contact with the hull is lethal only while the boss
// lives (ENTERING/FIGHTING) - the settled wreck is inert like any wreck.
bool ResolveBossContactDamage(const BossState *boss, Vector2 playerPos, float playerRadius);
bool ResolveMortarBlastPlayerHit(const BossState *boss, Vector2 playerPos, float playerRadius);

// Torpedo-vs-part: mirrors the surface-target rules - an armed hit
// returns EXPLOSION and leaves the damage to the splash pass, an unarmed
// hit deals its direct damage on the spot.
TorpedoImpact ResolveTorpedoBossPartCollision(Torpedo *torpedo, BossState *boss, GameEventQueue *events);
void ResolveBossSplashDamage(BossState *boss, Vector2 pos, GameEventQueue *events);

Vector2 BossPartPosition(const BossState *boss, BossPartId part);
Vector2 BossMortarPosition(const BossState *boss);
bool BossPartAlive(const BossState *boss, BossPartId part);

// Boss bar currency: gun-hit equivalents across all destructible parts
// (hull-section torpedoes count BOSS_HULL_SECTION_TORPEDO_WORTH each).
int BossRemainingHp(const BossState *boss);
int BossTotalHp(void);

#endif
