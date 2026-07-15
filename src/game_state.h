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

// Stage 1 boss (Leviathan) run state. The fight is part-driven, not
// phase-scripted: parts fire until individually destroyed, the core kill
// ends it (see README "Stage 1 boss design"). These phases only sequence
// the bookends - entrance slide-in, death chain, and the salvage beat
// that awards the mortar.
#define MAX_MORTAR_SHELLS 3
#define BOSS_ENTRANCE_DURATION 3.0f
#define BOSS_DEATH_DURATION 2.4f
#define BOSS_SALVAGE_APPROACH_DURATION 1.6f
#define BOSS_SALVAGE_DOCK_DURATION 1.8f
// Hull sprite (200x120) top-left when parked: content on the right ~40%
// of the play area, vertically centered.
#define BOSS_PARKED_X 312.0f
#define BOSS_HULL_TOP_Y 116.0f

typedef enum {
    BOSS_PHASE_INACTIVE,
    BOSS_PHASE_ENTERING,
    BOSS_PHASE_FIGHTING,
    BOSS_PHASE_DYING,
    BOSS_PHASE_SALVAGE_APPROACH,
    BOSS_PHASE_SALVAGE_DOCK,
    BOSS_PHASE_CLEARED
} BossPhase;

// One lobbed mortar shell: launch -> target over a fixed air time (the
// dodge window, telegraphed by the shadow at the target), then a short
// area blast that can kill the player.
typedef struct {
    Vector2 launch;
    Vector2 target;
    float t;
    float blastT;
    bool landed;
    bool active;
} MortarShell;

typedef struct {
    BossPhase phase;
    float phaseTimer;
    Vector2 hullPos;        // top-left of the 200x120 hull sprite
    float settleOffset;     // wreck sits a few px lower after the core dies
    int partHp[BOSS_PART_COUNT];
    float partFireTimer[BOSS_PART_COUNT];
    bool coreExposed;
    float mortarTimer;
    MortarShell shells[MAX_MORTAR_SHELLS];
    int deathExplosionsSpawned;
    Vector2 salvageStart;   // player position when the autopilot takes over
    Vector2 salvageDomePos; // mortar dome while lifting/docking
} BossState;

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
    // Raised by the salvage sequence after the boss falls; gates the
    // stage-clear overlay and the restart input alongside gameOver.
    bool stageClear;
    BossState boss;
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
