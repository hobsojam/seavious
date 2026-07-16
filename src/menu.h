#ifndef SEAVIOUS_MENU_H
#define SEAVIOUS_MENU_H

#include "settings.h"

// Pause menu: the host for the options (audio volumes) and controls
// screens until a title screen exists to link them from. Pure UI state -
// nothing here touches the run; the caller owns pausing, applying and
// saving settings, restarting, and quitting.
typedef enum {
    MENU_SCREEN_ROOT,
    MENU_SCREEN_OPTIONS,
    MENU_SCREEN_CONTROLS
} MenuScreen;

typedef enum {
    MENU_RESULT_NONE,
    MENU_RESULT_RESUME,
    MENU_RESULT_RESTART,
    MENU_RESULT_QUIT
} MenuResult;

typedef struct {
    MenuScreen screen;
    int cursor;
} PauseMenu;

void ResetPauseMenu(PauseMenu *menu);
// Navigate with the movement actions, select with Enter/Space, step back
// with Backspace. Sets *settingsChanged when a volume was adjusted this
// frame (the caller applies it to the audio and saves the file).
MenuResult UpdatePauseMenu(PauseMenu *menu, GameSettings *settings, bool *settingsChanged);
void DrawPauseMenu(const PauseMenu *menu, const GameSettings *settings);

#endif
