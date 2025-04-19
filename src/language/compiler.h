#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "lexer.h"
#include "parser.h"
#include "../hardware/cpu.h"

enum
{
	MAXINSTR = 1000,
	MAXFUNCCALS = 500,
	MAXSTRLEN = 500,
	MAXNAMELEN = 100,
	MAXFUNCTIONS = 100,
	MAXLOOPS = 10,
	MAXBREAKS = 10,
};

typedef enum
{
	COMPILE_SUCCESS, // no errors
	COMPILE_TOOMANYINSTR, // compiler limits the number of instructions it will emit
	COMPILE_TOOMANYCALLS, // compiler limits the number of function calls it will process
	COMPILE_TOOMANYVARS, // too many params + local vars in a function; limited due to low-level implementation details
	COMPILE_TOOMANYFUNCTIONS, // too many functions defined or imported
	COMPILE_INTOUTOFRANGE, // integer literal out of range for the architecture's word length
	COMPILE_STRINGTOOLONG, // compiler limits the length of string literals
	COMPILE_INVALIDVALUE, // literal value expression has invalid data type (somehow)
	COMPILE_IDENTUNDEF, // tried to use an identifier that was undefined
	COMPILE_FUNCUNDEF, // tried to call a function that was undefined
	COMPILE_INVALIDOP, // invalid data type(s) for a given operation, or the op itself was not recognized
	COMPILE_INVALIDEXPR, // invalid expression type (somehow)
	COMPILE_INVALIDTYPE, // tried to assign a value to a variable of an incompatible type
	COMPILE_INVALIDRETTYPE, // tried to return the wrong type from a function
	COMPILE_INVALIDREG, // tried to do getreg or setreg on an invalid register ID
	COMPILE_INVALIDBREAK, // tried to break with no loop
	COMPILE_TOOMANYBREAKS, // compiler limits the number of break statements per loop
	COMPILE_TOOMANYLOOPS, // too many nested loops
	COMPILE_BRANCHTOOFAR, // too many instructions inside a loop or conditional body to skip over with a branch instruction
	COMPILE_INVALIDARG, // function argument has wrong type, or function called with wrong number of args
	COMPILE_INVALIDSTMT, // invalid statement type (somehow)
	COMPILE_MAINREDEFINED, // main function defined more than once
	COMPILE_NOTIMPLEMENTED, // feature has not been implemented
	COMPILE_INVALIDDEREF, // tried to dereference an invalid type
} CompileStatus;

typedef struct
{
	int instructionAddress;
	int functionIndex;
} FunctionReference;

typedef struct
{
	int importIndex;
	uint16_t offset;
	uint16_t returnType;
	uint16_t numLocals;
	uint16_t numParams;
	uint16_t paramTypes[CONST_MAXPARAMS];
	char name[MAXNAMELEN];
} FunctionSignature;

typedef struct
{
	SyntaxTree* ast;
	Instruction instructions[MAXINSTR];
	FunctionReference functionReferences[MAXFUNCCALS];
	FunctionSignature functionSignatures[MAXFUNCTIONS];
	uint16_t breakOffsets[MAXLOOPS][MAXBREAKS];
	uint8_t numBreaks[MAXBREAKS];
	int numLoops;
	int numInstructions;
	int numReferences;
	int numFunctions;
	int mainAddress;
	int functionIndex;
	CompileStatus status;
} CompileContext;

typedef struct
{
	int numImports;
	int numFunctions;
	char **imports;
	FunctionSignature *functions;
	uint16_t mainAddress;
	uint16_t length;
	uint16_t *bin;
} Program;

Program* Compiler_BuildFile(char *filePath);
void Compiler_Destroy(Program* p);
