#ifndef SEAVIOUS_GAME_RENDER_H
#define SEAVIOUS_GAME_RENDER_H

#include "game_state.h"
#include "assets.h"

// Draws one frame of the scene plus HUD into the current render target.
// The caller owns BeginTextureMode/EndTextureMode and the final upscale.
void DrawGame(const GameState *state, const GameAssets *assets);

#endif
