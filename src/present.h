#ifndef SEAVIOUS_PRESENT_H
#define SEAVIOUS_PRESENT_H

#include "raylib.h"

// Presentation scaling: where the 512x384 internal canvas lands on the
// actual window/screen. Integer-only scaling keeps point-filtered pixels
// uniform (a fractional scale makes some pixels doubled and others
// tripled), so fullscreen letterboxes with black bars instead of
// stretching; the playtest call on fractional-fill stays open. A screen
// smaller than the canvas still gets scale 1 (center-cropped).
int CalculatePresentScale(int screenWidth, int screenHeight);
Rectangle CalculatePresentRect(int screenWidth, int screenHeight);

#endif
