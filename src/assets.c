#include "assets.h"

enum { TERRAIN_INTERIOR_INSET = 10 };

static Image CreateTerrainInterior(Image source) {
    Image interior = ImageCopy(source);
    ImageFormat(&interior, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    Color *pixels = interior.data;
    int pixelCount = interior.width * interior.height;
    unsigned char *alpha = MemAlloc((unsigned int)pixelCount);
    unsigned char *nextAlpha = MemAlloc((unsigned int)pixelCount);

    for (int i = 0; i < pixelCount; i++) alpha[i] = pixels[i].a;
    for (int pass = 0; pass < TERRAIN_INTERIOR_INSET; pass++) {
        for (int i = 0; i < pixelCount; i++) nextAlpha[i] = alpha[i];
        for (int y = 0; y < interior.height; y++) {
            for (int x = 0; x < interior.width; x++) {
                int i = y * interior.width + x;
                if (alpha[i] == 0) continue;
                bool edge = x == 0 || y == 0
                    || x == interior.width - 1 || y == interior.height - 1;
                if (!edge) {
                    edge = alpha[i - 1] == 0 || alpha[i + 1] == 0
                        || alpha[i - interior.width] == 0
                        || alpha[i + interior.width] == 0;
                }
                if (edge) nextAlpha[i] = 0;
            }
        }
        unsigned char *swap = alpha;
        alpha = nextAlpha;
        nextAlpha = swap;
    }
    for (int i = 0; i < pixelCount; i++) pixels[i].a = alpha[i];

    MemFree(nextAlpha);
    MemFree(alpha);
    return interior;
}

static void LoadTerrainSprite(const char *path, Texture2D *texture,
    Texture2D *interiorTexture) {
    Image image = LoadImage(path);
    Rectangle alphaBorder = GetImageAlphaBorder(image, 0.1f);
    ImageCrop(&image, alphaBorder);
    // Preserve the source aspect: the islet picker matches variant
    // shape to island-group shape, and the old square 128x128 resize
    // silently flattened every variant to 1:1 - the picker then always
    // scored a tie and stamped variant 0 down every chain.
    int width = image.height > 0
        ? (int)(128.0f * (float)image.width / (float)image.height + 0.5f) : 128;
    if (width < 64) width = 64;
    if (width > 320) width = 320;
    ImageResizeNN(&image, width, 128);
    Image interior = CreateTerrainInterior(image);
    *texture = LoadTextureFromImage(image);
    *interiorTexture = LoadTextureFromImage(interior);
    UnloadImage(image);
    UnloadImage(interior);
    SetTextureFilter(*texture, TEXTURE_FILTER_POINT);
    SetTextureFilter(*interiorTexture, TEXTURE_FILTER_POINT);
}

static Texture2D LoadTerrainTile(const char *path, int width, int height, bool cropAlpha) {
    Image image = LoadImage(path);
    if (cropAlpha) ImageCrop(&image, GetImageAlphaBorder(image, 0.1f));
    ImageResizeNN(&image, width, height);
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    return texture;
}

static Texture2D LoadChromaKeyTerrainTile(const char *path, int width, int height) {
    Image image = LoadImage(path);
    // The generated hardpoint is kept on a magenta key.  Use a range rather
    // than an exact color match: PNG conversion can shift the keyed pixels
    // by a few values, which would otherwise expose a pink square in game.
    ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    Color *pixels = image.data;
    int pixelCount = image.width * image.height;
    for (int i = 0; i < pixelCount; i++) {
        if (pixels[i].r > 220 && pixels[i].g < 80 && pixels[i].b > 180) {
            pixels[i].a = 0;
        }
    }
    ImageCrop(&image, GetImageAlphaBorder(image, 0.1f));
    ImageResizeNN(&image, width, height);
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    return texture;
}

GameAssets LoadGameAssets(void) {
    GameAssets assets = { 0 };

    assets.playerTex = LoadTexture("assets/sprites/player_ship.png");
    SetTextureFilter(assets.playerTex, TEXTURE_FILTER_POINT);

    assets.droneTex = LoadTexture("assets/sprites/skimmer_drone.png");
    SetTextureFilter(assets.droneTex, TEXTURE_FILTER_POINT);

    assets.interceptorTex = LoadTexture("assets/sprites/interceptor.png");
    SetTextureFilter(assets.interceptorTex, TEXTURE_FILTER_POINT);

    assets.gunshipTex = LoadTexture("assets/sprites/gunship.png");
    SetTextureFilter(assets.gunshipTex, TEXTURE_FILTER_POINT);

    assets.casemateTex = LoadTexture("assets/sprites/casemate.png");
    SetTextureFilter(assets.casemateTex, TEXTURE_FILTER_POINT);

    assets.trackingTurretTex = LoadTexture("assets/sprites/tracking_turret.png");
    SetTextureFilter(assets.trackingTurretTex, TEXTURE_FILTER_POINT);

    assets.relayNodeTex = LoadTexture("assets/sprites/relay_node.png");
    SetTextureFilter(assets.relayNodeTex, TEXTURE_FILTER_POINT);

    assets.mineTex = LoadTexture("assets/sprites/mine.png");
    SetTextureFilter(assets.mineTex, TEXTURE_FILTER_POINT);

    assets.mobilePlatformTex = LoadTexture("assets/sprites/mobile_platform.png");
    SetTextureFilter(assets.mobilePlatformTex, TEXTURE_FILTER_POINT);

    // Stage 2 land class (green family): installations on terrain pads.
    assets.mortarBatteryTex = LoadTexture("assets/sprites/mortar_battery.png");
    SetTextureFilter(assets.mortarBatteryTex, TEXTURE_FILTER_POINT);

    assets.droneBunkerTex = LoadTexture("assets/sprites/drone_bunker.png");
    SetTextureFilter(assets.droneBunkerTex, TEXTURE_FILTER_POINT);

    // Leviathan boss set: the hull base plus its four layered part
    // sprites (see README "Stage 1 boss design").
    assets.leviathanHullTex = LoadTexture("assets/sprites/leviathan_hull.png");
    SetTextureFilter(assets.leviathanHullTex, TEXTURE_FILTER_POINT);

    assets.leviathanPodTex = LoadTexture("assets/sprites/leviathan_pod.png");
    SetTextureFilter(assets.leviathanPodTex, TEXTURE_FILTER_POINT);

    assets.leviathanHullSectionTex = LoadTexture("assets/sprites/leviathan_hull_section.png");
    SetTextureFilter(assets.leviathanHullSectionTex, TEXTURE_FILTER_POINT);

    assets.leviathanMortarTex = LoadTexture("assets/sprites/leviathan_mortar.png");
    SetTextureFilter(assets.leviathanMortarTex, TEXTURE_FILTER_POINT);

    assets.leviathanCoreTex = LoadTexture("assets/sprites/leviathan_core.png");
    SetTextureFilter(assets.leviathanCoreTex, TEXTURE_FILTER_POINT);

    // Each authored silhouette is cropped and downsampled once so its pixel
    // clusters stay readable at the small Stage 1 terrain scale.
    static const char *isletPaths[STAGE1_ISLET_VARIANT_COUNT] = {
        "assets/sprites/stage1_islet.png",
        "assets/sprites/stage1_islet_crescent.png",
        "assets/sprites/stage1_islet_crag.png",
        "assets/sprites/stage1_islet_atoll.png",
        "assets/sprites/stage1_islet_ridge.png",
        "assets/sprites/stage1_islet_long.png",
    };
    for (int i = 0; i < STAGE1_ISLET_VARIANT_COUNT; i++) {
        LoadTerrainSprite(isletPaths[i], &assets.stage1IsletTex[i],
            &assets.stage1IsletInteriorTex[i]);
    }
    assets.terrainGroundTex = LoadTerrainTile("assets/tiles/terrain_ground.png", 128, 128, false);
    assets.terrainShoreTex = LoadTerrainTile("assets/tiles/terrain_shore_top.png", 128, 16, true);
    assets.terrainTileAtlasTex = LoadTerrainTile("assets/tiles/terrain_tile_atlas.png", 1152, 1152, false);
    assets.terrainFeatureAtlasTex = LoadTerrainTile("assets/tiles/terrain_feature_atlas.png", 256, 64, false);
    assets.terrainHardpointTex = LoadChromaKeyTerrainTile("assets/tiles/terrain_island_hardpoint.png", 32, 32);
    assets.terrainBrushTex = LoadChromaKeyTerrainTile("assets/tiles/terrain_brush_cluster.png", 36, 24);
    assets.terrainRockTex = LoadChromaKeyTerrainTile("assets/tiles/terrain_rock_scatter.png", 28, 24);
    assets.terrainTidePoolTex = LoadChromaKeyTerrainTile("assets/tiles/terrain_tide_pool.png", 28, 18);

    assets.oceanTex = LoadTexture("assets/tiles/ocean.png");
    SetTextureFilter(assets.oceanTex, TEXTURE_FILTER_POINT);
    SetTextureWrap(assets.oceanTex, TEXTURE_WRAP_REPEAT);

    // Foam-glint parallax layer: scrolls slightly faster than the base
    // water and has a different tile size, so neither layer's repeat ever
    // sits still relative to the other (baked glints in the base tile made
    // the repeat trackable).
    assets.oceanOverlayTex = LoadTexture("assets/tiles/ocean_overlay.png");
    SetTextureFilter(assets.oceanOverlayTex, TEXTURE_FILTER_POINT);
    SetTextureWrap(assets.oceanOverlayTex, TEXTURE_WRAP_REPEAT);

    assets.titleLogoTex = LoadTexture("assets/sprites/title_logo.png");
    SetTextureFilter(assets.titleLogoTex, TEXTURE_FILTER_POINT);

    return assets;
}

void UnloadGameAssets(GameAssets *assets) {
    UnloadTexture(assets->titleLogoTex);
    UnloadTexture(assets->oceanOverlayTex);
    UnloadTexture(assets->oceanTex);
    for (int i = 0; i < STAGE1_ISLET_VARIANT_COUNT; i++) {
        UnloadTexture(assets->stage1IsletTex[i]);
        UnloadTexture(assets->stage1IsletInteriorTex[i]);
    }
    UnloadTexture(assets->terrainHardpointTex);
    UnloadTexture(assets->terrainTidePoolTex);
    UnloadTexture(assets->terrainRockTex);
    UnloadTexture(assets->terrainBrushTex);
    UnloadTexture(assets->terrainFeatureAtlasTex);
    UnloadTexture(assets->terrainTileAtlasTex);
    UnloadTexture(assets->terrainShoreTex);
    UnloadTexture(assets->terrainGroundTex);
    UnloadTexture(assets->leviathanCoreTex);
    UnloadTexture(assets->leviathanMortarTex);
    UnloadTexture(assets->leviathanHullSectionTex);
    UnloadTexture(assets->leviathanPodTex);
    UnloadTexture(assets->leviathanHullTex);
    UnloadTexture(assets->droneBunkerTex);
    UnloadTexture(assets->mortarBatteryTex);
    UnloadTexture(assets->mobilePlatformTex);
    UnloadTexture(assets->mineTex);
    UnloadTexture(assets->relayNodeTex);
    UnloadTexture(assets->trackingTurretTex);
    UnloadTexture(assets->casemateTex);
    UnloadTexture(assets->gunshipTex);
    UnloadTexture(assets->interceptorTex);
    UnloadTexture(assets->droneTex);
    UnloadTexture(assets->playerTex);
}
