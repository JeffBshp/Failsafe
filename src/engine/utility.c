#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "utility.h"
#include "cglm/cglm.h"

// Returns true if `flags` has the bit(s) specified by `flag`.
bool EnumHasFlag(int flags, int flag)
{
	return (flags & flag) == flag;
}

// Sets the bits in `*flags` specified by `flag` to 1 or 0 if `set` is T or F.
void EnumSetFlag(int* flags, int flag, bool set)
{
	if (!set) flag = ~flag;
	*flags &= flag;
}

void ListUInt64Init(ListUInt64* list, size_t capacity)
{
	const size_t numBytes = capacity * sizeof(uint64_t);
	list->values = malloc(numBytes);
	memset(list->values, 0, numBytes);
	list->capacity = capacity;
	list->size = 0;
}

void ListUInt64Insert(ListUInt64* list, uint64_t value)
{
	if (list->size >= list->capacity)
	{
		list->capacity *= 2;
		const size_t numBytes = list->capacity * sizeof(uint64_t);
		void* newBlock = realloc(list->values, numBytes);
		if (newBlock != NULL) list->values = newBlock;
	}

	list->values[list->size++] = value;
}

void ListUInt64RemoveAt(ListUInt64* list, size_t index)
{
	if (0 <= index && index < list->size)
	{
		for (int i = index + 1; i < list->size; i++)
			list->values[i - 1] = list->values[i];

		list->size--;
	}
}

uint64_t ListUInt64Pop(ListUInt64* list)
{
	if (list->size > 0) return list->values[--list->size];

	return 0;
}

void ProgressPrint(char* text, size_t n, Progress* prog)
{
	snprintf(text, n, "%s%.1f%%\n%s%.1f%%\n",
		prog->message1, prog->percent1, prog->message2, prog->percent2);
}

// truncates vec3 to ivec3
void GetIntCoords(vec3 fPos, ivec3 iPos)
{
	iPos[0] = (int)floorf(fPos[0]);
	iPos[1] = (int)floorf(fPos[1]);
	iPos[2] = (int)floorf(fPos[2]);
}

static inline bool CheckCubeBounds(int x, int y, int z, int max)
{
	return 0 <= x && x <= max
		&& 0 <= y && y <= max
		&& 0 <= z && z <= max;
}

// Converts coords in a 3D cube to a flat array index.
static inline int Index3D(int x, int y, int z, int width)
{
	return (z * width * width) + (y * width) + x;
}
