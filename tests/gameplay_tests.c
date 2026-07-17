#include "gameplay.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\\n", __FILE__, __LINE__, #condition); \
        failures++; \
    } \
} while (0)

#define NEAR(actual, expected) CHECK(fabsf((actual) - (expected)) < 0.01f)

static void TestDamageAndScore(void) {
    AirTarget air = { .type = AIR_TARGET_SKIMMER_DRONE, .pos = { 4, 8 }, .hp = 1, .active = true };
    SurfaceTarget surface = { .type = SURFACE_TARGET_CASEMATE, .pos = { 9, 3 }, .hp = 1, .active = true };
    GameEventQueue events = { 0 };

    CHECK(!DamageAirTarget(NULL, 1, &events));
    CHECK(!DamageAirTarget(&air, 0, &events));
    CHECK(DamageAirTarget(&air, 1, &events));
    CHECK(!air.active && events.count == 1);
    CHECK(DamageSurfaceTarget(&surface, 1, &events));
    CHECK(ScoreGameEvents(&events) == SCORE_SKIMMER_DRONE + SCORE_CASEMATE);
    CHECK(ScoreGameEvents(NULL) == 0);
}

static void TestMovementAndProjectiles(void) {
    Vector2 player = { 0, PLAY_HEIGHT + 20 };
    MovePlayer(&player, -1, 1, 10, 1, 5, 5);
    NEAR(player.x, 5); NEAR(player.y, PLAY_HEIGHT - 5);
    NEAR(AdvanceOceanScroll(95, 1, 100), 35);

    WakeParticle wake[1] = { 0 };
    CHECK(TryEmitWakeParticle(wake, 1, (Vector2){ 10, 4 }));
    CHECK(!TryEmitWakeParticle(wake, 1, (Vector2){ 0, 0 }));
    UpdateWakeParticles(wake, 1, WAKE_LIFETIME);
    CHECK(!wake[0].active);

    Bullet bullets[1] = { 0 };
    CHECK(TrySpawnBullet(bullets, 1, (Vector2){ 10, 3 }));
    UpdateBullets(bullets, 1, 2);
    CHECK(!bullets[0].active);

    EnemyBullet enemy[1] = { 0 };
    CHECK(TrySpawnEnemyBullet(enemy, 1, (Vector2){ 4, 4 }, (Vector2){ 2, 0 }));
    UpdateEnemyBullets(enemy, 1, 1);
    CHECK(enemy[0].active);
    CHECK(ResolveEnemyBulletPlayerCollision(enemy, 1, (Vector2){ 6, 4 }, 2));
    CHECK(!enemy[0].active);
}

static void TestAirTargets(void) {
    AirTarget targets[1] = { 0 };
    CHECK(TrySpawnSkimmerDrone(targets, 1, 42));
    CHECK(targets[0].active && targets[0].hp == SKIMMER_DRONE_HP);
    UpdateAirTargets(targets, 1, 1);
    CHECK(targets[0].pos.x < GAME_WIDTH + SKIMMER_DRONE_RADIUS);

    Bullet bullet = { .pos = targets[0].pos, .active = true };
    GameEventQueue events = { 0 };
    ResolveBulletAirTargetCollisions(&bullet, 1, targets, 1, &events);
    CHECK(!bullet.active && !targets[0].active && events.count == 1);

    AirTarget contact = { .pos = { 5, 5 }, .radius = 2, .active = true };
    CHECK(ResolvePlayerContactDamage((Vector2){ 5, 5 }, 2, &contact, 1, NULL, 0, NULL));
    CHECK(!ResolvePlayerContactDamage((Vector2){ 100, 100 }, 1, &contact, 1, NULL, 0, NULL));
}

