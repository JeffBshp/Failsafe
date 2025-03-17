#ifdef _MSC_VER
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SDL2/SDL.h"
#include "SDL2/SDL_thread.h"
#include "cglm/cglm.h"
#include "utility.h"
#include "world.h"
#include "compress.h"

static inline void SetBlock(Chunk* chunk, int x, int y, int z, unsigned char type)
{
	int i = (z * 4096) + (y * 64) + x;
	chunk->blocks[i] = type;
}

// Generates block data with the help of Perlin noise.
static void GenerateChunk(Chunk* chunk)
{
	Uint32 ticks = SDL_GetTicks();
	int cx = chunk->coords[0];
	int cy = chunk->coords[1];
	int cz = chunk->coords[2];
	int minY = cy * 64;
	float p = 0.0f;
	NoiseMaker* nm = chunk->world->noiseMaker;

	unsigned char* noise2D = Noise_Generate2D(nm, cx, cz, &p);
	unsigned char* noise3D = Noise_Generate3D(nm, cx, cz, &p);

	for (int z = 0; z < 64; z++)
	{
		for (int x = 0; x < 64; x++)
		{
			int height = noise2D[(z * 64) + x] - 128;

			for (int y = 0; y < 64; y++)
			{
				int wy = minY + y;
				unsigned char type = BLOCK_AIR;

				if (wy < height - 10)
				{
					int i = (y * 4096) + (z * 64) + x;
					if (noise3D[i] < 192) type = BLOCK_STONE;
				}
				else if (wy < height - 2) type = BLOCK_DIRT;
				else if (wy < height) type = BLOCK_GRASS;

				SetBlock(chunk, x, y, z, type);
			}
		}
	}

	for (int z = 0; z < 64; z++)
	{
		for (int x = 0; x < 64; x++)
		{
			int height = noise2D[(z * 64) + x] - 128;

			if (height >= minY && height < minY + 56 &&
				x > 1 && x < 62 && z > 1 && z < 62 &&
				(rand() % 360) == 0)
			{
				height -= minY;

				for (int h = height; h < height + 6; h++)
					SetBlock(chunk, x, h, z, BLOCK_LOG);

				for (int w = 0; w < 3; w++)
					for (int u = x - 2 + w; u <= x + 2 - w; u++)
						for (int v = z - 2 + w; v <= z + 2 - w; v++)
							SetBlock(chunk, u, height + 6 + w, v, BLOCK_LEAVES);
			}
		}
	}

	free(noise2D);
	free(noise3D);

	ticks = SDL_GetTicks() - ticks;
	printf("Chunk gen took %d ms.\n", ticks);
}

static char* GetChunkFilePath(Chunk* chunk)
{
	const char* worldFolderPath = chunk->world->folderPath;
	int pathLen = strlen(worldFolderPath) + 34; // includes null term char
	char* chunkFilePath = calloc(pathLen, sizeof(char));
	sprintf(chunkFilePath, "%s/%+04d_%+04d_%+04d.chunk", worldFolderPath, chunk->coords[0], chunk->coords[1], chunk->coords[2]);
	return chunkFilePath;
}

// Saves block data to disk.
static void SaveChunk(Chunk* chunk)
{
	const int numBlocks = 64 * 64 * 64;
	char* chunkFilePath = GetChunkFilePath(chunk);

	SDL_LockMutex(chunk->mutex);
	FILE* file = fopen(chunkFilePath, "wb");

	if (file != NULL)
	{
		RleChunk rle = Compress_Chunk(chunk);
		fwrite(&rle.n, sizeof(uint32_t), 1, file);
		fwrite(rle.runs, sizeof(RleRun), rle.n, file);
		fclose(file);
		free(rle.runs);
	}

	SDL_UnlockMutex(chunk->mutex);
	free(chunkFilePath);
}

