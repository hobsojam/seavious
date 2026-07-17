#ifndef SEAVIOUS_WRECK_VISUAL_H
#define SEAVIOUS_WRECK_VISUAL_H

#include "game_state.h"

typedef void (*WreckTextureDrawFn)(Texture2D texture, int x, int y, Color tint);

Vector2 MobilePlatformSinkingPosition(Vector2 pos, float age);
unsigned char MobilePlatformSinkingOpacity(float age);
bool DrawSpecialSurfaceWreck(Texture2D texture, const SurfaceWreck *wreck,
    WreckTextureDrawFn drawTexture);

#endif