static void TestInterceptor(void) {
    AirTarget targets[1] = { 0 };
    EnemyBullet bullets[2] = { 0 };
    CHECK(TrySpawnInterceptor(targets, 1, 100.0f));
    CHECK(targets[0].type == AIR_TARGET_INTERCEPTOR && targets[0].hp == INTERCEPTOR_HP);

    // Straight lane flight: faster than a drone, no sine wobble.
    float startX = targets[0].pos.x;
    UpdateAirTargets(targets, 1, 1.0f);
    NEAR(startX - targets[0].pos.x, INTERCEPTOR_SPEED);
    NEAR(targets[0].pos.y, 100.0f);

    // Holds fire until it reaches mid-screen...
    GameEventQueue fireEvents = { 0 };
    targets[0].pos.x = INTERCEPTOR_FIRE_X + 5.0f;
    UpdateAirTargetFire(targets, 1, 0.016f, (Vector2){ 48, 100 }, bullets, 2, &fireEvents);
    CHECK(!bullets[0].active);
    CHECK(fireEvents.count == 0);

    // ...then fires exactly one shot aimed at the player, at double the
    // shared projectile speed, announcing itself with one fire event.
    targets[0].pos.x = INTERCEPTOR_FIRE_X - 1.0f;
    UpdateAirTargetFire(targets, 1, 0.016f, (Vector2){ 48, 100 }, bullets, 2, &fireEvents);
    CHECK(bullets[0].active);
    NEAR(bullets[0].vel.x, -INTERCEPTOR_SHOT_SPEED);
    NEAR(bullets[0].vel.y, 0.0f);
    CHECK(fireEvents.count == 1 && fireEvents.items[0].type == GAME_EVENT_ENEMY_FIRED);
    UpdateAirTargetFire(targets, 1, 10.0f, (Vector2){ 48, 100 }, bullets, 2, &fireEvents);
    CHECK(!bullets[1].active);
    CHECK(fireEvents.count == 1);

    // A player below the lane pulls the aim downward.
    AirTarget aimed[1] = { 0 };
    EnemyBullet aimedBullets[1] = { 0 };
    CHECK(TrySpawnInterceptor(aimed, 1, 100.0f));
    aimed[0].pos.x = INTERCEPTOR_FIRE_X - 1.0f;
    UpdateAirTargetFire(aimed, 1, 0.016f, (Vector2){ 48, 200 }, aimedBullets, 1, NULL);
    CHECK(aimedBullets[0].active);
    CHECK(aimedBullets[0].vel.x < 0 && aimedBullets[0].vel.y > 0);
    float shotSpeed = sqrtf(aimedBullets[0].vel.x * aimedBullets[0].vel.x
        + aimedBullets[0].vel.y * aimedBullets[0].vel.y);
    NEAR(shotSpeed, INTERCEPTOR_SHOT_SPEED);

    GameEventQueue events = { 0 };
    CHECK(DamageAirTarget(&targets[0], 1, &events));
    CHECK(ScoreGameEvents(&events) == SCORE_INTERCEPTOR);
}

static void TestGunship(void) {
    AirTarget targets[1] = { 0 };
    EnemyBullet bullets[8] = { 0 };
    GameEventQueue events = { 0 };
    CHECK(TrySpawnGunship(targets, 1, 150.0f));
    CHECK(targets[0].type == AIR_TARGET_GUNSHIP && targets[0].hp == GUNSHIP_HP);

    // Half the Interceptor's pace, holding its spawn lane.
    float startX = targets[0].pos.x;
    UpdateAirTargets(targets, 1, 1.0f);
    NEAR(startX - targets[0].pos.x, GUNSHIP_SPEED);
    NEAR(targets[0].pos.y, 150.0f);

    // Right of the activation line: held pre-armed, no volley.
    GameEventQueue fireEvents = { 0 };
    targets[0].pos.x = (float)GAME_WIDTH + 2.0f;
    UpdateAirTargetFire(targets, 1, GUNSHIP_FIRE_INTERVAL + 1.0f, (Vector2){ 100, 150 }, bullets, 8, &fireEvents);
    for (int i = 0; i < 8; i++) CHECK(!bullets[i].active);
    CHECK(fireEvents.count == 0);

    // Crossing the line releases the 3-way spread aimed at the player
    // immediately (pre-armed), not one interval later. The whole fan is
    // one fire event, not three.
    targets[0].pos.x = 400.0f;
    UpdateAirTargetFire(targets, 1, 0.01f, (Vector2){ 100, 150 }, bullets, 8, &fireEvents);
    int fired = 0;
    for (int i = 0; i < 8; i++) {
        if (bullets[i].active) fired++;
    }
    CHECK(fired == 3);
    CHECK(fireEvents.count == 1 && fireEvents.items[0].type == GAME_EVENT_ENEMY_FIRED);
    CHECK(bullets[0].vel.x < 0 && bullets[1].vel.x < 0 && bullets[2].vel.x < 0);
    // Outer shots straddle the center shot, one to each side of the lane.
    CHECK(bullets[0].vel.y * bullets[2].vel.y < -1.0f);
    NEAR(bullets[1].vel.y, 0.0f);

    // First multi-hit air target: survives two gun hits, dies to the third.
    CHECK(!DamageAirTarget(&targets[0], 1, &events));
    CHECK(!DamageAirTarget(&targets[0], 1, &events));
    CHECK(targets[0].active && events.count == 0);
    CHECK(DamageAirTarget(&targets[0], 1, &events));
    CHECK(ScoreGameEvents(&events) == SCORE_GUNSHIP);
}

static void TestTorpedoes(void) {
    Torpedo torpedo = { 0 };
    Vector2 spawn = { 10, 20 };
    FireFixedRangeTorpedo(&torpedo, spawn, NULL, 0, 0.0f);
    CHECK(torpedo.active && torpedo.target.x > spawn.x);
    TorpedoImpact impact = UpdateTorpedo(&torpedo, 1);
    CHECK(!torpedo.active && impact.type == TORPEDO_IMPACT_EXPLOSION);

    SurfaceTarget target = { .pos = { 100, 30 }, .active = true };
    FireLeadTorpedo(&torpedo, spawn, &target, 1);
    CHECK(torpedo.active && torpedo.vel.x > 0);
    torpedo.active = true; torpedo.armed = false; torpedo.pos = target.pos;
    target.hp = 1; target.radius = 10;
    GameEventQueue events = { 0 };
    impact = ResolveTorpedoSurfaceTargetCollision(&torpedo, &target, 1, &events);
    CHECK(impact.type == TORPEDO_IMPACT_DIRECT && !target.active && events.count == 1);
}

