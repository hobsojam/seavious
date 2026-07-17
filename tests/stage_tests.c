#include "game_state.h"
#include "stage.h"
#include "stage_data.h"

#include <stdio.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\\n", __FILE__, __LINE__, #condition); \
        failures++; \
    } \
} while (0)

static int CountActiveAir(const GameState *state) {
    int n = 0;
    for (int i = 0; i < MAX_AIR_TARGETS; i++) {
        if (state->airTargets[i].active) n++;
    }
    return n;
}

static int CountActiveSurface(const GameState *state) {
    int n = 0;
    for (int i = 0; i < MAX_SURFACE_TARGETS; i++) {
        if (state->surfaceTargets[i].active) n++;
    }
    return n;
}

static int CountActiveLand(const GameState *state) {
    int n = 0;
    for (int i = 0; i < MAX_LAND_TARGETS; i++) {
        if (state->landTargets[i].active) n++;
    }
    return n;
}

static void TestTableInvariants(void) {
    CHECK(STAGE1_EVENT_COUNT > 0);
    CHECK(STAGE1_LENGTH_PX > 0);
    for (int i = 0; i < STAGE1_EVENT_COUNT; i++) {
        CHECK(STAGE1_EVENTS[i].px >= 0);
        CHECK(STAGE1_EVENTS[i].px < STAGE1_LENGTH_PX);
        CHECK(STAGE1_EVENTS[i].laneY > 0.0f);
        CHECK(STAGE1_EVENTS[i].laneY < (float)PLAY_HEIGHT);
        if (i > 0) CHECK(STAGE1_EVENTS[i - 1].px <= STAGE1_EVENTS[i].px);
    }
    for (int i = 0; i < STAGE1_TERRAIN_COUNT; i++) {
        CHECK(STAGE1_TERRAIN[i].widthPx > 0 && STAGE1_TERRAIN[i].heightPx > 0);
        CHECK(STAGE1_TERRAIN[i].y >= 0);
        CHECK(STAGE1_TERRAIN[i].y + STAGE1_TERRAIN[i].heightPx <= PLAY_HEIGHT);
    }
}

static void TestEventsFireOnceInOrder(void) {
    GameState state;
    ResetRunState(&state);
    CHECK(state.scrollDistance == 0.0f && state.stageCursor == 0 && !state.bossLock);

    // Advance just past the first event's distance: it fires, later ones don't.
    float firstPx = (float)STAGE1_EVENTS[0].px;
    UpdateStageScript(&state, (firstPx + 1.0f) / OCEAN_SCROLL_SPEED);
    CHECK(state.stageCursor >= 1);
    int cursorAfterFirst = state.stageCursor;
    if (cursorAfterFirst < STAGE1_EVENT_COUNT) {
        CHECK((float)STAGE1_EVENTS[cursorAfterFirst].px > state.scrollDistance);
    }
    CHECK(CountActiveAir(&state) + CountActiveSurface(&state) >= 1);

    // A zero-dt update fires nothing new: events are one-shot.
    UpdateStageScript(&state, 0.0f);
    CHECK(state.stageCursor == cursorAfterFirst);

    // Restart rewinds the whole script.
    ResetRunState(&state);
    CHECK(state.stageCursor == 0 && state.scrollDistance == 0.0f);
}

static void TestRelayGlyphSpawns(void) {
    // Find the map's Relay Node event and fire exactly that one.
    int relayIndex = -1;
    for (int i = 0; i < STAGE1_EVENT_COUNT; i++) {
        if (STAGE1_EVENTS[i].kind == STAGE_SPAWN_RELAY_NODE) {
            relayIndex = i;
            break;
        }
    }
    CHECK(relayIndex >= 0);
    if (relayIndex < 0) return;

    GameState state;
    ResetRunState(&state);
    state.stageCursor = relayIndex;
    state.scrollDistance = (float)STAGE1_EVENTS[relayIndex].px - 0.5f;
    UpdateStageScript(&state, 1.0f / OCEAN_SCROLL_SPEED);

    bool relaySpawned = false;
    for (int i = 0; i < MAX_SURFACE_TARGETS; i++) {
        if (state.surfaceTargets[i].active
            && state.surfaceTargets[i].type == SURFACE_TARGET_RELAY_NODE) {
            relaySpawned = true;
        }
    }
    CHECK(relaySpawned);
}

