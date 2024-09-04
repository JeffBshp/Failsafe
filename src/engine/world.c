#ifdef _MSC_VER
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SDL.h"
#include "SDL_thread.h"
#include "cglm/cglm.h"
#include "utility.h"
#include "world.h"

static inline void SetBlock(Chunk* chunk, int x, int y, int z, unsigned char type)
{
	int i = (z * 4096) + (y * 64) + x;
	chunk->blocks[i] = type;
}

// Fills in the array of blocks.
// Sets the CHUNK_LOADED flag when finished to indicate that the chunk is ready to be meshed.
static void GenerateChunk(Chunk* chunk)
{
	// TODO: no vertically stacked chunks for now
	if (chunk->coords[1] == 0)
	{
		Uint32 ticks = SDL_GetTicks();
		int x = chunk->coords[0];
		int z = chunk->coords[2];
		float p = 0.0f;
		NoiseMaker* nm = chunk->world->noiseMaker;

		unsigned char* noise2D = Noise_Generate2D(nm, x, z, &p);
		unsigned char* noise3D = Noise_Generate3D(nm, x, z, &p);

		for (int v = 0; v < 64; v++)
		{
			for (int u = 0; u < 64; u++)
			{
				int height = noise2D[(v * 64) + u] / 4;

				for (int y = 0; y < 64; y++)
				{
					unsigned char type = BLOCK_AIR;

					if (y < 0.7 * height)
					{
						int i = (y * 4096) + (v * 64) + u;
						if (noise3D[i] < 192) type = BLOCK_STONE;
					}
					else if (y < 0.9 * height) type = BLOCK_DIRT;
					else if (y < height) type = BLOCK_GRASS;

					SetBlock(chunk, u, y, v, type);
				}
			}
		}

		for (int v = 0; v < 64; v++)
		{
			for (int u = 0; u < 64; u++)
			{
				int height = noise2D[(v * 64) + u] / 4;

				if (height < 56 && u > 1 && u < 62 && v > 1 && v < 62 && (rand() % 360) == 0)
				{
					for (int h = height; h < height + 6; h++)
						SetBlock(chunk, u, h, v, BLOCK_LOG);

					for (int y = 0; y < 3; y++)
						for (int x = u - 2 + y; x <= u + 2 - y; x++)
							for (int z = v - 2 + y; z <= v + 2 - y; z++)
								SetBlock(chunk, x, height + 6 + y, z, BLOCK_LEAVES);
				}
			}
		}

		free(noise2D);
		free(noise3D);

		ticks = SDL_GetTicks() - ticks;
		printf("Chunk gen took %d ms.\n", ticks);
	}

	// chunk is loaded and ready to mesh
	chunk->flags |= CHUNK_LOADED;
}

static void* ChunkLoader(void* obj, int x, int y, int z)
{
	const int numBlockBytes = 64 * 64 * 64 * sizeof(unsigned char);
	Chunk* chunk = malloc(sizeof(Chunk) + numBlockBytes);

	if (chunk != NULL)
	{
		chunk->mutex = SDL_CreateMutex();
		chunk->world = obj;
		chunk->blocks = chunk + 1;
		memset(chunk->blocks, 0, numBlockBytes);
		chunk->coords[0] = x;
		chunk->coords[1] = y;
		chunk->coords[2] = z;
		chunk->flags = 0;
		ListUInt64Init(&chunk->quads, 64);

		chunk->world->dirty = true;
	}

	return chunk;
}

static void ChunkUnloader(void* item)
{
	Chunk* chunk = item;
	SDL_LockMutex(chunk->world->mutex);
	chunk->flags |= CHUNK_DEAD;
	ListUInt64Insert(&chunk->world->deadChunks, chunk);
	SDL_UnlockMutex(chunk->world->mutex);
}

static void ChunkGenThread(void* threadData)
{
	World* world = threadData;

	while (world->alive)
	{
		Chunk* chunk = NULL;

		if (world->dirty)
		{
			Treadmill3D* chunks = world->chunks;

			for (int i = 0; i < chunks->length; i++)
			{
				Chunk* c = chunks->list[i];
				SDL_LockMutex(c->mutex);

				if (!EnumHasFlag(c->flags, CHUNK_LOADED) &&
					!EnumHasFlag(c->flags, CHUNK_DIRTY))
				{
					// Chunk will be meshed when it's loaded AND dirty;
					// right now it's dirty and about to start loading.
					c->flags |= CHUNK_DIRTY;
					SDL_UnlockMutex(c->mutex);

					chunk = c;
					break;
				}

				SDL_UnlockMutex(c->mutex);
			}
		}

		if (chunk == NULL) SDL_Delay(200);
		else GenerateChunk(chunk);
	}
}

static void ChunkKillThread(void* threadData)
{
	World* world = threadData;

	while (world->alive)
	{
		SDL_LockMutex(world->mutex);
		// list size may shrink as items are removed, and that's fine
		for (int i = 0; i < world->deadChunks.size; i++)
		{
			Chunk* c = world->deadChunks.values[i];

			// chunk needs to be dead AND finished loading
			if (EnumHasFlag(c->flags, CHUNK_DEAD | CHUNK_LOADED))
			{
				ListUInt64RemoveAt(&world->deadChunks, i);
				SDL_DestroyMutex(c->mutex);
				free(c->quads.values);
				free(c);
				i--; // repeat this index
			}
		}
		SDL_UnlockMutex(world->mutex);

		SDL_Delay(200);
	}
}

void World_Init(World* world)
{
	Uint32 ticks = SDL_GetTicks();

	const int radius = 3;
	world->mutex = SDL_CreateMutex();
	world->alive = true;
	world->dirty = true;
	ListUInt64Init(&world->deadChunks, 64);
	world->chunks = Treadmill3DNew(ChunkLoader, ChunkUnloader, world, radius);

	for (int i = 0; i < NUM_CHUNK_THREADS; i++)
	{
		SDL_Thread* thread = SDL_CreateThread(ChunkGenThread, "Chunk Generation Thread", world);
		world->chunkGenThreads[i] = thread;
		SDL_DetachThread(thread);
	}

	SDL_DetachThread(SDL_CreateThread(ChunkKillThread, "Chunk Cleanup Thread", world));

	ticks = SDL_GetTicks() - ticks;
	printf("World init took %d ms.\n", ticks);
}
