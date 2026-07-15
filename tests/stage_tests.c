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

static void TestBossLockFreezesScript(void) {
    GameState state;
    ResetRunState(&state);
    state.scrollDistance = (float)STAGE1_LENGTH_PX - 1.0f;
    state.stageCursor = STAGE1_EVENT_COUNT;

    UpdateStageScript(&state, 1.0f);
    CHECK(state.bossLock);
    CHECK(state.bossActive);

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

int main(void) {
    TestTableInvariants();
    TestEventsFireOnceInOrder();
    TestRelayGlyphSpawns();
    TestGroundRosterGlyphsSpawn();
    TestAirRosterGlyphsSpawn();
    TestMineLeavesNoWreck();
    TestRelayWreckRadius();
    TestBossLockFreezesScript();
    TestFormationPatterns();

    if (failures > 0) {
        fprintf(stderr, "%d stage test failure(s)\n", failures);
        return 1;
    }
    printf("stage tests passed\n");
    return 0;
}
