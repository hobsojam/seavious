#include "gameplay.h"
#include "raylib.h"
#include "rlgl.h"
#include <math.h>

// On hybrid-graphics laptops (integrated + discrete GPU), the OS/driver
// otherwise defaults to the integrated GPU. These exported symbols are a
// standard convention the NVIDIA and AMD drivers scan for at load time to
// route this specific executable to the discrete GPU instead.
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;

static void DrawHudShipSlot(Vector2 center, Color color) {
    DrawTriangleLines(
        (Vector2){ center.x + 6.0f, center.y },
        (Vector2){ center.x - 5.0f, center.y - 4.0f },
        (Vector2){ center.x - 5.0f, center.y + 4.0f },
        color
    );
    DrawLine((int)(center.x - 3.0f), (int)center.y, (int)(center.x + 3.0f), (int)center.y, color);
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

    Vector2 player = { 48.0f, PLAY_HEIGHT / 2.0f };
    const float playerSpeed = 120.0f;
    float oceanScroll = 0.0f;
    int score = 0;
    int lives = 3;
    GameEventQueue gameEvents = { 0 };

    Bullet bullets[MAX_BULLETS] = { 0 };
    float fireTimer = 0.0f;
    const Color bulletColor = (Color){ 76, 224, 232, 255 };

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

    // Turret Platform: baseline ground/surface target (see README roster).
    // Stationary on the water, so it drifts left at exactly the ocean scroll
    // speed. Torpedo-only: gun bullets fly over it (Xevious rule).
    SurfaceTarget surfaceTargets[MAX_SURFACE_TARGETS] = { 0 };
    float surfaceTargetSpawnTimer = 0.0f;
    const Color surfaceTargetColor = (Color){ 232, 148, 44, 255 };

    // Wake/spray puff left on the water by the player's rear ski-points.
    // Once emitted it belongs to the water, not the ship.
    WakeParticle wake[MAX_WAKE_PARTICLES] = { 0 };
    float wakeEmitTimer = 0.0f;

    while (!WindowShouldClose()) {
        gameEvents.count = 0;
        float dt = GetFrameTime();
        float halfW = playerTex.width / 2.0f;
        float halfH = playerTex.height / 2.0f;

        float inputX = 0.0f;
        float inputY = 0.0f;
        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A))  inputX -= 1.0f;
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) inputX += 1.0f;
        if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W))    inputY -= 1.0f;
        if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S))  inputY += 1.0f;
        MovePlayer(&player, inputX, inputY, playerSpeed, dt, halfW, halfH);
        Vector2 torpedoSpawn = { player.x + halfW, player.y };
        Vector2 torpedoReticle = CalculateTorpedoReticle(torpedoSpawn);

        // World scrolls right-to-left under the player. Kept bounded by
        // the tile width since REPEAT wrap only needs a modulo tile offset.
        oceanScroll = AdvanceOceanScroll(oceanScroll, dt, (float)oceanTex.width);

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
        UpdateWakeParticles(wake, MAX_WAKE_PARTICLES, dt);
        if (torpedoImpactTimer > 0.0f) torpedoImpactTimer -= dt;

        // Auto-fire: accumulate dt and emit as many shots as the interval
        // allows, so a stalled frame can't silently eat a shot.
        fireTimer += dt;
        while (fireTimer >= BULLET_FIRE_INTERVAL) {
            fireTimer -= BULLET_FIRE_INTERVAL;
            TrySpawnBullet(bullets, MAX_BULLETS, (Vector2){ player.x + halfW, player.y });
        }
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
        if (IsKeyPressed(KEY_SPACE) && !torpedo.active && torpedoCooldown <= 0.0f) {
            FireFixedRangeTorpedo(&torpedo, torpedoSpawn);
            torpedoCooldown = TORPEDO_COOLDOWN;
        }
        TorpedoImpact torpedoImpact = UpdateTorpedo(&torpedo, dt);

        // Turret Platforms spawn off the right edge like air enemies, but
        // being anchored to the water they drift left at the ocean scroll speed.
        surfaceTargetSpawnTimer += dt;
        while (surfaceTargetSpawnTimer >= TURRET_PLATFORM_SPAWN_INTERVAL) {
            surfaceTargetSpawnTimer -= TURRET_PLATFORM_SPAWN_INTERVAL;
            float y = (float)GetRandomValue(
                (int)TURRET_PLATFORM_RADIUS, (int)(PLAY_HEIGHT - TURRET_PLATFORM_RADIUS)
            );
            TrySpawnTurretPlatform(surfaceTargets, MAX_SURFACE_TARGETS, y);
        }
        UpdateSurfaceTargets(surfaceTargets, MAX_SURFACE_TARGETS, dt);

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
        score += ScoreGameEvents(&gameEvents);

        BeginTextureMode(target);
            ClearBackground(BLACK);

            // Single draw call tiles the whole play area: the source rect is
            // larger than the 32x32 texture, so REPEAT wrap fills it by sampling.
            DrawTexturePro(
                oceanTex,
                (Rectangle){ oceanScroll, 0, (float)GAME_WIDTH, (float)PLAY_HEIGHT },
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
            // Placeholder for the Turret Platform per README.
            for (int i = 0; i < MAX_SURFACE_TARGETS; i++) {
                if (!surfaceTargets[i].active) continue;
                DrawCircleLines((int)surfaceTargets[i].pos.x, (int)surfaceTargets[i].pos.y,
                    surfaceTargets[i].radius + 2.0f, (Color){ 232, 148, 44, 110 });
                DrawPoly(surfaceTargets[i].pos, 6, surfaceTargets[i].radius, 0.0f, surfaceTargetColor);
                DrawRectangle(
                    (int)(surfaceTargets[i].pos.x - surfaceTargets[i].radius - 3.0f),
                    (int)(surfaceTargets[i].pos.y - 1.0f),
                    (int)(surfaceTargets[i].radius + 3.0f), 2,
                    (Color){ 168, 100, 24, 255 }
                );
                DrawCircleV(surfaceTargets[i].pos, 3.0f, (Color){ 255, 200, 120, 255 });
            }

            // Ship points right (direction of travel / forward fire).
            DrawTexture(
                playerTex,
                (int)(player.x - playerTex.width / 2.0f),
                (int)(player.y - playerTex.height / 2.0f),
                WHITE
            );

            for (int i = 0; i < MAX_BULLETS; i++) {
                if (!bullets[i].active) continue;
                DrawCircleV(bullets[i].pos, BULLET_RADIUS, bulletColor);
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
    }

    UnloadTexture(oceanTex);
    UnloadTexture(droneTex);
    UnloadTexture(playerTex);
    UnloadRenderTexture(target);
    CloseWindow();
    return 0;
}
