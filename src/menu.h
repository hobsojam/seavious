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

typedef enum {
    QUIT_CONFIRM_NONE,
    QUIT_CONFIRM_CANCEL,
    QUIT_CONFIRM_QUIT
} QuitConfirmationResult;

typedef struct {
    MenuScreen screen;
    int cursor;
} PauseMenu;

// One frame's menu-navigation input, sampled once per frame by the
// caller (ReadMenuInput) and passed through the update functions - the
// menu and title state machines are pure over this struct, so tests can
// drive them without a window or key injection.
typedef struct {
    bool up;
    bool down;
    bool left;
    bool right;
    bool select; // Enter / numpad Enter / Space
    bool back;   // Backspace or Escape
} MenuInput;

MenuInput ReadMenuInput(void);

void ResetPauseMenu(PauseMenu *menu);
// Navigate with the movement actions, select with Enter/Space, step back
// with Backspace or Escape. Sets *settingsChanged when a volume was adjusted this
// frame (the caller applies it to the audio and saves the file).
MenuResult UpdatePauseMenu(PauseMenu *menu, GameSettings *settings, bool *settingsChanged,
    MenuInput input);
void DrawPauseMenu(const PauseMenu *menu, const GameSettings *settings);

// Confirmation shown for an Escape press during active gameplay. Select
// confirms quitting; Backspace/Escape dismisses it and returns to the run.
QuitConfirmationResult UpdateQuitConfirmation(MenuInput input);
void DrawQuitConfirmation(void);

// Shared navigation helper and sub-screens, used by both the pause menu
// and the title screen so the two hosts can never drift apart.
int MenuStepCursor(int cursor, int count, MenuInput input);
// Sub-screen updates return true when the user backs out to the host.
bool UpdateOptionsScreen(int *cursor, GameSettings *settings, bool *settingsChanged,
    MenuInput input);
void DrawOptionsScreen(int cursor, const GameSettings *settings);
bool UpdateControlsScreen(MenuInput input);
void DrawControlsScreen(void);

#endif
