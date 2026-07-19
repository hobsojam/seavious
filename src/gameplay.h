#ifndef SEAVIOUS_GAMEPLAY_H
#define SEAVIOUS_GAMEPLAY_H

#include "raylib.h"
#include "stage_data.h"

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

// Every acting enemy approaches fully pre-armed and takes its first
// action (shot, launch) right when it is 10% of the way into the play
// area (playtest: waiting for "fully on-screen" plus a full first
// interval meant most enemies only became threats far too deep into
// the screen). The Interceptor's mid-screen sniper trigger is the
// deliberate exception.
#define ENEMY_ACTIVATION_X (0.9f * GAME_WIDTH)

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
// Fires its single shot at mid-screen (playtest moved the trigger line
// up from two-thirds crossed so the shot comes sooner). The shot is
// aimed at the player and travels at twice the shared projectile
// speed: at 1x it barely outran the 140 px/s craft that fired it and
// read as meaningless in the playtest.
#define INTERCEPTOR_FIRE_X (GAME_WIDTH / 2.0f)
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

// Scavenged mortar (player weapon once salvaged from the Stage 1 boss):
// a lobbed shell to a shorter fixed-range reticle that ignores land
// edges entirely - arcing over blockers is the weapon's point - with an
// area blast where it lands. One shell in flight + cooldown, like the
// torpedo. Fire key: Left Shift or X (movement-style dual binding).
#define PLAYER_MORTAR_COOLDOWN 2.5f
#define PLAYER_MORTAR_MAX_RANGE 120.0f
#define PLAYER_MORTAR_AIR_TIME 1.0f
#define PLAYER_MORTAR_BLAST_RADIUS 22.0f
#define PLAYER_MORTAR_BLAST_DURATION 0.30f

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
// Mines arm a little before physical contact, then leave a short-lived
// danger circle. Both radii include the player's own hit radius at use.
#define MINE_PROXIMITY_RADIUS  34.0f
#define MINE_BLAST_RADIUS      30.0f
#define MINE_BLAST_DURATION     0.38f
#define MAX_MINE_BLASTS MAX_SURFACE_TARGETS

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
#define MOBILE_PLATFORM_SINK_DURATION    1.15f
#define MOBILE_PLATFORM_SINK_DEPTH        9.0f
#define SCORE_MOBILE_PLATFORM 500

// Stage 2 land class (green, mounted on terrain hardpoints): reachable
// by the mortar alone - no gun or torpedo collision path exists, the
// same structural separation that keeps bullets off surface targets.
// Land targets never contact-kill (flying over land is always safe, the
// "no terrain crashes" rule extended to what stands on it).
#define MAX_LAND_TARGETS 8

// Mortar Battery: the enemy counterpart of the player's mortar - lobs
// an arcing shell (red, enemy ordnance) at the player with the shadow
// telegraph the Leviathan taught. One shell in flight per battery.
#define MORTAR_BATTERY_RADIUS 10.0f
#define MORTAR_BATTERY_FIRE_INTERVAL 3.5f
#define MORTAR_BATTERY_HP 1
#define LAND_MORTAR_AIR_TIME 1.2f
#define LAND_MORTAR_BLAST_RADIUS 24.0f
#define LAND_MORTAR_BLAST_DURATION 0.30f
#define SCORE_MORTAR_BATTERY 600

// Drone Bunker: the Relay Node's land cousin - hatches Skimmer Drones
// from behind shorelines where the torpedo can't retaliate.
#define DRONE_BUNKER_RADIUS 11.0f
#define DRONE_BUNKER_LAUNCH_INTERVAL 3.0f
#define DRONE_BUNKER_MAX_DRONES 3
#define DRONE_BUNKER_HP 1
#define SCORE_DRONE_BUNKER 500

// Stage 1 boss (Leviathan) parts: the dual-targeting rule made literal.
// Pods are gun-weak (magenta, air-class), hull sections are torpedo-weak
// (amber, surface-class), and the core - revealed once both hull
// sections are gone - is weak to both. HP shares one currency for the
// boss bar: pods and the core count gun hits directly, hull sections
// count torpedoes (BOSS_HULL_SECTION_TORPEDO_WORTH gun-units each on
// the bar), and a torpedo into the core removes
// BOSS_CORE_TORPEDO_DAMAGE gun-units so "4 torpedoes or ~24 gun hits;
// mixing works" holds.
typedef enum {
    BOSS_PART_POD_FORE,
    BOSS_PART_POD_AFT,
    BOSS_PART_HULL_FORE,
    BOSS_PART_HULL_AFT,
    BOSS_PART_CORE,
    BOSS_PART_COUNT
} BossPartId;

