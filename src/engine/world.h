#pragma once

#include "cglm/cglm.h"
#include "noise.h"
#include "utility.h"

typedef enum
{
	CHUNK_LOADED = 1 << 0,
	CHUNK_DIRTY = 1 << 1,
	CHUNK_DEAD = 1 << 2,
} ChunkFlags;

enum
{
	NUM_CHUNK_THREADS = 4
};

typedef struct
{
	char* folderPath;
	Treadmill3D* chunks;
	NoiseMaker* noiseMaker;
	Progress* progress;
	SDL_mutex* mutex;
	SDL_Thread* chunkGenThreads[NUM_CHUNK_THREADS];
	ListUInt64 deadChunks;
	bool alive;
	bool dirty;
} World;

typedef struct
{
	World* world;
	SDL_mutex* mutex;
	ivec3 coords;
	unsigned char* blocks;
	uint64_t* occupancy;
	uint64_t* faceMasks;
	ListUInt64 quads;
	ChunkFlags flags;
} Chunk;

void World_Init(World* world);
