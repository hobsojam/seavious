#include "boss.h"

#include <math.h>

// Part layout on the 200x120 hull sprite (content spans x 6..196,
// y 24..96; the breach void is centered near 115,60 - the core sits in
// it). Pods ride the spine fore and aft, hull sections sit on the top
// and bottom waterline edges so their straight lane shots threaten two
// different rows, and the mortar dome is aft-center on the spine.
static const struct {
    Vector2 offset;
    float radius;
} BOSS_PART_GEOMETRY[BOSS_PART_COUNT] = {
    [BOSS_PART_POD_FORE]  = { { 56.0f, 60.0f }, 10.0f },
    [BOSS_PART_POD_AFT]   = { { 148.0f, 60.0f }, 10.0f },
    [BOSS_PART_HULL_FORE] = { { 72.0f, 88.0f }, 14.0f },
    [BOSS_PART_HULL_AFT]  = { { 152.0f, 32.0f }, 14.0f },
    [BOSS_PART_CORE]      = { { 115.0f, 60.0f }, 12.0f },
};

static const Vector2 BOSS_MORTAR_OFFSET = { 178.0f, 60.0f };

// Staggered opening shots so the volleys interleave into a stream
// rather than arriving as one wall (the fight-flow design note).
static const float BOSS_PART_FIRST_SHOT_DELAY[BOSS_PART_COUNT] = { 0.8f, 1.8f, 1.4f, 2.6f, 0.0f };
#define BOSS_MORTAR_FIRST_SHOT_DELAY 2.2f

// The death chain walks these hull-relative bursts bow-to-stern-ish,
// ending big at the breach.
#define BOSS_DEATH_EXPLOSION_INTERVAL 0.28f
static const Vector2 BOSS_DEATH_EXPLOSION_OFFSETS[] = {
    { 40.0f, 66.0f }, { 70.0f, 44.0f }, { 96.0f, 78.0f }, { 130.0f, 50.0f },
    { 60.0f, 84.0f }, { 152.0f, 38.0f }, { 174.0f, 64.0f }, { 115.0f, 60.0f },
};
#define BOSS_DEATH_EXPLOSION_COUNT \
    ((int)(sizeof(BOSS_DEATH_EXPLOSION_OFFSETS) / sizeof(BOSS_DEATH_EXPLOSION_OFFSETS[0])))

// Where the skimmer parks for the salvage beat: alongside the wreck,
// just forward of the mortar dome.
static const Vector2 BOSS_SALVAGE_DOCK_OFFSET = { -44.0f, 0.0f };

static float SmoothStep01(float u) {
    if (u < 0.0f) u = 0.0f;
    if (u > 1.0f) u = 1.0f;
    return u * u * (3.0f - 2.0f * u);
}

static Vector2 LerpV(Vector2 a, Vector2 b, float u) {
    return (Vector2){ a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u };
}

