#include "assets.h"
#include "audio.h"
#include "boss.h"
#include "game_render.h"
#include "game_state.h"
#include "game_update.h"
#include "input.h"
#include "menu.h"
#include "present.h"
#include "settings.h"
#include "stage.h"
#include "title.h"
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

// Fullscreen is borderless windowed (no video-mode switch, alt-tab
// friendly); the setting is the source of truth and this converges the
// window to it, so boot, F11, and the options row share one path.
static void ApplyFullscreenSetting(const GameSettings *settings) {
    SyncFullscreenSetting(settings->fullscreen,
        IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE), ToggleBorderlessWindowed);
}

int main(void) {
    // Set by the headless CI smoke test so the real game loop exits cleanly
    // after a deterministic number of frames and writes its gcov data.
    const char *smokeFramesValue = getenv("SEAVIOUS_SMOKE_FRAMES");
    int smokeFrames = smokeFramesValue == NULL ? 0 : atoi(smokeFramesValue);
    int framesRun = 0;

    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(GAME_WIDTH * WINDOW_SCALE, GAME_HEIGHT * WINDOW_SCALE, "Seavious");
    SetTargetFPS(60);

    // Runtime window/taskbar icon; the exe file's own icon is the same
    // art embedded via src/seavious.rc (both from tools/gen-exe-icon.py).
    Image windowIcon = LoadImage("assets/icon/window_icon.png");
    if (windowIcon.data != NULL) {
        SetWindowIcon(windowIcon);
        UnloadImage(windowIcon);
    }

    GameSettings settings = LoadGameSettings(SETTINGS_FILE);
    // The smoke run uses the default setting (windowed), but still follows
    // the same initialization path as a real launch.
    ApplyFullscreenSetting(&settings);
    GameAudio audio;
    LoadGameAudio(&audio, &settings);

    RenderTexture2D target = LoadRenderTexture(GAME_WIDTH, GAME_HEIGHT);
    SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);

    GameAssets assets = LoadGameAssets();

    GameState state;
    ResetRunState(&state);

    PauseMenu menu;
    ResetPauseMenu(&menu);
    TitleScreen title;
    ResetTitleScreen(&title);
    // Boot lands on the title screen; START begins a fresh run.
    bool onTitle = true;
    bool quit = false;

    while (!WindowShouldClose() && !quit) {
        float dt = GetFrameTime();
        // Sampled once per frame; the menu/title state machines are pure
        // over this struct (unit-testable without a window).
        MenuInput menuInput = ReadMenuInput();

        // F11 flips fullscreen anywhere (the options row is the
        // discoverable path, this is the shortcut) and persists like any
        // other setting change.
        if (IsKeyPressed(KEY_F11)) {
            settings.fullscreen = !settings.fullscreen;
            ApplyFullscreenSetting(&settings);
            SaveGameSettings(&settings, SETTINGS_FILE);
        }

        // Captured before the title screen may start a run, so the
        // Enter/Space press that selects START can't leak into this same
        // frame's gameplay input (it would fire a torpedo immediately).
        bool runGameFrame = !onTitle;

        if (onTitle) {
            bool settingsChanged = false;
            // The smoke run walks the title's sub-screens for render
            // coverage, then auto-starts (no input injection headlessly).
            if (smokeFrames > 0 && framesRun == 3) title.screen = TITLE_SCREEN_OPTIONS;
            if (smokeFrames > 0 && framesRun == 5) title.screen = TITLE_SCREEN_CONTROLS;
            TitleResult titleResult = UpdateTitleScreen(&title, &assets, &settings, &settingsChanged,
                menuInput, dt);
            if (smokeFrames > 0 && framesRun == 8) titleResult = TITLE_RESULT_START;
            if (settingsChanged) {
                ApplyAudioSettings(&audio, &settings);
                ApplyFullscreenSetting(&settings);
                SaveGameSettings(&settings, SETTINGS_FILE);
                if (IsSoundValid(audio.uiBlip)) PlaySound(audio.uiBlip);
            }
            if (titleResult == TITLE_RESULT_START) {
                ResetRunState(&state);
                onTitle = false;
                if (IsSoundValid(audio.uiBlip)) PlaySound(audio.uiBlip);
            } else if (titleResult == TITLE_RESULT_QUIT) {
                quit = true;
            }
        }

        // The smoke run holds pause open across a few frames after it
        // jumps to the islet, covering the real pause/resume path and
        // (with the forced screens below) the menu render paths, all
        // without input injection.
        bool smokePauseToggle = smokeFrames > 0 && (framesRun == 210 || framesRun == 216);
        if (runGameFrame && (InputActionPressed(INPUT_PAUSE_MENU) || smokePauseToggle)) {
            state.paused = !state.paused;
            if (state.paused) ResetPauseMenu(&menu);
            SetGameAudioPaused(&audio, state.paused);
        }
        if (smokeFrames > 0 && framesRun == 212) menu.screen = MENU_SCREEN_OPTIONS;
        if (smokeFrames > 0 && framesRun == 214) menu.screen = MENU_SCREEN_CONTROLS;
        UpdateGameMusic(&audio, &state, onTitle);

        if (smokeFrames > 0 && framesRun == 9) {
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
        // Stage 2 land class ahead of its stage: a battery and a bunker on
        // quiet lanes run the land drift/fire/launch/render paths
        // headlessly. Blast landings fall inside respawn invulnerability
        // windows, so the scripted lives timeline below still holds.
        if (smokeFrames > 0 && framesRun == 30) {
            state.landTargets[0] = (LandTarget){
                .type = LAND_TARGET_MORTAR_BATTERY,
                .pos = { 430.0f, 90.0f },
                .radius = MORTAR_BATTERY_RADIUS,
                .hp = MORTAR_BATTERY_HP,
                .active = true
            };
            state.landTargets[1] = (LandTarget){
                .type = LAND_TARGET_DRONE_BUNKER,
                .pos = { 430.0f, 260.0f },
                .radius = DRONE_BUNKER_RADIUS,
                .hp = DRONE_BUNKER_HP,
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
        // Grant the scavenged mortar; the forced input passed into UpdateGame
        // below fires it through the normal cooldown/event path.
        if (smokeFrames > 0 && framesRun == 100) {
            state.hasMortar = true;
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
            state.scrollDistance = (float)GetStageDescriptor(state.stageNumber)->lengthPx;
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
        // Exercise the stage-advance flow headlessly: ContinueRun is
        // exactly what the CONTINUE input path calls, and the remaining
        // frames render the advanced run (carried score/mortar, fresh
        // pools).
        if (smokeFrames > 0 && framesRun == 474 && state.stageClear) {
            ContinueRun(&state);
        }

        if (runGameFrame) {
            UpdateGame(&state, &assets, dt,
                smokeFrames > 0 && framesRun == 9,
                smokeFrames > 0 && framesRun == 100);
            PlayGameSfx(&audio, &state.gameEvents);

            // Menu runs after the (frozen) simulation step so a selection
            // that unpauses can't leak its key press into this same frame's
            // gameplay input (Enter would restart on the game-over screen,
            // Space would fire a torpedo).
            if (state.paused) {
                bool settingsChanged = false;
                MenuResult menuResult = UpdatePauseMenu(&menu, &settings, &settingsChanged, menuInput);
                if (settingsChanged) {
                    ApplyAudioSettings(&audio, &settings);
                    ApplyFullscreenSetting(&settings);
                    SaveGameSettings(&settings, SETTINGS_FILE);
                    // Click at the new level so the SFX volume is audible while
                    // adjusting (the music stream stays paused with the sim).
                    if (IsSoundValid(audio.uiBlip)) PlaySound(audio.uiBlip);
                }
                if (menuResult == MENU_RESULT_RESTART) ResetRunState(&state);
                if (menuResult == MENU_RESULT_RESUME || menuResult == MENU_RESULT_RESTART) {
                    state.paused = false;
                    SetGameAudioPaused(&audio, false);
                } else if (menuResult == MENU_RESULT_QUIT) {
                    quit = true;
                }
            }
        }

        BeginTextureMode(target);
            if (onTitle) {
                DrawTitleScreen(&title, &assets, &settings);
            } else {
                DrawGame(&state, &assets);
                if (state.paused) DrawPauseMenu(&menu, &settings);
            }
        EndTextureMode();

        BeginDrawing();
            ClearBackground(BLACK);
            // Integer-scaled and centered on whatever the window/screen
            // is right now: the fixed 2x window gets exactly the old
            // full-window blit, fullscreen letterboxes with black bars.
            DrawTexturePro(
                target.texture,
                (Rectangle){ 0, 0, (float)target.texture.width, -(float)target.texture.height },
                CalculatePresentRect(GetScreenWidth(), GetScreenHeight()),
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
