#include "menu.h"
#include "input.h"
#include "gameplay.h"
#include "raylib.h"
#include <stdio.h>

enum { ROOT_RESUME, ROOT_OPTIONS, ROOT_CONTROLS, ROOT_RESTART, ROOT_QUIT, ROOT_COUNT };
enum { OPTIONS_MUSIC, OPTIONS_SFX, OPTIONS_FULLSCREEN, OPTIONS_BACK, OPTIONS_COUNT };

static const char *ROOT_LABELS[ROOT_COUNT] = {
    "RESUME", "OPTIONS", "CONTROLS", "RESTART RUN", "QUIT"
};

// Shared HUD palette (see DrawHud) so the menu reads as the same UI.
static const Color MENU_PANEL = { 10, 14, 18, 245 };
static const Color MENU_CYAN = { 76, 224, 232, 255 };
static const Color MENU_PALE = { 232, 248, 248, 255 };
static const Color MENU_AMBER = { 232, 148, 44, 255 };
static const Color MENU_INACTIVE = { 74, 94, 102, 255 };

void ResetPauseMenu(PauseMenu *menu) {
    menu->screen = MENU_SCREEN_ROOT;
    menu->cursor = 0;
}

// The one place raylib input is sampled for menu navigation: everything
// downstream is a pure state machine over the returned struct.
MenuInput ReadMenuInput(void) {
    return (MenuInput){
        .up = InputActionPressed(INPUT_MOVE_UP),
        .down = InputActionPressed(INPUT_MOVE_DOWN),
        .left = InputActionPressed(INPUT_MOVE_LEFT),
        .right = InputActionPressed(INPUT_MOVE_RIGHT),
        .select = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_SPACE),
        // Backspace, not Escape: raylib's default exit key is Escape, so
        // binding "back" there would close the window instead.
        .back = IsKeyPressed(KEY_BACKSPACE),
    };
}

int MenuStepCursor(int cursor, int count, MenuInput input) {
    if (input.up) cursor = (cursor + count - 1) % count;
    if (input.down) cursor = (cursor + 1) % count;
    return cursor;
}

bool UpdateOptionsScreen(int *cursor, GameSettings *settings, bool *settingsChanged,
    MenuInput input) {
    *cursor = MenuStepCursor(*cursor, OPTIONS_COUNT, input);
    int *volume = *cursor == OPTIONS_MUSIC ? &settings->musicVolume
                : *cursor == OPTIONS_SFX ? &settings->sfxVolume
                : NULL;
    if (volume != NULL) {
        int before = *volume;
        if (input.left && *volume > 0) (*volume)--;
        if (input.right && *volume < SETTINGS_VOLUME_MAX) (*volume)++;
        *settingsChanged = *volume != before;
    }
    if (*cursor == OPTIONS_FULLSCREEN && (input.left || input.right)) {
        settings->fullscreen = !settings->fullscreen;
        *settingsChanged = true;
    }
    return input.back || (input.select && *cursor == OPTIONS_BACK);
}

bool UpdateControlsScreen(MenuInput input) {
    return input.back || input.select;
}

MenuResult UpdatePauseMenu(PauseMenu *menu, GameSettings *settings, bool *settingsChanged,
    MenuInput input) {
    *settingsChanged = false;

    switch (menu->screen) {
        case MENU_SCREEN_ROOT: {
            menu->cursor = MenuStepCursor(menu->cursor, ROOT_COUNT, input);
            if (input.back) return MENU_RESULT_RESUME;
            if (!input.select) return MENU_RESULT_NONE;
            switch (menu->cursor) {
                case ROOT_RESUME: return MENU_RESULT_RESUME;
                case ROOT_OPTIONS: menu->screen = MENU_SCREEN_OPTIONS; menu->cursor = 0; break;
                case ROOT_CONTROLS: menu->screen = MENU_SCREEN_CONTROLS; menu->cursor = 0; break;
                case ROOT_RESTART: return MENU_RESULT_RESTART;
                case ROOT_QUIT: return MENU_RESULT_QUIT;
            }
            return MENU_RESULT_NONE;
        }
        case MENU_SCREEN_OPTIONS:
            if (UpdateOptionsScreen(&menu->cursor, settings, settingsChanged, input)) {
                menu->screen = MENU_SCREEN_ROOT;
                menu->cursor = ROOT_OPTIONS;
            }
            return MENU_RESULT_NONE;
        case MENU_SCREEN_CONTROLS:
            if (UpdateControlsScreen(input)) {
                menu->screen = MENU_SCREEN_ROOT;
                menu->cursor = ROOT_CONTROLS;
            }
            return MENU_RESULT_NONE;
    }
    return MENU_RESULT_NONE;
}

static void DrawMenuPanel(const char *title, int panelY, int panelHeight) {
    DrawRectangle(0, 0, GAME_WIDTH, GAME_HEIGHT, (Color){ 0, 0, 0, 115 });
    int panelX = 146;
    int panelWidth = 220;
    DrawRectangle(panelX, panelY, panelWidth, panelHeight, MENU_PANEL);
    DrawRectangleLines(panelX, panelY, panelWidth, panelHeight, MENU_CYAN);
    int titleWidth = MeasureText(title, 18);
    DrawText(title, (GAME_WIDTH - titleWidth) / 2, panelY + 10, 18, MENU_PALE);
}

