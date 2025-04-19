#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "compiler.h"
#include "float16.h"
#include "parser.h"
#include "../hardware/cpu.h"

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

static int FindFunction(char *name, FunctionSignature *functions, int n, FunctionSignature **f)
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

static inline bool InstrComp(CompileContext* c, int comp, bool isSigned)
{
	Instruction instr = { .bits = 0 };
	instr.opCode = INSTR_EXT;
	instr.regA = OPX_MORE;
	instr.regB = isSigned ? OPXX_COMPS : OPXX_COMPU;
	instr.comp = comp;
	return AddInstruction(c, instr);
}

static inline bool InstrBinop(CompileContext* c, int op)
{
	Instruction instr = { .bits = 0 };
	instr.opCode = INSTR_EXT;
	instr.regA = OPX_BINOP;
	instr.regB = REG_OPERAND_A;
	instr.comp = op;
	instr.regC = REG_OPERAND_B;
	return AddInstruction(c, instr);
}

// Binary operation on 16-bit floating point numbers.
// Operates on REG_OPERAND_A and REG_OPERAND_B, and saves to REG_RESULT.
// opType is FPOP_MATH or FPOP_COMP
// op is FPMATH_XX or COMP_XX
static inline bool InstrFloat(CompileContext* c, int opType, int op)
{
	Instruction instr = { .bits = 0 };
	instr.opCode = INSTR_EXT;
	instr.regA = OPX_BINOP;
	instr.regB = opType;
	instr.comp = BINOP_FLOAT;
	instr.regC = op;
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

static inline bool BothAreIntegers(DataType a, DataType b)
{
	return (a == DAT_INT || a == DAT_UINT) && (b == DAT_INT || b == DAT_UINT);
}

#pragma endregion

#pragma region Expressions

// TODO: give warning for implicit conversion based on LHS
static bool CompileValueInt(CompileContext* c, int v, DataType* type)
{
	*type = DAT_INT;

	if (-64 <= v && v <= 63)
	{
		return Instr2R(c, INSTR_ADDI, REG_RESULT, REG_ZERO, v);
	}
	else if (v < -32768 || 65535 < v)
	{
		c->status = COMPILE_INTOUTOFRANGE;
		return false;
	}

	if (v > 32767) *type = DAT_UINT;
	return LoadFullWord(c, REG_RESULT, v);
}

static bool CompileValueFloat(CompileContext* c, double v)
{
	float16 f = Float16_FromDouble(v);
	return LoadFullWord(c, REG_RESULT, f.bits);
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
	Instr3R(c, INSTR_ADD, REG_RESULT, REG_RESULT, REG_MODULE_PTR);

	return c->status == COMPILE_SUCCESS;
}

static bool CompileValueExpression(CompileContext* c, EX_Value v, DataType* type)
{
	*type = v.type;

	switch (v.type)
	{
	case DAT_INT: // fall through
	case DAT_UINT: return CompileValueInt(c, v.value.asInt, type);
	case DAT_FLOAT: return CompileValueFloat(c, v.value.asFloat);
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

static bool CompileOpDereference(CompileContext* c, Expression* operand, DataType* type)
{
	if (CompileExpression(c, operand, type))
	{
		if (*type == DAT_INT || *type == DAT_UINT)
		{
			return Instr2R(c, INSTR_LW, REG_RESULT, REG_RESULT, 0);
		}
		else
		{
			c->status = COMPILE_INVALIDDEREF;
		}
	}

	return false;
}

static bool CompileOpNegate(CompileContext* c, Expression* operand, DataType* type)
{
	if (CompileExpression(c, operand, type))
	{
		if (*type == DAT_INT || *type == DAT_UINT)
		{
			// override type if the original number was unsigned
			*type = DAT_INT;

			// bitwise NOT, then ADDI 1
			Nand(c, 0xFFFF);
			return Instr2R(c, INSTR_ADDI, REG_RESULT, REG_RESULT, 1);
		}
		else if (*type == DAT_FLOAT)
		{
			Instr3R(c, INSTR_ADD, REG_OPERAND_A, REG_RESULT, REG_ZERO); // move the float value to operand A
			LoadFullWord(c, REG_OPERAND_B, 0x8000); // operand B has only the sign bit set
			return InstrBinop(c, BINOP_BW_XOR); // REG_RESULT now contains the float value with the sign bit inverted
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
		if (*type == DAT_INT || *type == DAT_UINT)
		{
			// override type if the original number was signed
			*type = DAT_UINT;
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

static bool CompileComparisonOp(CompileContext* c, DataType aType, DataType bType, int comp, DataType* resultType)
{
	*resultType = DAT_BOOL;

	if ((aType == DAT_INT || aType == DAT_UINT) &&
		(bType == DAT_INT || bType == DAT_UINT))
	{
		// default to a signed comparison if at least one is signed
		bool isSigned = aType == DAT_INT || bType == DAT_INT;
		return InstrComp(c, comp, isSigned);
	}
	else if (aType == bType)
	{
		if (aType == DAT_BOOL && (comp == COMP_EQ || comp == COMP_NE))
		{
			return InstrComp(c, comp, false);
		}
		else if (aType == DAT_FLOAT)
		{
			return InstrFloat(c, FPOP_COMP, comp);
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

static bool CompileArithmeticOp(CompileContext* c, DataType aType, DataType bType, OperationType op, DataType* resultType)
{
	if ((aType == DAT_INT || aType == DAT_UINT) &&
		(bType == DAT_INT || bType == DAT_UINT))
	{
		// default to signed if mixing signed with unsigned
		*resultType = aType == bType ? aType : DAT_INT;

		switch (op)
		{
		case OP_MULTIPLY: return InstrBinop(c, BINOP_MULT);
		case OP_DIVIDE: return InstrBinop(c, BINOP_DIV);
		case OP_MODULO: return InstrBinop(c, BINOP_MOD);
		case OP_ADD: return Instr3R(c, INSTR_ADD, REG_RESULT, REG_OPERAND_A, REG_OPERAND_B);
		case OP_SUBTRACT: return InstrBinop(c, BINOP_SUB);
		default: c->status = COMPILE_INVALIDOP; break;
		}
	}
	else if (aType == bType)
	{
		if (aType == DAT_FLOAT)
		{
			*resultType = DAT_FLOAT;

			switch (op)
			{
			case OP_ADD: return InstrFloat(c, FPOP_MATH, FPMATH_ADD);
			case OP_SUBTRACT: return InstrFloat(c, FPOP_MATH, FPMATH_SUB);
			case OP_MULTIPLY: return InstrFloat(c, FPOP_MATH, FPMATH_MUL);
			case OP_DIVIDE: return InstrFloat(c, FPOP_MATH, FPMATH_DIV);
			default: c->status = COMPILE_INVALIDOP; break;
			}
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

	return c->status == COMPILE_SUCCESS;
}

static bool CompileBitwiseOp(CompileContext* c, DataType aType, DataType bType, OperationType op, DataType* resultType)
{
	if ((aType == DAT_INT || aType == DAT_UINT) &&
		(bType == DAT_INT || bType == DAT_UINT))
	{
		// default to unsigned if mixing signed with unsigned
		*resultType = aType == bType ? aType : DAT_UINT;

		switch (op)
		{
		case OP_BW_AND: return InstrBinop(c, BINOP_BW_AND);
		case OP_BW_OR: return InstrBinop(c, BINOP_BW_OR);
		default: c->status = COMPILE_INVALIDOP; break;
		}
	}
	else if (aType == bType)
	{
		if (aType == DAT_BOOL)
		{
			*resultType = DAT_BOOL;

			switch (op)
			{
			// These are just bitwise ops with boolean type checking
			case OP_LOG_AND: return InstrBinop(c, BINOP_BW_AND);
			case OP_LOG_OR: return InstrBinop(c, BINOP_BW_OR);
			default: c->status = COMPILE_INVALIDOP; break;
			}
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

	return c->status == COMPILE_SUCCESS;
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
			case OP_ACCESS: // TODO?
				c->status = COMPILE_NOTIMPLEMENTED;
				return false;

			case OP_MULTIPLY: // fall through
			case OP_DIVIDE:
			case OP_MODULO:
			case OP_ADD:
			case OP_SUBTRACT:
				return CompileArithmeticOp(c, aType, bType, op.type, type);

			case OP_JOIN: // TODO
				c->status = COMPILE_NOTIMPLEMENTED;
				return false;

			case OP_GREATERTHAN: return CompileComparisonOp(c, aType, bType, COMP_GT, type);
			case OP_GREATEREQUAL: return CompileComparisonOp(c, aType, bType, COMP_GE, type);
			case OP_LESSTHAN: return CompileComparisonOp(c, aType, bType, COMP_LT, type);
			case OP_LESSEQUAL: return CompileComparisonOp(c, aType, bType, COMP_LE, type);
			case OP_EQUAL: return CompileComparisonOp(c, aType, bType, COMP_EQ, type);
			case OP_NOTEQUAL: return CompileComparisonOp(c, aType, bType, COMP_NE, type);

			case OP_LOG_AND: // fall through
			case OP_LOG_OR:
			case OP_BW_AND:
			case OP_BW_OR:
				return CompileBitwiseOp(c, aType, bType, op.type, type);

			default:
				c->status = COMPILE_INVALIDOP;
				return false;
			}
		}
	}

	switch (op.type)
	{
	case OP_DEREFERENCE: return CompileOpDereference(c, op.a, type);
	case OP_NEGATE: return CompileOpNegate(c, op.a, type);
	case OP_NOT: return CompileOpNot(c, op.a, type);
	default: c->status = COMPILE_NOTIMPLEMENTED; return false; // TODO
	}
}

static bool CompileCall(CompileContext* c, ST_Call call, DataType* type);

static bool CompileGetReg(CompileContext *c, int regId, DataType* type)
{
	*type = DAT_UINT;

	if (0 <= regId && regId <= 7)
	{
		// copy the target register to RR (unless it's the same register)
		if (regId != REG_RESULT)
			Instr3R(c, INSTR_ADD, REG_RESULT, regId, REG_ZERO);
	}
	else
	{
		c->status = COMPILE_INVALIDREG;
	}

	return c->status == COMPILE_SUCCESS;
}

// Adds instructions that evaluate an expression and place the result in REG_RESULT.
// This may use the stack and overwrite the two operand registers.
static bool CompileExpression(CompileContext* c, Expression* e, DataType* type)
{
	switch (e->type)
	{
	case EX_VALUE: return CompileValueExpression(c, e->content.asValue, type);
	case EX_READ: return CompileReadExpression(c, e->content.asRead, type);
	case EX_OPERATION: return CompileOperationExpression(c, e->content.asOperation, type);
	case EX_CALL: return CompileCall(c, e->content.asCall, type);
	case EX_GETREG: return CompileGetReg(c, e->content.asGetReg, type);
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

	if (a.derefLhs)
	{
		if (p.type != DAT_INT && p.type != DAT_UINT)
		{
			c->status = COMPILE_INVALIDDEREF;
			return false;
		}

		Instr2R(c, INSTR_LW, REG_OPERAND_A, REG_FRAME_PTR, i);
		Instr2R(c, INSTR_SW, REG_RESULT, REG_OPERAND_A, 0);
	}
	else
	{
		if (type != p.type && !BothAreIntegers(type, p.type))
		{
			c->status = COMPILE_INVALIDTYPE;
			return false;
		}

		Instr2R(c, INSTR_SW, REG_RESULT, REG_FRAME_PTR, i);
	}

	return c->status == COMPILE_SUCCESS;
}

static bool CompileStatement(CompileContext* c, Statement s);

static bool CompileCondition(CompileContext* c, ST_Condition cond)
{
	int n1 = 0;
	int n2 = 0;
	Instruction* branchInstr1 = NULL; // conditional branch over the if block
	Instruction* branchInstr2 = NULL; // inside the if block; unconditional branch over the else block

	if (cond.condition != NULL)
	{
		DataType type;
		if (CompileExpression(c, cond.condition, &type))
		{
			if (type == DAT_BOOL)
			{
				n1 = c->numInstructions;
				Instr1R(c, INSTR_BEZ, REG_RESULT, 0);
				branchInstr1 = c->instructions + n1;
			}
			else
			{
				c->status = COMPILE_INVALIDTYPE;
			}
		}
	}

	if (c->status == COMPILE_SUCCESS)
	{
		for (int i = 0; i < cond.numStatements; i++)
		{
			if (!CompileStatement(c, cond.statements[i])) return false;
		}

		// if there's an else block, jump over it before exiting the if block
		if (cond.orElse != NULL)
		{
			n2 = c->numInstructions;
			Instr1R(c, INSTR_BEZ, REG_ZERO, 0);
			branchInstr2 = c->instructions + n2;
		}

		// if there was a condition, then the branch needs to be filled in
		if (branchInstr1 != NULL)
		{
			// the first branch needs to jump past the second one (if it exists)
			// and land in the else block (if it exists) or on the next statement
			n1 = c->numInstructions - n1 - 1;

			// too many instructions to branch over?
			if (n1 > 511) c->status = COMPILE_BRANCHTOOFAR;
			else branchInstr1->immed10 = n1;
		}
	}

	if (c->status == COMPILE_SUCCESS && cond.orElse != NULL)
	{
		// recursively compile the else block
		if (CompileCondition(c, *cond.orElse))
		{
			// fill in the second branch so it jumps over the else block
			n2 = c->numInstructions - n2 - 1;

			if (n2 > 511) c->status = COMPILE_BRANCHTOOFAR;
			else branchInstr2->immed10 = n2;
		}
	}

	return c->status == COMPILE_SUCCESS;
}

static bool CompileLoop(CompileContext* c, ST_Condition cond)
{
	if (c->numLoops >= MAXLOOPS)
	{
		c->status = COMPILE_TOOMANYLOOPS;
		return false;
	}

	int start = c->numInstructions;

	DataType type;
	if (CompileExpression(c, cond.condition, &type) && type == DAT_BOOL)
	{
		// insert a conditional branch that checks the loop condition
		if (Instr1R(c, INSTR_BEZ, REG_RESULT, 0))
		{
			// reset breaks, increment nested loop level
			memset(c->breakOffsets[c->numLoops], 0, MAXBREAKS);
			c->numBreaks[c->numLoops] = 0;
			c->numLoops++;

			// remember the offset of the first branch instr
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

			// branch back to the start
			if (Instr1R(c, INSTR_BEZ, REG_ZERO, loopBranch))
			{
				n = c->numInstructions - n;

				if (n > 511)
				{
					// too many instructions to branch over
					c->status = COMPILE_BRANCHTOOFAR;
					return false;
				}

				// end this loop context
				branchInstr->immed10 = n;
				c->numLoops--;
				int numBreaks = c->numBreaks[c->numLoops];
				uint16_t *offsets = c->breakOffsets[c->numLoops];

				// fill in break instructions so they branch out of the loop
				for (int i = 0; i < numBreaks; i++)
				{
					uint16_t offset = offsets[i];
					Instruction *breakInstr = c->instructions + offset;
					breakInstr->immed10 = c->numInstructions - offset - 1;
				}

				return true;
			}
		}
	}

	return false;

	// TODO: factor out from loops and conditions
}

static bool CompileBreak(CompileContext *c)
{
	if (c->numLoops == 0)
	{
		c->status = COMPILE_INVALIDBREAK;
		return false;
	}

	int i = c->numLoops - 1;
	int j = c->numBreaks[i]; // number of breaks before adding this one == the index of this new one

	if (j >= MAXBREAKS)
	{
		c->status = COMPILE_TOOMANYBREAKS;
		return false;
	}

	// remember this offset to revisit at the end of the loop
	c->numBreaks[i]++;
	c->breakOffsets[i][j] = c->numInstructions;

	// insert branch instr to be modified later when the end of the loop is known
	return Instr1R(c, INSTR_BEZ, REG_ZERO, 0);
}

static bool CompileCall(CompileContext* c, ST_Call call, DataType* type)
{
	if (c->numReferences >= MAXFUNCCALS)
	{
		c->status = COMPILE_TOOMANYCALLS;
		return false;
	}

	FunctionSignature *fs = NULL;
	int fIndex = FindFunction(call.name, c->functionSignatures, c->numFunctions, &fs);

	if (fIndex < 0)
	{
		c->status = COMPILE_FUNCUNDEF;
		return false;
	}

	if (fs->numParams != call.numArguments)
	{
		c->status = COMPILE_INVALIDARG;
		return false;
	}

	// type pointer is null when the function is called as a statement, not null when called as an expression
	if (type != NULL) *type = fs->returnType;

	// Save RA, FP, and MP
	InstrStack(c, OPX_PUSH, REG_RET_ADDR);
	InstrStack(c, OPX_PUSH, REG_FRAME_PTR);
	if (fs->importIndex >= 0) InstrStack(c, OPX_PUSH, REG_MODULE_PTR);

	// Push args to stack
	for (int i = 0; i < call.numArguments; i++)
	{
		DataType pType = fs->paramTypes[i];
		DataType eType;

		if (CompileExpression(c, call.arguments + i, &eType))
		{
			if (eType == pType || BothAreIntegers(eType, pType))
				InstrStack(c, OPX_PUSH, REG_RESULT);
			else
				c->status = COMPILE_INVALIDARG;
		}

		if (c->status != COMPILE_SUCCESS) return false;
	}

	// Make FP point to the previous top of the stack, before the args were pushed
	Instr2R(c, INSTR_ADDI, REG_FRAME_PTR, REG_STACK_PTR, -(call.numArguments));

	if (fs->importIndex >= 0) // imported function
	{
		Instr2R(c, INSTR_LW, REG_MODULE_PTR, REG_MODULE_PTR, fs->importIndex); // switch to other module
		LoadFullWord(c, REG_RESULT, fs->offset); // load function offset
		Instr3R(c, INSTR_ADD, REG_RESULT, REG_RESULT, REG_MODULE_PTR); // calculate function address
		Instr2R(c, INSTR_EXT, OPX_CALL, REG_RESULT, fs->numLocals); // call the function
		InstrStack(c, OPX_POP, REG_MODULE_PTR); // restore mod ptr
	}
	else // local function
	{
		// Create a reference to the called function that will be revisited in a second pass, after each function has been assigned an address.
		// The call instruction will advance SP past all of the args and locals, overwrite REG_RET_ADDR, and set PC to the function address.
		// The ret instruction resets SP to the position of FP, which is where it was before pushing the args. Then it sets PC to REG_RET_ADDR.
		FunctionReference* ref = c->functionReferences + c->numReferences;
		ref->functionIndex = fIndex;
		ref->instructionAddress = c->numInstructions;
		c->numReferences++;
		c->numInstructions += 4; // will come back later and add instructions to these skipped slots
		// ^^^ Now numInstructions may be over max. In that case, status will be updated when adding instructions below.
	}

	// Restore the old FP and RA after the called function returns
	InstrStack(c, OPX_POP, REG_FRAME_PTR);
	InstrStack(c, OPX_POP, REG_RET_ADDR);

	return c->status == COMPILE_SUCCESS;
}

static bool CompileReturn(CompileContext* c, ST_Return r)
{
	Function* f = c->ast->functions + c->functionIndex;
	DataType type;

	if (r == NULL)
	{
		if (f->rtype == DAT_VOID)
		{
			Instr3R(c, INSTR_ADD, REG_RESULT, REG_ZERO, REG_ZERO);
			Instr2R(c, INSTR_EXT, OPX_MORE, OPXX_RET, 0);
		}
		else
		{
			c->status = COMPILE_INVALIDRETTYPE;
		}
	}
	else if (CompileExpression(c, r, &type))
	{
		// return value is in REG_RESULT
		if (type == f->rtype)
			Instr2R(c, INSTR_EXT, OPX_MORE, OPXX_RET, 0);
		else
			c->status = COMPILE_INVALIDRETTYPE;
	}

	return c->status == COMPILE_SUCCESS;
}

static bool CompileRawInstruction(CompileContext* c, int raw)
{
	Instruction instr = { .bits = raw };
	return AddInstruction(c, instr);
}

static bool CompileSetReg(CompileContext* c, ST_SetReg s)
{
	DataType type;
	int regId = s.registerId;

	if (CompileExpression(c, s.expr, &type))
	{
		if (1 <= regId && regId <= 7)
		{
			// copy the result to the target register (unless it's the same register)
			if (regId != REG_RESULT)
				Instr3R(c, INSTR_ADD, regId, REG_RESULT, REG_ZERO);
		}
		else
		{
			c->status = COMPILE_INVALIDREG;
		}
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
	case ST_BREAK: return CompileBreak(c);
	case ST_RETURN: return CompileReturn(c, s.content.asReturn);
	case ST_INSTR: return CompileRawInstruction(c, s.content.asInstr);
	case ST_SETREG: return CompileSetReg(c, s.content.asSetReg);
	default: c->status = COMPILE_INVALIDSTMT; return false;
	}
}

#pragma endregion

#pragma region Functions

static bool CompileFunction(CompileContext* c)
{
	// TODO: handle multiple functions or vars with the same name
	Function* f = c->ast->functions + c->functionIndex;
	FunctionSignature *fs = c->functionSignatures + c->functionIndex;
	fs->offset = c->numInstructions;
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
			c->mainAddress = fs->offset;

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
			EX_Value retValue = { .type = f->rtype, .value.asString = NULL };
			Expression retExpr = { .type = EX_VALUE, .content.asValue = retValue };
			ST_Return ret = &retExpr;
			CompileReturn(c, ret);
		}
	}

	return c->status == COMPILE_SUCCESS;
}

static void ResolveFunctionReference(CompileContext* c, FunctionReference ref)
{
	FunctionSignature *fs = c->functionSignatures + ref.functionIndex;
	Instruction instr = { .bits = 0 };

	// load the function offset into a temporary register
	instr.opCode = INSTR_LUI;
	instr.regA = REG_RESULT;
	instr.immed10 = (fs->offset & 0xFFC0) >> 6; // upper 10 bits, shifted right
	c->instructions[ref.instructionAddress++] = instr;

	instr.opCode = INSTR_ADDI;
	instr.regA = REG_RESULT;
	instr.regB = REG_RESULT;
	instr.immed7 = fs->offset & 0x003F; // lower 6 bits
	c->instructions[ref.instructionAddress++] = instr;

	// calculate the absolute address
	instr.opCode = INSTR_ADD;
	instr.regA = REG_RESULT;
	instr.regB = REG_RESULT;
	instr.regC = REG_MODULE_PTR;
	c->instructions[ref.instructionAddress++] = instr;

	// call the function
	instr.opCode = INSTR_EXT;
	instr.regA = OPX_CALL;
	instr.regB = REG_RESULT;
	instr.immed7 = fs->numLocals;
	c->instructions[ref.instructionAddress++] = instr;
}

#pragma endregion

static Program *GenerateCode(CompileContext *c)
{
	if (c->status != COMPILE_SUCCESS) return NULL;

	for (c->functionIndex = 0; c->functionIndex < c->ast->numFunctions; c->functionIndex++)
	{
		if (!CompileFunction(c)) break;
	}

	if (c->status == COMPILE_SUCCESS)
	{
		Program *program = calloc(1, sizeof(Program));

		for (int i = 0; i < c->numReferences; i++)
		{
			ResolveFunctionReference(c, c->functionReferences[i]);
		}

		// copy string pointers
		program->numImports = c->ast->numImports;
		program->imports = malloc(program->numImports * sizeof(char *));
		for (int i = 0; i < program->numImports; i++)
			program->imports[i] = DeepCopyStr(c->ast->imports[i]);

		// copy only local function signatures to the compiled module, not imported
		program->numFunctions = c->ast->numFunctions;
		program->functions = calloc(program->numFunctions, sizeof(FunctionSignature));
		for (int i = 0; i < program->numFunctions; i++)
			program->functions[i] = c->functionSignatures[i];

		program->mainAddress = c->mainAddress;
		program->length = c->numInstructions;

		size_t sizeBytes = c->numInstructions * sizeof(uword);
		program->bin = malloc(sizeBytes);
		memcpy(program->bin, c->instructions, sizeBytes);

		return program;
	}

	return NULL;
}

static void WriteCompiledModule(char *sourcePath, Program *module);
static Program *ReadCompiledModule(char *sourcePath);

// Recursively compiles a source file and its dependencies.
// Produces a module that's ready to load and run.
// Also wites the compiled module to disk in a custom file format.
Program *Compiler_BuildFile(char *filePath)
{
	Program *module = ReadCompiledModule(filePath);
	if (module != NULL) return module;

	CompileContext* c = calloc(1, sizeof(CompileContext));
	c->mainAddress = -1;
	c->ast = Parser_ParseFile(filePath);
	c->numInstructions = c->ast->numImports; // skip this many instruction slots at the start
	int n;

	// add function signatures to the context so they can be looked up when compiling function calls
	for (n = 0; n < c->ast->numFunctions; n++)
	{
		Function *f = c->ast->functions + n;
		FunctionSignature *fs = c->functionSignatures + n;
		strcpy(fs->name, f->name);
		fs->importIndex = -1; // mark this as a local function
		fs->offset = 0; // to be filled in during code gen
		fs->returnType = (uint8_t)f->rtype;
		fs->numLocals = f->numLocals;
		fs->numParams = f->numParams;
		for (int j = 0; j < f->numParams; j++)
			fs->paramTypes[j] = (uint8_t)f->params[j].type;
	}

	char pathBuffer[MAXSTRLEN];

	// also add signatures of imported functions
	for (int i = 0; i < c->ast->numImports; i++)
	{
		sprintf(pathBuffer, "res/code/%s.txt", c->ast->imports[i]);
		Program *importedModule = ReadCompiledModule(pathBuffer);
		if (importedModule == NULL)
			importedModule = Compiler_BuildFile(pathBuffer);

		for (int j = 0; j < importedModule->numFunctions; j++)
		{
			if (n >= MAXFUNCTIONS)
			{
				c->status = COMPILE_TOOMANYFUNCTIONS;
				goto finish;
			}

			c->functionSignatures[n] = importedModule->functions[j];
			c->functionSignatures[n].importIndex = i;
			n++;
		}
	}

	c->numFunctions = n;
	module = GenerateCode(c);

	finish:
	if (module == NULL) printf("Compile Error: %d\n", c->status);
	else WriteCompiledModule(filePath, module);

	Parser_Destroy(c->ast);
	free(c);

	return module;
}

void Compiler_Destroy(Program* p)
{
	if (p != NULL)
	{
		for (int i = 0; i < p->numImports; i++)
			free(p->imports[i]);

		free(p->imports);
		free(p->functions);
		free(p->bin);
		free(p);
	}
}

static int WritePackedString(char *s, FILE *file)
{
	int i = 0;
	int n = 0;
	uint16_t value;

	do
	{
		value = s[n++];

		if (value != 0)
		{
			value |= s[n++] << 8;
		}

		fwrite(&value, sizeof(uint16_t), 1, file);
		i++;

	} while ((value & 0xff00) != 0);

	return i;
}

static int ReadPackedString(char *s, FILE *file)
{
	int i = 0;
	int n = 0;
	uint16_t value;

	do
	{
		fread(&value, sizeof(uint16_t), 1, file);
		i++;

		s[n++] = (char)(value & 0x00ff);
		s[n++] = (char)(value >> 8);

	} while (s[n - 1] != '\0');

	return i;
}

static inline void WriteWord(uint16_t value, FILE *file)
{
	fwrite(&value, sizeof(uint16_t), 1, file);
}

static inline uint16_t ReadWord(FILE *file)
{
	uint16_t value;
	fread(&value, sizeof(uint16_t), 1, file);
	return value;
}

static void WriteCompiledModule(char *sourcePath, Program *module)
{
	int n = strlen(sourcePath) + 1;
	char *modulePath = malloc(n * sizeof(char));
	strncpy(modulePath, sourcePath, n);
	modulePath[n - 4] = 'm';
	modulePath[n - 3] = 'o';
	modulePath[n - 2] = 'd';
	FILE *file = fopen(modulePath, "wb");

	if (file != NULL)
	{
		uint16_t pos = 14;
		WriteWord(pos, file);
		WriteWord(module->numImports, file);
		fseek(file, pos, SEEK_SET);

		for (int i = 0; i < module->numImports; i++)
		{
			pos += 2 * WritePackedString(module->imports[i], file);
		}

		fseek(file, 4, SEEK_SET);
		WriteWord(pos, file);
		WriteWord(module->numFunctions, file);
		fseek(file, pos, SEEK_SET);

		for (int i = 0; i < module->numFunctions; i++)
		{
			FunctionSignature *fs = module->functions + i;
			WriteWord(fs->offset, file);
			WriteWord(fs->returnType, file);
			pos += 2 * WritePackedString(fs->name, file);
			WriteWord(fs->numLocals, file);
			WriteWord(fs->numParams, file);
			pos += 8;
			fwrite(fs->paramTypes, sizeof(uint16_t), fs->numParams, file);
			pos += 2 * fs->numParams;
		}

		fseek(file, 8, SEEK_SET);
		WriteWord(pos, file);
		WriteWord(module->length, file);
		WriteWord(module->mainAddress, file);
		fseek(file, pos, SEEK_SET);

		fwrite(module->bin, sizeof(uint16_t), module->length, file);
		fclose(file);
	}
}

static Program *ReadCompiledModule(char *sourcePath)
{
	// TODO: rebuild only if source file changed
	return NULL;

	int n = strlen(sourcePath) + 1;
	char *modulePath = malloc(n * sizeof(char));
	strncpy(modulePath, sourcePath, n);
	modulePath[n - 4] = 'm';
	modulePath[n - 3] = 'o';
	modulePath[n - 2] = 'd';
	FILE *file = fopen(modulePath, "rb");
	Program *module = NULL;

	if (file != NULL)
	{
		module = calloc(1, sizeof(Program));
		ReadWord(file);
		module->numImports = ReadWord(file);
		ReadWord(file);
		module->numFunctions = ReadWord(file);
		ReadWord(file);
		module->length = ReadWord(file);
		module->mainAddress = ReadWord(file);

		module->imports = malloc(module->numImports * sizeof(char *));
		module->functions = calloc(module->numFunctions, sizeof(FunctionSignature));
		module->bin = malloc(module->length * sizeof(uint16_t));

		for (int i = 0; i < module->numImports; i++)
		{
			module->imports[i] = malloc(MAXNAMELEN * sizeof(char));
			ReadPackedString(module->imports[i], file);
		}

		for (int i = 0; i < module->numFunctions; i++)
		{
			FunctionSignature *fs = module->functions + i;
			fs->offset = ReadWord(file);
			fs->returnType = ReadWord(file);
			ReadPackedString(fs->name, file);
			fs->numLocals = ReadWord(file);
			fs->numParams = ReadWord(file);
			fread(fs->paramTypes, sizeof(uint16_t), fs->numParams, file);
		}

		fread(module->bin, sizeof(uint16_t), module->length, file);
		fclose(file);
	}

	return module;
}
