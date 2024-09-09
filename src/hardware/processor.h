#pragma once

#include <stdint.h>
#include "memory.h"

enum
{
	MASK_LOWER3BITS = 0x0007,
	MASK_UPPER10BITS = 0xFFC0,
	MASK_OPX = 0x0060,
	MASK_SYSVALUE = 0x001F,

	SYS_SLEEP = 0x0020,
	SYS_NOP = 0x0040,
	SYS_HALT = 0x0060,
};

typedef struct
{
	uword programCounter;
	uword instruction;
	uword registers[8];
	Memory memory;
} Processor;

Processor* Processor_New(Memory memory);
void Processor_Reset(Processor* p, uword startAddress, uword stackPointer);
void Processor_Run(Processor* p);
