#ifndef SEAVIOUS_MENU_H
#define SEAVIOUS_MENU_H

#include "settings.h"

// Pause menu: one host for the options (audio volumes) and controls
// screens; the title screen is the other, through the shared sub-screen
// functions below. Pure UI state - nothing here touches the run; the
// caller owns pausing, applying and saving settings, restarting, and
// quitting.
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

// Shared menu-navigation chords and sub-screens, used by both the pause
// menu and the title screen so the two hosts can never drift apart.
bool MenuSelectPressed(void);
bool MenuBackPressed(void);
int MenuStepCursor(int cursor, int count);
// Sub-screen updates return true when the user backs out to the host.
bool UpdateOptionsScreen(int *cursor, GameSettings *settings, bool *settingsChanged);
void DrawOptionsScreen(int cursor, const GameSettings *settings);
bool UpdateControlsScreen(void);
void DrawControlsScreen(void);

#endif
