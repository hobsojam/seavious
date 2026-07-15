#include "game_state.h"
#include <string.h>

void ResetRunState(GameState *state) {
    // Zeroing the whole struct is the reset: every pool slot inactive, every
    // timer and flag cleared (TORPEDO_IMPACT_NONE is 0). Only the fields
    // with non-zero starting values follow.
    memset(state, 0, sizeof *state);
    state->player = (Vector2){ PLAYER_START_X, PLAYER_START_Y };
    state->lives = 3;
}

void BeginPlayerDeath(GameState *state) {
    state->lives--;
    if (state->lives < 0) state->lives = 0;
    state->playerDestroyed = true;
    state->playerDeathTimer = PLAYER_DEATH_DURATION;
    state->torpedo.active = false;
    state->torpedoCooldown = 0.0f;
    state->torpedoImpactType = TORPEDO_IMPACT_NONE;
    state->torpedoImpactTimer = 0.0f;
    PushGameEvent(&state->gameEvents, (GameEvent){
        .type = GAME_EVENT_PLAYER_DEATH, .pos = state->player
    });
}

bool TrySpawnExplosion(ExplosionEffect effects[], Vector2 pos, ExplosionType type, float radius, float lifetime) {
    for (int i = 0; i < MAX_EXPLOSION_EFFECTS; i++) {
        if (effects[i].active) continue;
        effects[i] = (ExplosionEffect){ pos, 0.0f, lifetime, radius, type, true };
        return true;
    }
    return false;
}

bool TrySpawnSurfaceWreck(SurfaceWreck wrecks[], Vector2 pos, SurfaceTargetType type, float radius) {
    for (int i = 0; i < MAX_SURFACE_WRECKS; i++) {
        if (wrecks[i].active) continue;
        wrecks[i] = (SurfaceWreck){ pos, radius, type, true };
        return true;
    }
    return false;
}

void SpawnTargetDestructionEffects(const GameEventQueue *events, ExplosionEffect explosions[], SurfaceWreck wrecks[]) {
    for (int i = 0; i < events->count; i++) {
        const GameEvent *event = &events->items[i];
        if (event->type == GAME_EVENT_AIR_TARGET_DESTROYED) {
            // The Gunship's bulk earns a bigger burst than the darts.
            float airRadius = event->target.airTarget == AIR_TARGET_GUNSHIP ? 14.0f : 10.0f;
            TrySpawnExplosion(explosions, event->pos, EXPLOSION_AIR_TARGET, airRadius, 0.28f);
        } else if (event->type == GAME_EVENT_SURFACE_TARGET_DESTROYED) {
            float radius = TRACKING_TURRET_RADIUS;
            if (event->target.surfaceTarget == SURFACE_TARGET_CASEMATE) radius = CASEMATE_RADIUS;
            if (event->target.surfaceTarget == SURFACE_TARGET_RELAY_NODE) radius = RELAY_NODE_RADIUS;
            if (event->target.surfaceTarget == SURFACE_TARGET_MINE) radius = MINE_RADIUS;
            if (event->target.surfaceTarget == SURFACE_TARGET_MOBILE_PLATFORM) radius = MOBILE_PLATFORM_RADIUS;
            TrySpawnExplosion(explosions, event->pos, EXPLOSION_SURFACE_TARGET, radius + 5.0f, 0.38f);
            // Mines detonate to nothing - there is no burnt-out hull left
            // to wreck, however they died.
            if (event->target.surfaceTarget != SURFACE_TARGET_MINE) {
                TrySpawnSurfaceWreck(wrecks, event->pos, event->target.surfaceTarget, radius);
            }
        } else if (event->type == GAME_EVENT_MINE_DETONATED) {
            // Contact detonation: a blast clearly bigger than the mine
            // itself, but no wreck and (via ScoreGameEvents) no score.
            TrySpawnExplosion(explosions, event->pos, EXPLOSION_SURFACE_TARGET, MINE_RADIUS + 8.0f, 0.38f);
        }
    }
}

void UpdateExplosionEffects(ExplosionEffect effects[], float dt) {
    for (int i = 0; i < MAX_EXPLOSION_EFFECTS; i++) {
        if (!effects[i].active) continue;
        effects[i].age += dt;
        if (effects[i].age >= effects[i].lifetime) effects[i].active = false;
    }
}

void UpdateSurfaceWrecks(SurfaceWreck wrecks[], float dt) {
    for (int i = 0; i < MAX_SURFACE_WRECKS; i++) {
        if (!wrecks[i].active) continue;
        wrecks[i].pos.x -= OCEAN_SCROLL_SPEED * dt;
        if (wrecks[i].pos.x < -wrecks[i].radius) wrecks[i].active = false;
    }
}
