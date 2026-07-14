#ifndef SEAVIOUS_AUDIO_H
#define SEAVIOUS_AUDIO_H

#include "raylib.h"
#include "game_state.h"

typedef struct {
    Music stage;
    Music boss;
    Music lament;
    Music *current;  // points at one of the three streams above
} GameAudio;

// Initializes the audio device and starts the stage theme. The struct is
// self-referential (current points into it), so it is filled in place
// rather than returned by value.
void LoadGameAudio(GameAudio *audio);
void UpdateGameMusic(GameAudio *audio, const GameState *state);
void UnloadGameAudio(GameAudio *audio);

#endif
