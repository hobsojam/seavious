#include "boss.h"
#include "stage.h"

#include <math.h>

// Part layout in hull sprite space, relative to the content center (the
// 200x120 sprite's content spans x 6..196, y 24..96, center 100,60; the
// breach void sits near 115,60 - the core lives in it). Offsets rotate
// with the ship: pods and the mortar dome ride the centerline spine, and
// the two hull sections sit on opposite broadsides (sprite bottom/top),
// so with the ship sailing vertically one section faces the player and
// the armor shields the other until the ship turns.
static const struct {
    Vector2 offset;
    float radius;
} BOSS_PART_GEOMETRY[BOSS_PART_COUNT] = {
    [BOSS_PART_POD_FORE]  = { { -44.0f, 0.0f }, 10.0f },
    [BOSS_PART_POD_AFT]   = { { 48.0f, 0.0f }, 10.0f },
    [BOSS_PART_HULL_FORE] = { { -28.0f, 28.0f }, 14.0f },
    [BOSS_PART_HULL_AFT]  = { { 52.0f, -28.0f }, 14.0f },
    [BOSS_PART_CORE]      = { { 15.0f, 0.0f }, 12.0f },
};

// The Fortress Atoll uses the same five gameplay parts as the Leviathan,
// but they sit on its four literal diagonal pads.  This leaves the west
// channel free as the readable torpedo approach to the harbour aperture.
static const struct {
    Vector2 offset;
    float radius;
} FORTRESS_PART_GEOMETRY[BOSS_PART_COUNT] = {
    [BOSS_PART_POD_FORE]  = { { 26.0f, -27.0f }, 12.0f },
    [BOSS_PART_POD_AFT]   = { { -27.0f, 26.0f }, 12.0f },
    [BOSS_PART_HULL_FORE] = { { -27.0f, -27.0f }, 15.0f },
    [BOSS_PART_HULL_AFT]  = { { 26.0f, 26.0f }, 15.0f },
    [BOSS_PART_CORE]      = { { 0.0f, 0.0f }, 18.0f },
};

static const Vector2 BOSS_MORTAR_OFFSET = { 78.0f, 0.0f };

// A gate opening has a concrete purpose: one patrol craft sorties through
// the harbour channel.  It is not ordinary scrolling water traffic, so it
// keeps moving while the boss lock freezes the stage behind it.
static bool LaunchFortressSortie(GameState *state) {
    for (int i = 0; i < MAX_SURFACE_TARGETS; i++) {
        SurfaceTarget *boat = &state->surfaceTargets[i];
        if (boat->active) continue;
        Vector2 core = BossPartPosition(&state->boss, BOSS_PART_CORE);
        *boat = (SurfaceTarget){
            .type = SURFACE_TARGET_MOBILE_PLATFORM,
            .pos = { core.x - 38.0f, core.y },
            .radius = MOBILE_PLATFORM_RADIUS,
            .hp = MOBILE_PLATFORM_HP,
            .aimDirection = { -1.0f, 0.0f },
            .fortressSortie = true,
            .active = true,
        };
        return true;
    }
    return false;
}

static void UpdateFortressSorties(GameState *state, float dt) {
    for (int i = 0; i < MAX_SURFACE_TARGETS; i++) {
        SurfaceTarget *boat = &state->surfaceTargets[i];
        if (!boat->active || !boat->fortressSortie) continue;
        boat->pos.x -= MOBILE_PLATFORM_SPEED * dt;
        if (boat->pos.x < -boat->radius) boat->active = false;
    }
}

// Hull content half-extents in sprite space: 190 long (bow-stern), 72
// wide. Sailing vertically the armor blocker is 72 wide by 190 tall.
#define BOSS_HULL_HALF_LENGTH 95.0f
#define BOSS_HULL_HALF_WIDTH 36.0f
// The breach gap that opens in the armor once the core is exposed.
#define BOSS_BREACH_HALF_GAP 20.0f

// Staggered opening shots so the volleys interleave into a stream
// rather than arriving as one wall (the fight-flow design note).
static const float BOSS_PART_FIRST_SHOT_DELAY[BOSS_PART_COUNT] = { 0.8f, 1.8f, 1.4f, 2.6f, 0.0f };
#define BOSS_MORTAR_FIRST_SHOT_DELAY 2.2f

