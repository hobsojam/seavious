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
// Applies one stage's clear award to the run (the boss salvage calls
// this with the current stage's descriptor award).
void ApplyUpgradeAward(GameState *state, UpgradeAward award);
// Grants the awards of stages 1..lastClearedStage, clamped to the table:
// the canonical loadout for a devtools start at stage lastClearedStage+1.
// Debug entry points only - normal progression carries upgrades through
// BeginStage, and auto-granting there would mask a broken salvage.
void GrantUpgradesThroughStage(GameState *state, int lastClearedStage);
// Devtools fast-forward (--boss): parks the run at the map end with the
// event cursor spent, so the next script update raises the boss lock
// cleanly instead of mass-firing every skipped spawn.
void SkipToStageEnd(GameState *state);
// What the end-of-run CONTINUE input does: stage clear advances the run
// (score, lives, and the salvaged mortar carry over), game over forfeits
// it back to a fresh Stage 1 start. Pushes the RUN_RESTARTED event.
void ContinueRun(GameState *state);

#endif