static void TestGroundRosterGlyphsSpawn(void) {
    // Mine and Mobile Platform map events spawn their enemies now that
    // the ground roster is implemented (their glyphs were skipped before).
    const StageSpawnKind kinds[] = { STAGE_SPAWN_MINE, STAGE_SPAWN_MOBILE_PLATFORM };
    const SurfaceTargetType types[] = { SURFACE_TARGET_MINE, SURFACE_TARGET_MOBILE_PLATFORM };
    for (int k = 0; k < 2; k++) {
        int index = -1;
        for (int i = 0; i < STAGE1_EVENT_COUNT; i++) {
            if (STAGE1_EVENTS[i].kind == kinds[k]) {
                index = i;
                break;
            }
        }
        CHECK(index >= 0);
        if (index < 0) continue;

        GameState state;
        ResetRunState(&state);
        state.stageCursor = index;
        state.scrollDistance = (float)STAGE1_EVENTS[index].px - 0.5f;
        UpdateStageScript(&state, 1.0f / OCEAN_SCROLL_SPEED);

        bool spawned = false;
        for (int i = 0; i < MAX_SURFACE_TARGETS; i++) {
            if (state.surfaceTargets[i].active && state.surfaceTargets[i].type == types[k]) {
                spawned = true;
            }
        }
        CHECK(spawned);
    }
}

static void TestAirRosterGlyphsSpawn(void) {
    // Interceptor and Gunship map events spawn their enemies now that the
    // air roster is implemented (their glyphs were skipped before).
    const StageSpawnKind kinds[] = { STAGE_SPAWN_INTERCEPTOR, STAGE_SPAWN_GUNSHIP };
    const AirTargetType types[] = { AIR_TARGET_INTERCEPTOR, AIR_TARGET_GUNSHIP };
    for (int k = 0; k < 2; k++) {
        int index = -1;
        for (int i = 0; i < STAGE1_EVENT_COUNT; i++) {
            if (STAGE1_EVENTS[i].kind == kinds[k]) {
                index = i;
                break;
            }
        }
        CHECK(index >= 0);
        if (index < 0) continue;

        GameState state;
        ResetRunState(&state);
        state.stageCursor = index;
        state.scrollDistance = (float)STAGE1_EVENTS[index].px - 0.5f;
        UpdateStageScript(&state, 1.0f / OCEAN_SCROLL_SPEED);

        bool spawned = false;
        for (int i = 0; i < MAX_AIR_TARGETS; i++) {
            if (state.airTargets[i].active && state.airTargets[i].type == types[k]) {
                spawned = true;
            }
        }
        CHECK(spawned);
    }
}

static void TestMineLeavesNoWreck(void) {
    // Mines detonate to nothing: neither death path leaves a wreck.
    ExplosionEffect explosions[MAX_EXPLOSION_EFFECTS] = { 0 };
    SurfaceWreck wrecks[MAX_SURFACE_WRECKS] = { 0 };
    GameEventQueue events = { 0 };
    PushGameEvent(&events, (GameEvent){
        .type = GAME_EVENT_SURFACE_TARGET_DESTROYED,
        .pos = { 80.0f, 80.0f },
        .target.surfaceTarget = SURFACE_TARGET_MINE
    });
    PushGameEvent(&events, (GameEvent){
        .type = GAME_EVENT_MINE_DETONATED,
        .pos = { 120.0f, 120.0f }
    });
    SpawnTargetDestructionEffects(&events, explosions, wrecks);
    CHECK(explosions[0].active && explosions[1].active);
    for (int i = 0; i < MAX_SURFACE_WRECKS; i++) CHECK(!wrecks[i].active);
}

static void TestRelayWreckRadius(void) {
    ExplosionEffect explosions[MAX_EXPLOSION_EFFECTS] = { 0 };
    SurfaceWreck wrecks[MAX_SURFACE_WRECKS] = { 0 };
    GameEventQueue events = { 0 };
    PushGameEvent(&events, (GameEvent){
        .type = GAME_EVENT_SURFACE_TARGET_DESTROYED,
        .pos = { 100.0f, 100.0f },
        .target.surfaceTarget = SURFACE_TARGET_RELAY_NODE
    });
    SpawnTargetDestructionEffects(&events, explosions, wrecks);
    CHECK(wrecks[0].active);
    CHECK(wrecks[0].radius == RELAY_NODE_RADIUS);
}

