#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "SDL2/SDL.h"
#include "cglm/cglm.h"
#include "noise.h"
#include "utility.h"

typedef enum
{
	CHUNK_LOADED = 1 << 0,
	CHUNK_GENERATED = 1 << 1,
	CHUNK_DIRTY = 1 << 2,
	CHUNK_DEAD = 1 << 3,
} ChunkFlags;

enum
{
	NUM_CHUNK_THREADS = 4
};

struct Chunk;
typedef struct Chunk Chunk;

struct Region;
typedef struct Region Region;

typedef struct
{
	char* folderPath;
	NoiseMaker* noiseMaker;
	Progress* progress;
	SDL_mutex* mutex;
	SDL_Thread* chunkGenThreads[NUM_CHUNK_THREADS];
	ListUInt64 deadChunks;
	bool alive;
	bool dirty;

	ivec3 visibleCenter;
	int visibleDistance;
	int lodDistance;
	ListUInt64 allChunks;
	ListUInt64 regions;
} World;

struct Chunk
{
	World* world;
	SDL_mutex* mutex;
	ivec3 coords;
	uint64_t* occupancy;
	uint64_t* faceMasks;
	ListUInt64 quads;
	ChunkFlags flags;
	int lodLevel;
	uint8_t blocks[64 * 64 * 64]; // 256 kiB
};

struct Region
{
	SDL_mutex *mutex;
	ivec3 baseCoords;
	int lodLevel;
	int numChunks;
	bool loaded;
	Chunk *chunks;
	World *world;
};

void World_Init(World* world);
void World_BlockToChunkCoords(ivec3 b, ivec3 c);
Chunk* World_GetChunkAndCoords(World* world, ivec3 wPos, ivec3 cPos);
uint8_t World_GetBlock(Chunk* chunk, ivec3 pos);
bool World_IsSolidBlock(World* world, ivec3 pos);
void World_SetBlock(World* world, ivec3 pos, uint8_t type);
void World_UpdatePosition(World *world, ivec3 globalCenterBlock);
