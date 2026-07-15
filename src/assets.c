#include "assets.h"

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

    return assets;
}

void UnloadGameAssets(GameAssets *assets) {
    UnloadTexture(assets->oceanOverlayTex);
    UnloadTexture(assets->oceanTex);
    UnloadTexture(assets->leviathanCoreTex);
    UnloadTexture(assets->leviathanMortarTex);
    UnloadTexture(assets->leviathanHullSectionTex);
    UnloadTexture(assets->leviathanPodTex);
    UnloadTexture(assets->leviathanHullTex);
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
