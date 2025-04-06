#pragma once

#include "game.h"

bool Render_Init(RenderState* rs);
void Render_InitBuffers(RenderState* rs);
void Render_Destroy(RenderState* rs);
void Render_Draw(GameState* gs);
