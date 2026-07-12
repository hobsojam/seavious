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

int main(void) {
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(GAME_WIDTH * WINDOW_SCALE, GAME_HEIGHT * WINDOW_SCALE, "Seavious");
    SetTargetFPS(60);

    RenderTexture2D target = LoadRenderTexture(GAME_WIDTH, GAME_HEIGHT);
    SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);

    Texture2D playerTex = LoadTexture("assets/sprites/player_ship.png");
    SetTextureFilter(playerTex, TEXTURE_FILTER_POINT);

    Texture2D oceanTex = LoadTexture("assets/tiles/ocean.png");
    SetTextureFilter(oceanTex, TEXTURE_FILTER_POINT);
    SetTextureWrap(oceanTex, TEXTURE_WRAP_REPEAT);

    Vector2 player = { 48.0f, PLAY_HEIGHT / 2.0f };
    const float playerSpeed = 120.0f;
    float oceanScroll = 0.0f;

    Bullet bullets[MAX_BULLETS] = { 0 };
    float fireTimer = 0.0f;
    const Color bulletColor = (Color){ 76, 224, 232, 255 };

    // Skimmer Drone: weakest, most common air filler (see README roster).
    // Sine-wave flight around a fixed baseline, dies in one hit for now —
    // the "1-2 hits" in the design doc is a tuning pass for later.
    Enemy enemies[MAX_ENEMIES] = { 0 };
    float enemySpawnTimer = 0.0f;
    const Color enemyColor = (Color){ 216, 72, 192, 255 };

    // Second fire input, single-shot-at-a-time with a reload cooldown (unlike
    // the gun's unlimited auto-fire). Fire-and-forget with lead-targeting:
    // the launch solves an intercept against turret drift and locks that
    // heading — it does not home mid-flight, so the skill is in when you fire.
    Torpedo torpedo = { 0 };
    float torpedoCooldown = 0.0f;
    const Color torpedoColor = (Color){ 232, 248, 248, 255 };

    // Turret Platform: baseline ground/surface target (see README roster).
    // Stationary on the water, so it drifts left at exactly the ocean scroll
    // speed. Torpedo-only: gun bullets fly over it (Xevious rule).
    Turret turrets[MAX_TURRETS] = { 0 };
    float turretSpawnTimer = 0.0f;
    const Color turretColor = (Color){ 232, 148, 44, 255 };

    // Wake/spray puff left on the water by the player's rear ski-points.
    // Once emitted it belongs to the water, not the ship.
    WakeParticle wake[MAX_WAKE_PARTICLES] = { 0 };
    float wakeEmitTimer = 0.0f;

    while (!WindowShouldClose()) {
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
        enemySpawnTimer += dt;
        while (enemySpawnTimer >= ENEMY_SPAWN_INTERVAL) {
            enemySpawnTimer -= ENEMY_SPAWN_INTERVAL;
            float margin = ENEMY_RADIUS + ENEMY_SINE_AMPLITUDE;
            float baseY = (float)GetRandomValue((int)margin, (int)(PLAY_HEIGHT - margin));
            TrySpawnEnemy(enemies, MAX_ENEMIES, baseY);
        }
        UpdateEnemies(enemies, MAX_ENEMIES, dt);

        // Gun-vs-air collision: brute-force O(bullets*enemies) is fine at
        // these pool sizes (32*16 max).
        ResolveBulletEnemyCollisions(bullets, MAX_BULLETS, enemies, MAX_ENEMIES);

        // Torpedo: manual second input, gated by both "one in flight at a
        // time" and a reload cooldown so it isn't unlimited-fire like the gun.
        if (torpedoCooldown > 0.0f) torpedoCooldown -= dt;
        if (IsKeyPressed(KEY_SPACE) && !torpedo.active && torpedoCooldown <= 0.0f) {
            FireTorpedo(&torpedo, (Vector2){ player.x + halfW, player.y }, turrets, MAX_TURRETS);
            torpedoCooldown = TORPEDO_COOLDOWN;
        }
        UpdateTorpedo(&torpedo, dt);

        // Turret Platforms spawn off the right edge like air enemies, but
        // being anchored to the water they drift left at the ocean scroll speed.
        turretSpawnTimer += dt;
        while (turretSpawnTimer >= TURRET_SPAWN_INTERVAL) {
            turretSpawnTimer -= TURRET_SPAWN_INTERVAL;
            float y = (float)GetRandomValue((int)TURRET_RADIUS, (int)(PLAY_HEIGHT - TURRET_RADIUS));
            TrySpawnTurret(turrets, MAX_TURRETS, y);
        }
        UpdateTurrets(turrets, MAX_TURRETS, dt);

        // Torpedo-vs-surface collision — the gun deliberately has no case
        // here: bullets pass over ground targets (dual-targeting rule).
        ResolveTorpedoTurretCollision(&torpedo, turrets, MAX_TURRETS);

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

            // Surface layer: ground targets draw under everything airborne.
            // Placeholder for the Turret Platform per README.
            for (int i = 0; i < MAX_TURRETS; i++) {
                if (!turrets[i].active) continue;
                DrawCircleLines((int)turrets[i].pos.x, (int)turrets[i].pos.y,
                    TURRET_RADIUS + 2.0f, (Color){ 232, 148, 44, 110 });
                DrawPoly(turrets[i].pos, 6, TURRET_RADIUS, 0.0f, turretColor);
                DrawRectangle(
                    (int)(turrets[i].pos.x - TURRET_RADIUS - 3.0f),
                    (int)(turrets[i].pos.y - 1.0f),
                    (int)(TURRET_RADIUS + 3.0f), 2,
                    (Color){ 168, 100, 24, 255 }
                );
                DrawCircleV(turrets[i].pos, 3.0f, (Color){ 255, 200, 120, 255 });
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

            // Placeholder diamond silhouette for the Skimmer Drone.
            for (int i = 0; i < MAX_ENEMIES; i++) {
                if (!enemies[i].active) continue;
                DrawPoly(enemies[i].pos, 4, ENEMY_RADIUS, 45.0f, enemyColor);
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

            // HUD bar placeholder — real content is a separate TODO item.
            DrawRectangle(0, PLAY_HEIGHT, GAME_WIDTH, HUD_HEIGHT, (Color){ 10, 14, 18, 255 });
            DrawLine(0, PLAY_HEIGHT, GAME_WIDTH, PLAY_HEIGHT, (Color){ 76, 224, 232, 255 });
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
    UnloadTexture(playerTex);
    UnloadRenderTexture(target);
    CloseWindow();
    return 0;
}
