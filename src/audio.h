#ifndef SEAVIOUS_AUDIO_H
#define SEAVIOUS_AUDIO_H

#include "raylib.h"
#include "game_state.h"
#include "settings.h"

typedef struct {
    Music stage;
    Music boss;
    Music lament;
    Music *current;  // points at one of the three streams above

    Sound gunShot;
    Sound torpedoLaunch;
    Sound torpedoSplash;
    Sound explosion;
    Sound airPop;
    Sound playerDeath;
    Sound relayLaunch;
    Sound mineDetonation;
    Sound uiBlip;
    Sound mortarFire;
    Sound mortarSalvage;
} GameAudio;

// Initializes the audio device and starts the stage theme at the given
// settings' volumes. The struct is self-referential (current points into
// it), so it is filled in place rather than returned by value.
void LoadGameAudio(GameAudio *audio, const GameSettings *settings);
// Re-applies the user volume trims (0..10 per channel) on top of the
// authored mix; called at load and whenever the options screen changes one.
void ApplyAudioSettings(GameAudio *audio, const GameSettings *settings);
void UpdateGameMusic(GameAudio *audio, const GameState *state);
// Pauses or resumes the current music stream with the simulation.
void SetGameAudioPaused(GameAudio *audio, bool paused);
// Plays one-shot SFX for this frame's game events (fired weapons, impacts,
// destroyed targets, player death, run restart).
void PlayGameSfx(GameAudio *audio, const GameEventQueue *events);
void UnloadGameAudio(GameAudio *audio);

#endif
