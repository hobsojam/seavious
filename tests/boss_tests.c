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
    CHECK(BossRemainingHp(&state.boss) == BossTotalHp());
    CHECK(BossTotalHp() == 2 * BOSS_POD_HP
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
    CHECK(BossRemainingHp(&state.boss) == BossTotalHp() - BOSS_POD_HP);
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

    // Within ~3.2s every armed part has opened fire (the latest opening
    // shot lands at 2.6s), and the shots arrive staggered rather than as
    // one volley.
    int lastCount = 0;
    bool sawStagger = false;
    for (int i = 0; i < 64; i++) {
        UpdateBossFight(&state, 0.05f);
        int count = 0;
        for (int b = 0; b < MAX_ENEMY_BULLETS; b++) {
            if (state.enemyBullets[b].active) count++;
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

    Rectangle blockers[2];
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
    UpdateBossFight(&state, 0.0001f);
    CHECK(state.boss.phase == BOSS_PHASE_DYING);
    CHECK(state.bossActive);

    // The chain clears surviving enemy fire and walks the hull.
    UpdateBossFight(&state, 0.1f);
    CHECK(!state.enemyBullets[0].active);
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

int main(void) {
    TestBossStartsOnLockAndParks();
    TestPodDiesToGunOnly();
    TestHullSectionsExposeCore();
    TestCoreHiddenUntilExposedAndMixedDamage();
    TestBossGunsFireStaggered();
    TestMortarCadenceAndBlast();
    TestBossContactWindow();
    TestSolidHullShieldsFarSection();
    TestPatrolTurnSwapsExposedSide();
    TestDefeatAndSalvageFlow();

    if (failures > 0) {
        fprintf(stderr, "%d boss test failure(s)\n", failures);
        return 1;
    }
    printf("boss tests passed\n");
    return 0;
}
