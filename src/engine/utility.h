#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "cglm/cglm.h"

#define PROGRESS_MSG_LEN 40

typedef struct
{
	uint64_t* values;
	size_t capacity;
	size_t size;
} ListUInt64;

typedef enum
{
	AXIS_X,
	AXIS_Y,
	AXIS_Z,
} Axis;

typedef void* (*TreadmillLoader)(void*, int);
typedef void* (*Treadmill3DLoader)(void*, int, int, int);
typedef void (*TreadmillUnloader)(void*);

typedef struct
{
	TreadmillLoader loader;
	void* loaderObj;
	void** list;
	int width;
	int i;
} Treadmill;

typedef struct
{
	Treadmill3DLoader loader;
	TreadmillUnloader unloader;
	void* loaderObj;
	void** list;
	int length;
	int radius;
	int stride;
	int x;
	int y;
	int z;
} Treadmill3D;

enum
{
	BLOCK_AIR = 0,
	BLOCK_DIRT = 1,
	BLOCK_GRASS = 2,
	BLOCK_STONE = 3,
	BLOCK_LOG = 4,
	BLOCK_LEAVES = 5,
	BLOCK_WOOD = 6,
	BLOCK_WINDOW = 7,
};

enum
{
	TEX_SET0 = 0,
	TEX_SET1 = 128,
	TEX_SET2 = 256,
	TEX_SET3 = 384,
	TEX_SET4 = 512,

	TEX_SPACE = 0,
	TEX_A_CAPITAL = 33,
	TEX_A_LOWER = 65,
	TEX_BLANK = 95,
	TEX_TAB = 96,
	TEX_RETURN = 97,

	TEX_GRAY = TEX_SET4 + 2,
	TEX_WHITE = TEX_SET4 + 3,
	TEX_RED = TEX_SET4 + 4,
	TEX_ORANGE = TEX_SET4 + 5,
	TEX_YELLOW = TEX_SET4 + 6,
	TEX_GREEN = TEX_SET4 + 7,
	TEX_BLUE = TEX_SET4 + 8,
	TEX_PURPLE = TEX_SET4 + 9,

	TEX_AIR = TEX_SET4 + 64,
	TEX_DIRT = TEX_AIR + BLOCK_DIRT,
	TEX_GRASS = TEX_AIR + BLOCK_GRASS,
	TEX_STONE = TEX_AIR + BLOCK_STONE,
	TEX_LOG = TEX_AIR + BLOCK_LOG,
	TEX_LEAVES = TEX_AIR + BLOCK_LEAVES,
	TEX_WOOD = TEX_AIR + BLOCK_WOOD,
	TEX_WINDOW = TEX_AIR + BLOCK_WINDOW,
};

bool EnumHasFlag(int flags, int flag);
void EnumSetFlag(int* flags, int flag, bool set);
size_t ReadWholeFile(const char *path, char* buffer, int max);
void ListUInt64Init(ListUInt64* list, size_t capacity);
void ListUInt64Insert(ListUInt64* list, uint64_t value);
void ListUInt64RemoveAt(ListUInt64* list, size_t index);
uint64_t ListUInt64Pop(ListUInt64* list);
void GetIntCoords(vec3 fPos, ivec3 iPos);
