#include "raylib.h"

#define GAME_WIDTH    512
#define GAME_HEIGHT   384
#define HUD_HEIGHT    32
#define PLAY_HEIGHT   (GAME_HEIGHT - HUD_HEIGHT)
#define WINDOW_SCALE  2
#define OCEAN_SCROLL_SPEED 40.0f

int main(void) {
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
