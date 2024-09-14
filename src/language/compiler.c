#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "compiler.h"
#include "parser.h"
#include "../hardware/processor.h"

#pragma region Helpers

static inline bool AddInstruction(CompileContext* c, Instruction instr)
{
	if (c->status != COMPILE_SUCCESS) return false;

	if (c->numInstructions >= MAXINSTR)
	{
		c->status = COMPILE_TOOMANYINSTR;
		return false;
	}

	c->instructions[c->numInstructions++] = instr;

	return true;
}

static int FindFunction(char* name, Function* functions, int n, Function** f)
{
	for (int i = 0; i < n; i++)
	{
		*f = functions + i;

		if (0 == strcmp((*f)->name, name))
			return i;
	}

	return -1;
}

static int FindParam(char* name, Parameter* params, int n, Parameter* p)
{
	for (int i = 0; i < n; i++)
	{
		*p = params[i];

		if (0 == strcmp(p->name, name))
			return i;
	}

	return -1;
}

static int FindVar(CompileContext* c, char* name, Parameter* p)
{
	Function* f = c->ast->functions + c->functionIndex;
	int i = FindParam(name, f->params, f->numParams, p);

	if (i < 0)
	{
		i = FindParam(name, f->locals, f->numLocals, p);
		if (i >= 0) i += f->numParams;
	}
	
	return i;
}

static inline bool Instr3R(CompileContext* c, int opCode, int ra, int rb, int rc)
{
	Instruction instr = { .bits = 0 };
	instr.opCode = opCode;
	instr.regA = ra;
	instr.regB = rb;
	instr.regC = rc;
	return AddInstruction(c, instr);
}

static inline bool Instr2R(CompileContext* c, int opCode, int ra, int rb, int immed7)
{
	Instruction instr = { .bits = 0 };
	instr.opCode = opCode;
	instr.regA = ra;
	instr.regB = rb;
	instr.immed7 = immed7;
	return AddInstruction(c, instr);
}

static inline bool Instr1R(CompileContext* c, int opCode, int ra, int immed10)
{
	Instruction instr = { .bits = 0 };
	instr.opCode = opCode;
	instr.regA = ra;
	instr.immed10 = immed10;
	return AddInstruction(c, instr);
}

static inline bool InstrStack(CompileContext* c, int opx, int reg)
{
	Instruction instr = { .bits = 0 };
	instr.opCode = INSTR_EXT;
	instr.regA = opx;
	instr.regB = reg;
	instr.immed7 = 0;
	return AddInstruction(c, instr);
}

static inline bool InstrComp(CompileContext* c, int left, int comp, int right)
{
	Instruction instr = { .bits = 0 };
	instr.opCode = INSTR_EXT;
	instr.regA = OPX_COMP;
	instr.regB = left;
	instr.comp = comp;
	instr.regC = right;
	return AddInstruction(c, instr);
}

// Loads a full 16-bit word in two instructions
static bool LoadFullWord(CompileContext* c, uword regIndex, uword value)
{
	uword immed = (value & 0xFFC0) >> 6; // upper 10 bits, shifted right
	Instr1R(c, INSTR_LUI, regIndex, immed);

	immed = value & 0x003F; // lower 6 bits
	return Instr2R(c, INSTR_ADDI, regIndex, regIndex, immed);
}

#pragma endregion

#pragma region Expressions

static bool CompileValueInt(CompileContext* c, int v)
{
	if (-64 <= v && v <= 63)
	{
		return Instr2R(c, INSTR_ADDI, REG_RESULT, REG_ZERO, v);
	}
	else if (v < -32768 || 32767 < v)
	{
		c->status = COMPILE_INTOUTOFRANGE;
		return false;
	}

	return LoadFullWord(c, REG_RESULT, v);
}

static bool CompileValueBool(CompileContext* c, bool v)
{
	word immed = v ? 1 : 0;
	return Instr2R(c, INSTR_ADDI, REG_RESULT, REG_ZERO, immed);
}

