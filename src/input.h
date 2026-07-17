#ifndef SEAVIOUS_INPUT_H
#define SEAVIOUS_INPUT_H

#include <stdbool.h>

// Central action->keys table: gameplay reads input through actions, and
// the controls screen renders itself from the same table, so the listing
// can never drift from what the game actually reads. Key remapping and
// gamepad support, if they ever land, hook in here (make the table
// mutable / add a second binding column) without touching gameplay code.
typedef enum {
    INPUT_MOVE_LEFT,
    INPUT_MOVE_RIGHT,
    INPUT_MOVE_UP,
    INPUT_MOVE_DOWN,
    INPUT_FIRE_TORPEDO,
    INPUT_FIRE_MORTAR,
    INPUT_PAUSE_MENU,
    INPUT_SCREENSHOT,
    INPUT_RESTART,
    INPUT_ACTION_COUNT
} InputAction;

#define MAX_ACTION_KEYS 3

typedef struct {
    const char *label;         // controls-screen name for the action
    const char *detail;        // when/what note ("" when self-evident)
    int keys[MAX_ACTION_KEYS]; // raylib KEY_* values, 0 = empty slot
} InputBinding;

const InputBinding *GetInputBinding(InputAction action);
// Display name for a bound key ("?" for keys the table never uses).
const char *InputKeyName(int key);
bool InputActionDown(InputAction action);
bool InputActionPressed(InputAction action);

#endif
