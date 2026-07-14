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
    CHECK(ResolvePlayerContactDamage((Vector2){ 5, 5 }, 2, &contact, 1, NULL, 0));
    CHECK(!ResolvePlayerContactDamage((Vector2){ 100, 100 }, 1, &contact, 1, NULL, 0));
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
    TestTorpedoes();
    TestSurfaceTargets();
    TestTurretLeadIsSoft();
    TestTurretAimClampedToPlayArea();
    return failures != 0;
}
