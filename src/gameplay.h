#ifndef SEAVIOUS_GAMEPLAY_H
#define SEAVIOUS_GAMEPLAY_H

#include "raylib.h"

#define GAME_WIDTH    512
#define GAME_HEIGHT   384
#define HUD_HEIGHT    32
#define PLAY_HEIGHT   (GAME_HEIGHT - HUD_HEIGHT)
#define WINDOW_SCALE  2
#define OCEAN_SCROLL_SPEED 40.0f
#define OCEAN_OVERLAY_SPEED_SCALE 1.15f

#define MAX_BULLETS         32
#define BULLET_SPEED        280.0f
#define BULLET_FIRE_INTERVAL 0.15f
#define BULLET_RADIUS       3.0f

#define MAX_ENEMY_BULLETS           24
#define ENEMY_BULLET_SPEED         160.0f
#define ENEMY_BULLET_RADIUS          3.0f

#define MAX_AIR_TARGETS          16
#define SKIMMER_DRONE_RADIUS         6.0f
#define SKIMMER_DRONE_SPEED          60.0f
#define SKIMMER_DRONE_SINE_AMPLITUDE 24.0f
#define SKIMMER_DRONE_SINE_FREQUENCY 2.0f
#define SKIMMER_DRONE_HP             1
#define SCORE_SKIMMER_DRONE  100

#define INTERCEPTOR_RADIUS   8.0f
#define INTERCEPTOR_SPEED  140.0f
#define INTERCEPTOR_HP       1
// Fires its single shot once it has crossed two-thirds of the screen
// width (flying right-to-left, that line sits at x = width/3). The shot
// is aimed at the player and travels at twice the shared projectile
// speed: at 1x it barely outran the 140 px/s craft that fired it and
// read as meaningless in the playtest.
#define INTERCEPTOR_FIRE_X (GAME_WIDTH / 3.0f)
#define INTERCEPTOR_SHOT_SPEED (2.0f * ENEMY_BULLET_SPEED)
#define SCORE_INTERCEPTOR  200

#define GUNSHIP_RADIUS          10.0f
// Half the Interceptor's pace: the bulk should read in motion.
#define GUNSHIP_SPEED           70.0f
#define GUNSHIP_HP               3
#define GUNSHIP_FIRE_INTERVAL    2.4f
#define GUNSHIP_FAN_SPREAD_DEG  16.0f
#define SCORE_GUNSHIP  500

#define TORPEDO_SPEED    200.0f
#define TORPEDO_COOLDOWN 1.5f
#define TORPEDO_WIDTH    12.0f
#define TORPEDO_HEIGHT   4.0f
#define TORPEDO_MAX_RANGE 170.0f
#define TORPEDO_RETICLE_SCREEN_MARGIN 24.0f
#define TORPEDO_ARMING_DISTANCE 48.0f
#define TORPEDO_SPLASH_RADIUS 20.0f
#define TORPEDO_DIRECT_HIT_RADIUS (TORPEDO_WIDTH / 2.0f)

#define MAX_SURFACE_TARGETS            8
#define CASEMATE_RADIUS          9.0f
#define CASEMATE_FIRE_INTERVAL   1.5f
#define CASEMATE_HP              1
#define SCORE_CASEMATE  300

#define TRACKING_TURRET_RADIUS         10.0f
#define TRACKING_TURRET_FIRE_INTERVAL   2.0f
// Fraction of the player's velocity fed into the intercept solve: 1.0 is a
// perfect (psychic) intercept, 0 aims at the current position.
#define TRACKING_TURRET_LEAD_FACTOR     0.1f
#define TRACKING_TURRET_HP              1
#define SCORE_TRACKING_TURRET  400

#define RELAY_NODE_RADIUS          12.0f
#define RELAY_NODE_LAUNCH_INTERVAL  2.5f
#define RELAY_NODE_MAX_DRONES       3
#define RELAY_NODE_HP               1
#define SCORE_RELAY_NODE  400

#define MINE_RADIUS  7.0f
#define MINE_HP      1
#define SCORE_MINE  100

