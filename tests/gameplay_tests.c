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
    targets[0].pos.x = INTERCEPTOR_FIRE_X + 5.0f;
    UpdateAirTargetFire(targets, 1, 0.016f, (Vector2){ 48, 100 }, bullets, 2);
    CHECK(!bullets[0].active);

    // ...then fires exactly one shot aimed at the player, at double the
    // shared projectile speed.
    targets[0].pos.x = INTERCEPTOR_FIRE_X - 1.0f;
    UpdateAirTargetFire(targets, 1, 0.016f, (Vector2){ 48, 100 }, bullets, 2);
    CHECK(bullets[0].active);
    NEAR(bullets[0].vel.x, -INTERCEPTOR_SHOT_SPEED);
    NEAR(bullets[0].vel.y, 0.0f);
    UpdateAirTargetFire(targets, 1, 10.0f, (Vector2){ 48, 100 }, bullets, 2);
    CHECK(!bullets[1].active);

    // A player below the lane pulls the aim downward.
    AirTarget aimed[1] = { 0 };
    EnemyBullet aimedBullets[1] = { 0 };
    CHECK(TrySpawnInterceptor(aimed, 1, 100.0f));
    aimed[0].pos.x = INTERCEPTOR_FIRE_X - 1.0f;
    UpdateAirTargetFire(aimed, 1, 0.016f, (Vector2){ 48, 200 }, aimedBullets, 1);
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

    // Not fully on-screen: the volley timer doesn't run.
    targets[0].pos.x = (float)GAME_WIDTH + 2.0f;
    UpdateAirTargetFire(targets, 1, GUNSHIP_FIRE_INTERVAL + 1.0f, (Vector2){ 100, 150 }, bullets, 8);
    for (int i = 0; i < 8; i++) CHECK(!bullets[i].active);

    // Fully on-screen: a 3-way spread aimed at the player.
    targets[0].pos.x = 400.0f;
    targets[0].fireTimer = 0.0f;
    UpdateAirTargetFire(targets, 1, GUNSHIP_FIRE_INTERVAL, (Vector2){ 100, 150 }, bullets, 8);
    int fired = 0;
    for (int i = 0; i < 8; i++) {
        if (bullets[i].active) fired++;
    }
    CHECK(fired == 3);
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
    FireFixedRangeTorpedo(&torpedo, spawn);
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

static void TestSurfaceTargets(void) {
    SurfaceTarget targets[2] = { 0 };
    CHECK(TrySpawnCasemate(targets, 2, 30));
    CHECK(TrySpawnTrackingTurret(targets, 2, 40));
    CHECK(targets[0].type == SURFACE_TARGET_CASEMATE);
    CHECK(targets[1].type == SURFACE_TARGET_TRACKING_TURRET);

    EnemyBullet bullets[4] = { 0 };
    UpdateSurfaceTargetFire(targets, 2, 2, (Vector2){ 100, 50 }, (Vector2){ 2, 0 }, bullets, 4);
    CHECK(bullets[0].active);
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

    // Off-screen (spawns beyond the right edge): interval elapses, no launch.
    UpdateRelayNodeLaunches(surface, MAX_SURFACE_TARGETS, RELAY_NODE_LAUNCH_INTERVAL + 0.1f,
        air, MAX_AIR_TARGETS, &events);
    CHECK(air[0].active == false && events.count == 0);

    // Fully on-screen: the held timer launches immediately.
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

    // Relays never fire enemy bullets.
    UpdateSurfaceTargetFire(surface, MAX_SURFACE_TARGETS, 10.0f, (Vector2){ 0, 0 }, (Vector2){ 0, 0 },
        bullets, MAX_ENEMY_BULLETS);
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) CHECK(!bullets[i].active);

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
    UpdateSurfaceTargetFire(mines, 2, 10.0f, (Vector2){ 0, 0 }, (Vector2){ 0, 0 }, bullets, 4);
    for (int i = 0; i < 4; i++) CHECK(!bullets[i].active);

    // Contact detonates the mine: it dies with a detonation event, not a
    // destroyed event, so the collision scores nothing.
    mines[0].pos = (Vector2){ 50.0f, 120.0f };
    CHECK(ResolvePlayerContactDamage((Vector2){ 50, 120 }, 2, NULL, 0, mines, 2, &events));
    CHECK(!mines[0].active);
    CHECK(events.count == 1 && events.items[0].type == GAME_EVENT_MINE_DETONATED);
    CHECK(ScoreGameEvents(&events) == 0);

    // A torpedoed mine scores like any other surface kill.
    GameEventQueue killEvents = { 0 };
    CHECK(TrySpawnMine(mines, 2, 60.0f));
    CHECK(DamageSurfaceTarget(&mines[0], 1, &killEvents));
    CHECK(ScoreGameEvents(&killEvents) == SCORE_MINE);
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

    // Not fully on-screen: the fire timer doesn't run.
    targets[0].pos = (Vector2){ (float)GAME_WIDTH + 5.0f, 200.0f };
    UpdateSurfaceTargetFire(targets, 1, MOBILE_PLATFORM_FIRE_INTERVAL + 1.0f, (Vector2){ 100, 200 },
        (Vector2){ 0, 0 }, bullets, 8);
    for (int i = 0; i < 8; i++) CHECK(!bullets[i].active);

    // Fully on-screen: a 3-shot fan aimed at the player, spread vertically.
    targets[0].pos = (Vector2){ 400.0f, 200.0f };
    targets[0].fireTimer = 0.0f;
    UpdateSurfaceTargetFire(targets, 1, MOBILE_PLATFORM_FIRE_INTERVAL, (Vector2){ 100, 200 },
        (Vector2){ 0, 0 }, bullets, 8);
    int fired = 0;
    for (int i = 0; i < 8; i++) {
        if (bullets[i].active) fired++;
    }
    CHECK(fired == 3);
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
        (Vector2){ 100, 176 }, (Vector2){ 0, -120 }, bullets, 1);
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
        (Vector2){ 100, 10 }, (Vector2){ 0, -120 }, bullets, 1);
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
    TestSurfaceTargets();
    TestRelayNode();
    TestMine();
    TestMobilePlatform();
    TestTurretLeadIsSoft();
    TestTurretAimClampedToPlayArea();
    return failures != 0;
}