static void TestStage1IsletSetPiece(void) {
    // Stage 1 ships terrain (beat 7's islet), and the map keeps its
    // permanent-shield puzzle: at least one Casemate sits east of an
    // islet in its own lane, so its row approach is blocked for good
    // (terrain and targets drift together) and the counter-play is
    // positional - fly past the island and fire from beyond it.
    CHECK(STAGE1_TERRAIN_COUNT > 0);

    bool shielded = false;
    for (int e = 0; e < STAGE1_EVENT_COUNT && !shielded; e++) {
        if (STAGE1_EVENTS[e].kind != STAGE_SPAWN_CASEMATE) continue;
        for (int t = 0; t < STAGE1_TERRAIN_COUNT; t++) {
            const StageTerrainFootprint *land = &STAGE1_TERRAIN[t];
            bool laneOverlaps = STAGE1_EVENTS[e].laneY > (float)land->y
                && STAGE1_EVENTS[e].laneY < (float)(land->y + land->heightPx);
            if (laneOverlaps && land->px + land->widthPx <= STAGE1_EVENTS[e].px) shielded = true;
        }
    }
    CHECK(shielded);
}

static void TestBossLockFreezesScript(void) {
    GameState state;
    ResetRunState(&state);
    state.scrollDistance = (float)STAGE1_LENGTH_PX - 1.0f;
    state.stageCursor = STAGE1_EVENT_COUNT;

    UpdateStageScript(&state, 1.0f);
    CHECK(state.bossLock);
    // The script only raises the lock; the boss fight module owns
    // bossActive (and the music) from its first update.
    CHECK(!state.bossActive);

    float lockedDistance = state.scrollDistance;
    UpdateStageScript(&state, 1.0f);
    CHECK(state.scrollDistance == lockedDistance);
}

static void TestFormationPatterns(void) {
    AirTarget targets[MAX_AIR_TARGETS] = { 0 };
    CHECK(SpawnSkimmerDroneLine(targets, MAX_AIR_TARGETS, 100.0f) == 3);
    // Line members trail off-screen right at distinct x, same lane.
    CHECK(targets[0].pos.x < targets[1].pos.x && targets[1].pos.x < targets[2].pos.x);
    CHECK(targets[0].baseY == targets[1].baseY && targets[1].baseY == targets[2].baseY);

    AirTarget v[MAX_AIR_TARGETS] = { 0 };
    CHECK(SpawnSkimmerDroneV(v, MAX_AIR_TARGETS, 176.0f) == 5);
    // Leader anchored at the glyph lane, wings spread to both sides.
    CHECK(v[0].baseY == 176.0f);
    CHECK(v[1].baseY < 176.0f && v[2].baseY > 176.0f);

    // Formations near the play-area edge stay inside drone flight bounds.
    AirTarget edge[MAX_AIR_TARGETS] = { 0 };
    CHECK(SpawnSkimmerDroneV(edge, MAX_AIR_TARGETS, 16.0f) == 5);
    float margin = SKIMMER_DRONE_RADIUS + SKIMMER_DRONE_SINE_AMPLITUDE;
    for (int i = 0; i < 5; i++) {
        CHECK(edge[i].baseY >= margin);
        CHECK(edge[i].baseY <= PLAY_HEIGHT - margin);
    }

    // A full pool spawns fewer, not more.
    AirTarget tiny[2] = { 0 };
    CHECK(SpawnSkimmerDroneV(tiny, 2, 176.0f) == 2);
}

