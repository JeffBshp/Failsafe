#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "compress.h"
#include "world.h"

// Compresses a chunk with run-length encoding (RLE).
// Caller is responsible for freeing the returned array of runs.
RleChunk Compress_Chunk(Chunk* chunk)
{
	const int sideSize = 64;
	const int fullSize = sideSize * sideSize * sideSize;
	uint8_t* raw = chunk->blocks;
	uint8_t buffer[fullSize];
	uint8_t runLength = 1;
	uint8_t runBlock = raw[0];
	int b = 0; // number of bytes written to the buffer

	// Start at 1 because the first one has been read.
	// One extra iteration for the special case at the end.
	for (int i = 1; i <= fullSize; i++)
	{
		// end the last run
		if (i == fullSize) goto end_run;

		uint8_t block = raw[i];

		if (block == runBlock)
		{
			if (++runLength == 255)
			{
				// peek ahead, then end the run as usual
				block = raw[++i];
				goto end_run;
			}
		}
		else
		{
			end_run:
			buffer[b++] = runLength;
			buffer[b++] = runBlock;

			//printf("(%d X %d) ", runLength, runBlock);
			//if ((b % 32) == 0) printf("\n");

			runLength = 1;
			runBlock = block;
		}
	}

	RleChunk rle;
	rle.n = b / 2;
	rle.runs = malloc(b);
	memcpy(rle.runs, buffer, b);
	//printf("\nRLE Compressed Size: %d bytes (%d runs)\n", b, rle.n);

	return rle;
}

// Fills in the raw blocks of a chunk from RLE.
// Assumes the chunk's block array has already been allocated.
void Decompress_Chunk(Chunk* chunk, RleChunk rle)
{
	const int sideSize = 64;
	const int fullSize = sideSize * sideSize * sideSize;
	int n = 0; // number of bytes written to the chunk
	uint8_t* raw = chunk->blocks;

	for (int i = 0; i < rle.n; i++)
	{
		RleRun run = rle.runs[i];

		if (n >= fullSize)
		{
			printf("Error: Too many runs in the chunk.\n");
			break;
		}

		if (n + run.length > fullSize)
		{
			printf("Error: A run is too long for the chunk.\n");
			run.length = fullSize - n;
		}

		//printf("(%d X %d) ", run.length, run.type);
		//if ((i % 16) == 15) printf("\n");

		memset(raw, run.type, run.length);
		raw += run.length;
		n += run.length;
	}

	//printf("\n");

	if (n < fullSize)
	{
		printf("Error: Incomplete chunk.\n");
		memset(raw, BLOCK_AIR, fullSize - n);
	}
}