#define MOBILE_PLATFORM_RADIUS          12.0f
// Absolute leftward speed (1.5x the 40 px/s scroll): the one self-propelled
// ground unit has to visibly move relative to the water.
#define MOBILE_PLATFORM_SPEED           60.0f
#define MOBILE_PLATFORM_FIRE_INTERVAL    3.0f
#define MOBILE_PLATFORM_FAN_SPREAD_DEG  14.0f
#define MOBILE_PLATFORM_HP               2
// Stern (right edge) of the 36px-wide bow-left hull, where the wake trails.
#define MOBILE_PLATFORM_STERN_OFFSET    16.0f
#define MOBILE_PLATFORM_WAKE_INTERVAL    0.07f
#define SCORE_MOBILE_PLATFORM 500

#define MAX_GAME_EVENTS 64

#define MAX_WAKE_PARTICLES   96
#define WAKE_EMIT_INTERVAL   0.04f
#define WAKE_LIFETIME        1.1f
#define WAKE_SKI_OFFSET_Y    8.0f

#define PLAYER_START_X 48.0f
#define PLAYER_START_Y (PLAY_HEIGHT / 2.0f)
#define PLAYER_HIT_RADIUS 10.0f
#define PLAYER_RESPAWN_INVULNERABILITY 1.0f

typedef struct {
    Vector2 pos;
    bool active;
} Bullet;

typedef struct {
    Vector2 pos;
    Vector2 vel;
    bool active;
} EnemyBullet;

typedef enum {
    AIR_TARGET_SKIMMER_DRONE,
    AIR_TARGET_INTERCEPTOR,
    AIR_TARGET_GUNSHIP
} AirTargetType;

typedef struct {
    AirTargetType type;
    Vector2 pos;
    float baseY;
    float t;
    float radius;
    int hp;
    // 0 = unowned; otherwise 1 + the owning Relay Node's pool index, so
    // relays can cap how many of *their* drones are alive at once.
    int ownerId;
    // Gunship volley cadence; unused by the one-shot Interceptor (hasFired)
    // and the unarmed Skimmer Drone.
    float fireTimer;
    bool hasFired;
    bool active;
} AirTarget;

typedef struct {
    Vector2 pos;
    Vector2 vel;
    Vector2 target;
    float distanceTravelled;
    bool armed;
    bool active;
} Torpedo;

typedef enum {
    TORPEDO_IMPACT_NONE,
    TORPEDO_IMPACT_DIRECT,
    TORPEDO_IMPACT_EXPLOSION
} TorpedoImpactType;

typedef struct {
    TorpedoImpactType type;
    Vector2 pos;
} TorpedoImpact;

typedef struct {
    Vector2 pos;
    float age;
    bool active;
} WakeParticle;

typedef enum {
    SURFACE_TARGET_CASEMATE,
    SURFACE_TARGET_TRACKING_TURRET,
    SURFACE_TARGET_RELAY_NODE,
    SURFACE_TARGET_MINE,
    SURFACE_TARGET_MOBILE_PLATFORM
} SurfaceTargetType;

typedef struct {
    SurfaceTargetType type;
    Vector2 pos;
    float radius;
    int hp;
    float fireTimer;
    // Cadence for the Mobile Platform's stern trail; unused by anchored types.
    float wakeTimer;
    Vector2 aimDirection;
    bool active;
} SurfaceTarget;

typedef enum {
    GAME_EVENT_AIR_TARGET_DESTROYED,
    GAME_EVENT_SURFACE_TARGET_DESTROYED,
    GAME_EVENT_GUN_FIRED,
    GAME_EVENT_TORPEDO_FIRED,
    GAME_EVENT_TORPEDO_IMPACT,
    GAME_EVENT_PLAYER_DEATH,
    GAME_EVENT_RUN_RESTARTED,
    GAME_EVENT_DRONE_LAUNCHED,
    GAME_EVENT_MINE_DETONATED
} GameEventType;

typedef struct {
    GameEventType type;
    Vector2 pos;
    union {
        AirTargetType airTarget;
        SurfaceTargetType surfaceTarget;
        TorpedoImpactType torpedoImpact;
    } target;
} GameEvent;

typedef struct {
    GameEvent items[MAX_GAME_EVENTS];
    int count;
} GameEventQueue;