static void TestStageDescriptor(void) {
    CHECK(StageCount() >= 1);
    const StageDescriptor *stage = GetStageDescriptor(1);
    CHECK(stage != NULL);
    CHECK(stage->events == STAGE1_EVENTS && stage->eventCount == STAGE1_EVENT_COUNT);
    CHECK(stage->terrain == STAGE1_TERRAIN && stage->terrainCount == STAGE1_TERRAIN_COUNT);
    CHECK(stage->hardpoints == STAGE1_TERRAIN_HARDPOINTS
        && stage->hardpointCount == STAGE1_TERRAIN_HARDPOINT_COUNT);
    CHECK(stage->lengthPx == STAGE1_LENGTH_PX);
    const StageDescriptor *stage2 = GetStageDescriptor(2);
    CHECK(stage2 != NULL && stage2->events == STAGE2_EVENTS
        && stage2->terrain == STAGE2_TERRAIN && stage2->lengthPx == STAGE2_LENGTH_PX);
    // Out-of-range numbers clamp to real Stage 1 content rather than
    // failing, so the advance flow's wrap can never dereference nothing.
    const StageDescriptor *below = GetStageDescriptor(0);
    CHECK(below != NULL && below->events == STAGE1_EVENTS
        && below->lengthPx == STAGE1_LENGTH_PX);
    const StageDescriptor *above = GetStageDescriptor(99);
    CHECK(above != NULL && above->terrain == STAGE1_TERRAIN
        && above->eventCount == STAGE1_EVENT_COUNT);
}

static void TestNextStageNumberWraps(void) {
    // With N stages the CONTINUE sequence is 1, 2, ..., N, back to 1.
    // Written as a property over StageCount so it keeps holding when
    // Stage 2 raises the count.
    for (int s = 1; s <= StageCount(); s++) {
        int next = NextStageNumber(s);
        CHECK(next >= 1 && next <= StageCount());
        if (s < StageCount()) CHECK(next == s + 1);
        else CHECK(next == 1);
    }
    // Nonsense inputs recover to Stage 1 instead of walking off the table.
    CHECK(NextStageNumber(0) == 1);
    CHECK(NextStageNumber(-3) == 1);
    CHECK(NextStageNumber(99) == 1);
}

static void TestStageAwardLoadout(void) {
    // The stage table's awards are the single source of truth for both
    // the boss salvage and the devtools stage-select loadout.
    CHECK(GetStageDescriptor(1)->award == UPGRADE_AWARD_MORTAR);
    CHECK(GetStageDescriptor(2)->award == UPGRADE_AWARD_TARGETING_COMPUTER);

    GameState state;
    ResetRunState(&state);
    // Entering Stage 1 grants nothing.
    GrantUpgradesThroughStage(&state, 0);
    CHECK(!state.hasMortar && !state.hasTargetingComputer);
    // Entering Stage 2 holds Stage 1's salvage and nothing further.
    GrantUpgradesThroughStage(&state, 1);
    CHECK(state.hasMortar && !state.hasTargetingComputer);
    // A request past the table clamps instead of reading off the end.
    ResetRunState(&state);
    GrantUpgradesThroughStage(&state, 99);
    CHECK(state.hasMortar && state.hasTargetingComputer);

    // The shared primitive the salvage dock calls.
    ResetRunState(&state);
    ApplyUpgradeAward(&state, UPGRADE_AWARD_NONE);
    CHECK(!state.hasMortar && !state.hasTargetingComputer);
    ApplyUpgradeAward(&state, UPGRADE_AWARD_TARGETING_COMPUTER);
    CHECK(state.hasTargetingComputer && !state.hasMortar);
}

static void TestSkipToStageEnd(void) {
    // The --boss fast-forward: the lock rises on the next script update
    // with none of the skipped spawn events firing.
    GameState state;
    ResetRunState(&state);
    BeginStage(&state, 2);
    SkipToStageEnd(&state);
    UpdateStageScript(&state, 0.001f);
    CHECK(state.bossLock);
    CHECK(CountActiveAir(&state) + CountActiveSurface(&state) + CountActiveLand(&state) == 0);
}

static void TestStageScriptRunsAgainAfterAdvance(void) {
    GameState state;
    ResetRunState(&state);

    // Play "through" Stage 1: jump to the map end and raise the lock.
    state.scrollDistance = (float)STAGE1_LENGTH_PX;
    state.stageCursor = STAGE1_EVENT_COUNT;
    UpdateStageScript(&state, 0.001f);
    CHECK(state.bossLock);
    state.score = 900;
    state.hasMortar = true;
    state.stageClear = true;

    // The advance: exactly what the stage-clear CONTINUE input does.
    BeginStage(&state, NextStageNumber(state.stageNumber));
    CHECK(state.stageNumber == 2);
    CHECK(state.score == 900 && state.hasMortar);
    CHECK(!state.bossLock && !state.stageClear);
    CHECK(state.player.x == PLAYER_START_X && state.player.y == PLAYER_START_Y);
    CHECK(state.boss.phase == BOSS_PHASE_INACTIVE);

    // The rewound script fires the first event again...
    state.scrollDistance = (float)STAGE2_EVENTS[0].px - 0.5f;
    UpdateStageScript(&state, 1.0f / OCEAN_SCROLL_SPEED);
    CHECK(state.stageCursor >= 1);
    CHECK(CountActiveAir(&state) + CountActiveSurface(&state) + CountActiveLand(&state) > 0);

    // ...and the boss lock rises again at this stage's end.
    state.scrollDistance = (float)GetStageDescriptor(state.stageNumber)->lengthPx;
    UpdateStageScript(&state, 0.001f);
    CHECK(state.bossLock);
}

