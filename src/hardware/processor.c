#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "cglm/cglm.h"
#include "../language/float16.h"
#include "processor.h"
#include "device.h"
#include "memory.h"

// Based on the RiSC-16 Instruction Set Architecture.

// TODO:
// Constrain stack size and program size.
// Send args to main function (place them on the stack at program startup).
// Send output to the game world via a proper I/O protocol (for now there are just hardcoded functions).
// Add various I/O devices, including disk storage.
// Handle libraries and multiple processes, possibly with an operating system.
// Write an assembler and disassembler.

Processor* Processor_New(Device device, Memory memory)
{
	Processor* p = calloc(1, sizeof(Processor));
	p->ticks = 0;
	p->halt = true;
	p->device = device;
	p->memory = memory;
	return p;
}

void Processor_Reset(Processor* p, uword startAddress, uword stackPointer)
{
	p->ticks = 0;
	p->halt = true;
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

static inline void Comp(uword* r, Instruction instr)
{
	word a = r[instr.regB];
	word b = r[instr.regC];
	bool result;

	switch (instr.comp)
	{
	case COMP_EQ: result = a == b; break;
	case COMP_NE: result = a != b; break;
	case COMP_LT: result = a < b; break;
	case COMP_LE: result = a <= b; break;
	case COMP_GT: result = a > b; break;
	case COMP_GE: result = a >= b; break;
	default: return;
	}

	r[REG_RESULT] = result ? 1 : 0;
}

static inline void FloatMath(uword* r, uword op)
{
	float16 a = { .bits = (word)r[REG_OPERAND_A] };
	float16 b = { .bits = (word)r[REG_OPERAND_B] };
	float16 result;

	switch (op)
	{
	case FPMATH_ADD: result = Float16_Add(a, b); break;
	case FPMATH_SUB: result = Float16_Sub(a, b); break;
	case FPMATH_MUL: result = Float16_Mul(a, b); break;
	case FPMATH_DIV: result = Float16_Div(a, b); break;
	default: return;
	}

	r[REG_RESULT] = result.bits;
}

static inline void FloatComp(uword* r, uword comp)
{
	float16 a = { .bits = (word)r[REG_OPERAND_A] };
	float16 b = { .bits = (word)r[REG_OPERAND_B] };
	bool result;

	switch (comp)
	{
	case COMP_EQ: result = Float16_Equal(a, b); break;
	case COMP_NE: result = !Float16_Equal(a, b); break;
	case COMP_LT: result = Float16_Less(a, b); break;
	case COMP_LE: result = Float16_LessEqual(a, b); break;
	case COMP_GT: result = Float16_Greater(a, b); break;
	case COMP_GE: result = Float16_GreaterEqual(a, b); break;
	default: return;
	}

	r[REG_RESULT] = result ? 1 : 0;
}

static inline void BinaryOp(uword* r, Instruction instr)
{
	word a = r[instr.regB];
	word b = r[instr.regC];
	word result;

	switch (instr.comp)
	{
	case BINOP_SUB:    result = a - b; break;
	case BINOP_MULT:   result = a * b; break;
	case BINOP_DIV:    result = a / b; break;
	case BINOP_MOD:    result = a % b; break;
	case BINOP_BW_AND: result = a & b; break;
	case BINOP_BW_OR:  result = a | b; break;
	case BINOP_BW_XOR: result = a ^ b; break;
	case BINOP_FLOAT:
		switch (instr.regB)
		{
		case FPOP_MATH: FloatMath(r, instr.regC); break;
		case FPOP_COMP: FloatComp(r, instr.regC); break;
		default: break;
		}
		return;
	default: return;
	}

	r[REG_RESULT] = result;
}

static inline void Print(uword* r, uword* mem, Instruction instr)
{
	uword fp = r[REG_FRAME_PTR]; // args are on the stack, starting at the frame pointer
	uword strptr = mem[fp]; // first arg is a pointer to the char array in virtual memory
	char* arg1 = (void*)(mem + strptr); // this is the pointer in real-life memory
	word arg2 = mem[fp + 1]; // second arg is an integer
	printf(">>> %s %d\n", arg1, arg2);
}

static inline void MoveObject(Processor* proc, Instruction instr)
{
	uword fp = proc->registers[REG_FRAME_PTR];
	word arg = proc->memory.data[fp];
	float* vel = (void*)(proc->device.model->vel);
	const float dv = 0.5f;
	glm_vec3_zero(vel);

	switch (arg)
	{
	case 0: vel[0] += dv; break;
	case 1: vel[0] -= dv; break;
	case 2: vel[1] += dv; break;
	case 3: vel[1] -= dv; break;
	case 4: vel[2] += dv; break;
	case 5: vel[2] -= dv; break;
	}
}

#pragma endregion

// Runs the processor for a certain number of ticks (milliseconds).
// It runs at one cycle per tick, which is 1 kHz.
// All instructions take exactly one cycle.
void Processor_Run(Processor* p, int ticks)
{
	const bool log = false;

	// Calibrate ticks on the first frame. It will begin executing next frame.
	if (p->ticks == 0) p->ticks = ticks;

	while (p->ticks < ticks && !p->halt && p->programCounter < p->memory.n)
	{
		uword* r = p->registers;
		uword* mem = p->memory.data;
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
					// ticks also gets incremented below; that is not counted toward the sleep time
					p->ticks += mem[r[REG_FRAME_PTR]];
					r[REG_STACK_PTR] = r[REG_FRAME_PTR];
					break;
				case EXTCALL_MOVE:
					if (log) printf("CALL move(%d)\n", mem[r[REG_FRAME_PTR]]);
					MoveObject(p, instr);
					r[REG_STACK_PTR] = r[REG_FRAME_PTR];
					break;
				case EXTCALL_BREAK:
					if (log) printf("CALL break(%d)\n", mem[r[REG_FRAME_PTR]]);\
					Device_BreakBlock(&p->device);
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
				Comp(r, instr);
				break;
			case OPX_BINOP:
				BinaryOp(r, instr);
				break;
			case OPX_HALT:
				if (log) printf("HALT\n");
				p->halt = true;
				break;
			default: break;
			}
			break;
		default: break;
		}

		p->programCounter = next;
		p->ticks++;
	}
}
