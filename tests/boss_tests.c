#include "boss.h"
#include "stage.h"
#include "stage_data.h"

#include <math.h>
#include <stdio.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\\n", __FILE__, __LINE__, #condition); \
        failures++; \
    } \
} while (0)

#define NEAR(actual, expected) CHECK(fabsf((actual) - (expected)) < 0.01f)

// Drive a run straight to the boss lock and through the entrance so
// tests start from a parked, fighting boss.
static void SetUpFightingBoss(GameState *state) {
    ResetRunState(state);
    state->scrollDistance = (float)STAGE1_LENGTH_PX;
    state->stageCursor = STAGE1_EVENT_COUNT;
    UpdateStageScript(state, 0.001f);
    UpdateBossFight(state, 0.001f);
    // Finish the entrance in small steps (phase transitions are per-update).
    for (int i = 0; i < 400 && state->boss.phase == BOSS_PHASE_ENTERING; i++) {
        UpdateBossFight(state, 0.05f);
    }
    state->gameEvents.count = 0;
}

static void TestBossStartsOnLockAndParks(void) {
    GameState state;
    ResetRunState(&state);
    state.scrollDistance = (float)STAGE1_LENGTH_PX;
    state.stageCursor = STAGE1_EVENT_COUNT;

    // Before the lock the boss module does nothing.
    UpdateBossFight(&state, 1.0f);
    CHECK(state.boss.phase == BOSS_PHASE_INACTIVE && !state.bossActive);

    UpdateStageScript(&state, 0.001f);
    CHECK(state.bossLock);
    UpdateBossFight(&state, 0.001f);
    CHECK(state.boss.phase == BOSS_PHASE_ENTERING);
    CHECK(state.bossActive);
    // Sails in from below the arena, bow up, on its patrol column.
    CHECK(state.boss.hullCenter.y > (float)PLAY_HEIGHT);
    NEAR(state.boss.hullCenter.x, BOSS_PATROL_X);
    NEAR(state.boss.rotation, 90.0f);

    // The entrance sails the hull up to the bottom of the patrol lane.
    for (int i = 0; i < 400 && state.boss.phase == BOSS_PHASE_ENTERING; i++) {
        UpdateBossFight(&state, 0.05f);
    }
    CHECK(state.boss.phase == BOSS_PHASE_FIGHTING);
    NEAR(state.boss.hullCenter.x, BOSS_PATROL_X);
    NEAR(state.boss.hullCenter.y, BOSS_PATROL_BOTTOM_Y);
    CHECK(BossRemainingHp(&state.boss) == BossTotalHp(&state.boss));
    CHECK(BossTotalHp(&state.boss) == 2 * BOSS_POD_HP
        + 2 * BOSS_HULL_SECTION_TORPEDO_WORTH * BOSS_HULL_SECTION_HP + BOSS_CORE_HP);
}

static void TestPodDiesToGunOnly(void) {
    GameState state;
    SetUpFightingBoss(&state);
    Vector2 podPos = BossPartPosition(&state.boss, BOSS_PART_POD_FORE);

    // A torpedo on the pod passes straight through: pods are gun-class.
    state.torpedo = (Torpedo){ .pos = podPos, .armed = true, .active = true };
    TorpedoImpact impact = ResolveTorpedoBossPartCollision(&state.torpedo, &state.boss, &state.gameEvents);
    CHECK(impact.type == TORPEDO_IMPACT_NONE && state.torpedo.active);
    state.torpedo.active = false;

    // Twelve gun hits kill it; each bullet is consumed.
    for (int hit = 0; hit < BOSS_POD_HP; hit++) {
        state.bullets[0] = (Bullet){ .pos = podPos, .active = true };
        UpdateBossFight(&state, 0.0001f);
        CHECK(!state.bullets[0].active);
    }
    CHECK(!BossPartAlive(&state.boss, BOSS_PART_POD_FORE));
    CHECK(BossRemainingHp(&state.boss) == BossTotalHp(&state.boss) - BOSS_POD_HP);
    CHECK(ScoreGameEvents(&state.gameEvents) == SCORE_BOSS_POD);
}

