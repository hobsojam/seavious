#include "present.h"
#include "title.h"

#include <math.h>
#include <stdio.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\\n", __FILE__, __LINE__, #condition); \
        failures++; \
    } \
} while (0)

#define NEAR(actual, expected) CHECK(fabsf((actual) - (expected)) < 0.01f)

// Title root cursor order (title.c): START, OPTIONS, CONTROLS, QUIT.
// Options-screen cursor order (menu.c): MUSIC, SFX, BACK.
// Pause root cursor order (menu.c): RESUME, OPTIONS, CONTROLS, RESTART, QUIT.

static MenuInput InUp(void) { return (MenuInput){ .up = true }; }
static MenuInput InDown(void) { return (MenuInput){ .down = true }; }
static MenuInput InLeft(void) { return (MenuInput){ .left = true }; }
static MenuInput InRight(void) { return (MenuInput){ .right = true }; }
static MenuInput InSelect(void) { return (MenuInput){ .select = true }; }
static MenuInput InBack(void) { return (MenuInput){ .back = true }; }
static MenuInput InNone(void) { return (MenuInput){ 0 }; }

// UpdateTitleScreen only reads texture dimensions from the assets, so a
// zeroed struct with tile widths set stands in for loaded textures.
static GameAssets FakeAssets(void) {
    GameAssets assets = { 0 };
    assets.oceanTex.width = 128;
    assets.oceanOverlayTex.width = 96;
    return assets;
}

static TitleResult Step(TitleScreen *title, GameAssets *assets, GameSettings *settings,
    bool *changed, MenuInput input) {
    return UpdateTitleScreen(title, assets, settings, changed, input, 0.016f);
}

static void TestTitleRootNavigation(void) {
    TitleScreen title;
    ResetTitleScreen(&title);
    GameAssets assets = FakeAssets();
    GameSettings settings = DefaultGameSettings();
    bool changed = false;

    CHECK(title.screen == TITLE_SCREEN_ROOT && title.cursor == 0);
    CHECK(Step(&title, &assets, &settings, &changed, InNone()) == TITLE_RESULT_NONE);
    CHECK(title.cursor == 0 && !changed);

    // Up from the top wraps to QUIT; down from there wraps back to START.
    Step(&title, &assets, &settings, &changed, InUp());
    CHECK(title.cursor == 3);
    Step(&title, &assets, &settings, &changed, InDown());
    CHECK(title.cursor == 0);

    // Select on START starts the run without touching the screen state.
    CHECK(Step(&title, &assets, &settings, &changed, InSelect()) == TITLE_RESULT_START);
    CHECK(title.screen == TITLE_SCREEN_ROOT);

    // Select on QUIT quits.
    title.cursor = 3;
    CHECK(Step(&title, &assets, &settings, &changed, InSelect()) == TITLE_RESULT_QUIT);
}

static void TestTitleOptionsFlow(void) {
    TitleScreen title;
    ResetTitleScreen(&title);
    GameAssets assets = FakeAssets();
    GameSettings settings = DefaultGameSettings();
    bool changed = false;

    // Enter options from the root; the entering select must not leak
    // into the options screen (it would immediately back out).
    Step(&title, &assets, &settings, &changed, InDown());
    CHECK(Step(&title, &assets, &settings, &changed, InSelect()) == TITLE_RESULT_NONE);
    CHECK(title.screen == TITLE_SCREEN_OPTIONS && title.cursor == 0);

    // Right at full volume clamps and reports no change.
    Step(&title, &assets, &settings, &changed, InRight());
    CHECK(settings.musicVolume == SETTINGS_VOLUME_MAX && !changed);
    // Left steps the music volume down and reports the change.
    Step(&title, &assets, &settings, &changed, InLeft());
    CHECK(settings.musicVolume == SETTINGS_VOLUME_MAX - 1 && changed);

    // The SFX row adjusts SFX only.
    Step(&title, &assets, &settings, &changed, InDown());
    Step(&title, &assets, &settings, &changed, InLeft());
    CHECK(settings.sfxVolume == SETTINGS_VOLUME_MAX - 1);
    CHECK(settings.musicVolume == SETTINGS_VOLUME_MAX - 1);

    // Clamp at mute: drain to zero, then one more is not a change.
    for (int i = 0; i < SETTINGS_VOLUME_MAX; i++) {
        Step(&title, &assets, &settings, &changed, InLeft());
    }
    CHECK(settings.sfxVolume == 0);
    Step(&title, &assets, &settings, &changed, InLeft());
    CHECK(settings.sfxVolume == 0 && !changed);

    // Backspace returns to the root with the cursor kept on OPTIONS.
    CHECK(Step(&title, &assets, &settings, &changed, InBack()) == TITLE_RESULT_NONE);
    CHECK(title.screen == TITLE_SCREEN_ROOT && title.cursor == 1);

    // The BACK row's select is the other way out, same cursor restore.
    Step(&title, &assets, &settings, &changed, InSelect());
    CHECK(title.screen == TITLE_SCREEN_OPTIONS);
    Step(&title, &assets, &settings, &changed, InUp()); // 0 wraps to BACK
    Step(&title, &assets, &settings, &changed, InSelect());
    CHECK(title.screen == TITLE_SCREEN_ROOT && title.cursor == 1);
}

