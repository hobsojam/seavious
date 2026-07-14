#ifndef SEAVIOUS_GAME_UPDATE_H
#define SEAVIOUS_GAME_UPDATE_H

#include "game_state.h"
#include "assets.h"

// One full simulation step: input, run flow (death/respawn/restart),
// spawning, movement, collision, scoring, and effect lifetimes.
// forceTorpedoFire lets the CI smoke harness fire without a keypress.
void UpdateGame(GameState *state, const GameAssets *assets, float dt, bool forceTorpedoFire);

#endif