static void TestTerrain(void) {
    // Screen mapping shares the ground-glyph clock: the footprint's left
    // edge sits at the right screen edge when scroll reaches its px, and
    // has drifted one full screen-width left one screen of scroll later.
    StageTerrainFootprint land = { 1000, 96, 64, 32 };
    Rectangle rect = TerrainScreenRect(land, 1000.0f);
    NEAR(rect.x, (float)GAME_WIDTH);
    NEAR(rect.y, 96.0f); NEAR(rect.width, 64.0f); NEAR(rect.height, 32.0f);
    rect = TerrainScreenRect(land, 1000.0f + (float)GAME_WIDTH);
    NEAR(rect.x, 0.0f);

    // Land left edge on screen at x=200 for the checks below.
    float scroll = 1000.0f + (float)GAME_WIDTH - 200.0f;

    // Reticle clamps to the first land edge in the lane; an adjacent
    // clear lane keeps full range; land fully behind the spawn point is
    // ignored (firing from beyond the island is the counter-play); a
    // spawn over land clamps to the spawn itself.
    Vector2 blocked = CalculateTorpedoReticle((Vector2){ 100, 112 }, &land, 1, scroll);
    NEAR(blocked.x, 200.0f);
    Vector2 clearLane = CalculateTorpedoReticle((Vector2){ 100, 200 }, &land, 1, scroll);
    NEAR(clearLane.x, 100.0f + TORPEDO_MAX_RANGE);
    Vector2 pastLand = CalculateTorpedoReticle((Vector2){ 300, 112 }, &land, 1, scroll);
    NEAR(pastLand.x, 300.0f + TORPEDO_MAX_RANGE);
    Vector2 overLand = CalculateTorpedoReticle((Vector2){ 220, 112 }, &land, 1, scroll);
    NEAR(overLand.x, 220.0f);

    // A torpedo whose nose crosses the edge unarmed fizzles: splash-less
    // DIRECT impact at the land edge, torpedo gone.
    Torpedo torpedo = { 0 };
    torpedo.active = true;
    torpedo.pos = (Vector2){ 199.0f, 112.0f };
    TorpedoImpact impact = ResolveTorpedoTerrainCollision(&torpedo, &land, 1, scroll);
    CHECK(impact.type == TORPEDO_IMPACT_DIRECT && !torpedo.active);
    NEAR(impact.pos.x, 200.0f);

    // Armed, the same crossing detonates (the caller resolves splash).
    torpedo.active = true;
    torpedo.armed = true;
    torpedo.pos = (Vector2){ 205.0f, 112.0f };
    impact = ResolveTorpedoTerrainCollision(&torpedo, &land, 1, scroll);
    CHECK(impact.type == TORPEDO_IMPACT_EXPLOSION && !torpedo.active);
    NEAR(impact.pos.x, 205.0f);

    // A clear adjacent lane lets the torpedo run on untouched.
    torpedo.active = true;
    torpedo.pos = (Vector2){ 205.0f, 60.0f };
    impact = ResolveTorpedoTerrainCollision(&torpedo, &land, 1, scroll);
    CHECK(impact.type == TORPEDO_IMPACT_NONE && torpedo.active);

    // Firing with the reticle clamped to the edge: the torpedo reaches
    // its shortened target and detonates armed right at the land edge.
    Torpedo fired = { 0 };
    FireFixedRangeTorpedo(&fired, (Vector2){ 100, 112 }, &land, 1, scroll);
    NEAR(fired.target.x, 200.0f);
    impact = UpdateTorpedo(&fired, 1.0f);
    CHECK(impact.type == TORPEDO_IMPACT_EXPLOSION);
}