static void TestContinueRun(void) {
    GameState state;
    ResetRunState(&state);

    // Stage clear: the run continues into the next stage with progression
    // carried, announced by the restart event.
    state.score = 4200;
    state.hasMortar = true;
    state.stageClear = true;
    ContinueRun(&state);
    CHECK(state.stageNumber == 2);
    CHECK(state.score == 4200 && state.hasMortar);
    CHECK(!state.stageClear);
    CHECK(state.gameEvents.count == 1
        && state.gameEvents.items[0].type == GAME_EVENT_RUN_RESTARTED);

    // Game over: everything forfeits back to a fresh Stage 1 run.
    state.score = 4200;
    state.hasMortar = true;
    state.gameOver = true;
    state.gameEvents.count = 0;
    ContinueRun(&state);
    CHECK(state.score == 0 && !state.hasMortar && !state.gameOver);
    CHECK(state.lives == 3 && state.stageNumber == 1);
    CHECK(state.gameEvents.count == 1
        && state.gameEvents.items[0].type == GAME_EVENT_RUN_RESTARTED);
}

static void TestStageScriptClampsUnknownStageNumber(void) {
    // A stage number past the table (future save data, a bug) must play
    // Stage 1 content through the clamp, not read a missing descriptor.
    GameState state;
    ResetRunState(&state);
    state.stageNumber = 99;
    state.scrollDistance = (float)STAGE1_EVENTS[0].px - 0.5f;
    UpdateStageScript(&state, 1.0f / OCEAN_SCROLL_SPEED);
    CHECK(state.stageCursor >= 1);
    CHECK(CountActiveAir(&state) + CountActiveSurface(&state) > 0);
}

static void TestBeginStageCarriesRunProgression(void) {
    GameState state;
    ResetRunState(&state);
    CHECK(state.stageNumber == 1);

    state.score = 12345;
    state.lives = 2;
    state.hasMortar = true;
    state.scrollDistance = 500.0f;
    state.stageCursor = 7;
    state.bossLock = true;
    state.stageClear = true;
    state.surfaceTargets[0].active = true;

    BeginStage(&state, 2);
    CHECK(state.stageNumber == 2);
    // Run progression carries across the stage transition...
    CHECK(state.score == 12345 && state.lives == 2 && state.hasMortar);
    // ...while stage state rewinds like a fresh start.
    CHECK(state.scrollDistance == 0.0f && state.stageCursor == 0);
    CHECK(!state.bossLock && !state.stageClear);
    CHECK(!state.surfaceTargets[0].active);

    // A full reset forfeits everything back to a Stage 1 fresh run.
    ResetRunState(&state);
    CHECK(state.stageNumber == 1 && state.score == 0 && state.lives == 3);
    CHECK(!state.hasMortar);
}

int main(void) {
    TestStageDescriptor();
    TestNextStageNumberWraps();
    TestStageAwardLoadout();
    TestSkipToStageEnd();
    TestBeginStageCarriesRunProgression();
    TestContinueRun();
    TestStageScriptRunsAgainAfterAdvance();
    TestStageScriptClampsUnknownStageNumber();
    TestTableInvariants();
    TestEventsFireOnceInOrder();
    TestRelayGlyphSpawns();
    TestGroundRosterGlyphsSpawn();
    TestAirRosterGlyphsSpawn();
    TestMineLeavesNoWreck();
    TestRelayWreckRadius();
    TestStage1IsletSetPiece();
    TestBossLockFreezesScript();
    TestFormationPatterns();

    if (failures > 0) {
        fprintf(stderr, "%d stage test failure(s)\n", failures);
        return 1;
    }
    printf("stage tests passed\n");
    return 0;
}
