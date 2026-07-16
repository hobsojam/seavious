#include "settings.h"
#include <stdio.h>
#include <string.h>

GameSettings DefaultGameSettings(void) {
    // Both channels at full: exactly the authored mix.
    return (GameSettings){
        .musicVolume = SETTINGS_VOLUME_MAX,
        .sfxVolume = SETTINGS_VOLUME_MAX
    };
}

static int ClampVolume(int value) {
    if (value < 0) return 0;
    if (value > SETTINGS_VOLUME_MAX) return SETTINGS_VOLUME_MAX;
    return value;
}

void ApplyGameSetting(GameSettings *settings, const char *key, int value) {
    if (strcmp(key, "music_volume") == 0) settings->musicVolume = ClampVolume(value);
    else if (strcmp(key, "sfx_volume") == 0) settings->sfxVolume = ClampVolume(value);
    else if (strcmp(key, "fullscreen") == 0) settings->fullscreen = value != 0;
}

GameSettings LoadGameSettings(const char *path) {
    GameSettings settings = DefaultGameSettings();
    FILE *file = fopen(path, "r");
    if (file == NULL) return settings;

    char line[128];
    while (fgets(line, sizeof line, file) != NULL) {
        char key[64];
        int value;
        // Lines that don't parse as key=value (comments, junk) are skipped.
        if (sscanf(line, " %63[a-z_] = %d", key, &value) == 2) {
            ApplyGameSetting(&settings, key, value);
        }
    }
    fclose(file);
    return settings;
}

bool SaveGameSettings(const GameSettings *settings, const char *path) {
    FILE *file = fopen(path, "w");
    if (file == NULL) return false;
    int written = fprintf(file, "music_volume=%d\nsfx_volume=%d\nfullscreen=%d\n",
        settings->musicVolume, settings->sfxVolume, settings->fullscreen ? 1 : 0);
    fclose(file);
    return written > 0;
}
