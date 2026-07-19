#include "game_render.h"
#include "boss.h"
#include "stage.h"
#include "stage_data.h"
#include "wreck_visual.h"
#include "raylib.h"
#include "rlgl.h"
#include <math.h>

static float Clamp01(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static float Fraction(float value) {
    return value - floorf(value);
}

static void DrawMineExplosion(const ExplosionEffect *effect) {
    float progress = Clamp01(effect->age / effect->lifetime);
    Vector2 center = effect->pos;

    // The damage footprint reads on the water as a low foam ring. Its
    // vertical squash separates it from the generic circular energy burst.
    float foamT = Clamp01((progress - 0.06f) / 0.94f);
    float foamRadius = effect->radius * (0.18f + 0.82f * foamT);
    unsigned char foamAlpha = (unsigned char)(230.0f * (1.0f - foamT));
    DrawEllipseLines((int)center.x, (int)center.y,
        foamRadius, foamRadius * 0.38f, (Color){ 224, 248, 244, foamAlpha });
    DrawEllipse((int)center.x, (int)center.y,
        foamRadius * 0.42f, foamRadius * 0.16f, (Color){ 82, 180, 190, (unsigned char)(90.0f * (1.0f - foamT)) });

    // A compact underwater pulse gives the plume a dark base rather than
    // the hot orange center used by ordinary surface explosions.
    float pulseRadius = effect->radius * (0.22f + 0.22f * progress);
    DrawCircleV(center, pulseRadius,
        (Color){ 20, 92, 112, (unsigned char)(150.0f * (1.0f - progress)) });

    // The water column rises and collapses during the first three quarters
    // of the effect. A triangle keeps the silhouette crisp at 512x384.
    float plumeT = progress / 0.74f;
    if (plumeT < 1.0f) {
        float rise = 32.0f * sinf(PI * plumeT);
        float halfBase = 7.0f - 2.0f * plumeT;
        unsigned char plumeAlpha = (unsigned char)(235.0f * (1.0f - 0.55f * plumeT));
        Vector2 top = { center.x, center.y - rise };
        DrawTriangle(
            (Vector2){ center.x - halfBase, center.y + 2.0f },
            (Vector2){ center.x + halfBase, center.y + 2.0f },
            top,
            (Color){ 138, 218, 224, plumeAlpha }
        );
        DrawCircleV(top, 3.0f + 2.0f * sinf(PI * plumeT),
            (Color){ 238, 252, 248, plumeAlpha });
    }

    // Stable position-derived variation avoids flickering as the effect is
    // redrawn while still keeping adjacent mines from looking cloned.
    float seed = Fraction(fabsf(center.x * 0.071f + center.y * 0.113f));
    float dropT = Clamp01((progress - 0.08f) / 0.92f);
    unsigned char dropAlpha = (unsigned char)(220.0f * (1.0f - dropT));
    for (int i = 0; i < 8; i++) {
        float variation = Fraction(seed + 0.6180339f * (float)i);
        float angle = 2.0f * PI * ((float)i / 8.0f + seed * 0.12f);
        float travel = effect->radius * (0.38f + 0.42f * variation) * dropT;
        float loft = (12.0f + 12.0f * variation) * sinf(PI * dropT);
        Vector2 droplet = {
            center.x + cosf(angle) * travel,
            center.y + sinf(angle) * travel * 0.38f - loft
        };
        DrawCircleV(droplet, variation > 0.55f ? 2.0f : 1.0f,
            (Color){ 218, 246, 244, dropAlpha });
    }

    // The initiating charge is deliberately brief and nearly white.
    if (progress < 0.16f) {
        float flashT = progress / 0.16f;
        DrawCircleV(center, 4.0f + 9.0f * flashT,
            (Color){ 255, 252, 224, (unsigned char)(255.0f * (1.0f - flashT)) });
    }
}

static void DrawHudShipSlot(Vector2 center, Color color) {
    DrawTriangleLines(
        (Vector2){ center.x + 6.0f, center.y },
        (Vector2){ center.x - 5.0f, center.y - 4.0f },
        (Vector2){ center.x - 5.0f, center.y + 4.0f },
        color
    );
    DrawLine((int)(center.x - 3.0f), (int)center.y, (int)(center.x + 3.0f), (int)center.y, color);
}

static void DrawHud(const GameState *state) {
    int score = state->score;
    int lives = state->lives;
    float torpedoCooldown = state->torpedoCooldown;
    bool torpedoActive = state->torpedo.active;
    const Color panel = (Color){ 10, 14, 18, 255 };
    const Color panelInset = (Color){ 16, 24, 30, 255 };
    const Color cyan = (Color){ 76, 224, 232, 255 };
    const Color pale = (Color){ 232, 248, 248, 255 };
    const Color amber = (Color){ 232, 148, 44, 255 };
    const Color inactive = (Color){ 74, 94, 102, 255 };
    const int top = PLAY_HEIGHT;

    DrawRectangle(0, top, GAME_WIDTH, HUD_HEIGHT, panel);
    DrawLine(0, top, GAME_WIDTH, top, cyan);
    DrawLine(64, top + 5, 64, GAME_HEIGHT - 5, (Color){ 76, 224, 232, 80 });
    DrawLine(236, top + 5, 236, GAME_HEIGHT - 5, (Color){ 76, 224, 232, 80 });
    DrawLine(380, top + 5, 380, GAME_HEIGHT - 5, (Color){ 76, 224, 232, 80 });

    // The active ship consumes one life; the HUD shows the remaining reserves.
    DrawText("LIVES", 6, top + 4, 8, inactive);
    int reserveLives = lives > 0 ? lives - 1 : 0;
    for (int i = 0; i < reserveLives; i++) {
        DrawHudShipSlot((Vector2){ 13.0f + 17.0f * i, (float)top + 21.0f }, cyan);
    }

    DrawText("SCORE", 72, top + 4, 8, cyan);
    DrawText(TextFormat("%08d", score), 72, top + 12, 18, pale);

    DrawText("TORPEDO", 244, top + 4, 8, cyan);
    Color torpedoStatusColor = amber;
    const char *torpedoStatus = "READY";
    float reloadProgress = 1.0f;
    if (torpedoActive) {
        torpedoStatus = "IN FLIGHT";
        torpedoStatusColor = pale;
        reloadProgress = 0.0f;
    } else if (torpedoCooldown > 0.0f) {
        torpedoStatus = "RELOAD";
        torpedoStatusColor = inactive;
        reloadProgress = 1.0f - torpedoCooldown / TORPEDO_COOLDOWN;
        if (reloadProgress < 0.0f) reloadProgress = 0.0f;
        if (reloadProgress > 1.0f) reloadProgress = 1.0f;
    }

    // Weapon panel: the torpedo alone owns the full block with a worded
    // status; once the mortar is scavenged the block splits into two
    // side-by-side icons whose state reads by color alone (green/amber
    // ready, pale in flight, dim with a filling bar on reload).
    int weaponBarWidth = state->hasMortar ? 56 : 126;
    DrawRectangle(244, top + 14, 14, 6, torpedoStatusColor);
    DrawTriangle(
        (Vector2){ 263.0f, (float)top + 17.0f },
        (Vector2){ 258.0f, (float)top + 14.0f },
        (Vector2){ 258.0f, (float)top + 20.0f },
        torpedoStatusColor
    );
    if (!state->hasMortar) DrawText(torpedoStatus, 270, top + 12, 10, torpedoStatusColor);
    DrawRectangle(244, top + 25, weaponBarWidth, 3, panelInset);
    DrawRectangle(244, top + 25, (int)((float)weaponBarWidth * reloadProgress), 3, torpedoStatusColor);

    if (state->hasMortar) {
        const Color green = (Color){ 108, 224, 96, 255 };
        Color mortarStatusColor = green;
        float mortarProgress = 1.0f;
        if (state->mortarShell.active) {
            mortarStatusColor = pale;
            mortarProgress = 0.0f;
        } else if (state->mortarCooldown > 0.0f) {
            mortarStatusColor = inactive;
            mortarProgress = 1.0f - state->mortarCooldown / PLAYER_MORTAR_COOLDOWN;
            if (mortarProgress < 0.0f) mortarProgress = 0.0f;
            if (mortarProgress > 1.0f) mortarProgress = 1.0f;
        }
        DrawText("MORTAR", 314, top + 4, 8, green);
        // Dome-on-base icon: the salvaged turret in miniature.
        DrawCircleV((Vector2){ 321.0f, (float)top + 16.0f }, 4.0f, mortarStatusColor);
        DrawRectangle(314, top + 18, 14, 3, mortarStatusColor);
        DrawRectangle(314, top + 25, 56, 3, panelInset);
        DrawRectangle(314, top + 25, (int)(56.0f * mortarProgress), 3, mortarStatusColor);
    }

    // Reserved boss region: a labeled health meter while the boss lives
    // (sum of remaining destructible-part HP), the empty inset otherwise.
    DrawRectangle(389, top + 12, 115, 8, panelInset);
    DrawRectangleLines(389, top + 12, 115, 8, (Color){ 74, 94, 102, 90 });
    if (state->boss.phase == BOSS_PHASE_ENTERING || state->boss.phase == BOSS_PHASE_FIGHTING) {
        const char *bossName = state->boss.type == BOSS_TYPE_FORTRESS_ATOLL ? "FORTRESS"
            : state->boss.type == BOSS_TYPE_STORM_WARDEN ? "WARDEN" : "LEVIATHAN";
        DrawText(bossName, 389, top + 4, 8, (Color){ 232, 72, 72, 255 });
        int fill = (int)(115.0f * (float)BossRemainingHp(&state->boss) / (float)BossTotalHp(&state->boss));
        DrawRectangle(389, top + 12, fill, 8, (Color){ 232, 72, 72, 255 });
    }
}

// Boss sprites rotate with the ship's heading (the hull sails vertical
// legs with 180-degree turns between them).
static void DrawTextureRotatedAtTinted(Texture2D tex, Vector2 center, float rotation, Color tint) {
    DrawTexturePro(
        tex,
        (Rectangle){ 0, 0, (float)tex.width, (float)tex.height },
        (Rectangle){ center.x, center.y, (float)tex.width, (float)tex.height },
        (Vector2){ tex.width / 2.0f, tex.height / 2.0f },
        rotation,
        tint
    );
}

static void DrawTextureRotatedAt(Texture2D tex, Vector2 center, float rotation) {
    DrawTextureRotatedAtTinted(tex, center, rotation, WHITE);
}

// Torpedo impacts sit above boss/terrain art.  A gate or core impact is
// feedback about that exact surface, so drawing it underneath the boss would
// make a successful hit look like a vanished projectile.
static void DrawTorpedoImpact(const GameState *state) {
    if (state->torpedoImpactTimer <= 0.0f) return;

    float life = state->torpedoImpactTimer / 0.18f;
    if (state->torpedoImpactType == TORPEDO_IMPACT_EXPLOSION) {
        DrawCircleLines((int)state->torpedoImpactPos.x, (int)state->torpedoImpactPos.y,
            TORPEDO_SPLASH_RADIUS * (1.0f + 0.25f * (1.0f - life)),
            (Color){ 255, 220, 140, (unsigned char)(220.0f * life) });
        DrawCircleV(state->torpedoImpactPos, 5.0f,
            (Color){ 255, 246, 216, (unsigned char)(180.0f * life) });
    } else {
        DrawCircleLines((int)state->torpedoImpactPos.x, (int)state->torpedoImpactPos.y,
            8.0f * (1.0f + 0.35f * (1.0f - life)),
            (Color){ 255, 188, 88, (unsigned char)(220.0f * life) });
        DrawCircleV(state->torpedoImpactPos, 4.0f,
            (Color){ 255, 246, 216, (unsigned char)(220.0f * life) });
    }
}

// The boss's water-level body: hull base, shell shadows, live parts,
// exposed core, and the mortar dome (until the salvage lifts it off).
// Drawn with the surface layer, under the player and everything airborne.
static void DrawBoss(const GameState *state, const GameAssets *assets) {
    const BossState *boss = &state->boss;
    if (boss->phase == BOSS_PHASE_INACTIVE) return;

    // The Storm Warden has no art of its own yet (see TODO.md): it
    // borrows the fortress's static-installation rendering wholesale
    // rather than the Leviathan's rotating-hull one, which would be a
    // worse mismatch for a fixed boss. A cold slate tint (bodyTint) is
    // the cheapest real differentiator available without new art - the
    // fortress's warm stone reads distinctly from this at a glance
    // instead of looking like a straight reskin. The gate sprite's
    // open/closed read still lines up semantically with gatesOpen (the
    // Storm Warden's CALM window), even though the art itself will need
    // replacing.
    if (boss->type != BOSS_TYPE_LEVIATHAN) {
        Color bodyTint = boss->type == BOSS_TYPE_STORM_WARDEN
            ? (Color){ 150, 175, 205, 255 } : WHITE;
        Vector2 c = boss->hullCenter;
        DrawTexture(assets->fortressAtollTex,
            (int)(c.x - assets->fortressAtollTex.width / 2.0f),
            (int)(c.y - assets->fortressAtollTex.height / 2.0f), bodyTint);
        for (int part = BOSS_PART_POD_FORE; part <= BOSS_PART_HULL_AFT; part++) {
            Vector2 pos = BossPartPosition(boss, (BossPartId)part);
            bool pod = part <= BOSS_PART_POD_AFT;
            if (!BossPartAlive(boss, (BossPartId)part)) {
                DrawCircleV(pos, pod ? 13.0f : 16.0f, (Color){ 20, 24, 21, 230 });
                DrawCircleLines((int)pos.x, (int)pos.y, pod ? 13.0f : 16.0f,
                    (Color){ 8, 14, 14, 235 });
                continue;
            }
            DrawTextureRotatedAtTinted(pod ? assets->fortressGunTex : assets->fortressMortarTex,
                pos, 0.0f, bodyTint);
        }
        if (BossPartAlive(boss, BOSS_PART_CORE)) {
            Vector2 core = BossPartPosition(boss, BOSS_PART_CORE);
            // The core rises into view only once the outer defenses fall.
            if (boss->coreExposed) {
                DrawTextureRotatedAtTinted(assets->fortressCoreTex, core, 0.0f, bodyTint);
            }
            // The sea gate cycles for the whole fight; its art retracts
            // into two cap pieces when open, leaving the water lane clear.
            bool justToggled = boss->phase == BOSS_PHASE_FIGHTING && boss->gateTimer < 0.2f;
            Color gateTint = justToggled
                ? (Color){ 232, 248, 248, 255 } : bodyTint;
            int gateX = (int)core.x - assets->fortressGateTex.width / 2 - 26;
            int gateY = (int)core.y - assets->fortressGateTex.height / 2;
            if (boss->gatesOpen) {
                DrawTexturePro(assets->fortressGateTex,
                    (Rectangle){ 0, 0, (float)assets->fortressGateTex.width, 7.0f },
                    (Rectangle){ (float)gateX, (float)gateY, (float)assets->fortressGateTex.width, 7.0f },
                    (Vector2){ 0 }, 0.0f, gateTint);
                DrawTexturePro(assets->fortressGateTex,
                    (Rectangle){ 0, (float)assets->fortressGateTex.height - 7.0f,
                        (float)assets->fortressGateTex.width, 7.0f },
                    (Rectangle){ (float)gateX, (float)gateY + 21.0f,
                        (float)assets->fortressGateTex.width, 7.0f },
                    (Vector2){ 0 }, 0.0f, gateTint);
            } else {
                DrawTexture(assets->fortressGateTex, gateX, gateY, gateTint);
            }
        }
        return;
    }

    DrawTextureRotatedAt(
        assets->leviathanHullTex,
        (Vector2){ boss->hullCenter.x, boss->hullCenter.y + boss->settleOffset },
        boss->rotation
    );

    // Burnt sockets where parts used to be: the wreck look assembling in
    // place on the base sprite, one scorch per destroyed pod/section.
    for (int part = BOSS_PART_POD_FORE; part <= BOSS_PART_HULL_AFT; part++) {
        if (BossPartAlive(boss, (BossPartId)part)) continue;
        Vector2 pos = BossPartPosition(boss, (BossPartId)part);
        float radius = part <= BOSS_PART_POD_AFT ? 9.0f : 13.0f;
        DrawCircleV(pos, radius, (Color){ 34, 29, 25, 230 });
        DrawCircleLines((int)pos.x, (int)pos.y, radius, (Color){ 12, 18, 20, 220 });
        DrawCircleV((Vector2){ pos.x - 2.0f, pos.y + 1.0f }, radius * 0.45f, (Color){ 10, 15, 17, 210 });
    }

    // Mortar shadows sit on the water at the landing point: they shrink
    // while the shell rises, grow back as it falls, and the red rim
    // sharpens as the landing (the dodge deadline) closes in.
    for (int i = 0; i < MAX_MORTAR_SHELLS; i++) {
        const MortarShell *shell = &boss->shells[i];
        if (!shell->active || shell->landed) continue;
        float u = shell->t / BOSS_MORTAR_AIR_TIME;
        float radius = 2.0f + 5.0f * fabsf(cosf(PI * u));
        DrawCircleV(shell->target, radius, (Color){ 8, 10, 14, 110 });
        DrawCircleLines((int)shell->target.x, (int)shell->target.y, radius + 1.0f,
            (Color){ 232, 60, 60, (unsigned char)(40.0f + 150.0f * u) });
    }

    if (BossPartAlive(boss, BOSS_PART_HULL_FORE)) {
        DrawTextureRotatedAt(assets->leviathanHullSectionTex,
            BossPartPosition(boss, BOSS_PART_HULL_FORE), boss->rotation);
    }
    if (BossPartAlive(boss, BOSS_PART_HULL_AFT)) {
        DrawTextureRotatedAt(assets->leviathanHullSectionTex,
            BossPartPosition(boss, BOSS_PART_HULL_AFT), boss->rotation);
    }
    if (BossPartAlive(boss, BOSS_PART_POD_FORE)) {
        DrawTextureRotatedAt(assets->leviathanPodTex,
            BossPartPosition(boss, BOSS_PART_POD_FORE), boss->rotation);
    }
    if (BossPartAlive(boss, BOSS_PART_POD_AFT)) {
        DrawTextureRotatedAt(assets->leviathanPodTex,
            BossPartPosition(boss, BOSS_PART_POD_AFT), boss->rotation);
    }
    if (boss->coreExposed && BossPartAlive(boss, BOSS_PART_CORE)) {
        Vector2 core = BossPartPosition(boss, BOSS_PART_CORE);
        DrawTextureRotatedAt(assets->leviathanCoreTex, core, boss->rotation);
        // Escaping glow: the white-hot inside of the machine, breathing.
        unsigned char pulse = (unsigned char)(110 + 70.0f * sinf((float)GetTime() * 9.0f));
        DrawCircleV(core, 5.0f, (Color){ 255, 246, 216, pulse });
    }
    if (boss->phase < BOSS_PHASE_SALVAGE_DOCK) {
        DrawTextureRotatedAt(assets->leviathanMortarTex, BossMortarPosition(boss), boss->rotation);
    }
}

// The boss's airborne pieces: shells arcing over everything on the
// water, their landing blasts, and the salvaged dome's docking flight.
// Drawn above the player and air targets.
static void DrawBossAirborne(const GameState *state, const GameAssets *assets) {
    const BossState *boss = &state->boss;
    if (boss->phase == BOSS_PHASE_INACTIVE) return;

    for (int i = 0; i < MAX_MORTAR_SHELLS; i++) {
        const MortarShell *shell = &boss->shells[i];
        if (!shell->active) continue;
        if (!shell->landed) {
            // Top-down lob: the shell tracks the launch->target line while
            // an arc offset lifts it up-screen and scales it larger near
            // the apex, "rising then falling" over the drifting shadow.
            float u = shell->t / BOSS_MORTAR_AIR_TIME;
            float arc = sinf(PI * u);
            Vector2 pos = {
                shell->launch.x + (shell->target.x - shell->launch.x) * u,
                shell->launch.y + (shell->target.y - shell->launch.y) * u - 40.0f * arc
            };
            DrawPoly(pos, 4, 3.0f + 3.0f * arc, 0.0f, (Color){ 232, 60, 60, 255 });
            DrawPixelV(pos, (Color){ 255, 250, 240, 255 });
        } else {
            // Area blast: red-accented (the universal "this kills you"),
            // distinct from the white-hot destruction explosions.
            float p = 1.0f - shell->blastT / BOSS_MORTAR_BLAST_DURATION;
            unsigned char fade = (unsigned char)(210.0f * (1.0f - p * 0.7f));
            DrawCircleV(shell->target, BOSS_MORTAR_BLAST_RADIUS * (0.4f + 0.6f * p),
                (Color){ 232, 90, 40, fade });
            DrawCircleLines((int)shell->target.x, (int)shell->target.y,
                BOSS_MORTAR_BLAST_RADIUS * (0.6f + 0.4f * p),
                (Color){ 232, 60, 60, (unsigned char)(220.0f * (1.0f - p)) });
            DrawCircleV(shell->target, 8.0f * (1.0f - p), (Color){ 255, 246, 216, fade });
        }
    }

    // SAMs: red dart with an exhaust trail, rotated to heading - enemy
    // ordnance is always red, and the dart shape plus trail separates
    // "homing missile" from the diamond bullets at a glance.
    for (int i = 0; i < MAX_BOSS_MISSILES; i++) {
        const BossMissile *missile = &state->boss.missiles[i];
        if (!missile->active) continue;
        rlPushMatrix();
        rlTranslatef(missile->pos.x, missile->pos.y, 0.0f);
        rlRotatef(atan2f(missile->vel.y, missile->vel.x) * RAD2DEG, 0.0f, 0.0f, 1.0f);
        for (int puff = 0; puff < 3; puff++) {
            DrawCircle(-7 - 4 * puff, 0, 1.6f - 0.3f * (float)puff,
                (Color){ 255, 140, 60, (unsigned char)(150 - 40 * puff) });
        }
        DrawRectangle(-5, -2, 9, 4, (Color){ 232, 60, 60, 255 });
        DrawTriangle(
            (Vector2){ 8.0f, 0.0f },
            (Vector2){ 4.0f, -2.0f },
            (Vector2){ 4.0f, 2.0f },
            (Color){ 232, 60, 60, 255 }
        );
        DrawRectangle(-1, -1, 2, 2, (Color){ 255, 250, 240, 255 });
        rlPopMatrix();
    }

    // Salvage: Stage 1 lifts the mortar dome from the wreck; Stage 2
    // extracts the fortress core's targeting module. Both shrink onto the
    // skimmer's spine, but the computer spins inside a cyan acquisition ring
    // so it cannot be mistaken for the mortar salvaged in the first stage.
    if (boss->phase >= BOSS_PHASE_SALVAGE_DOCK) {
        float u = boss->phase == BOSS_PHASE_SALVAGE_DOCK
            ? boss->phaseTimer / BOSS_SALVAGE_DOCK_DURATION : 1.0f;
        if (u > 1.0f) u = 1.0f;
        float scale = 1.0f - 0.45f * u;
        Vector2 salvagePos = boss->phase == BOSS_PHASE_CLEARED
            ? state->player : boss->salvageDomePos;
        Texture2D salvageTex = assets->leviathanMortarTex;
        float rotation = 0.0f;
        // The Storm Warden's stabilizer salvage reuses the fortress's
        // core-module art too (see the borrowed rendering note above) -
        // both read as "extracting a glowing tech module," which fits
        // better than reusing the plain mortar dome extraction.
        if (boss->type != BOSS_TYPE_LEVIATHAN) {
            salvageTex = assets->fortressCoreTex;
            rotation = 360.0f * u;
            if (boss->phase == BOSS_PHASE_SALVAGE_DOCK) {
                float pulse = 0.5f + 0.5f * sinf(u * PI * 8.0f);
                float ringRadius = 13.0f + 4.0f * pulse;
                unsigned char alpha = (unsigned char)(210.0f * (1.0f - 0.55f * u));
                DrawCircleLines((int)salvagePos.x, (int)salvagePos.y,
                    ringRadius, (Color){ 76, 224, 232, alpha });
                DrawLineEx((Vector2){ salvagePos.x - ringRadius - 3.0f, salvagePos.y },
                    (Vector2){ salvagePos.x - ringRadius + 3.0f, salvagePos.y },
                    1.0f, (Color){ 232, 248, 248, alpha });
                DrawLineEx((Vector2){ salvagePos.x + ringRadius - 3.0f, salvagePos.y },
                    (Vector2){ salvagePos.x + ringRadius + 3.0f, salvagePos.y },
                    1.0f, (Color){ 232, 248, 248, alpha });
            }
        }
        DrawTexturePro(salvageTex,
            (Rectangle){ 0, 0, (float)salvageTex.width, (float)salvageTex.height },
            (Rectangle){ salvagePos.x, salvagePos.y,
                salvageTex.width * scale, salvageTex.height * scale },
            (Vector2){ salvageTex.width * scale / 2.0f,
                salvageTex.height * scale / 2.0f },
            rotation, WHITE);
    }
}

static bool TerrainFootprintsTouch(StageTerrainFootprint first, StageTerrainFootprint second) {
    const float firstRight = first.px + first.widthPx;
    const float firstBottom = first.y + first.heightPx;
    const float secondRight = second.px + second.widthPx;
    const float secondBottom = second.y + second.heightPx;
    return first.px <= secondRight && firstRight >= second.px
        && first.y <= secondBottom && firstBottom >= second.y;
}

// An islet variant whose native aspect suits the destination -
// stretching a round islet over Stage 2's 7:1 merged groups read as
// obvious horizontal smearing (playtest 2026-07-17). Every variant
// within tolerance of the best score is a candidate and the seed picks
// among them, so adjacent chain segments and neighboring islands vary
// instead of stamping one winner (the "two identical islands next to
// each other" playtest note). Keep that band narrow: the former 0.30
// log tolerance admitted sources that needed up to 35% more stretching
// than the best match, which made a deliberately varied selection look
// visibly distorted on Stage 2's 1.75:1 and long-bank groups.
static int PickIsletVariant(const GameAssets *assets, float destAspect,
    unsigned int seed) {
    float score[STAGE1_ISLET_VARIANT_COUNT];
    float best = 1e9f;
    for (int i = 0; i < STAGE1_ISLET_VARIANT_COUNT; i++) {
        Texture2D candidate = assets->stage1IsletTex[i];
        float aspect = candidate.height > 0
            ? (float)candidate.width / (float)candidate.height : 1.0f;
        score[i] = fabsf(logf(aspect / destAspect));
        if (score[i] < best) best = score[i];
    }
    int candidates[STAGE1_ISLET_VARIANT_COUNT];
    int count = 0;
    for (int i = 0; i < STAGE1_ISLET_VARIANT_COUNT; i++) {
        if (score[i] <= best + 0.10f) candidates[count++] = i;
    }
    return candidates[seed % (unsigned int)count];
}

// One island group's art: a single aspect-matched islet for compact and
// double-length groups, and for very long groups a chain of overlapping
// aspect-preserved islets along the axis (an archipelago ridge) instead of
// one sprite stretched to the whole bounding box. Adapts to any stage's
// shapes with no per-stage art.
static void DrawIsletGroup(const GameAssets *assets, Rectangle destination,
    unsigned int seed) {
    float aspect = destination.height > 0.0f
        ? destination.width / destination.height : 1.0f;
    // The authored long variant is continuous at about 3:1. Preserve it as
    // one landform; only start composing sprites beyond that useful aspect.
    int segments = (int)ceilf(aspect / 3.2f);
    if (segments < 1) segments = 1;
    float stride = destination.width / (float)segments;
    // Segments overlap ~40% so the chained blobs fuse into one landmass
    // instead of reading as separate stamped islets.
    float segWidth = segments > 1 ? stride * 1.4f : destination.width;

    // Full sprites establish the silhouette and external shore. For a
    // composite, inset copies then cover any coast pixels that landed inside
    // another segment, leaving one continuous landmass instead of a cyan seam.
    int layerCount = segments > 1 ? 2 : 1;
    for (int layer = 0; layer < layerCount; layer++) {
        for (int seg = 0; seg < segments; seg++) {
            int variant = PickIsletVariant(assets,
                segWidth / destination.height, seed + 31u * (unsigned int)seg);
            Texture2D islet = layer == 0 ? assets->stage1IsletTex[variant]
                : assets->stage1IsletInteriorTex[variant];
            Rectangle source = {
                0.0f, 0.0f, (float)islet.width, (float)islet.height
            };
            // Alternate horizontal flips off the seed so a repeated variant
            // doesn't read as a copy-paste row.
            if (((seed >> seg) ^ seg) & 1) source.width = -source.width;
            Rectangle dst = {
                destination.x + stride * (float)seg
                    + (stride - segWidth) / 2.0f,
                destination.y, segWidth, destination.height
            };
            DrawTexturePro(islet, source, dst, (Vector2){ 0 }, 0.0f, WHITE);
        }
    }
}

// Collision retains the authored rectangular cells. Rendering merges every
// touching group, so islet art replaces the visibly stacked shore rings
// while targets still use the same stage coordinates. Islet art is still
// the Stage 1 set; per-stage art selection remains Stage 2 content work.
static void DrawStandaloneTerrain(const GameState *state, const GameAssets *assets) {
    const StageDescriptor *stage = GetStageDescriptor(state->stageNumber);
    // Belt and braces: stage_tests enforce the cap, so this bail can't
    // fire for shipped data - and if it somehow does, invisible-but-
    // colliding islands must never come back.
    if (stage->terrainCount > MAX_STAGE_TERRAIN_RECTS) return;

    bool visited[MAX_STAGE_TERRAIN_RECTS] = { false };

    for (int start = 0; start < stage->terrainCount; start++) {
        if (visited[start]) continue;

        int pending[MAX_STAGE_TERRAIN_RECTS];
        int next = 0;
        int pendingCount = 1;
        pending[0] = start;
        float minX = 100000.0f, minY = 100000.0f, maxX = -100000.0f, maxY = -100000.0f;
        visited[start] = true;

        while (next < pendingCount) {
            int current = pending[next++];
            Rectangle currentRect = TerrainScreenRect(stage->terrain[current], state->scrollDistance);
            if (currentRect.x < minX) minX = currentRect.x;
            if (currentRect.y < minY) minY = currentRect.y;
            if (currentRect.x + currentRect.width > maxX) maxX = currentRect.x + currentRect.width;
            if (currentRect.y + currentRect.height > maxY) maxY = currentRect.y + currentRect.height;

            for (int candidate = 0; candidate < stage->terrainCount; candidate++) {
                if (!visited[candidate] && TerrainFootprintsTouch(stage->terrain[current], stage->terrain[candidate])) {
                    visited[candidate] = true;
                    pending[pendingCount++] = candidate;
                }
            }
        }

        const float shorelineMargin = 10.0f;
        Rectangle destination = {
            minX - shorelineMargin, minY - shorelineMargin,
            maxX - minX + shorelineMargin * 2.0f, maxY - minY + shorelineMargin * 2.0f
        };
        if (destination.x > (float)GAME_WIDTH + shorelineMargin || destination.x + destination.width < -shorelineMargin) continue;
        unsigned int variantSeed = (unsigned int)stage->terrain[start].px * 17u
            ^ (unsigned int)stage->terrain[start].y * 31u ^ (unsigned int)start;
        DrawIsletGroup(assets, destination, variantSeed);
    }
}

// Stage 2 land installations: drawn on their terrain pads, above the
// terrain/pad layer and below everything moving on the water.
static void DrawLandTargets(const GameState *state, const GameAssets *assets) {
    for (int i = 0; i < MAX_LAND_TARGETS; i++) {
        if (!state->landTargets[i].active) continue;
        Texture2D tex = state->landTargets[i].type == LAND_TARGET_DRONE_BUNKER
            ? assets->droneBunkerTex : assets->mortarBatteryTex;
        DrawTexture(tex,
            (int)(state->landTargets[i].pos.x - tex.width / 2.0f),
            (int)(state->landTargets[i].pos.y - tex.height / 2.0f),
            WHITE);
    }
}

// Land-battery shells speak the boss's red mortar language verbatim:
// shadow telegraph on the surface, arcing shell and blast at the top.
static void DrawLandMortarShadows(const GameState *state) {
    for (int i = 0; i < MAX_LAND_TARGETS; i++) {
        const MortarShell *shell = &state->landTargets[i].shell;
        if (!shell->active || shell->landed) continue;
        float u = shell->t / LAND_MORTAR_AIR_TIME;
        float radius = 2.0f + 5.0f * fabsf(cosf(PI * u));
        DrawCircleV(shell->target, radius, (Color){ 8, 10, 14, 110 });
        DrawCircleLines((int)shell->target.x, (int)shell->target.y, radius + 1.0f,
            (Color){ 232, 60, 60, (unsigned char)(40.0f + 150.0f * u) });
    }
}

static void DrawLandMortarShells(const GameState *state) {
    for (int i = 0; i < MAX_LAND_TARGETS; i++) {
        const MortarShell *shell = &state->landTargets[i].shell;
        if (!shell->active) continue;
        if (!shell->landed) {
            float u = shell->t / LAND_MORTAR_AIR_TIME;
            float arc = sinf(PI * u);
            Vector2 pos = {
                shell->launch.x + (shell->target.x - shell->launch.x) * u,
                shell->launch.y + (shell->target.y - shell->launch.y) * u - 40.0f * arc
            };
            DrawPoly(pos, 4, 3.0f + 3.0f * arc, 0.0f, (Color){ 232, 60, 60, 255 });
            DrawPixelV(pos, (Color){ 255, 250, 240, 255 });
        } else {
            float p = 1.0f - shell->blastT / LAND_MORTAR_BLAST_DURATION;
            unsigned char fade = (unsigned char)(210.0f * (1.0f - p * 0.7f));
            DrawCircleV(shell->target, LAND_MORTAR_BLAST_RADIUS * (0.4f + 0.6f * p),
                (Color){ 232, 90, 40, fade });
            DrawCircleLines((int)shell->target.x, (int)shell->target.y,
                LAND_MORTAR_BLAST_RADIUS * (0.6f + 0.4f * p),
                (Color){ 232, 60, 60, (unsigned char)(220.0f * (1.0f - p)) });
            DrawCircleV(shell->target, 8.0f * (1.0f - p), (Color){ 255, 246, 216, fade });
        }
    }
}

// Rogue waves read as water, not a shield/energy effect: a low mound
// piling up at the edge while telegraphing (the gap already visible so
// the player can start moving early), then a genuine wall - foam crest
// leading a trailing body of turbulent water - that sweeps the entire
// play width. Only the gap around gapCenterY stays passable.
static void DrawRogueWaves(const GameState *state) {
    const Color deepWater = { 40, 110, 122, 255 };
    const Color foamCrest = { 226, 248, 244, 255 };

    for (int i = 0; i < MAX_ROGUE_WAVES; i++) {
        const RogueWave *wave = &state->rogueWaves[i];
        if (!wave->active) continue;
        // Stable per-wave variation (index-derived, not position-derived:
        // the front moves continuously, so a position seed would jitter
        // the whitecap pattern every frame instead of holding still).
        float seed = Fraction(0.4137f + 0.6180339f * (float)i);
        RogueWaveGapBand gap = ComputeRogueWaveGapBand(wave->gapCenterY);

        if (!wave->sweeping) {
            // Telegraph: water mounding at the edge, reaching further
            // left as it builds toward the surge - the gap is already
            // where it will be, so reading the lane and moving early is
            // rewarded rather than only reacting once it breaks.
            float bulge = RogueWaveTelegraphBulge(wave->t);
            unsigned char alpha = (unsigned char)RogueWaveTelegraphAlpha(wave->t);
            if (gap.top > 0.0f) {
                DrawRectangle((int)(wave->frontX - bulge), 0, (int)bulge, (int)gap.top,
                    (Color){ deepWater.r, deepWater.g, deepWater.b, alpha });
            }
            if (gap.bottom < (float)PLAY_HEIGHT) {
                DrawRectangle((int)(wave->frontX - bulge), (int)gap.bottom, (int)bulge,
                    (int)((float)PLAY_HEIGHT - gap.bottom), (Color){ deepWater.r, deepWater.g, deepWater.b, alpha });
            }
            continue;
        }

        // Sweeping: a full wall with one passable gap. The crest (bright
        // foam) leads at frontX; a trailing body fades from the crest
        // back into ordinary water rather than reading as a hard slab.
        const float trailDepth = 34.0f;
        for (int half = 0; half < 2; half++) {
            float top = half == 0 ? 0.0f : gap.bottom;
            float bottom = half == 0 ? gap.top : (float)PLAY_HEIGHT;
            if (bottom <= top) continue;
            DrawRectangleGradientH((int)wave->frontX, (int)top, (int)trailDepth, (int)(bottom - top),
                (Color){ deepWater.r, deepWater.g, deepWater.b, 210 },
                (Color){ deepWater.r, deepWater.g, deepWater.b, 30 });
            DrawRectangle((int)(wave->frontX - 3.0f), (int)top, (int)(ROGUE_WAVE_FRONT_THICKNESS * 0.5f),
                (int)(bottom - top), foamCrest);
        }

        // Whitecap flecks scattered along the crest for texture. Biased
        // downwind (same crosswind pushing the player and torpedoes) so
        // the spray visibly answers to the current wind instead of
        // sitting in a perfectly vertical band.
        float windLean = state->windSign * 5.0f;
        for (int f = 0; f < 14; f++) {
            Vector2 fleck = RogueWaveWhitecapFleckPos(seed, f, wave->frontX, windLean);
            if (fleck.y > gap.top && fleck.y < gap.bottom) continue;
            DrawCircleV(fleck, Fraction(seed + 0.6180339f * (float)f) > 0.5f ? 1.5f : 1.0f,
                (Color){ 255, 255, 250, 220 });
        }

        // The gap reads as a wind-carved trough, not an arbitrary hole:
        // extra churn right at whichever gap edge the current wind is
        // blowing toward, as if the crosswind is knocking the crest down
        // there.
        float knockedEdgeY = RogueWaveKnockedEdgeY(state->windSign, gap);
        for (int f = 0; f < 6; f++) {
            Vector2 fleck = RogueWaveKnockdownFleckPos(seed, f, wave->frontX, state->windSign, knockedEdgeY);
            float variation = Fraction(seed + 0.381966f + 0.6180339f * (float)f);
            DrawCircleV(fleck, variation > 0.5f ? 1.5f : 1.0f, (Color){ 255, 255, 250, 190 });
        }
    }
}

#define STORM_OVERLAY_FADE_TIME 0.4f
#define STORM_OVERLAY_WASH_ALPHA 85.0f
static const Color STORM_OVERLAY_WASH_COLOR = { 24, 32, 48, 255 };

// One tuning knob set per rain layer, named instead of scattered through
// the draw call - the parallax look comes entirely from the back/front
// layers below using different values for the same shared draw.
typedef struct {
    float spacing;
    float slant;
    float speedPxPerSec;
    float thickness;
    float maxAlpha;
    Color color;
} StormRainLayer;

static const StormRainLayer STORM_RAIN_BACK_LAYER = {
    .spacing = 22.0f, .slant = 15.0f, .speedPxPerSec = 90.0f,
    .thickness = 1.0f, .maxAlpha = 75.0f, .color = { 140, 165, 200, 255 }
};
static const StormRainLayer STORM_RAIN_FRONT_LAYER = {
    .spacing = 34.0f, .slant = 26.0f, .speedPxPerSec = 170.0f,
    .thickness = 2.0f, .maxAlpha = 160.0f, .color = { 205, 228, 238, 255 }
};

// A handful of diagonal streaks scrolling at the layer's own speed reads
// as weather. Driven by wall-clock time rather than scrollDistance:
// UpdateStageScript freezes scrollDistance once bossLock is set, so a
// scroll-tied phase would go static for the whole boss fight.
static void DrawStormRainLayer(const StormRainLayer *layer, float t, float fadeIn) {
    float phase = fmodf(t * layer->speedPxPerSec, layer->spacing);
    Color tint = layer->color;
    tint.a = (unsigned char)(layer->maxAlpha * fadeIn);
    int spacing = (int)layer->spacing;
    for (int i = -2; i < GAME_WIDTH / spacing + 2; i++) {
        float x = (float)i * layer->spacing + phase;
        DrawLineEx((Vector2){ x, 0.0f }, (Vector2){ x - layer->slant, (float)PLAY_HEIGHT },
            layer->thickness, tint);
    }
}

// Screen-wide wash during the Storm Warden's STORM window: parts are
// invulnerable behind it, so the whole play field visibly closing off
// is the readable cue, not just the boss's own tint. Fades in/out at the
// edges of the window so it doesn't hard-cut against CALM, and clears
// entirely once gatesOpen flips true.
static void DrawStormOverlay(const GameState *state) {
    const BossState *boss = &state->boss;
    if (boss->type != BOSS_TYPE_STORM_WARDEN) return;
    if (boss->phase != BOSS_PHASE_FIGHTING) return;
    if (boss->gatesOpen) return;

    float fadeIn = 1.0f;
    if (boss->gateTimer < STORM_OVERLAY_FADE_TIME) {
        fadeIn = boss->gateTimer / STORM_OVERLAY_FADE_TIME;
    } else if (boss->gateTimer > STORM_WARDEN_STORM_DURATION - STORM_OVERLAY_FADE_TIME) {
        fadeIn = (STORM_WARDEN_STORM_DURATION - boss->gateTimer) / STORM_OVERLAY_FADE_TIME;
    }
    if (fadeIn < 0.0f) fadeIn = 0.0f;
    if (fadeIn > 1.0f) fadeIn = 1.0f;

    Color wash = STORM_OVERLAY_WASH_COLOR;
    wash.a = (unsigned char)(STORM_OVERLAY_WASH_ALPHA * fadeIn);
    DrawRectangle(0, 0, GAME_WIDTH, PLAY_HEIGHT, wash);

    float t = (float)GetTime();
    DrawStormRainLayer(&STORM_RAIN_BACK_LAYER, t, fadeIn);
    DrawStormRainLayer(&STORM_RAIN_FRONT_LAYER, t, fadeIn);
}

static void DrawTerrainHardpoints(const GameState *state, const GameAssets *assets) {
    const StageDescriptor *stage = GetStageDescriptor(state->stageNumber);
    Rectangle source = { 0.0f, 0.0f, (float)assets->terrainHardpointTex.width,
        (float)assets->terrainHardpointTex.height };
    for (int i = 0; i < stage->hardpointCount; i++) {
        StageTerrainHardpoint hardpoint = stage->hardpoints[i];
        Rectangle cell = TerrainScreenRect((StageTerrainFootprint){
            hardpoint.px, hardpoint.y, 32, 32
        }, state->scrollDistance);
        if (cell.x > GAME_WIDTH || cell.x + cell.width < 0.0f) continue;
        Rectangle pad = { cell.x + 4.0f, cell.y + 4.0f, 24.0f, 24.0f };
        DrawTexturePro(assets->terrainHardpointTex, source, pad, (Vector2){ 0 }, 0.0f, WHITE);
    }
}

typedef enum {
    TERRAIN_DETAIL_BRUSH,
    TERRAIN_DETAIL_ROCKS,
    TERRAIN_DETAIL_TIDE_POOL,
} TerrainDetailKind;

typedef struct {
    int px;
    int y;
    TerrainDetailKind kind;
} StageTerrainDetail;

// Decorative only: these positions are intentionally clear of hardpoints
// and do not participate in collision or torpedo blocking.
static void DrawTerrainDetails(const GameState *state, const GameAssets *assets) {
    static const StageTerrainDetail details[] = {
        { 452, 14, TERRAIN_DETAIL_BRUSH },
        { 510, 68, TERRAIN_DETAIL_ROCKS },
        { 452, 72, TERRAIN_DETAIL_TIDE_POOL },
        { 4426, 116, TERRAIN_DETAIL_BRUSH },
        { 4460, 166, TERRAIN_DETAIL_ROCKS },
        { 4482, 172, TERRAIN_DETAIL_TIDE_POOL },
    };

    for (int i = 0; i < (int)(sizeof(details) / sizeof(details[0])); i++) {
        Texture2D texture = assets->terrainBrushTex;
        if (details[i].kind == TERRAIN_DETAIL_ROCKS) texture = assets->terrainRockTex;
        if (details[i].kind == TERRAIN_DETAIL_TIDE_POOL) texture = assets->terrainTidePoolTex;

        float x = (float)details[i].px - state->scrollDistance + GAME_WIDTH;
        if (x > GAME_WIDTH || x + texture.width < 0.0f) continue;
        DrawTexture(texture, (int)x, details[i].y, WHITE);
    }
}


void DrawGame(const GameState *state, const GameAssets *assets) {
    const Color bulletColor = (Color){ 76, 224, 232, 255 };
    const Color enemyBulletColor = (Color){ 232, 72, 72, 255 };
    const Color airTargetColor = (Color){ 216, 72, 192, 255 };
    const Color torpedoColor = (Color){ 232, 248, 248, 255 };

    float halfW = assets->playerTex.width / 2.0f;
    Vector2 torpedoSpawn = { state->player.x + halfW, state->player.y };
    const StageDescriptor *currentStage = GetStageDescriptor(state->stageNumber);
    Vector2 torpedoReticle;
    if (state->hasTargetingComputer) {
        Vector2 lead = CalculateLeadTorpedoVelocity(
            torpedoSpawn, state->surfaceTargets, MAX_SURFACE_TARGETS
        );
        torpedoReticle = (Vector2){
            torpedoSpawn.x + lead.x / TORPEDO_SPEED * TORPEDO_MAX_RANGE,
            torpedoSpawn.y + lead.y / TORPEDO_SPEED * TORPEDO_MAX_RANGE
        };
    } else {
        torpedoReticle = CalculateTorpedoReticle(
            torpedoSpawn, currentStage->terrain, currentStage->terrainCount, state->scrollDistance
        );
    }
    // The boss's armored hull clamps the reticle like a land edge, so a
    // shielded lane reads before firing.
    Rectangle bossBlockers[3];
    int bossBlockerCount = BossHullBlockers(&state->boss, bossBlockers);
    torpedoReticle = ClampReticleToRects(torpedoSpawn, torpedoReticle, bossBlockers, bossBlockerCount);

    ClearBackground(BLACK);

    // Single draw call tiles the whole play area: the source rect is
    // larger than the tile texture, so REPEAT wrap fills it by sampling.
    DrawTexturePro(
        assets->oceanTex,
        (Rectangle){ state->oceanScroll, 0, (float)GAME_WIDTH, (float)PLAY_HEIGHT },
        (Rectangle){ 0, 0, (float)GAME_WIDTH, (float)PLAY_HEIGHT },
        (Vector2){ 0, 0 },
        0.0f,
        WHITE
    );

    // The glint overlay belongs to the water surface: it must stay
    // immediately above the base ocean and below everything that
    // sits on the water (wake, surface targets, any future land
    // tiles), so glints never draw on top of solid objects.
    DrawTexturePro(
        assets->oceanOverlayTex,
        (Rectangle){ state->oceanOverlayScroll, 0, (float)GAME_WIDTH, (float)PLAY_HEIGHT },
        (Rectangle){ 0, 0, (float)GAME_WIDTH, (float)PLAY_HEIGHT },
        (Vector2){ 0, 0 },
        0.0f,
        WHITE
    );

    // Wake sits directly on the water, under every other layer.
    for (int i = 0; i < MAX_WAKE_PARTICLES; i++) {
        if (!state->wake[i].active) continue;
        float life = 1.0f - state->wake[i].age / WAKE_LIFETIME;
        DrawCircleV(
            state->wake[i].pos,
            life > 0.5f ? 2.0f : 1.0f,
            (Color){ 207, 239, 240, (unsigned char)(150.0f * life) }
        );
    }

    // Wrecks belong to the water layer and are inert: they only mark
    // that a surface installation was destroyed while the world scrolls on.
    for (int i = 0; i < MAX_SURFACE_WRECKS; i++) {
        if (!state->wrecks[i].active) continue;
        if (DrawSpecialSurfaceWreck(assets->mobilePlatformTex, &state->wrecks[i], DrawTexture)) continue;
        Color scorch = state->wrecks[i].type == SURFACE_TARGET_CASEMATE
            ? (Color){ 28, 36, 34, 230 } : (Color){ 34, 29, 25, 230 };
        DrawCircleV(state->wrecks[i].pos, state->wrecks[i].radius + 2.0f, scorch);
        DrawCircleLines((int)state->wrecks[i].pos.x, (int)state->wrecks[i].pos.y,
            state->wrecks[i].radius + 2.0f, (Color){ 12, 18, 20, 220 });
        DrawCircleV(
            (Vector2){ state->wrecks[i].pos.x - 2.0f, state->wrecks[i].pos.y + 1.0f },
            state->wrecks[i].radius * 0.45f,
            (Color){ 10, 15, 17, 210 }
        );
    }

    DrawStandaloneTerrain(state, assets);
    DrawTerrainDetails(state, assets);
    DrawTerrainHardpoints(state, assets);
    DrawLandTargets(state, assets);
    DrawLandMortarShadows(state);

    // Reticle marks maximum torpedo range, not a target lock. It stands
    // down with the rest of the weapons once the salvage autopilot flies.
    if (!BossSequenceOwnsPlayer(state)) {
        DrawLine((int)torpedoSpawn.x, (int)torpedoSpawn.y, (int)torpedoReticle.x, (int)torpedoReticle.y,
            (Color){ 76, 224, 232, 55 });
        DrawCircleLines((int)torpedoReticle.x, (int)torpedoReticle.y, 6.0f, (Color){ 232, 148, 44, 190 });
        DrawLine((int)(torpedoReticle.x - 10.0f), (int)torpedoReticle.y, (int)(torpedoReticle.x - 4.0f), (int)torpedoReticle.y,
            (Color){ 232, 148, 44, 190 });
        DrawLine((int)(torpedoReticle.x + 4.0f), (int)torpedoReticle.y, (int)(torpedoReticle.x + 10.0f), (int)torpedoReticle.y,
            (Color){ 232, 148, 44, 190 });
        DrawLine((int)torpedoReticle.x, (int)(torpedoReticle.y - 10.0f), (int)torpedoReticle.x, (int)(torpedoReticle.y - 4.0f),
            (Color){ 232, 148, 44, 190 });
        DrawLine((int)torpedoReticle.x, (int)(torpedoReticle.y + 4.0f), (int)torpedoReticle.x, (int)(torpedoReticle.y + 10.0f),
            (Color){ 232, 148, 44, 190 });

        // Mortar reticle: shorter range, never clamped - it deliberately
        // ignores the land edges and armor that cap the torpedo lane, so
        // the two markers split apart exactly when the lane is blocked.
        if (state->hasMortar) {
            Vector2 mortarReticle = CalculateMortarReticle(torpedoSpawn);
            const Color mortarGreen = (Color){ 108, 224, 96, 190 };
            DrawCircleLines((int)mortarReticle.x, (int)mortarReticle.y, 4.0f, mortarGreen);
            DrawLine((int)(mortarReticle.x - 8.0f), (int)mortarReticle.y,
                (int)(mortarReticle.x - 3.0f), (int)mortarReticle.y, mortarGreen);
            DrawLine((int)(mortarReticle.x + 3.0f), (int)mortarReticle.y,
                (int)(mortarReticle.x + 8.0f), (int)mortarReticle.y, mortarGreen);
        }
    }

    // Keep the physical shadow and the gameplay telegraph visually distinct:
    // the dark shadow tracks beneath the shell, while an unfilled green ring
    // marks the water-anchored impact point as it scrolls with the world.
    if (state->mortarShell.active && !state->mortarShell.landed) {
        float u = state->mortarShell.t / PLAYER_MORTAR_AIR_TIME;
        float shadowRadius = 2.0f + 5.0f * fabsf(cosf(PI * u));
        Vector2 shadowPos = CalculateMortarGroundPosition(
            &state->mortarShell, PLAYER_MORTAR_AIR_TIME);
        DrawCircleV(shadowPos, shadowRadius, (Color){ 8, 10, 14, 110 });

        float markerRadius = 7.0f - 2.0f * u;
        Color marker = (Color){ 108, 224, 96,
            (unsigned char)(90.0f + 100.0f * u) };
        Vector2 target = state->mortarShell.target;
        DrawCircleLines((int)target.x, (int)target.y, markerRadius, marker);
        DrawLine((int)(target.x - markerRadius - 3.0f), (int)target.y,
            (int)(target.x - markerRadius), (int)target.y, marker);
        DrawLine((int)(target.x + markerRadius), (int)target.y,
            (int)(target.x + markerRadius + 3.0f), (int)target.y, marker);
        DrawLine((int)target.x, (int)(target.y - markerRadius - 3.0f),
            (int)target.x, (int)(target.y - markerRadius), marker);
        DrawLine((int)target.x, (int)(target.y + markerRadius),
            (int)target.x, (int)(target.y + markerRadius + 3.0f), marker);
    }

    // Surface layer: ground targets draw under everything airborne.
    for (int i = 0; i < MAX_SURFACE_TARGETS; i++) {
        if (!state->surfaceTargets[i].active) continue;
        if (state->surfaceTargets[i].type == SURFACE_TARGET_CASEMATE) {
            // Sprite bakes the waterline ring, hull, fixed left barrel,
            // and emitter core - nothing code-drawn on top.
            DrawTexture(
                assets->casemateTex,
                (int)(state->surfaceTargets[i].pos.x - assets->casemateTex.width / 2.0f),
                (int)(state->surfaceTargets[i].pos.y - assets->casemateTex.height / 2.0f),
                WHITE
            );
        } else if (state->surfaceTargets[i].type == SURFACE_TARGET_RELAY_NODE) {
            // The beacon flash right after a drone launch is code-driven
            // (fireTimer resets to 0 on launch), per the roster design.
            DrawTexture(
                assets->relayNodeTex,
                (int)(state->surfaceTargets[i].pos.x - assets->relayNodeTex.width / 2.0f),
                (int)(state->surfaceTargets[i].pos.y - assets->relayNodeTex.height / 2.0f),
                WHITE
            );
            float flash = 1.0f - state->surfaceTargets[i].fireTimer / 0.3f;
            if (flash > 0.0f) {
                DrawCircleV(state->surfaceTargets[i].pos, 4.0f,
                    (Color){ 255, 224, 168, (unsigned char)(200.0f * flash) });
            }
        } else if (state->surfaceTargets[i].type == SURFACE_TARGET_MINE) {
            // Deliberately the least luminous thing on the water: sprite
            // only, nothing code-drawn to announce it.
            DrawTexture(
                assets->mineTex,
                (int)(state->surfaceTargets[i].pos.x - assets->mineTex.width / 2.0f),
                (int)(state->surfaceTargets[i].pos.y - assets->mineTex.height / 2.0f),
                WHITE
            );
        } else if (state->surfaceTargets[i].type == SURFACE_TARGET_MOBILE_PLATFORM) {
            // Sprite bakes the hull-hugging waterline and edge emitters;
            // the stern wake is emitted into the shared pool at update time.
            DrawTexture(
                assets->mobilePlatformTex,
                (int)(state->surfaceTargets[i].pos.x - assets->mobilePlatformTex.width / 2.0f),
                (int)(state->surfaceTargets[i].pos.y - assets->mobilePlatformTex.height / 2.0f),
                WHITE
            );
        } else {
            // Sprite bakes the ring and rotating mount; only the aiming
            // barrel stays code-drawn so it can lead the player.
            DrawTexture(
                assets->trackingTurretTex,
                (int)(state->surfaceTargets[i].pos.x - assets->trackingTurretTex.width / 2.0f),
                (int)(state->surfaceTargets[i].pos.y - assets->trackingTurretTex.height / 2.0f),
                WHITE
            );
            Vector2 barrelEnd = {
                state->surfaceTargets[i].pos.x + state->surfaceTargets[i].aimDirection.x * (state->surfaceTargets[i].radius + 4.0f),
                state->surfaceTargets[i].pos.y + state->surfaceTargets[i].aimDirection.y * (state->surfaceTargets[i].radius + 4.0f)
            };
            DrawLineEx(state->surfaceTargets[i].pos, barrelEnd, 3.0f, (Color){ 168, 100, 24, 255 });
            DrawCircleV(state->surfaceTargets[i].pos, 3.0f, (Color){ 255, 200, 120, 255 });
        }
    }

    // The boss's body belongs to the surface layer: above the surface
    // targets it dwarfs, below the player and everything airborne.
    DrawBoss(state, assets);
    DrawTorpedoImpact(state);

    // Ship points right (direction of travel / forward fire). It is
    // hidden during its explosion; respawn follows once the effect ends.
    if (!state->playerDestroyed) {
        DrawTexture(
            assets->playerTex,
            (int)(state->player.x - assets->playerTex.width / 2.0f),
            (int)(state->player.y - assets->playerTex.height / 2.0f),
            WHITE
        );
        // The scavenged dome rides the spine (carried over from the
        // previous run's salvage); once a fresh salvage sequence starts,
        // the docking animation in DrawBossAirborne draws it instead.
        if (state->hasMortar && state->boss.phase < BOSS_PHASE_SALVAGE_DOCK) {
            float domeScale = 0.55f;
            DrawTextureEx(
                assets->leviathanMortarTex,
                (Vector2){
                    state->player.x - assets->leviathanMortarTex.width / 2.0f * domeScale,
                    state->player.y - assets->leviathanMortarTex.height / 2.0f * domeScale
                },
                0.0f, domeScale, WHITE
            );
        }
    }

    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!state->bullets[i].active) continue;
        DrawCircleV(state->bullets[i].pos, BULLET_RADIUS, bulletColor);
    }

    // Universal enemy projectile (see README roster): a red diamond with a
    // white-hot single-pixel center, identical for every enemy that shoots.
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) {
        if (!state->enemyBullets[i].active) continue;
        DrawPoly(state->enemyBullets[i].pos, 4, ENEMY_BULLET_RADIUS, 0.0f, enemyBulletColor);
        DrawPixelV(state->enemyBullets[i].pos, (Color){ 255, 250, 240, 255 });
    }

    // Air roster: every sprite bakes its own energy detail (Interceptor
    // spine stripe + nose core, Gunship emitters); only the Skimmer
    // Drone's core pulse stays code-driven (no extra frames), phased by
    // each drone's flight time so swarms shimmer instead of strobing
    // in unison. Core sits 4px ahead of sprite center (see generator).
    for (int i = 0; i < MAX_AIR_TARGETS; i++) {
        if (!state->airTargets[i].active) continue;
        Texture2D airTex = assets->droneTex;
        if (state->airTargets[i].type == AIR_TARGET_INTERCEPTOR) airTex = assets->interceptorTex;
        if (state->airTargets[i].type == AIR_TARGET_GUNSHIP) airTex = assets->gunshipTex;
        DrawTexture(
            airTex,
            (int)(state->airTargets[i].pos.x - airTex.width / 2.0f),
            (int)(state->airTargets[i].pos.y - airTex.height / 2.0f),
            WHITE
        );
        if (state->airTargets[i].type != AIR_TARGET_SKIMMER_DRONE) continue;
        unsigned char corePulse = (unsigned char)(120 + 80.0f * sinf(state->airTargets[i].t * 6.0f));
        DrawCircleV(
            (Vector2){ state->airTargets[i].pos.x - 4.0f, state->airTargets[i].pos.y },
            2.5f,
            (Color){ airTargetColor.r, airTargetColor.g, airTargetColor.b, corePulse }
        );
    }

    for (int i = 0; i < MAX_EXPLOSION_EFFECTS; i++) {
        if (!state->explosions[i].active) continue;
        if (state->explosions[i].type == EXPLOSION_MINE) {
            DrawMineExplosion(&state->explosions[i]);
            continue;
        }
        float progress = state->explosions[i].age / state->explosions[i].lifetime;
        float radius = state->explosions[i].radius * (0.45f + 0.75f * progress);
        unsigned char alpha = (unsigned char)(255.0f * (1.0f - progress));
        Color edge = state->explosions[i].type == EXPLOSION_AIR_TARGET
            ? (Color){ 255, 100, 216, alpha }
            : (Color){ 255, 180, 72, alpha };
        if (state->explosions[i].type == EXPLOSION_PLAYER) edge = (Color){ 96, 232, 255, alpha };
        if (state->explosions[i].type == EXPLOSION_LAND_TARGET) edge = (Color){ 140, 255, 120, alpha };
        DrawCircleLines((int)state->explosions[i].pos.x, (int)state->explosions[i].pos.y, radius, edge);
        DrawCircleV(state->explosions[i].pos, radius * 0.35f, (Color){ 255, 242, 196, alpha });
    }

    // Mortar shells at their arc apex are the highest thing in the scene,
    // and the docking dome flies over the player: top of the world layer.
    DrawBossAirborne(state, assets);
    DrawLandMortarShells(state);
    DrawRogueWaves(state);

    // The player's shell arcs at the same height as the boss's - same
    // lob-and-blast language, green for player ordnance.
    if (state->mortarShell.active) {
        const MortarShell *shell = &state->mortarShell;
        if (!shell->landed) {
            float u = shell->t / PLAYER_MORTAR_AIR_TIME;
            float arc = sinf(PI * u);
            Vector2 shellPos = CalculateMortarGroundPosition(
                shell, PLAYER_MORTAR_AIR_TIME);
            shellPos.y -= 40.0f * arc;
            DrawPoly(shellPos, 4, 3.0f + 3.0f * arc, 0.0f, (Color){ 108, 224, 96, 255 });
            DrawPixelV(shellPos, (Color){ 255, 250, 240, 255 });
        } else {
            float p = 1.0f - shell->blastT / PLAYER_MORTAR_BLAST_DURATION;
            unsigned char fade = (unsigned char)(210.0f * (1.0f - p * 0.7f));
            DrawCircleV(shell->target, PLAYER_MORTAR_BLAST_RADIUS * (0.4f + 0.6f * p),
                (Color){ 130, 224, 90, fade });
            DrawCircleLines((int)shell->target.x, (int)shell->target.y,
                PLAYER_MORTAR_BLAST_RADIUS * (0.6f + 0.4f * p),
                (Color){ 108, 224, 96, (unsigned char)(220.0f * (1.0f - p)) });
            DrawCircleV(shell->target, 8.0f * (1.0f - p), (Color){ 255, 246, 216, fade });
        }
    }

    // Torpedo reads as player tech: hull-white body with pointed nose,
    // cyan spine stripe + engine glow, and a fading surface wake behind it.
    if (state->torpedo.active) {
        float hw = TORPEDO_WIDTH / 2.0f;
        float hh = TORPEDO_HEIGHT / 2.0f;
        const Color cyan = (Color){ 76, 224, 232, 255 };

        rlPushMatrix();
        rlTranslatef(state->torpedo.pos.x, state->torpedo.pos.y, 0.0f);
        rlRotatef(atan2f(state->torpedo.vel.y, state->torpedo.vel.x) * RAD2DEG, 0.0f, 0.0f, 1.0f);

        for (int i = 0; i < 3; i++) {
            DrawRectangle(
                (int)(-hw - 5.0f - 7.0f * i),
                -1,
                (int)(6.0f - 1.5f * i),
                2,
                (Color){ 207, 239, 240, (unsigned char)(110 - 32 * i) }
            );
        }

        unsigned char pulse = (unsigned char)(190 + 60.0f * sinf((float)GetTime() * 20.0f));
        DrawCircle((int)(-hw - 1.0f), 0, 2.0f, (Color){ 76, 224, 232, pulse });
        DrawRectangle((int)-hw, (int)(-hh - 1.0f), 2, (int)TORPEDO_HEIGHT + 2, torpedoColor);
        DrawRectangle((int)-hw, (int)-hh, (int)TORPEDO_WIDTH, (int)TORPEDO_HEIGHT, torpedoColor);
        DrawTriangle(
            (Vector2){ hw + 4.0f, 0.0f },
            (Vector2){ hw, -hh },
            (Vector2){ hw, hh },
            torpedoColor
        );
        DrawRectangle((int)(-hw + 1.0f), 0, (int)(TORPEDO_WIDTH - 1.0f), 1, cyan);

        rlPopMatrix();
    }

    DrawStormOverlay(state);

    if (state->gameOver) {
        DrawRectangle(0, 0, GAME_WIDTH, GAME_HEIGHT, (Color){ 0, 0, 0, 140 });
        DrawText("GAME OVER", 162, 150, 24, (Color){ 232, 248, 248, 255 });
        DrawText("PRESS R TO RESTART", 132, 184, 12, (Color){ 76, 224, 232, 255 });
    } else if (state->stageClear) {
        // Lighter dim than game over: the wreck and the docked upgrade
        // stay readable behind the stage-clear card.
        DrawRectangle(0, 0, GAME_WIDTH, GAME_HEIGHT, (Color){ 0, 0, 0, 110 });
        DrawText(TextFormat("STAGE %d CLEAR", state->stageNumber), 120, 140, 24,
            (Color){ 232, 248, 248, 255 });
        const char *salvageMessage = "MORTAR TURRET SALVAGED";
        Color salvageColor = (Color){ 232, 148, 44, 255 };
        if (GetStageDescriptor(state->stageNumber)->award == UPGRADE_AWARD_TARGETING_COMPUTER) {
            salvageMessage = "TARGETING COMPUTER SALVAGED";
            salvageColor = (Color){ 76, 224, 232, 255 };
        }
        DrawText(salvageMessage, (GAME_WIDTH - MeasureText(salvageMessage, 12)) / 2,
            176, 12, salvageColor);
        DrawText("PRESS R TO CONTINUE", 164, 200, 12, (Color){ 76, 224, 232, 255 });
    } else if (state->respawnInvulnerability > 0.0f) {
        unsigned char blink = (unsigned char)(120 + 80 * sinf(GetTime() * 22.0f));
        DrawText("RESPAWNING", 180, 150, 12, (Color){ 76, 224, 232, blink });
    }

    // The paused overlay is the pause menu's, drawn by DrawPauseMenu on
    // top of this frame (main.c owns the menu; DrawGame stays UI-unaware).

    DrawHud(state);
}
