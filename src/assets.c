#include "assets.h"

GameAssets LoadGameAssets(void) {
    GameAssets assets = { 0 };

    assets.playerTex = LoadTexture("assets/sprites/player_ship.png");
    SetTextureFilter(assets.playerTex, TEXTURE_FILTER_POINT);

    assets.droneTex = LoadTexture("assets/sprites/skimmer_drone.png");
    SetTextureFilter(assets.droneTex, TEXTURE_FILTER_POINT);

    assets.casemateTex = LoadTexture("assets/sprites/casemate.png");
    SetTextureFilter(assets.casemateTex, TEXTURE_FILTER_POINT);

    assets.trackingTurretTex = LoadTexture("assets/sprites/tracking_turret.png");
    SetTextureFilter(assets.trackingTurretTex, TEXTURE_FILTER_POINT);

    assets.relayNodeTex = LoadTexture("assets/sprites/relay_node.png");
    SetTextureFilter(assets.relayNodeTex, TEXTURE_FILTER_POINT);

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
    UnloadTexture(assets->relayNodeTex);
    UnloadTexture(assets->trackingTurretTex);
    UnloadTexture(assets->casemateTex);
    UnloadTexture(assets->droneTex);
    UnloadTexture(assets->playerTex);
}