static void TestPlayerMortar(void) {
    // The mortar reticle ignores land outright: the same footprint that
    // clamps the torpedo lane leaves the mortar's fixed range untouched.
    StageTerrainFootprint land = { 1000, 96, 64, 32 };
    float scroll = 1200.0f; // land on screen at x = 312..376
    Vector2 spawn = { 250.0f, 112.0f };
    Vector2 torpedoReticle = CalculateTorpedoReticle(spawn, &land, 1, scroll);
    NEAR(torpedoReticle.x, 312.0f);
    Vector2 mortarReticle = CalculateMortarReticle(spawn);
    NEAR(mortarReticle.x, spawn.x + PLAYER_MORTAR_MAX_RANGE);
    NEAR(mortarReticle.y, spawn.y);

    // Only the screen margin caps the range.
    Vector2 nearEdge = CalculateMortarReticle((Vector2){ (float)GAME_WIDTH - 40.0f, 100.0f });
    NEAR(nearEdge.x, (float)GAME_WIDTH - TORPEDO_RETICLE_SCREEN_MARGIN);

    // Flight: the water-anchored target drifts with scrollDt, the shell
    // reports its landing exactly once, blasts, then expires.
    MortarShell shell = { 0 };
    FirePlayerMortar(&shell, spawn, mortarReticle);
    CHECK(shell.active && !shell.landed);
    Vector2 shadow = CalculateMortarGroundPosition(&shell, PLAYER_MORTAR_AIR_TIME);
    NEAR(shadow.x, spawn.x);
    NEAR(shadow.y, spawn.y);
    CHECK(!UpdatePlayerMortarShell(&shell, PLAYER_MORTAR_AIR_TIME / 2.0f, 1.0f));
    NEAR(shell.target.x, mortarReticle.x - OCEAN_SCROLL_SPEED);
    shadow = CalculateMortarGroundPosition(&shell, PLAYER_MORTAR_AIR_TIME);
    NEAR(shadow.x, (spawn.x + shell.target.x) / 2.0f);
    NEAR(shadow.y, (spawn.y + shell.target.y) / 2.0f);
    CHECK(UpdatePlayerMortarShell(&shell, PLAYER_MORTAR_AIR_TIME / 2.0f, 0.0f));
    CHECK(shell.landed);
    NEAR(shell.blastT, PLAYER_MORTAR_BLAST_DURATION);
    CHECK(!UpdatePlayerMortarShell(&shell, PLAYER_MORTAR_BLAST_DURATION, 0.0f));
    CHECK(!shell.active);
    CHECK(!UpdatePlayerMortarShell(&shell, 1.0f, 0.0f));

    // Blast: area damage against surface targets inside the radius only
    // (the provisional water-class rule that playtesting will confirm).
    SurfaceTarget targets[2] = {
        { .type = SURFACE_TARGET_CASEMATE, .pos = { 100.0f, 100.0f },
          .radius = CASEMATE_RADIUS, .hp = 1, .active = true },
        { .type = SURFACE_TARGET_CASEMATE,
          .pos = { 100.0f + PLAYER_MORTAR_BLAST_RADIUS + CASEMATE_RADIUS + 5.0f, 100.0f },
          .radius = CASEMATE_RADIUS, .hp = 1, .active = true },
    };
    GameEventQueue events = { 0 };
    ResolveMortarBlastSurfaceTargets((Vector2){ 100.0f, 100.0f }, targets, 2, &events);
    CHECK(!targets[0].active);
    CHECK(targets[1].active);
    CHECK(events.count == 1);
}

static void TestMortarBattery(void) {
    LandTarget land[1] = { 0 };
    GameEventQueue events = { 0 };
    CHECK(TrySpawnMortarBattery(land, 1, 120.0f));
    CHECK(land[0].type == LAND_TARGET_MORTAR_BATTERY && land[0].hp == MORTAR_BATTERY_HP);
    CHECK(land[0].pos.x == (float)GAME_WIDTH + 16.0f);

    // Anchored drift at scroll speed (glued to its terrain cell).
    float startX = land[0].pos.x;
    UpdateLandTargets(land, 1, 1.0f);
    NEAR(startX - land[0].pos.x, OCEAN_SCROLL_SPEED);

    // Right of the activation line: held pre-armed, no lob.
    land[0].pos = (Vector2){ (float)GAME_WIDTH + 5.0f, 120.0f };
    UpdateMortarBatteries(land, 1, MORTAR_BATTERY_FIRE_INTERVAL * 2.0f, (Vector2){ 100, 200 }, &events);
    CHECK(!land[0].shell.active && events.count == 0);

    // Crossing the line lobs immediately at the player: one fire event.
    land[0].pos = (Vector2){ 400.0f, 120.0f };
    UpdateMortarBatteries(land, 1, 0.01f, (Vector2){ 100, 200 }, &events);
    CHECK(land[0].shell.active && !land[0].shell.landed);
    NEAR(land[0].shell.target.x, 100.0f);
    NEAR(land[0].shell.target.y, 200.0f);
    CHECK(events.count == 1 && events.items[0].type == GAME_EVENT_MORTAR_FIRED);

    // A full interval later the shell has landed (one blast event), and
    // no second shell double-lobbed behind it.
    events.count = 0;
    UpdateMortarBatteries(land, 1, MORTAR_BATTERY_FIRE_INTERVAL, (Vector2){ 100, 200 }, &events);
    CHECK(land[0].shell.landed);
    CHECK(events.count == 1 && events.items[0].type == GAME_EVENT_MORTAR_BLAST);

    // The landed blast kills a player standing in it, misses one outside.
    CHECK(ResolveLandMortarBlastPlayerHit(land, 1, (Vector2){ 100, 200 }, PLAYER_HIT_RADIUS));
    CHECK(!ResolveLandMortarBlastPlayerHit(land, 1, (Vector2){ 300, 60 }, PLAYER_HIT_RADIUS));

    // Blast expiry frees the tube; the banked interval relobs at once.
    events.count = 0;
    UpdateMortarBatteries(land, 1, LAND_MORTAR_BLAST_DURATION, (Vector2){ 100, 200 }, &events);
    CHECK(land[0].shell.active && !land[0].shell.landed);
    CHECK(events.count == 1 && events.items[0].type == GAME_EVENT_MORTAR_FIRED);
}

