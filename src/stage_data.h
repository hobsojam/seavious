#ifndef SEAVIOUS_STAGE_DATA_H
#define SEAVIOUS_STAGE_DATA_H

// Compiled stage data: what a stage map (assets/stages/*.txt) compiles
// down to via tools/compile-stage.py. The generated tables live in
// stage*_data.c and are committed; tests/test_stage_assets.py keeps map
// and table from drifting. The spawn-event runner that consumes this at
// runtime is still to come (see TODO).

typedef enum {
    STAGE_SPAWN_SKIMMER_DRONE,
    STAGE_SPAWN_DRONE_LINE3,
    STAGE_SPAWN_DRONE_V5,
    STAGE_SPAWN_INTERCEPTOR,
    STAGE_SPAWN_GUNSHIP,
    STAGE_SPAWN_CASEMATE,
    STAGE_SPAWN_TRACKING_TURRET,
    STAGE_SPAWN_RELAY_NODE,
    STAGE_SPAWN_MINE,
    STAGE_SPAWN_MOBILE_PLATFORM
} StageSpawnType;

// One map glyph. x/y are the glyph cell's center in world coordinates
// (world x = scroll distance; y spans the play area). The event fires
// when its x column scrolls to the right screen edge: surface types
// spawn anchored at that world position, air types enter at lane y.
typedef struct {
    float x;
    float y;
    StageSpawnType type;
} StageSpawnEvent;

// Axis-aligned land footprint in world coordinates. Non-colliding for
// the ship and gun; blocks torpedoes (armed shots detonate at its west
// edge, the reticle clamps there).
typedef struct {
    float x;
    float y;
    float width;
    float height;
} StageTerrainRect;

typedef struct {
    const StageSpawnEvent *events;   // sorted by x
    int eventCount;
    const StageTerrainRect *terrain;
    int terrainCount;
    float length;  // total scroll distance in px; the boss lock fires here
} StageData;

extern const StageData stage1Data;

#endif