static bool CompileValueString(CompileContext* c, char* s)
{
	char schar = s[0];
	int i = 0, n = 0;

	// First leave a space for a branch instruction
	int branchInstr = c->numInstructions++;

	// Then insert the literal characters of the string
	while (true)
	{
		if (i > MAXSTRLEN)
		{
			c->status = COMPILE_STRINGTOOLONG;
			return false;
		}
		else if (s[i] == '\0')
		{
			i += 2;
			n++;
			Instruction instr = { .bits = 0 };
			AddInstruction(c, instr);
			break;
		}
		else
		{
			n++;
			// This is little-endian. Hopefully it works on other machines.
			uword charPair = s[i++];
			charPair |= s[i++] << 8;
			Instruction instr = { .bits = charPair };
			AddInstruction(c, instr);

			if (s[i - 1] == '\0') break;
		}
	}

	// Fill in the branch instruction so it jumps over the char data
	Instruction instr = { .bits = 0 };
	instr.opCode = INSTR_BEZ;
	instr.regA = REG_ZERO;
	instr.immed10 = n;
	c->instructions[branchInstr] = instr;

	// Finally, load the address of the first char
	LoadFullWord(c, REG_RESULT, branchInstr + 1);

	return c->status == COMPILE_SUCCESS;
}

static bool CompileValueExpression(CompileContext* c, EX_Value v, DataType* type)
{
	*type = v.type;

	switch (v.type)
	{
	case DAT_INT: return CompileValueInt(c, v.value.asInt);
	case DAT_FLOAT: c->status = COMPILE_NOTIMPLEMENTED; return false; // TODO
	case DAT_BOOL: return CompileValueBool(c, v.value.asBool);
	case DAT_STRING: return CompileValueString(c, v.value.asString);
	// TODO: make sure void can't actually be assigned to anything
	case DAT_VOID: return Instr3R(c, INSTR_ADD, REG_RESULT, REG_ZERO, REG_ZERO);

	case DAT_INVALID:
	default: c->status = COMPILE_INVALIDVALUE; return false;
	}
}

static bool CompileReadExpression(CompileContext* c, EX_Read r, DataType* type)
{
	Parameter p;
	int i = FindVar(c, r, &p);

	if (i < 0)
	{
		c->status = COMPILE_IDENTUNDEF;
		return false;
	}

	*type = p.type;
	return Instr2R(c, INSTR_LW, REG_RESULT, REG_FRAME_PTR, i);
}

static bool CompileExpression(CompileContext* c, Expression* e, DataType* type);

// Assuming an expression has just been evaluated, this NANDs that result with a given constant.
static bool Nand(CompileContext* c, uword operand)
{
	// load operand into another register (takes 2 instructions)
	LoadFullWord(c, REG_OPERAND_A, operand);
	// NAND with previous expression result
	return Instr3R(c, INSTR_NAND, REG_RESULT, REG_RESULT, REG_OPERAND_A);
}

static bool CompileOpNegate(CompileContext* c, Expression* operand, DataType* type)
{
	if (CompileExpression(c, operand, type))
	{
		if (*type == DAT_INT)
		{
			// bitwise NOT, then ADDI 1
			Nand(c, 0xFFFF);
			return Instr2R(c, INSTR_ADDI, REG_RESULT, REG_RESULT, 1);
		}
		else if (*type == DAT_FLOAT)
		{
			c->status = COMPILE_NOTIMPLEMENTED; // TODO
		}
		else
		{
			c->status = COMPILE_INVALIDOP;
		}
	}

	return false;
}

static bool CompileOpNot(CompileContext* c, Expression* operand, DataType* type)
{
	if (CompileExpression(c, operand, type))
	{
		if (*type == DAT_INT)
		{
			return Nand(c, 0xFFFF); // bitwise NOT
		}
		else if (*type == DAT_BOOL)
		{
			LoadFullWord(c, REG_OPERAND_A, 0xFFFE);
			Instr3R(c, INSTR_ADD, REG_RESULT, REG_RESULT, REG_OPERAND_A);
			return Nand(c, 0xFFFF);
		}
		else
		{
			c->status = COMPILE_INVALIDOP;
		}
	}

	return false;
}

static bool CompileOpAdd(CompileContext* c, DataType aType, DataType bType, DataType* resultType)
{
	if (aType == bType) // TODO: mixed types
	{
		if (aType == DAT_INT)
		{
			*resultType = DAT_INT;
			return Instr3R(c, INSTR_ADD, REG_RESULT, REG_OPERAND_A, REG_OPERAND_B);
		}
		else if (aType == DAT_FLOAT)
		{
			c->status = COMPILE_NOTIMPLEMENTED;
		}
		else if (aType == DAT_STRING)
		{
			c->status = COMPILE_NOTIMPLEMENTED;
		}
		else
		{
			c->status = COMPILE_INVALIDOP;
		}
	}
	else
	{
		c->status = COMPILE_INVALIDOP;
	}

	return false;
}

