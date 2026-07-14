// Music follows the design's hard-cut rule: whole tracks are swapped at
// state changes, never crossfaded or layered live. Theme A carries the
// stage, the boss "hammer" theme takes over while a boss is active, and
// the boss "siren" doubles as a lament over the game-over screen (the
// 0.6s per-life death is far too short for a music cut, so only the
// final death changes the track). Guarded so a headless/no-audio run
// (CI smoke test) just plays nothing instead of crashing.
#include "audio.h"

void LoadGameAudio(GameAudio *audio) {
    InitAudioDevice();
    audio->stage = LoadMusicStream("assets/audio/stage1_theme_a.xm");
    audio->boss = LoadMusicStream("assets/audio/boss1_theme_b.xm");
    audio->lament = LoadMusicStream("assets/audio/boss1_theme_a.xm");
    audio->current = &audio->stage;
    if (IsMusicValid(audio->stage)) PlayMusicStream(audio->stage);
}

void UpdateGameMusic(GameAudio *audio, const GameState *state) {
    Music *want = state->gameOver ? &audio->lament
                : state->bossActive ? &audio->boss
                : &audio->stage;
    if (want != audio->current) {
        if (IsMusicValid(*audio->current)) StopMusicStream(*audio->current);
        if (IsMusicValid(*want)) PlayMusicStream(*want);
        audio->current = want;
    }
    if (IsMusicValid(*audio->current)) UpdateMusicStream(*audio->current);
}

void UnloadGameAudio(GameAudio *audio) {
    UnloadMusicStream(audio->lament);
    UnloadMusicStream(audio->boss);
    UnloadMusicStream(audio->stage);
    CloseAudioDevice();
}
