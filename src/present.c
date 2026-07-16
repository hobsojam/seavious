#include "present.h"
#include "gameplay.h"

int CalculatePresentScale(int screenWidth, int screenHeight) {
    int scaleX = screenWidth / GAME_WIDTH;
    int scaleY = screenHeight / GAME_HEIGHT;
    int scale = scaleX < scaleY ? scaleX : scaleY;
    return scale < 1 ? 1 : scale;
}

Rectangle CalculatePresentRect(int screenWidth, int screenHeight) {
    int scale = CalculatePresentScale(screenWidth, screenHeight);
    float width = (float)(GAME_WIDTH * scale);
    float height = (float)(GAME_HEIGHT * scale);
    return (Rectangle){
        ((float)screenWidth - width) / 2.0f,
        ((float)screenHeight - height) / 2.0f,
        width,
        height
    };
}

bool SyncFullscreenSetting(bool fullscreen, bool borderlessActive, void (*toggle)(void)) {
    if (fullscreen == borderlessActive) return false;
    toggle();
    return true;
}