static void TestTitleControlsFlow(void) {
    TitleScreen title;
    ResetTitleScreen(&title);
    GameAssets assets = FakeAssets();
    GameSettings settings = DefaultGameSettings();
    bool changed = false;

    title.cursor = 2;
    Step(&title, &assets, &settings, &changed, InSelect());
    CHECK(title.screen == TITLE_SCREEN_CONTROLS);
    // Idle frames stay on the controls screen.
    Step(&title, &assets, &settings, &changed, InNone());
    CHECK(title.screen == TITLE_SCREEN_CONTROLS);
    // Select and Backspace both return, cursor kept on CONTROLS.
    Step(&title, &assets, &settings, &changed, InSelect());
    CHECK(title.screen == TITLE_SCREEN_ROOT && title.cursor == 2);
    Step(&title, &assets, &settings, &changed, InSelect());
    Step(&title, &assets, &settings, &changed, InBack());
    CHECK(title.screen == TITLE_SCREEN_ROOT && title.cursor == 2);
}

static void TestTitleAmbientSimulation(void) {
    TitleScreen title;
    ResetTitleScreen(&title);
    GameAssets assets = FakeAssets();
    GameSettings settings = DefaultGameSettings();
    bool changed = false;

    // One emit interval spawns the two ski trails' first puffs.
    UpdateTitleScreen(&title, &assets, &settings, &changed, InNone(), WAKE_EMIT_INTERVAL);
    CHECK(title.wake[0].active && title.wake[1].active);
    CHECK(title.time > 0.0f);

    // Long idle: the scroll offsets stay wrapped inside their tile widths
    // and the wake pool keeps cycling without overflowing.
    for (int i = 0; i < 400; i++) {
        UpdateTitleScreen(&title, &assets, &settings, &changed, InNone(), WAKE_EMIT_INTERVAL);
        CHECK(title.oceanScroll >= 0.0f && title.oceanScroll < 128.0f);
        CHECK(title.oceanOverlayScroll >= 0.0f && title.oceanOverlayScroll < 96.0f);
    }
    int active = 0;
    for (int i = 0; i < MAX_WAKE_PARTICLES; i++) {
        if (title.wake[i].active) active++;
    }
    CHECK(active > 0 && active <= MAX_WAKE_PARTICLES);
}

static void TestOptionsFullscreenToggle(void) {
    TitleScreen title;
    ResetTitleScreen(&title);
    GameAssets assets = FakeAssets();
    GameSettings settings = DefaultGameSettings();
    bool changed = false;
    CHECK(!settings.fullscreen);

    // Title -> OPTIONS -> down to the FULLSCREEN row.
    Step(&title, &assets, &settings, &changed, InDown());
    Step(&title, &assets, &settings, &changed, InSelect());
    Step(&title, &assets, &settings, &changed, InDown());
    Step(&title, &assets, &settings, &changed, InDown());

    // Left/right both flip the toggle, each reporting the change.
    Step(&title, &assets, &settings, &changed, InRight());
    CHECK(settings.fullscreen && changed);
    Step(&title, &assets, &settings, &changed, InLeft());
    CHECK(!settings.fullscreen && changed);
    // Idle on the row reports no change.
    Step(&title, &assets, &settings, &changed, InNone());
    CHECK(!changed);
}