#define BOSS_POD_HP 12
#define BOSS_HULL_SECTION_HP 2
#define BOSS_CORE_HP 24
#define BOSS_CORE_TORPEDO_DAMAGE 6
#define BOSS_HULL_SECTION_TORPEDO_WORTH 6
#define BOSS_POD_FIRE_INTERVAL 2.0f
// The hull sections are SAM batteries: launch cells through the armor
// belt (their open doors are exactly the gap a torpedo fits into - the
// weak-point fiction). The player-facing battery launches one homing
// missile per interval; the missile is airborne, so the gun can shoot
// it down (strict class mapping holds), it flies slightly faster than
// the skimmer with a capped turn rate so it can be out-turned, and it
// fizzles when its fuel runs out.
#define BOSS_SAM_INTERVAL 3.0f
#define MAX_BOSS_MISSILES 4
#define BOSS_MISSILE_SPEED 130.0f
#define BOSS_MISSILE_TURN_RATE 140.0f
#define BOSS_MISSILE_LIFETIME 4.0f
#define BOSS_MISSILE_RADIUS 4.0f
#define SCORE_BOSS_MISSILE 50
#define BOSS_MORTAR_INTERVAL 4.0f
#define BOSS_MORTAR_INTERVAL_CORE_EXPOSED 2.8f

// Fortress atoll (Stage 2 boss): the Leviathan part skeleton re-armed
// as a static fortified island. Part HP is retuned per weapon class -
// pods stay gun-weak, ring batteries take a few mortar blasts, the core
// eats torpedoes through the gate windows.
#define FORTRESS_POD_HP 8
#define FORTRESS_RING_BATTERY_HP 3
#define FORTRESS_CORE_HP 18
// Ring batteries return the stage's own mortar language: the same
// shadow-telegraphed lob as the map's Mortar Battery, staggered per
// battery (BOSS_PART_FIRST_SHOT_DELAY) so shadows interleave with the
// central atoll's lob instead of landing as one wall.
#define FORTRESS_RING_MORTAR_INTERVAL 4.5f
// Sea gates cycle from the fight's first seconds so the player learns
// the rhythm while the outer fight is still on; the open dwell runs
// longer than the closed one so the torpedo window is a timing skill,
// not a coin flip against the 1.5s reload.
#define FORTRESS_GATE_OPEN_DURATION 2.6f
#define FORTRESS_GATE_CLOSED_DURATION 2.2f
#define BOSS_MORTAR_AIR_TIME 1.2f
#define BOSS_MORTAR_BLAST_RADIUS 24.0f
#define BOSS_MORTAR_BLAST_DURATION 0.30f
#define SCORE_BOSS_POD 1000
#define SCORE_BOSS_HULL_SECTION 1000
#define SCORE_BOSS_CORE 2000

#define MAX_GAME_EVENTS 64

#define MAX_WAKE_PARTICLES   96
#define WAKE_EMIT_INTERVAL   0.04f
#define WAKE_LIFETIME        1.1f
#define PLAYER_WAKE_COLUMNS  3
#define PLAYER_WAKE_ROWS     5
#define PLAYER_WAKE_PER_TICK 2

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

// One lobbed mortar shell: launch -> target over a fixed air time, then a
// short area blast. Shared by the boss's turret and the player's scavenged
// one; only the timings/radii and what the blast hurts differ.
typedef struct {
    Vector2 launch;
    Vector2 target;
    float t;
    float blastT;
    bool landed;
    bool active;
} MortarShell;

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
    // Boss-launched boats move under the boss lock; ordinary stage water
    // targets remain frozen with the map at that point.
    bool fortressSortie;
    bool active;
} SurfaceTarget;

// Mine detonations are hazards as well as effects: their visual ring is
// mirrored by this short-lived damage pool, so a nearby player can still
// be caught after the proximity fuse fires.
typedef struct {
    Vector2 pos;
    float remaining;
    bool active;
} MineBlast;

typedef enum {
    LAND_TARGET_MORTAR_BATTERY,
    LAND_TARGET_DRONE_BUNKER
} LandTargetType;

// World-anchored like surface targets (drifts with the scroll, so it
// stays glued to the terrain cell it mounted on). The battery's shell
// lives in its slot; spawning prefers slots whose shell has also died
// so a mid-air shell isn't clipped by pool reuse.
typedef struct {
    LandTargetType type;
    Vector2 pos;
    float radius;
    int hp;
    float fireTimer;
    MortarShell shell;
    bool active;
} LandTarget;

