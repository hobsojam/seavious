#include "assets.h"
#include "audio.h"
#include "boss.h"
#include "game_render.h"
#include "game_state.h"
#include "game_update.h"
#include "stage_data.h"
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
        if (IsKeyPressed(KEY_P)) {
            state.paused = !state.paused;
            SetGameAudioPaused(&audio, state.paused);
        }
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
            // A relay on a quiet lane exercises the launch/flash/SFX path
            // headlessly; the map's own relay sits far beyond the smoke run.
            state.surfaceTargets[1] = (SurfaceTarget){
                .type = SURFACE_TARGET_RELAY_NODE,
                .pos = { 400.0f, 60.0f },
                .radius = RELAY_NODE_RADIUS,
                .hp = RELAY_NODE_HP,
                .aimDirection = { -1.0f, 0.0f },
                .active = true
            };
            // A Mobile Platform on a lower lane runs the self-propelled
            // drift, aimed-fan fire, and stern-wake paths.
            state.surfaceTargets[2] = (SurfaceTarget){
                .type = SURFACE_TARGET_MOBILE_PLATFORM,
                .pos = { 420.0f, 300.0f },
                .radius = MOBILE_PLATFORM_RADIUS,
                .hp = MOBILE_PLATFORM_HP,
                .aimDirection = { -1.0f, 0.0f },
                .active = true
            };
        }
        // A mine dropped onto the player exercises the contact-detonation
        // path (blast + SFX + no score) before the forced bullet hits below.
        if (smokeFrames > 0 && framesRun == 60) {
            state.surfaceTargets[3] = (SurfaceTarget){
                .type = SURFACE_TARGET_MINE,
                .pos = state.player,
                .radius = MINE_RADIUS,
                .hp = MINE_HP,
                .aimDirection = { -1.0f, 0.0f },
                .active = true
            };
        }
        if (smokeFrames > 0 && (framesRun == 120 || framesRun == 240 || framesRun == 360)) {
            state.enemyBullets[0] = (EnemyBullet){ .pos = state.player, .active = true };
        }
        // Jump the scroll so beat 7's islet is on screen: the terrain
        // rendering (organic coastline, beach rings, grain) draws
        // headlessly for the rest of the run. The skipped-over stage
        // events all fire at once; the spawn pools just cap out.
        if (smokeFrames > 0 && framesRun == 200 && !state.bossLock) {
            state.scrollDistance = 4600.0f;
        }
        // Keep the run alive for the scripted boss segment below: the
        // three forced hits above can exhaust the stock, and game over
        // would freeze the boss update path for the rest of the run.
        if (smokeFrames > 0 && framesRun == 365) state.lives = 3;
        // Jump to the map end so the boss lock, the fight entrance, and
        // the music hard-cut all run headlessly in the remaining frames.
        if (smokeFrames > 0 && framesRun == 380 && !state.bossLock) {
            state.scrollDistance = (float)STAGE1_LENGTH_PX;
        }
        // Force-walk the fight through states the short headless run
        // can't reach in real time, so their update+render paths (part
        // fire, scorches, exposed core, shell arc/shadow/blast, salvage
        // dome, stage-clear overlay) all execute.
        if (smokeFrames > 0 && framesRun == 410 && state.boss.phase == BOSS_PHASE_ENTERING) {
            state.boss.phase = BOSS_PHASE_FIGHTING;
            state.boss.hullCenter = (Vector2){ BOSS_PATROL_X, 200.0f };
            state.boss.rotation = 90.0f;
            state.boss.sailDirection = -1;
            state.boss.partHp[BOSS_PART_POD_FORE] = 0;
            state.boss.partHp[BOSS_PART_HULL_FORE] = 0;
            state.boss.coreExposed = true;
            state.boss.shells[0] = (MortarShell){
                .launch = BossMortarPosition(&state.boss),
                .target = { 300.0f, 100.0f },
                .t = 0.3f,
                .active = true
            };
            state.boss.shells[1] = (MortarShell){
                .launch = BossMortarPosition(&state.boss),
                .target = { 350.0f, 250.0f },
                .t = BOSS_MORTAR_AIR_TIME,
                .blastT = 0.25f,
                .landed = true,
                .active = true
            };
            // A SAM already in flight covers the homing + render paths
            // (and likely the shootdown, crossing the gun stream).
            state.boss.missiles[0] = (BossMissile){
                .pos = { 320.0f, 176.0f },
                .vel = { -BOSS_MISSILE_SPEED, 0.0f },
                .active = true
            };
        }
        if (smokeFrames > 0 && framesRun == 450 && state.boss.phase == BOSS_PHASE_FIGHTING) {
            state.boss.phase = BOSS_PHASE_SALVAGE_DOCK;
            state.boss.phaseTimer = 0.0f;
            state.boss.salvageDomePos = BossMortarPosition(&state.boss);
        }
        if (smokeFrames > 0 && framesRun == 470) state.stageClear = true;

        UpdateGame(&state, &assets, dt, smokeFrames > 0 && framesRun == 0);
        PlayGameSfx(&audio, &state.gameEvents);

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
