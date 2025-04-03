#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "lod.h"

static inline int splitBits(int x)
{
	x = (x | (x << 8)) & 0b0011000000001111;
	x = (x | (x << 4)) & 0b0011000011000011;
	x = (x | (x << 2)) & 0b1001001001001001;

	return x;
}

static inline int unsplitBits(int x)
{
	x = (x | (x >> 2)) & 0b11000011000011;
	x = (x | (x >> 4)) & 0b00001100001111;
	x = (x | (x >> 4)) & 0b00000000111111;

	return x;
}

// Calculates the z-order index of the given coords.
// Each coord is limited to 6 bits. Result is 18 bits.
int GetMortonCode(int x, int y, int z)
{
	x = splitBits(x);
	y = splitBits(y) << 1;
	z = splitBits(z) << 2;

	return z | y | x;
}

// Calculates the coords that correspond to a z-order index (aka morton code)
// The morton code is limited to 18 bits. Each resulting coord is 6 bits.
void SplitMortonCode(int morton, int *x, int *y, int *z)
{
	int x0 = (morton & 0b001001001001001001);
	int y0 = (morton & 0b010010010010010010) >> 1;
	int z0 = (morton & 0b100100100100100100) >> 2;

	*x = unsplitBits(x0);
	*y = unsplitBits(y0);
	*z = unsplitBits(z0);
}

// blocks should contain exactly 8 pointers in z-order to 8 block arrays representing 8 chunks,
// and each block array should contain 64*64*64 blocks, also in z-order.
// lod is the output, which represents all 8 chunks condensed to the size of one.
void Lod_Generate(uint8_t **blocks, uint8_t *lod)
{
	const int width = 64;
	const int width3 = width * width * width;
	int counts[256];
	int maxCount;
	uint8_t maxType;
	int lodIndex = 0;

	for (int i = 0; i < 8; i++)
	{
		uint8_t *chunk = blocks[i];

		// Z-ordering makes this convenient. Just iterate over groups of 8 blocks in the current order.
		for (int j = 0; j < width3; j += 8)
		{
			// reset counts for each group
			memset(counts, 0, 256 * sizeof(int));
			maxCount = 0;
			maxType = 0;

			// check individual blocks in the group
			for (int k = 0; k < 8; k++)
			{
				uint8_t block = chunk[j + k];
				int curCount = ++counts[block];

				// non-empty blocks take priority over air
				if (curCount > maxCount && block != 0)
				{
					maxCount = curCount;
					maxType = block;
				}
			}

			// only choose air if more than half the blocks are air
			if (counts[0] > 4) maxType = 0;

			// lodIndex carries over from one chunk to the next until all 8 are finished
			lod[lodIndex++] = maxType;
		}
	}
}
