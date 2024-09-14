#pragma once

#include "parser.h"

typedef struct
{
	char* name;
	DataType type;
	ValUnion value;
} Variable;

typedef struct
{
	SyntaxTree* ast;
	Variable* vars;
	int numVars;
	EX_Value rVal;
	bool ret;
} RunContext;

void Runner_Execute(SyntaxTree* ast);
