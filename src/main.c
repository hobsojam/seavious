#include "gameplay.h"
#include "raylib.h"
#include "rlgl.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

// On hybrid-graphics laptops (integrated + discrete GPU), the OS/driver
// otherwise defaults to the integrated GPU. These exported symbols are a
// standard convention the NVIDIA and AMD drivers scan for at load time to
// route this specific executable to the discrete GPU instead.
#ifdef _WIN32
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
#endif

#define MAX_EXPLOSION_EFFECTS 32
#define MAX_SURFACE_WRECKS 16
#define PLAYER_DEATH_DURATION 0.60f

typedef enum {
    EXPLOSION_AIR_TARGET,
    EXPLOSION_SURFACE_TARGET,
    EXPLOSION_PLAYER
} ExplosionType;

typedef struct {
    Vector2 pos;
    float age;
    float lifetime;
    float radius;
    ExplosionType type;
    bool active;
} ExplosionEffect;

typedef struct {
    Vector2 pos;
    float radius;
    SurfaceTargetType type;
    bool active;
} SurfaceWreck;

static void DrawHudShipSlot(Vector2 center, Color color) {
    DrawTriangleLines(
        (Vector2){ center.x + 6.0f, center.y },
        (Vector2){ center.x - 5.0f, center.y - 4.0f },
        (Vector2){ center.x - 5.0f, center.y + 4.0f },
        color
    );
    DrawLine((int)(center.x - 3.0f), (int)center.y, (int)(center.x + 3.0f), (int)center.y, color);
}

static void ResetPlayerProgress(Vector2 *player, int *score, int *lives) {
    *player = (Vector2){ PLAYER_START_X, PLAYER_START_Y };
    *score = 0;
    *lives = 3;
}

static void ResetTransientState(float *fireTimer, float *airTargetSpawnTimer, float *surfaceTargetSpawnTimer,
    float *wakeEmitTimer, float *torpedoCooldown, Torpedo *torpedo, TorpedoImpactType *torpedoImpactType,
    Vector2 *torpedoImpactPos, float *torpedoImpactTimer, float *oceanScroll, float *oceanOverlayScroll,
    float *respawnInvulnerability, bool *playerDestroyed, float *playerDeathTimer) {
    *fireTimer = 0.0f;
    *airTargetSpawnTimer = 0.0f;
    *surfaceTargetSpawnTimer = 0.0f;
    *wakeEmitTimer = 0.0f;
    *torpedoCooldown = 0.0f;
    *torpedo = (Torpedo){ 0 };
    *torpedoImpactType = TORPEDO_IMPACT_NONE;
    *torpedoImpactPos = (Vector2){ 0.0f, 0.0f };
    *torpedoImpactTimer = 0.0f;
    *oceanScroll = 0.0f;
    *oceanOverlayScroll = 0.0f;
    *respawnInvulnerability = 0.0f;
    *playerDestroyed = false;
    *playerDeathTimer = 0.0f;
}

static void ResetEntityPools(Bullet bullets[], AirTarget airTargets[], SurfaceTarget surfaceTargets[],
    EnemyBullet enemyBullets[], WakeParticle wake[], ExplosionEffect explosions[], SurfaceWreck wrecks[],
    GameEventQueue *gameEvents) {
    memset(bullets, 0, sizeof(Bullet) * MAX_BULLETS);
    memset(airTargets, 0, sizeof(AirTarget) * MAX_AIR_TARGETS);
    memset(surfaceTargets, 0, sizeof(SurfaceTarget) * MAX_SURFACE_TARGETS);
    memset(enemyBullets, 0, sizeof(EnemyBullet) * MAX_ENEMY_BULLETS);
    memset(wake, 0, sizeof(WakeParticle) * MAX_WAKE_PARTICLES);
    memset(explosions, 0, sizeof(ExplosionEffect) * MAX_EXPLOSION_EFFECTS);
    memset(wrecks, 0, sizeof(SurfaceWreck) * MAX_SURFACE_WRECKS);
    gameEvents->count = 0;
}

static void BeginPlayerDeath(int *lives, bool *playerDestroyed, float *playerDeathTimer, Torpedo *torpedo,
    float *torpedoCooldown, TorpedoImpactType *torpedoImpactType, float *torpedoImpactTimer) {
    (*lives)--;
    if (*lives < 0) *lives = 0;
    *playerDestroyed = true;
    *playerDeathTimer = PLAYER_DEATH_DURATION;
    torpedo->active = false;
    *torpedoCooldown = 0.0f;
    *torpedoImpactType = TORPEDO_IMPACT_NONE;
    *torpedoImpactTimer = 0.0f;
}

