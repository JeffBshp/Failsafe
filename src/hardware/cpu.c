#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "cglm/cglm.h"
#include "../language/kernel.h"
#include "../language/float16.h"
#include "cpu.h"
#include "device.h"
#include "memory.h"

// Originally based on the RiSC-16 Instruction Set Architecture.

Cpu* Cpu_New(Device device, Disk disk, Memory memory)
{
	Cpu* cpu = calloc(1, sizeof(Cpu));
	cpu->cycles = 0;
	cpu->halt = true;
	cpu->device = device;
	cpu->disk = disk;
	cpu->memory = memory;
	cpu->device.memory = memory;
	cpu->device.irq = &cpu->irq;
	cpu->disk.memory = memory;
	cpu->poweredOn = false;
	return cpu;
}

bool Cpu_Boot(Cpu *cpu)
{
	memset(cpu->registers, 0, 16);
	memset(cpu->memory.data, 0, cpu->memory.n * sizeof(uint16_t));
	uint16_t startAddress = Kernel_Load(cpu->memory);
	if (startAddress == 0xffff) return false;

	Disk_Reset(&cpu->disk);
	cpu->device.timerTicks = -1;
	cpu->instructionPointer = startAddress;
	cpu->irq = 0;
	cpu->cycles = 0;
	cpu->interruptEnable = false;
	cpu->halt = false;
	cpu->poweredOn = true;
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

static inline void CompSigned(uword* r, uword comp)
{
	word a = r[REG_OPERAND_A];
	word b = r[REG_OPERAND_B];
	bool result;

	switch (comp)
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

static inline void CompUnsigned(uword* r, uword comp)
{
	uword a = r[REG_OPERAND_A];
	uword b = r[REG_OPERAND_B];
	bool result;

	switch (comp)
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

static void ExecuteNextInstruction(Cpu *cpu)
{
	uword* r = cpu->registers;
	uword* mem = cpu->memory.data;
	Instruction instr;
	instr.bits = mem[cpu->instructionPointer];
	uword next = cpu->instructionPointer + 1;
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
		case OPX_CAS:
			if (mem[r[instr.regB]] == 0)
			{
				mem[r[instr.regB]] = r[instr.regC];
				r[instr.regC] = 0;
			}
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
				case OPXX_COMPS:
					CompSigned(r, instr.comp);
					break;
				case OPXX_COMPU:
					CompUnsigned(r, instr.comp);
					break;
				case OPXX_RET:
					r[REG_STACK_PTR] = r[REG_FRAME_PTR]; // drop args/locals from stack
					next = r[REG_RET_ADDR]; // jump to RA
					break;
				case OPXX_IRET:
					// pop return address
					next = mem[--(r[REG_STACK_PTR])];
					// pop registers from when the task was interrupted
					for (int i = 6; i >= 1; i--)
						r[i] = mem[--(r[REG_STACK_PTR])];
					cpu->interruptEnable = true;
					break;
				case OPXX_IEN:
					cpu->interruptEnable = instr.immed7 != 0;
					break;
				case OPXX_INT:
					cpu->irq |= 1 << (instr.immed7 & 0x0f);
					break;
				case OPXX_HALT:
					cpu->halt = true;
					break;
				default: break;
			}
			break;
		default: break;
		}
		break;
	default: break;
	}

	cpu->instructionPointer = next;
}

// Runs the processor for a certain number of ticks (milliseconds).
// It runs at one cycle per tick, which is 1 kHz.
// All instructions take exactly one cycle.
void Cpu_Run(Cpu* cpu, int ticks)
{
	if (!cpu->poweredOn) return;

	uint64_t cycles = ticks * 100ull;

	if (cpu->interruptEnable && cpu->irq != 0)
	{
		int irq, bit, isr;
		for (irq = 0; irq < 16; irq++)
		{
			bit = 1 << irq;
			if ((cpu->irq & bit) != 0)
			{
				isr = cpu->memory.data[irq];
				cpu->irq &= ~bit;

				if (isr != 0)
				{
					// push registers onto the stack of the interrupted task
					for (int i = 1; i <= 6; i++)
						cpu->memory.data[(cpu->registers[REG_STACK_PTR])++] = cpu->registers[i];

					// push the RA needed to resume the task later
					cpu->memory.data[(cpu->registers[REG_STACK_PTR])++] = cpu->instructionPointer;

					cpu->interruptEnable = false;
					cpu->instructionPointer = isr;
					cpu->halt = false;
					break;
				}
			}
		}
	}

	// Calibrate cycles on the first frame. It will begin executing next frame.
	if (cpu->cycles == 0) cpu->cycles = cycles;

	while (cpu->cycles < cycles && !cpu->halt && cpu->instructionPointer < cpu->memory.n)
	{
		ExecuteNextInstruction(cpu);
		cpu->cycles++;
	}

	Device_Update(&cpu->device, ticks);
	Disk_Update(&cpu->disk, ticks);
}
