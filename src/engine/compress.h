#pragma once

#include <stdint.h>
#include "world.h"

typedef struct
{
	uint8_t length;
	uint8_t type;
} RleRun;

typedef struct
{
	uint32_t n;
	RleRun* runs;
} RleChunk;

RleChunk Compress_Chunk(Chunk* chunk);
void Decompress_Chunk(Chunk* chunk, RleChunk rle);