static void TestPresentRect(void) {
    // The fixed 2x window gets exactly the old full-window blit.
    CHECK(CalculatePresentScale(1024, 768) == 2);
    Rectangle window = CalculatePresentRect(1024, 768);
    NEAR(window.x, 0.0f);
    NEAR(window.y, 0.0f);
    NEAR(window.width, 1024.0f);
    NEAR(window.height, 768.0f);

    // Integer-only scaling: 1080p letterboxes a 2x image (a fractional
    // 2.8x would make point-filtered pixels uneven), 1440p fits 3x,
    // 4K fits 5x (height-bound).
    CHECK(CalculatePresentScale(1920, 1080) == 2);
    Rectangle fhd = CalculatePresentRect(1920, 1080);
    NEAR(fhd.x, 448.0f);
    NEAR(fhd.y, 156.0f);
    NEAR(fhd.width, 1024.0f);
    NEAR(fhd.height, 768.0f);
    CHECK(CalculatePresentScale(2560, 1440) == 3);
    CHECK(CalculatePresentScale(3840, 2160) == 5);

    // A screen smaller than the canvas still presents at 1x, centered
    // (symmetric crop via negative offsets, never a zero scale).
    CHECK(CalculatePresentScale(300, 200) == 1);
    Rectangle tiny = CalculatePresentRect(300, 200);
    NEAR(tiny.x, -106.0f);
    NEAR(tiny.y, -92.0f);
}

static int fullscreenToggles;

static void CountFullscreenToggle(void) {
    fullscreenToggles++;
}

static void TestFullscreenSynchronization(void) {
    fullscreenToggles = 0;
    CHECK(!SyncFullscreenSetting(false, false, CountFullscreenToggle));
    CHECK(!SyncFullscreenSetting(true, true, CountFullscreenToggle));
    CHECK(fullscreenToggles == 0);

    CHECK(SyncFullscreenSetting(true, false, CountFullscreenToggle));
    CHECK(SyncFullscreenSetting(false, true, CountFullscreenToggle));
    CHECK(fullscreenToggles == 2);
}

static void TestPauseMenuResults(void) {
    PauseMenu menu;
    ResetPauseMenu(&menu);
    GameSettings settings = DefaultGameSettings();
    bool changed = false;

    CHECK(UpdatePauseMenu(&menu, &settings, &changed, InNone()) == MENU_RESULT_NONE);
    // Select on RESUME resumes; Backspace resumes from anywhere on root.
    CHECK(UpdatePauseMenu(&menu, &settings, &changed, InSelect()) == MENU_RESULT_RESUME);
    CHECK(UpdatePauseMenu(&menu, &settings, &changed, InBack()) == MENU_RESULT_RESUME);

    // RESTART and QUIT rows return their results.
    menu.cursor = 3;
    CHECK(UpdatePauseMenu(&menu, &settings, &changed, InSelect()) == MENU_RESULT_RESTART);
    menu.cursor = 4;
    CHECK(UpdatePauseMenu(&menu, &settings, &changed, InSelect()) == MENU_RESULT_QUIT);

    // The shared options screen behaves identically from the pause host:
    // adjust reports the change, back-out restores the cursor.
    menu.cursor = 1;
    CHECK(UpdatePauseMenu(&menu, &settings, &changed, InSelect()) == MENU_RESULT_NONE);
    CHECK(menu.screen == MENU_SCREEN_OPTIONS && menu.cursor == 0);
    UpdatePauseMenu(&menu, &settings, &changed, InLeft());
    CHECK(settings.musicVolume == SETTINGS_VOLUME_MAX - 1 && changed);
    UpdatePauseMenu(&menu, &settings, &changed, InBack());
    CHECK(menu.screen == MENU_SCREEN_ROOT && menu.cursor == 1);
}

int main(void) {
    TestTitleRootNavigation();
    TestTitleOptionsFlow();
    TestTitleControlsFlow();
    TestTitleAmbientSimulation();
    TestOptionsFullscreenToggle();
    TestPresentRect();
    TestFullscreenSynchronization();
    TestPauseMenuResults();
    return failures != 0;
}
