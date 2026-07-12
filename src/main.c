#include "raylib.h"
#include <math.h>

// On hybrid-graphics laptops (integrated + discrete GPU), the OS/driver
// otherwise defaults to the integrated GPU. These exported symbols are a
// standard convention the NVIDIA and AMD drivers scan for at load time to
// route this specific executable to the discrete GPU instead.
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;

#define GAME_WIDTH    512
#define GAME_HEIGHT   384
#define HUD_HEIGHT    32
#define PLAY_HEIGHT   (GAME_HEIGHT - HUD_HEIGHT)
#define WINDOW_SCALE  2
#define OCEAN_SCROLL_SPEED 40.0f

#define MAX_BULLETS         32
#define BULLET_SPEED        280.0f
#define BULLET_FIRE_INTERVAL 0.15f
#define BULLET_RADIUS       3.0f

#define MAX_ENEMIES          16
#define ENEMY_RADIUS         6.0f
#define ENEMY_SPEED          60.0f
#define ENEMY_SPAWN_INTERVAL 1.2f
#define ENEMY_SINE_AMPLITUDE 24.0f
#define ENEMY_SINE_FREQUENCY 2.0f
#define ENEMY_HP             1

#define TORPEDO_SPEED    200.0f
#define TORPEDO_COOLDOWN 1.5f
#define TORPEDO_WIDTH    12.0f
#define TORPEDO_HEIGHT   4.0f

#define MAX_TURRETS            8
#define TURRET_RADIUS          9.0f
#define TURRET_SPAWN_INTERVAL  3.5f
#define TURRET_HP              1

#define MAX_WAKE_PARTICLES   96
#define WAKE_EMIT_INTERVAL   0.04f
#define WAKE_LIFETIME        1.1f
#define WAKE_SKI_OFFSET_Y    8.0f

typedef struct {
    Vector2 pos;
    bool active;
} Bullet;

// Skimmer Drone: weakest, most common air filler (see README roster).
// Sine-wave flight around a fixed baseline, dies in one hit for now —
// the "1-2 hits" in the design doc is a tuning pass for later.
typedef struct {
    Vector2 pos;
    float baseY;
    float t;
    int hp;
    bool active;
} Enemy;

// Second fire input, single-shot-at-a-time with a reload cooldown (unlike
// the gun's unlimited auto-fire). Travels level along the player's y at
// fire time rather than arcing — lead-targeting toward a ground target is
// a separate TODO now that ground targets exist.
typedef struct {
    Vector2 pos;
    bool active;
} Torpedo;

// Wake/spray puff left on the water by the player's rear ski-points (the
// one design element only the player has — see README). Once emitted it
// belongs to the water, not the ship: it drifts left at the ocean scroll
// speed and fades out, marking the surface the ship just flew over.
typedef struct {
    Vector2 pos;
    float age;
    bool active;
} WakeParticle;

