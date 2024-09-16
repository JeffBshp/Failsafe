#pragma once

#include "SDL_keyboard.h"
#include "shape.h"

void Editor_Edit(TextBox* tb, SDL_Keysym sym);
void Editor_Update(TextBox* tb, int ticks);
void Editor_SaveToFile(TextBox* tb, char* filePath);