static bool CompileOpComparison(CompileContext* c, DataType aType, DataType bType, int comp, DataType* resultType)
{
	*resultType = DAT_BOOL;

	if (aType == bType) // TODO: mixed types
	{
		if (aType == DAT_INT || aType == DAT_BOOL)
		{
			return InstrComp(c, REG_OPERAND_A, comp, REG_OPERAND_B);
		}
		else if (aType == DAT_FLOAT)
		{
			c->status = COMPILE_NOTIMPLEMENTED;
		}
		else if (aType == DAT_STRING)
		{
			if (comp == COMP_EQ || comp == COMP_NE)
				c->status = COMPILE_NOTIMPLEMENTED;
			else
				c->status = COMPILE_INVALIDOP;
		}
		else
		{
			c->status = COMPILE_INVALIDOP;
		}
	}
	else
	{
		c->status = COMPILE_INVALIDOP;
	}

	return false;
}

static bool CompileOperationExpression(CompileContext* c, EX_Operation op, DataType* type)
{
	if (op.isBinary)
	{
		DataType aType, bType;

		// evaluate left side and push to stack before evaluating the right side
		if (CompileExpression(c, op.a, &aType))
			InstrStack(c, OPX_PUSH, REG_RESULT);
		else return false;

		// evaluate right side and move result to operand register
		if (CompileExpression(c, op.b, &bType))
			Instr3R(c, INSTR_ADD, REG_OPERAND_B, REG_RESULT, REG_ZERO);
		else return false;

		// pop left operand from stack
		InstrStack(c, OPX_POP, REG_OPERAND_A);

		if (c->status == COMPILE_SUCCESS)
		{
			// do the binary op
			switch (op.type)
			{
			case OP_ADD: return CompileOpAdd(c, aType, bType, type);
			case OP_LESSTHAN: return CompileOpComparison(c, aType, bType, COMP_LT, type);
			case OP_EQUAL: return CompileOpComparison(c, aType, bType, COMP_EQ, type);
			default: c->status = COMPILE_NOTIMPLEMENTED; return false; // TODO
			}
		}
	}

	switch (op.type)
	{
	case OP_NEGATE: return CompileOpNegate(c, op.a, type);
	case OP_NOT: return CompileOpNot(c, op.a, type);
	default: c->status = COMPILE_NOTIMPLEMENTED; return false; // TODO
	}
}

static bool CompileCall(CompileContext* c, ST_Call call, DataType* type);

static bool CompileCallExpression(CompileContext* c, ST_Call call, DataType* type)
{
	if (CompileCall(c, call, type))
	{
		return Instr3R(c, INSTR_ADD, REG_RESULT, REG_RET_VAL, REG_ZERO);
	}

	return false;
}

// Adds instructions that evaluate an expression and place the result in REG_RESULT.
// May overwrite R2 and R3.
static bool CompileExpression(CompileContext* c, Expression* e, DataType* type)
{
	switch (e->type)
	{
	case EX_VALUE: return CompileValueExpression(c, e->content.asValue, type);
	case EX_READ: return CompileReadExpression(c, e->content.asRead, type);
	case EX_OPERATION: return CompileOperationExpression(c, e->content.asOperation, type);
	case EX_CALL: return CompileCallExpression(c, e->content.asCall, type);
	default: c->status = COMPILE_INVALIDEXPR; return false;
	}
}

#pragma endregion

#pragma region Statements

static bool CompileAssign(CompileContext* c, ST_Assign a)
{
	Parameter p;
	int i = FindVar(c, a.left, &p);

	if (i < 0)
	{
		c->status = COMPILE_IDENTUNDEF;
		return false;
	}

	DataType type;
	if (!CompileExpression(c, a.right, &type)) return false;

	if (type != p.type)
	{
		c->status = COMPILE_INVALIDTYPE;
		return false;
	}

	return Instr2R(c, INSTR_SW, REG_RESULT, REG_FRAME_PTR, i);
}

static bool CompileStatement(CompileContext* c, Statement s);

static bool CompileCondition(CompileContext* c, ST_Condition cond)
{
	DataType type;
	if (CompileExpression(c, cond.condition, &type) && type == DAT_BOOL)
	{
		if (Instr1R(c, INSTR_BEZ, REG_RESULT, 0))
		{
			int n = c->numInstructions;
			Instruction* branchInstr = c->instructions + n - 1;

			for (int i = 0; i < cond.numStatements; i++)
			{
				if (!CompileStatement(c, cond.statements[i])) return false;
			}

			n = c->numInstructions - n;

			if (n > 511)
			{
				// too many instructions to branch over
				c->status = COMPILE_BRANCHTOOFAR;
				return false;
			}

			branchInstr->immed10 = n;
			return true;
		}
	}

	return false;
}

