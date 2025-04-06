#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>

#include "SDL2/SDL.h"
#include "SDL2/SDL_timer.h"

#include "engine/game.h"
#include "engine/render.h"
#include "engine/input.h"

int main(int argc, char* argv[])
{
	GameState *gs = Game_New();

	if (gs == NULL)
	{
		printf("ERROR: Could not start game.\n");
		return 1;
	}

	printf("Starting game loop...\n");
	while (gs->running)
	{
		Input_Poll(gs);
		Game_Update(gs);
		Render_Draw(gs);
	}

	printf("Shutting down...\n");
	Game_Destroy(gs);
	SDL_Delay(1000);
	SDL_Quit();
	printf("Shutdown successful.\n");

	return 0;
}
