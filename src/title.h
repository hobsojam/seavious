#ifndef SEAVIOUS_TITLE_H
#define SEAVIOUS_TITLE_H

#include "assets.h"
#include "gameplay.h"
#include "menu.h"
#include "settings.h"

// Title screen (see README "Title screen"): the SEAVIOUS wordmark over
// live scrolling ocean, the player ship as a banked accent skimming
// under it with a real wake trail, and the root menu that links the
// shared options/controls screens (menu.c). Pure UI state - the caller
// owns starting the run, applying/saving settings, and quitting.
typedef enum {
    TITLE_RESULT_NONE,
    TITLE_RESULT_START,
    TITLE_RESULT_QUIT
} TitleResult;

typedef enum {
    TITLE_SCREEN_ROOT,
    TITLE_SCREEN_OPTIONS,
    TITLE_SCREEN_CONTROLS
} TitleSubScreen;

typedef struct {
    TitleSubScreen screen;
    int cursor;
    float time;
    float oceanScroll;
    float oceanOverlayScroll;
    WakeParticle wake[MAX_WAKE_PARTICLES];
    float wakeEmitTimer;
} TitleScreen;

void ResetTitleScreen(TitleScreen *title);
// Pure over the injected input (see MenuInput); assets are only read for
// texture dimensions, so tests can pass a zero-initialized struct with
// the tile widths set.
TitleResult UpdateTitleScreen(TitleScreen *title, const GameAssets *assets,
    GameSettings *settings, bool *settingsChanged, MenuInput input, float dt);
void DrawTitleScreen(const TitleScreen *title, const GameAssets *assets,
    const GameSettings *settings);

#endif