static void ResetRunState(Vector2 *player, int *score, int *lives, bool *gameOver,
    float *fireTimer, float *airTargetSpawnTimer, float *surfaceTargetSpawnTimer, float *wakeEmitTimer,
    float *torpedoCooldown, Torpedo *torpedo, TorpedoImpactType *torpedoImpactType,
    Vector2 *torpedoImpactPos, float *torpedoImpactTimer, float *oceanScroll, float *oceanOverlayScroll,
    float *respawnInvulnerability, Bullet bullets[], AirTarget airTargets[], SurfaceTarget surfaceTargets[],
    EnemyBullet enemyBullets[], WakeParticle wake[], ExplosionEffect explosions[], SurfaceWreck wrecks[],
    bool *playerDestroyed, float *playerDeathTimer, GameEventQueue *gameEvents) {
    ResetPlayerProgress(player, score, lives);
    *gameOver = false;
    ResetTransientState(
        fireTimer, airTargetSpawnTimer, surfaceTargetSpawnTimer, wakeEmitTimer, torpedoCooldown, torpedo,
        torpedoImpactType, torpedoImpactPos, torpedoImpactTimer, oceanScroll, oceanOverlayScroll,
        respawnInvulnerability, playerDestroyed, playerDeathTimer
    );
    ResetEntityPools(bullets, airTargets, surfaceTargets, enemyBullets, wake, explosions, wrecks, gameEvents);
}

static bool TrySpawnExplosion(ExplosionEffect effects[], Vector2 pos, ExplosionType type, float radius, float lifetime) {
    for (int i = 0; i < MAX_EXPLOSION_EFFECTS; i++) {
        if (effects[i].active) continue;
        effects[i] = (ExplosionEffect){ pos, 0.0f, lifetime, radius, type, true };
        return true;
    }
    return false;
}

static bool TrySpawnSurfaceWreck(SurfaceWreck wrecks[], Vector2 pos, SurfaceTargetType type, float radius) {
    for (int i = 0; i < MAX_SURFACE_WRECKS; i++) {
        if (wrecks[i].active) continue;
        wrecks[i] = (SurfaceWreck){ pos, radius, type, true };
        return true;
    }
    return false;
}

static void SpawnTargetDestructionEffects(const GameEventQueue *events, ExplosionEffect explosions[], SurfaceWreck wrecks[]) {
    for (int i = 0; i < events->count; i++) {
        const GameEvent *event = &events->items[i];
        if (event->type == GAME_EVENT_AIR_TARGET_DESTROYED) {
            TrySpawnExplosion(explosions, event->pos, EXPLOSION_AIR_TARGET, 10.0f, 0.28f);
        } else if (event->type == GAME_EVENT_SURFACE_TARGET_DESTROYED) {
            float radius = event->target.surfaceTarget == SURFACE_TARGET_CASEMATE
                ? CASEMATE_RADIUS : TRACKING_TURRET_RADIUS;
            TrySpawnExplosion(explosions, event->pos, EXPLOSION_SURFACE_TARGET, radius + 5.0f, 0.38f);
            TrySpawnSurfaceWreck(wrecks, event->pos, event->target.surfaceTarget, radius);
        }
    }
}

static void UpdateExplosionEffects(ExplosionEffect effects[], float dt) {
    for (int i = 0; i < MAX_EXPLOSION_EFFECTS; i++) {
        if (!effects[i].active) continue;
        effects[i].age += dt;
        if (effects[i].age >= effects[i].lifetime) effects[i].active = false;
    }
}

static void UpdateSurfaceWrecks(SurfaceWreck wrecks[], float dt) {
    for (int i = 0; i < MAX_SURFACE_WRECKS; i++) {
        if (!wrecks[i].active) continue;
        wrecks[i].pos.x -= OCEAN_SCROLL_SPEED * dt;
        if (wrecks[i].pos.x < -wrecks[i].radius) wrecks[i].active = false;
    }
}

