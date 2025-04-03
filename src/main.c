#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <stdint.h>
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
#include "engine/world.h"

#include "language/parser.h"
#include "language/runner.h"
#include "language/compiler.h"
#include "language/float16.h"

#include "hardware/device.h"
#include "hardware/memory.h"
#include "hardware/processor.h"

// This just demonstrates that the virtual program can interact with the game world by breaking blocks.
// A pointer to this function gets passed to the virtual processor.
// The processor calls it during the code demo.
static void BreakBlock(void* worldData, float* coords)
{
	ivec3 wPos, cPos;
	GetIntCoords(coords, wPos);
	wPos[1] -= 1; // go one block down
	World* world = worldData;
	Chunk* chunk = World_GetChunkAndCoords(world, wPos, cPos);
	uint8_t blockType = World_GetBlock(chunk, cPos);

	if (blockType != BLOCK_AIR)
	{
		printf("Breaking block type %d\n", blockType);
		World_SetBlock(world, wPos, BLOCK_AIR);
	}
}

static int CodeDemo(void* threadData)
{
	GameState* gs = threadData;
	while (!gs->initialized) SDL_Delay(1000);

	Memory mem = Memory_New(2048); // Create a virtual memory unit
	// Get a reference to the position and velocity of one of the objects
	vec3* pos = (void*)(gs->shapes[0].models[0].pos);
	vec3* vel = (void*)(gs->shapes[0].models[0].vel);
	Device device = { .world = gs->world, .funcBreakBlock = BreakBlock, .pos = pos, .vel = vel };
	Processor* proc = Processor_New(device, mem); // Create a virtual processor
	gs->processorHalt = &(proc->halt); // this is so hacky

	while (gs->world->alive)
	{
		SDL_Delay(1000);

		if (gs->runProgram)
		{
			printf("Compile Program...\n");
			gs->runProgram = false;

			// Save the source code
			Editor_SaveToFile(gs->codeTextBox, gs->programFilePath);
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
	World w = { .noiseMaker = &nm, .progress = &prog, .folderPath = "res/world/debug" };
	GameState gs = { .world = &w, .cam = &cam, .progress = &prog, .processorHalt = NULL };
	gs.programFilePath = "res/code/example.temp";
	InputState key = { .gravity = false, .running = true };
	SDL_Thread* codeThread = SDL_CreateThread(&CodeDemo, "Code Demo Thread", &gs);

	if (Render_Init(&gs))
	{
		printf("Game started.\n");
		gs.initialized = true;
	}
	else
	{
		printf("ERROR: Could not start game.\n");
		return 1;
	}

	printf("Starting game loop...\n");
	while (key.running)
	{
		Input_HandleInput(&key, &gs);
		Input_Update(&key, &gs);
		Render_Draw(&gs);
	}

	printf("Shutting down...\n");
	gs.runProgram = false;
	if (gs.processorHalt != NULL) *(gs.processorHalt) = true;
	Render_Destroy(&gs);
	SDL_WaitThread(codeThread, NULL);
	printf("Code thread finished.\n");
	SDL_Delay(1000);
	SDL_Quit();
	printf("Shutdown successful.\n");

	return 0;
}
