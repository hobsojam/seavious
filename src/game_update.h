#ifndef SEAVIOUS_GAME_UPDATE_H
#define SEAVIOUS_GAME_UPDATE_H

#include "game_state.h"
#include "assets.h"

// One full simulation step: input, run flow (death/respawn/restart),
// spawning, movement, collision, scoring, and effect lifetimes.
// Forced-fire flags let the CI smoke harness exercise manual weapons
// deterministically without synthesizing keyboard input.
void UpdateGame(GameState *state, const GameAssets *assets, float dt,
    bool forceTorpedoFire, bool forceMortarFire);

#endif
