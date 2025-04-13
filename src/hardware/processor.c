#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "cglm/cglm.h"
#include "../language/kernel.h"
#include "../language/float16.h"
#include "processor.h"
#include "device.h"
#include "memory.h"

// Originally based on the RiSC-16 Instruction Set Architecture.

Processor* Processor_New(Device device, Memory memory)
{
	Processor* p = calloc(1, sizeof(Processor));
	p->ticks = 0;
	p->halt = true;
	p->device = device;
	p->memory = memory;
	p->device.memory = memory;
	p->device.irq = &p->irq;
	p->poweredOn = false;
	return p;
}

bool Processor_Boot(Processor *p)
{
	memset(p->registers, 0, 16);
	memset(p->memory.data, 0, p->memory.n * sizeof(uint16_t));
	uint16_t startAddress = Kernel_Load(p->memory.data);
	if (startAddress == 0xffff) return false;

	p->instructionPointer = startAddress;
	p->irq = 0;
	p->ticks = 0;
	p->interruptEnable  = false;
	p->halt = false;
	p->poweredOn = true;
	return true;
}

#pragma region Helpers

static inline void SetReg(uword* r, uword i, uword value)
{
	if (i > 0) r[i] = value;
}

static inline void Shift(uword* r, word n)
{
	uword result = n < 0
		? r[REG_OPERAND_A] << n
		: r[REG_OPERAND_A] >> n;

	SetReg(r, REG_RESULT, result);
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

#pragma endregion

static void ExecuteNextInstruction(Processor *p)
{
	uword* r = p->registers;
	uword* mem = p->memory.data;
	Instruction instr;
	instr.bits = mem[p->instructionPointer];
	uword next = p->instructionPointer + 1;
	uword result = 0;

	switch (instr.opCode)
	{
	case INSTR_ADD:
		SetReg(r, instr.regA, r[instr.regB] + r[instr.regC]);
		break;
	case INSTR_ADDI:
		SetReg(r, instr.regA, r[instr.regB] + instr.immed7);
		break;
	case INSTR_NAND:
		SetReg(r, instr.regA, ~(r[instr.regB] & r[instr.regC]));
		break;
	case INSTR_LUI:
		SetReg(r, instr.regA, instr.immed10 << 6);
		break;
	case INSTR_SW:
		mem[r[instr.regB] + instr.immed7] = r[instr.regA];
		break;
	case INSTR_LW:
		SetReg(r, instr.regA, mem[r[instr.regB] + instr.immed7]);
		break;
	case INSTR_BEZ:
		if (r[instr.regA] == 0) next += instr.immed10;
		break;
	case INSTR_EXT: // extended op code
		switch (instr.regA)
		{
		case OPX_PUSH:
			mem[(r[REG_STACK_PTR])++] = r[instr.regB];
			break;
		case OPX_POP:
			SetReg(r, instr.regB, mem[--(r[REG_STACK_PTR])]);
			break;
		case OPX_CALL:
			r[REG_STACK_PTR] += instr.immed7; // allocate uninitialized locals on stack
			r[REG_RET_ADDR] = next; // set RA
			next = r[instr.regB]; // jump to function
			break;
		case OPX_COMP:
			Comp(r, instr);
			break;
		case OPX_BINOP:
			BinaryOp(r, instr);
			break;
		case OPX_MORE:
			switch (instr.regB)
			{
				case OPXX_SHIFT:
					Shift(r, instr.immed7);
					break;
				case OPXX_RET:
					r[REG_STACK_PTR] = r[REG_FRAME_PTR]; // drop args/locals from stack
					next = r[REG_RET_ADDR]; // jump to RA
					break;
				case OPXX_IRET:
					next = r[REG_RET_ADDR];
					r[REG_STACK_PTR] -= 8;
					memcpy(r, mem + r[REG_STACK_PTR], 16);
					p->interruptEnable = true;
					break;
				case OPXX_IEN:
					p->interruptEnable = instr.immed7 != 0;
					break;
				case OPXX_INT:
					p->irq |= 1 << (instr.immed7 & 0x0f);
					break;
				case OPXX_HALT:
					p->halt = true;
					break;
				default: break;
			}
			break;
		default: break;
		}
		break;
	default: break;
	}

	p->instructionPointer = next;
}

// Runs the processor for a certain number of ticks (milliseconds).
// It runs at one cycle per tick, which is 1 kHz.
// All instructions take exactly one cycle.
void Processor_Run(Processor* p, int ticks)
{
	if (!p->poweredOn) return;

	if (p->interruptEnable && p->irq != 0)
	{
		int irq, bit, isr;
		for (irq = 0; irq < 16; irq++)
		{
			bit = 1 << irq;
			if ((p->irq & bit) != 0)
			{
				isr = p->memory.data[irq];
				p->irq &= ~bit;

				if (isr != 0)
				{
					memcpy(p->memory.data + p->registers[REG_STACK_PTR], p->registers, 16);
					p->registers[REG_STACK_PTR] += 8;
					p->registers[REG_RET_ADDR] = p->instructionPointer;
					p->interruptEnable = false;
					p->instructionPointer = isr;
					p->ticks = ticks;
					p->halt = false;
					break;
				}
			}
		}
	}

	// Calibrate ticks on the first frame. It will begin executing next frame.
	if (p->ticks == 0) p->ticks = ticks;

	while (p->ticks < ticks && !p->halt && p->instructionPointer < p->memory.n)
	{
		ExecuteNextInstruction(p);
		p->ticks++;
	}

	Device_Update(&p->device, ticks);
}