void MovePlayer(Vector2 *player, float inputX, float inputY, float speed, float dt, float halfW, float halfH);
float AdvanceOceanScroll(float oceanScroll, float dt, float tileWidth);

bool TryEmitWakeParticle(WakeParticle wake[], int count, Vector2 pos);
void UpdateWakeParticles(WakeParticle wake[], int count, float dt);

bool TrySpawnBullet(Bullet bullets[], int count, Vector2 pos);
void UpdateBullets(Bullet bullets[], int count, float dt);
bool TrySpawnEnemyBullet(EnemyBullet bullets[], int count, Vector2 pos, Vector2 vel);
void UpdateEnemyBullets(EnemyBullet bullets[], int count, float dt);
bool ResolveEnemyBulletPlayerCollision(EnemyBullet bullets[], int bulletCount, Vector2 playerPos, float playerRadius);

bool PushGameEvent(GameEventQueue *events, GameEvent event);
bool DamageAirTarget(AirTarget *target, int damage, GameEventQueue *events);
bool DamageSurfaceTarget(SurfaceTarget *target, int damage, GameEventQueue *events);
int ScoreGameEvents(const GameEventQueue *events);
bool ResolvePlayerContactDamage(Vector2 playerPos, float playerRadius, const AirTarget airTargets[], int airCount,
    SurfaceTarget surfaceTargets[], int surfaceCount, GameEventQueue *events);

bool TrySpawnSkimmerDrone(AirTarget targets[], int count, float baseY);
bool TrySpawnSkimmerDroneAt(AirTarget targets[], int count, float baseY, float spawnXOffset);
int SpawnSkimmerDroneLine(AirTarget targets[], int count, float baseY);
int SpawnSkimmerDroneV(AirTarget targets[], int count, float baseY);
bool TrySpawnInterceptor(AirTarget targets[], int count, float baseY);
bool TrySpawnGunship(AirTarget targets[], int count, float baseY);
void UpdateAirTargets(AirTarget targets[], int count, float dt);
void UpdateAirTargetFire(AirTarget targets[], int count, float dt, Vector2 playerPos,
    EnemyBullet bullets[], int bulletCount);
void ResolveBulletAirTargetCollisions(Bullet bullets[], int bulletCount, AirTarget targets[], int targetCount,
    GameEventQueue *events);

Vector2 CalculateTorpedoReticle(Vector2 spawn);
Vector2 CalculateLeadTorpedoVelocity(Vector2 spawn, const SurfaceTarget targets[], int targetCount);
void FireFixedRangeTorpedo(Torpedo *torpedo, Vector2 spawn);
void FireLeadTorpedo(Torpedo *torpedo, Vector2 spawn, const SurfaceTarget targets[], int targetCount);
TorpedoImpact UpdateTorpedo(Torpedo *torpedo, float dt);

bool TrySpawnCasemate(SurfaceTarget targets[], int count, float y);
bool TrySpawnTrackingTurret(SurfaceTarget targets[], int count, float y);
bool TrySpawnRelayNode(SurfaceTarget targets[], int count, float y);
bool TrySpawnMine(SurfaceTarget targets[], int count, float y);
bool TrySpawnMobilePlatform(SurfaceTarget targets[], int count, float y);
void EmitMobilePlatformWake(SurfaceTarget targets[], int count, WakeParticle wake[], int wakeCount, float dt);
void UpdateRelayNodeLaunches(SurfaceTarget targets[], int count, float dt, AirTarget airTargets[], int airCount,
    GameEventQueue *events);
void UpdateSurfaceTargets(SurfaceTarget targets[], int count, float dt);
void UpdateSurfaceTargetFire(SurfaceTarget targets[], int count, float dt, Vector2 playerPos, Vector2 playerVelocity,
    EnemyBullet bullets[], int bulletCount);
TorpedoImpact ResolveTorpedoSurfaceTargetCollision(Torpedo *torpedo, SurfaceTarget targets[], int targetCount,
    GameEventQueue *events);
void ResolveTorpedoExplosion(Vector2 pos, SurfaceTarget targets[], int targetCount, GameEventQueue *events);

#endif
