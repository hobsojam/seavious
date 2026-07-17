#ifndef SEAVIOUS_SCREENSHOT_H
#define SEAVIOUS_SCREENSHOT_H

#include <stdbool.h>

#define SCREENSHOT_PATH_CAPACITY 64
#define SCREENSHOT_MAX_NUMBER 999999

typedef struct {
    bool (*makeDirectory)(const char *path, void *context);
    bool (*fileExists)(const char *path, void *context);
    bool (*takeScreenshot)(const char *path, void *context);
    void *context;
} ScreenshotOps;

// Print Screen is intentionally gameplay-only: menus, pause, and the quit
// confirmation must not create a captured modal image.
bool ShouldCaptureGameplayScreenshot(bool runGameFrame, bool paused, bool quitConfirm,
    bool screenshotPressed);

// Finds the first unused six-digit screenshot path and asks the supplied
// platform operations to create it. Returns false on an I/O failure or when
// every supported filename is occupied.
bool SaveGameplayScreenshot(const ScreenshotOps *ops, char path[SCREENSHOT_PATH_CAPACITY]);

#endif
