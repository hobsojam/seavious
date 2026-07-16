#ifndef SEAVIOUS_SETTINGS_H
#define SEAVIOUS_SETTINGS_H

#include <stdbool.h>

// User-facing configuration, persisted as plain key=value text next to
// the executable (the game already assumes CWD = exe dir for assets/).
// Volumes are 0..10 steps scaling the authored mix - 10 plays the levels
// tuned in audio.c, 0 is muted - so the in-file balance stays the single
// source of truth and the setting is only a user trim on top of it.
#define SETTINGS_FILE "settings.cfg"
#define SETTINGS_VOLUME_MAX 10

typedef struct {
    int musicVolume;
    int sfxVolume;
} GameSettings;

GameSettings DefaultGameSettings(void);
// Clamps and applies one parsed key=value pair; unknown keys are ignored
// so older builds skip settings written by newer ones.
void ApplyGameSetting(GameSettings *settings, const char *key, int value);
// A missing or unreadable file just yields the defaults.
GameSettings LoadGameSettings(const char *path);
bool SaveGameSettings(const GameSettings *settings, const char *path);

#endif
