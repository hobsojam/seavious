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
    }
}

void UpdateStageScript(GameState *state, float dt) {
    if (state->bossLock) return;

    state->scrollDistance += OCEAN_SCROLL_SPEED * dt;

    while (state->stageCursor < STAGE1_EVENT_COUNT
           && (float)STAGE1_EVENTS[state->stageCursor].px <= state->scrollDistance) {
        FireSpawnEvent(state, &STAGE1_EVENTS[state->stageCursor]);
        state->stageCursor++;
    }

    if (state->scrollDistance >= (float)STAGE1_LENGTH_PX) {
        state->bossLock = true;
        // The boss fight structure doesn't exist yet; raising bossActive
        // makes the lock audible (hammer theme hard-cuts in) until the
        // fight itself owns this flag.
        state->bossActive = true;
    }
}