typedef enum {
    GAME_EVENT_AIR_TARGET_DESTROYED,
    GAME_EVENT_SURFACE_TARGET_DESTROYED,
    GAME_EVENT_GUN_FIRED,
    GAME_EVENT_TORPEDO_FIRED,
    GAME_EVENT_TORPEDO_IMPACT,
    GAME_EVENT_PLAYER_DEATH,
    GAME_EVENT_RUN_RESTARTED,
    GAME_EVENT_DRONE_LAUNCHED,
    GAME_EVENT_MINE_DETONATED,
    GAME_EVENT_BOSS_PART_DESTROYED,
    GAME_EVENT_BOSS_MISSILE_DOWNED,
    GAME_EVENT_BOSS_DEFEATED,
    // The fortress's sea gates announce every cycle edge (sound + the
    // gate visual) so the torpedo window reads as a learnable rhythm.
    GAME_EVENT_BOSS_GATES_OPENED,
    GAME_EVENT_BOSS_GATES_CLOSED,
    GAME_EVENT_MORTAR_FIRED,
    GAME_EVENT_MORTAR_BLAST,
    GAME_EVENT_MORTAR_SALVAGED,
    GAME_EVENT_TARGETING_COMPUTER_SALVAGED,
    // One per shot/volley (a Gunship fan is one event, matching how the
    // volley reads), shared by every bullet-firing enemy - the audible
    // counterpart of the universal red-diamond projectile.
    GAME_EVENT_ENEMY_FIRED,
    GAME_EVENT_SAM_LAUNCHED,
    GAME_EVENT_LAND_TARGET_DESTROYED
} GameEventType;

typedef struct {
    GameEventType type;
    Vector2 pos;
    union {
        AirTargetType airTarget;
        SurfaceTargetType surfaceTarget;
        LandTargetType landTarget;
        TorpedoImpactType torpedoImpact;
        BossPartId bossPart;
    } target;
} GameEvent;

typedef struct {
    GameEvent items[MAX_GAME_EVENTS];
    int count;
} GameEventQueue;

// drift is the current stage's constant environmental push (px/s), {0, 0}
// outside Stage 3 or once the stabilizer is held (see stage_data.h).
void MovePlayer(Vector2 *player, float inputX, float inputY, float speed, float dt, float halfW, float halfH,
    Vector2 drift);
float AdvanceOceanScroll(float oceanScroll, float dt, float tileWidth);

bool TryEmitWakeParticle(WakeParticle wake[], int count, Vector2 pos);
// Emits the same two particles per cadence as the original ski-tip wake, but
// cycles them through the whole rear half of the player's hull. Returns the
// next position in that spread so callers can retain a continuous pattern.
int EmitPlayerWake(WakeParticle wake[], int count, Vector2 player, float halfW, float halfH,
    int phase, float jitterX, float jitterY);
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
bool DetonateNearbyMines(SurfaceTarget targets[], int count, Vector2 playerPos, float playerRadius,
    GameEventQueue *events);
void SpawnMineBlastsFromEvents(MineBlast blasts[], int blastCount, const GameEventQueue *events);
void UpdateMineBlasts(MineBlast blasts[], int count, float dt);
bool ResolveMineBlastPlayerHit(const MineBlast blasts[], int count, Vector2 playerPos, float playerRadius);

bool TrySpawnSkimmerDrone(AirTarget targets[], int count, float baseY);
bool TrySpawnSkimmerDroneAt(AirTarget targets[], int count, float baseY, float spawnXOffset);
int SpawnSkimmerDroneLine(AirTarget targets[], int count, float baseY);
int SpawnSkimmerDroneV(AirTarget targets[], int count, float baseY);
bool TrySpawnInterceptor(AirTarget targets[], int count, float baseY);
bool TrySpawnGunship(AirTarget targets[], int count, float baseY);
void UpdateAirTargets(AirTarget targets[], int count, float dt);
void UpdateAirTargetFire(AirTarget targets[], int count, float dt, Vector2 playerPos,
    EnemyBullet bullets[], int bulletCount, GameEventQueue *events);
void ResolveBulletAirTargetCollisions(Bullet bullets[], int bulletCount, AirTarget targets[], int targetCount,
    GameEventQueue *events);

