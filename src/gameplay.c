#include "gameplay.h"

#include <math.h>

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

void ResolveBulletEnemyCollisions(Bullet bullets[], int bulletCount, Enemy enemies[], int enemyCount) {
    for (int b = 0; b < bulletCount; b++) {
        if (!bullets[b].active) continue;
        for (int e = 0; e < enemyCount; e++) {
            if (!enemies[e].active) continue;
            float dx = bullets[b].pos.x - enemies[e].pos.x;
            float dy = bullets[b].pos.y - enemies[e].pos.y;
            float hitDist = BULLET_RADIUS + ENEMY_RADIUS;
            if (dx * dx + dy * dy <= hitDist * hitDist) {
                bullets[b].active = false;
                enemies[e].hp--;
                if (enemies[e].hp <= 0) enemies[e].active = false;
                break;
            }
        }
    }
}

Vector2 CalculateTorpedoVelocity(Vector2 spawn, const Turret turrets[], int turretCount) {
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

void FireTorpedo(Torpedo *torpedo, Vector2 spawn, const Turret turrets[], int turretCount) {
    torpedo->active = true;
    torpedo->pos = spawn;
    torpedo->vel = CalculateTorpedoVelocity(spawn, turrets, turretCount);
}

void UpdateTorpedo(Torpedo *torpedo, float dt) {
    if (!torpedo->active) return;

    torpedo->pos.x += torpedo->vel.x * dt;
    torpedo->pos.y += torpedo->vel.y * dt;
    float cull = TORPEDO_WIDTH;
    if (torpedo->pos.x - cull > GAME_WIDTH || torpedo->pos.x + cull < 0.0f ||
        torpedo->pos.y - cull > PLAY_HEIGHT || torpedo->pos.y + cull < 0.0f) {
        torpedo->active = false;
    }
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

void ResolveTorpedoTurretCollision(Torpedo *torpedo, Turret turrets[], int turretCount) {
    if (!torpedo->active) return;

    for (int i = 0; i < turretCount; i++) {
        if (!turrets[i].active) continue;
        float dx = torpedo->pos.x - turrets[i].pos.x;
        float dy = torpedo->pos.y - turrets[i].pos.y;
        float hitDist = TURRET_RADIUS + TORPEDO_WIDTH / 2.0f;
        if (dx * dx + dy * dy <= hitDist * hitDist) {
            torpedo->active = false;
            turrets[i].hp--;
            if (turrets[i].hp <= 0) turrets[i].active = false;
            break;
        }
    }
}
