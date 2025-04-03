#ifdef _MSC_VER
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "SDL2/SDL.h"
#include "SDL2/SDL_thread.h"
#include "cglm/cglm.h"
#include "utility.h"
#include "world.h"
#include "compress.h"
#include "lod.h"
#include "filesystem.h"

static void LoadRegion(Region *region);

static void Align(ivec3 coords, int alignment)
{
	int mod = coords[0] % alignment;
	coords[0] -= mod < 0 ? mod + alignment : mod;
	mod = coords[1] % alignment;
	coords[1] -= mod < 0 ? mod + alignment : mod;
	mod = coords[2] % alignment;
	coords[2] -= mod < 0 ? mod + alignment : mod;
}

static inline int NumChunksInRegion(int lodLevel)
{
	switch (lodLevel)
	{
		case 0: return 512;
		case 1: return 64;
		case 2: return 8;
		case 3: return 1;
		default: return 1;
	}
}

static void Coords_ApplyMortonOffset(ivec3 base, int morton, int lodLevel, ivec3 result)
{
	int alignment = 1 << lodLevel;
	int x, y, z;

	SplitMortonCode(morton, &x, &y, &z);

	result[0] = base[0] + (x * alignment);
	result[1] = base[1] + (y * alignment);
	result[2] = base[2] + (z * alignment);
}

// Frees a region and all of its chunks.
static void DestroyRegion(Region *region)
{
	int numChunks = NumChunksInRegion(region->lodLevel);

	for (int i = 0; i < numChunks; i++)
	{
		Chunk *c = region->chunks + i;
		SDL_DestroyMutex(c->mutex);
		free(c->quads.values);
	}

	SDL_DestroyMutex(region->mutex);
	free(region);
}

// Initializes a Chunk struct.
static void ConstructChunk(Chunk *chunk, World *world, ivec3 coords, int lodLevel)
{
	chunk->mutex = SDL_CreateMutex();
	chunk->world = world;
	chunk->flags = CHUNK_DIRTY;
	chunk->lodLevel = lodLevel;
	ListUInt64Init(&chunk->quads, 64);
	glm_ivec3_copy(coords, chunk->coords);
}

// Initializes a Region struct and all of its chunks without loading block data.
static Region *ConstructRegion(World *world, ivec3 baseCoords, int lodLevel)
{
	ivec3 coords;
	int numChunks = NumChunksInRegion(lodLevel);
	size_t size = sizeof(Region) + (numChunks * sizeof(Chunk));
	Region *region = calloc(1, size);
	region->mutex = SDL_CreateMutex();
	region->chunks = (void *)(region + 1);
	region->world = world;
	region->lodLevel = lodLevel;
	region->numChunks = numChunks;
	region->loaded = false;
	glm_ivec3_copy(baseCoords, region->baseCoords);

	for (int i = 0; i < numChunks; i++)
	{
		Chunk *chunk = region->chunks + i;
		Coords_ApplyMortonOffset(baseCoords, i, lodLevel, coords);
		ConstructChunk(chunk, world, coords, lodLevel);
	}

	return region;
}

static inline void SetBlock(Chunk* chunk, int x, int y, int z, uint8_t type)
{
	int m = GetMortonCode(x, y, z);
	chunk->blocks[m] = type;
}

