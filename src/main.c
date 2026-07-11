#include "raylib.h"

#define GAME_WIDTH   512
#define GAME_HEIGHT  384
#define HUD_HEIGHT   32
#define PLAY_HEIGHT  (GAME_HEIGHT - HUD_HEIGHT)
#define WINDOW_SCALE 2
#define STAR_COUNT   60

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
        stars[i].y = (float)GetRandomValue(0, PLAY_HEIGHT);
        stars[i].speed = (float)GetRandomValue(40, 160) / 60.0f;
    }

    Vector2 player = { 48.0f, PLAY_HEIGHT / 2.0f };
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
        if (player.y > PLAY_HEIGHT - 8) player.y = PLAY_HEIGHT - 8;

        // World scrolls right-to-left under the player, like the ocean
        // background will once its tile art exists (see TODO.md).
        for (int i = 0; i < STAR_COUNT; i++) {
            stars[i].x -= stars[i].speed;
            if (stars[i].x < 0) {
                stars[i].x = GAME_WIDTH;
                stars[i].y = (float)GetRandomValue(0, PLAY_HEIGHT);
            }
        }

        BeginTextureMode(target);
            ClearBackground(BLACK);

            for (int i = 0; i < STAR_COUNT; i++) {
                DrawPixel((int)stars[i].x, (int)stars[i].y, RAYWHITE);
            }

            // Ship points right (direction of travel / forward fire).
            DrawTriangle(
                (Vector2){ player.x + 8, player.y },
                (Vector2){ player.x - 8, player.y - 6 },
                (Vector2){ player.x - 8, player.y + 6 },
                GREEN
            );

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

    UnloadRenderTexture(target);
    CloseWindow();
    return 0;
}
