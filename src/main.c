#include "assets.h"
#include "audio.h"
#include "game_render.h"
#include "game_state.h"
#include "game_update.h"
#include "raylib.h"
#include <stdlib.h>

// On hybrid-graphics laptops (integrated + discrete GPU), the OS/driver
// otherwise defaults to the integrated GPU. These exported symbols are a
// standard convention the NVIDIA and AMD drivers scan for at load time to
// route this specific executable to the discrete GPU instead.
#ifdef _WIN32
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
#endif

int main(void) {
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(GAME_WIDTH * WINDOW_SCALE, GAME_HEIGHT * WINDOW_SCALE, "Seavious");
    SetTargetFPS(60);

    GameAudio audio;
    LoadGameAudio(&audio);

    RenderTexture2D target = LoadRenderTexture(GAME_WIDTH, GAME_HEIGHT);
    SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);

    GameAssets assets = LoadGameAssets();

    GameState state;
    ResetRunState(&state);

    // Set by the headless CI smoke test so the real game loop exits cleanly
    // after a deterministic number of frames and writes its gcov data.
    const char *smokeFramesValue = getenv("SEAVIOUS_SMOKE_FRAMES");
    int smokeFrames = smokeFramesValue == NULL ? 0 : atoi(smokeFramesValue);
    int framesRun = 0;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        UpdateGameMusic(&audio, &state);

        if (smokeFrames > 0 && framesRun == 0) {
            state.surfaceTargets[0] = (SurfaceTarget){
                .type = SURFACE_TARGET_CASEMATE,
                .pos = { state.player.x + 150.0f, state.player.y },
                .radius = CASEMATE_RADIUS,
                .hp = CASEMATE_HP,
                .aimDirection = { -1.0f, 0.0f },
                .active = true
            };
        }
        if (smokeFrames > 0 && (framesRun == 120 || framesRun == 240 || framesRun == 360)) {
            state.enemyBullets[0] = (EnemyBullet){ .pos = state.player, .active = true };
        }

        UpdateGame(&state, &assets, dt, smokeFrames > 0 && framesRun == 0);

        BeginTextureMode(target);
            DrawGame(&state, &assets);
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

    UnloadGameAudio(&audio);
    UnloadGameAssets(&assets);
    UnloadRenderTexture(target);
    CloseWindow();
    return 0;
}