static void TestHullSectionsExposeCore(void) {
    GameState state;
    SetUpFightingBoss(&state);

    // Gun bullets never touch the amber hull sections.
    Vector2 forePos = BossPartPosition(&state.boss, BOSS_PART_HULL_FORE);
    state.bullets[0] = (Bullet){ .pos = forePos, .active = true };
    UpdateBossFight(&state, 0.0001f);
    CHECK(state.bullets[0].active);
    CHECK(state.boss.partHp[BOSS_PART_HULL_FORE] == BOSS_HULL_SECTION_HP);
    state.bullets[0].active = false;

    // An armed torpedo detonates on the section; the splash pass damages
    // it exactly once (the no-double-hit rule).
    for (int part = BOSS_PART_HULL_FORE; part <= BOSS_PART_HULL_AFT; part++) {
        for (int hit = 0; hit < BOSS_HULL_SECTION_HP; hit++) {
            Vector2 pos = BossPartPosition(&state.boss, (BossPartId)part);
            state.torpedo = (Torpedo){ .pos = pos, .armed = true, .active = true };
            TorpedoImpact impact = ResolveTorpedoBossPartCollision(&state.torpedo, &state.boss, &state.gameEvents);
            CHECK(impact.type == TORPEDO_IMPACT_EXPLOSION && !state.torpedo.active);
            ResolveBossSplashDamage(&state.boss, impact.pos, &state.gameEvents);
        }
        CHECK(!BossPartAlive(&state.boss, (BossPartId)part));
    }

    CHECK(state.boss.coreExposed);
    CHECK(ScoreGameEvents(&state.gameEvents) == 2 * SCORE_BOSS_HULL_SECTION);
}

static void TestCoreHiddenUntilExposedAndMixedDamage(void) {
    GameState state;
    SetUpFightingBoss(&state);
    Vector2 corePos = BossPartPosition(&state.boss, BOSS_PART_CORE);

    // Hidden core: neither weapon touches it.
    state.bullets[0] = (Bullet){ .pos = corePos, .active = true };
    UpdateBossFight(&state, 0.0001f);
    CHECK(state.bullets[0].active);
    state.bullets[0].active = false;
    state.torpedo = (Torpedo){ .pos = corePos, .armed = true, .active = true };
    CHECK(ResolveTorpedoBossPartCollision(&state.torpedo, &state.boss, &state.gameEvents).type
        == TORPEDO_IMPACT_NONE);
    state.torpedo.active = false;

    // Mixing works once exposed: 3 torpedoes (18) + 6 gun hits = 24.
    state.boss.partHp[BOSS_PART_HULL_FORE] = 0;
    state.boss.partHp[BOSS_PART_HULL_AFT] = 0;
    state.boss.coreExposed = true;
    for (int i = 0; i < 3; i++) {
        state.torpedo = (Torpedo){ .pos = corePos, .armed = false, .active = true };
        TorpedoImpact impact = ResolveTorpedoBossPartCollision(&state.torpedo, &state.boss, &state.gameEvents);
        CHECK(impact.type == TORPEDO_IMPACT_DIRECT);
    }
    CHECK(state.boss.partHp[BOSS_PART_CORE] == BOSS_CORE_HP - 3 * BOSS_CORE_TORPEDO_DAMAGE);
    for (int i = 0; i < 6; i++) {
        state.bullets[0] = (Bullet){ .pos = corePos, .active = true };
        UpdateBossFight(&state, 0.0001f);
    }
    CHECK(!BossPartAlive(&state.boss, BOSS_PART_CORE));
    CHECK(state.boss.phase == BOSS_PHASE_DYING);
    CHECK(ScoreGameEvents(&state.gameEvents) == SCORE_BOSS_CORE);
}

static void TestBossGunsFireStaggered(void) {
    GameState state;
    SetUpFightingBoss(&state);

    // Within ~3.2s every armed part on the player's side has opened
    // fire - pod bullets plus the facing SAM battery's missile - and
    // the openings arrive staggered rather than as one volley.
    int lastCount = 0;
    bool sawStagger = false;
    for (int i = 0; i < 64; i++) {
        UpdateBossFight(&state, 0.05f);
        int count = 0;
        for (int b = 0; b < MAX_ENEMY_BULLETS; b++) {
            if (state.enemyBullets[b].active) count++;
        }
        for (int m = 0; m < MAX_BOSS_MISSILES; m++) {
            if (state.boss.missiles[m].active) count++;
        }
        if (count > lastCount && lastCount > 0) sawStagger = true;
        lastCount = count;
    }
    CHECK(lastCount >= 4);
    CHECK(sawStagger);
}

