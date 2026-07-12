#ifndef SEAVIOUS_GAMEPLAY_H
#define SEAVIOUS_GAMEPLAY_H

#include "raylib.h"

#define GAME_WIDTH    512
#define GAME_HEIGHT   384
#define HUD_HEIGHT    32
#define PLAY_HEIGHT   (GAME_HEIGHT - HUD_HEIGHT)
#define WINDOW_SCALE  2
#define OCEAN_SCROLL_SPEED 40.0f

#define MAX_BULLETS         32
#define BULLET_SPEED        280.0f
#define BULLET_FIRE_INTERVAL 0.15f
#define BULLET_RADIUS       3.0f

#define MAX_ENEMIES          16
#define ENEMY_RADIUS         6.0f
#define ENEMY_SPEED          60.0f
#define ENEMY_SPAWN_INTERVAL 1.2f
#define ENEMY_SINE_AMPLITUDE 24.0f
#define ENEMY_SINE_FREQUENCY 2.0f
#define ENEMY_HP             1

#define TORPEDO_SPEED    200.0f
#define TORPEDO_COOLDOWN 1.5f
#define TORPEDO_WIDTH    12.0f
#define TORPEDO_HEIGHT   4.0f

#define MAX_TURRETS            8
#define TURRET_RADIUS          9.0f
#define TURRET_SPAWN_INTERVAL  3.5f
#define TURRET_HP              1

#define MAX_WAKE_PARTICLES   96
#define WAKE_EMIT_INTERVAL   0.04f
#define WAKE_LIFETIME        1.1f
#define WAKE_SKI_OFFSET_Y    8.0f

typedef struct {
    Vector2 pos;
    bool active;
} Bullet;

typedef struct {
    Vector2 pos;
    float baseY;
    float t;
    int hp;
    bool active;
} Enemy;

typedef struct {
    Vector2 pos;
    Vector2 vel;
    bool active;
} Torpedo;

typedef struct {
    Vector2 pos;
    float age;
    bool active;
} WakeParticle;

typedef struct {
    Vector2 pos;
    int hp;
    bool active;
} Turret;

void MovePlayer(Vector2 *player, float inputX, float inputY, float speed, float dt, float halfW, float halfH);
float AdvanceOceanScroll(float oceanScroll, float dt, float tileWidth);

bool TryEmitWakeParticle(WakeParticle wake[], int count, Vector2 pos);
void UpdateWakeParticles(WakeParticle wake[], int count, float dt);

bool TrySpawnBullet(Bullet bullets[], int count, Vector2 pos);
void UpdateBullets(Bullet bullets[], int count, float dt);

bool TrySpawnEnemy(Enemy enemies[], int count, float baseY);
void UpdateEnemies(Enemy enemies[], int count, float dt);
void ResolveBulletEnemyCollisions(Bullet bullets[], int bulletCount, Enemy enemies[], int enemyCount);

Vector2 CalculateTorpedoVelocity(Vector2 spawn, const Turret turrets[], int turretCount);
void FireTorpedo(Torpedo *torpedo, Vector2 spawn, const Turret turrets[], int turretCount);
void UpdateTorpedo(Torpedo *torpedo, float dt);

bool TrySpawnTurret(Turret turrets[], int count, float y);
void UpdateTurrets(Turret turrets[], int count, float dt);
void ResolveTorpedoTurretCollision(Torpedo *torpedo, Turret turrets[], int turretCount);

#endif
