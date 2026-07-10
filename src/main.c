#include "raylib.h"

#define GAME_WIDTH  240
#define GAME_HEIGHT 320
#define WINDOW_SCALE 3
#define STAR_COUNT 60

typedef struct {
    float x, y;
    float speed;
} Star;

int main(void) {
    InitWindow(GAME_WIDTH * WINDOW_SCALE, GAME_HEIGHT * WINDOW_SCALE, "Seavious");
    SetTargetFPS(60);

    RenderTexture2D target = LoadRenderTexture(GAME_WIDTH, GAME_HEIGHT);
    SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);

    Star stars[STAR_COUNT];
    for (int i = 0; i < STAR_COUNT; i++) {
        stars[i].x = (float)GetRandomValue(0, GAME_WIDTH);
        stars[i].y = (float)GetRandomValue(0, GAME_HEIGHT);
        stars[i].speed = (float)GetRandomValue(40, 160) / 60.0f;
    }

    Vector2 player = { GAME_WIDTH / 2.0f, GAME_HEIGHT - 32.0f };
    const float playerSpeed = 120.0f;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A))  player.x -= playerSpeed * dt;
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) player.x += playerSpeed * dt;
        if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W))    player.y -= playerSpeed * dt;
        if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S))  player.y += playerSpeed * dt;

        if (player.x < 8) player.x = 8;
        if (player.x > GAME_WIDTH - 8) player.x = GAME_WIDTH - 8;
        if (player.y < 8) player.y = 8;
        if (player.y > GAME_HEIGHT - 8) player.y = GAME_HEIGHT - 8;

        for (int i = 0; i < STAR_COUNT; i++) {
            stars[i].y += stars[i].speed;
            if (stars[i].y > GAME_HEIGHT) {
                stars[i].y = 0;
                stars[i].x = (float)GetRandomValue(0, GAME_WIDTH);
            }
        }

        BeginTextureMode(target);
            ClearBackground(BLACK);

            for (int i = 0; i < STAR_COUNT; i++) {
                DrawPixel((int)stars[i].x, (int)stars[i].y, RAYWHITE);
            }

            DrawTriangle(
                (Vector2){ player.x, player.y - 8 },
                (Vector2){ player.x - 6, player.y + 8 },
                (Vector2){ player.x + 6, player.y + 8 },
                GREEN
            );
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

    UnloadRenderTexture(target);
    CloseWindow();
    return 0;
}