static void TestMortarCadenceAndBlast(void) {
    GameState state;
    SetUpFightingBoss(&state);
    state.player = (Vector2){ 100.0f, 200.0f };

    // First lob lands within the first interval-and-air-time.
    int active = 0;
    for (int i = 0; i < 60 && active == 0; i++) {
        UpdateBossFight(&state, 0.1f);
        for (int s = 0; s < MAX_MORTAR_SHELLS; s++) {
            if (state.boss.shells[s].active) active++;
        }
    }
    CHECK(active == 1);
    const MortarShell *shell = &state.boss.shells[0];
    NEAR(shell->target.x, 100.0f);
    NEAR(shell->target.y, 200.0f);

    // Ride it down to the blast: the blast kills inside its radius and
    // misses outside it.
    for (int i = 0; i < 60 && !state.boss.shells[0].landed; i++) {
        UpdateBossFight(&state, 0.05f);
    }
    CHECK(state.boss.shells[0].landed);
    CHECK(ResolveMortarBlastPlayerHit(&state.boss, (Vector2){ 100.0f, 200.0f }, PLAYER_HIT_RADIUS));
    CHECK(!ResolveMortarBlastPlayerHit(&state.boss,
        (Vector2){ 100.0f + BOSS_MORTAR_BLAST_RADIUS + PLAYER_HIT_RADIUS + 2.0f, 200.0f }, PLAYER_HIT_RADIUS));

    // The blast window expires and frees the shell slot.
    for (int i = 0; i < 20; i++) UpdateBossFight(&state, 0.05f);
    CHECK(!state.boss.shells[0].active);

    // Core exposure quickens the cadence.
    CHECK(BOSS_MORTAR_INTERVAL_CORE_EXPOSED < BOSS_MORTAR_INTERVAL);
}

static void TestBossContactWindow(void) {
    GameState state;
    SetUpFightingBoss(&state);
    Vector2 onHull = state.boss.hullCenter;
    Vector2 leftOfHull = { state.boss.hullCenter.x - 60.0f, state.boss.hullCenter.y };

    CHECK(ResolveBossContactDamage(&state.boss, onHull, PLAYER_HIT_RADIUS));
    CHECK(!ResolveBossContactDamage(&state.boss, leftOfHull, PLAYER_HIT_RADIUS));

    // The settled wreck is inert, like every wreck.
    state.boss.phase = BOSS_PHASE_DYING;
    CHECK(!ResolveBossContactDamage(&state.boss, onHull, PLAYER_HIT_RADIUS));
}

static void TestSolidHullShieldsFarSection(void) {
    GameState state;
    SetUpFightingBoss(&state);

    // Sailing up (first leg): the sprite-bottom section swings to the
    // player's side; the other broadside is shielded behind the armor.
    CHECK(BossSectionFacesPlayer(&state.boss, BOSS_PART_HULL_FORE));
    CHECK(!BossSectionFacesPlayer(&state.boss, BOSS_PART_HULL_AFT));

    Rectangle blockers[3];
    CHECK(BossHullBlockers(&state.boss, blockers) == 1);
    NEAR(blockers[0].x, BOSS_PATROL_X - 36.0f);

    // The reticle clamps at the armor in the far section's lane.
    Vector2 aft = BossPartPosition(&state.boss, BOSS_PART_HULL_AFT);
    Vector2 spawn = { 100.0f, aft.y };
    Vector2 reticle = ClampReticleToRects(spawn, (Vector2){ 500.0f, aft.y }, blockers, 1);
    NEAR(reticle.x, blockers[0].x);

    // A torpedo down that lane detonates on the armor: no part is close
    // enough to hit first, the armor eats the shot, and the splash can't
    // reach across the deck to the shielded section.
    state.torpedo = (Torpedo){ .pos = { blockers[0].x - 3.0f, aft.y }, .armed = true, .active = true };
    TorpedoImpact impact = ResolveTorpedoBossPartCollision(&state.torpedo, &state.boss, &state.gameEvents);
    CHECK(impact.type == TORPEDO_IMPACT_NONE);
    impact = ResolveTorpedoRectCollision(&state.torpedo, blockers, 1);
    CHECK(impact.type == TORPEDO_IMPACT_EXPLOSION && !state.torpedo.active);
    NEAR(impact.pos.x, blockers[0].x);
    ResolveBossSplashDamage(&state.boss, impact.pos, &state.gameEvents);
    CHECK(state.boss.partHp[BOSS_PART_HULL_AFT] == BOSS_HULL_SECTION_HP);

    // Unarmed shots fizzle on the armor, land-style.
    state.torpedo = (Torpedo){ .pos = { blockers[0].x - 3.0f, aft.y }, .armed = false, .active = true };
    impact = ResolveTorpedoRectCollision(&state.torpedo, blockers, 1);
    CHECK(impact.type == TORPEDO_IMPACT_DIRECT && !state.torpedo.active);

    // Blowing both sections tears the breach open: the armor splits in
    // two and the core's lane becomes the one torpedo path through.
    state.boss.partHp[BOSS_PART_HULL_FORE] = 0;
    state.boss.partHp[BOSS_PART_HULL_AFT] = 0;
    state.boss.coreExposed = true;
    CHECK(BossHullBlockers(&state.boss, blockers) == 2);
    Vector2 core = BossPartPosition(&state.boss, BOSS_PART_CORE);
    CHECK(blockers[0].y + blockers[0].height < core.y);
    CHECK(blockers[1].y > core.y);

    state.torpedo = (Torpedo){ .pos = { core.x - 30.0f, core.y }, .armed = false, .active = true };
    impact = ResolveTorpedoRectCollision(&state.torpedo, blockers, 2);
    CHECK(impact.type == TORPEDO_IMPACT_NONE && state.torpedo.active);
    state.torpedo.pos.x = core.x - 15.0f;
    impact = ResolveTorpedoBossPartCollision(&state.torpedo, &state.boss, &state.gameEvents);
    CHECK(impact.type == TORPEDO_IMPACT_DIRECT);
    CHECK(state.boss.partHp[BOSS_PART_CORE] == BOSS_CORE_HP - BOSS_CORE_TORPEDO_DAMAGE);
}

