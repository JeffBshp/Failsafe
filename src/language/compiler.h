#pragma once

#include "parser.h"
#include "../hardware/processor.h"

enum
{
	MAXINSTR = 1000,
	MAXFUNCCALS = 500,
	MAXSTRLEN = 500,
};

typedef enum
{
	COMPILE_SUCCESS, // no errors
	COMPILE_TOOMANYINSTR, // compiler limits the number of instructions it will emit
	COMPILE_TOOMANYCALLS, // compiler limits the number of function calls it will process
	COMPILE_TOOMANYVARS, // too many params + local vars in a function; limited due to low-level implementation details
	COMPILE_INTOUTOFRANGE, // integer literal out of range for the architecture's word length
	COMPILE_STRINGTOOLONG, // compiler limits the length of string literals
	COMPILE_INVALIDVALUE, // literal value expression has invalid data type (somehow)
	COMPILE_IDENTUNDEF, // tried to use an identifier that was undefined
	COMPILE_FUNCUNDEF, // tried to call a function that was undefined
	COMPILE_INVALIDOP, // invalid data type(s) for a given operation
	COMPILE_INVALIDEXPR, // invalid expression type (somehow)
	COMPILE_INVALIDTYPE, // tried to assign a value to a variable of an incompatible type
	COMPILE_BRANCHTOOFAR, // too many instructions inside a loop or conditional body to skip over with a branch instruction
	COMPILE_INVALIDARG, // function argument has wrong type, or function called with wrong number of args
	COMPILE_INVALIDSTMT, // invalid statement type (somehow)
	COMPILE_MAINREDEFINED, // main function defined more than once
	COMPILE_NOTIMPLEMENTED, // feature has not been implemented
} CompileStatus;

typedef struct
{
	int instructionAddress;
	int functionIndex;
} FunctionReference;

typedef struct
{
	SyntaxTree* ast;
	Instruction instructions[MAXINSTR];
	FunctionReference functionReferences[MAXFUNCCALS];
	int numInstructions;
	int numReferences;
	int mainAddress;
	int functionIndex;
	CompileStatus status;
} CompileContext;

typedef struct
{
	CompileStatus status;
	uword mainAddress;
	uword length;
	uword* bin;
} Program;

Program* Compiler_GenerateCode(SyntaxTree* ast);
Compiler_Destroy(Program* p);