static bool CompileLoop(CompileContext* c, ST_Condition cond)
{
	int start = c->numInstructions;

	DataType type;
	if (CompileExpression(c, cond.condition, &type) && type == DAT_BOOL)
	{
		if (Instr1R(c, INSTR_BEZ, REG_RESULT, 0))
		{
			int n = c->numInstructions;
			Instruction* branchInstr = c->instructions + n - 1;

			for (int i = 0; i < cond.numStatements; i++)
			{
				if (!CompileStatement(c, cond.statements[i])) return false;
			}

			int loopBranch = start - c->numInstructions - 1;

			if (loopBranch < -512)
			{
				// too many instructions to branch over
				c->status = COMPILE_BRANCHTOOFAR;
				return false;
			}

			if (Instr1R(c, INSTR_BEZ, REG_ZERO, loopBranch))
			{
				n = c->numInstructions - n;

				if (n > 511)
				{
					// too many instructions to branch over
					c->status = COMPILE_BRANCHTOOFAR;
					return false;
				}

				branchInstr->immed10 = n;
				return true;
			}
		}
	}

	return false;

	// TODO: factor out from loops and conditions
}

static bool CompileCall(CompileContext* c, ST_Call call, DataType* type)
{
	if (c->numReferences >= MAXFUNCCALS)
	{
		c->status = COMPILE_TOOMANYCALLS;
		return false;
	}

	Function* f = NULL;
	int fIndex = FindFunction(call.name, c->ast->functions, c->ast->numFunctions, &f);

	if (fIndex < 0)
	{
		c->status = COMPILE_FUNCUNDEF;
		return false;
	}

	if (f->numParams != call.numArguments)
	{
		c->status = COMPILE_INVALIDARG;
		return false;
	}

	// type pointer is null when the function is called as a statement, not null when called as an expression
	if (type != NULL) *type = f->rtype;

	// Save RA and FP
	InstrStack(c, OPX_PUSH, REG_RET_ADDR);
	InstrStack(c, OPX_PUSH, REG_FRAME_PTR);

	// Push args to stack
	for (int i = 0; i < call.numArguments; i++)
	{
		DataType type;
		if (CompileExpression(c, call.arguments + i, &type))
		{
			if (type == f->params[i].type)
				InstrStack(c, OPX_PUSH, REG_RESULT);
			else
				c->status = COMPILE_INVALIDARG;
		}

		if (c->status != COMPILE_SUCCESS) return false;
	}

	// Make FP point to the previous top of the stack, before the args were pushed
	Instr2R(c, INSTR_ADDI, REG_FRAME_PTR, REG_STACK_PTR, -(call.numArguments));

	// Create a reference to the called function that will be revisited in a second pass, after each function has been assigned an address.
	// The call instruction will advance SP past all of the args and locals, overwrite REG_RET_ADDR, and set PC to the function address.
	// The ret instruction resets SP to the position of FP, which is where it was before pushing the args. Then it sets PC to REG_RET_ADDR.
	FunctionReference* ref = c->functionReferences + c->numReferences;
	ref->functionIndex = fIndex;
	ref->instructionAddress = c->numInstructions;
	c->numReferences++;
	c->numInstructions += 3; // will come back later and add instructions to these skipped slots
	// ^^^ Now numInstructions may be over max. In that case, status will be updated when adding instructions below.

	// Restore the old FP and RA after the called function returns
	InstrStack(c, OPX_POP, REG_FRAME_PTR);
	InstrStack(c, OPX_POP, REG_RET_ADDR);

	return c->status == COMPILE_SUCCESS;
}

static bool CompileReturn(CompileContext* c, ST_Return r)
{
	Function* f = c->ast->functions + c->functionIndex;
	DataType type;

	if (CompileExpression(c, r, &type) && type == f->rtype)
	{
		Instr3R(c, INSTR_ADD, REG_RET_VAL, REG_RESULT, REG_ZERO);
		Instr2R(c, INSTR_EXT, OPX_RET, 0, 0);
	}

	return c->status == COMPILE_SUCCESS;
}

static bool CompileStatement(CompileContext* c, Statement s)
{
	switch (s.type)
	{
	case ST_ASSIGN: return CompileAssign(c, s.content.asAssign);
	case ST_CONDITION: return CompileCondition(c, s.content.asCondition);
	case ST_LOOP: return CompileLoop(c, s.content.asCondition);
	case ST_CALL: return CompileCall(c, s.content.asCall, NULL);
	case ST_RETURN: return CompileReturn(c, s.content.asReturn);
	default: c->status = COMPILE_INVALIDSTMT; return false;
	}
}