static void TestLandTargetDeathAndShellPersistence(void) {
    LandTarget land[1] = { 0 };
    GameEventQueue events = { 0 };
    CHECK(TrySpawnMortarBattery(land, 1, 100.0f));
    land[0].pos = (Vector2){ 300.0f, 100.0f };
    land[0].fireTimer = MORTAR_BATTERY_FIRE_INTERVAL;
    UpdateMortarBatteries(land, 1, 0.001f, (Vector2){ 80, 90 }, &events);
    CHECK(land[0].shell.active);

    // The battery dies (scoring once); its shell keeps falling.
    events.count = 0;
    CHECK(DamageLandTarget(&land[0], 1, &events));
    CHECK(!land[0].active);
    CHECK(events.count == 1 && events.items[0].type == GAME_EVENT_LAND_TARGET_DESTROYED);
    CHECK(ScoreGameEvents(&events) == SCORE_MORTAR_BATTERY);
    events.count = 0;
    UpdateMortarBatteries(land, 1, LAND_MORTAR_AIR_TIME, (Vector2){ 80, 90 }, &events);
    CHECK(land[0].shell.landed);
    CHECK(events.count == 1 && events.items[0].type == GAME_EVENT_MORTAR_BLAST);

    // Pool reuse prefers a slot with no live shell; when only shell-
    // carrying slots are free, the newcomer inherits the shell untouched.
    LandTarget pool[2] = { 0 };
    pool[0].shell.active = true;
    CHECK(TrySpawnDroneBunker(pool, 2, 90.0f));
    CHECK(pool[1].active && !pool[0].active);
    CHECK(TrySpawnMortarBattery(pool, 2, 60.0f)); // only slot 0 left
    CHECK(pool[0].active && pool[0].shell.active);
}

static void TestDroneBunker(void) {
    LandTarget land[1] = { 0 };
    AirTarget air[4] = { 0 };
    GameEventQueue events = { 0 };
    CHECK(TrySpawnDroneBunker(land, 1, 200.0f));
    CHECK(land[0].type == LAND_TARGET_DRONE_BUNKER && land[0].hp == DRONE_BUNKER_HP);
    CHECK(land[0].pos.x == (float)GAME_WIDTH + 16.0f);

    // Pre-armed hold right of the line: intervals elapse, nothing hatches.
    land[0].pos = (Vector2){ (float)GAME_WIDTH + 5.0f, 200.0f };
    UpdateDroneBunkerLaunches(land, 1, DRONE_BUNKER_LAUNCH_INTERVAL * 2.0f, air, 4, &events);
    CHECK(events.count == 0);

    // First drone right at the line; owner ids sit past the surface
    // pool's relay range so the two namespaces can't collide.
    land[0].pos = (Vector2){ 400.0f, 200.0f };
    UpdateDroneBunkerLaunches(land, 1, 0.01f, air, 4, &events);
    CHECK(events.count == 1 && events.items[0].type == GAME_EVENT_DRONE_LAUNCHED);
    int owned = 0;
    for (int i = 0; i < 4; i++) {
        if (!air[i].active) continue;
        CHECK(air[i].ownerId == 1 + MAX_SURFACE_TARGETS);
        owned++;
    }
    CHECK(owned == 1);

    // The cap holds at DRONE_BUNKER_MAX_DRONES...
    for (int burst = 0; burst < 5; burst++) {
        UpdateDroneBunkerLaunches(land, 1, DRONE_BUNKER_LAUNCH_INTERVAL, air, 4, &events);
    }
    owned = 0;
    for (int i = 0; i < 4; i++) {
        if (air[i].active) owned++;
    }
    CHECK(owned == DRONE_BUNKER_MAX_DRONES);
    // ...and a freed slot refills immediately (held at the interval).
    air[0].active = false;
    UpdateDroneBunkerLaunches(land, 1, 0.0f, air, 4, &events);
    owned = 0;
    for (int i = 0; i < 4; i++) {
        if (air[i].active) owned++;
    }
    CHECK(owned == DRONE_BUNKER_MAX_DRONES);

    // Dies to one player mortar blast; a blast outside the radius misses.
    GameEventQueue killEvents = { 0 };
    ResolveMortarBlastLandTargets(land[0].pos, land, 1, &killEvents);
    CHECK(!land[0].active);
    CHECK(ScoreGameEvents(&killEvents) == SCORE_DRONE_BUNKER);
    LandTarget spared[1] = { 0 };
    CHECK(TrySpawnMortarBattery(spared, 1, 60.0f));
    spared[0].pos = (Vector2){ 100.0f, 60.0f };
    ResolveMortarBlastLandTargets(
        (Vector2){ 100.0f + PLAYER_MORTAR_BLAST_RADIUS + MORTAR_BATTERY_RADIUS + 5.0f, 60.0f },
        spared, 1, &killEvents);
    CHECK(spared[0].active);
}

