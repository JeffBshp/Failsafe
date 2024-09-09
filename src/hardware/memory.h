#pragma once

#include <stdint.h>

typedef uint16_t uword;
typedef int16_t word;

typedef struct
{
	uword n;
	uword* data;
} Memory;

Memory Memory_New(uword size);
void Memory_Destroy(Memory mem);
void Memory_ReadFile(Memory mem, char* filePath);
void Memory_WriteFile(Memory mem, char* filePath);
uword Memory_Allocate(Memory mem, uword size);
void Memory_Free(Memory mem, uword address);
