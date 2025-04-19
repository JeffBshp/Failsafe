#pragma once

#include <stdint.h>

#include "device.h"
#include "memory.h"

// TODO: clean up the instruction set architecture
typedef enum
{
	// primary opcodes
	INSTR_ADD,
	INSTR_ADDI,
	INSTR_NAND,
	INSTR_LUI,
	INSTR_SW,
	INSTR_LW,
	INSTR_BEZ,
	INSTR_EXT,

	// extended opcodes given by instr.regA when INSTR_EXT is used
	OPX_PUSH = 0,
	OPX_POP,
	OPX_CALL,
	OPX_RESERVED1,
	OPX_RESERVED2,
	OPX_RESERVED3,
	OPX_BINOP, // additional binary operations on B and C
	OPX_MORE, // even more opcodes that only take immed7 as an operand

	// doubly extended opcodes given by instr.regB when OPX_MORE is used
	OPXX_SHIFT = 0,
	OPXX_COMPS, // (signed) compare OA and OB, save boolean to RR
	OPXX_COMPU, // (unsigned) compare OA and OB, save boolean to RR
	OPXX_RET,
	OPXX_IRET, // return from interrupt
	OPXX_IEN, // set interrupt enable flag
	OPXX_INT, // software interrupt
	OPXX_HALT,

	// comparison codes given by instr.comp (except float comparisons)
	COMP_EQ = 0,
	COMP_NE,
	COMP_LT,
	COMP_LE,
	COMP_GT,
	COMP_GE,

	// binary operations given by instr.comp
	BINOP_SUB = 0,
	BINOP_MULT,
	BINOP_DIV,
	BINOP_MOD,
	BINOP_BW_AND,
	BINOP_BW_OR,
	BINOP_BW_XOR,
	BINOP_FLOAT, // floating point op

	// floating point operations:
	// - this opcode is given by instr.regB
	// - all operate on REG_OPERAND_(A/B) and save to REG_RESULT
	FPOP_MATH = 0, // FPMATH opcode given by instr.regC
	FPOP_COMP, // COMP opcode given by instr.regC

	// floating point math
	FPMATH_ADD = 0,
	FPMATH_SUB,
	FPMATH_MUL,
	FPMATH_DIV,
} OpCode;

enum
{
	// register numbers
	REG_ZERO = 0,			// RZ: always zero
	REG_RESULT = 1,			// RR: general purpose result register; also holds a function's return value
	REG_OPERAND_A = 2,		// OA: general purpose operand
	REG_OPERAND_B = 3,		// OB: general purpose operand
	REG_RET_ADDR = 4,		// RA: where to go after the current function returns
	REG_MODULE_PTR = 5,		// MP: base address of the current module; needed to turn various offsets into absolute addresses
	REG_FRAME_PTR = 6,		// FP: args and locals are accessed relative to the frame pointer, which is stable throughout a function
	REG_STACK_PTR = 7,		// SP: the stack is used for passing args and evaluating nested expressions
};

typedef union
{
	// three-register format (ADD, NAND, PUSH, POP, RET, COMP, HALT)
	struct
	{
		uword regC : 3;
		uword comp : 4;
		uword regB : 3;
		uword regA : 3;
		uword opCode : 3;
	};
	// two-register format (ADDI, SW, LW, CALL, SHIFT)
	struct
	{
		word immed7 : 7; // signed
		uword _unused2 : 9;
	};
	// single-register format (LUI, BEZ)
	struct
	{
		word immed10 : 10;
		uword _unused3 : 6;
	};
	// machine code
	uword bits;
} Instruction;

typedef struct
{
	uint16_t instructionPointer;
	uint16_t irq;
	uint16_t registers[8];
	Memory memory;
	Device device;
	uint64_t cycles;
	bool interruptEnable;
	bool halt;
	bool poweredOn;
} Processor;

Processor* Processor_New(Device device, Memory memory);
bool Processor_Boot(Processor *p);
void Processor_Run(Processor* p, int ticks);
