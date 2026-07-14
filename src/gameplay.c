#include "gameplay.h"

#include <math.h>
#include <stddef.h>

static TorpedoImpact NoTorpedoImpact(void) {
    return (TorpedoImpact){ TORPEDO_IMPACT_NONE, (Vector2){ 0.0f, 0.0f } };
}

static TorpedoImpact TorpedoImpactAt(TorpedoImpactType type, Vector2 pos) {
    return (TorpedoImpact){ type, pos };
}

static float DistanceSquared(Vector2 a, Vector2 b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

bool PushGameEvent(GameEventQueue *events, GameEvent event) {
    if (events == NULL || events->count >= MAX_GAME_EVENTS) return false;
    events->items[events->count++] = event;
    return true;
}

static bool DamageTarget(int *hp, bool *active, int damage, GameEventQueue *events, GameEvent event) {
    if (hp == NULL || active == NULL || !*active || damage <= 0) return false;

    *hp -= damage;
    if (*hp > 0) return false;

    *hp = 0;
    *active = false;
    PushGameEvent(events, event);
    return true;
}

bool DamageAirTarget(AirTarget *target, int damage, GameEventQueue *events) {
    if (target == NULL || !target->active || damage <= 0) return false;

    return DamageTarget(&target->hp, &target->active, damage, events, (GameEvent){
        .type = GAME_EVENT_AIR_TARGET_DESTROYED,
        .pos = target->pos,
        .target.airTarget = target->type
    });
}

bool DamageSurfaceTarget(SurfaceTarget *target, int damage, GameEventQueue *events) {
    if (target == NULL || !target->active || damage <= 0) return false;

    return DamageTarget(&target->hp, &target->active, damage, events, (GameEvent){
        .type = GAME_EVENT_SURFACE_TARGET_DESTROYED,
        .pos = target->pos,
        .target.surfaceTarget = target->type
    });
}

int ScoreGameEvents(const GameEventQueue *events) {
    if (events == NULL) return 0;

    int score = 0;
    for (int i = 0; i < events->count; i++) {
        const GameEvent *event = &events->items[i];
        if (event->type == GAME_EVENT_AIR_TARGET_DESTROYED) {
            if (event->target.airTarget == AIR_TARGET_SKIMMER_DRONE) score += SCORE_SKIMMER_DRONE;
        } else if (event->type == GAME_EVENT_SURFACE_TARGET_DESTROYED) {
            if (event->target.surfaceTarget == SURFACE_TARGET_CASEMATE) score += SCORE_CASEMATE;
            if (event->target.surfaceTarget == SURFACE_TARGET_TRACKING_TURRET) score += SCORE_TRACKING_TURRET;
            if (event->target.surfaceTarget == SURFACE_TARGET_RELAY_NODE) score += SCORE_RELAY_NODE;
        }
    }
    return score;
}

static bool CirclesOverlap(Vector2 a, float ar, Vector2 b, float br) {
    float hitDist = ar + br;
    return DistanceSquared(a, b) <= hitDist * hitDist;
}

bool ResolvePlayerContactDamage(Vector2 playerPos, float playerRadius, const AirTarget airTargets[], int airCount,
    const SurfaceTarget surfaceTargets[], int surfaceCount) {
    for (int i = 0; i < airCount; i++) {
        if (!airTargets[i].active) continue;
        if (CirclesOverlap(playerPos, playerRadius, airTargets[i].pos, airTargets[i].radius)) return true;
    }

    for (int i = 0; i < surfaceCount; i++) {
        if (!surfaceTargets[i].active) continue;
        if (CirclesOverlap(playerPos, playerRadius, surfaceTargets[i].pos, surfaceTargets[i].radius)) return true;
    }

    return false;
}

void MovePlayer(Vector2 *player, float inputX, float inputY, float speed, float dt, float halfW, float halfH) {
    player->x += inputX * speed * dt;
    player->y += inputY * speed * dt;

    if (player->x < halfW) player->x = halfW;
    if (player->x > GAME_WIDTH - halfW) player->x = GAME_WIDTH - halfW;
    if (player->y < halfH) player->y = halfH;
    if (player->y > PLAY_HEIGHT - halfH) player->y = PLAY_HEIGHT - halfH;
}

float AdvanceOceanScroll(float oceanScroll, float dt, float tileWidth) {
    oceanScroll += OCEAN_SCROLL_SPEED * dt;
    if (oceanScroll >= tileWidth) oceanScroll -= tileWidth;
    return oceanScroll;
}

bool TryEmitWakeParticle(WakeParticle wake[], int count, Vector2 pos) {
    for (int i = 0; i < count; i++) {
        if (!wake[i].active) {
            wake[i].active = true;
            wake[i].age = 0.0f;
            wake[i].pos = pos;
            return true;
        }
    }
    return false;
}

void UpdateWakeParticles(WakeParticle wake[], int count, float dt) {
    for (int i = 0; i < count; i++) {
        if (!wake[i].active) continue;
        wake[i].age += dt;
        wake[i].pos.x -= OCEAN_SCROLL_SPEED * dt;
        if (wake[i].age >= WAKE_LIFETIME || wake[i].pos.x < 0.0f) wake[i].active = false;
    }
}

bool TrySpawnBullet(Bullet bullets[], int count, Vector2 pos) {
    for (int i = 0; i < count; i++) {
        if (!bullets[i].active) {
            bullets[i].active = true;
            bullets[i].pos = pos;
            return true;
        }
    }
    return false;
}

bool TrySpawnEnemyBullet(EnemyBullet bullets[], int count, Vector2 pos, Vector2 vel) {
    for (int i = 0; i < count; i++) {
        if (!bullets[i].active) {
            bullets[i].active = true;
            bullets[i].pos = pos;
            bullets[i].vel = vel;
            return true;
        }
    }
    return false;
}

void UpdateBullets(Bullet bullets[], int count, float dt) {
    for (int i = 0; i < count; i++) {
        if (!bullets[i].active) continue;
        bullets[i].pos.x += BULLET_SPEED * dt;
        if (bullets[i].pos.x - BULLET_RADIUS > GAME_WIDTH) bullets[i].active = false;
    }
}

void UpdateEnemyBullets(EnemyBullet bullets[], int count, float dt) {
    for (int i = 0; i < count; i++) {
        if (!bullets[i].active) continue;
        bullets[i].pos.x += bullets[i].vel.x * dt;
        bullets[i].pos.y += bullets[i].vel.y * dt;
        if (bullets[i].pos.x < -ENEMY_BULLET_RADIUS || bullets[i].pos.x > GAME_WIDTH + ENEMY_BULLET_RADIUS
            || bullets[i].pos.y < -ENEMY_BULLET_RADIUS || bullets[i].pos.y > PLAY_HEIGHT + ENEMY_BULLET_RADIUS) {
            bullets[i].active = false;
        }
    }
}

static float ClampDroneBaseY(float baseY) {
    float margin = SKIMMER_DRONE_RADIUS + SKIMMER_DRONE_SINE_AMPLITUDE;
    if (baseY < margin) return margin;
    if (baseY > PLAY_HEIGHT - margin) return PLAY_HEIGHT - margin;
    return baseY;
}

static bool TrySpawnSkimmerDroneFrom(AirTarget targets[], int count, Vector2 pos, int ownerId) {
    for (int i = 0; i < count; i++) {
        if (!targets[i].active) {
            targets[i].type = AIR_TARGET_SKIMMER_DRONE;
            targets[i].active = true;
            targets[i].hp = SKIMMER_DRONE_HP;
            targets[i].radius = SKIMMER_DRONE_RADIUS;
            targets[i].t = 0.0f;
            targets[i].baseY = pos.y;
            targets[i].ownerId = ownerId;
            targets[i].pos = pos;
            return true;
        }
    }
    return false;
}

bool TrySpawnSkimmerDroneAt(AirTarget targets[], int count, float baseY, float spawnXOffset) {
    return TrySpawnSkimmerDroneFrom(targets, count,
        (Vector2){ GAME_WIDTH + SKIMMER_DRONE_RADIUS + spawnXOffset, baseY }, 0);
}

bool TrySpawnSkimmerDrone(AirTarget targets[], int count, float baseY) {
    return TrySpawnSkimmerDroneAt(targets, count, baseY, 0.0f);
}

// Wave patterns for the stage script (see README "Stage authoring
// format": map glyphs are pattern instances; the formation's internal
// shape lives here). Members trail off the right edge so the formation
// flies in as a shape rather than popping in at once.

int SpawnSkimmerDroneLine(AirTarget targets[], int count, float baseY) {
    int spawned = 0;
    for (int i = 0; i < 3; i++) {
        if (TrySpawnSkimmerDroneAt(targets, count, ClampDroneBaseY(baseY), 36.0f * i)) spawned++;
    }
    return spawned;
}

int SpawnSkimmerDroneV(AirTarget targets[], int count, float baseY) {
    // Leader anchored at the glyph lane; wing pairs trail behind and
    // outward, opening rightward like a V flying left.
    int spawned = 0;
    if (TrySpawnSkimmerDroneAt(targets, count, ClampDroneBaseY(baseY), 0.0f)) spawned++;
    for (int rank = 1; rank <= 2; rank++) {
        for (int side = -1; side <= 1; side += 2) {
            float y = ClampDroneBaseY(baseY + side * 26.0f * rank);
            if (TrySpawnSkimmerDroneAt(targets, count, y, 30.0f * rank)) spawned++;
        }
    }
    return spawned;
}

void UpdateAirTargets(AirTarget targets[], int count, float dt) {
    for (int i = 0; i < count; i++) {
        if (!targets[i].active) continue;
        targets[i].t += dt;
        targets[i].pos.x -= SKIMMER_DRONE_SPEED * dt;
        targets[i].pos.y = targets[i].baseY
            + sinf(targets[i].t * SKIMMER_DRONE_SINE_FREQUENCY) * SKIMMER_DRONE_SINE_AMPLITUDE;
        if (targets[i].pos.x < -targets[i].radius) targets[i].active = false;
    }
}

void ResolveBulletAirTargetCollisions(Bullet bullets[], int bulletCount, AirTarget targets[], int targetCount,
    GameEventQueue *events) {
    for (int b = 0; b < bulletCount; b++) {
        if (!bullets[b].active) continue;
        for (int i = 0; i < targetCount; i++) {
            if (!targets[i].active) continue;
            float hitDist = BULLET_RADIUS + targets[i].radius;
            if (DistanceSquared(bullets[b].pos, targets[i].pos) <= hitDist * hitDist) {
                bullets[b].active = false;
                DamageAirTarget(&targets[i], 1, events);
                break;
            }
        }
    }

}

Vector2 CalculateTorpedoReticle(Vector2 spawn) {
    float clampedX = spawn.x + TORPEDO_MAX_RANGE;
    float screenMaxX = GAME_WIDTH - TORPEDO_RETICLE_SCREEN_MARGIN;
    if (clampedX > screenMaxX) clampedX = screenMaxX;
    if (clampedX < spawn.x) clampedX = spawn.x;
    return (Vector2){ clampedX, spawn.y };
}

Vector2 CalculateLeadTorpedoVelocity(Vector2 spawn, const SurfaceTarget targets[], int targetCount) {
    Vector2 aim = { TORPEDO_SPEED, 0.0f };
    float bestT = -1.0f;

    for (int i = 0; i < targetCount; i++) {
        if (!targets[i].active) continue;
        float rx = targets[i].pos.x - spawn.x;
        float ry = targets[i].pos.y - spawn.y;
        if (rx <= 0.0f) continue;

        float a = OCEAN_SCROLL_SPEED * OCEAN_SCROLL_SPEED - TORPEDO_SPEED * TORPEDO_SPEED;
        float b = 2.0f * rx * -OCEAN_SCROLL_SPEED;
        float c = rx * rx + ry * ry;
        float sq = sqrtf(b * b - 4.0f * a * c);
        float t = (-b - sq) / (2.0f * a);
        if (t <= 0.0f) t = (-b + sq) / (2.0f * a);

        float ix = rx - OCEAN_SCROLL_SPEED * t;
        if (t <= 0.0f || ix <= 0.0f) continue;
        if (bestT < 0.0f || t < bestT) {
            bestT = t;
            float ilen = sqrtf(ix * ix + ry * ry);
            aim = (Vector2){ ix / ilen * TORPEDO_SPEED, ry / ilen * TORPEDO_SPEED };
        }
    }

    return aim;
}

void FireFixedRangeTorpedo(Torpedo *torpedo, Vector2 spawn) {
    torpedo->active = true;
    torpedo->pos = spawn;
    torpedo->target = CalculateTorpedoReticle(spawn);
    torpedo->vel = (Vector2){ TORPEDO_SPEED, 0.0f };
    torpedo->distanceTravelled = 0.0f;
    torpedo->armed = false;
}

void FireLeadTorpedo(Torpedo *torpedo, Vector2 spawn, const SurfaceTarget targets[], int targetCount) {
    torpedo->active = true;
    torpedo->pos = spawn;
    torpedo->vel = CalculateLeadTorpedoVelocity(spawn, targets, targetCount);
    torpedo->target = (Vector2){
        spawn.x + torpedo->vel.x / TORPEDO_SPEED * TORPEDO_MAX_RANGE,
        spawn.y + torpedo->vel.y / TORPEDO_SPEED * TORPEDO_MAX_RANGE
    };
    torpedo->distanceTravelled = 0.0f;
    torpedo->armed = false;
}

TorpedoImpact UpdateTorpedo(Torpedo *torpedo, float dt) {
    if (!torpedo->active) return NoTorpedoImpact();

    torpedo->target.x -= OCEAN_SCROLL_SPEED * dt;

    Vector2 toTarget = { torpedo->target.x - torpedo->pos.x, torpedo->target.y - torpedo->pos.y };
    float remaining = sqrtf(toTarget.x * toTarget.x + toTarget.y * toTarget.y);
    float step = TORPEDO_SPEED * dt;

    if (remaining <= step || remaining <= 0.0f) {
        torpedo->pos = torpedo->target;
        torpedo->distanceTravelled += remaining;
        torpedo->armed = torpedo->distanceTravelled >= TORPEDO_ARMING_DISTANCE;
        torpedo->active = false;
        return TorpedoImpactAt(torpedo->armed ? TORPEDO_IMPACT_EXPLOSION : TORPEDO_IMPACT_DIRECT, torpedo->pos);
    }

    torpedo->pos.x += torpedo->vel.x * dt;
    torpedo->pos.y += torpedo->vel.y * dt;
    torpedo->distanceTravelled += step;
    if (torpedo->distanceTravelled >= TORPEDO_ARMING_DISTANCE) torpedo->armed = true;

    return NoTorpedoImpact();
}

static bool TrySpawnSurfaceTarget(SurfaceTarget targets[], int count, SurfaceTargetType type, float radius, int hp,
    Vector2 aimDirection, float y) {
    for (int i = 0; i < count; i++) {
        if (!targets[i].active) {
            targets[i].type = type;
            targets[i].active = true;
            targets[i].hp = hp;
            targets[i].radius = radius;
            targets[i].fireTimer = 0.0f;
            targets[i].aimDirection = aimDirection;
            targets[i].pos = (Vector2){ GAME_WIDTH + radius, y };
            return true;
        }
    }
    return false;
}

bool TrySpawnCasemate(SurfaceTarget targets[], int count, float y) {
    return TrySpawnSurfaceTarget(targets, count, SURFACE_TARGET_CASEMATE, CASEMATE_RADIUS, CASEMATE_HP,
        (Vector2){ -1.0f, 0.0f }, y);
}

bool TrySpawnTrackingTurret(SurfaceTarget targets[], int count, float y) {
    return TrySpawnSurfaceTarget(targets, count, SURFACE_TARGET_TRACKING_TURRET, TRACKING_TURRET_RADIUS,
        TRACKING_TURRET_HP, (Vector2){ -1.0f, 0.0f }, y);
}

bool TrySpawnRelayNode(SurfaceTarget targets[], int count, float y) {
    return TrySpawnSurfaceTarget(targets, count, SURFACE_TARGET_RELAY_NODE, RELAY_NODE_RADIUS,
        RELAY_NODE_HP, (Vector2){ -1.0f, 0.0f }, y);
}

static int CountOwnedDrones(const AirTarget airTargets[], int airCount, int ownerId) {
    int owned = 0;
    for (int i = 0; i < airCount; i++) {
        if (airTargets[i].active && airTargets[i].ownerId == ownerId) owned++;
    }
    return owned;
}

void UpdateRelayNodeLaunches(SurfaceTarget targets[], int count, float dt, AirTarget airTargets[], int airCount,
    GameEventQueue *events) {
    for (int i = 0; i < count; i++) {
        if (!targets[i].active || targets[i].type != SURFACE_TARGET_RELAY_NODE) continue;
        targets[i].fireTimer += dt;
        if (targets[i].fireTimer < RELAY_NODE_LAUNCH_INTERVAL) continue;
        // Hold at the interval while blocked (off-screen or at the drone
        // cap), so a freed slot launches immediately - the relay reads as
        // straining to reinforce, and killing its drones has urgency.
        targets[i].fireTimer = RELAY_NODE_LAUNCH_INTERVAL;
        bool fullyOnScreen = targets[i].pos.x >= targets[i].radius
                          && targets[i].pos.x <= GAME_WIDTH - targets[i].radius;
        if (!fullyOnScreen) continue;
        int ownerId = i + 1;
        if (CountOwnedDrones(airTargets, airCount, ownerId) >= RELAY_NODE_MAX_DRONES) continue;
        if (TrySpawnSkimmerDroneFrom(airTargets, airCount, targets[i].pos, ownerId)) {
            targets[i].fireTimer = 0.0f;
            PushGameEvent(events, (GameEvent){
                .type = GAME_EVENT_DRONE_LAUNCHED, .pos = targets[i].pos
            });
        }
    }
}

void UpdateSurfaceTargets(SurfaceTarget targets[], int count, float dt) {
    for (int i = 0; i < count; i++) {
        if (!targets[i].active) continue;
        targets[i].pos.x -= OCEAN_SCROLL_SPEED * dt;
        if (targets[i].pos.x < -targets[i].radius) targets[i].active = false;
    }
}

static Vector2 CalculateInterceptVelocity(Vector2 spawn, Vector2 targetPos, Vector2 targetVelocity, float speed) {
    Vector2 offset = { targetPos.x - spawn.x, targetPos.y - spawn.y };
    float a = targetVelocity.x * targetVelocity.x + targetVelocity.y * targetVelocity.y - speed * speed;
    float b = 2.0f * (offset.x * targetVelocity.x + offset.y * targetVelocity.y);
    float c = offset.x * offset.x + offset.y * offset.y;
    float discriminant = b * b - 4.0f * a * c;
    float time = -1.0f;

    if (fabsf(a) > 0.0001f && discriminant >= 0.0f) {
        float root = sqrtf(discriminant);
        float first = (-b - root) / (2.0f * a);
        float second = (-b + root) / (2.0f * a);
        if (first > 0.0f) time = first;
        if (second > 0.0f && (time < 0.0f || second < time)) time = second;
    } else if (fabsf(b) > 0.0001f) {
        float linearTime = -c / b;
        if (linearTime > 0.0f) time = linearTime;
    }

    Vector2 aim = time > 0.0f
        ? (Vector2){ offset.x + targetVelocity.x * time, offset.y + targetVelocity.y * time }
        : offset;
    float length = sqrtf(aim.x * aim.x + aim.y * aim.y);
    if (length <= 0.0001f) return (Vector2){ -speed, 0.0f };
    return (Vector2){ aim.x / length * speed, aim.y / length * speed };
}

void UpdateSurfaceTargetFire(SurfaceTarget targets[], int count, float dt, Vector2 playerPos, Vector2 playerVelocity,
    EnemyBullet bullets[], int bulletCount) {
    for (int i = 0; i < count; i++) {
        if (!targets[i].active) continue;
        // Relay Nodes never attack directly; their launch behavior lives
        // in UpdateRelayNodeLaunches.
        if (targets[i].type == SURFACE_TARGET_RELAY_NODE) continue;
        float fireInterval = CASEMATE_FIRE_INTERVAL;
        Vector2 velocity = (Vector2){ -ENEMY_BULLET_SPEED, 0.0f };
        if (targets[i].type == SURFACE_TARGET_TRACKING_TURRET) {
            velocity = CalculateInterceptVelocity(targets[i].pos, playerPos, playerVelocity, ENEMY_BULLET_SPEED);
            fireInterval = TRACKING_TURRET_FIRE_INTERVAL;
        }
        float speed = sqrtf(velocity.x * velocity.x + velocity.y * velocity.y);
        targets[i].aimDirection = (Vector2){ velocity.x / speed, velocity.y / speed };
        targets[i].fireTimer += dt;
        while (targets[i].fireTimer >= fireInterval) {
            targets[i].fireTimer -= fireInterval;
            TrySpawnEnemyBullet(
                bullets,
                bulletCount,
                (Vector2){
                    targets[i].pos.x + targets[i].aimDirection.x * (targets[i].radius + 3.0f),
                    targets[i].pos.y + targets[i].aimDirection.y * (targets[i].radius + 3.0f)
                },
                velocity
            );
        }
    }
}

bool ResolveEnemyBulletPlayerCollision(EnemyBullet bullets[], int bulletCount, Vector2 playerPos, float playerRadius) {
    for (int i = 0; i < bulletCount; i++) {
        if (!bullets[i].active) continue;
        float hitDist = playerRadius + ENEMY_BULLET_RADIUS;
        if (DistanceSquared(playerPos, bullets[i].pos) <= hitDist * hitDist) {
            bullets[i].active = false;
            return true;
        }
    }
    return false;
}

void ResolveTorpedoExplosion(Vector2 pos, SurfaceTarget targets[], int targetCount, GameEventQueue *events) {
    for (int i = 0; i < targetCount; i++) {
        if (!targets[i].active) continue;
        float hitDist = targets[i].radius + TORPEDO_SPLASH_RADIUS;
        if (DistanceSquared(pos, targets[i].pos) <= hitDist * hitDist) {
            DamageSurfaceTarget(&targets[i], 1, events);
        }
    }
}

TorpedoImpact ResolveTorpedoSurfaceTargetCollision(Torpedo *torpedo, SurfaceTarget targets[], int targetCount,
    GameEventQueue *events) {
    if (!torpedo->active) return NoTorpedoImpact();

    for (int i = 0; i < targetCount; i++) {
        if (!targets[i].active) continue;
        float hitDist = targets[i].radius + TORPEDO_DIRECT_HIT_RADIUS;
        if (DistanceSquared(torpedo->pos, targets[i].pos) <= hitDist * hitDist) {
            Vector2 impactPos = torpedo->pos;
            torpedo->active = false;
            if (torpedo->armed) {
                ResolveTorpedoExplosion(impactPos, targets, targetCount, events);
                return TorpedoImpactAt(TORPEDO_IMPACT_EXPLOSION, impactPos);
            }

            DamageSurfaceTarget(&targets[i], 1, events);
            return TorpedoImpactAt(TORPEDO_IMPACT_DIRECT, impactPos);
        }
    }

    return NoTorpedoImpact();
}