static void TestSamBattery(void) {
    GameState state;
    SetUpFightingBoss(&state);
    state.player = (Vector2){ 80.0f, 176.0f };

    // The facing battery's first launch comes at its stagger delay; the
    // missile leaves the cell flying west.
    int alive = 0;
    for (int i = 0; i < 40 && alive == 0; i++) {
        UpdateBossFight(&state, 0.05f);
        for (int m = 0; m < MAX_BOSS_MISSILES; m++) {
            if (state.boss.missiles[m].active) alive++;
        }
    }
    CHECK(alive == 1);
    CHECK(state.boss.missiles[0].vel.x < 0.0f);

    // Homing: with the player due north, the heading bends toward it
    // but never faster than the turn cap.
    BossMissile *missile = &state.boss.missiles[0];
    missile->pos = (Vector2){ 200.0f, 200.0f };
    missile->vel = (Vector2){ -BOSS_MISSILE_SPEED, 0.0f };
    state.player = (Vector2){ 200.0f, 60.0f };
    float before = atan2f(missile->vel.y, missile->vel.x);
    UpdateBossFight(&state, 0.05f);
    float after = atan2f(missile->vel.y, missile->vel.x);
    float turned = fabsf(after - before);
    if (turned > PI) turned = 2.0f * PI - turned;
    CHECK(turned > 0.01f);
    CHECK(turned <= BOSS_MISSILE_TURN_RATE * DEG2RAD * 0.05f + 0.001f);

    // The gun knocks it down for a small score (air-class rule).
    state.gameEvents.count = 0;
    state.bullets[0] = (Bullet){ .pos = missile->pos, .active = true };
    UpdateBossFight(&state, 0.0001f);
    CHECK(!missile->active && !state.bullets[0].active);
    CHECK(ScoreGameEvents(&state.gameEvents) == SCORE_BOSS_MISSILE);

    // Player contact absorbs the missile (the kill gating lives in the
    // update loop, like enemy bullets).
    state.boss.missiles[1] = (BossMissile){
        .pos = state.player, .vel = { -BOSS_MISSILE_SPEED, 0.0f }, .active = true
    };
    CHECK(ResolveBossMissilePlayerCollision(&state.boss, state.player, PLAYER_HIT_RADIUS));
    CHECK(!state.boss.missiles[1].active);

    // Out of fuel: fizzles harmlessly, no downed event, no score.
    state.boss.missiles[2] = (BossMissile){
        .pos = { 200.0f, 100.0f }, .vel = { -BOSS_MISSILE_SPEED, 0.0f },
        .age = BOSS_MISSILE_LIFETIME - 0.01f, .active = true
    };
    state.gameEvents.count = 0;
    UpdateBossFight(&state, 0.05f);
    CHECK(!state.boss.missiles[2].active);
    CHECK(state.gameEvents.count == 0);
}