// Turret Platform: baseline ground/surface target (see README roster).
// Stationary on the water, so it drifts left at exactly the ocean scroll
// speed. Torpedo-only: gun bullets fly over it (Xevious rule — shape and
// amber color signal "wrong weapon" before the player even tests it).
// Doesn't fire back yet — that lands with the lives/damage system.
typedef struct {
    Vector2 pos;
    int hp;
    bool active;
} Turret;

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

    Enemy enemies[MAX_ENEMIES] = { 0 };
    float enemySpawnTimer = 0.0f;
    const Color enemyColor = (Color){ 216, 72, 192, 255 };

    Torpedo torpedo = { 0 };
    float torpedoCooldown = 0.0f;
    const Color torpedoColor = (Color){ 232, 248, 248, 255 };

    Turret turrets[MAX_TURRETS] = { 0 };
    float turretSpawnTimer = 0.0f;
    const Color turretColor = (Color){ 232, 148, 44, 255 };

    WakeParticle wake[MAX_WAKE_PARTICLES] = { 0 };
    float wakeEmitTimer = 0.0f;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A))  player.x -= playerSpeed * dt;
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) player.x += playerSpeed * dt;
        if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W))    player.y -= playerSpeed * dt;
        if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S))  player.y += playerSpeed * dt;

        float halfW = playerTex.width / 2.0f;
        float halfH = playerTex.height / 2.0f;
        if (player.x < halfW) player.x = halfW;
        if (player.x > GAME_WIDTH - halfW) player.x = GAME_WIDTH - halfW;
        if (player.y < halfH) player.y = halfH;
        if (player.y > PLAY_HEIGHT - halfH) player.y = PLAY_HEIGHT - halfH;

        // World scrolls right-to-left under the player. Kept bounded by
        // the tile width (rather than growing unbounded) since REPEAT
        // wrap only needs the offset modulo the tile size.
        oceanScroll += OCEAN_SCROLL_SPEED * dt;
        if (oceanScroll >= oceanTex.width) oceanScroll -= oceanTex.width;

        // Wake: drop a puff from each rear ski-point on a fixed cadence.
        // The 1px jitter keeps the two trails from reading as ruled lines.
        wakeEmitTimer += dt;
        while (wakeEmitTimer >= WAKE_EMIT_INTERVAL) {
            wakeEmitTimer -= WAKE_EMIT_INTERVAL;
            for (int ski = -1; ski <= 1; ski += 2) {
                for (int i = 0; i < MAX_WAKE_PARTICLES; i++) {
                    if (!wake[i].active) {
                        wake[i].active = true;
                        wake[i].age = 0.0f;
                        wake[i].pos = (Vector2){
                            player.x - halfW + (float)GetRandomValue(-1, 1),
                            player.y + ski * WAKE_SKI_OFFSET_Y + (float)GetRandomValue(-1, 1)
                        };
                        break;
                    }
                }
            }
        }

        for (int i = 0; i < MAX_WAKE_PARTICLES; i++) {
            if (!wake[i].active) continue;
            wake[i].age += dt;
            wake[i].pos.x -= OCEAN_SCROLL_SPEED * dt;
            if (wake[i].age >= WAKE_LIFETIME || wake[i].pos.x < 0.0f) wake[i].active = false;
        }

        // Auto-fire: accumulate dt and emit as many shots as the interval
        // allows, so a stalled frame can't silently eat a shot.
        fireTimer += dt;
        while (fireTimer >= BULLET_FIRE_INTERVAL) {
            fireTimer -= BULLET_FIRE_INTERVAL;
            for (int i = 0; i < MAX_BULLETS; i++) {
                if (!bullets[i].active) {
                    bullets[i].active = true;
                    bullets[i].pos = (Vector2){ player.x + halfW, player.y };
                    break;
                }
            }
        }

        for (int i = 0; i < MAX_BULLETS; i++) {
            if (!bullets[i].active) continue;
            bullets[i].pos.x += BULLET_SPEED * dt;
            if (bullets[i].pos.x - BULLET_RADIUS > GAME_WIDTH) bullets[i].active = false;
        }

        // Enemies spawn off the right edge and fly left to meet the
        // player, opposite the bullets, since the world scrolls the
        // other way under them.
        enemySpawnTimer += dt;
        while (enemySpawnTimer >= ENEMY_SPAWN_INTERVAL) {
            enemySpawnTimer -= ENEMY_SPAWN_INTERVAL;
            for (int i = 0; i < MAX_ENEMIES; i++) {
                if (!enemies[i].active) {
                    float margin = ENEMY_RADIUS + ENEMY_SINE_AMPLITUDE;
                    float baseY = (float)GetRandomValue((int)margin, (int)(PLAY_HEIGHT - margin));
                    enemies[i].active = true;
                    enemies[i].hp = ENEMY_HP;
                    enemies[i].t = 0.0f;
                    enemies[i].baseY = baseY;
                    enemies[i].pos = (Vector2){ GAME_WIDTH + ENEMY_RADIUS, baseY };
                    break;
                }
            }
        }

        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (!enemies[i].active) continue;
            enemies[i].t += dt;
            enemies[i].pos.x -= ENEMY_SPEED * dt;
            enemies[i].pos.y = enemies[i].baseY + sinf(enemies[i].t * ENEMY_SINE_FREQUENCY) * ENEMY_SINE_AMPLITUDE;
            if (enemies[i].pos.x < -ENEMY_RADIUS) enemies[i].active = false;
        }

        // Gun-vs-air collision: brute-force O(bullets*enemies) is fine at
        // these pool sizes (32*16 max).
        for (int b = 0; b < MAX_BULLETS; b++) {
            if (!bullets[b].active) continue;
            for (int e = 0; e < MAX_ENEMIES; e++) {
                if (!enemies[e].active) continue;
                float dx = bullets[b].pos.x - enemies[e].pos.x;
                float dy = bullets[b].pos.y - enemies[e].pos.y;
                float hitDist = BULLET_RADIUS + ENEMY_RADIUS;
                if (dx * dx + dy * dy <= hitDist * hitDist) {
                    bullets[b].active = false;
                    enemies[e].hp--;
                    if (enemies[e].hp <= 0) enemies[e].active = false;
                    break;
                }
            }
        }

        // Torpedo: manual second input, gated by both "one in flight at a
        // time" and a reload cooldown so it isn't unlimited-fire like the
        // gun. Runs straight along the fire-time y — lead-targeting toward
        // a turret is still its own TODO item.
        if (torpedoCooldown > 0.0f) torpedoCooldown -= dt;
        if (IsKeyPressed(KEY_SPACE) && !torpedo.active && torpedoCooldown <= 0.0f) {
            torpedo.active = true;
            torpedo.pos = (Vector2){ player.x + halfW, player.y };
            torpedoCooldown = TORPEDO_COOLDOWN;
        }

        if (torpedo.active) {
            torpedo.pos.x += TORPEDO_SPEED * dt;
            if (torpedo.pos.x - TORPEDO_WIDTH / 2.0f > GAME_WIDTH) torpedo.active = false;
        }

        // Turret Platforms spawn off the right edge like air enemies, but
        // being anchored to the water they drift left at the ocean scroll
        // speed instead of flying under their own power.
        turretSpawnTimer += dt;
        while (turretSpawnTimer >= TURRET_SPAWN_INTERVAL) {
            turretSpawnTimer -= TURRET_SPAWN_INTERVAL;
            for (int i = 0; i < MAX_TURRETS; i++) {
                if (!turrets[i].active) {
                    turrets[i].active = true;
                    turrets[i].hp = TURRET_HP;
                    turrets[i].pos = (Vector2){
                        GAME_WIDTH + TURRET_RADIUS,
                        (float)GetRandomValue((int)TURRET_RADIUS, (int)(PLAY_HEIGHT - TURRET_RADIUS))
                    };
                    break;
                }
            }
        }

        for (int i = 0; i < MAX_TURRETS; i++) {
            if (!turrets[i].active) continue;
            turrets[i].pos.x -= OCEAN_SCROLL_SPEED * dt;
            if (turrets[i].pos.x < -TURRET_RADIUS) turrets[i].active = false;
        }

        // Torpedo-vs-surface collision — the gun deliberately has no case
        // here: bullets pass over ground targets (dual-targeting rule).
        if (torpedo.active) {
            for (int i = 0; i < MAX_TURRETS; i++) {
                if (!turrets[i].active) continue;
                float dx = torpedo.pos.x - turrets[i].pos.x;
                float dy = torpedo.pos.y - turrets[i].pos.y;
                float hitDist = TURRET_RADIUS + TORPEDO_WIDTH / 2.0f;
                if (dx * dx + dy * dy <= hitDist * hitDist) {
                    torpedo.active = false;
                    turrets[i].hp--;
                    if (turrets[i].hp <= 0) turrets[i].active = false;
                    break;
                }
            }
        }

        BeginTextureMode(target);
            ClearBackground(BLACK);

            // Single draw call tiles the whole play area: the source
            // rect is larger than the 32x32 texture, so REPEAT wrap mode
            // fills it by sampling the tile over and over.
            DrawTexturePro(
                oceanTex,
                (Rectangle){ oceanScroll, 0, (float)GAME_WIDTH, (float)PLAY_HEIGHT },
                (Rectangle){ 0, 0, (float)GAME_WIDTH, (float)PLAY_HEIGHT },
                (Vector2){ 0, 0 },
                0.0f,
                WHITE
            );

            // Wake sits directly on the water, under every other layer.
            // Foam-colored puffs that fade and shrink with age.
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
            // Placeholder for the Turret Platform per README: low hexagon,
            // amber waterline glow ring, stub cannon facing the player's
            // approach — real sprite art is a separate TODO item.
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
            // Sprite is drawn nose-to-tail already facing right, so no
            // rotation is needed — just center it on the player position.
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

            // Placeholder diamond silhouette for the Skimmer Drone — real
            // sprite art is a separate TODO item.
            for (int i = 0; i < MAX_ENEMIES; i++) {
                if (!enemies[i].active) continue;
                DrawPoly(enemies[i].pos, 4, ENEMY_RADIUS, 45.0f, enemyColor);
            }

            // Torpedo reads as player tech: hull-white body with pointed
            // nose, cyan spine stripe + engine glow (same "glow = active
            // tech" grammar as the ship), and a fading surface wake behind
            // it. TORPEDO_WIDTH/HEIGHT stay the logical body size; the
            // nose/wake are visual-only overhang.
            if (torpedo.active) {
                float tx = torpedo.pos.x;
                float ty = torpedo.pos.y;
                float hw = TORPEDO_WIDTH / 2.0f;
                float hh = TORPEDO_HEIGHT / 2.0f;
                const Color cyan = (Color){ 76, 224, 232, 255 };

                // fading wake segments trailing off to the left
                for (int i = 0; i < 3; i++) {
                    DrawRectangle(
                        (int)(tx - hw - 5.0f - 7.0f * i),
                        (int)(ty - 1.0f),
                        (int)(6.0f - 1.5f * i),
                        2,
                        (Color){ 207, 239, 240, (unsigned char)(110 - 32 * i) }
                    );
                }

                // engine glow at the tail, pulsing slightly
                unsigned char pulse = (unsigned char)(190 + 60.0f * sinf((float)GetTime() * 20.0f));
                DrawCircle((int)(tx - hw - 1.0f), (int)ty, 2.0f, (Color){ 76, 224, 232, pulse });

                // tail fin
                DrawRectangle((int)(tx - hw), (int)(ty - hh - 1.0f), 2, (int)TORPEDO_HEIGHT + 2, torpedoColor);

                // body
                DrawRectangle((int)(tx - hw), (int)(ty - hh), (int)TORPEDO_WIDTH, (int)TORPEDO_HEIGHT, torpedoColor);

                // nose cone (points right, same order as raylib's CCW convention)
                DrawTriangle(
                    (Vector2){ tx + hw + 4.0f, ty },
                    (Vector2){ tx + hw, ty - hh },
                    (Vector2){ tx + hw, ty + hh },
                    torpedoColor
                );

                // cyan spine stripe along the body centerline
                DrawRectangle((int)(tx - hw + 1.0f), (int)(ty - 0.5f), (int)(TORPEDO_WIDTH - 1.0f), 1, cyan);
            }

            // HUD bar placeholder — real content (score/lives/torpedo
            // status) is a separate TODO item.
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
