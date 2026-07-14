#include "game_update.h"
#include "raylib.h"

void UpdateGame(GameState *state, const GameAssets *assets, float dt, bool forceTorpedoFire) {
    state->gameEvents.count = 0;
    float halfW = assets->playerTex.width / 2.0f;
    float halfH = assets->playerTex.height / 2.0f;
    float playerRadius = PLAYER_HIT_RADIUS;

    if (state->gameOver && (IsKeyPressed(KEY_R) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))) {
        ResetRunState(state);
    }
    if (state->gameOver) return;

    Vector2 playerVelocity = { 0.0f, 0.0f };
    if (state->playerDestroyed) {
        state->playerDeathTimer -= dt;
        if (state->playerDeathTimer <= 0.0f) {
            state->playerDestroyed = false;
            if (state->lives > 0) {
                state->player = (Vector2){ PLAYER_START_X, PLAYER_START_Y };
                state->respawnInvulnerability = PLAYER_RESPAWN_INVULNERABILITY;
            } else {
                state->gameOver = true;
            }
        }
    } else {
        if (state->respawnInvulnerability > 0.0f) state->respawnInvulnerability -= dt;

        float inputX = 0.0f;
        float inputY = 0.0f;
        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A))  inputX -= 1.0f;
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) inputX += 1.0f;
        if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W))    inputY -= 1.0f;
        if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S))  inputY += 1.0f;
        Vector2 playerBeforeMove = state->player;
        MovePlayer(&state->player, inputX, inputY, PLAYER_SPEED, dt, halfW, halfH);
        if (dt > 0.0f) {
            playerVelocity = (Vector2){
                (state->player.x - playerBeforeMove.x) / dt, (state->player.y - playerBeforeMove.y) / dt
            };
        }

        // Wake: drop a puff from each rear ski-point on a fixed cadence.
        // The 1px jitter keeps the two trails from reading as ruled lines.
        state->wakeEmitTimer += dt;
        while (state->wakeEmitTimer >= WAKE_EMIT_INTERVAL) {
            state->wakeEmitTimer -= WAKE_EMIT_INTERVAL;
            for (int ski = -1; ski <= 1; ski += 2) {
                TryEmitWakeParticle(state->wake, MAX_WAKE_PARTICLES, (Vector2){
                    state->player.x - halfW + (float)GetRandomValue(-1, 1),
                    state->player.y + ski * WAKE_SKI_OFFSET_Y + (float)GetRandomValue(-1, 1)
                });
            }
        }

        // Auto-fire: accumulate dt and emit as many shots as the interval
        // allows, so a stalled frame can't silently eat a shot.
        state->fireTimer += dt;
        while (state->fireTimer >= BULLET_FIRE_INTERVAL) {
            state->fireTimer -= BULLET_FIRE_INTERVAL;
            TrySpawnBullet(state->bullets, MAX_BULLETS, (Vector2){ state->player.x + halfW, state->player.y });
        }
    }

    // World scrolls right-to-left under the player. Kept bounded by
    // the tile width since REPEAT wrap only needs a modulo tile offset.
    state->oceanScroll = AdvanceOceanScroll(state->oceanScroll, dt, (float)assets->oceanTex.width);
    state->oceanOverlayScroll = AdvanceOceanScroll(
        state->oceanOverlayScroll, dt * OCEAN_OVERLAY_SPEED_SCALE, (float)assets->oceanOverlayTex.width
    );

    UpdateWakeParticles(state->wake, MAX_WAKE_PARTICLES, dt);
    if (state->torpedoImpactTimer > 0.0f) state->torpedoImpactTimer -= dt;
    UpdateBullets(state->bullets, MAX_BULLETS, dt);

    // Enemies spawn off the right edge and fly left to meet the player,
    // opposite the bullets, since the world scrolls the other way under them.
    state->airTargetSpawnTimer += dt;
    while (state->airTargetSpawnTimer >= SKIMMER_DRONE_SPAWN_INTERVAL) {
        state->airTargetSpawnTimer -= SKIMMER_DRONE_SPAWN_INTERVAL;
        float margin = SKIMMER_DRONE_RADIUS + SKIMMER_DRONE_SINE_AMPLITUDE;
        float baseY = (float)GetRandomValue((int)margin, (int)(PLAY_HEIGHT - margin));
        TrySpawnSkimmerDrone(state->airTargets, MAX_AIR_TARGETS, baseY);
    }
    UpdateAirTargets(state->airTargets, MAX_AIR_TARGETS, dt);

    // Gun-vs-air collision: brute-force O(bullets*targets) is fine at
    // these pool sizes (32*16 max).
    ResolveBulletAirTargetCollisions(
        state->bullets, MAX_BULLETS, state->airTargets, MAX_AIR_TARGETS, &state->gameEvents
    );

    // Torpedo: manual second input, gated by both "one in flight at a
    // time" and a reload cooldown so it isn't unlimited-fire like the gun.
    if (state->torpedoCooldown > 0.0f) state->torpedoCooldown -= dt;
    bool fireTorpedo = IsKeyPressed(KEY_SPACE) || forceTorpedoFire;
    if (!state->playerDestroyed && fireTorpedo && !state->torpedo.active && state->torpedoCooldown <= 0.0f) {
        Vector2 torpedoSpawn = { state->player.x + halfW, state->player.y };
        FireFixedRangeTorpedo(&state->torpedo, torpedoSpawn);
        state->torpedoCooldown = TORPEDO_COOLDOWN;
    }
    TorpedoImpact torpedoImpact = UpdateTorpedo(&state->torpedo, dt);

    // Surface threats spawn off the right edge and drift left with the water.
    state->surfaceTargetSpawnTimer += dt;
    while (state->surfaceTargetSpawnTimer >= SURFACE_TARGET_SPAWN_INTERVAL) {
        state->surfaceTargetSpawnTimer -= SURFACE_TARGET_SPAWN_INTERVAL;
        float y = (float)GetRandomValue(
            (int)TRACKING_TURRET_RADIUS, (int)(PLAY_HEIGHT - TRACKING_TURRET_RADIUS)
        );
        if (GetRandomValue(0, 1) == 0) {
            TrySpawnCasemate(state->surfaceTargets, MAX_SURFACE_TARGETS, y);
        } else {
            TrySpawnTrackingTurret(state->surfaceTargets, MAX_SURFACE_TARGETS, y);
        }
    }
    UpdateSurfaceTargets(state->surfaceTargets, MAX_SURFACE_TARGETS, dt);
    UpdateSurfaceTargetFire(
        state->surfaceTargets, MAX_SURFACE_TARGETS, dt, state->player, playerVelocity,
        state->enemyBullets, MAX_ENEMY_BULLETS
    );
    UpdateEnemyBullets(state->enemyBullets, MAX_ENEMY_BULLETS, dt);

    // Torpedo-vs-surface collision — the gun deliberately has no case
    // here: bullets pass over ground targets (dual-targeting rule).
    if (torpedoImpact.type == TORPEDO_IMPACT_EXPLOSION) {
        ResolveTorpedoExplosion(
            torpedoImpact.pos, state->surfaceTargets, MAX_SURFACE_TARGETS, &state->gameEvents
        );
    } else if (torpedoImpact.type == TORPEDO_IMPACT_NONE) {
        torpedoImpact = ResolveTorpedoSurfaceTargetCollision(
            &state->torpedo, state->surfaceTargets, MAX_SURFACE_TARGETS, &state->gameEvents
        );
    }
    if (torpedoImpact.type != TORPEDO_IMPACT_NONE) {
        state->torpedoImpactType = torpedoImpact.type;
        state->torpedoImpactPos = torpedoImpact.pos;
        state->torpedoImpactTimer = torpedoImpact.type == TORPEDO_IMPACT_EXPLOSION ? 0.18f : 0.10f;
    }
    SpawnTargetDestructionEffects(&state->gameEvents, state->explosions, state->wrecks);
    state->score += ScoreGameEvents(&state->gameEvents);

    UpdateExplosionEffects(state->explosions, dt);
    UpdateSurfaceWrecks(state->wrecks, dt);

    bool enemyBulletHit = ResolveEnemyBulletPlayerCollision(
        state->enemyBullets, MAX_ENEMY_BULLETS, state->player, playerRadius
    );
    bool contactHit = ResolvePlayerContactDamage(
        state->player, playerRadius, state->airTargets, MAX_AIR_TARGETS,
        state->surfaceTargets, MAX_SURFACE_TARGETS
    );
    if (!state->playerDestroyed && state->respawnInvulnerability <= 0.0f && (enemyBulletHit || contactHit)) {
        TrySpawnExplosion(state->explosions, state->player, EXPLOSION_PLAYER, 20.0f, PLAYER_DEATH_DURATION);
        BeginPlayerDeath(state);
    }
}