static void DrawHud(int score, int lives, float torpedoCooldown, bool torpedoActive) {
    const Color panel = (Color){ 10, 14, 18, 255 };
    const Color panelInset = (Color){ 16, 24, 30, 255 };
    const Color cyan = (Color){ 76, 224, 232, 255 };
    const Color pale = (Color){ 232, 248, 248, 255 };
    const Color amber = (Color){ 232, 148, 44, 255 };
    const Color inactive = (Color){ 74, 94, 102, 255 };
    const int top = PLAY_HEIGHT;

    DrawRectangle(0, top, GAME_WIDTH, HUD_HEIGHT, panel);
    DrawLine(0, top, GAME_WIDTH, top, cyan);
    DrawLine(64, top + 5, 64, GAME_HEIGHT - 5, (Color){ 76, 224, 232, 80 });
    DrawLine(236, top + 5, 236, GAME_HEIGHT - 5, (Color){ 76, 224, 232, 80 });
    DrawLine(380, top + 5, 380, GAME_HEIGHT - 5, (Color){ 76, 224, 232, 80 });

    // The active ship consumes one life; the HUD shows the remaining reserves.
    DrawText("LIVES", 6, top + 4, 8, inactive);
    int reserveLives = lives > 0 ? lives - 1 : 0;
    for (int i = 0; i < reserveLives; i++) {
        DrawHudShipSlot((Vector2){ 13.0f + 17.0f * i, (float)top + 21.0f }, cyan);
    }

    DrawText("SCORE", 72, top + 4, 8, cyan);
    DrawText(TextFormat("%08d", score), 72, top + 12, 18, pale);

    DrawText("TORPEDO", 244, top + 4, 8, cyan);
    Color torpedoStatusColor = amber;
    const char *torpedoStatus = "READY";
    float reloadProgress = 1.0f;
    if (torpedoActive) {
        torpedoStatus = "IN FLIGHT";
        torpedoStatusColor = pale;
        reloadProgress = 0.0f;
    } else if (torpedoCooldown > 0.0f) {
        torpedoStatus = "RELOAD";
        torpedoStatusColor = inactive;
        reloadProgress = 1.0f - torpedoCooldown / TORPEDO_COOLDOWN;
        if (reloadProgress < 0.0f) reloadProgress = 0.0f;
        if (reloadProgress > 1.0f) reloadProgress = 1.0f;
    }

    DrawRectangle(244, top + 14, 14, 6, torpedoStatusColor);
    DrawTriangle(
        (Vector2){ 263.0f, (float)top + 17.0f },
        (Vector2){ 258.0f, (float)top + 14.0f },
        (Vector2){ 258.0f, (float)top + 20.0f },
        torpedoStatusColor
    );
    DrawText(torpedoStatus, 270, top + 12, 10, torpedoStatusColor);
    DrawRectangle(244, top + 25, 126, 3, panelInset);
    DrawRectangle(244, top + 25, (int)(126.0f * reloadProgress), 3, torpedoStatusColor);

    // Reserved boss region: it becomes a labeled health meter during fights.
    DrawRectangle(389, top + 12, 115, 8, panelInset);
    DrawRectangleLines(389, top + 12, 115, 8, (Color){ 74, 94, 102, 90 });
}