// Reads block data from disk or generates the chunk fresh.
// Sets the CHUNK_LOADED flag when finished to indicate that the chunk is ready to be meshed.
static void LoadChunk(Chunk* chunk)
{
	const int numBlocks = 64 * 64 * 64;
	char* chunkFilePath = GetChunkFilePath(chunk);

	SDL_LockMutex(chunk->mutex);
	FILE* file = fopen(chunkFilePath, "rb");
	ivec3 coords;

	if (file == NULL)
	{
		SDL_UnlockMutex(chunk->mutex);
		GenerateChunk(chunk);
		SaveChunk(chunk);
	}
	else
	{
		RleChunk rle;
		fread(&rle.n, sizeof(uint32_t), 1, file);
		rle.runs = malloc(rle.n * sizeof(RleRun));
		fread(rle.runs, sizeof(RleRun), rle.n, file);
		fclose(file);
		Decompress_Chunk(chunk, rle);
		SDL_UnlockMutex(chunk->mutex);
		free(rle.runs);
	}

	// chunk is loaded and ready to mesh
	chunk->flags |= CHUNK_LOADED;
	free(chunkFilePath);
}

static void* ChunkLoader(void* obj, int x, int y, int z)
{
	const int numBlockBytes = 64 * 64 * 64 * sizeof(unsigned char);
	Chunk* chunk = calloc(1, sizeof(Chunk) + numBlockBytes);

	if (chunk != NULL)
	{
		chunk->mutex = SDL_CreateMutex();
		chunk->world = obj;
		chunk->blocks = (void*)(chunk + 1);
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
	ListUInt64Insert(&chunk->world->deadChunks, (uint64_t)chunk);
	SDL_UnlockMutex(chunk->world->mutex);
}

static int ChunkGenThread(void* threadData)
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
		else LoadChunk(chunk);
	}

	return 0;
}

static int ChunkKillThread(void* threadData)
{
	World* world = threadData;

	while (world->alive)
	{
		SDL_LockMutex(world->mutex);
		// list size may shrink as items are removed, and that's fine
		for (int i = 0; i < world->deadChunks.size; i++)
		{
			Chunk* c = (Chunk*)(world->deadChunks.values[i]);

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

	return 0;
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

// Returns the chunk at wPos (world position), and fills in cPos (chunk position) with the local chunk coords.
Chunk* World_GetChunkAndCoords(World* world, ivec3 wPos, ivec3 cPos)
{
	// coords of the chunk relative to other chunks
	int cx = (wPos[0] - (wPos[0] < 0 ? 63 : 0)) / 64;
	int cy = (wPos[1] - (wPos[1] < 0 ? 63 : 0)) / 64;
	int cz = (wPos[2] - (wPos[2] < 0 ? 63 : 0)) / 64;

	// local coords within the chunk
	cPos[0] = wPos[0] - (cx * 64);
	cPos[1] = wPos[1] - (cy * 64);
	cPos[2] = wPos[2] - (cz * 64);

	return Treadmill3DGet(world->chunks, cx, cy, cz);
}

unsigned char World_GetBlock(Chunk* chunk, ivec3 pos)
{
	int x = pos[0];
	int y = pos[1];
	int z = pos[2];

	if (x < 0 || x > 63 || y < 0 || y > 63 || z < 0 || z > 63)
		return 0;

	int i = (z * 64 * 64) + (y * 64) + x;

	return chunk->blocks[i];
}

bool World_IsSolidBlock(World* world, ivec3 pos)
{
	ivec3 cPos;
	Chunk* chunk = World_GetChunkAndCoords(world, pos, cPos);
	if (chunk == NULL) return false;
	return World_GetBlock(chunk, cPos) != 0;
}

void World_SetBlock(World* world, ivec3 pos, unsigned char type)
{
	ivec3 cPos;
	Chunk* chunk = World_GetChunkAndCoords(world, pos, cPos);

	if (chunk != NULL)
	{
		SetBlock(chunk, cPos[0], cPos[1], cPos[2], type);
		chunk->flags |= CHUNK_DIRTY;
		world->dirty = true;
	}
}