// Terrain: stage-data land footprints drifting with the scroll. Land never
// collides with the ship or gun (not Scramble - no terrain crashes); it
// only interacts with the torpedo lane: armed torpedoes detonate at the
// first land edge, unarmed ones fizzle, and the reticle clamps to that
// edge so a blocked lane reads before firing. Footprints live in world
// scroll-distance coordinates, so screen position derives purely from
// scrollDistance - no entity pool, and the boss lock freezes land with
// the rest of the water for free.
Rectangle TerrainScreenRect(StageTerrainFootprint footprint, float scrollDistance);
Vector2 CalculateTorpedoReticle(Vector2 spawn, const StageTerrainFootprint terrain[], int terrainCount,
    float scrollDistance);
// Generic screen-space blockers under the same rules as land, for solid
// obstacles that aren't stage terrain (the boss's armored hull).
Vector2 ClampReticleToRects(Vector2 spawn, Vector2 reticle, const Rectangle rects[], int rectCount);
TorpedoImpact ResolveTorpedoRectCollision(Torpedo *torpedo, const Rectangle rects[], int rectCount);
Vector2 CalculateLeadTorpedoVelocity(Vector2 spawn, const SurfaceTarget targets[], int targetCount);
void FireFixedRangeTorpedo(Torpedo *torpedo, Vector2 spawn, const StageTerrainFootprint terrain[], int terrainCount,
    float scrollDistance);
// driftY compensates the launch heading so the lead solve still lands on
// its true intercept point despite the crosswind UpdateTorpedo applies
// in flight; the reticle (CalculateLeadTorpedoVelocity above) intentionally
// keeps showing the uncompensated aim - see docs/game-design.md "Stage 3".
void FireLeadTorpedo(Torpedo *torpedo, Vector2 spawn, const SurfaceTarget targets[], int targetCount, float driftY);
// A fixed-lane torpedo gets no compensation: driftY here is what makes an
// unled shot visibly miss its promised lane in a crosswind.
TorpedoImpact UpdateTorpedo(Torpedo *torpedo, float dt, float driftY);
TorpedoImpact ResolveTorpedoTerrainCollision(Torpedo *torpedo, const StageTerrainFootprint terrain[],
    int terrainCount, float scrollDistance);

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
    EnemyBullet bullets[], int bulletCount, GameEventQueue *events);
TorpedoImpact ResolveTorpedoSurfaceTargetCollision(Torpedo *torpedo, SurfaceTarget targets[], int targetCount,
    GameEventQueue *events);
void ResolveTorpedoExplosion(Vector2 pos, SurfaceTarget targets[], int targetCount, GameEventQueue *events);

// Player mortar. The blast damages surface targets too - settled by
// playtest: harder to land than the torpedo, so it earns the overlap on
// the water class. Stage 2's land targets will additionally be
// mortar-only.
Vector2 CalculateMortarReticle(Vector2 spawn);
Vector2 CalculateMortarGroundPosition(const MortarShell *shell, float airTime);
void FirePlayerMortar(MortarShell *shell, Vector2 spawn, Vector2 target);
bool UpdatePlayerMortarShell(MortarShell *shell, float dt, float scrollDt);
void ResolveMortarBlastSurfaceTargets(Vector2 pos, SurfaceTarget targets[], int targetCount, GameEventQueue *events);

// Stage 2 land class (see the constants block above). Batteries and
// bunkers hold pre-armed at the shared activation line like every other
// acting enemy; only the player's mortar blast can damage them.
bool TrySpawnMortarBattery(LandTarget targets[], int count, float laneY);
bool TrySpawnDroneBunker(LandTarget targets[], int count, float laneY);
bool DamageLandTarget(LandTarget *target, int damage, GameEventQueue *events);
void UpdateLandTargets(LandTarget targets[], int count, float dt);
// Battery fire control + shell flight: lobs at the player's (clamped)
// position, pushes MORTAR_FIRED on launch and MORTAR_BLAST on landing.
// Shells keep flying after their battery dies (boss rule).
void UpdateMortarBatteries(LandTarget targets[], int count, float dt, Vector2 playerPos,
    GameEventQueue *events);
bool ResolveLandMortarBlastPlayerHit(const LandTarget targets[], int count, Vector2 playerPos,
    float playerRadius);
void UpdateDroneBunkerLaunches(LandTarget targets[], int count, float dt, AirTarget airTargets[],
    int airCount, GameEventQueue *events);
void ResolveMortarBlastLandTargets(Vector2 pos, LandTarget targets[], int targetCount,
    GameEventQueue *events);

#endif
