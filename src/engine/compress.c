#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "compress.h"
#include "world.h"
#include "zhelp.h"
#include "zlib.h"
#include "lod.h"

typedef struct
{
	uint32_t numBytes;
	uint32_t numRuns;
} RleInfo;

// Compresses a chunk with run-length encoding (RLE).
// Caller is responsible for managing both of the buffers.
static RleInfo Rle_EncodeChunk(uint8_t *raw, uint8_t *rle)
{
	const int sideSize = 64;
	const int fullSize = sideSize * sideSize * sideSize;
	RleInfo info = { .numBytes = 0, .numRuns = 0 };
	int b = 0; // byte index for output
	int runLength = 1;
	uint8_t runBlock = raw[0];
	uint8_t curBlock = runBlock;

	// Start at 1 because the first one has been read.
	// One extra iteration for the special case at the end.
	for (int i = 1; i <= fullSize; i++)
	{
		if (i == fullSize) goto end_run; // end the last run

		curBlock = raw[i];

		if (curBlock == runBlock)
		{
			runLength++;
			continue;
		}

		end_run:
		int numBytes;

		// encode the run length, which uses a variable number of bytes
		if (runLength > 0xffff)
		{
			numBytes = 6;
			rle[b++] = 0xff; // four-byte length indicator
			rle[b++] = (uint8_t)runLength;
			rle[b++] = (uint8_t)(runLength >> 8);
			rle[b++] = (uint8_t)(runLength >> 16);
			rle[b++] = (uint8_t)(runLength >> 24);
		}
		else if (runLength > 0xfe)
		{
			numBytes = 4;
			rle[b++] = 0; // two-byte length indicator
			rle[b++] = (uint8_t)runLength;
			rle[b++] = (uint8_t)(runLength >> 8);
		}
		else
		{
			numBytes = 2;
			rle[b++] = (uint8_t)runLength;
		}

		// encode the block type of the run
		rle[b++] = runBlock;

		// track the total size and number of runs
		info.numBytes += numBytes;
		info.numRuns++;

		// start next run
		runLength = 1;
		runBlock = curBlock;
	}

	return info;
}

// Fills in the raw blocks of a chunk from RLE.
// Caller is responsible for managing both of the buffers.
static void Rle_DecodeChunk(uint8_t *raw, uint8_t *rle, RleInfo info)
{
	const int sideSize = 64;
	const int fullSize = sideSize * sideSize * sideSize;
	int n = 0; // number of blocks written to the chunk
	int b = 0; // byte index of rle

	for (int i = 0; i < info.numRuns; i++)
	{
		uint8_t firstByte = rle[b++];
		int runLength;

		// decode the run length, which uses a variable number of bytes
		if (firstByte == 0xff)
		{
			runLength = rle[b++];
			runLength |= ((int)rle[b++]) << 8;
			runLength |= ((int)rle[b++]) << 16;
			runLength |= ((int)rle[b++]) << 24;
		}
		else if (firstByte == 0)
		{
			runLength = rle[b++];
			runLength |= ((int)rle[b++]) << 8;
		}
		else
		{
			runLength = firstByte;
		}

		// decode the block type of the run
		uint8_t runType = rle[b++];

		if (n >= fullSize)
		{
			printf("Error: Too many runs in the chunk.\n");
			break;
		}

		if (n + runLength > fullSize)
		{
			printf("Error: A run is too long for the chunk.\n");
			runLength = fullSize - n;
		}

		// fill in the raw block data
		memset(raw, runType, runLength);
		raw += runLength;
		n += runLength;
	}

	if (b != info.numBytes)
		printf("Error: Corrupt RLE data.\n");

	if (n < fullSize)
	{
		printf("Error: Incomplete chunk.\n");
		memset(raw, BLOCK_AIR, fullSize - n);
	}
}

// Encodes block data as RLE, compresses with zlib, and writes the whole region to disk.
void Region_Write(Region *region, char *path)
{
	FILE *file = fopen(path, "wb");

	if (file != NULL)
	{
		int numChunks = region->numChunks;
		uint8_t *buffer = malloc(2 * 64 * 64 * 64 * sizeof(uint8_t)); // worst case size requirement for one RLE chunk

		// 3 integers per chunk will be written first, indicating the location of the chunk within the file,
		// the compressed size in bytes, and the number of runs.
		// So the first chunk contents go after those integers.
		int contentPos = 3 * numChunks * sizeof(uint32_t);
		int tablePos = 0;

		for (int i = 0; i < numChunks; i++)
		{
			// encode chunk into buffer
			RleInfo info = Rle_EncodeChunk(region->chunks[i].blocks, buffer);

			// compress numBytes from buffer, write to file, and give me back the compressed size
			fseek(file, contentPos, SEEK_SET);
			int compressedSize;
			int zres = Z_Deflate(buffer, info.numBytes, file, &compressedSize);
			if (zres != Z_OK) break;

			// write table entry for chunk
			fseek(file, tablePos, SEEK_SET);
			fwrite(&contentPos, sizeof(uint32_t), 1, file);
			fwrite(&compressedSize, sizeof(uint32_t), 1, file);
			fwrite(&(info.numRuns), sizeof(uint32_t), 1, file);

			// use the size of the written data to calculate the offset for the next chunk
			contentPos += compressedSize;
			tablePos += 3 * sizeof(uint32_t);
		}

		fclose(file);
		free(buffer);
	}
}

// Reads a region file, decompresses chunks, and decodes RLE block data.
// If successful, all chunks in the region will have their block arrays filled in.
void Region_Read(Region *region, char *path)
{
	FILE *file = fopen(path, "rb");
	bool success = false;

	if (file != NULL)
	{
		int numChunks = region->numChunks;
		int numLengths = 3 * numChunks;
		int readResult;
		uint32_t lengths[numLengths];
		uint8_t *buffer = malloc(2 * 64 * 64 * 64 * sizeof(uint8_t)); // worst case size requirement for one RLE chunk

		// read the entire metadata table at once
		readResult = fread(lengths, sizeof(uint32_t), numLengths, file);
		if (readResult != numLengths) { printf("Error reading file.\n"); goto f_close; }

		success = true;

		for (int i = 0; i < numChunks; i++)
		{
			// read zlib stream from file, decompress, and give me back the uncompressed size
			int compressedSize = lengths[(i * 3) + 1]; // second table entry (offset 1) per chunk is the compressed size
			int uncompressedSize;
			int zres = Z_Inflate(file, compressedSize, buffer, &uncompressedSize);
			if (zres != Z_OK) { success = false; goto f_close; }

			// decode rle to get a raw chunk
			RleInfo info;
			info.numBytes = uncompressedSize;
			info.numRuns = lengths[(i * 3) + 2]; // third table entry (offset 2) is the number of runs
			Rle_DecodeChunk(region->chunks[i].blocks, buffer, info);

			// mark chunk as loaded
			region->chunks[i].flags |= CHUNK_LOADED | CHUNK_GENERATED;
		}

		f_close:
		fclose(file);
		free(buffer);
	}

	//printf("Loaded region (%d, %d, %d) (success = %d)\n", region->baseCoords[0], region->baseCoords[1], region->baseCoords[2], success);
	region->loaded = success;
}
