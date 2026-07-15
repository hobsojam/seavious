// Music follows the design's hard-cut rule: whole tracks are swapped at
// state changes, never crossfaded or layered live. Theme A carries the
// stage, the boss "hammer" theme takes over while a boss is active, and
// the boss "siren" doubles as a lament over the game-over screen (the
// 0.6s per-life death is far too short for a music cut, so only the
// final death changes the track). Guarded so a headless/no-audio run
// (CI smoke test) just plays nothing instead of crashing.
#include "audio.h"

// Whole-music level under the SFX: even after the in-file mix was
// rebalanced (lead vs bass), the third playtest still found the music
// too loud overall against the game.
#define MUSIC_VOLUME 0.5f

static Sound LoadSfx(const char *path, float volume) {
    Sound sound = LoadSound(path);
    if (IsSoundValid(sound)) SetSoundVolume(sound, volume);
    return sound;
}

void LoadGameAudio(GameAudio *audio) {
    InitAudioDevice();
    audio->stage = LoadMusicStream("assets/audio/stage1_theme_a.xm");
    audio->boss = LoadMusicStream("assets/audio/boss1_theme_b.xm");
    audio->lament = LoadMusicStream("assets/audio/boss1_theme_a.xm");
    if (IsMusicValid(audio->stage)) SetMusicVolume(audio->stage, MUSIC_VOLUME);
    if (IsMusicValid(audio->boss)) SetMusicVolume(audio->boss, MUSIC_VOLUME);
    if (IsMusicValid(audio->lament)) SetMusicVolume(audio->lament, MUSIC_VOLUME);
    audio->current = &audio->stage;
    if (IsMusicValid(audio->stage)) PlayMusicStream(audio->stage);

    // Per-sound volumes balance against the music: the constant auto-fire
    // gun stays far in the background, big one-shots (explosion, death)
    // are allowed to punch through.
    audio->gunShot = LoadSfx("assets/audio/sfx/gun_shot.wav", 0.25f);
    audio->torpedoLaunch = LoadSfx("assets/audio/sfx/torpedo_launch.wav", 0.55f);
    audio->torpedoSplash = LoadSfx("assets/audio/sfx/torpedo_splash.wav", 0.50f);
    audio->explosion = LoadSfx("assets/audio/sfx/explosion.wav", 0.70f);
    audio->airPop = LoadSfx("assets/audio/sfx/air_pop.wav", 0.50f);
    audio->playerDeath = LoadSfx("assets/audio/sfx/player_death.wav", 0.80f);
    audio->relayLaunch = LoadSfx("assets/audio/sfx/relay_launch.wav", 0.40f);
    // Plays under the simultaneous player-death sound (contact detonation
    // always costs the ship), so it sits a step below that 0.80.
    audio->mineDetonation = LoadSfx("assets/audio/sfx/mine_detonation.wav", 0.60f);
    audio->uiBlip = LoadSfx("assets/audio/sfx/ui_blip.wav", 0.45f);
    audio->mortarFire = LoadSfx("assets/audio/sfx/mortar_fire.wav", 0.45f);
    audio->mortarSalvage = LoadSfx("assets/audio/sfx/mortar_salvage.wav", 0.50f);
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

void SetGameAudioPaused(GameAudio *audio, bool paused) {
    if (!IsMusicValid(*audio->current)) return;
    if (paused) PauseMusicStream(*audio->current);
    else ResumeMusicStream(*audio->current);
}

static void PlayIfValid(Sound sound) {
    if (IsSoundValid(sound)) PlaySound(sound);
}

static void StopIfValid(Sound sound) {
    if (IsSoundValid(sound)) StopSound(sound);
}

void PlayGameSfx(GameAudio *audio, const GameEventQueue *events) {
    for (int i = 0; i < events->count; i++) {
        const GameEvent *event = &events->items[i];
        switch (event->type) {
            case GAME_EVENT_GUN_FIRED:
                PlayIfValid(audio->gunShot);
                break;
            case GAME_EVENT_TORPEDO_FIRED:
                PlayIfValid(audio->torpedoLaunch);
                break;
            case GAME_EVENT_TORPEDO_IMPACT:
                // The launch sound carries the whole running flight; cut it
                // at the moment of impact so it never overlaps the payoff.
                StopIfValid(audio->torpedoLaunch);
                // Armed explosions boom; unarmed direct hits just plip.
                PlayIfValid(event->target.torpedoImpact == TORPEDO_IMPACT_EXPLOSION
                    ? audio->explosion : audio->torpedoSplash);
                break;
            case GAME_EVENT_AIR_TARGET_DESTROYED:
                PlayIfValid(audio->airPop);
                break;
            case GAME_EVENT_SURFACE_TARGET_DESTROYED:
                // No dedicated sound: surface kills only happen inside a
                // torpedo explosion, whose boom already covers the moment.
                break;
            case GAME_EVENT_PLAYER_DEATH:
                // Death deactivates an in-flight torpedo without an impact
                // event, so silence its run sound here too.
                StopIfValid(audio->torpedoLaunch);
                PlayIfValid(audio->playerDeath);
                break;
            case GAME_EVENT_RUN_RESTARTED:
                PlayIfValid(audio->uiBlip);
                break;
            case GAME_EVENT_DRONE_LAUNCHED:
                PlayIfValid(audio->relayLaunch);
                break;
            case GAME_EVENT_MINE_DETONATED:
                PlayIfValid(audio->mineDetonation);
                break;
            case GAME_EVENT_BOSS_PART_DESTROYED:
            case GAME_EVENT_BOSS_DEFEATED:
            case GAME_EVENT_MORTAR_BLAST:
                PlayIfValid(audio->explosion);
                break;
            case GAME_EVENT_BOSS_MISSILE_DOWNED:
                // A downed SAM pops like any small air kill; the launch
                // itself stays silent with the rest of the enemy fire
                // (see the enemy-fire SFX task).
                PlayIfValid(audio->airPop);
                break;
            case GAME_EVENT_MORTAR_FIRED:
                PlayIfValid(audio->mortarFire);
                break;
            case GAME_EVENT_MORTAR_SALVAGED:
                PlayIfValid(audio->mortarSalvage);
                break;
        }
    }
}

void UnloadGameAudio(GameAudio *audio) {
    UnloadSound(audio->mortarSalvage);
    UnloadSound(audio->mortarFire);
    UnloadSound(audio->uiBlip);
    UnloadSound(audio->mineDetonation);
    UnloadSound(audio->relayLaunch);
    UnloadSound(audio->playerDeath);
    UnloadSound(audio->airPop);
    UnloadSound(audio->explosion);
    UnloadSound(audio->torpedoSplash);
    UnloadSound(audio->torpedoLaunch);
    UnloadSound(audio->gunShot);
    UnloadMusicStream(audio->lament);
    UnloadMusicStream(audio->boss);
    UnloadMusicStream(audio->stage);
    CloseAudioDevice();
}
