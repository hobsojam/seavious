#include "game_update.h"
#include "boss.h"
#include "input.h"
#include "stage.h"
#include "stage_data.h"
#include "raylib.h"

void UpdateGame(GameState *state, const GameAssets *assets, float dt,
    bool forceTorpedoFire, bool forceMortarFire) {
    state->gameEvents.count = 0;
    if (state->paused) return;
    UpdateMineBlasts(state->mineBlasts, MAX_MINE_BLASTS, dt);
    float halfW = assets->playerTex.width / 2.0f;
    float halfH = assets->playerTex.height / 2.0f;
    float playerRadius = PLAYER_HIT_RADIUS;

    // Advance or forfeit the run (ContinueRun in stage.c owns the flow).
    if ((state->gameOver || state->stageClear) && InputActionPressed(INPUT_RESTART)) {
        ContinueRun(state);
    }
    if (state->gameOver) return;

    // From the salvage autopilot on, the boss sequence flies the ship:
    // no input, no weapons, no reticle - the run is already won.
    bool bossOwnsPlayer = BossSequenceOwnsPlayer(state);

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
        if (!bossOwnsPlayer) {
            if (InputActionDown(INPUT_MOVE_LEFT))  inputX -= 1.0f;
            if (InputActionDown(INPUT_MOVE_RIGHT)) inputX += 1.0f;
            if (InputActionDown(INPUT_MOVE_UP))    inputY -= 1.0f;
            if (InputActionDown(INPUT_MOVE_DOWN))  inputY += 1.0f;
        }
        Vector2 playerBeforeMove = state->player;
        MovePlayer(&state->player, inputX, inputY, PLAYER_SPEED, dt, halfW, halfH);
        if (dt > 0.0f) {
            playerVelocity = (Vector2){
                (state->player.x - playerBeforeMove.x) / dt, (state->player.y - playerBeforeMove.y) / dt
            };
        }

        // Wake: the same two puffs per cadence as before, distributed across
        // the rear half of the hull instead of locked to its two ski tips.
        // The 1px jitter keeps that broad churn from reading as a grid.
        state->wakeEmitTimer += dt;
        while (state->wakeEmitTimer >= WAKE_EMIT_INTERVAL) {
            state->wakeEmitTimer -= WAKE_EMIT_INTERVAL;
            state->wakeEmitPhase = EmitPlayerWake(state->wake, MAX_WAKE_PARTICLES, state->player,
                halfW, halfH, state->wakeEmitPhase, (float)GetRandomValue(-1, 1),
                (float)GetRandomValue(-1, 1));
        }

        // Auto-fire: accumulate dt and emit as many shots as the interval
        // allows, so a stalled frame can't silently eat a shot.
        if (!bossOwnsPlayer) {
            state->fireTimer += dt;
            while (state->fireTimer >= BULLET_FIRE_INTERVAL) {
                state->fireTimer -= BULLET_FIRE_INTERVAL;
                Vector2 muzzle = { state->player.x + halfW, state->player.y };
                if (TrySpawnBullet(state->bullets, MAX_BULLETS, muzzle)) {
                    PushGameEvent(&state->gameEvents, (GameEvent){
                        .type = GAME_EVENT_GUN_FIRED, .pos = muzzle
                    });
                }
            }
        }
    }

    // The stage script owns all enemy spawning: scroll distance is the
    // trigger currency, so the boss lock freezing scroll also freezes
    // spawns and water-anchored drift below (scrollDt), while airborne
    // craft, projectiles, and timers keep running on real dt.
    UpdateStageScript(state, dt);
    // The boss fight starts the frame the script raises the lock, and
    // from then on owns bossActive, the part fight, and the salvage flow.
    UpdateBossFight(state, dt);
    // The boss's armored hull blocks torpedoes like land does; these are
    // its screen-space blocker rects for this frame (0 when no boss).
    Rectangle bossBlockers[3];
    int bossBlockerCount = BossHullBlockers(&state->boss, bossBlockers);
    float scrollDt = state->bossLock ? 0.0f : dt;

    // World scrolls right-to-left under the player. Kept bounded by
    // the tile width since REPEAT wrap only needs a modulo tile offset.
    state->oceanScroll = AdvanceOceanScroll(state->oceanScroll, scrollDt, (float)assets->oceanTex.width);
    state->oceanOverlayScroll = AdvanceOceanScroll(
        state->oceanOverlayScroll, scrollDt * OCEAN_OVERLAY_SPEED_SCALE, (float)assets->oceanOverlayTex.width
    );

    UpdateWakeParticles(state->wake, MAX_WAKE_PARTICLES, dt);
    if (state->torpedoImpactTimer > 0.0f) state->torpedoImpactTimer -= dt;
    UpdateBullets(state->bullets, MAX_BULLETS, dt);

    UpdateAirTargets(state->airTargets, MAX_AIR_TARGETS, dt);
    // Air return fire (Interceptor one-shot, Gunship spreads) shares the
    // enemy bullet pool with the surface installations.
    UpdateAirTargetFire(
        state->airTargets, MAX_AIR_TARGETS, dt, state->player, state->enemyBullets, MAX_ENEMY_BULLETS,
        &state->gameEvents
    );

    // Gun-vs-air collision: brute-force O(bullets*targets) is fine at
    // these pool sizes (32*16 max).
    ResolveBulletAirTargetCollisions(
        state->bullets, MAX_BULLETS, state->airTargets, MAX_AIR_TARGETS, &state->gameEvents
    );

    // The current stage's compiled content (terrain for the torpedo lane
    // checks below; events/length are the stage script's business).
    const StageDescriptor *stage = GetStageDescriptor(state->stageNumber);

    // Torpedo: manual second input, gated by both "one in flight at a
    // time" and a reload cooldown so it isn't unlimited-fire like the gun.
    if (state->torpedoCooldown > 0.0f) state->torpedoCooldown -= dt;
    bool fireTorpedo = (InputActionPressed(INPUT_FIRE_TORPEDO) || forceTorpedoFire) && !bossOwnsPlayer;
    if (!state->playerDestroyed && fireTorpedo && !state->torpedo.active && state->torpedoCooldown <= 0.0f) {
        Vector2 torpedoSpawn = { state->player.x + halfW, state->player.y };
        if (state->hasTargetingComputer) {
            FireLeadTorpedo(&state->torpedo, torpedoSpawn, state->surfaceTargets, MAX_SURFACE_TARGETS);
        } else {
            FireFixedRangeTorpedo(&state->torpedo, torpedoSpawn, stage->terrain, stage->terrainCount,
                state->scrollDistance);
        }
        // The boss's armor clamps the range like a land edge would.
        state->torpedo.target = ClampReticleToRects(
            torpedoSpawn, state->torpedo.target, bossBlockers, bossBlockerCount
        );
        state->torpedoCooldown = TORPEDO_COOLDOWN;
        PushGameEvent(&state->gameEvents, (GameEvent){
            .type = GAME_EVENT_TORPEDO_FIRED, .pos = torpedoSpawn
        });
    }
    TorpedoImpact torpedoImpact = UpdateTorpedo(&state->torpedo, dt);
    // Land blocks torpedoes (checked before target collision, so a target
    // tucked behind a shoreline is genuinely shielded): armed shots
    // detonate at the land edge - the explosion branch below still splashes
    // shoreline targets - and unarmed shots fizzle as a splash-less DIRECT.
    if (torpedoImpact.type == TORPEDO_IMPACT_NONE) {
        torpedoImpact = ResolveTorpedoTerrainCollision(
            &state->torpedo, stage->terrain, stage->terrainCount, state->scrollDistance
        );
    }

    // Surface threats are world-anchored: they drift with the water, so
    // their motion freezes with the scroll under the boss lock while
    // their fire control below keeps running.
    UpdateSurfaceTargets(state->surfaceTargets, MAX_SURFACE_TARGETS, scrollDt);
    // Mobile Platform stern trails share the wake pool; scrollDt keeps the
    // emission frozen with the hull under the boss lock.
    EmitMobilePlatformWake(state->surfaceTargets, MAX_SURFACE_TARGETS, state->wake, MAX_WAKE_PARTICLES, scrollDt);
    UpdateSurfaceTargetFire(
        state->surfaceTargets, MAX_SURFACE_TARGETS, dt, state->player, playerVelocity,
        state->enemyBullets, MAX_ENEMY_BULLETS, &state->gameEvents
    );
    UpdateRelayNodeLaunches(
        state->surfaceTargets, MAX_SURFACE_TARGETS, dt, state->airTargets, MAX_AIR_TARGETS,
        &state->gameEvents
    );
    // Land installations: anchored drift on scrollDt like the terrain
    // they mount, fire control on real dt like every other enemy.
    UpdateLandTargets(state->landTargets, MAX_LAND_TARGETS, scrollDt);
    UpdateMortarBatteries(state->landTargets, MAX_LAND_TARGETS, dt, state->player, &state->gameEvents);
    UpdateDroneBunkerLaunches(
        state->landTargets, MAX_LAND_TARGETS, dt, state->airTargets, MAX_AIR_TARGETS,
        &state->gameEvents
    );
    UpdateEnemyBullets(state->enemyBullets, MAX_ENEMY_BULLETS, dt);

    // The fortress atoll's painted land banks are real torpedo blockers,
    // so they take priority over its pad defenses.  The Leviathan instead
    // keeps its proud part-before-hull ordering.
    if (torpedoImpact.type == TORPEDO_IMPACT_NONE && BossIsFortressAtoll(&state->boss)) {
        torpedoImpact = ResolveTorpedoRectCollision(&state->torpedo, bossBlockers, bossBlockerCount);
    }

    // Torpedo-vs-surface collision — the gun deliberately has no case
    // here: bullets pass over ground targets (dual-targeting rule). Boss
    // parts join the same chain: hull sections (and the exposed core)
    // take direct hits, and an armed explosion's splash reaches both
    // surface targets and boss parts.
    if (torpedoImpact.type == TORPEDO_IMPACT_NONE) {
        torpedoImpact = ResolveTorpedoBossPartCollision(&state->torpedo, &state->boss, &state->gameEvents);
    }
    // The armored hull itself: parts are checked first because the
    // player-facing section sits proud of the armor edge; a torpedo in
    // any other lane detonates (or fizzles) on the hull like on land.
    if (torpedoImpact.type == TORPEDO_IMPACT_NONE && !BossIsFortressAtoll(&state->boss)) {
        torpedoImpact = ResolveTorpedoRectCollision(&state->torpedo, bossBlockers, bossBlockerCount);
    }
    if (torpedoImpact.type == TORPEDO_IMPACT_EXPLOSION) {
        ResolveTorpedoExplosion(
            torpedoImpact.pos, state->surfaceTargets, MAX_SURFACE_TARGETS, &state->gameEvents
        );
        ResolveBossSplashDamage(&state->boss, torpedoImpact.pos, &state->gameEvents);
    } else if (torpedoImpact.type == TORPEDO_IMPACT_NONE) {
        torpedoImpact = ResolveTorpedoSurfaceTargetCollision(
            &state->torpedo, state->surfaceTargets, MAX_SURFACE_TARGETS, &state->gameEvents
        );
    }
    if (torpedoImpact.type != TORPEDO_IMPACT_NONE) {
        state->torpedoImpactType = torpedoImpact.type;
        state->torpedoImpactPos = torpedoImpact.pos;
        state->torpedoImpactTimer = 0.18f;
        PushGameEvent(&state->gameEvents, (GameEvent){
            .type = GAME_EVENT_TORPEDO_IMPACT, .pos = torpedoImpact.pos,
            .target.torpedoImpact = torpedoImpact.type
        });
    }
    // Scavenged mortar: same manual gating as the torpedo (one shell in
    // flight + a cooldown), but the shot ignores every blocker - the shell
    // arcs over land and the boss's armor alike, which is its whole point.
    // The landing blast splashes surface targets and boss parts - kept
    // by playtest (harder to aim than the torpedo earns the water-class
    // overlap); Stage 2's land targets will additionally be mortar-only.
    if (state->mortarCooldown > 0.0f) state->mortarCooldown -= dt;
    bool fireMortar = state->hasMortar && !bossOwnsPlayer
        && (InputActionPressed(INPUT_FIRE_MORTAR) || forceMortarFire);
    if (!state->playerDestroyed && fireMortar && !state->mortarShell.active && state->mortarCooldown <= 0.0f) {
        // The shell leaves from the salvaged dome on the spine - the
        // salvage docks it at the player center (see the boss render
        // path) - while the reticle stays nose-anchored like the
        // torpedo's, so the corrected launch point doesn't shorten the
        // weapon's playtest-tuned reach.
        Vector2 mortarSpawn = { state->player.x, state->player.y };
        Vector2 mortarNose = { state->player.x + halfW, state->player.y };
        FirePlayerMortar(&state->mortarShell, mortarSpawn, CalculateMortarReticle(mortarNose));
        state->mortarCooldown = PLAYER_MORTAR_COOLDOWN;
        PushGameEvent(&state->gameEvents, (GameEvent){
            .type = GAME_EVENT_MORTAR_FIRED, .pos = mortarSpawn
        });
    }
    if (UpdatePlayerMortarShell(&state->mortarShell, dt, scrollDt)) {
        Vector2 blast = state->mortarShell.target;
        PushGameEvent(&state->gameEvents, (GameEvent){
            .type = GAME_EVENT_MORTAR_BLAST, .pos = blast
        });
        ResolveMortarBlastSurfaceTargets(blast, state->surfaceTargets, MAX_SURFACE_TARGETS, &state->gameEvents);
        ResolveMortarBlastLandTargets(blast, state->landTargets, MAX_LAND_TARGETS, &state->gameEvents);
        ResolveBossMortarSplashDamage(&state->boss, blast, &state->gameEvents);
    }
    // Player hits resolve before the destruction-effects pass so a mine
    // detonated by contact still gets its blast and SFX this same frame.
    // Enemy bullets are absorbed even while invulnerable; contact is only
    // resolved when it can kill, so a blinking (respawning) player passes
    // over mines without setting them off.
    bool enemyBulletHit = ResolveEnemyBulletPlayerCollision(
        state->enemyBullets, MAX_ENEMY_BULLETS, state->player, playerRadius
    );
    // SAMs are absorbed on contact like enemy bullets, kill only when
    // the player is vulnerable.
    bool missileHit = ResolveBossMissilePlayerCollision(&state->boss, state->player, playerRadius);
    // Once the core dies the run is won: nothing can kill the player
    // through the death chain and salvage beat.
    bool bossVictory = state->boss.phase >= BOSS_PHASE_DYING;
    bool playerVulnerable = !state->playerDestroyed && state->respawnInvulnerability <= 0.0f && !bossVictory;
    // The fuse starts before hull contact. The resulting blast is a real
    // area hazard, so a quick turn can still escape after the warning gap.
    if (playerVulnerable) {
        DetonateNearbyMines(state->surfaceTargets, MAX_SURFACE_TARGETS,
            state->player, playerRadius, &state->gameEvents);
    }
    SpawnMineBlastsFromEvents(state->mineBlasts, MAX_MINE_BLASTS, &state->gameEvents);
    bool mineBlastHit = playerVulnerable && ResolveMineBlastPlayerHit(
        state->mineBlasts, MAX_MINE_BLASTS, state->player, playerRadius
    );
    bool contactHit = playerVulnerable && ResolvePlayerContactDamage(
        state->player, playerRadius, state->airTargets, MAX_AIR_TARGETS,
        state->surfaceTargets, MAX_SURFACE_TARGETS, &state->gameEvents
    );
    // Boss hazards: ramming the live hull and standing in a mortar blast
    // both cost the ship like any other hit.
    bool bossHit = playerVulnerable && (
        ResolveBossContactDamage(&state->boss, state->player, playerRadius)
        || ResolveMortarBlastPlayerHit(&state->boss, state->player, playerRadius)
    );
    // Land-battery blasts kill like the boss's shells; the installations
    // themselves never contact-kill (flying over land is always safe).
    bool landBlastHit = playerVulnerable
        && ResolveLandMortarBlastPlayerHit(state->landTargets, MAX_LAND_TARGETS, state->player, playerRadius);
    if (playerVulnerable && (enemyBulletHit || missileHit || mineBlastHit || contactHit || bossHit || landBlastHit)) {
        TrySpawnExplosion(state->explosions, state->player, EXPLOSION_PLAYER, 20.0f, PLAYER_DEATH_DURATION);
        BeginPlayerDeath(state);
    }

    SpawnTargetDestructionEffects(&state->gameEvents, state->explosions, state->wrecks);
    state->score += ScoreGameEvents(&state->gameEvents);

    UpdateExplosionEffects(state->explosions, dt);
    UpdateSurfaceWrecks(state->wrecks, scrollDt);
}
