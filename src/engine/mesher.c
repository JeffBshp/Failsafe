#ifdef _MSC_VER
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include "SDL2/SDL.h"
#include "cglm/cglm.h"
#include "noise.h"
#include "utility.h"
#include "mesher.h"
#include "lod.h"
#include "world.h"

// Indexes an occupancy mask by plane and row.
// Returns an int representing the row, where each bit indicates the presence of a block in the corresponding column.
// The axes of the plane and row depend on the axis of the occupancy mask.
// Ex: For the x mask, plane is the z coord and row is y. Then x is the bit position within the returned value.
static inline const uint64_t GetOccupancy(uint64_t* occupancy, Axis axis, int plane, int row)
{
	return occupancy[(axis * 4096) + (plane * 64) + row];
}

// Sets the occupancy flag at the given coordinates.
static inline void SetOccupancy(uint64_t* occupancy, Axis axis, int plane, int row, int column)
{
	occupancy[(axis * 4096) + (plane * 64) + row] |= (1ull << column);
}

static bool GenerateOccupancyMasks(Chunk* chunk)
{
	bool anyBlocks = false;

	// load occupancy data from raw chunk
	for (int z = 0; z < 64; z++)
	{
		for (int y = 0; y < 64; y++)
		{
			for (int x = 0; x < 64; x++)
			{
				int m = GetMortonCode(x, y, z);
				uint8_t blockType = chunk->blocks[m];

				if (blockType != 0)
				{
					// add the block to the occupancy masks
					SetOccupancy(chunk->occupancy, 0, z, y, x);
					SetOccupancy(chunk->occupancy, 1, z, x, y);
					SetOccupancy(chunk->occupancy, 2, y, x, z);
					anyBlocks = true;
				}
			}
		}
	}

	return anyBlocks;
}

static inline const uint64_t LeftFaces(uint64_t blocks, bool prev)
{
	uint64_t blocksToTheLeft = blocks >> 1;
	if (prev)
	{
		blocksToTheLeft |= 0x8000000000000000ull;
	}
	uint64_t leftFaces = blocks & ~blocksToTheLeft;
	return leftFaces;
}

static inline const uint64_t RightFaces(uint64_t blocks, bool next)
{
	uint64_t blocksToTheRight = blocks << 1;
	if (next)
	{
		blocksToTheRight |= 1ull;
	}
	uint64_t rightFaces = blocks & ~blocksToTheRight;
	return rightFaces;
}

static inline const bool GetFace(uint64_t* faceMasks, int dir, int plane, int row, int column)
{
	return 0 != (faceMasks[(dir * 4096) + (plane * 64) + row] & (1ull << column));
}

static inline void SetFaces(uint64_t* faceMasks, int dir, int plane, int row, uint64_t mask)
{
	faceMasks[(dir * 4096) + (plane * 64) + row] = mask;
}

static inline void ClearOneFace(uint64_t* faceMasks, int dir, int plane, int row, int column)
{
	faceMasks[(dir * 4096) + (plane * 64) + row] &= ~(1ull << column);
}

static void CalculateFaces(uint64_t* faceMasks, uint64_t blocks, Axis axis, int plane, int row, bool prevBlock, bool nextBlock)
{
	uint64_t posFaces = RightFaces(blocks, nextBlock);
	uint64_t negFaces = LeftFaces(blocks, prevBlock);
	SetFaces(faceMasks, 2 * axis, plane, row, posFaces);
	SetFaces(faceMasks, (2 * axis) + 1, plane, row, negFaces);
}

static inline void ConvertAxisCoords(ivec3 coords, Axis axis, int column, int row, int plane)
{
	switch (axis)
	{
	case AXIS_X:
		coords[0] = column;
		coords[1] = row;
		coords[2] = plane;
		break;
	case AXIS_Y:
		coords[0] = row;
		coords[1] = column;
		coords[2] = plane;
		break;
	case AXIS_Z:
		coords[0] = row;
		coords[1] = plane;
		coords[2] = column;
		break;
	}
}

static void CalculateFacesForAxis(Chunk* chunk, World* world, Axis axis, int plane, int row)
{
	uint64_t blocks = GetOccupancy(chunk->occupancy, axis, plane, row);
	if (blocks == 0) return;

	// TODO: determine whether the faces at the chunk boundaries are exposed
	bool prevBlock = false;
	bool nextBlock = false;

	CalculateFaces(chunk->faceMasks, blocks, axis, plane, row, prevBlock, nextBlock);
}

// uses binary operations to find exposed faces
static void GenerateFaceMasks(Chunk* chunk, World* world)
{
	for (int z = 0; z < 64; z++)
	{
		for (int y = 0; y < 64; y++)
			CalculateFacesForAxis(chunk, world, AXIS_X, z, y);

		for (int x = 0; x < 64; x++)
			CalculateFacesForAxis(chunk, world, AXIS_Y, z, x);
	}

	for (int y = 0; y < 64; y++)
		for (int x = 0; x < 64; x++)
			CalculateFacesForAxis(chunk, world, AXIS_Z, y, x);
}