static void TestPatrolTurnSwapsExposedSide(void) {
    GameState state;
    SetUpFightingBoss(&state);

    // Sail the first (upward) leg and ride through the turn: the ship
    // ends up bow-down with the other broadside facing the player.
    for (int i = 0; i < 400 && state.boss.sailDirection < 0; i++) {
        UpdateBossFight(&state, 0.05f);
    }
    CHECK(state.boss.sailDirection > 0);
    CHECK(!state.boss.turning);
    NEAR(state.boss.rotation, -90.0f);
    NEAR(state.boss.hullCenter.y, BOSS_PATROL_TOP_Y);
    CHECK(BossSectionFacesPlayer(&state.boss, BOSS_PART_HULL_AFT));
    CHECK(!BossSectionFacesPlayer(&state.boss, BOSS_PART_HULL_FORE));
}

static void TestDefeatAndSalvageFlow(void) {
    GameState state;
    SetUpFightingBoss(&state);
    state.player = (Vector2){ 60.0f, 176.0f };

    // Kill the core directly: the fight tips into the death chain.
    state.boss.partHp[BOSS_PART_HULL_FORE] = 0;
    state.boss.partHp[BOSS_PART_HULL_AFT] = 0;
    state.boss.coreExposed = true;
    state.boss.partHp[BOSS_PART_CORE] = 1;
    state.bullets[0] = (Bullet){ .pos = BossPartPosition(&state.boss, BOSS_PART_CORE), .active = true };
    state.enemyBullets[0] = (EnemyBullet){ .pos = { 10.0f, 10.0f }, .vel = { -1.0f, 0.0f }, .active = true };
    state.boss.missiles[0] = (BossMissile){
        .pos = { 200.0f, 100.0f }, .vel = { -BOSS_MISSILE_SPEED, 0.0f }, .active = true
    };
    UpdateBossFight(&state, 0.0001f);
    CHECK(state.boss.phase == BOSS_PHASE_DYING);
    CHECK(state.bossActive);

    // The chain clears surviving enemy fire - bullets and SAMs both -
    // and walks the hull.
    UpdateBossFight(&state, 0.1f);
    CHECK(!state.enemyBullets[0].active);
    CHECK(!state.boss.missiles[0].active);
    for (int i = 0; i < 100 && state.boss.phase == BOSS_PHASE_DYING; i++) {
        UpdateBossFight(&state, 0.05f);
    }
    CHECK(state.boss.phase == BOSS_PHASE_SALVAGE_APPROACH);
    CHECK(!state.bossActive);
    CHECK(state.boss.settleOffset > 0.0f);
    bool sawDefeated = false;
    for (int i = 0; i < state.gameEvents.count; i++) {
        if (state.gameEvents.items[i].type == GAME_EVENT_BOSS_DEFEATED) sawDefeated = true;
    }
    CHECK(sawDefeated);

    // Autopilot parks the skimmer alongside the dome, the dome docks,
    // and the run flips to stage clear with the salvage jingle event.
    CHECK(BossSequenceOwnsPlayer(&state));
    state.gameEvents.count = 0;
    for (int i = 0; i < 200 && state.boss.phase != BOSS_PHASE_CLEARED; i++) {
        UpdateBossFight(&state, 0.05f);
    }
    CHECK(state.boss.phase == BOSS_PHASE_CLEARED);
    CHECK(state.stageClear);
    CHECK(state.hasMortar);
    Vector2 mortar = BossMortarPosition(&state.boss);
    CHECK(fabsf(state.player.x - (mortar.x - 44.0f)) < 1.0f);
    CHECK(fabsf(state.player.y - mortar.y) < 1.0f);
    bool sawSalvaged = false;
    for (int i = 0; i < state.gameEvents.count; i++) {
        if (state.gameEvents.items[i].type == GAME_EVENT_MORTAR_SALVAGED) sawSalvaged = true;
    }
    CHECK(sawSalvaged);

    // Restart rewinds everything, boss included.
    ResetRunState(&state);
    CHECK(state.boss.phase == BOSS_PHASE_INACTIVE && !state.stageClear && !state.bossActive);
}

static void SetUpFightingAtoll(GameState *state) {
    ResetRunState(state);
    BeginStage(state, 2);
    state->scrollDistance = (float)STAGE2_LENGTH_PX;
    state->stageCursor = STAGE2_EVENT_COUNT;
    UpdateStageScript(state, 0.001f);
    UpdateBossFight(state, 0.001f);
    for (int i = 0; i < 400 && state->boss.phase == BOSS_PHASE_ENTERING; i++) {
        UpdateBossFight(state, 0.05f);
    }
    state->gameEvents.count = 0;
}