static void TestSurfaceTargets(void) {
    SurfaceTarget targets[2] = { 0 };
    CHECK(TrySpawnCasemate(targets, 2, 30));
    CHECK(TrySpawnTrackingTurret(targets, 2, 40));
    CHECK(targets[0].type == SURFACE_TARGET_CASEMATE);
    CHECK(targets[1].type == SURFACE_TARGET_TRACKING_TURRET);

    EnemyBullet bullets[4] = { 0 };
    // Past the activation line, one interval of dt fires both.
    targets[0].pos.x = 200.0f;
    targets[1].pos.x = 200.0f;
    GameEventQueue fireEvents = { 0 };
    UpdateSurfaceTargetFire(targets, 2, 2, (Vector2){ 100, 50 }, (Vector2){ 2, 0 }, bullets, 4, &fireEvents);
    CHECK(bullets[0].active);
    // One fire event per installation that shot.
    CHECK(fireEvents.count == 2 && fireEvents.items[0].type == GAME_EVENT_ENEMY_FIRED);
    UpdateSurfaceTargets(targets, 2, 1);
    CHECK(targets[0].pos.x < GAME_WIDTH + CASEMATE_RADIUS);

    targets[0].hp = 1; targets[0].active = true; targets[0].pos = (Vector2){ 10, 10 };
    targets[1].hp = 1; targets[1].active = true; targets[1].pos = (Vector2){ 100, 100 };
    GameEventQueue events = { 0 };
    ResolveTorpedoExplosion((Vector2){ 10, 10 }, targets, 2, &events);
    CHECK(!targets[0].active && targets[1].active && events.count == 1);
}

static void TestRelayNode(void) {
    SurfaceTarget surface[MAX_SURFACE_TARGETS] = { 0 };
    AirTarget air[MAX_AIR_TARGETS] = { 0 };
    EnemyBullet bullets[MAX_ENEMY_BULLETS] = { 0 };
    GameEventQueue events = { 0 };

    CHECK(TrySpawnRelayNode(surface, MAX_SURFACE_TARGETS, 100.0f));
    CHECK(surface[0].type == SURFACE_TARGET_RELAY_NODE);

    // Right of the activation line (spawns beyond the right edge):
    // interval elapses, no launch.
    UpdateRelayNodeLaunches(surface, MAX_SURFACE_TARGETS, RELAY_NODE_LAUNCH_INTERVAL + 0.1f,
        air, MAX_AIR_TARGETS, &events);
    CHECK(air[0].active == false && events.count == 0);

    // Past the activation line: the held timer launches immediately.
    surface[0].pos.x = 200.0f;
    UpdateRelayNodeLaunches(surface, MAX_SURFACE_TARGETS, 0.01f, air, MAX_AIR_TARGETS, &events);
    CHECK(air[0].active && air[0].ownerId == 1 && events.count == 1);
    CHECK(events.items[0].type == GAME_EVENT_DRONE_LAUNCHED);
    // Launched drones start at the relay, not the screen edge.
    NEAR(air[0].pos.x, 200.0f);
    NEAR(air[0].baseY, 100.0f);

    // Cap: at most RELAY_NODE_MAX_DRONES of its drones alive at once.
    for (int i = 0; i < 5; i++) {
        UpdateRelayNodeLaunches(surface, MAX_SURFACE_TARGETS, RELAY_NODE_LAUNCH_INTERVAL + 0.1f,
            air, MAX_AIR_TARGETS, &events);
    }
    int owned = 0;
    for (int i = 0; i < MAX_AIR_TARGETS; i++) {
        if (air[i].active && air[i].ownerId == 1) owned++;
    }
    CHECK(owned == RELAY_NODE_MAX_DRONES);

    // A freed slot refills immediately (timer held at the interval).
    air[0].active = false;
    UpdateRelayNodeLaunches(surface, MAX_SURFACE_TARGETS, 0.01f, air, MAX_AIR_TARGETS, &events);
    owned = 0;
    for (int i = 0; i < MAX_AIR_TARGETS; i++) {
        if (air[i].active && air[i].ownerId == 1) owned++;
    }
    CHECK(owned == RELAY_NODE_MAX_DRONES);

    // Relays never fire enemy bullets (and so make no fire sound).
    GameEventQueue fireEvents = { 0 };
    UpdateSurfaceTargetFire(surface, MAX_SURFACE_TARGETS, 10.0f, (Vector2){ 0, 0 }, (Vector2){ 0, 0 },
        bullets, MAX_ENEMY_BULLETS, &fireEvents);
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) CHECK(!bullets[i].active);
    CHECK(fireEvents.count == 0);

    // Destroying a relay scores it; its drones stay alive (only the
    // reinforcement source is cut off).
    GameEventQueue killEvents = { 0 };
    CHECK(DamageSurfaceTarget(&surface[0], 1, &killEvents));
    CHECK(ScoreGameEvents(&killEvents) == SCORE_RELAY_NODE);
    owned = 0;
    for (int i = 0; i < MAX_AIR_TARGETS; i++) {
        if (air[i].active && air[i].ownerId == 1) owned++;
    }
    CHECK(owned == RELAY_NODE_MAX_DRONES);
}

