#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <stdbool.h>
#include <math.h>

#include "SDL2/SDL.h"
#include "SDL2/SDL_timer.h"
#include "GL/glew.h"
#include "cglm/cglm.h"

#include "engine/camera.h"
#include "engine/shader.h"
#include "engine/render.h"
#include "engine/input.h"
#include "engine/shape.h"
#include "engine/mesher.h"
#include "engine/noise.h"
#include "engine/editor.h"
#include "engine/utility.h"

#include "language/parser.h"
#include "language/runner.h"
#include "language/compiler.h"
#include "language/float16.h"

#include "hardware/device.h"
#include "hardware/memory.h"
#include "hardware/processor.h"

static int CodeDemo(void* threadData)
{
	GameState* gs = threadData;
	SDL_Delay(3000);

	Memory mem = Memory_New(2048); // Create a virtual memory unit
	vec3* vel = gs->shapes[0].models[0].vel; // Get a reference to the velocity of one of the objects
	Device device = { .vel = vel };
	Processor* proc = Processor_New(device, mem); // Create a virtual processor
	gs->processorHalt = &(proc->halt); // this is so hacky

	while (true)
	{
		SDL_Delay(1000);

		if (gs->runProgram)
		{
			printf("Compile Program...\n");
			gs->runProgram = false;

			// Save the source code
			Editor_SaveToFile(gs->textBox, gs->programFilePath);
			// Parse and compile the virtual program
			SyntaxTree* ast = Parser_ParseFile(gs->programFilePath);
			Program* p = Compiler_GenerateCode(ast);

			if (p->status == COMPILE_SUCCESS)
			{
				printf("Run Program...\n");
				SDL_memcpy(mem.data, p->bin, p->length * sizeof(uword)); // Load program into virtual memory
				Memory_WriteFile(mem, "res/code/example.mem"); // Save the raw binary to a file

				Processor_Reset(proc, p->mainAddress, 1024); // Set the addresses of the program and the stack
				Processor_Run(proc); // Run the processor until it halts
			}
			else
			{
				printf("Compile Error: %d\n", p->status);
			}

			Compiler_Destroy(p);
			Parser_Destroy(ast);
		}
	}

	free(proc);
	Memory_Destroy(mem);
	return 0;
}

int main(int argc, char* argv[])
{
	Camera cam;
	NoiseMaker nm = { .initialized = false };
	Progress prog = { .percent1 = 0, .percent2 = 0, .done = false };
	World w = { .noiseMaker = &nm, .progress = &prog };
	GameState gs = { .world = &w, .cam = &cam, .progress = &prog };
	gs.programFilePath = "res/code/example.temp";
	gs.worldFilePath = "res/world/world.world";
	InputState key = { .gravity = false, .running = true };

	SDL_DetachThread(SDL_CreateThread(&CodeDemo, "Code Demo Thread", &gs));

	if (Render_Init(&gs))
	{
		printf("Game started.\n");
	}
	else
	{
		printf("ERROR: Could not start game.");
		return 1;
	}

	printf("Start game loop...\n");
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
