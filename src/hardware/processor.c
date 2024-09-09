#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "SDL.h"
#include "processor.h"

// Instruction Bits:
// 15-13: 3-Bit Op Code
// 12-10: Register A
// 9-7: Register B
// 9-0: Immediate 10 upper bits for LUI
// 6-0: Immediate 7-Bit Signed Integer
// 6-3: Not used for ADD, NAND
// 2-0: Register C

// Instruction Pseudocode:
// 000 (ADD):	RegA = RegB + RegC;
// 001 (ADDI):	RegA = RegB + Immed;
// 010 (NAND):	RegA = ~(RegB & RegC);
// 011 (LUI):	RegA = Immed10 << 6; (sets upper 10 bits)
// 100 (SW):	Memory[RegB + Immed] = RegA;
// 101 (LW):	RegA = Memory[RegB + Immed];
// 110 (BEQ):	if (RegA == RegB) PC += Immed;
// 111: instruction is further decoded by bits 6 and 5.
//		00 (JALR):	RegA = PC+1; PC = RegB; (jump to B and link A)
//		01 (SLEEP): Sleep for a certain length of time based on the lowest 5 bits
//		10 (NOP): No operation. May replace this instruction with something else.
//		11 (HALT): Stop execution immediately.

// Notes:
// Based on the RiSC-16 Instruction Set Architecture.
// Immed can range from -64 to +63.
// SW, LW, and BEQ use relative addresses with Immed.
// Register 0 always contains zero.
// Register 6 is designated as the stack pointer.
// Register 7 is recommended for holding the return address when calling a subroutine via JALR.
// A subroutine can then return with: JALR R0 R7 (link to R0, which does nothing, and jump to R7).

// TODO: Write a compiler and/or assembler

Processor* Processor_New(Memory memory)
{
	Processor* p = calloc(1, sizeof(Processor));
	p->memory = memory;
	return p;
}

void Processor_Reset(Processor* p, uword startAddress, uword stackPointer)
{
	p->programCounter = startAddress;
	p->registers[6] = stackPointer;
}

static inline uword RegA(Processor* p)
{
	return (p->instruction >> 10) & MASK_LOWER3BITS;
}

static inline uword RegB(Processor* p)
{
	return p->registers[(p->instruction >> 7) & MASK_LOWER3BITS];
}

static inline uword RegC(Processor* p)
{
	return p->registers[p->instruction & MASK_LOWER3BITS];
}

static inline word Immediate(Processor* p)
{
	return ((word)(p->instruction << 9)) >> 9;
}

static inline uword MemAddress(Processor* p)
{
	return (uword)(RegB(p) + Immediate(p));
}

static inline void SetReg(Processor* p, uword value)
{
	uword i = RegA(p);
	if (i > 0) p->registers[i] = value;
}

void Processor_Run(Processor* p)
{
	bool halt = false;

	while (!halt)
	{
		p->instruction = p->memory.data[p->programCounter];
		uword next = p->programCounter + 1;
		uword result = 0;

		switch ((p->instruction >> 13) & MASK_LOWER3BITS)
		{
		case 0: // ADD
			SetReg(p, RegB(p) + RegC(p));
			break;
		case 1: // ADDI
			SetReg(p, RegB(p) + Immediate(p));
			break;
		case 2: // NAND
			SetReg(p, ~(RegB(p) & RegC(p)));
			break;
		case 3: // LUI
			SetReg(p, (p->instruction << 6) & MASK_UPPER10BITS);
			break;
		case 4: // SW
			p->memory.data[MemAddress(p)] = p->registers[RegA(p)];
			break;
		case 5: // LW
			SetReg(p, p->memory.data[MemAddress(p)]);
			break;
		case 6: // BEQ
			if (p->registers[RegA(p)] == RegB(p))
				next += Immediate(p);
			break;
		case 7: // JALR or SYS

			SetReg(p, next);
			next = RegB(p);
			result = Immediate(p);

			// Handle extended op code
			switch (result & MASK_OPX)
			{
			case SYS_SLEEP:
				SDL_Delay((result & MASK_SYSVALUE) * 100);
				break;
			case SYS_NOP:
				break;
			case SYS_HALT:
				halt = true;
				break;
			}

			break;
		}

		p->programCounter = next;
	}
}
