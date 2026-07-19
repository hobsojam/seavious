#include "stage.h"
#include "stage_data.h"

static void FireSpawnEvent(GameState *state, const StageSpawnEvent *event) {
    switch (event->kind) {
        case STAGE_SPAWN_DRONE:
            TrySpawnSkimmerDrone(state->airTargets, MAX_AIR_TARGETS, event->laneY);
            break;
        case STAGE_SPAWN_DRONE_LINE3:
            SpawnSkimmerDroneLine(state->airTargets, MAX_AIR_TARGETS, event->laneY);
            break;
        case STAGE_SPAWN_DRONE_V5:
            SpawnSkimmerDroneV(state->airTargets, MAX_AIR_TARGETS, event->laneY);
            break;
        case STAGE_SPAWN_INTERCEPTOR:
            TrySpawnInterceptor(state->airTargets, MAX_AIR_TARGETS, event->laneY);
            break;
        case STAGE_SPAWN_GUNSHIP:
            TrySpawnGunship(state->airTargets, MAX_AIR_TARGETS, event->laneY);
            break;
        case STAGE_SPAWN_CASEMATE:
            TrySpawnCasemate(state->surfaceTargets, MAX_SURFACE_TARGETS, event->laneY);
            break;
        case STAGE_SPAWN_TRACKING_TURRET:
            TrySpawnTrackingTurret(state->surfaceTargets, MAX_SURFACE_TARGETS, event->laneY);
            break;
        case STAGE_SPAWN_RELAY_NODE:
            TrySpawnRelayNode(state->surfaceTargets, MAX_SURFACE_TARGETS, event->laneY);
            break;
        case STAGE_SPAWN_MINE:
            TrySpawnMine(state->surfaceTargets, MAX_SURFACE_TARGETS, event->laneY);
            break;
        case STAGE_SPAWN_MOBILE_PLATFORM:
            TrySpawnMobilePlatform(state->surfaceTargets, MAX_SURFACE_TARGETS, event->laneY);
            break;
        case STAGE_SPAWN_MORTAR_BATTERY:
            TrySpawnMortarBattery(state->landTargets, MAX_LAND_TARGETS, event->laneY);
            break;
        case STAGE_SPAWN_DRONE_BUNKER:
            TrySpawnDroneBunker(state->landTargets, MAX_LAND_TARGETS, event->laneY);
            break;
        case STAGE_SPAWN_ROGUE_WAVE:
            TrySpawnRogueWave(state->rogueWaves, MAX_ROGUE_WAVES, event->laneY);
            break;
    }
}

int StageCount(void) {
    return 2;
}

int NextStageNumber(int stageNumber) {
    if (stageNumber < 1 || stageNumber >= StageCount()) return 1;
    return stageNumber + 1;
}

void ContinueRun(GameState *state) {
    if (state->stageClear) {
        // Stage clear advances the run: score, lives, and the salvaged
        // mortar carry into the next stage, wrapping back to Stage 1
        // until more stages exist.
        BeginStage(state, NextStageNumber(state->stageNumber));
    } else {
        // Game over forfeits the run: fresh start at Stage 1.
        ResetRunState(state);
    }
    PushGameEvent(&state->gameEvents, (GameEvent){
        .type = GAME_EVENT_RUN_RESTARTED, .pos = state->player
    });
}

const StageDescriptor *GetStageDescriptor(int stageNumber) {
    // Rebuilt on each call: the generated counts are const ints, not
    // constant expressions, so a static initializer can't hold them.
    static StageDescriptor stages[2];
    stages[0] = (StageDescriptor){
        .events = STAGE1_EVENTS,
        .eventCount = STAGE1_EVENT_COUNT,
        .terrain = STAGE1_TERRAIN,
        .terrainCount = STAGE1_TERRAIN_COUNT,
        .hardpoints = STAGE1_TERRAIN_HARDPOINTS,
        .hardpointCount = STAGE1_TERRAIN_HARDPOINT_COUNT,
        .lengthPx = STAGE1_LENGTH_PX,
        .award = UPGRADE_AWARD_MORTAR,
    };
    stages[1] = (StageDescriptor){
        .events = STAGE2_EVENTS,
        .eventCount = STAGE2_EVENT_COUNT,
        .terrain = STAGE2_TERRAIN,
        .terrainCount = STAGE2_TERRAIN_COUNT,
        .hardpoints = STAGE2_TERRAIN_HARDPOINTS,
        .hardpointCount = STAGE2_TERRAIN_HARDPOINT_COUNT,
        .lengthPx = STAGE2_LENGTH_PX,
        .award = UPGRADE_AWARD_TARGETING_COMPUTER,
    };
    if (stageNumber < 1 || stageNumber > 2) stageNumber = 1;
    return &stages[stageNumber - 1];
}

void ApplyUpgradeAward(GameState *state, UpgradeAward award) {
    switch (award) {
        case UPGRADE_AWARD_MORTAR: state->hasMortar = true; break;
        case UPGRADE_AWARD_TARGETING_COMPUTER: state->hasTargetingComputer = true; break;
        case UPGRADE_AWARD_STABILIZER: state->hasStabilizer = true; break;
        case UPGRADE_AWARD_NONE: break;
    }
}

void GrantUpgradesThroughStage(GameState *state, int lastClearedStage) {
    if (lastClearedStage > StageCount()) lastClearedStage = StageCount();
    for (int s = 1; s <= lastClearedStage; s++) {
        ApplyUpgradeAward(state, GetStageDescriptor(s)->award);
    }
}

void SkipToStageEnd(GameState *state) {
    const StageDescriptor *stage = GetStageDescriptor(state->stageNumber);
    state->scrollDistance = (float)stage->lengthPx;
    state->stageCursor = stage->eventCount;
}

void UpdateStageScript(GameState *state, float dt) {
    if (state->bossLock) return;

    const StageDescriptor *stage = GetStageDescriptor(state->stageNumber);
    state->scrollDistance += OCEAN_SCROLL_SPEED * dt;

    while (state->stageCursor < stage->eventCount
           && (float)stage->events[state->stageCursor].px <= state->scrollDistance) {
        FireSpawnEvent(state, &stage->events[state->stageCursor]);
        state->stageCursor++;
    }

    if (state->scrollDistance >= (float)stage->lengthPx) {
        // UpdateBossFight sees the lock and starts the fight (it owns
        // bossActive and with it the music hard-cuts).
        state->bossLock = true;
    }
}