// Generates block data with the help of Perlin noise.
static void GenerateChunk(Chunk* chunk)
{
	if (chunk->lodLevel != 0)
	{
		printf("Error: Called GenerateChunk with invalid LOD level.\n");
		return;
	}

	//Uint32 ticks = SDL_GetTicks();
	int cx = chunk->coords[0];
	int cy = chunk->coords[1];
	int cz = chunk->coords[2];
	int minY = cy * 64;
	float p = 0.0f;
	NoiseMaker* nm = chunk->world->noiseMaker;

	uint8_t* noise2D = Noise_Generate2D(nm, cx, cz, &p);
	//uint8_t* noise3D = Noise_Generate3D(nm, cx, cy, cz, &p);

	for (int z = 0; z < 64; z++)
	{
		for (int x = 0; x < 64; x++)
		{
			int height = noise2D[(z * 64) + x] - 128;

			for (int y = 0; y < 64; y++)
			{
				int wy = minY + y;
				uint8_t type = BLOCK_AIR;

				if (wy < height - 10)
				{
					type = BLOCK_STONE;
					//int i = (y * 4096) + (z * 64) + x;
					//if (noise3D[i] < 192) type = BLOCK_STONE;
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

	chunk->flags |= CHUNK_LOADED | CHUNK_GENERATED;
	free(noise2D);
	//free(noise3D);

	//ticks = SDL_GetTicks() - ticks;
	//printf("Chunk gen took %d ms.\n", ticks);
}

// Generates new block data for a whole region and any subregions.
static void GenerateRegion(Region *region)
{
	World *world = region->world;
	int lodLevel = region->lodLevel;
	int numChunks = NumChunksInRegion(lodLevel);

	if (lodLevel == 0)
	{
		for (int i = 0; i < numChunks; i++)
		{
			Chunk *chunk = region->chunks + i;
			GenerateChunk(chunk);
		}
	}
	else
	{
		// Load the next-lower-lod-level region. This will recursively load/generate regions until level 0 is reached.
		// TODO: for now, L3 is the max, and all region files are aligned to L3 coords, hence the same baseCoords being used for the subRegion
		Region *subRegion = ConstructRegion(world, region->baseCoords, lodLevel - 1);
		LoadRegion(subRegion);

		for (int i = 0; i < numChunks; i++)
		{
			// subRegion has 8 times as many chunks
			int subChunkStart = i * 8;
			Chunk *chunk = region->chunks + i;
			uint8_t *blocks[8];

			for (int m = 0; m < 8; m++)
			{
				Chunk *subChunk = subRegion->chunks + subChunkStart + m;
				blocks[m] = subChunk->blocks;
			}

			Lod_Generate(blocks, chunk->blocks);
			chunk->flags |= CHUNK_LOADED | CHUNK_GENERATED;
		}

		DestroyRegion(subRegion);
	}
}

// Fills in 15 characters including '\0' starting at buffer.
static inline void CoordsToString(char *buffer, ivec3 coords)
{
	sprintf(buffer, "%+04d_%+04d_%+04d", coords[0], coords[1], coords[2]);
}

// Fills in the buffer with the region's file path. Also creates directories as needed.
static void GetRegionFilePath(Region *region, char *buffer)
{
	char *dirs[2];
	char subdirL3[20];
	char filename[2] = { '0' + region->lodLevel, '\0' };

	dirs[0] = region->world->folderPath;
	dirs[1] = subdirL3;

	const int alignment = 1 << 3;
	ivec3 alignedCoords;
	glm_ivec3_copy(region->baseCoords, alignedCoords);
	Align(alignedCoords, alignment);
	CoordsToString(subdirL3, alignedCoords);

	PathBuilder path;
	path.dirs = dirs;
	path.file = filename;
	path.ext = ".chunk";
	path.numDirs = 2;

	Path_BuildStrAndMakeDirs(buffer, path);
}

// Reads block data from disk or generates the region from scratch.
static void LoadRegion(Region *region)
{
	SDL_LockMutex(region->mutex);
	if (region->loaded)
	{
		SDL_UnlockMutex(region->mutex);
		return;
	}

	char regionFilePath[100];
	GetRegionFilePath(region, regionFilePath);
	Region_Read(region, regionFilePath);

	if (!region->loaded)
	{
		GenerateRegion(region);
		Region_Write(region, regionFilePath);
		region->loaded = true;
	}

	SDL_UnlockMutex(region->mutex);
}

// Loads or generates regions that have been added to the region list.
// This runs in a dedicated thread.
static int RegionGenThread(void* threadData)
{
	World* world = threadData;

	while (world->alive)
	{
		Region* region = NULL;

		if (world->dirty)
		{
			ListUInt64 regionList = world->regions;

			SDL_LockMutex(world->mutex);
			for (int i = 0; i < regionList.size; i++)
			{
				Region *r = (void *)regionList.values[i];
				if (!r->loaded) { region = r; break; }
			}
			SDL_UnlockMutex(world->mutex);
		}

		if (region == NULL) SDL_Delay(200);
		else LoadRegion(region);
	}

	return 0;
}

// This finds/loads a chunk at a certain LOD level and places it in the active list. It also returns the chunk.
static Chunk *LoadLodChunk(World *world, ivec3 coords, int lodLevel, int alignment)
{
	ivec3 end;
	glm_ivec3_adds(coords, alignment, end);

	ListUInt64 *chunkList = &(world->allChunks);
	Chunk *foundChunk = NULL;

	// Check if the chunk is already loaded and in the active list.
	// Also remove previously loaded chunks at different LOD levels that overlap the desired chunk.
	for (int i = 0; i < chunkList->size; i++)
	{
		Chunk *c = (void *)chunkList->values[i];
		ivec3 cEnd;
		glm_ivec3_adds(c->coords, 1 << c->lodLevel, cEnd);

		if (c->coords[0] < end[0] && cEnd[0] > coords[0] &&
			c->coords[1] < end[1] && cEnd[1] > coords[1] &&
			c->coords[2] < end[2] && cEnd[2] > coords[2])
		{
			if (c->lodLevel == lodLevel)
			{
				foundChunk = c;
			}
			else
			{
				ListUInt64RemoveAt(&world->allChunks, i--);
				printf("Dead chunk.\n");
			}
		}
	}

	if (foundChunk != NULL) return foundChunk;

	// TODO: For now, all region files cover the same width (eight L0 chunks).
	const int w = 8;
	ListUInt64 regionList = world->regions;
	Region *region = NULL;

	// The desired chunk is not in the active list, but has its region been loaded into memory?
	for (int i = 0; i < regionList.size; i++)
	{
		Region *r = (void *)regionList.values[i];
		if (r->lodLevel != lodLevel) continue;

		if (r->baseCoords[0] < end[0] && (r->baseCoords[0] + w) > coords[0] &&
			r->baseCoords[1] < end[1] && (r->baseCoords[1] + w) > coords[1] &&
			r->baseCoords[2] < end[2] && (r->baseCoords[2] + w) > coords[2])
		{
			region = r;
			break;
		}
	}

	// Construct the region if not found. Its block data will be loaded/generated on another thread.
	if (region == NULL)
	{
		ivec3 baseCoords;
		glm_ivec3_copy(coords, baseCoords);
		int baseLodLevel = 3; // TODO: this is constant for now
		Align(baseCoords, 1 << baseLodLevel);
		region = ConstructRegion(world, baseCoords, lodLevel);
		ListUInt64Insert(&(world->regions), (uint64_t)region);
	}

	Chunk *newChunk = NULL;
	int numChunks = NumChunksInRegion(lodLevel);

	// Now find the chunk within the region. It may or may not have its block data filled in yet, and that's fine.
	for (int i = 0; i < numChunks; i++)
	{
		Chunk *c = region->chunks + i;

		if (c->coords[0] == coords[0] &&
			c->coords[1] == coords[1] &&
			c->coords[2] == coords[2])
		{
			newChunk = c;
			break;
		}
	}

	if (newChunk == NULL)
	{
		// this should not happen
		printf("Chunk not found!\n");
		newChunk = calloc(1, sizeof(Chunk));
		ConstructChunk(newChunk, world, coords, lodLevel);
	}

	// assuming the right chunk was found...
	ListUInt64Insert(&(world->allChunks), (uint64_t)newChunk);
	world->dirty = true;
	return newChunk;
}

// Loads a ring of chunks at a certain LOD level around the already loaded area.
// This gets called with progressively higher LOD levels so that distant chunks are less detailed.
// The width of the ring is determined by world->lodDistance.
static void LoadLodLevel(World *world, ivec3 loadedStart, ivec3 loadedEnd, int lodLevel)
{
	int alignment = 1 << lodLevel;
	int nextAlignment = alignment << 1;
	int lDist = world->lodDistance * alignment;
	ivec3 coords, start, end;
	glm_ivec3_subs(loadedStart, lDist, start);
	glm_ivec3_adds(loadedEnd, lDist - 1, end);
	Align(start, nextAlignment);
	Align(end, nextAlignment);
	glm_ivec3_adds(end, nextAlignment, end);

	// find individual chunks within the target area and load them
	for (coords[2] = start[2]; coords[2] < end[2]; coords[2] += alignment)
	{
		for (coords[1] = start[1]; coords[1] < end[1]; coords[1] += alignment)
		{
			for (coords[0] = start[0]; coords[0] < end[0]; coords[0] += alignment)
			{
				if (loadedStart[2] <= coords[2] && coords[2] < loadedEnd[2] &&
					loadedStart[1] <= coords[1] && coords[1] < loadedEnd[1] &&
					loadedStart[0] <= coords[0] && coords[0] < loadedEnd[0])
					continue;

				LoadLodChunk(world, coords, lodLevel, alignment);
			}
		}
	}

	/*
	const int w = 8;
	ListUInt64 regionList = world->regions;
	ListUInt64 *chunkList = &(world->allChunks);

	// check for regions out of range and free them
	for (int i = 0; i < regionList.size; i++)
	{
		Region *r = (void *)regionList.values[i];
		if (r->lodLevel != lodLevel) continue;

		// Is the region outside the area we just loaded?
		if (r->baseCoords[0] >= end[0] || (r->baseCoords[0] + w) < start[0] ||
			r->baseCoords[1] >= end[1] || (r->baseCoords[1] + w) < start[1] ||
			r->baseCoords[2] >= end[2] || (r->baseCoords[2] + w) < start[2])
		{
			int numChunks = NumChunksInRegion(r->lodLevel);
			uint64_t firstChunkAddress = (uint64_t)r->chunks;
			uint64_t lastChunkAddress = firstChunkAddress + ((numChunks - 1) * sizeof(Chunk));

			SDL_LockMutex(world->mutex);
			for (int j = 0; j < chunkList->size; j++)
			{
				uint64_t chunkAddress = chunkList->values[j];

				// Does the chunk belong to the region we're trying to remove?
				if (firstChunkAddress <= chunkAddress && chunkAddress <= lastChunkAddress)
				{
					ListUInt64RemoveAt(&world->allChunks, j--);
					printf("Removing chunk from dead region.\n");
				}
			}

			// release memory
			DestroyRegion(r);
			SDL_UnlockMutex(world->mutex);
		}
	}
	*/

	// mark the area as loaded
	glm_ivec3_copy(start, loadedStart);
	glm_ivec3_copy(end, loadedEnd);
}

void World_Init(World* world)
{
	Uint32 ticks = SDL_GetTicks();

	world->mutex = SDL_CreateMutex();
	world->visibleDistance = 3;
	world->lodDistance = 1;
	world->alive = true;
	world->dirty = true;
	ListUInt64Init(&world->regions, 64);
	ListUInt64Init(&world->allChunks, 64);
	ListUInt64Init(&world->deadChunks, 64);

	for (int i = 0; i < NUM_CHUNK_THREADS; i++)
	{
		SDL_Thread *thread = SDL_CreateThread(RegionGenThread, "World Generation Thread", world);
		world->chunkGenThreads[i] = thread;
		SDL_DetachThread(thread);
	}

	ticks = SDL_GetTicks() - ticks;
	printf("World init took %d ms.\n", ticks);
}

void World_BlockToChunkCoords(ivec3 b, ivec3 c)
{
	c[0] = (b[0] - (b[0] < 0 ? 63 : 0)) / 64;
	c[1] = (b[1] - (b[1] < 0 ? 63 : 0)) / 64;
	c[2] = (b[2] - (b[2] < 0 ? 63 : 0)) / 64;
}

// Returns the chunk at wPos (world position), and fills in cPos (chunk position) with the local chunk coords.
Chunk* World_GetChunkAndCoords(World* world, ivec3 wPos, ivec3 cPos)
{
	// coords of the chunk relative to other chunks
	ivec3 chunkCoords;
	World_BlockToChunkCoords(wPos, chunkCoords);

	// local coords within the chunk
	cPos[0] = wPos[0] - (chunkCoords[0] * 64);
	cPos[1] = wPos[1] - (chunkCoords[1] * 64);
	cPos[2] = wPos[2] - (chunkCoords[2] * 64);

	return LoadLodChunk(world, chunkCoords, 0, 1);
}

uint8_t World_GetBlock(Chunk* chunk, ivec3 pos)
{
	int x = pos[0];
	int y = pos[1];
	int z = pos[2];

	if (x < 0 || x > 63 || y < 0 || y > 63 || z < 0 || z > 63)
		return 0;

	int m = GetMortonCode(x, y, z);

	return chunk->blocks[m];
}

bool World_IsSolidBlock(World* world, ivec3 pos)
{
	ivec3 cPos;
	Chunk* chunk = World_GetChunkAndCoords(world, pos, cPos);
	if (chunk == NULL) return false;
	return World_GetBlock(chunk, cPos) != 0;
}

void World_SetBlock(World* world, ivec3 pos, uint8_t type)
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

void World_UpdatePosition(World *world, ivec3 globalCenterBlock)
{
	int maxLodLevel = world->visibleDistance;

	ivec3 chunkCoords, loadedStart, loadedEnd;
	World_BlockToChunkCoords(globalCenterBlock, chunkCoords);

	if (chunkCoords[0] == world->visibleCenter[0] &&
		chunkCoords[1] == world->visibleCenter[1] &&
		chunkCoords[2] == world->visibleCenter[2])
	{
		return;
	}

	glm_ivec3_copy(chunkCoords, world->visibleCenter);

	// load the chunk at the center position at full detail (L0)
	LoadLodChunk(world, chunkCoords, 0, 1);
	glm_ivec3_copy(chunkCoords, loadedStart);
	glm_ivec3_adds(chunkCoords, 1, loadedEnd);

	// load an additional ring around the center at L0 and repeat for each LOD
	for (int level = 0; level <= maxLodLevel; level++)
	{
		LoadLodLevel(world, loadedStart, loadedEnd, level);
	}
}
