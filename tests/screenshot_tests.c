#include "screenshot.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\\n", __FILE__, __LINE__, #condition); \
        failures++; \
    } \
} while (0)

typedef struct {
    bool directoryOk;
    int occupiedThrough;
    bool writeOk;
    int directoryCalls;
    int existsCalls;
    int writeCalls;
    char writtenPath[SCREENSHOT_PATH_CAPACITY];
} FakeFilesystem;

static bool MakeDirectory(const char *path, void *context) {
    FakeFilesystem *fs = context;
    fs->directoryCalls++;
    return fs->directoryOk && strcmp(path, "screenshots") == 0;
}

static bool FileExists(const char *path, void *context) {
    FakeFilesystem *fs = context;
    fs->existsCalls++;
    int number = 0;
    CHECK(sscanf(path, "screenshots/seavious-%d.png", &number) == 1);
    return number <= fs->occupiedThrough;
}

static bool TakeScreenshot(const char *path, void *context) {
    FakeFilesystem *fs = context;
    fs->writeCalls++;
    snprintf(fs->writtenPath, sizeof fs->writtenPath, "%s", path);
    return fs->writeOk;
}

static ScreenshotOps FakeOps(FakeFilesystem *fs) {
    return (ScreenshotOps){
        .makeDirectory = MakeDirectory,
        .fileExists = FileExists,
        .takeScreenshot = TakeScreenshot,
        .context = fs,
    };
}

static void TestCaptureEligibility(void) {
    CHECK(ShouldCaptureGameplayScreenshot(true, false, false, true));
    CHECK(!ShouldCaptureGameplayScreenshot(false, false, false, true));
    CHECK(!ShouldCaptureGameplayScreenshot(true, true, false, true));
    CHECK(!ShouldCaptureGameplayScreenshot(true, false, true, true));
    CHECK(!ShouldCaptureGameplayScreenshot(true, false, false, false));
}

static void TestFirstFreeNumberIsSaved(void) {
    FakeFilesystem fs = { .directoryOk = true, .occupiedThrough = 2, .writeOk = true };
    ScreenshotOps ops = FakeOps(&fs);
    char path[SCREENSHOT_PATH_CAPACITY] = "";

    CHECK(SaveGameplayScreenshot(&ops, path));
    CHECK(fs.directoryCalls == 1);
    CHECK(fs.existsCalls == 3);
    CHECK(fs.writeCalls == 1);
    CHECK(strcmp(path, "screenshots/seavious-000003.png") == 0);
    CHECK(strcmp(fs.writtenPath, path) == 0);
}

static void TestWriteAndDirectoryFailures(void) {
    FakeFilesystem noDirectory = { 0 };
    ScreenshotOps ops = FakeOps(&noDirectory);
    char path[SCREENSHOT_PATH_CAPACITY] = "unchanged";
    CHECK(!SaveGameplayScreenshot(&ops, path));
    CHECK(noDirectory.existsCalls == 0 && noDirectory.writeCalls == 0);

    FakeFilesystem noWrite = { .directoryOk = true };
    ops = FakeOps(&noWrite);
    CHECK(!SaveGameplayScreenshot(&ops, path));
    CHECK(noWrite.existsCalls == 1 && noWrite.writeCalls == 1);
}

static bool EveryPathExists(const char *path, void *context) {
    FakeFilesystem *fs = context;
    (void)path;
    fs->existsCalls++;
    return true;
}

static void TestFullFolderDoesNotOverwrite(void) {
    FakeFilesystem fs = { .directoryOk = true, .writeOk = true };
    ScreenshotOps ops = FakeOps(&fs);
    ops.fileExists = EveryPathExists;
    char path[SCREENSHOT_PATH_CAPACITY] = "";

    CHECK(!SaveGameplayScreenshot(&ops, path));
    CHECK(fs.existsCalls == SCREENSHOT_MAX_NUMBER);
    CHECK(fs.writeCalls == 0);
}

static void TestInvalidOperationsAreRejected(void) {
    char path[SCREENSHOT_PATH_CAPACITY] = "";
    CHECK(!SaveGameplayScreenshot(NULL, path));
    ScreenshotOps incomplete = { 0 };
    CHECK(!SaveGameplayScreenshot(&incomplete, path));
    FakeFilesystem fs = { .directoryOk = true, .writeOk = true };
    ScreenshotOps ops = FakeOps(&fs);
    CHECK(!SaveGameplayScreenshot(&ops, NULL));
}

int main(void) {
    TestCaptureEligibility();
    TestFirstFreeNumberIsSaved();
    TestWriteAndDirectoryFailures();
    TestFullFolderDoesNotOverwrite();
    TestInvalidOperationsAreRejected();
    return failures != 0;
}
