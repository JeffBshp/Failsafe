#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "SDL.h"
#include "processor.h"
#include "memory.h"

// Based on the RiSC-16 Instruction Set Architecture.

// TODO:
// Constrain stack size and program size.
// Send args to main function (place them on the stack at program startup).
// Send output to the game world via a proper I/O protocol (for now there are just hardcoded functions).
// Add various I/O devices, including disk storage.
// Handle libraries and multiple processes, possibly with an operating system.
// Write an assembler and disassembler.

Processor* Processor_New(Memory memory)
{
	Processor* p = calloc(1, sizeof(Processor));
	p->memory = memory;
	return p;
}

void Processor_Reset(Processor* p, uword startAddress, uword stackPointer)
{
	p->programCounter = startAddress;
	p->startAddress = 0;
	p->registers[REG_STACK_PTR] = stackPointer;
}

#pragma region Helpers

static inline void SetReg(uword* r, uword i, uword value)
{
	if (i > 0) r[i] = value;
}

static inline void Shift(uword* r, uword i, word n)
{
	uword result = n < 0 ? r[i] << n : r[i] >> n;
	SetReg(r, i, result);
}

static inline void Comp(uword* r, bool result)
{
	r[REG_RESULT] = result ? 1 : 0;
}

static inline void Print(uword* r, uword* mem, Instruction instr)
{
	uword fp = r[REG_FRAME_PTR]; // args are on the stack, starting at the frame pointer
	uword strptr = mem[fp]; // first arg is a pointer to the char array in virtual memory
	char* arg1 = mem + strptr; // this is the pointer in real-life memory
	word arg2 = mem[fp + 1]; // second arg is an integer
	printf(">>> %s %d\n", arg1, arg2);
}

#pragma endregion

void Processor_Run(Processor* p)
{
	const bool log = false;

	uword* r = &(p->registers);
	uword* mem = p->memory.data;
	bool halt = false;

	while (!halt && p->programCounter < p->memory.n)
	{
		Instruction instr;
		instr.bits = mem[p->programCounter];
		p->instruction = instr.bits;
		uword next = p->programCounter + 1;
		uword result = 0;

		switch (instr.opCode)
		{
		case INSTR_ADD:
			SetReg(r, instr.regA, r[instr.regB] + r[instr.regC]);
			if (log) printf("ADD %d\n", r[instr.regA]);
			break;
		case INSTR_ADDI:
			SetReg(r, instr.regA, r[instr.regB] + instr.immed7);
			if (log) printf("ADDI %d\n", r[instr.regA]);
			break;
		case INSTR_NAND:
			SetReg(r, instr.regA, ~(r[instr.regB] & r[instr.regC]));
			if (log) printf("NAND %d\n", r[instr.regA]);
			break;
		case INSTR_LUI:
			SetReg(r, instr.regA, instr.immed10 << 6);
			if (log) printf("LUI %d\n", r[instr.regA]);
			break;
		case INSTR_SW:
			mem[r[instr.regB] + instr.immed7] = r[instr.regA];
			if (log) printf("SW %d at %d\n", r[instr.regA], r[instr.regB] + instr.immed7);
			break;
		case INSTR_LW:
			SetReg(r, instr.regA, mem[r[instr.regB] + instr.immed7]);
			if (log) printf("LW %d from %d\n", r[instr.regA], r[instr.regB] + instr.immed7);
			break;
		case INSTR_BEZ:
			if (log) printf("BEZ (%d) --> %d\n", r[instr.regA], instr.immed10);
			if (r[instr.regA] == 0)
				next += instr.immed10;
			break;
		case INSTR_EXT: // extended op code
			switch (instr.regA)
			{
			case OPX_PUSH:
				if (log) printf("PUSH %d (%d)\n", r[instr.regB], r[REG_STACK_PTR]);
				mem[(r[REG_STACK_PTR])++] = r[instr.regB];
				break;
			case OPX_POP:
				SetReg(r, instr.regB, mem[--(r[REG_STACK_PTR])]);
				if (log) printf("POP %d (%d)\n", r[instr.regB], r[REG_STACK_PTR]);
				break;
			case OPX_CALL:
				switch (r[instr.regB])
				{
				case EXTCALL_PRINT:
					if (log) printf("CALL print (%d)\n", instr.immed7);
					Print(r, mem, instr);
					r[REG_STACK_PTR] = r[REG_FRAME_PTR];
					break;
				case EXTCALL_SLEEP:
					if (log) printf("CALL sleep(%d)\n", mem[r[REG_FRAME_PTR]]);
					SDL_Delay(mem[r[REG_FRAME_PTR]]);
					r[REG_STACK_PTR] = r[REG_FRAME_PTR];
					break;
				default:
					if (log) printf("CALL %d (%d)\n", r[instr.regB], instr.immed7);
					r[REG_STACK_PTR] += instr.immed7;
					r[REG_RET_ADDR] = next;
					next = p->startAddress + r[instr.regB];
					break;
				}
				break;
			case OPX_RET:
				if (log) printf("RET %d\n", r[REG_RET_ADDR]);
				r[REG_STACK_PTR] = r[REG_FRAME_PTR];
				next = r[REG_RET_ADDR];
				break;
			case OPX_SHIFT:
				Shift(r, instr.regB, instr.immed7);
				if (log) printf("SHIFT %d (%d)\n", r[instr.regB], instr.immed7);
				break;
			case OPX_COMP:
				if (log) printf("COMP (%d) %d (%d)\n", r[instr.regB], instr.comp, r[instr.regC]);
				switch (instr.comp)
				{
				case COMP_EQ: Comp(r, r[instr.regB] == r[instr.regC]); break;
				case COMP_NE: Comp(r, r[instr.regB] != r[instr.regC]); break;
				case COMP_LT: Comp(r, r[instr.regB] < r[instr.regC]); break;
				case COMP_LE: Comp(r, r[instr.regB] <= r[instr.regC]); break;
				case COMP_GT: Comp(r, r[instr.regB] > r[instr.regC]); break;
				case COMP_GE: Comp(r, r[instr.regB] >= r[instr.regC]); break;
				default: break;
				}
				break;
			case OPX_BINOP:
				switch (instr.comp)
				{
				case BINOP_SUB:    SetReg(r, REG_RESULT, ((word)r[REG_OPERAND_A]) - ((word)r[REG_OPERAND_B])); break;
				case BINOP_MULT:   SetReg(r, REG_RESULT, ((word)r[REG_OPERAND_A]) * ((word)r[REG_OPERAND_B])); break;
				case BINOP_DIV:    SetReg(r, REG_RESULT, ((word)r[REG_OPERAND_A]) / ((word)r[REG_OPERAND_B])); break;
				case BINOP_MOD:    SetReg(r, REG_RESULT, ((word)r[REG_OPERAND_A]) % ((word)r[REG_OPERAND_B])); break;
				case BINOP_BW_AND: SetReg(r, REG_RESULT, ((word)r[REG_OPERAND_A]) & ((word)r[REG_OPERAND_B])); break;
				case BINOP_BW_OR:  SetReg(r, REG_RESULT, ((word)r[REG_OPERAND_A]) | ((word)r[REG_OPERAND_B])); break;
				default: break;
				}
				break;
			case OPX_HALT:
				if (log) printf("HALT\n");
				halt = true;
				break;
			default: break;
			}
			break;
		default: break;
		}

		p->programCounter = next;
	}
}
