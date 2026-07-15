#ifndef SEAVIOUS_ASSETS_H
#define SEAVIOUS_ASSETS_H

#include "raylib.h"

typedef struct {
    Texture2D playerTex;
    Texture2D droneTex;
    Texture2D interceptorTex;
    Texture2D gunshipTex;
    Texture2D casemateTex;
    Texture2D trackingTurretTex;
    Texture2D relayNodeTex;
    Texture2D mineTex;
    Texture2D mobilePlatformTex;
    Texture2D oceanTex;
    Texture2D oceanOverlayTex;
} GameAssets;

GameAssets LoadGameAssets(void);
void UnloadGameAssets(GameAssets *assets);

#endif
