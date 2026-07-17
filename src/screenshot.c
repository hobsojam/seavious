#include "screenshot.h"

#include <stdio.h>

bool ShouldCaptureGameplayScreenshot(bool runGameFrame, bool paused, bool quitConfirm,
    bool screenshotPressed) {
    return runGameFrame && !paused && !quitConfirm && screenshotPressed;
}

bool SaveGameplayScreenshot(const ScreenshotOps *ops, char path[SCREENSHOT_PATH_CAPACITY]) {
    if (ops == NULL || ops->makeDirectory == NULL || ops->fileExists == NULL
        || ops->takeScreenshot == NULL || path == NULL) return false;
    if (!ops->makeDirectory("screenshots", ops->context)) return false;

    for (int number = 1; number <= SCREENSHOT_MAX_NUMBER; number++) {
        snprintf(path, SCREENSHOT_PATH_CAPACITY, "screenshots/seavious-%06d.png", number);
        if (ops->fileExists(path, ops->context)) continue;
        return ops->takeScreenshot(path, ops->context);
    }
    return false;
}