// The death chain walks these center-relative bursts bow-to-stern-ish
// (rotated with the ship's final heading), ending big at the breach.
#define BOSS_DEATH_EXPLOSION_INTERVAL 0.28f
static const Vector2 BOSS_DEATH_EXPLOSION_OFFSETS[] = {
    { -60.0f, 6.0f }, { -30.0f, -16.0f }, { -4.0f, 18.0f }, { 30.0f, -10.0f },
    { -40.0f, 24.0f }, { 52.0f, -22.0f }, { 74.0f, 4.0f }, { 15.0f, 0.0f },
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

// Screen-clockwise rotation (raylib's convention with y-down).
static Vector2 RotateOffset(Vector2 v, float degrees) {
    float r = degrees * DEG2RAD;
    float c = cosf(r);
    float s = sinf(r);
    return (Vector2){ v.x * c - v.y * s, v.x * s + v.y * c };
}

static Vector2 BossOffsetPosition(const BossState *boss, Vector2 offset) {
    Vector2 rotated = RotateOffset(offset, boss->rotation);
    return (Vector2){
        boss->hullCenter.x + rotated.x,
        boss->hullCenter.y + boss->settleOffset + rotated.y
    };
}

Vector2 BossPartPosition(const BossState *boss, BossPartId part) {
    if (boss->type == BOSS_TYPE_FORTRESS_ATOLL) {
        Vector2 offset = FORTRESS_PART_GEOMETRY[part].offset;
        return (Vector2){ boss->hullCenter.x + offset.x,
            boss->hullCenter.y + boss->settleOffset + offset.y };
    }
    return BossOffsetPosition(boss, BOSS_PART_GEOMETRY[part].offset);
}

static float BossPartRadius(const BossState *boss, BossPartId part) {
    return boss->type == BOSS_TYPE_FORTRESS_ATOLL ? FORTRESS_PART_GEOMETRY[part].radius
        : BOSS_PART_GEOMETRY[part].radius;
}

Vector2 BossMortarPosition(const BossState *boss) {
    if (boss->type == BOSS_TYPE_FORTRESS_ATOLL) {
        return (Vector2){ boss->hullCenter.x, boss->hullCenter.y + boss->settleOffset };
    }
    return BossOffsetPosition(boss, BOSS_MORTAR_OFFSET);
}

// A broadside section is only a real target while it faces the player;
// on the far side the armor shields it (and it holds fire).
bool BossSectionFacesPlayer(const BossState *boss, BossPartId part) {
    return BossPartPosition(boss, part).x < boss->hullCenter.x - 2.0f;
}

// The armored hull blocks torpedoes under the same rules as land. One
// solid blocker while the armor holds; once the core is exposed the
// breach splits it in two, opening the single torpedo lane to the core.
// Empty from the death chain on - the settled wreck is inert like every
// wreck. The blocker stays the sailing footprint during the brief
// in-place turn (the sweeping bow/stern are an accepted approximation).
int BossHullBlockers(const BossState *boss, Rectangle out[3]) {
    if (boss->phase != BOSS_PHASE_ENTERING && boss->phase != BOSS_PHASE_FIGHTING) return 0;

    // The fortress's sea gates close over the only torpedo route to the
    // core. They cycle for the whole fight (the player learns the rhythm
    // while the outer defenses still stand), and closed gates always
    // block - what changes at core exposure is only that the core rises
    // into the target list behind them.
    if (boss->type == BOSS_TYPE_FORTRESS_ATOLL) {
        Vector2 core = BossPartPosition(boss, BOSS_PART_CORE);
        // The painted atoll is real torpedo-blocking land.  Only its
        // horizontal west channel remains navigable, so the diagonal
        // defenses cannot be bypassed with a level torpedo shot.
        out[0] = (Rectangle){ core.x - 96.0f, core.y - 96.0f, 192.0f, 78.0f };
        out[1] = (Rectangle){ core.x - 96.0f, core.y + 18.0f, 192.0f, 78.0f };
        if (boss->gatesOpen) return 2;
        // A narrow, visible sluice gate sits across the west-channel mouth;
        // it no longer disguises the whole harbour as a solid rectangle.
        out[2] = (Rectangle){ core.x - 30.0f, core.y - 13.0f, 7.0f, 26.0f };
        return 3;
    }

    float left = boss->hullCenter.x - BOSS_HULL_HALF_WIDTH;
    float top = boss->hullCenter.y - BOSS_HULL_HALF_LENGTH;
    float bottom = boss->hullCenter.y + BOSS_HULL_HALF_LENGTH;
    if (!boss->coreExposed) {
        out[0] = (Rectangle){ left, top, 2.0f * BOSS_HULL_HALF_WIDTH, bottom - top };
        return 1;
    }
    float coreY = BossPartPosition(boss, BOSS_PART_CORE).y;
    out[0] = (Rectangle){ left, top, 2.0f * BOSS_HULL_HALF_WIDTH, (coreY - BOSS_BREACH_HALF_GAP) - top };
    out[1] = (Rectangle){ left, coreY + BOSS_BREACH_HALF_GAP,
        2.0f * BOSS_HULL_HALF_WIDTH, bottom - (coreY + BOSS_BREACH_HALF_GAP) };
    return 2;
}

bool BossPartAlive(const BossState *boss, BossPartId part) {
    return boss->partHp[part] > 0;
}

bool BossIsFortressAtoll(const BossState *boss) {
    return boss->type == BOSS_TYPE_FORTRESS_ATOLL;
}

int BossRemainingHp(const BossState *boss) {
    // Leviathan hull sections are worth several gun-units per torpedo
    // point; fortress ring batteries count their mortar blasts 1:1, so
    // the health bar tracks real progress on either boss.
    int hullWorth = boss->type == BOSS_TYPE_FORTRESS_ATOLL ? 1 : BOSS_HULL_SECTION_TORPEDO_WORTH;
    return boss->partHp[BOSS_PART_POD_FORE] + boss->partHp[BOSS_PART_POD_AFT]
        + hullWorth * (boss->partHp[BOSS_PART_HULL_FORE] + boss->partHp[BOSS_PART_HULL_AFT])
        + boss->partHp[BOSS_PART_CORE];
}

int BossTotalHp(const BossState *boss) {
    if (boss->type == BOSS_TYPE_FORTRESS_ATOLL) {
        return 2 * FORTRESS_POD_HP + 2 * FORTRESS_RING_BATTERY_HP + FORTRESS_CORE_HP;
    }
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
    bool outerDestroyed = !BossPartAlive(boss, BOSS_PART_HULL_FORE)
        && !BossPartAlive(boss, BOSS_PART_HULL_AFT);
    if (boss->type == BOSS_TYPE_FORTRESS_ATOLL) {
        outerDestroyed = outerDestroyed && !BossPartAlive(boss, BOSS_PART_POD_FORE)
            && !BossPartAlive(boss, BOSS_PART_POD_AFT);
    }
    if (outerDestroyed) {
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
    // Sails into view from below the arena, bow up, already on its
    // patrol column.
    boss->type = state->stageNumber == 2 ? BOSS_TYPE_FORTRESS_ATOLL : BOSS_TYPE_LEVIATHAN;
    boss->gatesOpen = false;
    boss->gateTimer = 0.0f;
    boss->fortressGateOpenCount = 0;
    boss->hullCenter = boss->type == BOSS_TYPE_FORTRESS_ATOLL
        ? (Vector2){ 404.0f, 176.0f }
        : (Vector2){ BOSS_PATROL_X, (float)PLAY_HEIGHT + BOSS_HULL_HALF_LENGTH + 15.0f };
    boss->rotation = 90.0f;
    boss->sailDirection = -1;
    boss->turning = false;
    boss->turnTimer = 0.0f;
    boss->settleOffset = 0.0f;
    boss->coreExposed = false;
    boss->partHp[BOSS_PART_POD_FORE] = boss->type == BOSS_TYPE_FORTRESS_ATOLL ? FORTRESS_POD_HP : BOSS_POD_HP;
    boss->partHp[BOSS_PART_POD_AFT] = boss->type == BOSS_TYPE_FORTRESS_ATOLL ? FORTRESS_POD_HP : BOSS_POD_HP;
    boss->partHp[BOSS_PART_HULL_FORE] =
        boss->type == BOSS_TYPE_FORTRESS_ATOLL ? FORTRESS_RING_BATTERY_HP : BOSS_HULL_SECTION_HP;
    boss->partHp[BOSS_PART_HULL_AFT] =
        boss->type == BOSS_TYPE_FORTRESS_ATOLL ? FORTRESS_RING_BATTERY_HP : BOSS_HULL_SECTION_HP;
    boss->partHp[BOSS_PART_CORE] = boss->type == BOSS_TYPE_FORTRESS_ATOLL ? FORTRESS_CORE_HP : BOSS_CORE_HP;
    for (int part = 0; part < BOSS_PART_COUNT; part++) {
        float interval = part <= BOSS_PART_POD_AFT ? BOSS_POD_FIRE_INTERVAL
            : (boss->type == BOSS_TYPE_FORTRESS_ATOLL ? FORTRESS_RING_MORTAR_INTERVAL : BOSS_SAM_INTERVAL);
        boss->partFireTimer[part] = interval - BOSS_PART_FIRST_SHOT_DELAY[part];
    }
    boss->mortarTimer = BOSS_MORTAR_INTERVAL - BOSS_MORTAR_FIRST_SHOT_DELAY;
    for (int i = 0; i < MAX_MORTAR_SHELLS; i++) boss->shells[i].active = false;
    boss->deathExplosionsSpawned = 0;
    // The fight owns the music flag from here to the defeat hard-cut.
    state->bossActive = true;
}

// Returns whether a cell was free to launch, so the door flash and the
// launch sound only ever happen together.
static bool LaunchBossMissile(GameState *state, Vector2 from) {
    BossState *boss = &state->boss;
    for (int i = 0; i < MAX_BOSS_MISSILES; i++) {
        if (boss->missiles[i].active) continue;
        boss->missiles[i] = (BossMissile){
            .pos = from,
            .vel = { -BOSS_MISSILE_SPEED, 0.0f },
            .age = 0.0f,
            .active = true
        };
        TrySpawnExplosion(state->explosions, from, EXPLOSION_SURFACE_TARGET, 5.0f, 0.15f);
        return true;
    }
    return false;
}

// Missiles steer toward the player at a capped turn rate, so a hard
// juke beats them; equal-ish speed means a straight run merely outlasts
// their fuel. They keep flying in every phase (the death chain clears
// them with the rest of the enemy fire).
static void UpdateBossMissiles(GameState *state, float dt) {
    BossState *boss = &state->boss;
    for (int i = 0; i < MAX_BOSS_MISSILES; i++) {
        BossMissile *missile = &boss->missiles[i];
        if (!missile->active) continue;

        missile->age += dt;
        if (missile->age >= BOSS_MISSILE_LIFETIME) {
            // Out of fuel: fizzles with a small pop, no hazard.
            missile->active = false;
            TrySpawnExplosion(state->explosions, missile->pos, EXPLOSION_AIR_TARGET, 5.0f, 0.18f);
            continue;
        }

        float heading = atan2f(missile->vel.y, missile->vel.x);
        float desired = atan2f(state->player.y - missile->pos.y, state->player.x - missile->pos.x);
        float turn = desired - heading;
        while (turn > PI) turn -= 2.0f * PI;
        while (turn < -PI) turn += 2.0f * PI;
        float maxTurn = BOSS_MISSILE_TURN_RATE * DEG2RAD * dt;
        if (turn > maxTurn) turn = maxTurn;
        if (turn < -maxTurn) turn = -maxTurn;
        heading += turn;
        missile->vel = (Vector2){ cosf(heading) * BOSS_MISSILE_SPEED, sinf(heading) * BOSS_MISSILE_SPEED };
        missile->pos.x += missile->vel.x * dt;
        missile->pos.y += missile->vel.y * dt;

        if (missile->pos.x < -24.0f || missile->pos.x > (float)GAME_WIDTH + 24.0f
            || missile->pos.y < -24.0f || missile->pos.y > (float)PLAY_HEIGHT + 24.0f) {
            missile->active = false;
        }
    }
}

// The missile is airborne, so the gun's air-class rule applies: bullets
// knock it down (for a small score via the downed event).
static void ResolveBulletBossMissileCollisions(GameState *state) {
    BossState *boss = &state->boss;
    for (int b = 0; b < MAX_BULLETS; b++) {
        if (!state->bullets[b].active) continue;
        for (int i = 0; i < MAX_BOSS_MISSILES; i++) {
            if (!boss->missiles[i].active) continue;
            float hitDist = BULLET_RADIUS + BOSS_MISSILE_RADIUS;
            if (DistanceSquared(state->bullets[b].pos, boss->missiles[i].pos) <= hitDist * hitDist) {
                state->bullets[b].active = false;
                boss->missiles[i].active = false;
                PushGameEvent(&state->gameEvents, (GameEvent){
                    .type = GAME_EVENT_BOSS_MISSILE_DOWNED, .pos = boss->missiles[i].pos
                });
                break;
            }
        }
    }
}

bool ResolveBossMissilePlayerCollision(BossState *boss, Vector2 playerPos, float playerRadius) {
    bool hit = false;
    for (int i = 0; i < MAX_BOSS_MISSILES; i++) {
        if (!boss->missiles[i].active) continue;
        float hitDist = BOSS_MISSILE_RADIUS + playerRadius;
        if (DistanceSquared(playerPos, boss->missiles[i].pos) <= hitDist * hitDist) {
            // Absorbed on contact either way (like enemy bullets), even
            // if invulnerability means it costs nothing.
            boss->missiles[i].active = false;
            hit = true;
        }
    }
    return hit;
}

// The patrol: sail a leg, then turn 180 in place at the lane end - the
// bow sweeps over the right screen edge, away from the player - and
// sail back. Each turn swaps which broadside (and hull section) faces
// the player.
static void UpdateBossPatrol(BossState *boss, float dt) {
    if (boss->turning) {
        boss->turnTimer += dt;
        float u = SmoothStep01(boss->turnTimer / BOSS_TURN_DURATION);
        // Rotate through 0 (bow east): up->down runs 90 -> -90, down->up
        // runs -90 -> 90, keyed on the pre-flip heading.
        boss->rotation = boss->sailDirection < 0 ? 90.0f - 180.0f * u : -90.0f + 180.0f * u;
        if (boss->turnTimer >= BOSS_TURN_DURATION) {
            boss->turning = false;
            boss->sailDirection = -boss->sailDirection;
            boss->rotation = boss->sailDirection < 0 ? 90.0f : -90.0f;
        }
        return;
    }

    boss->hullCenter.y += (float)boss->sailDirection * BOSS_SAIL_SPEED * dt;
    if (boss->sailDirection < 0 && boss->hullCenter.y <= BOSS_PATROL_TOP_Y) {
        boss->hullCenter.y = BOSS_PATROL_TOP_Y;
        boss->turning = true;
        boss->turnTimer = 0.0f;
    } else if (boss->sailDirection > 0 && boss->hullCenter.y >= BOSS_PATROL_BOTTOM_Y) {
        boss->hullCenter.y = BOSS_PATROL_BOTTOM_Y;
        boss->turning = true;
        boss->turnTimer = 0.0f;
    }
}

// One lob at the player's current position from any of the boss's
// mortar mouths (the Leviathan dome, the atoll center, a ring battery).
// A full shell pool skips the lob, the usual starve rule.
static void LobBossShell(GameState *state, Vector2 launch) {
    BossState *boss = &state->boss;
    for (int i = 0; i < MAX_MORTAR_SHELLS; i++) {
        if (boss->shells[i].active) continue;
        Vector2 target = state->player;
        if (target.x < 0.0f) target.x = 0.0f;
        if (target.x > (float)GAME_WIDTH) target.x = (float)GAME_WIDTH;
        if (target.y < 0.0f) target.y = 0.0f;
        if (target.y > (float)PLAY_HEIGHT) target.y = (float)PLAY_HEIGHT;
        boss->shells[i] = (MortarShell){
            .launch = launch,
            .target = target,
            .t = 0.0f,
            .blastT = 0.0f,
            .landed = false,
            .active = true
        };
        PushGameEvent(&state->gameEvents, (GameEvent){
            .type = GAME_EVENT_MORTAR_FIRED, .pos = launch
        });
        return;
    }
}

static void FireBossGuns(GameState *state, float dt) {
    BossState *boss = &state->boss;
    for (int part = BOSS_PART_POD_FORE; part <= BOSS_PART_HULL_AFT; part++) {
        if (!BossPartAlive(boss, (BossPartId)part)) continue;
        bool pod = part <= BOSS_PART_POD_AFT;
        // Fortress ring batteries return the stage's own mortar language:
        // staggered lobs so their shadows interleave with the central
        // atoll's, and silencing each one visibly thins the barrage -
        // the mid-fight pressure the fight-flow rework asked for.
        if (boss->type == BOSS_TYPE_FORTRESS_ATOLL && !pod) {
            boss->partFireTimer[part] += dt;
            while (boss->partFireTimer[part] >= FORTRESS_RING_MORTAR_INTERVAL) {
                boss->partFireTimer[part] -= FORTRESS_RING_MORTAR_INTERVAL;
                LobBossShell(state, BossPartPosition(boss, (BossPartId)part));
            }
            continue;
        }
        float interval = pod ? BOSS_POD_FIRE_INTERVAL : BOSS_SAM_INTERVAL;
        // A shielded (far-side) section holds pre-armed, like enemies at
        // the activation line: its first shot comes right as the turn
        // swings it around to face the player.
        if (!pod && !BossSectionFacesPlayer(boss, (BossPartId)part)) {
            boss->partFireTimer[part] = interval;
            continue;
        }
        boss->partFireTimer[part] += dt;
        while (boss->partFireTimer[part] >= interval) {
            boss->partFireTimer[part] -= interval;
            Vector2 from = BossPartPosition(boss, (BossPartId)part);
            float muzzle = BossPartRadius(boss, part) + 3.0f;
            if (pod) {
                // Pods fire turret-style aimed shots.
                Vector2 dir = AimAt(from, state->player);
                bool spawned = TrySpawnEnemyBullet(
                    state->enemyBullets,
                    MAX_ENEMY_BULLETS,
                    (Vector2){ from.x + dir.x * muzzle, from.y + dir.y * muzzle },
                    (Vector2){ dir.x * ENEMY_BULLET_SPEED, dir.y * ENEMY_BULLET_SPEED }
                );
                if (spawned) {
                    PushGameEvent(&state->gameEvents, (GameEvent){
                        .type = GAME_EVENT_ENEMY_FIRED, .pos = from
                    });
                }
            } else {
                // Hull sections are SAM batteries: the missile leaves the
                // cell westward (out the player-facing door), then its
                // own steering takes over. A cell-door flash marks the
                // launch alongside its whoosh.
                if (LaunchBossMissile(state, (Vector2){ from.x - muzzle, from.y })) {
                    PushGameEvent(&state->gameEvents, (GameEvent){
                        .type = GAME_EVENT_SAM_LAUNCHED, .pos = from
                    });
                }
            }
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
        LobBossShell(state, BossMortarPosition(boss));
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
            if (part == BOSS_PART_CORE && (!boss->coreExposed || boss->type == BOSS_TYPE_FORTRESS_ATOLL)) continue;
            if (!BossPartAlive(boss, part)) continue;
            float hitDist = BULLET_RADIUS + BossPartRadius(boss, part);
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
    int targetCount = boss->type == BOSS_TYPE_FORTRESS_ATOLL ? 1 : 3;
    for (int i = 0; i < targetCount; i++) {
        BossPartId part = boss->type == BOSS_TYPE_FORTRESS_ATOLL ? BOSS_PART_CORE : torpedoTargets[i];
        if (part == BOSS_PART_CORE
            && (!boss->coreExposed || (boss->type == BOSS_TYPE_FORTRESS_ATOLL && !boss->gatesOpen))) continue;
        if (!BossPartAlive(boss, part)) continue;
        float hitDist = BossPartRadius(boss, part) + TORPEDO_DIRECT_HIT_RADIUS;
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
    if (boss->type == BOSS_TYPE_FORTRESS_ATOLL) {
        if (!boss->coreExposed || !boss->gatesOpen || !BossPartAlive(boss, BOSS_PART_CORE)) return;
        float hitDist = BossPartRadius(boss, BOSS_PART_CORE) + TORPEDO_SPLASH_RADIUS;
        if (DistanceSquared(pos, BossPartPosition(boss, BOSS_PART_CORE)) <= hitDist * hitDist) {
            DamageBossPart(boss, BOSS_PART_CORE, BOSS_CORE_TORPEDO_DAMAGE, events);
        }
        return;
    }

    const BossPartId torpedoTargets[] = { BOSS_PART_HULL_FORE, BOSS_PART_HULL_AFT, BOSS_PART_CORE };
    for (int i = 0; i < 3; i++) {
        BossPartId part = torpedoTargets[i];
        if (part == BOSS_PART_CORE && !boss->coreExposed) continue;
        if (!BossPartAlive(boss, part)) continue;
        float hitDist = BossPartRadius(boss, part) + TORPEDO_SPLASH_RADIUS;
        if (DistanceSquared(pos, BossPartPosition(boss, part)) <= hitDist * hitDist) {
            DamageBossPart(boss, part,
                part == BOSS_PART_CORE ? BOSS_CORE_TORPEDO_DAMAGE : 1, events);
        }
    }
}

void ResolveBossMortarSplashDamage(BossState *boss, Vector2 pos, GameEventQueue *events) {
    if (boss->phase != BOSS_PHASE_FIGHTING) return;
    if (boss->type != BOSS_TYPE_FORTRESS_ATOLL) {
        ResolveBossSplashDamage(boss, pos, events);
        return;
    }
    const BossPartId batteries[] = { BOSS_PART_HULL_FORE, BOSS_PART_HULL_AFT };
    for (int i = 0; i < 2; i++) {
        BossPartId part = batteries[i];
        if (!BossPartAlive(boss, part)) continue;
        float hitDist = BossPartRadius(boss, part) + PLAYER_MORTAR_BLAST_RADIUS;
        if (DistanceSquared(pos, BossPartPosition(boss, part)) <= hitDist * hitDist) {
            DamageBossPart(boss, part, 1, events);
        }
    }
}

bool ResolveBossContactDamage(const BossState *boss, Vector2 playerPos, float playerRadius) {
    // The player flies safely over regular land, and the fixed fortress
    // atoll follows that established rule. Its channel and harbour are
    // visually water, so reusing their torpedo blockers as lethal contact
    // geometry made otherwise safe water look arbitrarily deadly.
    if (boss->type == BOSS_TYPE_FORTRESS_ATOLL) return false;
    Rectangle blockers[3];
    int count = BossHullBlockers(boss, blockers);
    for (int i = 0; i < count; i++) {
        float right = blockers[i].x + blockers[i].width;
        float bottom = blockers[i].y + blockers[i].height;
        float cx = playerPos.x < blockers[i].x ? blockers[i].x : (playerPos.x > right ? right : playerPos.x);
        float cy = playerPos.y < blockers[i].y ? blockers[i].y : (playerPos.y > bottom ? bottom : playerPos.y);
        if (DistanceSquared(playerPos, (Vector2){ cx, cy }) <= playerRadius * playerRadius) return true;
    }
    return false;
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
            if (boss->type == BOSS_TYPE_FORTRESS_ATOLL) {
                if (boss->phaseTimer >= BOSS_ENTRANCE_DURATION) {
                    boss->phase = BOSS_PHASE_FIGHTING;
                    boss->phaseTimer = 0.0f;
                }
                break;
            }
            // Sails up into view from below the arena to the bottom of
            // its patrol lane; guns stay quiet until the patrol starts.
            float startY = (float)PLAY_HEIGHT + BOSS_HULL_HALF_LENGTH + 15.0f;
            float u = SmoothStep01(boss->phaseTimer / BOSS_ENTRANCE_DURATION);
            boss->hullCenter.y = startY - (startY - BOSS_PATROL_BOTTOM_Y) * u;
            if (boss->phaseTimer >= BOSS_ENTRANCE_DURATION) {
                boss->hullCenter.y = BOSS_PATROL_BOTTOM_Y;
                boss->phase = BOSS_PHASE_FIGHTING;
                boss->phaseTimer = 0.0f;
            }
            break;
        }
        case BOSS_PHASE_FIGHTING:
            if (boss->type != BOSS_TYPE_FORTRESS_ATOLL) UpdateBossPatrol(boss, dt);
            if (boss->type == BOSS_TYPE_FORTRESS_ATOLL) {
                // Gates cycle from the fight's first seconds with an
                // asymmetric dwell (open runs longer than closed), each
                // edge announced by an event for the sound and the gate
                // visual - a rhythm to learn, not a coin flip.
                // Existing boats leave before this frame's gate state can
                // create the next one, so a newly opened gate visibly
                // starts with its craft at the channel mouth.
                UpdateFortressSorties(state, dt);
                boss->gateTimer += dt;
                float dwell = boss->gatesOpen
                    ? FORTRESS_GATE_OPEN_DURATION : FORTRESS_GATE_CLOSED_DURATION;
                while (boss->gateTimer >= dwell) {
                    boss->gateTimer -= dwell;
                    boss->gatesOpen = !boss->gatesOpen;
                    // A sortie on alternate openings halves boat pressure
                    // without shortening the core's attack window.
                    if (boss->gatesOpen && (boss->fortressGateOpenCount++ % 2) == 0) {
                        LaunchFortressSortie(state);
                    }
                    PushGameEvent(&state->gameEvents, (GameEvent){
                        .type = boss->gatesOpen
                            ? GAME_EVENT_BOSS_GATES_OPENED : GAME_EVENT_BOSS_GATES_CLOSED,
                        .pos = BossPartPosition(boss, BOSS_PART_CORE)
                    });
                    dwell = boss->gatesOpen
                        ? FORTRESS_GATE_OPEN_DURATION : FORTRESS_GATE_CLOSED_DURATION;
                }
            }
            FireBossGuns(state, dt);
            FireBossMortar(state, dt);
            ResolveBulletBossPartCollisions(state);
            break;
        case BOSS_PHASE_DYING: {
            // The remaining guns died with the core: their shots vanish
            // into the spectacle, and the chain walks the hull.
            for (int i = 0; i < MAX_ENEMY_BULLETS; i++) state->enemyBullets[i].active = false;
            for (int i = 0; i < MAX_BOSS_MISSILES; i++) boss->missiles[i].active = false;
            while (boss->deathExplosionsSpawned < BOSS_DEATH_EXPLOSION_COUNT
                   && boss->phaseTimer >= BOSS_DEATH_EXPLOSION_INTERVAL * (float)(boss->deathExplosionsSpawned + 1)) {
                TrySpawnExplosion(
                    state->explosions,
                    BossOffsetPosition(boss, BOSS_DEATH_EXPLOSION_OFFSETS[boss->deathExplosionsSpawned]),
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
                    .type = GAME_EVENT_BOSS_DEFEATED, .pos = boss->hullCenter
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
            // The stage's upgrade module lifts from the boss and docks onto
            // the spine. Stage 1 uses the mortar dome; Stage 2 extracts the
            // targeting computer from the fortress core at this same point.
            float u = SmoothStep01(boss->phaseTimer / BOSS_SALVAGE_DOCK_DURATION);
            boss->salvageDomePos = LerpV(BossMortarPosition(boss), state->player, u);
            if (boss->phaseTimer >= BOSS_SALVAGE_DOCK_DURATION) {
                boss->salvageDomePos = state->player;
                PushGameEvent(&state->gameEvents, (GameEvent){
                    .type = boss->type == BOSS_TYPE_FORTRESS_ATOLL ? GAME_EVENT_TARGETING_COMPUTER_SALVAGED
                        : GAME_EVENT_MORTAR_SALVAGED,
                    .pos = state->player
                });
                boss->phase = BOSS_PHASE_CLEARED;
                boss->phaseTimer = 0.0f;
                // The dock is the pickup and survives the stage transition;
                // the award comes from the stage table so the devtools
                // stage-select loadout can never drift from the real drop.
                ApplyUpgradeAward(state, GetStageDescriptor(state->stageNumber)->award);
                state->stageClear = true;
            }
            break;
        }
        default:
            break;
    }

    UpdateBossMissiles(state, dt);
    ResolveBulletBossMissileCollisions(state);
    UpdateMortarShells(boss, &state->gameEvents, dt);
}
