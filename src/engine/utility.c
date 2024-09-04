#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "utility.h"

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

Treadmill3D* Treadmill3DNew(Treadmill3DLoader loader, TreadmillUnloader unloader, void* loaderObj, int radius)
{
	int w = (2 * radius + 1);
	int length = w * w * w;
	int listSize = length * sizeof(void*);
	Treadmill3D* t = malloc(sizeof(Treadmill3D) + listSize);

	if (t != NULL)
	{
		t->list = t + 1;
		t->loader = loader;
		t->unloader = unloader;
		t->loaderObj = loaderObj;
		t->length = length;
		t->radius = radius;
		t->stride = 2 * radius + 1;
		t->x = 0;
		t->y = 0;
		t->z = 0;

		// populate the list
		int i = 0;
		for (int z = -radius; z <= radius; z++)
		{
			for (int y = -radius; y <= radius; y++)
			{
				for (int x = -radius; x <= radius; x++)
				{
					t->list[i] = loader(loaderObj, x, y, z);
					i++;
				}
			}
		}
	}

	return t;
}

void* Treadmill3DGet(Treadmill3D* t, int x, int y, int z)
{
	void* itemPtr = NULL;
	int max = 2 * t->radius;
	int relX = x - t->x + t->radius;
	int relY = y - t->y + t->radius;
	int relZ = z - t->z + t->radius;

	if (CheckCubeBounds(relX, relY, relZ, max))
	{
		int i = Index3D(relX, relY, relZ, max + 1);
		itemPtr = t->list[i];
	}

	return itemPtr;
}

// Shifts and wraps the entire flat list of a 3D treadmill.
static void Treadmill3DWrap(Treadmill3D* t, int wrap)
{
	if (wrap == 0) return;

	const int aWrap = abs(wrap);
	const int numToShift = t->length - aWrap;
	const int bufLen = aWrap * sizeof(void*);
	void** buffer = malloc(bufLen);
	void** list = t->list;

	int wrapSrc, wrapDest, shiftSrc, shiftDest;

	if (wrap < 0)
	{
		wrapSrc = 0;
		wrapDest = numToShift;
		shiftSrc = aWrap;
		shiftDest = 0;
	}
	else
	{
		wrapSrc = numToShift;
		wrapDest = 0;
		shiftSrc = 0;
		shiftDest = aWrap;
	}

	memcpy(buffer, list + wrapSrc, bufLen);
	memmove(list + shiftDest, list + shiftSrc, numToShift * sizeof(void*));
	memcpy(list + wrapDest, buffer, bufLen);
	free(buffer);
}

static inline void Treadmill3DMoveX(Treadmill3D* t, int step)
{
	Treadmill3DWrap(t, -step);
	t->x += step;

	int r = t->radius;
	int stride = 2 * r + 1;
	int x = t->x + (step * r);
	int i = step < 0 ? 0 : stride - 1;

	// refresh one fixed x position for each y & z
	for (int z = t->z - r; z <= t->z + r; z++)
	{
		for (int y = t->y - r; y <= t->y + r; y++)
		{
			t->unloader(t->list[i]);
			t->list[i] = t->loader(t->loaderObj, x, y, z);
			i += stride;
		}
	}
}

static inline void Treadmill3DMoveY(Treadmill3D* t, int step)
{
	int r = t->radius;
	int stride = 2 * r + 1;

	Treadmill3DWrap(t, -step * stride);
	t->y += step;

	int y = t->y + (step * r);
	int i = step < 0 ? 0 : (stride - 1) * stride;

	// refresh one row of x positions per z, with y held constant
	for (int z = t->z - r; z <= t->z + r; z++)
	{
		// refresh a whole row along the x axis
		for (int x = t->x - r; x <= t->x + r; x++)
		{
			t->unloader(t->list[i]);
			t->list[i] = t->loader(t->loaderObj, x, y, z);
			i++;
		}

		// skip the rest of the y values for the current z
		i += (stride - 1) * stride;
	}
}

static inline void Treadmill3DMoveZ(Treadmill3D* t, int step)
{
	int r = t->radius;
	int stride = 2 * r + 1;

	Treadmill3DWrap(t, -step * stride * stride);
	t->z += step;

	int z = t->z + (step * r);
	int i = step < 0 ? 0 : t->length - (stride * stride);

	// refresh all x & y positions at a fixed z
	for (int y = t->y - r; y <= t->y + r; y++)
	{
		// refresh a whole row along the x axis
		for (int x = t->x - r; x <= t->x + r; x++)
		{
			t->unloader(t->list[i]);
			t->list[i] = t->loader(t->loaderObj, x, y, z);
			i++;
		}
	}
}

// Moves the treadmill one space in the given direction.
void Treadmill3DMove(Treadmill3D* t, int dir)
{
	if (dir < 0 || dir > 5) return;

	switch (dir)
	{
	case 0: Treadmill3DMoveX(t, -1); break;
	case 1: Treadmill3DMoveX(t, 1); break;
	case 2: Treadmill3DMoveY(t, -1); break;
	case 3: Treadmill3DMoveY(t, 1); break;
	case 4: Treadmill3DMoveZ(t, -1); break;
	case 5: Treadmill3DMoveZ(t, 1); break;
	}
}
