#include "settings.h"
#include "input.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\\n", __FILE__, __LINE__, #condition); \
        failures++; \
    } \
} while (0)

static void TestDefaultsAndClamp(void) {
    GameSettings settings = DefaultGameSettings();
    CHECK(settings.musicVolume == SETTINGS_VOLUME_MAX);
    CHECK(settings.sfxVolume == SETTINGS_VOLUME_MAX);

    CHECK(!settings.fullscreen);

    ApplyGameSetting(&settings, "music_volume", -3);
    CHECK(settings.musicVolume == 0);
    ApplyGameSetting(&settings, "sfx_volume", 99);
    CHECK(settings.sfxVolume == SETTINGS_VOLUME_MAX);
    ApplyGameSetting(&settings, "unknown_key", 5);
    CHECK(settings.musicVolume == 0 && settings.sfxVolume == SETTINGS_VOLUME_MAX);

    // Any nonzero value reads as fullscreen-on (hand-edited files).
    ApplyGameSetting(&settings, "fullscreen", 5);
    CHECK(settings.fullscreen);
    ApplyGameSetting(&settings, "fullscreen", 0);
    CHECK(!settings.fullscreen);
}

static void TestMissingFileYieldsDefaults(void) {
    GameSettings settings = LoadGameSettings("settings_test_does_not_exist.cfg");
    CHECK(settings.musicVolume == SETTINGS_VOLUME_MAX);
    CHECK(settings.sfxVolume == SETTINGS_VOLUME_MAX);
}

static void TestSaveLoadRoundtrip(void) {
    const char *path = "settings_test_roundtrip.cfg";
    GameSettings saved = { .musicVolume = 3, .sfxVolume = 7, .fullscreen = true };
    CHECK(SaveGameSettings(&saved, path));
    GameSettings loaded = LoadGameSettings(path);
    CHECK(loaded.musicVolume == 3);
    CHECK(loaded.sfxVolume == 7);
    CHECK(loaded.fullscreen);
    remove(path);
}

static void TestUnparsableLinesIgnoredAndValuesClamped(void) {
    const char *path = "settings_test_garbage.cfg";
    FILE *file = fopen(path, "w");
    CHECK(file != NULL);
    if (file == NULL) return;
    fprintf(file, "# hand-edited comment\nmusic_volume = 4\nnonsense\nsfx_volume=12\n");
    fclose(file);

    GameSettings settings = LoadGameSettings(path);
    CHECK(settings.musicVolume == 4);
    CHECK(settings.sfxVolume == SETTINGS_VOLUME_MAX);
    remove(path);
}

// The controls screen renders itself from the binding table; this keeps
// the table complete (every action labeled, at least one key, and every
// bound key with a real display name) so that screen can't show holes.
static void TestInputTableComplete(void) {
    for (int action = 0; action < INPUT_ACTION_COUNT; action++) {
        const InputBinding *binding = GetInputBinding((InputAction)action);
        CHECK(binding != NULL);
        if (binding == NULL) continue;
        CHECK(binding->label != NULL && binding->label[0] != '\0');
        CHECK(binding->detail != NULL);
        CHECK(binding->keys[0] != 0);
        for (int k = 0; k < MAX_ACTION_KEYS; k++) {
            if (binding->keys[k] == 0) continue;
            CHECK(strcmp(InputKeyName(binding->keys[k]), "?") != 0);
        }
    }
    CHECK(GetInputBinding((InputAction)INPUT_ACTION_COUNT) == NULL);
}

int main(void) {
    TestDefaultsAndClamp();
    TestMissingFileYieldsDefaults();
    TestSaveLoadRoundtrip();
    TestUnparsableLinesIgnoredAndValuesClamped();
    TestInputTableComplete();
    return failures != 0;
}