int main(void) {
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(GAME_WIDTH * WINDOW_SCALE, GAME_HEIGHT * WINDOW_SCALE, "Seavious");
    SetTargetFPS(60);

    RenderTexture2D target = LoadRenderTexture(GAME_WIDTH, GAME_HEIGHT);
    SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);

    Texture2D playerTex = LoadTexture("assets/sprites/player_ship.png");
    SetTextureFilter(playerTex, TEXTURE_FILTER_POINT);

    Texture2D droneTex = LoadTexture("assets/sprites/skimmer_drone.png");
    SetTextureFilter(droneTex, TEXTURE_FILTER_POINT);

    Texture2D oceanTex = LoadTexture("assets/tiles/ocean.png");
    SetTextureFilter(oceanTex, TEXTURE_FILTER_POINT);
    SetTextureWrap(oceanTex, TEXTURE_WRAP_REPEAT);

    // Foam-glint parallax layer: scrolls slightly faster than the base
    // water and has a different tile size, so neither layer's repeat ever
    // sits still relative to the other (baked glints in the base tile made
    // the repeat trackable).
    Texture2D oceanOverlayTex = LoadTexture("assets/tiles/ocean_overlay.png");
    SetTextureFilter(oceanOverlayTex, TEXTURE_FILTER_POINT);
    SetTextureWrap(oceanOverlayTex, TEXTURE_WRAP_REPEAT);

    Vector2 player = { PLAYER_START_X, PLAYER_START_Y };
    const float playerSpeed = 120.0f;
    float oceanScroll = 0.0f;
    float oceanOverlayScroll = 0.0f;
    int score = 0;
    int lives = 3;
    bool gameOver = false;
    float respawnInvulnerability = 0.0f;
    bool playerDestroyed = false;
    float playerDeathTimer = 0.0f;
    GameEventQueue gameEvents = { 0 };

    Bullet bullets[MAX_BULLETS] = { 0 };
    float fireTimer = 0.0f;
    const Color bulletColor = (Color){ 76, 224, 232, 255 };

    EnemyBullet enemyBullets[MAX_ENEMY_BULLETS] = { 0 };
    const Color enemyBulletColor = (Color){ 232, 72, 72, 255 };
    ExplosionEffect explosions[MAX_EXPLOSION_EFFECTS] = { 0 };
    SurfaceWreck wrecks[MAX_SURFACE_WRECKS] = { 0 };

    // Skimmer Drone: weakest, most common air filler (see README roster).
    // Sine-wave flight around a fixed baseline, dies in one hit for now —
    // the "1-2 hits" in the design doc is a tuning pass for later.
    AirTarget airTargets[MAX_AIR_TARGETS] = { 0 };
    float airTargetSpawnTimer = 0.0f;
    const Color airTargetColor = (Color){ 216, 72, 192, 255 };

    // Second fire input, single-shot-at-a-time with a reload cooldown (unlike
    // the gun's unlimited auto-fire). The default torpedo runs straight along
    // the surface lane to the reticle max range; lead targeting is reserved for
    // a future upgrade path in the gameplay layer.
    Torpedo torpedo = { 0 };
    float torpedoCooldown = 0.0f;
    const Color torpedoColor = (Color){ 232, 248, 248, 255 };
    TorpedoImpactType torpedoImpactType = TORPEDO_IMPACT_NONE;
    Vector2 torpedoImpactPos = { 0.0f, 0.0f };
    float torpedoImpactTimer = 0.0f;

    // Casemates and tracking turrets are anchored to the water, so they drift
    // left at ocean-scroll speed. Both are torpedo-only (Xevious rule).
    SurfaceTarget surfaceTargets[MAX_SURFACE_TARGETS] = { 0 };
    float surfaceTargetSpawnTimer = 0.0f;
    const Color surfaceTargetColor = (Color){ 232, 148, 44, 255 };

    // Wake/spray puff left on the water by the player's rear ski-points.
    // Once emitted it belongs to the water, not the ship.
    WakeParticle wake[MAX_WAKE_PARTICLES] = { 0 };
    float wakeEmitTimer = 0.0f;

    // Set by the headless CI smoke test so the real game loop exits cleanly
    // after a deterministic number of frames and writes its gcov data.
    const char *smokeFramesValue = getenv("SEAVIOUS_SMOKE_FRAMES");
    int smokeFrames = smokeFramesValue == NULL ? 0 : atoi(smokeFramesValue);
    int framesRun = 0;

    while (!WindowShouldClose()) {
        gameEvents.count = 0;
        float dt = GetFrameTime();
        float halfW = playerTex.width / 2.0f;
        float halfH = playerTex.height / 2.0f;
        float playerRadius = PLAYER_HIT_RADIUS;
        Vector2 torpedoSpawn = { player.x + halfW, player.y };
        Vector2 torpedoReticle = CalculateTorpedoReticle(torpedoSpawn);

        if (gameOver && (IsKeyPressed(KEY_R) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))) {
            ResetRunState(
                &player, &score, &lives, &gameOver, &fireTimer, &airTargetSpawnTimer,
                &surfaceTargetSpawnTimer, &wakeEmitTimer, &torpedoCooldown, &torpedo,
                &torpedoImpactType, &torpedoImpactPos, &torpedoImpactTimer, &oceanScroll,
                &oceanOverlayScroll, &respawnInvulnerability, bullets, airTargets, surfaceTargets, enemyBullets,
                wake, explosions, wrecks, &playerDestroyed, &playerDeathTimer, &gameEvents
            );
        }

        if (!gameOver) {
            Vector2 playerVelocity = { 0.0f, 0.0f };
            if (playerDestroyed) {
                playerDeathTimer -= dt;
                if (playerDeathTimer <= 0.0f) {
                    playerDestroyed = false;
                    if (lives > 0) {
                        player = (Vector2){ PLAYER_START_X, PLAYER_START_Y };
                        respawnInvulnerability = PLAYER_RESPAWN_INVULNERABILITY;
                    } else {
                        gameOver = true;
                    }
                }
            } else {
                if (respawnInvulnerability > 0.0f) respawnInvulnerability -= dt;

                float inputX = 0.0f;
                float inputY = 0.0f;
                if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A))  inputX -= 1.0f;
                if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) inputX += 1.0f;
                if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W))    inputY -= 1.0f;
                if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S))  inputY += 1.0f;
                Vector2 playerBeforeMove = player;
                MovePlayer(&player, inputX, inputY, playerSpeed, dt, halfW, halfH);
                if (dt > 0.0f) {
                    playerVelocity = (Vector2){
                        (player.x - playerBeforeMove.x) / dt, (player.y - playerBeforeMove.y) / dt
                    };
                }

                // Wake: drop a puff from each rear ski-point on a fixed cadence.
                // The 1px jitter keeps the two trails from reading as ruled lines.
                wakeEmitTimer += dt;
                while (wakeEmitTimer >= WAKE_EMIT_INTERVAL) {
                    wakeEmitTimer -= WAKE_EMIT_INTERVAL;
                    for (int ski = -1; ski <= 1; ski += 2) {
                        TryEmitWakeParticle(wake, MAX_WAKE_PARTICLES, (Vector2){
                            player.x - halfW + (float)GetRandomValue(-1, 1),
                            player.y + ski * WAKE_SKI_OFFSET_Y + (float)GetRandomValue(-1, 1)
                        });
                    }
                }

                // Auto-fire: accumulate dt and emit as many shots as the interval
                // allows, so a stalled frame can't silently eat a shot.
                fireTimer += dt;
                while (fireTimer >= BULLET_FIRE_INTERVAL) {
                    fireTimer -= BULLET_FIRE_INTERVAL;
                    TrySpawnBullet(bullets, MAX_BULLETS, (Vector2){ player.x + halfW, player.y });
                }
            }
            torpedoSpawn = (Vector2){ player.x + halfW, player.y };
            torpedoReticle = CalculateTorpedoReticle(torpedoSpawn);

            // World scrolls right-to-left under the player. Kept bounded by
            // the tile width since REPEAT wrap only needs a modulo tile offset.
            oceanScroll = AdvanceOceanScroll(oceanScroll, dt, (float)oceanTex.width);
            oceanOverlayScroll = AdvanceOceanScroll(
                oceanOverlayScroll, dt * OCEAN_OVERLAY_SPEED_SCALE, (float)oceanOverlayTex.width
            );

            UpdateWakeParticles(wake, MAX_WAKE_PARTICLES, dt);
            if (torpedoImpactTimer > 0.0f) torpedoImpactTimer -= dt;
            UpdateBullets(bullets, MAX_BULLETS, dt);

            // Enemies spawn off the right edge and fly left to meet the player,
            // opposite the bullets, since the world scrolls the other way under them.
            airTargetSpawnTimer += dt;
            while (airTargetSpawnTimer >= SKIMMER_DRONE_SPAWN_INTERVAL) {
                airTargetSpawnTimer -= SKIMMER_DRONE_SPAWN_INTERVAL;
                float margin = SKIMMER_DRONE_RADIUS + SKIMMER_DRONE_SINE_AMPLITUDE;
                float baseY = (float)GetRandomValue((int)margin, (int)(PLAY_HEIGHT - margin));
                TrySpawnSkimmerDrone(airTargets, MAX_AIR_TARGETS, baseY);
            }
            UpdateAirTargets(airTargets, MAX_AIR_TARGETS, dt);

            // Gun-vs-air collision: brute-force O(bullets*targets) is fine at
            // these pool sizes (32*16 max).
            ResolveBulletAirTargetCollisions(
                bullets, MAX_BULLETS, airTargets, MAX_AIR_TARGETS, &gameEvents
            );

            // Torpedo: manual second input, gated by both "one in flight at a
            // time" and a reload cooldown so it isn't unlimited-fire like the gun.
            if (torpedoCooldown > 0.0f) torpedoCooldown -= dt;
            if (!playerDestroyed && IsKeyPressed(KEY_SPACE) && !torpedo.active && torpedoCooldown <= 0.0f) {
                FireFixedRangeTorpedo(&torpedo, torpedoSpawn);
                torpedoCooldown = TORPEDO_COOLDOWN;
            }
            TorpedoImpact torpedoImpact = UpdateTorpedo(&torpedo, dt);

            // Surface threats spawn off the right edge and drift left with the water.
            surfaceTargetSpawnTimer += dt;
            while (surfaceTargetSpawnTimer >= SURFACE_TARGET_SPAWN_INTERVAL) {
                surfaceTargetSpawnTimer -= SURFACE_TARGET_SPAWN_INTERVAL;
                float y = (float)GetRandomValue(
                    (int)TRACKING_TURRET_RADIUS, (int)(PLAY_HEIGHT - TRACKING_TURRET_RADIUS)
                );
                if (GetRandomValue(0, 1) == 0) {
                    TrySpawnCasemate(surfaceTargets, MAX_SURFACE_TARGETS, y);
                } else {
                    TrySpawnTrackingTurret(surfaceTargets, MAX_SURFACE_TARGETS, y);
                }
            }
            UpdateSurfaceTargets(surfaceTargets, MAX_SURFACE_TARGETS, dt);
            UpdateSurfaceTargetFire(
                surfaceTargets, MAX_SURFACE_TARGETS, dt, player, playerVelocity, enemyBullets, MAX_ENEMY_BULLETS
            );
            UpdateEnemyBullets(enemyBullets, MAX_ENEMY_BULLETS, dt);

            // Torpedo-vs-surface collision — the gun deliberately has no case
            // here: bullets pass over ground targets (dual-targeting rule).
            if (torpedoImpact.type == TORPEDO_IMPACT_EXPLOSION) {
                ResolveTorpedoExplosion(
                    torpedoImpact.pos, surfaceTargets, MAX_SURFACE_TARGETS, &gameEvents
                );
            } else if (torpedoImpact.type == TORPEDO_IMPACT_NONE) {
                torpedoImpact = ResolveTorpedoSurfaceTargetCollision(
                    &torpedo, surfaceTargets, MAX_SURFACE_TARGETS, &gameEvents
                );
            }
            if (torpedoImpact.type != TORPEDO_IMPACT_NONE) {
                torpedoImpactType = torpedoImpact.type;
                torpedoImpactPos = torpedoImpact.pos;
                torpedoImpactTimer = torpedoImpact.type == TORPEDO_IMPACT_EXPLOSION ? 0.18f : 0.10f;
            }
            SpawnTargetDestructionEffects(&gameEvents, explosions, wrecks);
            score += ScoreGameEvents(&gameEvents);

            UpdateExplosionEffects(explosions, dt);
            UpdateSurfaceWrecks(wrecks, dt);

            bool enemyBulletHit = ResolveEnemyBulletPlayerCollision(
                enemyBullets, MAX_ENEMY_BULLETS, player, playerRadius
            );
            bool contactHit = ResolvePlayerContactDamage(
                player, playerRadius, airTargets, MAX_AIR_TARGETS, surfaceTargets, MAX_SURFACE_TARGETS
            );
            if (!playerDestroyed && respawnInvulnerability <= 0.0f && (enemyBulletHit || contactHit)) {
                TrySpawnExplosion(explosions, player, EXPLOSION_PLAYER, 20.0f, PLAYER_DEATH_DURATION);
                BeginPlayerDeath(
                    &lives, &playerDestroyed, &playerDeathTimer, &torpedo, &torpedoCooldown, &torpedoImpactType,
                    &torpedoImpactTimer
                );
            }
        }

        BeginTextureMode(target);
            ClearBackground(BLACK);

            // Single draw call tiles the whole play area: the source rect is
            // larger than the tile texture, so REPEAT wrap fills it by sampling.
            DrawTexturePro(
                oceanTex,
                (Rectangle){ oceanScroll, 0, (float)GAME_WIDTH, (float)PLAY_HEIGHT },
                (Rectangle){ 0, 0, (float)GAME_WIDTH, (float)PLAY_HEIGHT },
                (Vector2){ 0, 0 },
                0.0f,
                WHITE
            );

            // The glint overlay belongs to the water surface: it must stay
            // immediately above the base ocean and below everything that
            // sits on the water (wake, surface targets, any future land
            // tiles), so glints never draw on top of solid objects.
            DrawTexturePro(
                oceanOverlayTex,
                (Rectangle){ oceanOverlayScroll, 0, (float)GAME_WIDTH, (float)PLAY_HEIGHT },
                (Rectangle){ 0, 0, (float)GAME_WIDTH, (float)PLAY_HEIGHT },
                (Vector2){ 0, 0 },
                0.0f,
                WHITE
            );

            // Wake sits directly on the water, under every other layer.
            for (int i = 0; i < MAX_WAKE_PARTICLES; i++) {
                if (!wake[i].active) continue;
                float life = 1.0f - wake[i].age / WAKE_LIFETIME;
                DrawCircleV(
                    wake[i].pos,
                    life > 0.5f ? 2.0f : 1.0f,
                    (Color){ 207, 239, 240, (unsigned char)(150.0f * life) }
                );
            }

            // Wrecks belong to the water layer and are inert: they only mark
            // that a surface installation was destroyed while the world scrolls on.
            for (int i = 0; i < MAX_SURFACE_WRECKS; i++) {
                if (!wrecks[i].active) continue;
                Color scorch = wrecks[i].type == SURFACE_TARGET_CASEMATE
                    ? (Color){ 28, 36, 34, 230 } : (Color){ 34, 29, 25, 230 };
                DrawCircleV(wrecks[i].pos, wrecks[i].radius + 2.0f, scorch);
                DrawCircleLines((int)wrecks[i].pos.x, (int)wrecks[i].pos.y,
                    wrecks[i].radius + 2.0f, (Color){ 12, 18, 20, 220 });
                DrawCircleV(
                    (Vector2){ wrecks[i].pos.x - 2.0f, wrecks[i].pos.y + 1.0f },
                    wrecks[i].radius * 0.45f,
                    (Color){ 10, 15, 17, 210 }
                );
            }

            // Reticle marks maximum torpedo range, not a target lock.
            DrawLine((int)torpedoSpawn.x, (int)torpedoSpawn.y, (int)torpedoReticle.x, (int)torpedoReticle.y,
                (Color){ 76, 224, 232, 55 });
            DrawCircleLines((int)torpedoReticle.x, (int)torpedoReticle.y, 6.0f, (Color){ 232, 148, 44, 190 });
            DrawLine((int)(torpedoReticle.x - 10.0f), (int)torpedoReticle.y, (int)(torpedoReticle.x - 4.0f), (int)torpedoReticle.y,
                (Color){ 232, 148, 44, 190 });
            DrawLine((int)(torpedoReticle.x + 4.0f), (int)torpedoReticle.y, (int)(torpedoReticle.x + 10.0f), (int)torpedoReticle.y,
                (Color){ 232, 148, 44, 190 });
            DrawLine((int)torpedoReticle.x, (int)(torpedoReticle.y - 10.0f), (int)torpedoReticle.x, (int)(torpedoReticle.y - 4.0f),
                (Color){ 232, 148, 44, 190 });
            DrawLine((int)torpedoReticle.x, (int)(torpedoReticle.y + 4.0f), (int)torpedoReticle.x, (int)(torpedoReticle.y + 10.0f),
                (Color){ 232, 148, 44, 190 });

            if (torpedoImpactTimer > 0.0f) {
                float life = torpedoImpactTimer / (torpedoImpactType == TORPEDO_IMPACT_EXPLOSION ? 0.18f : 0.10f);
                if (torpedoImpactType == TORPEDO_IMPACT_EXPLOSION) {
                    DrawCircleLines((int)torpedoImpactPos.x, (int)torpedoImpactPos.y,
                        TORPEDO_SPLASH_RADIUS * (1.0f + 0.25f * (1.0f - life)),
                        (Color){ 255, 220, 140, (unsigned char)(220.0f * life) });
                    DrawCircleV(torpedoImpactPos, 5.0f, (Color){ 255, 246, 216, (unsigned char)(180.0f * life) });
                } else {
                    DrawCircleV(torpedoImpactPos, 3.0f, (Color){ 207, 239, 240, (unsigned char)(160.0f * life) });
                }
            }

            // Surface layer: ground targets draw under everything airborne.
            for (int i = 0; i < MAX_SURFACE_TARGETS; i++) {
                if (!surfaceTargets[i].active) continue;
                DrawCircleLines((int)surfaceTargets[i].pos.x, (int)surfaceTargets[i].pos.y,
                    surfaceTargets[i].radius + 2.0f, (Color){ 232, 148, 44, 110 });
                if (surfaceTargets[i].type == SURFACE_TARGET_CASEMATE) {
                    DrawPoly(surfaceTargets[i].pos, 6, surfaceTargets[i].radius, 0.0f, surfaceTargetColor);
                } else {
                    DrawCircleV(surfaceTargets[i].pos, surfaceTargets[i].radius, (Color){ 126, 78, 34, 255 });
                    DrawCircleLines((int)surfaceTargets[i].pos.x, (int)surfaceTargets[i].pos.y,
                        surfaceTargets[i].radius - 3.0f, surfaceTargetColor);
                }
                Vector2 barrelEnd = {
                    surfaceTargets[i].pos.x + surfaceTargets[i].aimDirection.x * (surfaceTargets[i].radius + 4.0f),
                    surfaceTargets[i].pos.y + surfaceTargets[i].aimDirection.y * (surfaceTargets[i].radius + 4.0f)
                };
                DrawLineEx(surfaceTargets[i].pos, barrelEnd, 3.0f, (Color){ 168, 100, 24, 255 });
                DrawCircleV(surfaceTargets[i].pos, 3.0f, (Color){ 255, 200, 120, 255 });
            }

            // Ship points right (direction of travel / forward fire). It is
            // hidden during its explosion; respawn follows once the effect ends.
            if (!playerDestroyed) {
                DrawTexture(
                    playerTex,
                    (int)(player.x - playerTex.width / 2.0f),
                    (int)(player.y - playerTex.height / 2.0f),
                    WHITE
                );
            }

            for (int i = 0; i < MAX_BULLETS; i++) {
                if (!bullets[i].active) continue;
                DrawCircleV(bullets[i].pos, BULLET_RADIUS, bulletColor);
            }

            for (int i = 0; i < MAX_ENEMY_BULLETS; i++) {
                if (!enemyBullets[i].active) continue;
                DrawCircleV(enemyBullets[i].pos, ENEMY_BULLET_RADIUS, enemyBulletColor);
            }

            // Skimmer Drone: dark hull so the magenta glow carries the color
            // read. The core pulse is code-driven (no extra frames), phased by
            // each drone's flight time so swarms shimmer instead of strobing
            // in unison. Core sits 4px ahead of sprite center (see generator).
            for (int i = 0; i < MAX_AIR_TARGETS; i++) {
                if (!airTargets[i].active) continue;
                DrawTexture(
                    droneTex,
                    (int)(airTargets[i].pos.x - droneTex.width / 2.0f),
                    (int)(airTargets[i].pos.y - droneTex.height / 2.0f),
                    WHITE
                );
                unsigned char corePulse = (unsigned char)(120 + 80.0f * sinf(airTargets[i].t * 6.0f));
                DrawCircleV(
                    (Vector2){ airTargets[i].pos.x - 4.0f, airTargets[i].pos.y },
                    2.5f,
                    (Color){ airTargetColor.r, airTargetColor.g, airTargetColor.b, corePulse }
                );
            }

            for (int i = 0; i < MAX_EXPLOSION_EFFECTS; i++) {
                if (!explosions[i].active) continue;
                float progress = explosions[i].age / explosions[i].lifetime;
                float radius = explosions[i].radius * (0.45f + 0.75f * progress);
                unsigned char alpha = (unsigned char)(255.0f * (1.0f - progress));
                Color edge = explosions[i].type == EXPLOSION_AIR_TARGET
                    ? (Color){ 255, 100, 216, alpha }
                    : (Color){ 255, 180, 72, alpha };
                if (explosions[i].type == EXPLOSION_PLAYER) edge = (Color){ 96, 232, 255, alpha };
                DrawCircleLines((int)explosions[i].pos.x, (int)explosions[i].pos.y, radius, edge);
                DrawCircleV(explosions[i].pos, radius * 0.35f, (Color){ 255, 242, 196, alpha });
            }

            // Torpedo reads as player tech: hull-white body with pointed nose,
            // cyan spine stripe + engine glow, and a fading surface wake behind it.
            if (torpedo.active) {
                float hw = TORPEDO_WIDTH / 2.0f;
                float hh = TORPEDO_HEIGHT / 2.0f;
                const Color cyan = (Color){ 76, 224, 232, 255 };

                rlPushMatrix();
                rlTranslatef(torpedo.pos.x, torpedo.pos.y, 0.0f);
                rlRotatef(atan2f(torpedo.vel.y, torpedo.vel.x) * RAD2DEG, 0.0f, 0.0f, 1.0f);

                for (int i = 0; i < 3; i++) {
                    DrawRectangle(
                        (int)(-hw - 5.0f - 7.0f * i),
                        -1,
                        (int)(6.0f - 1.5f * i),
                        2,
                        (Color){ 207, 239, 240, (unsigned char)(110 - 32 * i) }
                    );
                }

                unsigned char pulse = (unsigned char)(190 + 60.0f * sinf((float)GetTime() * 20.0f));
                DrawCircle((int)(-hw - 1.0f), 0, 2.0f, (Color){ 76, 224, 232, pulse });
                DrawRectangle((int)-hw, (int)(-hh - 1.0f), 2, (int)TORPEDO_HEIGHT + 2, torpedoColor);
                DrawRectangle((int)-hw, (int)-hh, (int)TORPEDO_WIDTH, (int)TORPEDO_HEIGHT, torpedoColor);
                DrawTriangle(
                    (Vector2){ hw + 4.0f, 0.0f },
                    (Vector2){ hw, -hh },
                    (Vector2){ hw, hh },
                    torpedoColor
                );
                DrawRectangle((int)(-hw + 1.0f), 0, (int)(TORPEDO_WIDTH - 1.0f), 1, cyan);

                rlPopMatrix();
            }

            if (gameOver) {
                DrawRectangle(0, 0, GAME_WIDTH, GAME_HEIGHT, (Color){ 0, 0, 0, 140 });
                DrawText("GAME OVER", 162, 150, 24, (Color){ 232, 248, 248, 255 });
                DrawText("PRESS R TO RESTART", 132, 184, 12, (Color){ 76, 224, 232, 255 });
            } else if (respawnInvulnerability > 0.0f) {
                unsigned char blink = (unsigned char)(120 + 80 * sinf(GetTime() * 22.0f));
                DrawText("RESPAWNING", 180, 150, 12, (Color){ 76, 224, 232, blink });
            }

            DrawHud(score, lives, torpedoCooldown, torpedo.active);
        EndTextureMode();

        BeginDrawing();
            ClearBackground(BLACK);
            DrawTexturePro(
                target.texture,
                (Rectangle){ 0, 0, (float)target.texture.width, -(float)target.texture.height },
                (Rectangle){ 0, 0, (float)GAME_WIDTH * WINDOW_SCALE, (float)GAME_HEIGHT * WINDOW_SCALE },
                (Vector2){ 0, 0 },
                0.0f,
                WHITE
            );
        EndDrawing();

        if (smokeFrames > 0) {
            framesRun++;
            if (framesRun >= smokeFrames) break;
        }
    }

    UnloadTexture(oceanOverlayTex);
    UnloadTexture(oceanTex);
    UnloadTexture(droneTex);
    UnloadTexture(playerTex);
    UnloadRenderTexture(target);
    CloseWindow();
    return 0;
}