static void DrawRootMenu(const PauseMenu *menu) {
    DrawMenuPanel("PAUSED", 108, 128);
    for (int i = 0; i < ROOT_COUNT; i++) {
        bool selected = menu->cursor == i;
        int y = 140 + 18 * i;
        if (selected) DrawText(">", 176, y, 10, MENU_AMBER);
        DrawText(ROOT_LABELS[i], 190, y, 10, selected ? MENU_PALE : MENU_CYAN);
    }
    DrawText("UP/DOWN + ENTER", 190, 140 + 18 * ROOT_COUNT + 2, 8, MENU_INACTIVE);
}

static void DrawVolumeRow(const char *label, int value, int y, bool selected) {
    if (selected) DrawText(">", 162, y, 10, MENU_AMBER);
    DrawText(label, 176, y, 10, selected ? MENU_PALE : MENU_CYAN);
    // 0..10 segment bar: filled segments in the row's accent, the rest
    // as inset stubs, with the numeric level after it.
    for (int seg = 0; seg < SETTINGS_VOLUME_MAX; seg++) {
        Color segColor = seg < value ? (selected ? MENU_AMBER : MENU_CYAN) : (Color){ 16, 24, 30, 255 };
        DrawRectangle(240 + 9 * seg, y + 1, 7, 8, segColor);
    }
    DrawText(TextFormat("%2d", value), 334, y, 10, selected ? MENU_PALE : MENU_INACTIVE);
}

void DrawOptionsScreen(int cursor, const GameSettings *settings) {
    DrawMenuPanel("OPTIONS", 110, 126);
    DrawVolumeRow("MUSIC", settings->musicVolume, 142, cursor == OPTIONS_MUSIC);
    DrawVolumeRow("SFX", settings->sfxVolume, 160, cursor == OPTIONS_SFX);
    bool fullscreenSelected = cursor == OPTIONS_FULLSCREEN;
    if (fullscreenSelected) DrawText(">", 162, 178, 10, MENU_AMBER);
    DrawText("FULLSCREEN", 176, 178, 10, fullscreenSelected ? MENU_PALE : MENU_CYAN);
    DrawText(settings->fullscreen ? "ON" : "OFF", 280, 178, 10,
        fullscreenSelected ? MENU_AMBER : MENU_INACTIVE);
    bool backSelected = cursor == OPTIONS_BACK;
    if (backSelected) DrawText(">", 162, 200, 10, MENU_AMBER);
    DrawText("BACK", 176, 200, 10, backSelected ? MENU_PALE : MENU_CYAN);
    DrawText("LEFT/RIGHT TO ADJUST", 176, 218, 8, MENU_INACTIVE);
}

void DrawControlsScreen(void) {
    const int rowHeight = 12;
    // Rows: the keyless gun line, one per action, plus one extra line per
    // action that carries a detail note.
    int rows = 1 + INPUT_ACTION_COUNT;
    for (int action = 0; action < INPUT_ACTION_COUNT; action++) {
        if (GetInputBinding((InputAction)action)->detail[0] != '\0') rows++;
    }
    int panelHeight = 38 + rowHeight * rows + 20;
    int panelY = (GAME_HEIGHT - panelHeight) / 2;
    DrawMenuPanel("CONTROLS", panelY, panelHeight);

    int y = panelY + 34;
    // The gun has no binding to list, but a controls screen that omits
    // the primary weapon would read as a bug.
    DrawText("GUN", 162, y, 8, MENU_CYAN);
    DrawText("AUTO-FIRE (AIR TARGETS)", 240, y, 8, MENU_PALE);
    y += rowHeight;

    for (int action = 0; action < INPUT_ACTION_COUNT; action++) {
        const InputBinding *binding = GetInputBinding((InputAction)action);
        char keys[48] = "";
        int used = 0;
        for (int k = 0; k < MAX_ACTION_KEYS; k++) {
            if (binding->keys[k] == 0) continue;
            used += snprintf(keys + used, sizeof keys - used, "%s%s",
                used > 0 ? " / " : "", InputKeyName(binding->keys[k]));
        }
        DrawText(binding->label, 162, y, 8, MENU_CYAN);
        DrawText(keys, 240, y, 8, MENU_PALE);
        y += rowHeight;
        if (binding->detail[0] != '\0') {
            DrawText(binding->detail, 248, y, 8, MENU_INACTIVE);
            y += rowHeight;
        }
    }
    DrawText("ENTER/BACKSPACE TO RETURN", 162, panelY + panelHeight - 14, 8, MENU_INACTIVE);
}

void DrawPauseMenu(const PauseMenu *menu, const GameSettings *settings) {
    switch (menu->screen) {
        case MENU_SCREEN_ROOT: DrawRootMenu(menu); break;
        case MENU_SCREEN_OPTIONS: DrawOptionsScreen(menu->cursor, settings); break;
        case MENU_SCREEN_CONTROLS: DrawControlsScreen(); break;
    }
}
