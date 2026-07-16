#include "input.h"
#include "raylib.h"

static const InputBinding BINDINGS[INPUT_ACTION_COUNT] = {
    [INPUT_MOVE_LEFT]    = { "MOVE LEFT",    "",                        { KEY_LEFT, KEY_A } },
    [INPUT_MOVE_RIGHT]   = { "MOVE RIGHT",   "",                        { KEY_RIGHT, KEY_D } },
    [INPUT_MOVE_UP]      = { "MOVE UP",      "",                        { KEY_UP, KEY_W } },
    [INPUT_MOVE_DOWN]    = { "MOVE DOWN",    "",                        { KEY_DOWN, KEY_S } },
    [INPUT_FIRE_TORPEDO] = { "FIRE TORPEDO", "SURFACE TARGETS",         { KEY_SPACE } },
    [INPUT_FIRE_MORTAR]  = { "FIRE MORTAR",  "ONCE SALVAGED",           { KEY_LEFT_SHIFT, KEY_X } },
    [INPUT_PAUSE_MENU]   = { "PAUSE / MENU", "",                        { KEY_P } },
    [INPUT_RESTART]      = { "RESTART",      "GAME OVER / STAGE CLEAR", { KEY_R, KEY_ENTER, KEY_KP_ENTER } },
};

const InputBinding *GetInputBinding(InputAction action) {
    if (action < 0 || action >= INPUT_ACTION_COUNT) return NULL;
    return &BINDINGS[action];
}

const char *InputKeyName(int key) {
    switch (key) {
        case KEY_LEFT: return "LEFT";
        case KEY_RIGHT: return "RIGHT";
        case KEY_UP: return "UP";
        case KEY_DOWN: return "DOWN";
        case KEY_A: return "A";
        case KEY_D: return "D";
        case KEY_W: return "W";
        case KEY_S: return "S";
        case KEY_SPACE: return "SPACE";
        case KEY_LEFT_SHIFT: return "L-SHIFT";
        case KEY_X: return "X";
        case KEY_P: return "P";
        case KEY_R: return "R";
        case KEY_ENTER: return "ENTER";
        case KEY_KP_ENTER: return "NUM ENTER";
        default: return "?";
    }
}

bool InputActionDown(InputAction action) {
    const InputBinding *binding = GetInputBinding(action);
    if (binding == NULL) return false;
    for (int i = 0; i < MAX_ACTION_KEYS; i++) {
        if (binding->keys[i] != 0 && IsKeyDown(binding->keys[i])) return true;
    }
    return false;
}

bool InputActionPressed(InputAction action) {
    const InputBinding *binding = GetInputBinding(action);
    if (binding == NULL) return false;
    for (int i = 0; i < MAX_ACTION_KEYS; i++) {
        if (binding->keys[i] != 0 && IsKeyPressed(binding->keys[i])) return true;
    }
    return false;
}
