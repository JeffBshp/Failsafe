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

#include "language/parser.h"
#include "language/runner.h"
#include "language/compiler.h"
#include "language/float16.h"

#include "hardware/memory.h"
#include "hardware/processor.h"

static void CodeDemo(void* threadData)
{
	// Parse source file of the example program
	SyntaxTree* ast = Parser_ParseFile("./code/example.txt");

	// The runner interprets and executes the AST in real time, starting with the "main" function.
	Runner_Execute(ast);

	// Alternatively, the program can be fully compiled and run on a virtual processor.
	Program* p = Compiler_GenerateCode(ast);

	if (p->status == COMPILE_SUCCESS)
	{
		Memory mem = Memory_New(1024); // Create a virtual memory unit
		SDL_memcpy(mem.data, p->bin, p->length * sizeof(uword)); // Load program into virtual memory
		Memory_WriteFile(mem, "./code/example.mem"); // Save the raw binary to a file
		Processor* proc = Processor_New(mem); // Create a virtual processor
		Processor_Reset(proc, p->mainAddress, 768); // Set the addresses of the program and the stack
		Processor_Run(proc); // Run the processor until it halts

		Compiler_Destroy(p);
		free(proc);
		Memory_Destroy(mem);
	}
	else
	{
		printf("Compile Error: %d\n", p->status);
	}

	Parser_Destroy(ast);

	printf("\n\n\n");
}

int main(int argc, char* argv[])
{
	SDL_DetachThread(SDL_CreateThread(&CodeDemo, "Code Demo Thread", NULL));

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
