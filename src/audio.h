#ifndef SEAVIOUS_AUDIO_H
#define SEAVIOUS_AUDIO_H

#include "raylib.h"
#include "game_state.h"

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
    Sound uiBlip;
} GameAudio;

// Initializes the audio device and starts the stage theme. The struct is
// self-referential (current points into it), so it is filled in place
// rather than returned by value.
void LoadGameAudio(GameAudio *audio);
void UpdateGameMusic(GameAudio *audio, const GameState *state);
// Plays one-shot SFX for this frame's game events (fired weapons, impacts,
// destroyed targets, player death, run restart).
void PlayGameSfx(GameAudio *audio, const GameEventQueue *events);
void UnloadGameAudio(GameAudio *audio);

#endif
