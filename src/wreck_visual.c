#include "wreck_visual.h"

static float MobilePlatformSinkProgress(float age) {
    float progress = age / MOBILE_PLATFORM_SINK_DURATION;
    if (progress < 0.0f) return 0.0f;
    if (progress > 1.0f) return 1.0f;
    return progress;
}

Vector2 MobilePlatformSinkingPosition(Vector2 pos, float age) {
    pos.y += MOBILE_PLATFORM_SINK_DEPTH * MobilePlatformSinkProgress(age);
    return pos;
}

unsigned char MobilePlatformSinkingOpacity(float age) {
    return (unsigned char)(150.0f * (1.0f - MobilePlatformSinkProgress(age)));
}

bool DrawSpecialSurfaceWreck(Texture2D texture, const SurfaceWreck *wreck,
    WreckTextureDrawFn drawTexture) {
    if (wreck->type != SURFACE_TARGET_MOBILE_PLATFORM) return false;

    Vector2 sunkPos = MobilePlatformSinkingPosition(wreck->pos, wreck->age);
    Color underwater = { 112, 156, 166, MobilePlatformSinkingOpacity(wreck->age) };
    drawTexture(texture,
        (int)(sunkPos.x - texture.width / 2.0f),
        (int)(sunkPos.y - texture.height / 2.0f), underwater);
    return true;
}
