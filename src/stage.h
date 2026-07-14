#ifndef SEAVIOUS_STAGE_H
#define SEAVIOUS_STAGE_H

#include "game_state.h"

// Advances stage scroll distance, fires due spawn events from the
// compiled stage table (src/stage1_data.c), and raises the boss lock
// when the map ends. Scroll distance - not wall-clock time - is the
// trigger currency, so the boss lock halting scroll also halts spawns
// with no special casing, and the script runs on through the 0.6s
// per-life death pause.
void UpdateStageScript(GameState *state, float dt);

#endif
