#ifndef SEAVIOUS_STAGE_H
#define SEAVIOUS_STAGE_H

#include "game_state.h"

// Advances stage scroll distance, fires due spawn events from the
// current stage's compiled table, and raises the boss lock when the map
// ends. Scroll distance - not wall-clock time - is the trigger currency,
// so the boss lock halting scroll also halts spawns with no special
// casing, and the script runs on through the 0.6s per-life death pause.
void UpdateStageScript(GameState *state, float dt);

// The stage table, numbered from 1. Out-of-range numbers clamp to a
// valid stage, so the wrap in the stage-advance flow can never
// dereference nothing.
int StageCount(void);
const StageDescriptor *GetStageDescriptor(int stageNumber);
// The stage-clear CONTINUE target: the next stage, wrapping the last
// back to Stage 1.
int NextStageNumber(int stageNumber);

#endif