static void TestMine(void) {
    SurfaceTarget mines[2] = { 0 };
    EnemyBullet bullets[4] = { 0 };
    GameEventQueue events = { 0 };

    CHECK(TrySpawnMine(mines, 2, 120.0f));
    CHECK(mines[0].type == SURFACE_TARGET_MINE && mines[0].hp == MINE_HP);

    // Mines never shoot.
    UpdateSurfaceTargetFire(mines, 2, 10.0f, (Vector2){ 0, 0 }, (Vector2){ 0, 0 }, bullets, 4, NULL);
    for (int i = 0; i < 4; i++) CHECK(!bullets[i].active);

    // The fuse trips before contact: it creates a no-score detonation
    // event and the resulting splash catches a nearby player.
    mines[0].pos = (Vector2){ 50.0f, 120.0f };
    CHECK(DetonateNearbyMines(mines, 2,
        (Vector2){ 50.0f + MINE_PROXIMITY_RADIUS, 120.0f }, 2.0f, &events));
    CHECK(!mines[0].active);
    CHECK(events.count == 1 && events.items[0].type == GAME_EVENT_MINE_DETONATED);
    CHECK(ScoreGameEvents(&events) == 0);

    MineBlast blasts[1] = { 0 };
    SpawnMineBlastsFromEvents(blasts, 1, &events);
    CHECK(blasts[0].active);
    CHECK(ResolveMineBlastPlayerHit(blasts, 1, (Vector2){ 50.0f, 120.0f }, 2.0f));
    CHECK(!ResolveMineBlastPlayerHit(blasts, 1,
        (Vector2){ 50.0f + MINE_BLAST_RADIUS + 3.0f, 120.0f }, 2.0f));
    UpdateMineBlasts(blasts, 1, MINE_BLAST_DURATION);
    CHECK(!blasts[0].active);

    // A torpedoed mine still scores, but its destroyed event also creates
    // the same blast hazard.
    GameEventQueue killEvents = { 0 };
    CHECK(TrySpawnMine(mines, 2, 60.0f));
    CHECK(DamageSurfaceTarget(&mines[0], 1, &killEvents));
    CHECK(ScoreGameEvents(&killEvents) == SCORE_MINE);
    SpawnMineBlastsFromEvents(blasts, 1, &killEvents);
    CHECK(blasts[0].active);
}

static void TestMobilePlatform(void) {
    SurfaceTarget targets[2] = { 0 };
    EnemyBullet bullets[8] = { 0 };
    GameEventQueue events = { 0 };

    CHECK(TrySpawnMobilePlatform(targets, 2, 200.0f));
    CHECK(targets[0].type == SURFACE_TARGET_MOBILE_PLATFORM && targets[0].hp == MOBILE_PLATFORM_HP);

    // Self-propelled: it outruns the water-anchored drift.
    CHECK(TrySpawnCasemate(targets, 2, 100.0f));
    float platformX = targets[0].pos.x;
    float casemateX = targets[1].pos.x;
    UpdateSurfaceTargets(targets, 2, 1.0f);
    NEAR(platformX - targets[0].pos.x, MOBILE_PLATFORM_SPEED);
    NEAR(casemateX - targets[1].pos.x, OCEAN_SCROLL_SPEED);

    // Right of the activation line: held pre-armed, no fan.
    GameEventQueue fireEvents = { 0 };
    targets[0].pos = (Vector2){ (float)GAME_WIDTH + 5.0f, 200.0f };
    UpdateSurfaceTargetFire(targets, 1, MOBILE_PLATFORM_FIRE_INTERVAL + 1.0f, (Vector2){ 100, 200 },
        (Vector2){ 0, 0 }, bullets, 8, &fireEvents);
    for (int i = 0; i < 8; i++) CHECK(!bullets[i].active);
    CHECK(fireEvents.count == 0);

    // Crossing the line releases the pre-armed 3-shot fan aimed at the
    // player immediately, spread vertically. The fan is one fire event.
    targets[0].pos = (Vector2){ 400.0f, 200.0f };
    UpdateSurfaceTargetFire(targets, 1, 0.01f, (Vector2){ 100, 200 },
        (Vector2){ 0, 0 }, bullets, 8, &fireEvents);
    int fired = 0;
    for (int i = 0; i < 8; i++) {
        if (bullets[i].active) fired++;
    }
    CHECK(fired == 3);
    CHECK(fireEvents.count == 1 && fireEvents.items[0].type == GAME_EVENT_ENEMY_FIRED);
    CHECK(bullets[0].vel.x < 0 && bullets[1].vel.x < 0 && bullets[2].vel.x < 0);
    // Outer shots straddle the center shot, one to each side of the lane.
    CHECK(bullets[0].vel.y * bullets[2].vel.y < -1.0f);
    NEAR(bullets[1].vel.y, 0.0f);

    // First multi-torpedo surface target: survives one hit, dies to the
    // second, scoring only on the kill.
    CHECK(!DamageSurfaceTarget(&targets[0], 1, &events));
    CHECK(targets[0].active && events.count == 0);
    CHECK(DamageSurfaceTarget(&targets[0], 1, &events));
    CHECK(ScoreGameEvents(&events) == SCORE_MOBILE_PLATFORM);

    // Stern wake: puffs drop behind the hull on a fixed cadence.
    SurfaceTarget wakeSource[1] = { 0 };
    CHECK(TrySpawnMobilePlatform(wakeSource, 1, 150.0f));
    wakeSource[0].pos = (Vector2){ 300.0f, 150.0f };
    WakeParticle wake[4] = { 0 };
    EmitMobilePlatformWake(wakeSource, 1, wake, 4, MOBILE_PLATFORM_WAKE_INTERVAL);
    CHECK(wake[0].active);
    NEAR(wake[0].pos.x, 300.0f + MOBILE_PLATFORM_STERN_OFFSET);
    NEAR(wake[0].pos.y, 150.0f);
}

