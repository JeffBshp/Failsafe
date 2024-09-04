#pragma once

#include "render.h"
#include <stdbool.h>

#define FPS 30
#define FRAME_TARGET_TIME (1000 / FPS)

typedef struct
{
	bool w;
	bool s;
	bool a;
	bool d;
	bool q;
	bool e;

	bool t;
	bool g;
	bool f;
	bool h;
	bool r;
	bool y;

	bool i;
	bool k;
	bool j;
	bool l;
	bool u;
	bool o;

	bool up;
	bool down;
	bool left;
	bool right;
	bool rshift;
	bool rctrl;

	bool z;
	bool x;
	bool c;
	bool space;
	bool lshift;
	bool lctrl;

	bool gravity;
	bool running;
} InputState;

void Input_Update(InputState* key, GameState* gs);
void Input_HandleInput(InputState* key, GameState* gs);
