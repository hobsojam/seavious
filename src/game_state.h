#ifndef SEAVIOUS_GAME_STATE_H
#define SEAVIOUS_GAME_STATE_H

#include "gameplay.h"

#define MAX_EXPLOSION_EFFECTS 32
#define MAX_SURFACE_WRECKS 16
#define PLAYER_DEATH_DURATION 0.60f
#define PLAYER_SPEED 120.0f

typedef enum {
    EXPLOSION_AIR_TARGET,
    EXPLOSION_SURFACE_TARGET,
    EXPLOSION_PLAYER
} ExplosionType;

typedef struct {
    Vector2 pos;
    float age;
    float lifetime;
    float radius;
    ExplosionType type;
    bool active;
} ExplosionEffect;

typedef struct {
    Vector2 pos;
    float radius;
    SurfaceTargetType type;
    bool active;
} SurfaceWreck;

// Everything a single run owns: player, scoring, run flow flags, weapon
// state, and every entity pool. One ResetRunState call starts a fresh run.
typedef struct {
    Vector2 player;
    float oceanScroll;
    float oceanOverlayScroll;
    int score;
    int lives;
    bool gameOver;
    // Music trigger only for now: the boss fight structure (see TODO) will
    // own setting/clearing this when it exists.
    bool bossActive;
    // Stage script progress: total scroll distance is the trigger currency
    // for the compiled spawn table; the cursor walks the sorted events.
    // bossLock (map end reached) freezes scroll, and with it all
    // water-anchored drift and further spawns.
    float scrollDistance;
    int stageCursor;
    bool bossLock;
    float respawnInvulnerability;
    bool playerDestroyed;
    float playerDeathTimer;
    GameEventQueue gameEvents;

    Bullet bullets[MAX_BULLETS];
    float fireTimer;

    EnemyBullet enemyBullets[MAX_ENEMY_BULLETS];
    ExplosionEffect explosions[MAX_EXPLOSION_EFFECTS];
    SurfaceWreck wrecks[MAX_SURFACE_WRECKS];

    AirTarget airTargets[MAX_AIR_TARGETS];

    Torpedo torpedo;
    float torpedoCooldown;
    TorpedoImpactType torpedoImpactType;
    Vector2 torpedoImpactPos;
    float torpedoImpactTimer;

    SurfaceTarget surfaceTargets[MAX_SURFACE_TARGETS];

    WakeParticle wake[MAX_WAKE_PARTICLES];
    float wakeEmitTimer;
} GameState;

void ResetRunState(GameState *state);
void BeginPlayerDeath(GameState *state);

bool TrySpawnExplosion(ExplosionEffect effects[], Vector2 pos, ExplosionType type, float radius, float lifetime);
bool TrySpawnSurfaceWreck(SurfaceWreck wrecks[], Vector2 pos, SurfaceTargetType type, float radius);
void SpawnTargetDestructionEffects(const GameEventQueue *events, ExplosionEffect explosions[], SurfaceWreck wrecks[]);
void UpdateExplosionEffects(ExplosionEffect effects[], float dt);
void UpdateSurfaceWrecks(SurfaceWreck wrecks[], float dt);

#endif