static float DistanceSquared(Vector2 a, Vector2 b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

static Vector2 AimAt(Vector2 from, Vector2 to) {
    Vector2 d = { to.x - from.x, to.y - from.y };
    float length = sqrtf(d.x * d.x + d.y * d.y);
    if (length <= 0.0001f) return (Vector2){ -1.0f, 0.0f };
    return (Vector2){ d.x / length, d.y / length };
}

Vector2 BossPartPosition(const BossState *boss, BossPartId part) {
    return (Vector2){
        boss->hullPos.x + BOSS_PART_GEOMETRY[part].offset.x,
        boss->hullPos.y + boss->settleOffset + BOSS_PART_GEOMETRY[part].offset.y
    };
}

Vector2 BossMortarPosition(const BossState *boss) {
    return (Vector2){
        boss->hullPos.x + BOSS_MORTAR_OFFSET.x,
        boss->hullPos.y + boss->settleOffset + BOSS_MORTAR_OFFSET.y
    };
}

bool BossPartAlive(const BossState *boss, BossPartId part) {
    return boss->partHp[part] > 0;
}

int BossRemainingHp(const BossState *boss) {
    return boss->partHp[BOSS_PART_POD_FORE] + boss->partHp[BOSS_PART_POD_AFT]
        + BOSS_HULL_SECTION_TORPEDO_WORTH
            * (boss->partHp[BOSS_PART_HULL_FORE] + boss->partHp[BOSS_PART_HULL_AFT])
        + boss->partHp[BOSS_PART_CORE];
}

int BossTotalHp(void) {
    return 2 * BOSS_POD_HP + 2 * BOSS_HULL_SECTION_TORPEDO_WORTH * BOSS_HULL_SECTION_HP + BOSS_CORE_HP;
}

bool BossSequenceOwnsPlayer(const GameState *state) {
    return state->boss.phase >= BOSS_PHASE_SALVAGE_APPROACH;
}

// Every destroyed part permanently silences its gun; both hull sections
// down reveals the core; the core kill ends the fight.
static bool DamageBossPart(BossState *boss, BossPartId part, int damage, GameEventQueue *events) {
    if (boss->partHp[part] <= 0 || damage <= 0) return false;

    boss->partHp[part] -= damage;
    if (boss->partHp[part] > 0) return false;

    boss->partHp[part] = 0;
    PushGameEvent(events, (GameEvent){
        .type = GAME_EVENT_BOSS_PART_DESTROYED,
        .pos = BossPartPosition(boss, part),
        .target.bossPart = part
    });
    if (!BossPartAlive(boss, BOSS_PART_HULL_FORE) && !BossPartAlive(boss, BOSS_PART_HULL_AFT)) {
        boss->coreExposed = true;
    }
    if (part == BOSS_PART_CORE) {
        boss->phase = BOSS_PHASE_DYING;
        boss->phaseTimer = 0.0f;
        boss->deathExplosionsSpawned = 0;
    }
    return true;
}

static void StartBossFight(GameState *state) {
    BossState *boss = &state->boss;
    boss->phase = BOSS_PHASE_ENTERING;
    boss->phaseTimer = 0.0f;
    // Hull content starts at sprite x 6, so this keeps it fully offscreen.
    boss->hullPos = (Vector2){ (float)GAME_WIDTH + 8.0f, BOSS_HULL_TOP_Y };
    boss->settleOffset = 0.0f;
    boss->coreExposed = false;
    boss->partHp[BOSS_PART_POD_FORE] = BOSS_POD_HP;
    boss->partHp[BOSS_PART_POD_AFT] = BOSS_POD_HP;
    boss->partHp[BOSS_PART_HULL_FORE] = BOSS_HULL_SECTION_HP;
    boss->partHp[BOSS_PART_HULL_AFT] = BOSS_HULL_SECTION_HP;
    boss->partHp[BOSS_PART_CORE] = BOSS_CORE_HP;
    for (int part = 0; part < BOSS_PART_COUNT; part++) {
        float interval = part <= BOSS_PART_POD_AFT ? BOSS_POD_FIRE_INTERVAL : BOSS_HULL_SECTION_FIRE_INTERVAL;
        boss->partFireTimer[part] = interval - BOSS_PART_FIRST_SHOT_DELAY[part];
    }
    boss->mortarTimer = BOSS_MORTAR_INTERVAL - BOSS_MORTAR_FIRST_SHOT_DELAY;
    for (int i = 0; i < MAX_MORTAR_SHELLS; i++) boss->shells[i].active = false;
    boss->deathExplosionsSpawned = 0;
    // The fight owns the music flag from here to the defeat hard-cut.
    state->bossActive = true;
}

static void FireBossGuns(GameState *state, float dt) {
    BossState *boss = &state->boss;
    for (int part = BOSS_PART_POD_FORE; part <= BOSS_PART_HULL_AFT; part++) {
        if (!BossPartAlive(boss, (BossPartId)part)) continue;
        bool pod = part <= BOSS_PART_POD_AFT;
        float interval = pod ? BOSS_POD_FIRE_INTERVAL : BOSS_HULL_SECTION_FIRE_INTERVAL;
        boss->partFireTimer[part] += dt;
        while (boss->partFireTimer[part] >= interval) {
            boss->partFireTimer[part] -= interval;
            Vector2 from = BossPartPosition(boss, (BossPartId)part);
            // Pods fire turret-style aimed shots; hull sections fire
            // straight lane shots, casemate-style.
            Vector2 dir = pod ? AimAt(from, state->player) : (Vector2){ -1.0f, 0.0f };
            float muzzle = BOSS_PART_GEOMETRY[part].radius + 3.0f;
            TrySpawnEnemyBullet(
                state->enemyBullets,
                MAX_ENEMY_BULLETS,
                (Vector2){ from.x + dir.x * muzzle, from.y + dir.y * muzzle },
                (Vector2){ dir.x * ENEMY_BULLET_SPEED, dir.y * ENEMY_BULLET_SPEED }
            );
        }
    }
}

static void FireBossMortar(GameState *state, float dt) {
    BossState *boss = &state->boss;
    // The final push stays under pressure: the cadence quickens once the
    // core is exposed.
    float interval = boss->coreExposed ? BOSS_MORTAR_INTERVAL_CORE_EXPOSED : BOSS_MORTAR_INTERVAL;
    boss->mortarTimer += dt;
    while (boss->mortarTimer >= interval) {
        boss->mortarTimer -= interval;
        for (int i = 0; i < MAX_MORTAR_SHELLS; i++) {
            if (boss->shells[i].active) continue;
            Vector2 target = state->player;
            if (target.x < 0.0f) target.x = 0.0f;
            if (target.x > (float)GAME_WIDTH) target.x = (float)GAME_WIDTH;
            if (target.y < 0.0f) target.y = 0.0f;
            if (target.y > (float)PLAY_HEIGHT) target.y = (float)PLAY_HEIGHT;
            boss->shells[i] = (MortarShell){
                .launch = BossMortarPosition(boss),
                .target = target,
                .t = 0.0f,
                .blastT = 0.0f,
                .landed = false,
                .active = true
            };
            PushGameEvent(&state->gameEvents, (GameEvent){
                .type = GAME_EVENT_MORTAR_FIRED, .pos = boss->shells[i].launch
            });
            break;
        }
    }
}

// Shells advance in every phase: a shell already in the air when the
// core dies still comes down - the mortar only stops lobbing new ones.
static void UpdateMortarShells(BossState *boss, GameEventQueue *events, float dt) {
    for (int i = 0; i < MAX_MORTAR_SHELLS; i++) {
        MortarShell *shell = &boss->shells[i];
        if (!shell->active) continue;
        if (!shell->landed) {
            shell->t += dt;
            if (shell->t >= BOSS_MORTAR_AIR_TIME) {
                shell->landed = true;
                shell->blastT = BOSS_MORTAR_BLAST_DURATION;
                PushGameEvent(events, (GameEvent){
                    .type = GAME_EVENT_MORTAR_BLAST, .pos = shell->target
                });
            }
        } else {
            shell->blastT -= dt;
            if (shell->blastT <= 0.0f) shell->active = false;
        }
    }
}

static void ResolveBulletBossPartCollisions(GameState *state) {
    BossState *boss = &state->boss;
    // Strict class mapping: bullets touch the gun-weak pods, plus the
    // core once exposed. Hull sections, dome, and hull pass under them.
    const BossPartId gunTargets[] = { BOSS_PART_POD_FORE, BOSS_PART_POD_AFT, BOSS_PART_CORE };
    for (int b = 0; b < MAX_BULLETS; b++) {
        if (!state->bullets[b].active) continue;
        for (int i = 0; i < 3; i++) {
            BossPartId part = gunTargets[i];
            if (part == BOSS_PART_CORE && !boss->coreExposed) continue;
            if (!BossPartAlive(boss, part)) continue;
            float hitDist = BULLET_RADIUS + BOSS_PART_GEOMETRY[part].radius;
            if (DistanceSquared(state->bullets[b].pos, BossPartPosition(boss, part)) <= hitDist * hitDist) {
                state->bullets[b].active = false;
                DamageBossPart(boss, part, 1, &state->gameEvents);
                break;
            }
        }
    }
}

TorpedoImpact ResolveTorpedoBossPartCollision(Torpedo *torpedo, BossState *boss, GameEventQueue *events) {
    TorpedoImpact none = { TORPEDO_IMPACT_NONE, (Vector2){ 0.0f, 0.0f } };
    if (!torpedo->active || boss->phase != BOSS_PHASE_FIGHTING) return none;

    const BossPartId torpedoTargets[] = { BOSS_PART_HULL_FORE, BOSS_PART_HULL_AFT, BOSS_PART_CORE };
    for (int i = 0; i < 3; i++) {
        BossPartId part = torpedoTargets[i];
        if (part == BOSS_PART_CORE && !boss->coreExposed) continue;
        if (!BossPartAlive(boss, part)) continue;
        float hitDist = BOSS_PART_GEOMETRY[part].radius + TORPEDO_DIRECT_HIT_RADIUS;
        if (DistanceSquared(torpedo->pos, BossPartPosition(boss, part)) <= hitDist * hitDist) {
            Vector2 impactPos = torpedo->pos;
            torpedo->active = false;
            // Armed hits leave the damage to the splash pass (same
            // no-double-hit rule as the surface targets).
            if (torpedo->armed) return (TorpedoImpact){ TORPEDO_IMPACT_EXPLOSION, impactPos };
            DamageBossPart(boss, part,
                part == BOSS_PART_CORE ? BOSS_CORE_TORPEDO_DAMAGE : 1, events);
            return (TorpedoImpact){ TORPEDO_IMPACT_DIRECT, impactPos };
        }
    }
    return none;
}

void ResolveBossSplashDamage(BossState *boss, Vector2 pos, GameEventQueue *events) {
    if (boss->phase != BOSS_PHASE_FIGHTING) return;

    const BossPartId torpedoTargets[] = { BOSS_PART_HULL_FORE, BOSS_PART_HULL_AFT, BOSS_PART_CORE };
    for (int i = 0; i < 3; i++) {
        BossPartId part = torpedoTargets[i];
        if (part == BOSS_PART_CORE && !boss->coreExposed) continue;
        if (!BossPartAlive(boss, part)) continue;
        float hitDist = BOSS_PART_GEOMETRY[part].radius + TORPEDO_SPLASH_RADIUS;
        if (DistanceSquared(pos, BossPartPosition(boss, part)) <= hitDist * hitDist) {
            DamageBossPart(boss, part,
                part == BOSS_PART_CORE ? BOSS_CORE_TORPEDO_DAMAGE : 1, events);
        }
    }
}

bool ResolveBossContactDamage(const BossState *boss, Vector2 playerPos, float playerRadius) {
    if (boss->phase != BOSS_PHASE_ENTERING && boss->phase != BOSS_PHASE_FIGHTING) return false;

    // Hull content rect inside the 200x120 sprite canvas.
    float left = boss->hullPos.x + 6.0f;
    float top = boss->hullPos.y + 24.0f;
    float right = left + 190.0f;
    float bottom = top + 72.0f;
    float cx = playerPos.x < left ? left : (playerPos.x > right ? right : playerPos.x);
    float cy = playerPos.y < top ? top : (playerPos.y > bottom ? bottom : playerPos.y);
    return DistanceSquared(playerPos, (Vector2){ cx, cy }) <= playerRadius * playerRadius;
}

bool ResolveMortarBlastPlayerHit(const BossState *boss, Vector2 playerPos, float playerRadius) {
    for (int i = 0; i < MAX_MORTAR_SHELLS; i++) {
        const MortarShell *shell = &boss->shells[i];
        if (!shell->active || !shell->landed || shell->blastT <= 0.0f) continue;
        float hitDist = BOSS_MORTAR_BLAST_RADIUS + playerRadius;
        if (DistanceSquared(playerPos, shell->target) <= hitDist * hitDist) return true;
    }
    return false;
}

void UpdateBossFight(GameState *state, float dt) {
    BossState *boss = &state->boss;

    if (boss->phase == BOSS_PHASE_INACTIVE) {
        if (!state->bossLock) return;
        StartBossFight(state);
    }

    boss->phaseTimer += dt;

    switch (boss->phase) {
        case BOSS_PHASE_ENTERING: {
            // Slide in from offscreen right and park; guns stay quiet
            // until parked.
            float u = SmoothStep01(boss->phaseTimer / BOSS_ENTRANCE_DURATION);
            boss->hullPos.x = (float)GAME_WIDTH + 8.0f - ((float)GAME_WIDTH + 8.0f - BOSS_PARKED_X) * u;
            if (boss->phaseTimer >= BOSS_ENTRANCE_DURATION) {
                boss->hullPos.x = BOSS_PARKED_X;
                boss->phase = BOSS_PHASE_FIGHTING;
                boss->phaseTimer = 0.0f;
            }
            break;
        }
        case BOSS_PHASE_FIGHTING:
            FireBossGuns(state, dt);
            FireBossMortar(state, dt);
            ResolveBulletBossPartCollisions(state);
            break;
        case BOSS_PHASE_DYING: {
            // The remaining guns died with the core: their shots vanish
            // into the spectacle, and the chain walks the hull.
            for (int i = 0; i < MAX_ENEMY_BULLETS; i++) state->enemyBullets[i].active = false;
            while (boss->deathExplosionsSpawned < BOSS_DEATH_EXPLOSION_COUNT
                   && boss->phaseTimer >= BOSS_DEATH_EXPLOSION_INTERVAL * (float)(boss->deathExplosionsSpawned + 1)) {
                Vector2 offset = BOSS_DEATH_EXPLOSION_OFFSETS[boss->deathExplosionsSpawned];
                TrySpawnExplosion(
                    state->explosions,
                    (Vector2){ boss->hullPos.x + offset.x, boss->hullPos.y + offset.y },
                    EXPLOSION_SURFACE_TARGET,
                    12.0f + 1.5f * (float)boss->deathExplosionsSpawned,
                    0.5f
                );
                boss->deathExplosionsSpawned++;
            }
            if (boss->phaseTimer >= BOSS_DEATH_DURATION) {
                // Wreck settles, music hard-cuts back to the stage theme,
                // and the salvage autopilot takes the stick.
                boss->settleOffset = 3.0f;
                state->bossActive = false;
                PushGameEvent(&state->gameEvents, (GameEvent){
                    .type = GAME_EVENT_BOSS_DEFEATED,
                    .pos = { boss->hullPos.x + 100.0f, boss->hullPos.y + 60.0f }
                });
                boss->salvageStart = state->player;
                boss->phase = BOSS_PHASE_SALVAGE_APPROACH;
                boss->phaseTimer = 0.0f;
            }
            break;
        }
        case BOSS_PHASE_SALVAGE_APPROACH: {
            Vector2 mortar = BossMortarPosition(boss);
            Vector2 dock = { mortar.x + BOSS_SALVAGE_DOCK_OFFSET.x, mortar.y + BOSS_SALVAGE_DOCK_OFFSET.y };
            float u = SmoothStep01(boss->phaseTimer / BOSS_SALVAGE_APPROACH_DURATION);
            state->player = LerpV(boss->salvageStart, dock, u);
            if (boss->phaseTimer >= BOSS_SALVAGE_APPROACH_DURATION) {
                boss->salvageDomePos = mortar;
                boss->phase = BOSS_PHASE_SALVAGE_DOCK;
                boss->phaseTimer = 0.0f;
            }
            break;
        }
        case BOSS_PHASE_SALVAGE_DOCK: {
            // The dome lifts off the wreck and docks onto the spine.
            float u = SmoothStep01(boss->phaseTimer / BOSS_SALVAGE_DOCK_DURATION);
            boss->salvageDomePos = LerpV(BossMortarPosition(boss), state->player, u);
            if (boss->phaseTimer >= BOSS_SALVAGE_DOCK_DURATION) {
                boss->salvageDomePos = state->player;
                PushGameEvent(&state->gameEvents, (GameEvent){
                    .type = GAME_EVENT_MORTAR_SALVAGED, .pos = state->player
                });
                boss->phase = BOSS_PHASE_CLEARED;
                boss->phaseTimer = 0.0f;
                state->stageClear = true;
            }
            break;
        }
        default:
            break;
    }

    UpdateMortarShells(boss, &state->gameEvents, dt);
}