static bool SamePos(Vector2 a, Vector2 b) {
    return fabsf(a.x - b.x) < 0.5f && fabsf(a.y - b.y) < 0.5f;
}

static void TestFortressRingBarrageAndGateRhythm(void) {
    GameState state;
    SetUpFightingAtoll(&state);

    // Three staggered lobbers: both ring batteries return the stage's
    // mortar language alongside the central atoll's lob - one shell each
    // inside the first three seconds, from three distinct mouths.
    for (int i = 0; i < 60; i++) UpdateBossFight(&state, 0.05f);
    Vector2 fore = BossPartPosition(&state.boss, BOSS_PART_HULL_FORE);
    Vector2 aft = BossPartPosition(&state.boss, BOSS_PART_HULL_AFT);
    Vector2 central = BossMortarPosition(&state.boss);
    int foreLobs = 0, aftLobs = 0, centralLobs = 0;
    for (int i = 0; i < state.gameEvents.count; i++) {
        const GameEvent *e = &state.gameEvents.items[i];
        if (e->type != GAME_EVENT_MORTAR_FIRED) continue;
        if (SamePos(e->pos, fore)) foreLobs++;
        else if (SamePos(e->pos, aft)) aftLobs++;
        else if (SamePos(e->pos, central)) centralLobs++;
    }
    CHECK(foreLobs == 1 && aftLobs == 1 && centralLobs == 1);

    // Silencing the ring visibly thins the barrage: only the central
    // atoll keeps lobbing once both batteries are down.
    state.boss.partHp[BOSS_PART_HULL_FORE] = 0;
    state.boss.partHp[BOSS_PART_HULL_AFT] = 0;
    for (int i = 0; i < MAX_MORTAR_SHELLS; i++) state.boss.shells[i].active = false;
    state.gameEvents.count = 0;
    for (int i = 0; i < 100; i++) UpdateBossFight(&state, 0.05f);
    int lobs = 0;
    for (int i = 0; i < state.gameEvents.count; i++) {
        const GameEvent *e = &state.gameEvents.items[i];
        if (e->type != GAME_EVENT_MORTAR_FIRED) continue;
        lobs++;
        CHECK(SamePos(e->pos, central));
    }
    CHECK(lobs >= 1);

    // The gates cycle from the fight's start - before core exposure -
    // open dwelling longer than closed, each edge announced for the
    // sound/visual telegraph.
    CHECK(!state.boss.coreExposed);
    state.boss.gateTimer = 0.0f;
    state.boss.gatesOpen = false;
    state.gameEvents.count = 0;
    UpdateBossFight(&state, FORTRESS_GATE_CLOSED_DURATION - 0.05f);
    CHECK(!state.boss.gatesOpen);
    UpdateBossFight(&state, 0.1f);
    CHECK(state.boss.gatesOpen);
    int sorties = 0;
    int sortieIndex = -1;
    for (int i = 0; i < MAX_SURFACE_TARGETS; i++) {
        if (state.surfaceTargets[i].active && state.surfaceTargets[i].fortressSortie) {
            sorties++;
            sortieIndex = i;
        }
    }
    CHECK(sorties == 1);
    CHECK(state.surfaceTargets[sortieIndex].type == SURFACE_TARGET_MOBILE_PLATFORM);
    NEAR(state.surfaceTargets[sortieIndex].pos.x, state.boss.hullCenter.x - 38.0f);
    UpdateBossFight(&state, 0.5f);
    NEAR(state.surfaceTargets[sortieIndex].pos.x,
        state.boss.hullCenter.x - 38.0f - MOBILE_PLATFORM_SPEED * 0.5f);
    UpdateBossFight(&state, FORTRESS_GATE_OPEN_DURATION);
    CHECK(!state.boss.gatesOpen);
    // The next opening keeps the full core window but deliberately has no
    // second boat: fortress sorties run at half the gate frequency.
    UpdateBossFight(&state, FORTRESS_GATE_CLOSED_DURATION);
    CHECK(state.boss.gatesOpen);
    int secondCycleSorties = 0;
    for (int i = 0; i < MAX_SURFACE_TARGETS; i++) {
        if (state.surfaceTargets[i].active && state.surfaceTargets[i].fortressSortie) secondCycleSorties++;
    }
    CHECK(secondCycleSorties == 1);
    int opened = 0, closed = 0;
    for (int i = 0; i < state.gameEvents.count; i++) {
        if (state.gameEvents.items[i].type == GAME_EVENT_BOSS_GATES_OPENED) opened++;
        if (state.gameEvents.items[i].type == GAME_EVENT_BOSS_GATES_CLOSED) closed++;
    }
    CHECK(opened == 2 && closed == 1);

    // Closed gates block the torpedo route for the whole fight, not just
    // after exposure; open gates clear it (core targeting stays gated on
    // exposure separately).
    Rectangle blockers[3];
    state.boss.gatesOpen = false;
    CHECK(BossHullBlockers(&state.boss, blockers) == 3);
    state.boss.gatesOpen = true;
    CHECK(BossHullBlockers(&state.boss, blockers) == 2);
    // The central water channel remains the sole clear level-torpedo lane.
    Torpedo lane = { .pos = { state.boss.hullCenter.x - 70.0f, state.boss.hullCenter.y },
        .active = true };
    CHECK(ResolveTorpedoRectCollision(&lane, blockers, 2).type == TORPEDO_IMPACT_NONE);
    // A diagonal pad lane is solid island, forcing the mortar-only route.
    Torpedo bank = { .pos = { state.boss.hullCenter.x - 70.0f, state.boss.hullCenter.y - 27.0f },
        .active = true };
    CHECK(ResolveTorpedoRectCollision(&bank, blockers, 2).type == TORPEDO_IMPACT_DIRECT);
}