static void TestEnemyActivationLine(void) {
    SurfaceTarget casemate[1] = { 0 };
    EnemyBullet bullets[2] = { 0 };
    CHECK(TrySpawnCasemate(casemate, 1, 60.0f));

    // Right of the line: intervals elapse but every shot is held.
    casemate[0].pos.x = ENEMY_ACTIVATION_X + 1.0f;
    UpdateSurfaceTargetFire(casemate, 1, CASEMATE_FIRE_INTERVAL * 3.0f, (Vector2){ 100, 60 },
        (Vector2){ 0, 0 }, bullets, 2, NULL);
    CHECK(!bullets[0].active);

    // Crossing the line releases exactly one pre-armed shot immediately
    // (held at one interval, never a burst of banked shots).
    casemate[0].pos.x = ENEMY_ACTIVATION_X - 1.0f;
    UpdateSurfaceTargetFire(casemate, 1, 0.01f, (Vector2){ 100, 60 },
        (Vector2){ 0, 0 }, bullets, 2, NULL);
    CHECK(bullets[0].active);
    CHECK(!bullets[1].active);
}

static void TestTurretLeadIsSoft(void) {
    // A player moving straight up at full speed, turret dead ahead. A
    // perfect intercept would fire steeply upward (|vy| > |vx|, pinning the
    // aim near the top edge); the soft lead must stay a gentle nudge in the
    // movement direction instead.
    SurfaceTarget turret[1] = { 0 };
    CHECK(TrySpawnTrackingTurret(turret, 1, 176));
    turret[0].pos = (Vector2){ 400, 176 };

    EnemyBullet bullets[1] = { 0 };
    UpdateSurfaceTargetFire(turret, 1, TRACKING_TURRET_FIRE_INTERVAL,
        (Vector2){ 100, 176 }, (Vector2){ 0, -120 }, bullets, 1, NULL);
    CHECK(bullets[0].active);
    CHECK(bullets[0].vel.x < 0);
    CHECK(bullets[0].vel.y < 0);
    CHECK(fabsf(bullets[0].vel.y) < 0.15f * fabsf(bullets[0].vel.x));
}

static void TestTurretAimClampedToPlayArea(void) {
    // A player hugging the top edge while moving up: even the soft lead
    // extrapolates past the edge, where the player can never be. The
    // implied aim point at the player's x must stay inside the play area.
    SurfaceTarget turret[1] = { 0 };
    CHECK(TrySpawnTrackingTurret(turret, 1, 20));
    turret[0].pos = (Vector2){ 400, 20 };

    EnemyBullet bullets[1] = { 0 };
    UpdateSurfaceTargetFire(turret, 1, TRACKING_TURRET_FIRE_INTERVAL,
        (Vector2){ 100, 10 }, (Vector2){ 0, -120 }, bullets, 1, NULL);
    CHECK(bullets[0].active);
    float timeToPlayerX = (100.0f - 400.0f) / bullets[0].vel.x;
    float yAtPlayerX = 20.0f + bullets[0].vel.y * timeToPlayerX;
    CHECK(yAtPlayerX >= -0.01f);
    CHECK(yAtPlayerX <= 10.01f);
}

int main(void) {
    TestDamageAndScore();
    TestMovementAndProjectiles();
    TestAirTargets();
    TestInterceptor();
    TestGunship();
    TestTorpedoes();
    TestTerrain();
    TestPlayerMortar();
    TestMortarBattery();
    TestLandTargetDeathAndShellPersistence();
    TestDroneBunker();
    TestSurfaceTargets();
    TestRelayNode();
    TestMine();
    TestMobilePlatform();
    TestEnemyActivationLine();
    TestTurretLeadIsSoft();
    TestTurretAimClampedToPlayArea();
    return failures != 0;
}
