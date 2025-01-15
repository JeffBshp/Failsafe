#pragma once

#include <stdint.h>
#include "device.h"
#include "memory.h"

typedef enum
{
	// instruction opcodes
	INSTR_ADD,
	INSTR_ADDI,
	INSTR_NAND,
	INSTR_LUI,
	INSTR_SW,
	INSTR_LW,
	INSTR_BEZ,
	INSTR_EXT,

	// extended opcodes
	OPX_PUSH = 0,
	OPX_POP,
	OPX_CALL,
	OPX_RET,
	OPX_SHIFT,
	OPX_COMP, // compare instr.regB and instr.regC, save boolean to REG_RESULT
	OPX_BINOP, // additional binary operations on B and C
	OPX_HALT,

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

	// external function calls
	EXTCALL_PRINT = 0,
	EXTCALL_SLEEP,
	EXTCALL_MOVE,
	EXTCALL_BREAK,
} OpCode;

enum
{
	// register numbers
	REG_ZERO = 0,
	REG_RESULT = 1,
	REG_OPERAND_A = 2,
	REG_OPERAND_B = 3,
	REG_RET_VAL = 4,
	REG_RET_ADDR = 5,
	REG_FRAME_PTR = 6,
	REG_STACK_PTR = 7,
};

typedef union
{
	// three-register format (ADD, NAND, PUSH, POP, RET, COMP, HALT)
	struct
	{
		uword regC : 3;
		uword comp : 4; // COMP instructions only
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
	uword programCounter;
	uword startAddress;
	uword instruction;
	uword registers[8];
	Memory memory;
	Device device;
	bool halt;
} Processor;

Processor* Processor_New(Device device, Memory memory);
void Processor_Reset(Processor* p, uword startAddress, uword stackPointer);
void Processor_Run(Processor* p);
