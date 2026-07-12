#include "gameplay.h"

#include <math.h>

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

static bool PushGameEvent(GameEventQueue *events, GameEvent event) {
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
            if (event->target.surfaceTarget == SURFACE_TARGET_TURRET_PLATFORM) score += SCORE_TURRET_PLATFORM;
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
        if (bullets[i].pos.x < -ENEMY_BULLET_RADIUS || bullets[i].pos.x > GAME_WIDTH + ENEMY_BULLET_RADIUS) {
            bullets[i].active = false;
        }
    }
}

bool TrySpawnSkimmerDrone(AirTarget targets[], int count, float baseY) {
    for (int i = 0; i < count; i++) {
        if (!targets[i].active) {
            targets[i].type = AIR_TARGET_SKIMMER_DRONE;
            targets[i].active = true;
            targets[i].hp = SKIMMER_DRONE_HP;
            targets[i].radius = SKIMMER_DRONE_RADIUS;
            targets[i].t = 0.0f;
            targets[i].baseY = baseY;
            targets[i].pos = (Vector2){ GAME_WIDTH + SKIMMER_DRONE_RADIUS, baseY };
            return true;
        }
    }
    return false;
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

bool TrySpawnTurretPlatform(SurfaceTarget targets[], int count, float y) {
    for (int i = 0; i < count; i++) {
        if (!targets[i].active) {
            targets[i].type = SURFACE_TARGET_TURRET_PLATFORM;
            targets[i].active = true;
            targets[i].hp = TURRET_PLATFORM_HP;
            targets[i].radius = TURRET_PLATFORM_RADIUS;
            targets[i].fireTimer = 0.0f;
            targets[i].pos = (Vector2){ GAME_WIDTH + TURRET_PLATFORM_RADIUS, y };
            return true;
        }
    }
    return false;
}

void UpdateSurfaceTargets(SurfaceTarget targets[], int count, float dt) {
    for (int i = 0; i < count; i++) {
        if (!targets[i].active) continue;
        targets[i].pos.x -= OCEAN_SCROLL_SPEED * dt;
        if (targets[i].pos.x < -targets[i].radius) targets[i].active = false;
    }
}

void UpdateTurretPlatformFire(SurfaceTarget targets[], int count, float dt, EnemyBullet bullets[], int bulletCount) {
    for (int i = 0; i < count; i++) {
        if (!targets[i].active || targets[i].type != SURFACE_TARGET_TURRET_PLATFORM) continue;
        targets[i].fireTimer += dt;
        while (targets[i].fireTimer >= TURRET_PLATFORM_FIRE_INTERVAL) {
            targets[i].fireTimer -= TURRET_PLATFORM_FIRE_INTERVAL;
            TrySpawnEnemyBullet(
                bullets,
                bulletCount,
                (Vector2){ targets[i].pos.x - targets[i].radius - 3.0f, targets[i].pos.y },
                (Vector2){ -ENEMY_BULLET_SPEED, 0.0f }
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
