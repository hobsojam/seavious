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
            if (event->target.airTarget == AIR_TARGET_INTERCEPTOR) score += SCORE_INTERCEPTOR;
            if (event->target.airTarget == AIR_TARGET_GUNSHIP) score += SCORE_GUNSHIP;
        } else if (event->type == GAME_EVENT_SURFACE_TARGET_DESTROYED) {
            if (event->target.surfaceTarget == SURFACE_TARGET_CASEMATE) score += SCORE_CASEMATE;
            if (event->target.surfaceTarget == SURFACE_TARGET_TRACKING_TURRET) score += SCORE_TRACKING_TURRET;
            if (event->target.surfaceTarget == SURFACE_TARGET_RELAY_NODE) score += SCORE_RELAY_NODE;
            if (event->target.surfaceTarget == SURFACE_TARGET_MINE) score += SCORE_MINE;
            if (event->target.surfaceTarget == SURFACE_TARGET_MOBILE_PLATFORM) score += SCORE_MOBILE_PLATFORM;
        } else if (event->type == GAME_EVENT_BOSS_PART_DESTROYED) {
            if (event->target.bossPart == BOSS_PART_CORE) score += SCORE_BOSS_CORE;
            else if (event->target.bossPart == BOSS_PART_POD_FORE
                || event->target.bossPart == BOSS_PART_POD_AFT) score += SCORE_BOSS_POD;
            else score += SCORE_BOSS_HULL_SECTION;
        } else if (event->type == GAME_EVENT_BOSS_MISSILE_DOWNED) {
            score += SCORE_BOSS_MISSILE;
        } else if (event->type == GAME_EVENT_LAND_TARGET_DESTROYED) {
            if (event->target.landTarget == LAND_TARGET_MORTAR_BATTERY) score += SCORE_MORTAR_BATTERY;
            if (event->target.landTarget == LAND_TARGET_DRONE_BUNKER) score += SCORE_DRONE_BUNKER;
        }
    }
    return score;
}

static bool CirclesOverlap(Vector2 a, float ar, Vector2 b, float br) {
    float hitDist = ar + br;
    return DistanceSquared(a, b) <= hitDist * hitDist;
}

bool ResolvePlayerContactDamage(Vector2 playerPos, float playerRadius, const AirTarget airTargets[], int airCount,
    SurfaceTarget surfaceTargets[], int surfaceCount, GameEventQueue *events) {
    bool hit = false;

    for (int i = 0; i < airCount; i++) {
        if (!airTargets[i].active) continue;
        if (CirclesOverlap(playerPos, playerRadius, airTargets[i].pos, airTargets[i].radius)) hit = true;
    }

    for (int i = 0; i < surfaceCount; i++) {
        if (!surfaceTargets[i].active) continue;
        // Mines use their proximity fuse and blast pool below. They are not
        // ordinary contact hazards, otherwise their splash could never have
        // its own readable damage window.
        if (surfaceTargets[i].type == SURFACE_TARGET_MINE) continue;
        if (!CirclesOverlap(playerPos, playerRadius, surfaceTargets[i].pos, surfaceTargets[i].radius)) continue;
        hit = true;
    }

    return hit;
}

bool DetonateNearbyMines(SurfaceTarget targets[], int count, Vector2 playerPos, float playerRadius,
    GameEventQueue *events) {
    bool detonated = false;
    for (int i = 0; i < count; i++) {
        if (!targets[i].active || targets[i].type != SURFACE_TARGET_MINE) continue;
        if (!CirclesOverlap(playerPos, playerRadius, targets[i].pos, MINE_PROXIMITY_RADIUS)) continue;
        targets[i].active = false;
        PushGameEvent(events, (GameEvent){
            .type = GAME_EVENT_MINE_DETONATED, .pos = targets[i].pos
        });
        detonated = true;
    }
    return detonated;
}

static bool TrySpawnMineBlast(MineBlast blasts[], int count, Vector2 pos) {
    for (int i = 0; i < count; i++) {
        if (blasts[i].active) continue;
        blasts[i] = (MineBlast){ .pos = pos, .remaining = MINE_BLAST_DURATION, .active = true };
        return true;
    }
    return false;
}

void SpawnMineBlastsFromEvents(MineBlast blasts[], int blastCount, const GameEventQueue *events) {
    for (int i = 0; i < events->count; i++) {
        const GameEvent *event = &events->items[i];
        bool mineDestroyed = event->type == GAME_EVENT_SURFACE_TARGET_DESTROYED
            && event->target.surfaceTarget == SURFACE_TARGET_MINE;
        if (event->type == GAME_EVENT_MINE_DETONATED || mineDestroyed) {
            TrySpawnMineBlast(blasts, blastCount, event->pos);
        }
    }
}

