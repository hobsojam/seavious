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
        DrawText(BossIsFortressAtoll(&state->boss) ? "FORTRESS" : "LEVIATHAN",
            389, top + 4, 8, (Color){ 232, 72, 72, 255 });
        int fill = (int)(115.0f * (float)BossRemainingHp(&state->boss) / (float)BossTotalHp(&state->boss));
        DrawRectangle(389, top + 12, fill, 8, (Color){ 232, 72, 72, 255 });
    }
}

// Boss sprites rotate with the ship's heading (the hull sails vertical
// legs with 180-degree turns between them).
static void DrawTextureRotatedAt(Texture2D tex, Vector2 center, float rotation) {
    DrawTexturePro(
        tex,
        (Rectangle){ 0, 0, (float)tex.width, (float)tex.height },
        (Rectangle){ center.x, center.y, (float)tex.width, (float)tex.height },
        (Vector2){ tex.width / 2.0f, tex.height / 2.0f },
        rotation,
        WHITE
    );
}

// The boss's water-level body: hull base, shell shadows, live parts,
// exposed core, and the mortar dome (until the salvage lifts it off).
// Drawn with the surface layer, under the player and everything airborne.
static void DrawBoss(const GameState *state, const GameAssets *assets) {
    const BossState *boss = &state->boss;
    if (boss->phase == BOSS_PHASE_INACTIVE) return;

    if (BossIsFortressAtoll(boss)) {
        Vector2 c = boss->hullCenter;
        // A low, fortified island rather than a vehicle: layered coast,
        // dark stone, and green installation rings identify the Stage 2
        // land family without needing a second sprite set.
        DrawCircleV(c, 86.0f, (Color){ 40, 104, 112, 255 });
        DrawCircleV(c, 78.0f, (Color){ 224, 218, 172, 255 });
        DrawCircleV(c, 68.0f, (Color){ 58, 66, 42, 255 });
        DrawCircleV(c, 54.0f, (Color){ 28, 35, 30, 255 });
        for (int part = BOSS_PART_POD_FORE; part <= BOSS_PART_HULL_AFT; part++) {
            Vector2 pos = BossPartPosition(boss, (BossPartId)part);
            bool pod = part <= BOSS_PART_POD_AFT;
            if (!BossPartAlive(boss, (BossPartId)part)) {
                DrawCircleV(pos, pod ? 10.0f : 14.0f, (Color){ 20, 24, 21, 240 });
                continue;
            }
            Color ring = pod ? (Color){ 232, 72, 72, 255 } : (Color){ 108, 224, 96, 255 };
            DrawCircleV(pos, pod ? 10.0f : 14.0f, (Color){ 18, 26, 25, 255 });
            DrawCircleLines((int)pos.x, (int)pos.y, pod ? 10.0f : 14.0f, ring);
            DrawCircleV(pos, 3.0f, (Color){ 255, 246, 216, 255 });
        }
        if (BossPartAlive(boss, BOSS_PART_CORE)) {
            Vector2 core = BossPartPosition(boss, BOSS_PART_CORE);
            // The core rises into view only once the outer defenses fall.
            if (boss->coreExposed) {
                DrawCircleV(core, 12.0f, (Color){ 12, 20, 22, 255 });
                DrawCircleLines((int)core.x, (int)core.y, 12.0f,
                    boss->gatesOpen ? (Color){ 76, 224, 232, 255 } : (Color){ 232, 72, 72, 255 });
                DrawCircleV(core, 4.0f, (Color){ 255, 246, 216, 255 });
            }
            // The sea gates cycle for the whole fight so the rhythm can
            // be learned early; a bright edge flashes at each toggle
            // (the visual half of the gate-cycle telegraph).
            bool justToggled = boss->phase == BOSS_PHASE_FIGHTING && boss->gateTimer < 0.2f;
            Color gateEdge = justToggled
                ? (Color){ 232, 248, 248, 255 } : (Color){ 96, 128, 124, 255 };
            if (boss->gatesOpen) {
                // Open: the slabs retract to stubs at the channel mouth.
                DrawRectangle((int)core.x - 22, (int)core.y - 28, 28, 8, (Color){ 36, 48, 48, 240 });
                DrawRectangle((int)core.x - 22, (int)core.y + 20, 28, 8, (Color){ 36, 48, 48, 240 });
                DrawRectangleLines((int)core.x - 22, (int)core.y - 28, 28, 8, gateEdge);
                DrawRectangleLines((int)core.x - 22, (int)core.y + 20, 28, 8, gateEdge);
            } else {
                DrawRectangle((int)core.x - 22, (int)core.y - 28, 28, 56, (Color){ 36, 48, 48, 240 });
                DrawRectangleLines((int)core.x - 22, (int)core.y - 28, 28, 56, gateEdge);
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

    // Salvage: the dome lifts off the wreck, shrinks slightly as it
    // docks onto the skimmer's spine, and rides there from then on.
    if (boss->phase >= BOSS_PHASE_SALVAGE_DOCK) {
        float u = boss->phase == BOSS_PHASE_SALVAGE_DOCK
            ? boss->phaseTimer / BOSS_SALVAGE_DOCK_DURATION : 1.0f;
        if (u > 1.0f) u = 1.0f;
        float scale = 1.0f - 0.45f * u;
        Vector2 domePos = boss->phase == BOSS_PHASE_CLEARED ? state->player : boss->salvageDomePos;
        DrawTextureEx(
            assets->leviathanMortarTex,
            (Vector2){
                domePos.x - assets->leviathanMortarTex.width / 2.0f * scale,
                domePos.y - assets->leviathanMortarTex.height / 2.0f * scale
            },
            0.0f, scale, WHITE
        );
    }
}

enum { TERRAIN_TILE_SIZE = 32 };
enum {
    TERRAIN_WANG_PROFILE_COUNT = 81,
    TERRAIN_WANG_PHASE_COUNT = 4,
    TERRAIN_WANG_ATLAS_COLUMNS = 72
};

static bool TerrainHasCell(const StageDescriptor *stage, int px, int y) {
    for (int i = 0; i < stage->terrainCount; i++) {
        StageTerrainFootprint footprint = stage->terrain[i];
        if (px >= footprint.px && px < footprint.px + footprint.widthPx
            && y >= footprint.y && y < footprint.y + footprint.heightPx) return true;
    }
    return false;
}

static bool TerrainVisualVertexLand(const StageDescriptor *stage, int px, int y) {
    // Majority-filter the four 16px collision cells around a 32px visual-grid
    // vertex. This preserves authored bays and hardpoint islands while
    // removing the one-cell cog teeth visible in the first Wang prototype.
    int land = 0;
    if (TerrainHasCell(stage, px - 16, y - 16)) land++;
    if (TerrainHasCell(stage, px, y - 16)) land++;
    if (TerrainHasCell(stage, px - 16, y)) land++;
    if (TerrainHasCell(stage, px, y)) land++;
    return land >= 2;
}

static float TerrainInterfaceLatticeValue(int gridX, int gridY, unsigned int salt) {
    unsigned int value = (unsigned int)gridX * 0x9e3779b9u
        + (unsigned int)gridY * 0x85ebca6bu + salt;
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    return (float)(value & 0xffffu) / 32767.5f - 1.0f;
}

static float TerrainSmoothInterfaceNoise(float x, float y, unsigned int salt) {
    int x0 = (int)floorf(x);
    int y0 = (int)floorf(y);
    float tx = x - (float)x0;
    float ty = y - (float)y0;
    tx = tx * tx * (3.0f - 2.0f * tx);
    ty = ty * ty * (3.0f - 2.0f * ty);
    float topLeft = TerrainInterfaceLatticeValue(x0, y0, salt);
    float top = topLeft
        + (TerrainInterfaceLatticeValue(x0 + 1, y0, salt) - topLeft) * tx;
    float bottomLeft = TerrainInterfaceLatticeValue(x0, y0 + 1, salt);
    float bottom = bottomLeft
        + (TerrainInterfaceLatticeValue(x0 + 1, y0 + 1, salt) - bottomLeft) * tx;
    return top + (bottom - top) * ty;
}

static unsigned int TerrainEdgeInterfaceLevel(int gridX, int gridY, bool vertical) {
    // A shared world-grid edge still owns its interface, but sample a smooth
    // field instead of hashing every edge independently. Crossing positions
    // therefore drift over several tiles instead of alternating abruptly
    // between opposite extremes and creating star-shaped coastlines.
    float noise = TerrainSmoothInterfaceNoise(
        (float)gridX * 0.28f + (vertical ? 7.25f : 0.0f),
        (float)gridY * 0.28f + (vertical ? 3.75f : 0.0f),
        vertical ? 0xc2b2ae35u : 0x27d4eb2fu);
    if (noise < -0.24f) return 0u;
    if (noise > 0.24f) return 2u;
    return 1u;
}

// Select one 32px Wang tile from four shared corner states plus explicit
// quarter/centre/three-quarter crossings on N/E/S/W. The atlas connects those
// edge interfaces with a smooth pixel-field curve across the whole tile.
static void DrawStandaloneTerrain(const GameState *state, const GameAssets *assets) {
    const StageDescriptor *stage = GetStageDescriptor(state->stageNumber);
    int firstPx = (int)floorf((state->scrollDistance - GAME_WIDTH)
        / TERRAIN_TILE_SIZE) * TERRAIN_TILE_SIZE;
    int lastPx = (int)ceilf(state->scrollDistance / TERRAIN_TILE_SIZE)
        * TERRAIN_TILE_SIZE;
    for (int y = -TERRAIN_TILE_SIZE; y < PLAY_HEIGHT; y += TERRAIN_TILE_SIZE) {
        for (int px = firstPx; px <= lastPx; px += TERRAIN_TILE_SIZE) {
            int mask = 0;
            if (TerrainVisualVertexLand(stage, px, y)) mask |= 1;
            if (TerrainVisualVertexLand(stage, px + TERRAIN_TILE_SIZE, y)) mask |= 2;
            if (TerrainVisualVertexLand(stage, px + TERRAIN_TILE_SIZE,
                    y + TERRAIN_TILE_SIZE)) mask |= 4;
            if (TerrainVisualVertexLand(stage, px, y + TERRAIN_TILE_SIZE)) mask |= 8;
            if (mask == 0) continue;

            int gridX = px / TERRAIN_TILE_SIZE;
            int gridY = y / TERRAIN_TILE_SIZE;
            unsigned int north = TerrainEdgeInterfaceLevel(gridX, gridY, false);
            unsigned int east = TerrainEdgeInterfaceLevel(gridX + 1, gridY, true);
            unsigned int south = TerrainEdgeInterfaceLevel(gridX, gridY + 1, false);
            unsigned int west = TerrainEdgeInterfaceLevel(gridX, gridY, true);
            int profile = (int)(north * 27u + east * 9u + south * 3u + west);
            int phaseX = gridX & 1;
            int phaseY = gridY & 1;
            int tileIndex = (mask * TERRAIN_WANG_PROFILE_COUNT + profile)
                * TERRAIN_WANG_PHASE_COUNT + phaseY * 2 + phaseX;
            Rectangle source = {
                (float)(tileIndex % TERRAIN_WANG_ATLAS_COLUMNS * TERRAIN_TILE_SIZE),
                (float)(tileIndex / TERRAIN_WANG_ATLAS_COLUMNS * TERRAIN_TILE_SIZE),
                TERRAIN_TILE_SIZE, TERRAIN_TILE_SIZE
            };
            float screenX = (float)px - state->scrollDistance + GAME_WIDTH;
            DrawTextureRec(assets->terrainTileAtlasTex, source,
                (Vector2){ screenX, (float)y }, WHITE);
        }
    }
}

// Coast tiles deliberately keep a quiet shared ground. One transparent 128px
// metatile then crosses that grid around each Stage 2 installation. Every art
// variant has a clear centre, so the hardpoint remains a readable mounting pad.
static void DrawTerrainFeatures(const GameState *state, const GameAssets *assets) {
    if (state->stageNumber != 2) return;
    const StageDescriptor *stage = GetStageDescriptor(state->stageNumber);
    for (int i = 0; i < stage->hardpointCount; i++) {
        StageTerrainHardpoint hardpoint = stage->hardpoints[i];
        float screenX = (float)hardpoint.px - state->scrollDistance
            + GAME_WIDTH - 48.0f;
        if (screenX > GAME_WIDTH || screenX + 128.0f < 0.0f) continue;
        unsigned int seed = (unsigned int)hardpoint.px * 0x9e3779b9u
            + (unsigned int)hardpoint.y * 0x85ebca6bu;
        seed ^= seed >> 16;
        seed *= 0x7feb352du;
        seed ^= seed >> 15;
        unsigned int family = (seed >> 3) & 1u;
        unsigned int variant = seed & 3u;
        Rectangle source = {
            (float)((family * 4u + variant) * 128u), 0.0f, 128.0f, 128.0f
        };
        DrawTextureRec(assets->terrainFeatureAtlasTex, source,
            (Vector2){ screenX, (float)hardpoint.y - 48.0f }, WHITE);

        float centreX = screenX + 64.0f;
        float centreY = (float)hardpoint.y + 16.0f;
        float directionX = (seed & 0x10u) ? -1.0f : 1.0f;
        float directionY = (seed & 0x20u) ? -1.0f : 1.0f;
        if (family == 1u) {
            DrawTexture(assets->terrainBrushTex,
                (int)(centreX + directionX * 42.0f
                    - assets->terrainBrushTex.width / 2.0f),
                (int)(centreY + directionY * 18.0f
                    - assets->terrainBrushTex.height / 2.0f), WHITE);
        }
        if (((seed >> 6) & 3u) == 0u) {
            DrawTexture(assets->terrainTidePoolTex,
                (int)(centreX - directionX * 40.0f
                    - assets->terrainTidePoolTex.width / 2.0f),
                (int)(centreY - directionY * 18.0f
                    - assets->terrainTidePoolTex.height / 2.0f), WHITE);
        }
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
    Rectangle bossBlockers[2];
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
    DrawTerrainFeatures(state, assets);
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

    if (state->torpedoImpactTimer > 0.0f) {
        float life = state->torpedoImpactTimer / (state->torpedoImpactType == TORPEDO_IMPACT_EXPLOSION ? 0.18f : 0.10f);
        if (state->torpedoImpactType == TORPEDO_IMPACT_EXPLOSION) {
            DrawCircleLines((int)state->torpedoImpactPos.x, (int)state->torpedoImpactPos.y,
                TORPEDO_SPLASH_RADIUS * (1.0f + 0.25f * (1.0f - life)),
                (Color){ 255, 220, 140, (unsigned char)(220.0f * life) });
            DrawCircleV(state->torpedoImpactPos, 5.0f, (Color){ 255, 246, 216, (unsigned char)(180.0f * life) });
        } else {
            DrawCircleV(state->torpedoImpactPos, 3.0f, (Color){ 207, 239, 240, (unsigned char)(160.0f * life) });
        }
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

    if (state->gameOver) {
        DrawRectangle(0, 0, GAME_WIDTH, GAME_HEIGHT, (Color){ 0, 0, 0, 140 });
        DrawText("GAME OVER", 162, 150, 24, (Color){ 232, 248, 248, 255 });
        DrawText("PRESS R TO RESTART", 132, 184, 12, (Color){ 76, 224, 232, 255 });
    } else if (state->stageClear) {
        // Lighter dim than game over: the wreck and the docked mortar
        // stay readable behind the stage-clear card.
        DrawRectangle(0, 0, GAME_WIDTH, GAME_HEIGHT, (Color){ 0, 0, 0, 110 });
        DrawText(TextFormat("STAGE %d CLEAR", state->stageNumber), 120, 140, 24,
            (Color){ 232, 248, 248, 255 });
        DrawText("MORTAR TURRET SALVAGED", 152, 176, 12, (Color){ 232, 148, 44, 255 });
        DrawText("PRESS R TO CONTINUE", 164, 200, 12, (Color){ 76, 224, 232, 255 });
    } else if (state->respawnInvulnerability > 0.0f) {
        unsigned char blink = (unsigned char)(120 + 80 * sinf(GetTime() * 22.0f));
        DrawText("RESPAWNING", 180, 150, 12, (Color){ 76, 224, 232, blink });
    }

    // The paused overlay is the pause menu's, drawn by DrawPauseMenu on
    // top of this frame (main.c owns the menu; DrawGame stays UI-unaware).

    DrawHud(state);
}
