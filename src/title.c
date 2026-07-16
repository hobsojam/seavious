#include "title.h"
#include "input.h"
#include "menu.h"
#include "raylib.h"
#include <math.h>

enum { TITLE_START, TITLE_OPTIONS, TITLE_CONTROLS, TITLE_QUIT, TITLE_ROOT_COUNT };

static const char *TITLE_LABELS[TITLE_ROOT_COUNT] = {
    "START", "OPTIONS", "CONTROLS", "QUIT"
};

// The attract ship: scaled up as the logo's supporting element, banked a
// subtle few degrees - enough to read "in motion", not enough to fight
// the sprite's baked top-down perspective (a full 45-degree rotation of
// a top-down sprite just looks tilted, not dynamic).
#define TITLE_SHIP_SCALE 2.5f
#define TITLE_SHIP_BANK_DEG -12.0f
#define TITLE_SHIP_X 236.0f
#define TITLE_SHIP_Y 182.0f

void ResetTitleScreen(TitleScreen *title) {
    *title = (TitleScreen){ .screen = TITLE_SCREEN_ROOT };
}

static Vector2 TitleShipPos(float time) {
    // A slow figure-drift: alive, but calm enough to stay "a logo".
    return (Vector2){
        TITLE_SHIP_X + 12.0f * sinf(time * 0.45f),
        TITLE_SHIP_Y + 4.0f * sinf(time * 1.3f)
    };
}

TitleResult UpdateTitleScreen(TitleScreen *title, const GameAssets *assets,
    GameSettings *settings, bool *settingsChanged, float dt) {
    *settingsChanged = false;
    title->time += dt;
    title->oceanScroll = AdvanceOceanScroll(title->oceanScroll, dt, (float)assets->oceanTex.width);
    title->oceanOverlayScroll = AdvanceOceanScroll(
        title->oceanOverlayScroll, dt * OCEAN_OVERLAY_SPEED_SCALE, (float)assets->oceanOverlayTex.width
    );

    // Wake trails from the attract ship's stern: the same particles as
    // in-game, underlining the wordmark with motion.
    Vector2 ship = TitleShipPos(title->time);
    title->wakeEmitTimer += dt;
    while (title->wakeEmitTimer >= WAKE_EMIT_INTERVAL) {
        title->wakeEmitTimer -= WAKE_EMIT_INTERVAL;
        // Stern of the 2.5x ship, nudged down for the bank angle, one
        // trail per ski like in-game.
        for (int ski = -1; ski <= 1; ski += 2) {
            TryEmitWakeParticle(title->wake, MAX_WAKE_PARTICLES, (Vector2){
                ship.x - 56.0f + (float)GetRandomValue(-1, 1),
                ship.y + 12.0f + ski * 18.0f + (float)GetRandomValue(-1, 1)
            });
        }
    }
    UpdateWakeParticles(title->wake, MAX_WAKE_PARTICLES, dt);

    switch (title->screen) {
        case TITLE_SCREEN_ROOT: {
            title->cursor = MenuStepCursor(title->cursor, TITLE_ROOT_COUNT);
            if (!MenuSelectPressed()) return TITLE_RESULT_NONE;
            switch (title->cursor) {
                case TITLE_START: return TITLE_RESULT_START;
                case TITLE_OPTIONS: title->screen = TITLE_SCREEN_OPTIONS; title->cursor = 0; break;
                case TITLE_CONTROLS: title->screen = TITLE_SCREEN_CONTROLS; title->cursor = 0; break;
                case TITLE_QUIT: return TITLE_RESULT_QUIT;
            }
            return TITLE_RESULT_NONE;
        }
        case TITLE_SCREEN_OPTIONS:
            if (UpdateOptionsScreen(&title->cursor, settings, settingsChanged)) {
                title->screen = TITLE_SCREEN_ROOT;
                title->cursor = TITLE_OPTIONS;
            }
            return TITLE_RESULT_NONE;
        case TITLE_SCREEN_CONTROLS:
            if (UpdateControlsScreen()) {
                title->screen = TITLE_SCREEN_ROOT;
                title->cursor = TITLE_CONTROLS;
            }
            return TITLE_RESULT_NONE;
    }
    return TITLE_RESULT_NONE;
}

void DrawTitleScreen(const TitleScreen *title, const GameAssets *assets,
    const GameSettings *settings) {
    const Color cyan = (Color){ 76, 224, 232, 255 };
    const Color pale = (Color){ 232, 248, 248, 255 };
    const Color amber = (Color){ 232, 148, 44, 255 };
    const Color inactive = (Color){ 74, 94, 102, 255 };

    ClearBackground(BLACK);

    // Live ocean under everything: the full canvas (no HUD bar here).
    DrawTexturePro(
        assets->oceanTex,
        (Rectangle){ title->oceanScroll, 0, (float)GAME_WIDTH, (float)GAME_HEIGHT },
        (Rectangle){ 0, 0, (float)GAME_WIDTH, (float)GAME_HEIGHT },
        (Vector2){ 0, 0 }, 0.0f, WHITE
    );
    DrawTexturePro(
        assets->oceanOverlayTex,
        (Rectangle){ title->oceanOverlayScroll, 0, (float)GAME_WIDTH, (float)GAME_HEIGHT },
        (Rectangle){ 0, 0, (float)GAME_WIDTH, (float)GAME_HEIGHT },
        (Vector2){ 0, 0 }, 0.0f, WHITE
    );

    for (int i = 0; i < MAX_WAKE_PARTICLES; i++) {
        if (!title->wake[i].active) continue;
        float life = 1.0f - title->wake[i].age / WAKE_LIFETIME;
        DrawCircleV(title->wake[i].pos, life > 0.5f ? 2.0f : 1.0f,
            (Color){ 207, 239, 240, (unsigned char)(150.0f * life) });
    }

    // The banked attract ship, skimming under the wordmark.
    Vector2 ship = TitleShipPos(title->time);
    DrawTexturePro(
        assets->playerTex,
        (Rectangle){ 0, 0, (float)assets->playerTex.width, (float)assets->playerTex.height },
        (Rectangle){ ship.x, ship.y,
            assets->playerTex.width * TITLE_SHIP_SCALE, assets->playerTex.height * TITLE_SHIP_SCALE },
        (Vector2){ assets->playerTex.width * TITLE_SHIP_SCALE / 2.0f,
            assets->playerTex.height * TITLE_SHIP_SCALE / 2.0f },
        TITLE_SHIP_BANK_DEG, WHITE
    );

    DrawTexture(assets->titleLogoTex, (GAME_WIDTH - assets->titleLogoTex.width) / 2, 52, WHITE);

    if (title->screen == TITLE_SCREEN_ROOT) {
        for (int i = 0; i < TITLE_ROOT_COUNT; i++) {
            bool selected = title->cursor == i;
            int y = 248 + 20 * i;
            if (selected) DrawText(">", 218, y, 10, amber);
            DrawText(TITLE_LABELS[i], 232, y, 10, selected ? pale : cyan);
        }
        DrawText("UP/DOWN + ENTER", 218, 248 + 20 * TITLE_ROOT_COUNT + 4, 8, inactive);
    } else if (title->screen == TITLE_SCREEN_OPTIONS) {
        DrawOptionsScreen(title->cursor, settings);
    } else {
        DrawControlsScreen();
    }
}
