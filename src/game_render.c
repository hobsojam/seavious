#include "game_render.h"
#include "boss.h"
#include "stage_data.h"
#include "raylib.h"
#include "rlgl.h"
#include <math.h>

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

    DrawRectangle(244, top + 14, 14, 6, torpedoStatusColor);
    DrawTriangle(
        (Vector2){ 263.0f, (float)top + 17.0f },
        (Vector2){ 258.0f, (float)top + 14.0f },
        (Vector2){ 258.0f, (float)top + 20.0f },
        torpedoStatusColor
    );
    DrawText(torpedoStatus, 270, top + 12, 10, torpedoStatusColor);
    DrawRectangle(244, top + 25, 126, 3, panelInset);
    DrawRectangle(244, top + 25, (int)(126.0f * reloadProgress), 3, torpedoStatusColor);

    // Reserved boss region: a labeled health meter while the boss lives
    // (sum of remaining destructible-part HP), the empty inset otherwise.
    DrawRectangle(389, top + 12, 115, 8, panelInset);
    DrawRectangleLines(389, top + 12, 115, 8, (Color){ 74, 94, 102, 90 });
    if (state->boss.phase == BOSS_PHASE_ENTERING || state->boss.phase == BOSS_PHASE_FIGHTING) {
        DrawText("LEVIATHAN", 389, top + 4, 8, (Color){ 232, 72, 72, 255 });
        int fill = (int)(115.0f * (float)BossRemainingHp(&state->boss) / (float)BossTotalHp());
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

static bool TerrainFootprintsTouch(StageTerrainFootprint first, StageTerrainFootprint second) {
    const float firstRight = first.px + first.widthPx;
    const float firstBottom = first.y + first.heightPx;
    const float secondRight = second.px + second.widthPx;
    const float secondBottom = second.y + second.heightPx;
    return first.px <= secondRight && firstRight >= second.px
        && first.y <= secondBottom && firstBottom >= second.y;
}

// Collision retains the authored rectangular cells. Rendering merges every
// touching group, so one transparent islet sprite replaces the visibly
// stacked shore rings while targets still use the same stage coordinates.
static void DrawStandaloneTerrain(const GameState *state, const GameAssets *assets) {
    enum { MAX_STAGE1_TERRAIN = 32 };
    if (STAGE1_TERRAIN_COUNT > MAX_STAGE1_TERRAIN) return;

    bool visited[MAX_STAGE1_TERRAIN] = { false };

    for (int start = 0; start < STAGE1_TERRAIN_COUNT; start++) {
        if (visited[start]) continue;

        int pending[MAX_STAGE1_TERRAIN];
        int next = 0;
        int pendingCount = 1;
        pending[0] = start;
        float minX = 100000.0f, minY = 100000.0f, maxX = -100000.0f, maxY = -100000.0f;
        visited[start] = true;

        while (next < pendingCount) {
            int current = pending[next++];
            Rectangle currentRect = TerrainScreenRect(STAGE1_TERRAIN[current], state->scrollDistance);
            if (currentRect.x < minX) minX = currentRect.x;
            if (currentRect.y < minY) minY = currentRect.y;
            if (currentRect.x + currentRect.width > maxX) maxX = currentRect.x + currentRect.width;
            if (currentRect.y + currentRect.height > maxY) maxY = currentRect.y + currentRect.height;

            for (int candidate = 0; candidate < STAGE1_TERRAIN_COUNT; candidate++) {
                if (!visited[candidate] && TerrainFootprintsTouch(STAGE1_TERRAIN[current], STAGE1_TERRAIN[candidate])) {
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
        unsigned int variantSeed = (unsigned int)STAGE1_TERRAIN[start].px * 17u
            ^ (unsigned int)STAGE1_TERRAIN[start].y * 31u ^ (unsigned int)start;
        Texture2D islet = assets->stage1IsletTex[variantSeed % STAGE1_ISLET_VARIANT_COUNT];
        Rectangle source = { 0.0f, 0.0f, (float)islet.width, (float)islet.height };
        DrawTexturePro(islet, source, destination, (Vector2){ 0 }, 0.0f, WHITE);
    }
}

static void DrawTerrainHardpoints(const GameState *state, const GameAssets *assets) {
    Rectangle source = { 0.0f, 0.0f, (float)assets->terrainHardpointTex.width,
        (float)assets->terrainHardpointTex.height };
    for (int i = 0; i < STAGE1_TERRAIN_HARDPOINT_COUNT; i++) {
        StageTerrainHardpoint hardpoint = STAGE1_TERRAIN_HARDPOINTS[i];
        Rectangle cell = TerrainScreenRect((StageTerrainFootprint){
            hardpoint.px, hardpoint.y, 32, 32
        }, state->scrollDistance);
        if (cell.x > GAME_WIDTH || cell.x + cell.width < 0.0f) continue;
        Rectangle pad = { cell.x + 4.0f, cell.y + 4.0f, 24.0f, 24.0f };
        DrawTexturePro(assets->terrainHardpointTex, source, pad, (Vector2){ 0 }, 0.0f, WHITE);
    }
}

enum { TERRAIN_CELL_SIZE = 32 };

static bool TerrainCellOccupied(int px, int y) {
    for (int i = 0; i < STAGE1_TERRAIN_COUNT; i++) {
        StageTerrainFootprint footprint = STAGE1_TERRAIN[i];
        if (px >= footprint.px && px < footprint.px + footprint.widthPx
            && y >= footprint.y && y < footprint.y + footprint.heightPx) return true;
    }
    return false;
}

static bool TerrainCellHasHardpoint(int px, int y) {
    for (int i = 0; i < STAGE1_TERRAIN_HARDPOINT_COUNT; i++) {
        if (STAGE1_TERRAIN_HARDPOINTS[i].px == px && STAGE1_TERRAIN_HARDPOINTS[i].y == y) return true;
    }
    return false;
}

static void DrawTerrainShoreEdge(Texture2D shore, Rectangle cell, int direction) {
    Rectangle source = { 0.0f, 0.0f, (float)shore.width, (float)shore.height };
    Rectangle destination = { cell.x + cell.width * 0.5f, cell.y + 3.0f, cell.width, 6.0f };
    Vector2 origin = { cell.width * 0.5f, 3.0f };
    float rotation = 0.0f;
    if (direction == 1) { destination.y = cell.y + cell.height - 3.0f; rotation = 180.0f; }
    if (direction == 2) { destination.x = cell.x + 3.0f; destination.y = cell.y + cell.height * 0.5f; rotation = -90.0f; }
    if (direction == 3) { destination.x = cell.x + cell.width - 3.0f; destination.y = cell.y + cell.height * 0.5f; rotation = 90.0f; }
    DrawTexturePro(shore, source, destination, origin, rotation, WHITE);
}

// The stage keeps its collision rectangles, but each occupied 32px cell now
// receives terrain material and only exposed neighbours receive shoreline.
// This is the base of an autotile system: large land masses and small islets
// use one representation, while hardpoints are authored land features.
static void DrawTerrain(const GameState *state, const GameAssets *assets) {
    for (int i = 0; i < STAGE1_TERRAIN_COUNT; i++) {
        StageTerrainFootprint footprint = STAGE1_TERRAIN[i];
        for (int y = footprint.y; y < footprint.y + footprint.heightPx; y += TERRAIN_CELL_SIZE) {
            for (int px = footprint.px; px < footprint.px + footprint.widthPx; px += TERRAIN_CELL_SIZE) {
                StageTerrainFootprint cellFootprint = { px, y, TERRAIN_CELL_SIZE, TERRAIN_CELL_SIZE };
                Rectangle cell = TerrainScreenRect(cellFootprint, state->scrollDistance);
                if (cell.x > GAME_WIDTH || cell.x + cell.width < 0.0f) continue;

                unsigned int sample = (unsigned int)(px / TERRAIN_CELL_SIZE) * 17u
                    ^ (unsigned int)(y / TERRAIN_CELL_SIZE) * 29u;
                Rectangle groundSource = {
                    (float)((sample & 3u) * TERRAIN_CELL_SIZE),
                    (float)(((sample >> 2) & 3u) * TERRAIN_CELL_SIZE),
                    TERRAIN_CELL_SIZE, TERRAIN_CELL_SIZE
                };
                DrawTexturePro(assets->terrainGroundTex, groundSource, cell, (Vector2){ 0 }, 0.0f, WHITE);

                if (!TerrainCellOccupied(px, y - TERRAIN_CELL_SIZE)) DrawTerrainShoreEdge(assets->terrainShoreTex, cell, 0);
                if (!TerrainCellOccupied(px, y + TERRAIN_CELL_SIZE)) DrawTerrainShoreEdge(assets->terrainShoreTex, cell, 1);
                if (!TerrainCellOccupied(px - TERRAIN_CELL_SIZE, y)) DrawTerrainShoreEdge(assets->terrainShoreTex, cell, 2);
                if (!TerrainCellOccupied(px + TERRAIN_CELL_SIZE, y)) DrawTerrainShoreEdge(assets->terrainShoreTex, cell, 3);
                if (TerrainCellHasHardpoint(px, y)) DrawTexture(assets->terrainHardpointTex, (int)cell.x, (int)cell.y, WHITE);
            }
        }
    }
}

void DrawGame(const GameState *state, const GameAssets *assets) {
    const Color bulletColor = (Color){ 76, 224, 232, 255 };
    const Color enemyBulletColor = (Color){ 232, 72, 72, 255 };
    const Color airTargetColor = (Color){ 216, 72, 192, 255 };
    const Color torpedoColor = (Color){ 232, 248, 248, 255 };

    float halfW = assets->playerTex.width / 2.0f;
    Vector2 torpedoSpawn = { state->player.x + halfW, state->player.y };
    Vector2 torpedoReticle = CalculateTorpedoReticle(
        torpedoSpawn, STAGE1_TERRAIN, STAGE1_TERRAIN_COUNT, state->scrollDistance
    );
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
    DrawTerrainHardpoints(state, assets);

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
        float progress = state->explosions[i].age / state->explosions[i].lifetime;
        float radius = state->explosions[i].radius * (0.45f + 0.75f * progress);
        unsigned char alpha = (unsigned char)(255.0f * (1.0f - progress));
        Color edge = state->explosions[i].type == EXPLOSION_AIR_TARGET
            ? (Color){ 255, 100, 216, alpha }
            : (Color){ 255, 180, 72, alpha };
        if (state->explosions[i].type == EXPLOSION_PLAYER) edge = (Color){ 96, 232, 255, alpha };
        DrawCircleLines((int)state->explosions[i].pos.x, (int)state->explosions[i].pos.y, radius, edge);
        DrawCircleV(state->explosions[i].pos, radius * 0.35f, (Color){ 255, 242, 196, alpha });
    }

    // Mortar shells at their arc apex are the highest thing in the scene,
    // and the docking dome flies over the player: top of the world layer.
    DrawBossAirborne(state, assets);

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
        DrawText("STAGE 1 CLEAR", 120, 140, 24, (Color){ 232, 248, 248, 255 });
        DrawText("MORTAR TURRET SALVAGED", 152, 176, 12, (Color){ 232, 148, 44, 255 });
        DrawText("PRESS R TO RESTART", 168, 200, 12, (Color){ 76, 224, 232, 255 });
    } else if (state->respawnInvulnerability > 0.0f) {
        unsigned char blink = (unsigned char)(120 + 80 * sinf(GetTime() * 22.0f));
        DrawText("RESPAWNING", 180, 150, 12, (Color){ 76, 224, 232, blink });
    }

    if (state->paused) {
        DrawRectangle(0, 0, GAME_WIDTH, PLAY_HEIGHT, (Color){ 0, 0, 0, 115 });
        DrawText("PAUSED", 220, 150, 18, (Color){ 232, 248, 248, 255 });
        DrawText("PRESS P TO RESUME", 185, 176, 10, (Color){ 76, 224, 232, 255 });
    }

    DrawHud(state);
}
