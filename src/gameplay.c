#include "gameplay.h"

#include <math.h>

static TorpedoImpact NoTorpedoImpact(void) {
    return (TorpedoImpact){ TORPEDO_IMPACT_NONE, (Vector2){ 0.0f, 0.0f }, 0 };
}

static TorpedoImpact TorpedoImpactAt(TorpedoImpactType type, Vector2 pos, int scoreAwarded) {
    return (TorpedoImpact){ type, pos, scoreAwarded };
}

static float DistanceSquared(Vector2 a, Vector2 b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return dx * dx + dy * dy;
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

void UpdateBullets(Bullet bullets[], int count, float dt) {
    for (int i = 0; i < count; i++) {
        if (!bullets[i].active) continue;
        bullets[i].pos.x += BULLET_SPEED * dt;
        if (bullets[i].pos.x - BULLET_RADIUS > GAME_WIDTH) bullets[i].active = false;
    }
}

bool TrySpawnEnemy(Enemy enemies[], int count, float baseY) {
    for (int i = 0; i < count; i++) {
        if (!enemies[i].active) {
            enemies[i].active = true;
            enemies[i].hp = ENEMY_HP;
            enemies[i].t = 0.0f;
            enemies[i].baseY = baseY;
            enemies[i].pos = (Vector2){ GAME_WIDTH + ENEMY_RADIUS, baseY };
            return true;
        }
    }
    return false;
}

void UpdateEnemies(Enemy enemies[], int count, float dt) {
    for (int i = 0; i < count; i++) {
        if (!enemies[i].active) continue;
        enemies[i].t += dt;
        enemies[i].pos.x -= ENEMY_SPEED * dt;
        enemies[i].pos.y = enemies[i].baseY + sinf(enemies[i].t * ENEMY_SINE_FREQUENCY) * ENEMY_SINE_AMPLITUDE;
        if (enemies[i].pos.x < -ENEMY_RADIUS) enemies[i].active = false;
    }
}

int ResolveBulletEnemyCollisions(Bullet bullets[], int bulletCount, Enemy enemies[], int enemyCount) {
    int scoreAwarded = 0;

    for (int b = 0; b < bulletCount; b++) {
        if (!bullets[b].active) continue;
        for (int e = 0; e < enemyCount; e++) {
            if (!enemies[e].active) continue;
            float hitDist = BULLET_RADIUS + ENEMY_RADIUS;
            if (DistanceSquared(bullets[b].pos, enemies[e].pos) <= hitDist * hitDist) {
                bullets[b].active = false;
                enemies[e].hp--;
                if (enemies[e].hp <= 0) {
                    enemies[e].active = false;
                    scoreAwarded += SCORE_SKIMMER_DRONE;
                }
                break;
            }
        }
    }

    return scoreAwarded;
}

Vector2 CalculateTorpedoReticle(Vector2 spawn) {
    float clampedX = spawn.x + TORPEDO_MAX_RANGE;
    float screenMaxX = GAME_WIDTH - TORPEDO_RETICLE_SCREEN_MARGIN;
    if (clampedX > screenMaxX) clampedX = screenMaxX;
    if (clampedX < spawn.x) clampedX = spawn.x;
    return (Vector2){ clampedX, spawn.y };
}

Vector2 CalculateLeadTorpedoVelocity(Vector2 spawn, const Turret turrets[], int turretCount) {
    Vector2 aim = { TORPEDO_SPEED, 0.0f };
    float bestT = -1.0f;

    for (int i = 0; i < turretCount; i++) {
        if (!turrets[i].active) continue;
        float rx = turrets[i].pos.x - spawn.x;
        float ry = turrets[i].pos.y - spawn.y;
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

void FireLeadTorpedo(Torpedo *torpedo, Vector2 spawn, const Turret turrets[], int turretCount) {
    torpedo->active = true;
    torpedo->pos = spawn;
    torpedo->vel = CalculateLeadTorpedoVelocity(spawn, turrets, turretCount);
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
        return TorpedoImpactAt(torpedo->armed ? TORPEDO_IMPACT_EXPLOSION : TORPEDO_IMPACT_DIRECT, torpedo->pos, 0);
    }

    torpedo->pos.x += torpedo->vel.x * dt;
    torpedo->pos.y += torpedo->vel.y * dt;
    torpedo->distanceTravelled += step;
    if (torpedo->distanceTravelled >= TORPEDO_ARMING_DISTANCE) torpedo->armed = true;

    return NoTorpedoImpact();
}

bool TrySpawnTurret(Turret turrets[], int count, float y) {
    for (int i = 0; i < count; i++) {
        if (!turrets[i].active) {
            turrets[i].active = true;
            turrets[i].hp = TURRET_HP;
            turrets[i].pos = (Vector2){ GAME_WIDTH + TURRET_RADIUS, y };
            return true;
        }
    }
    return false;
}

void UpdateTurrets(Turret turrets[], int count, float dt) {
    for (int i = 0; i < count; i++) {
        if (!turrets[i].active) continue;
        turrets[i].pos.x -= OCEAN_SCROLL_SPEED * dt;
        if (turrets[i].pos.x < -TURRET_RADIUS) turrets[i].active = false;
    }
}

int ResolveTorpedoExplosion(Vector2 pos, Turret turrets[], int turretCount) {
    int scoreAwarded = 0;
    float hitDist = TURRET_RADIUS + TORPEDO_SPLASH_RADIUS;
    for (int i = 0; i < turretCount; i++) {
        if (!turrets[i].active) continue;
        if (DistanceSquared(pos, turrets[i].pos) <= hitDist * hitDist) {
            turrets[i].hp--;
            if (turrets[i].hp <= 0) {
                turrets[i].active = false;
                scoreAwarded += SCORE_TURRET_PLATFORM;
            }
        }
    }

    return scoreAwarded;
}

TorpedoImpact ResolveTorpedoTurretCollision(Torpedo *torpedo, Turret turrets[], int turretCount) {
    if (!torpedo->active) return NoTorpedoImpact();

    float hitDist = TURRET_RADIUS + TORPEDO_DIRECT_HIT_RADIUS;
    for (int i = 0; i < turretCount; i++) {
        if (!turrets[i].active) continue;
        if (DistanceSquared(torpedo->pos, turrets[i].pos) <= hitDist * hitDist) {
            Vector2 impactPos = torpedo->pos;
            torpedo->active = false;
            if (torpedo->armed) {
                int scoreAwarded = ResolveTorpedoExplosion(impactPos, turrets, turretCount);
                return TorpedoImpactAt(TORPEDO_IMPACT_EXPLOSION, impactPos, scoreAwarded);
            }

            turrets[i].hp--;
            int scoreAwarded = 0;
            if (turrets[i].hp <= 0) {
                turrets[i].active = false;
                scoreAwarded = SCORE_TURRET_PLATFORM;
            }
            return TorpedoImpactAt(TORPEDO_IMPACT_DIRECT, impactPos, scoreAwarded);
        }
    }

    return NoTorpedoImpact();
}
