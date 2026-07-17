#include "game_state.h"
#include <string.h>

void ResetRunState(GameState *state) {
    // Zeroing the whole struct is the reset: every pool slot inactive, every
    // timer and flag cleared (TORPEDO_IMPACT_NONE is 0). Only the fields
    // with non-zero starting values follow.
    memset(state, 0, sizeof *state);
    state->player = (Vector2){ PLAYER_START_X, PLAYER_START_Y };
    state->lives = 3;
    state->stageNumber = 1;
}

void BeginStage(GameState *state, int stageNumber) {
    // Run progression survives the stage transition; everything else
    // rewinds exactly like a fresh run.
    int score = state->score;
    int lives = state->lives;
    bool hasMortar = state->hasMortar;
    bool hasTargetingComputer = state->hasTargetingComputer;
    ResetRunState(state);
    state->score = score;
    state->lives = lives;
    state->hasMortar = hasMortar;
    state->hasTargetingComputer = hasTargetingComputer;
    state->stageNumber = stageNumber;
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
    state->mortarShell.active = false;
    state->mortarCooldown = 0.0f;
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
            if (event->target.surfaceTarget == SURFACE_TARGET_MINE) radius = MINE_BLAST_RADIUS;
            if (event->target.surfaceTarget == SURFACE_TARGET_MOBILE_PLATFORM) radius = MOBILE_PLATFORM_RADIUS;
            TrySpawnExplosion(explosions, event->pos, EXPLOSION_SURFACE_TARGET,
                event->target.surfaceTarget == SURFACE_TARGET_MINE ? MINE_BLAST_RADIUS : radius + 5.0f,
                event->target.surfaceTarget == SURFACE_TARGET_MINE ? MINE_BLAST_DURATION : 0.38f);
            // Mines detonate to nothing - there is no burnt-out hull left
            // to wreck, however they died.
            if (event->target.surfaceTarget != SURFACE_TARGET_MINE) {
                TrySpawnSurfaceWreck(wrecks, event->pos, event->target.surfaceTarget, radius);
            }
        } else if (event->type == GAME_EVENT_MINE_DETONATED) {
            // Proximity detonation: a blast clearly bigger than the mine,
            // with matching gameplay damage and no wreck or score.
            TrySpawnExplosion(explosions, event->pos, EXPLOSION_SURFACE_TARGET,
                MINE_BLAST_RADIUS, MINE_BLAST_DURATION);
        } else if (event->type == GAME_EVENT_BOSS_PART_DESTROYED) {
            // Pods burst in the air family's magenta; hull sections and
            // the core in the surface family's orange. The burnt socket a
            // destroyed part leaves is drawn by the boss renderer on top
            // of the hull sprite (the wreck pool draws under the hull, so
            // it can't carry these).
            bool pod = event->target.bossPart == BOSS_PART_POD_FORE
                || event->target.bossPart == BOSS_PART_POD_AFT;
            bool core = event->target.bossPart == BOSS_PART_CORE;
            TrySpawnExplosion(explosions, event->pos,
                pod ? EXPLOSION_AIR_TARGET : EXPLOSION_SURFACE_TARGET,
                core ? 22.0f : 16.0f, core ? 0.55f : 0.45f);
        } else if (event->type == GAME_EVENT_BOSS_MISSILE_DOWNED) {
            // A missile shot out of the air: small airborne pop, no wreck.
            TrySpawnExplosion(explosions, event->pos, EXPLOSION_AIR_TARGET, 7.0f, 0.22f);
        } else if (event->type == GAME_EVENT_BOSS_DEFEATED) {
            // Capstone of the death chain: the biggest single burst in
            // the game, at the hull center.
            TrySpawnExplosion(explosions, event->pos, EXPLOSION_SURFACE_TARGET, 28.0f, 0.6f);
        } else if (event->type == GAME_EVENT_MORTAR_FIRED) {
            // Launch puff at the dome; the shell/shadow/blast themselves
            // are code-drawn VFX from the shell state, not effects.
            TrySpawnExplosion(explosions, event->pos, EXPLOSION_SURFACE_TARGET, 6.0f, 0.18f);
        } else if (event->type == GAME_EVENT_LAND_TARGET_DESTROYED) {
            // Green burst for the land family; no wreck - the scorched
            // hardpoint pad it stood on stays behind as the marker.
            float radius = event->target.landTarget == LAND_TARGET_DRONE_BUNKER
                ? DRONE_BUNKER_RADIUS : MORTAR_BATTERY_RADIUS;
            TrySpawnExplosion(explosions, event->pos, EXPLOSION_LAND_TARGET, radius + 5.0f, 0.38f);
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