#pragma endregion

#pragma region Functions

static bool CompileFunction(CompileContext* c)
{
	// TODO: handle multiple functions or vars with the same name
	Function* f = c->ast->functions + c->functionIndex;
	f->address = c->numInstructions;
	int numVars = f->numParams + f->numLocals;

	// Vars will be accessed with an offset from FP, limited to 7 bits (-64 to 63)
	if (numVars > 64)
	{
		c->status = COMPILE_TOOMANYVARS;
		return false;
	}
	
	if (f->isMain)
	{
		if (c->mainAddress < 0)
		{
			Instr3R(c, INSTR_EXT, OPX_HALT, REG_ZERO, REG_ZERO);
			LoadFullWord(c, REG_RET_ADDR, f->address);

			f->address++;
			c->mainAddress = f->address;

			// Initialize SP and FP. Assume main's args have already been pushed, but no space yet allocated for locals
			Instr2R(c, INSTR_ADDI, REG_FRAME_PTR, REG_STACK_PTR, -(f->numParams));
			Instr2R(c, INSTR_ADDI, REG_STACK_PTR, REG_STACK_PTR, f->numLocals);
		}
		else
		{
			c->status = COMPILE_MAINREDEFINED;
			return false;
		}
	}
	else if (f->isExternal)
	{
		// Insert a NOP instruction to occupy the function's memory address.
		// This instruction won't be executed; the processor specially handles external calls.
		Instr3R(c, INSTR_ADD, REG_ZERO, REG_ZERO, REG_ZERO);
		return true;
	}

	// Compile the function body
	for (int i = 0; i < f->numStatements; i++)
	{
		if (!CompileStatement(c, f->statements[i])) return false;
	}

	if (f->numStatements > 0)
	{
		Statement lastStatement = f->statements[f->numStatements - 1];

		// Return null/zero by default
		if (lastStatement.type != ST_RETURN)
		{
			if (f->isMain)
			{
				Instr3R(c, INSTR_EXT, OPX_HALT, 0, 0);
			}
			else
			{
				EX_Value retValue = { .type = f->rtype, .value.asString = NULL };
				Expression retExpr = { .type = EX_VALUE, .content.asValue = retValue };
				ST_Return ret = &retExpr;
				CompileReturn(c, ret);
			}
		}
	}

	return c->status == COMPILE_SUCCESS;
}

static void ResolveFunctionReference(CompileContext* c, FunctionReference ref)
{
	Function f = c->ast->functions[ref.functionIndex];

	// Load the address into a temporary register
	uword immed = (f.address & 0xFFC0) >> 6; // upper 10 bits, shifted right
	Instruction instr = { .bits = 0 };
	instr.opCode = INSTR_LUI;
	instr.regA = REG_RESULT;
	instr.immed10 = immed;
	c->instructions[ref.instructionAddress++] = instr;

	immed = f.address & 0x003F; // lower 6 bits
	instr.opCode = INSTR_ADDI;
	instr.regA = REG_RESULT;
	instr.regB = REG_ZERO;
	instr.immed7 = immed;
	c->instructions[ref.instructionAddress++] = instr;

	// Call the function
	instr.opCode = INSTR_EXT;
	instr.regA = OPX_CALL;
	instr.regB = REG_RESULT;
	instr.immed7 = f.numLocals;
	c->instructions[ref.instructionAddress++] = instr;
}

#pragma endregion

Program* Compiler_GenerateCode(SyntaxTree* ast)
{
	Program* program = calloc(1, sizeof(Program));
	CompileContext* c = calloc(1, sizeof(CompileContext));
	c->ast = ast;
	c->mainAddress = -1;

	for (c->functionIndex = 0; c->functionIndex < ast->numFunctions; c->functionIndex++)
	{
		if (!CompileFunction(c)) break;
	}

	program->status = c->status;

	if (c->status == COMPILE_SUCCESS)
	{
		for (int i = 0; i < c->numReferences; i++)
		{
			ResolveFunctionReference(c, c->functionReferences[i]);
		}

		program->mainAddress = c->mainAddress;
		program->length = c->numInstructions;

		size_t sizeBytes = c->numInstructions * sizeof(uword);
		program->bin = malloc(sizeBytes);
		memcpy(program->bin, c->instructions, sizeBytes);
	}

	free(c);
	return program;
}

Compiler_Destroy(Program* p)
{
	if (p != NULL)
	{
		free(p->bin);
		free(p);
	}
}
