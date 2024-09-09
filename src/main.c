#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <stdbool.h>
#include <math.h>

#include "SDL.h"
#include "gl/glew.h"
#include "SDL_opengl.h"
#include "SOIL2/SOIL2.h"
#include "cglm/cglm.h"

#include "engine/camera.h"
#include "engine/shader.h"
#include "engine/render.h"
#include "engine/input.h"
#include "engine/shape.h"
#include "engine/mesher.h"
#include "engine/noise.h"
#include "engine/utility.h"

#include "language/tokens.h"
#include "language/parser.h"
#include "language/runner.h"

#include "hardware/memory.h"
#include "hardware/processor.h"

int main(int argc, char* argv[])
{
	Memory mem = Memory_New(512); // Create a virtual memory unit
	// Read the machine-language example program into virtual memory.
	// I "compiled" it by hand. It just adds a series of numbers and writes the results to memory.
	Memory_ReadFile(mem, "./code/example-in.mem");
	Processor* proc = Processor_New(mem); // Create a virtual processor
	Processor_Reset(proc, 0x40, 0); // Set start address to its hardcoded location
	Processor_Run(proc); // Run the processor until it halts
	Memory_WriteFile(mem, "./code/example-out.mem"); // Write the resulting memory to another file
	free(proc);
	Memory_Destroy(mem);

	// Load the other program. This one is written in a higher level, C-like language.
	// The parser creates an abstract syntax tree (AST) and returns a root Function object.
	Function* f = Parser_ParseFile("./code/example.txt");
	// The runner interprets the AST and runs it, starting with the root function.
	Runner_Execute(f);
	printf("\n\n\n");



	Camera cam;
	NoiseMaker nm = { .initialized = false };
	Progress prog = { .percent1 = 0, .percent2 = 0, .done = false };
	World w = { .noiseMaker = &nm, .progress = &prog };
	GameState gs = { .world = &w, .cam = &cam, .progress = &prog };
	InputState key = { .gravity = false, .running = true };

	if (Render_Init(&gs))
	{
		printf("Game started.\n");
	}
	else
	{
		printf("ERROR: Could not start game.");
		return 1;
	}

	while (key.running)
	{
		Input_HandleInput(&key, &gs);
		Input_Update(&key, &gs);
		Render_Draw(&gs);
	}

	Render_Destroy(&gs);
	SDL_Quit();
	printf("Shutting down...\n");

	return 0;
}