static void TestFortressAtollWeaponGatesAndSalvage(void) {
    GameState state;
    SetUpFightingAtoll(&state);
    CHECK(BossIsFortressAtoll(&state.boss));
    CHECK(state.boss.phase == BOSS_PHASE_FIGHTING);
    NEAR(state.boss.hullCenter.x, 404.0f);
    NEAR(state.boss.hullCenter.y, 176.0f);
    Vector2 northEastGun = BossPartPosition(&state.boss, BOSS_PART_POD_FORE);
    Vector2 southWestGun = BossPartPosition(&state.boss, BOSS_PART_POD_AFT);
    Vector2 northWestBattery = BossPartPosition(&state.boss, BOSS_PART_HULL_FORE);
    Vector2 southEastBattery = BossPartPosition(&state.boss, BOSS_PART_HULL_AFT);
    NEAR(northEastGun.x, state.boss.hullCenter.x + 26.0f);
    NEAR(northEastGun.y, state.boss.hullCenter.y - 27.0f);
    NEAR(southWestGun.x, state.boss.hullCenter.x - 27.0f);
    NEAR(southWestGun.y, state.boss.hullCenter.y + 26.0f);
    NEAR(northWestBattery.x, state.boss.hullCenter.x - 27.0f);
    NEAR(northWestBattery.y, state.boss.hullCenter.y - 27.0f);
    NEAR(southEastBattery.x, state.boss.hullCenter.x + 26.0f);
    NEAR(southEastBattery.y, state.boss.hullCenter.y + 26.0f);

    // AA pods are gun-only.
    Vector2 pod = BossPartPosition(&state.boss, BOSS_PART_POD_FORE);
    int podHp = state.boss.partHp[BOSS_PART_POD_FORE];
    state.torpedo = (Torpedo){ .pos = pod, .armed = false, .active = true };
    CHECK(ResolveTorpedoBossPartCollision(&state.torpedo, &state.boss, &state.gameEvents).type
        == TORPEDO_IMPACT_NONE);
    state.torpedo.active = false;
    state.bullets[0] = (Bullet){ .pos = pod, .active = true };
    UpdateBossFight(&state, 0.0001f);
    CHECK(state.boss.partHp[BOSS_PART_POD_FORE] == podHp - 1);

    // Ring batteries are mortar-only.
    Vector2 battery = BossPartPosition(&state.boss, BOSS_PART_HULL_FORE);
    int batteryHp = state.boss.partHp[BOSS_PART_HULL_FORE];
    state.bullets[0] = (Bullet){ .pos = battery, .active = true };
    UpdateBossFight(&state, 0.0001f);
    CHECK(state.bullets[0].active && state.boss.partHp[BOSS_PART_HULL_FORE] == batteryHp);
    state.bullets[0].active = false;
    ResolveBossMortarSplashDamage(&state.boss, battery, &state.gameEvents);
    CHECK(state.boss.partHp[BOSS_PART_HULL_FORE] == batteryHp - 1);

    // With all outer defenses down, a closed sea gate still blocks the
    // torpedo route; an open gate exposes the core to torpedoes only.
    state.boss.partHp[BOSS_PART_POD_FORE] = 0;
    state.boss.partHp[BOSS_PART_POD_AFT] = 0;
    state.boss.partHp[BOSS_PART_HULL_FORE] = 0;
    state.boss.partHp[BOSS_PART_HULL_AFT] = 0;
    state.boss.coreExposed = true;
    state.boss.gatesOpen = false;
    Vector2 core = BossPartPosition(&state.boss, BOSS_PART_CORE);
    state.torpedo = (Torpedo){ .pos = core, .armed = false, .active = true };
    CHECK(ResolveTorpedoBossPartCollision(&state.torpedo, &state.boss, &state.gameEvents).type
        == TORPEDO_IMPACT_NONE);
    Rectangle gate[3];
    CHECK(BossHullBlockers(&state.boss, gate) == 3);
    CHECK(gate[2].x + gate[2].width < core.x);
    CHECK(!ResolveBossContactDamage(&state.boss,
        (Vector2){ gate[2].x + gate[2].width / 2.0f, core.y }, PLAYER_HIT_RADIUS));

    // The sea gate keeps cycling on its asymmetric dwell; realigned here
    // so the edge lands exactly on the closed-duration boundary.
    state.boss.gateTimer = 0.0f;
    state.boss.gatesOpen = false;
    UpdateBossFight(&state, FORTRESS_GATE_CLOSED_DURATION - 0.01f);
    CHECK(!state.boss.gatesOpen);
    UpdateBossFight(&state, 0.02f);
    CHECK(state.boss.gatesOpen);
    state.boss.gatesOpen = true;
    int coreHp = state.boss.partHp[BOSS_PART_CORE];
    // The harbour aperture is deliberately forgiving: a torpedo through
    // the open gate need not land on the core's exact centre pixel.
    state.torpedo.pos = (Vector2){ core.x, core.y + 20.0f };
    CHECK(ResolveTorpedoBossPartCollision(&state.torpedo, &state.boss, &state.gameEvents).type
        == TORPEDO_IMPACT_DIRECT);
    CHECK(state.boss.partHp[BOSS_PART_CORE] == coreHp - BOSS_CORE_TORPEDO_DAMAGE);

    // Armed torpedoes defer the damage to their splash; at the atoll that
    // same gate controls the splash route as well.
    state.torpedo = (Torpedo){ .pos = core, .armed = true, .active = true };
    CHECK(ResolveTorpedoBossPartCollision(&state.torpedo, &state.boss, &state.gameEvents).type
        == TORPEDO_IMPACT_EXPLOSION);
    coreHp = state.boss.partHp[BOSS_PART_CORE];
    ResolveBossSplashDamage(&state.boss, core, &state.gameEvents);
    CHECK(state.boss.partHp[BOSS_PART_CORE] == coreHp - BOSS_CORE_TORPEDO_DAMAGE);

    state.boss.partHp[BOSS_PART_CORE] = 1;
    state.torpedo = (Torpedo){ .pos = core, .armed = false, .active = true };
    ResolveTorpedoBossPartCollision(&state.torpedo, &state.boss, &state.gameEvents);
    for (int i = 0; i < 240 && state.boss.phase != BOSS_PHASE_CLEARED; i++) {
        UpdateBossFight(&state, 0.05f);
    }
    CHECK(state.boss.phase == BOSS_PHASE_CLEARED);
    CHECK(state.hasTargetingComputer && !state.hasMortar);
    bool sawTargetingComputer = false;
    for (int i = 0; i < state.gameEvents.count; i++) {
        if (state.gameEvents.items[i].type == GAME_EVENT_TARGETING_COMPUTER_SALVAGED) {
            sawTargetingComputer = true;
        }
    }
    CHECK(sawTargetingComputer);
    BeginStage(&state, 1);
    CHECK(state.hasTargetingComputer);
}

int main(void) {
    TestBossStartsOnLockAndParks();
    TestPodDiesToGunOnly();
    TestHullSectionsExposeCore();
    TestCoreHiddenUntilExposedAndMixedDamage();
    TestBossGunsFireStaggered();
    TestMortarCadenceAndBlast();
    TestBossContactWindow();
    TestSolidHullShieldsFarSection();
    TestSamBattery();
    TestPatrolTurnSwapsExposedSide();
    TestDefeatAndSalvageFlow();
    TestFortressAtollWeaponGatesAndSalvage();
    TestFortressRingBarrageAndGateRhythm();

    if (failures > 0) {
        fprintf(stderr, "%d boss test failure(s)\n", failures);
        return 1;
    }
    printf("boss tests passed\n");
    return 0;
}