void UpdateMineBlasts(MineBlast blasts[], int count, float dt) {
    for (int i = 0; i < count; i++) {
        if (!blasts[i].active) continue;
        blasts[i].remaining -= dt;
        if (blasts[i].remaining <= 0.0f) blasts[i].active = false;
    }
}

bool ResolveMineBlastPlayerHit(const MineBlast blasts[], int count, Vector2 playerPos, float playerRadius) {
    for (int i = 0; i < count; i++) {
        if (blasts[i].active && CirclesOverlap(playerPos, playerRadius, blasts[i].pos, MINE_BLAST_RADIUS)) {
            return true;
        }
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

static float ClampAirLaneY(float baseY, float margin) {
    if (baseY < margin) return margin;
    if (baseY > PLAY_HEIGHT - margin) return PLAY_HEIGHT - margin;
    return baseY;
}

static float ClampDroneBaseY(float baseY) {
    return ClampAirLaneY(baseY, SKIMMER_DRONE_RADIUS + SKIMMER_DRONE_SINE_AMPLITUDE);
}

static bool TrySpawnAirTargetFrom(AirTarget targets[], int count, AirTargetType type, float radius, int hp,
    Vector2 pos, int ownerId) {
    for (int i = 0; i < count; i++) {
        if (!targets[i].active) {
            targets[i].type = type;
            targets[i].active = true;
            targets[i].hp = hp;
            targets[i].radius = radius;
            targets[i].t = 0.0f;
            targets[i].baseY = pos.y;
            targets[i].ownerId = ownerId;
            targets[i].fireTimer = 0.0f;
            targets[i].hasFired = false;
            targets[i].pos = pos;
            return true;
        }
    }
    return false;
}

static bool TrySpawnSkimmerDroneFrom(AirTarget targets[], int count, Vector2 pos, int ownerId) {
    return TrySpawnAirTargetFrom(targets, count, AIR_TARGET_SKIMMER_DRONE, SKIMMER_DRONE_RADIUS,
        SKIMMER_DRONE_HP, pos, ownerId);
}

// Straight flyers only need their own radius as lane margin (no sine swing).
bool TrySpawnInterceptor(AirTarget targets[], int count, float baseY) {
    return TrySpawnAirTargetFrom(targets, count, AIR_TARGET_INTERCEPTOR, INTERCEPTOR_RADIUS, INTERCEPTOR_HP,
        (Vector2){ GAME_WIDTH + INTERCEPTOR_RADIUS, ClampAirLaneY(baseY, INTERCEPTOR_RADIUS) }, 0);
}

bool TrySpawnGunship(AirTarget targets[], int count, float baseY) {
    return TrySpawnAirTargetFrom(targets, count, AIR_TARGET_GUNSHIP, GUNSHIP_RADIUS, GUNSHIP_HP,
        (Vector2){ GAME_WIDTH + GUNSHIP_RADIUS, ClampAirLaneY(baseY, GUNSHIP_RADIUS) }, 0);
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

static float AirTargetSpeed(AirTargetType type) {
    switch (type) {
        case AIR_TARGET_INTERCEPTOR: return INTERCEPTOR_SPEED;
        case AIR_TARGET_GUNSHIP: return GUNSHIP_SPEED;
        default: return SKIMMER_DRONE_SPEED;
    }
}

void UpdateAirTargets(AirTarget targets[], int count, float dt) {
    for (int i = 0; i < count; i++) {
        if (!targets[i].active) continue;
        targets[i].t += dt;
        targets[i].pos.x -= AirTargetSpeed(targets[i].type) * dt;
        // Only the Skimmer Drone weaves; Interceptor and Gunship hold their
        // spawn lane so their attack runs stay readable.
        targets[i].pos.y = targets[i].type == AIR_TARGET_SKIMMER_DRONE
            ? targets[i].baseY + sinf(targets[i].t * SKIMMER_DRONE_SINE_FREQUENCY) * SKIMMER_DRONE_SINE_AMPLITUDE
            : targets[i].baseY;
        if (targets[i].pos.x < -targets[i].radius) targets[i].active = false;
    }
}

static Vector2 AimAtPlayer(Vector2 from, Vector2 playerPos) {
    Vector2 toPlayer = { playerPos.x - from.x, playerPos.y - from.y };
    float distance = sqrtf(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y);
    if (distance <= 0.0001f) return (Vector2){ -1.0f, 0.0f };
    return (Vector2){ toPlayer.x / distance, toPlayer.y / distance };
}

// One ENEMY_FIRED event per shot or volley that actually spawned a
// bullet (a full pool fires nothing, so it sounds like nothing): a
// Gunship/Mobile Platform fan is one event, matching how the volley
// reads on screen.
static void PushEnemyFired(GameEventQueue *events, Vector2 pos, bool spawned) {
    if (!spawned) return;
    PushGameEvent(events, (GameEvent){ .type = GAME_EVENT_ENEMY_FIRED, .pos = pos });
}

void UpdateAirTargetFire(AirTarget targets[], int count, float dt, Vector2 playerPos,
    EnemyBullet bullets[], int bulletCount, GameEventQueue *events) {
    for (int i = 0; i < count; i++) {
        if (!targets[i].active) continue;

        if (targets[i].type == AIR_TARGET_INTERCEPTOR) {
            if (targets[i].hasFired || targets[i].pos.x > INTERCEPTOR_FIRE_X) continue;
            targets[i].hasFired = true;
            Vector2 aim = AimAtPlayer(targets[i].pos, playerPos);
            bool spawned = TrySpawnEnemyBullet(
                bullets,
                bulletCount,
                (Vector2){
                    targets[i].pos.x + aim.x * (targets[i].radius + 3.0f),
                    targets[i].pos.y + aim.y * (targets[i].radius + 3.0f)
                },
                (Vector2){ aim.x * INTERCEPTOR_SHOT_SPEED, aim.y * INTERCEPTOR_SHOT_SPEED }
            );
            PushEnemyFired(events, targets[i].pos, spawned);
        } else if (targets[i].type == AIR_TARGET_GUNSHIP) {
            // Held fully pre-armed while approaching the activation line,
            // so the first spread comes right at the line.
            if (targets[i].pos.x > ENEMY_ACTIVATION_X) {
                targets[i].fireTimer = GUNSHIP_FIRE_INTERVAL;
                continue;
            }
            targets[i].fireTimer += dt;
            while (targets[i].fireTimer >= GUNSHIP_FIRE_INTERVAL) {
                targets[i].fireTimer -= GUNSHIP_FIRE_INTERVAL;
                Vector2 aim = AimAtPlayer(targets[i].pos, playerPos);
                bool spawned = false;
                for (int shot = -1; shot <= 1; shot++) {
                    float angle = (float)shot * GUNSHIP_FAN_SPREAD_DEG * DEG2RAD;
                    float ca = cosf(angle);
                    float sa = sinf(angle);
                    Vector2 dir = { aim.x * ca - aim.y * sa, aim.x * sa + aim.y * ca };
                    spawned |= TrySpawnEnemyBullet(
                        bullets,
                        bulletCount,
                        (Vector2){
                            targets[i].pos.x + dir.x * (targets[i].radius + 3.0f),
                            targets[i].pos.y + dir.y * (targets[i].radius + 3.0f)
                        },
                        (Vector2){ dir.x * ENEMY_BULLET_SPEED, dir.y * ENEMY_BULLET_SPEED }
                    );
                }
                PushEnemyFired(events, targets[i].pos, spawned);
            }
        }
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

Rectangle TerrainScreenRect(StageTerrainFootprint footprint, float scrollDistance) {
    // Same clock as the ground glyphs: a footprint's left edge reaches the
    // right screen edge exactly when scrollDistance reaches its px, then
    // drifts left with the water.
    return (Rectangle){
        (float)footprint.px - scrollDistance + GAME_WIDTH,
        (float)footprint.y,
        (float)footprint.widthPx,
        (float)footprint.heightPx
    };
}

// True when a torpedo travelling on laneY overlaps the blocker's rows.
static bool RectBlocksTorpedoLane(Rectangle rect, float laneY) {
    float hh = TORPEDO_HEIGHT / 2.0f;
    return laneY + hh > rect.y && laneY - hh < rect.y + rect.height;
}

// Shared blocked-lane-reads-before-firing rule: clamp to the blocker's
// left edge if it sits ahead of the nose in this lane (a blocker fully
// behind the spawn point is ignored - firing from beyond it is the
// counter-play).
static float ClampLaneToRectEdge(Vector2 spawn, float clampedX, Rectangle rect) {
    if (!RectBlocksTorpedoLane(rect, spawn.y)) return clampedX;
    if (rect.x + rect.width <= spawn.x) return clampedX;
    return rect.x < clampedX ? rect.x : clampedX;
}

Vector2 CalculateTorpedoReticle(Vector2 spawn, const StageTerrainFootprint terrain[], int terrainCount,
    float scrollDistance) {
    float clampedX = spawn.x + TORPEDO_MAX_RANGE;
    float screenMaxX = GAME_WIDTH - TORPEDO_RETICLE_SCREEN_MARGIN;
    if (clampedX > screenMaxX) clampedX = screenMaxX;
    for (int i = 0; i < terrainCount; i++) {
        clampedX = ClampLaneToRectEdge(spawn, clampedX, TerrainScreenRect(terrain[i], scrollDistance));
    }
    if (clampedX < spawn.x) clampedX = spawn.x;
    return (Vector2){ clampedX, spawn.y };
}

// Re-clamps an already-computed reticle against screen-space blocker
// rects (the boss's armored hull) under the same rules as land.
Vector2 ClampReticleToRects(Vector2 spawn, Vector2 reticle, const Rectangle rects[], int rectCount) {
    float clampedX = reticle.x;
    for (int i = 0; i < rectCount; i++) {
        clampedX = ClampLaneToRectEdge(spawn, clampedX, rects[i]);
    }
    if (clampedX < spawn.x) clampedX = spawn.x;
    return (Vector2){ clampedX, reticle.y };
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

void FireFixedRangeTorpedo(Torpedo *torpedo, Vector2 spawn, const StageTerrainFootprint terrain[], int terrainCount,
    float scrollDistance) {
    torpedo->active = true;
    torpedo->pos = spawn;
    torpedo->target = CalculateTorpedoReticle(spawn, terrain, terrainCount, scrollDistance);
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

// Impact sits at the blocker edge the nose crossed; a torpedo fired
// while already over the blocker (the ship flies above it) dies in place.
static bool TorpedoRectImpactEdge(const Torpedo *torpedo, Rectangle rect, float *edgeX) {
    float hw = TORPEDO_WIDTH / 2.0f;
    if (!RectBlocksTorpedoLane(rect, torpedo->pos.y)) return false;
    if (torpedo->pos.x + hw < rect.x || torpedo->pos.x - hw > rect.x + rect.width) return false;
    *edgeX = torpedo->pos.x < rect.x ? rect.x : torpedo->pos.x;
    return true;
}

// Armed = detonate at the edge (the caller resolves splash, which can
// still catch targets hugging the blocker); unarmed = fizzle, no splash.
static TorpedoImpact BlockedTorpedoImpact(Torpedo *torpedo, bool hit, float edgeX) {
    if (!hit) return NoTorpedoImpact();
    Vector2 impactPos = { edgeX, torpedo->pos.y };
    torpedo->active = false;
    return TorpedoImpactAt(torpedo->armed ? TORPEDO_IMPACT_EXPLOSION : TORPEDO_IMPACT_DIRECT, impactPos);
}

TorpedoImpact ResolveTorpedoTerrainCollision(Torpedo *torpedo, const StageTerrainFootprint terrain[],
    int terrainCount, float scrollDistance) {
    if (!torpedo->active) return NoTorpedoImpact();

    float bestEdgeX = 0.0f;
    bool hit = false;
    for (int i = 0; i < terrainCount; i++) {
        float edgeX;
        if (!TorpedoRectImpactEdge(torpedo, TerrainScreenRect(terrain[i], scrollDistance), &edgeX)) continue;
        if (!hit || edgeX < bestEdgeX) {
            bestEdgeX = edgeX;
            hit = true;
        }
    }
    return BlockedTorpedoImpact(torpedo, hit, bestEdgeX);
}

TorpedoImpact ResolveTorpedoRectCollision(Torpedo *torpedo, const Rectangle rects[], int rectCount) {
    if (!torpedo->active) return NoTorpedoImpact();

    float bestEdgeX = 0.0f;
    bool hit = false;
    for (int i = 0; i < rectCount; i++) {
        float edgeX;
        if (!TorpedoRectImpactEdge(torpedo, rects[i], &edgeX)) continue;
        if (!hit || edgeX < bestEdgeX) {
            bestEdgeX = edgeX;
            hit = true;
        }
    }
    return BlockedTorpedoImpact(torpedo, hit, bestEdgeX);
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
            targets[i].wakeTimer = 0.0f;
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

bool TrySpawnMine(SurfaceTarget targets[], int count, float y) {
    return TrySpawnSurfaceTarget(targets, count, SURFACE_TARGET_MINE, MINE_RADIUS,
        MINE_HP, (Vector2){ -1.0f, 0.0f }, y);
}

bool TrySpawnMobilePlatform(SurfaceTarget targets[], int count, float y) {
    return TrySpawnSurfaceTarget(targets, count, SURFACE_TARGET_MOBILE_PLATFORM, MOBILE_PLATFORM_RADIUS,
        MOBILE_PLATFORM_HP, (Vector2){ -1.0f, 0.0f }, y);
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
        // Approaching the activation line the relay is held fully
        // pre-armed, so its first launch comes right at the line.
        if (targets[i].pos.x > ENEMY_ACTIVATION_X) targets[i].fireTimer = RELAY_NODE_LAUNCH_INTERVAL;
        if (targets[i].fireTimer < RELAY_NODE_LAUNCH_INTERVAL) continue;
        // Hold at the interval while blocked (before the line or at the
        // drone cap), so a freed slot launches immediately - the relay
        // reads as straining to reinforce, and killing its drones has
        // urgency.
        targets[i].fireTimer = RELAY_NODE_LAUNCH_INTERVAL;
        if (targets[i].pos.x > ENEMY_ACTIVATION_X) continue;
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
        // The Mobile Platform is self-propelled past the anchored drift.
        // It shares the scroll dt, so the boss lock freezes it with the
        // rest of the water traffic.
        float speed = targets[i].type == SURFACE_TARGET_MOBILE_PLATFORM
            ? MOBILE_PLATFORM_SPEED : OCEAN_SCROLL_SPEED;
        targets[i].pos.x -= speed * dt;
        if (targets[i].pos.x < -targets[i].radius) targets[i].active = false;
    }
}

void EmitMobilePlatformWake(SurfaceTarget targets[], int count, WakeParticle wake[], int wakeCount, float dt) {
    for (int i = 0; i < count; i++) {
        if (!targets[i].active || targets[i].type != SURFACE_TARGET_MOBILE_PLATFORM) continue;
        targets[i].wakeTimer += dt;
        while (targets[i].wakeTimer >= MOBILE_PLATFORM_WAKE_INTERVAL) {
            targets[i].wakeTimer -= MOBILE_PLATFORM_WAKE_INTERVAL;
            // Puffs drop at the stern and drift at water speed while the
            // hull pulls away at 1.5x, so the trail stretches out behind.
            TryEmitWakeParticle(wake, wakeCount, (Vector2){
                targets[i].pos.x + MOBILE_PLATFORM_STERN_OFFSET, targets[i].pos.y
            });
        }
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

    Vector2 predicted = time > 0.0f
        ? (Vector2){ targetPos.x + targetVelocity.x * time, targetPos.y + targetVelocity.y * time }
        : targetPos;
    // The target is the player, who can never leave the play area — don't
    // aim at extrapolated points outside it.
    if (predicted.x < 0.0f) predicted.x = 0.0f;
    if (predicted.x > GAME_WIDTH) predicted.x = GAME_WIDTH;
    if (predicted.y < 0.0f) predicted.y = 0.0f;
    if (predicted.y > PLAY_HEIGHT) predicted.y = PLAY_HEIGHT;

    Vector2 aim = { predicted.x - spawn.x, predicted.y - spawn.y };
    float length = sqrtf(aim.x * aim.x + aim.y * aim.y);
    if (length <= 0.0001f) return (Vector2){ -speed, 0.0f };
    return (Vector2){ aim.x / length * speed, aim.y / length * speed };
}

void UpdateSurfaceTargetFire(SurfaceTarget targets[], int count, float dt, Vector2 playerPos, Vector2 playerVelocity,
    EnemyBullet bullets[], int bulletCount, GameEventQueue *events) {
    for (int i = 0; i < count; i++) {
        if (!targets[i].active) continue;
        // Relay Nodes never attack directly (their launch behavior lives
        // in UpdateRelayNodeLaunches); Mines carry no weapon at all.
        if (targets[i].type == SURFACE_TARGET_RELAY_NODE || targets[i].type == SURFACE_TARGET_MINE) continue;

        if (targets[i].type == SURFACE_TARGET_MOBILE_PLATFORM) {
            Vector2 toPlayer = { playerPos.x - targets[i].pos.x, playerPos.y - targets[i].pos.y };
            float distance = sqrtf(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y);
            if (distance > 0.0001f) {
                targets[i].aimDirection = (Vector2){ toPlayer.x / distance, toPlayer.y / distance };
            }
            // Held fully pre-armed while approaching the activation line,
            // so the first fan comes right at the line.
            if (targets[i].pos.x > ENEMY_ACTIVATION_X) {
                targets[i].fireTimer = MOBILE_PLATFORM_FIRE_INTERVAL;
                continue;
            }
            targets[i].fireTimer += dt;
            while (targets[i].fireTimer >= MOBILE_PLATFORM_FIRE_INTERVAL) {
                targets[i].fireTimer -= MOBILE_PLATFORM_FIRE_INTERVAL;
                bool spawned = false;
                for (int shot = -1; shot <= 1; shot++) {
                    float angle = (float)shot * MOBILE_PLATFORM_FAN_SPREAD_DEG * DEG2RAD;
                    float ca = cosf(angle);
                    float sa = sinf(angle);
                    Vector2 dir = {
                        targets[i].aimDirection.x * ca - targets[i].aimDirection.y * sa,
                        targets[i].aimDirection.x * sa + targets[i].aimDirection.y * ca
                    };
                    spawned |= TrySpawnEnemyBullet(
                        bullets,
                        bulletCount,
                        (Vector2){
                            targets[i].pos.x + dir.x * (targets[i].radius + 3.0f),
                            targets[i].pos.y + dir.y * (targets[i].radius + 3.0f)
                        },
                        (Vector2){ dir.x * ENEMY_BULLET_SPEED, dir.y * ENEMY_BULLET_SPEED }
                    );
                }
                PushEnemyFired(events, targets[i].pos, spawned);
            }
            continue;
        }

        float fireInterval = CASEMATE_FIRE_INTERVAL;
        Vector2 velocity = (Vector2){ -ENEMY_BULLET_SPEED, 0.0f };
        if (targets[i].type == SURFACE_TARGET_TRACKING_TURRET) {
            Vector2 leadVelocity = {
                playerVelocity.x * TRACKING_TURRET_LEAD_FACTOR,
                playerVelocity.y * TRACKING_TURRET_LEAD_FACTOR
            };
            velocity = CalculateInterceptVelocity(targets[i].pos, playerPos, leadVelocity, ENEMY_BULLET_SPEED);
            fireInterval = TRACKING_TURRET_FIRE_INTERVAL;
        }
        float speed = sqrtf(velocity.x * velocity.x + velocity.y * velocity.y);
        targets[i].aimDirection = (Vector2){ velocity.x / speed, velocity.y / speed };
        // Aim tracks every frame (the turret barrel follows the player even
        // while holding fire), but the shot itself is held fully pre-armed
        // until the activation line.
        if (targets[i].pos.x > ENEMY_ACTIVATION_X) {
            targets[i].fireTimer = fireInterval;
            continue;
        }
        targets[i].fireTimer += dt;
        while (targets[i].fireTimer >= fireInterval) {
            targets[i].fireTimer -= fireInterval;
            bool spawned = TrySpawnEnemyBullet(
                bullets,
                bulletCount,
                (Vector2){
                    targets[i].pos.x + targets[i].aimDirection.x * (targets[i].radius + 3.0f),
                    targets[i].pos.y + targets[i].aimDirection.y * (targets[i].radius + 3.0f)
                },
                velocity
            );
            PushEnemyFired(events, targets[i].pos, spawned);
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

Vector2 CalculateMortarReticle(Vector2 spawn) {
    // Shorter range than the torpedo and no terrain clamp: the shell arcs
    // over land, so a blocked lane doesn't exist for this weapon. Only the
    // screen margin caps it.
    float x = spawn.x + PLAYER_MORTAR_MAX_RANGE;
    float screenMaxX = GAME_WIDTH - TORPEDO_RETICLE_SCREEN_MARGIN;
    if (x > screenMaxX) x = screenMaxX;
    return (Vector2){ x, spawn.y };
}

void FirePlayerMortar(MortarShell *shell, Vector2 spawn, Vector2 target) {
    *shell = (MortarShell){
        .launch = spawn,
        .target = target,
        .t = 0.0f,
        .blastT = 0.0f,
        .landed = false,
        .active = true
    };
}

// Returns true on the single step the shell lands (the caller resolves
// the blast); afterwards the shell only ticks its blast visual down.
bool UpdatePlayerMortarShell(MortarShell *shell, float dt, float scrollDt) {
    if (!shell->active) return false;
    if (!shell->landed) {
        // The landing point is water-anchored like the torpedo's target;
        // scrollDt keeps it frozen with the water under the boss lock.
        shell->target.x -= OCEAN_SCROLL_SPEED * scrollDt;
        shell->t += dt;
        if (shell->t >= PLAYER_MORTAR_AIR_TIME) {
            shell->landed = true;
            shell->blastT = PLAYER_MORTAR_BLAST_DURATION;
            return true;
        }
    } else {
        shell->blastT -= dt;
        if (shell->blastT <= 0.0f) shell->active = false;
    }
    return false;
}

void ResolveMortarBlastSurfaceTargets(Vector2 pos, SurfaceTarget targets[], int targetCount, GameEventQueue *events) {
    for (int i = 0; i < targetCount; i++) {
        if (!targets[i].active) continue;
        float hitDist = targets[i].radius + PLAYER_MORTAR_BLAST_RADIUS;
        if (DistanceSquared(pos, targets[i].pos) <= hitDist * hitDist) {
            DamageSurfaceTarget(&targets[i], 1, events);
        }
    }
}

// Spawning prefers a slot whose shell has also finished, so pool reuse
// can't clip a dead battery's still-flying shell out of the air.
static int FreeLandTargetSlot(const LandTarget targets[], int count) {
    for (int i = 0; i < count; i++) {
        if (!targets[i].active && !targets[i].shell.active) return i;
    }
    for (int i = 0; i < count; i++) {
        if (!targets[i].active) return i;
    }
    return -1;
}

static bool TrySpawnLandTarget(LandTarget targets[], int count, LandTargetType type,
    float radius, int hp, float y) {
    int slot = FreeLandTargetSlot(targets, count);
    if (slot < 0) return false;
    // If every free slot still carries a flying shell, the new
    // installation inherits it: the shell finishes its arc untouched and
    // the battery simply can't lob until it clears.
    MortarShell carried = targets[slot].shell;
    targets[slot] = (LandTarget){
        .type = type,
        // Stage land glyphs are 32px terrain cells. At the moment their
        // event fires, the matching hardpoint cell starts at GAME_WIDTH,
        // so its visual pad center is exactly +16px, irrespective of the
        // installation sprite/radius.
        .pos = { GAME_WIDTH + 16.0f, y },
        .radius = radius,
        .hp = hp,
        .active = true
    };
    targets[slot].shell = carried;
    return true;
}

bool TrySpawnMortarBattery(LandTarget targets[], int count, float laneY) {
    return TrySpawnLandTarget(targets, count, LAND_TARGET_MORTAR_BATTERY,
        MORTAR_BATTERY_RADIUS, MORTAR_BATTERY_HP, laneY);
}

bool TrySpawnDroneBunker(LandTarget targets[], int count, float laneY) {
    return TrySpawnLandTarget(targets, count, LAND_TARGET_DRONE_BUNKER,
        DRONE_BUNKER_RADIUS, DRONE_BUNKER_HP, laneY);
}

bool DamageLandTarget(LandTarget *target, int damage, GameEventQueue *events) {
    if (target == NULL || !target->active || damage <= 0) return false;
    target->hp -= damage;
    if (target->hp > 0) return false;
    target->active = false;
    PushGameEvent(events, (GameEvent){
        .type = GAME_EVENT_LAND_TARGET_DESTROYED,
        .pos = target->pos,
        .target.landTarget = target->type
    });
    return true;
}

void UpdateLandTargets(LandTarget targets[], int count, float dt) {
    for (int i = 0; i < count; i++) {
        if (!targets[i].active) continue;
        // Anchored drift: same clock as the terrain cell it mounted on.
        targets[i].pos.x -= OCEAN_SCROLL_SPEED * dt;
        if (targets[i].pos.x < -targets[i].radius) targets[i].active = false;
    }
}

void UpdateMortarBatteries(LandTarget targets[], int count, float dt, Vector2 playerPos,
    GameEventQueue *events) {
    for (int i = 0; i < count; i++) {
        LandTarget *battery = &targets[i];
        MortarShell *shell = &battery->shell;

        // Shells advance whether or not the battery still stands: one
        // already in the air when it dies still comes down (boss rule).
        if (shell->active) {
            if (!shell->landed) {
                shell->t += dt;
                if (shell->t >= LAND_MORTAR_AIR_TIME) {
                    shell->landed = true;
                    shell->blastT = LAND_MORTAR_BLAST_DURATION;
                    PushGameEvent(events, (GameEvent){
                        .type = GAME_EVENT_MORTAR_BLAST, .pos = shell->target
                    });
                }
            } else {
                shell->blastT -= dt;
                if (shell->blastT <= 0.0f) shell->active = false;
            }
        }

        if (!battery->active || battery->type != LAND_TARGET_MORTAR_BATTERY) continue;
        // Held fully pre-armed while approaching the activation line, so
        // the first lob comes right at the line.
        if (battery->pos.x > ENEMY_ACTIVATION_X) {
            battery->fireTimer = MORTAR_BATTERY_FIRE_INTERVAL;
            continue;
        }
        battery->fireTimer += dt;
        if (battery->fireTimer < MORTAR_BATTERY_FIRE_INTERVAL || shell->active) {
            // Cap the bank at one interval so a shell still in flight
            // can't queue an instant double-lob behind it.
            if (battery->fireTimer > MORTAR_BATTERY_FIRE_INTERVAL) {
                battery->fireTimer = MORTAR_BATTERY_FIRE_INTERVAL;
            }
            continue;
        }
        Vector2 target = playerPos;
        if (target.x < 0.0f) target.x = 0.0f;
        if (target.x > (float)GAME_WIDTH) target.x = (float)GAME_WIDTH;
        if (target.y < 0.0f) target.y = 0.0f;
        if (target.y > (float)PLAY_HEIGHT) target.y = (float)PLAY_HEIGHT;
        *shell = (MortarShell){
            .launch = battery->pos,
            .target = target,
            .active = true
        };
        battery->fireTimer = 0.0f;
        PushGameEvent(events, (GameEvent){
            .type = GAME_EVENT_MORTAR_FIRED, .pos = battery->pos
        });
    }
}

bool ResolveLandMortarBlastPlayerHit(const LandTarget targets[], int count, Vector2 playerPos,
    float playerRadius) {
    for (int i = 0; i < count; i++) {
        const MortarShell *shell = &targets[i].shell;
        if (!shell->active || !shell->landed || shell->blastT <= 0.0f) continue;
        float hitDist = LAND_MORTAR_BLAST_RADIUS + playerRadius;
        if (DistanceSquared(playerPos, shell->target) <= hitDist * hitDist) return true;
    }
    return false;
}

void UpdateDroneBunkerLaunches(LandTarget targets[], int count, float dt, AirTarget airTargets[],
    int airCount, GameEventQueue *events) {
    for (int i = 0; i < count; i++) {
        if (!targets[i].active || targets[i].type != LAND_TARGET_DRONE_BUNKER) continue;
        targets[i].fireTimer += dt;
        // Same launch discipline as the Relay Node: pre-armed at the
        // line, held at the interval while blocked so a freed drone slot
        // refills immediately.
        if (targets[i].pos.x > ENEMY_ACTIVATION_X) targets[i].fireTimer = DRONE_BUNKER_LAUNCH_INTERVAL;
        if (targets[i].fireTimer < DRONE_BUNKER_LAUNCH_INTERVAL) continue;
        targets[i].fireTimer = DRONE_BUNKER_LAUNCH_INTERVAL;
        if (targets[i].pos.x > ENEMY_ACTIVATION_X) continue;
        // Owner ids share one namespace with the Relay Nodes; bunkers
        // sit past the surface pool's index range.
        int ownerId = 1 + MAX_SURFACE_TARGETS + i;
        if (CountOwnedDrones(airTargets, airCount, ownerId) >= DRONE_BUNKER_MAX_DRONES) continue;
        if (TrySpawnSkimmerDroneFrom(airTargets, airCount, targets[i].pos, ownerId)) {
            targets[i].fireTimer = 0.0f;
            PushGameEvent(events, (GameEvent){
                .type = GAME_EVENT_DRONE_LAUNCHED, .pos = targets[i].pos
            });
        }
    }
}

void ResolveMortarBlastLandTargets(Vector2 pos, LandTarget targets[], int targetCount,
    GameEventQueue *events) {
    for (int i = 0; i < targetCount; i++) {
        if (!targets[i].active) continue;
        float hitDist = targets[i].radius + PLAYER_MORTAR_BLAST_RADIUS;
        if (DistanceSquared(pos, targets[i].pos) <= hitDist * hitDist) {
            DamageLandTarget(&targets[i], 1, events);
        }
    }
}