// x axis (dir 0/1): z -> plane, y -> row, x -> column
// y axis (dir 2/3): z, x, y
// z axis (dir 4/5): y, x, z
static void GreedyMesh(Chunk* chunk)
{
	uint64_t* faceMasks = chunk->faceMasks;
	ListUInt64* quadList = &chunk->quads;
	quadList->size = 0; // reset list

	for (int dir = 0; dir < 6; dir++)
	{
		Axis axis = dir / 2;

		// The column is a bit position in an int64_t, which indicates where faces are.
		// Inner loops will iterate over planes and rows while a column is held constant.
		// That's how adjacent faces are found.
		for (int column = 0; column < 64; column++)
		{
			for (int plane = 0; plane < 64; plane++)
			{
				for (int row = 0; row < 64;) // not incremented here
				{
					int quadStartRow = -1;
					int quadEndRow = -1;
					int quadEndPlane = plane;
					bool face = false;
					bool foundFace = false;

					// search for one or more contiguous faces, across rows, in the same column
					for (; row < 64; row++)
					{
						face = GetFace(faceMasks, dir, plane, row, column);

						if (foundFace)
						{
							if (face)
							{
								quadEndRow++;
								//ClearOneFace(faceMasks, dir, plane, row, column);
							}
							else break;
						}
						else
						{
							if (face)
							{
								quadStartRow = row;
								quadEndRow = quadStartRow;
								foundFace = true;
							}
						}
					}

					// if no faces, nothing left to do in this plane
					if (quadStartRow < 0) continue;

					// try to expand across planes
					for (int p = plane + 1; p < 64; p++)
					{
						bool planeOk = true;

						for (int r = quadStartRow; r <= quadEndRow; r++)
						{
							if (!GetFace(faceMasks, dir, p, r, column))
							{
								planeOk = false;
								break;
							}
						}

						if (planeOk)
						{
							quadEndPlane++;

							for (int r = quadStartRow; r <= quadEndRow; r++)
							{
								ClearOneFace(faceMasks, dir, p, r, column);
							}
						}
						else break;
					}

					// construct a rectangle
					ivec3 coords;
					ConvertAxisCoords(coords, axis, column, quadStartRow, plane);
					uint64_t height = quadEndRow - quadStartRow;
					uint64_t width = quadEndPlane - plane;
					uint64_t quad = (((uint64_t)dir) << 61) | (height << 38) | (width << 32) | (coords[2] << 12) | (coords[1] << 6) | coords[0];
					ListUInt64Insert(quadList, quad);
				}
			}
		}
	}
}

void Mesher_MeshWorld(World* world)
{
	SDL_LockMutex(world->mutex);
	if (!world->dirty)
	{
		SDL_UnlockMutex(world->mutex);
		return;
	}

	const size_t sizeOfMaskArrays = 64 * 64 * sizeof(uint64_t);
	Uint32 ticks = SDL_GetTicks();
	bool dirty = false;
	ListUInt64 chunkList = world->allChunks;

	for (int i = 0; i < chunkList.size; i++)
	{
		Chunk *chunk = (void *)chunkList.values[i];
		if (chunk == NULL) continue;

		SDL_LockMutex(chunk->mutex);

		if (!EnumHasFlag(chunk->flags, CHUNK_LOADED))
		{
			SDL_UnlockMutex(chunk->mutex);
			dirty = true; // skip meshing this chunk and let the world remain dirty
			continue;
		}

		if (!EnumHasFlag(chunk->flags, CHUNK_DIRTY))
		{
			SDL_UnlockMutex(chunk->mutex);
			continue;
		}

		ticks = SDL_GetTicks();
		chunk->occupancy = malloc(3 * sizeOfMaskArrays); // 96 kB
		memset(chunk->occupancy, 0, 3 * sizeOfMaskArrays);
		chunk->faceMasks = malloc(6 * sizeOfMaskArrays); // 192 kB
		memset(chunk->faceMasks, 0, 6 * sizeOfMaskArrays);

		if (GenerateOccupancyMasks(chunk))
		{
			GenerateFaceMasks(chunk, world);
			GreedyMesh(chunk);
		}

		free(chunk->occupancy);
		free(chunk->faceMasks);
		EnumSetFlag((int*)(&chunk->flags), CHUNK_DIRTY, false);
		SDL_UnlockMutex(chunk->mutex);

		ticks = SDL_GetTicks() - ticks;
		//printf("Chunk mesh took %d ms for %d quads.\n", ticks, chunk->quads.size);
	}

	if (!dirty) world->dirty = false;
	SDL_UnlockMutex(world->mutex);
}
