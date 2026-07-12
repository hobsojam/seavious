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
