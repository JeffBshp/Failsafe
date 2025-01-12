#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "SDL2/SDL.h"
#include "runner.h"
#include "parser.h"

#pragma region Operations

static inline EX_Value InvalidValue()
{
	return (EX_Value) { .type = DAT_INVALID, .value.asInt = (int)NULL };
}

static EX_Value Evaluate(RunContext* c, Expression* e);
static bool ExecuteStatement(RunContext* c, Statement* s);

static EX_Value OpNegate(RunContext* c, EX_Operation op)
{
	EX_Value a = Evaluate(c, op.a);
	ValUnion result;

	switch (a.type)
	{
	case DAT_INT: result.asInt = -a.value.asInt;
	case DAT_FLOAT: result.asFloat = -a.value.asFloat;
	default: return InvalidValue();
	}

	return (EX_Value) { .type = DAT_BOOL, .value = result };;
}

static EX_Value OpNot(RunContext* c, EX_Operation op)
{
	EX_Value a = Evaluate(c, op.a);
	ValUnion result;

	switch (a.type)
	{
	case DAT_INT: result.asInt = ~a.value.asInt;
	case DAT_BOOL: result.asBool = !a.value.asBool;
	default: return InvalidValue();
	}

	return (EX_Value) { .type = DAT_BOOL, .value = result };;
}

#pragma region Arithmetic

static EX_Value OpArithmetic(RunContext* c, EX_Operation op, bool (opFunction)(DataType, ValUnion, ValUnion, ValUnion*))
{
	EX_Value a = Evaluate(c, op.a);
	EX_Value b = Evaluate(c, op.b);

	if (a.type == b.type)
	{
		ValUnion result;
		if (opFunction(a.type, a.value, b.value, &result))
			return (EX_Value) { .type = a.type, .value = result };
	}

	return InvalidValue();
}

static bool OpAdd(DataType type, ValUnion a, ValUnion b, ValUnion* result)
{
	switch (type)
	{
	case DAT_INT: (*result).asInt = a.asInt + b.asInt; break;
	case DAT_FLOAT: (*result).asFloat = a.asFloat + b.asFloat; break;
	// TODO: strcat
	default: (*result).asString = NULL; return false;
	}

	return true;
}

static bool OpSub(DataType type, ValUnion a, ValUnion b, ValUnion* result)
{
	switch (type)
	{
	case DAT_INT: (*result).asInt = a.asInt - b.asInt; break;
	case DAT_FLOAT: (*result).asFloat = a.asFloat - b.asFloat; break;
	default: (*result).asString = NULL; return false;
	}

	return true;
}

static bool OpMul(DataType type, ValUnion a, ValUnion b, ValUnion* result)
{
	switch (type)
	{
	case DAT_INT: (*result).asInt = a.asInt * b.asInt; break;
	case DAT_FLOAT: (*result).asFloat = a.asFloat * b.asFloat; break;
	default: (*result).asString = NULL; return false;
	}

	return true;
}

static bool OpDiv(DataType type, ValUnion a, ValUnion b, ValUnion* result)
{
	switch (type)
	{
	case DAT_INT: (*result).asInt = a.asInt / b.asInt; break;
	case DAT_FLOAT: (*result).asFloat = a.asFloat / b.asFloat; break;
	default: (*result).asString = NULL; return false;
	}

	return true;
}

static bool OpMod(DataType type, ValUnion a, ValUnion b, ValUnion* result)
{
	if (type == DAT_INT)
	{
		(*result).asInt = a.asInt % b.asInt;
		return true;
	}

	(*result).asString = NULL;
	return false;
}

static bool OpBwAnd(DataType type, ValUnion a, ValUnion b, ValUnion* result)
{
	if (type == DAT_INT)
	{
		(*result).asInt = a.asInt & b.asInt;
		return true;
	}

	(*result).asString = NULL;
	return false;
}

static bool OpBwOr(DataType type, ValUnion a, ValUnion b, ValUnion* result)
{
	if (type == DAT_INT)
	{
		(*result).asInt = a.asInt | b.asInt;
		return true;
	}

	(*result).asString = NULL;
	return false;
}

#pragma endregion

#pragma region Boolean

static EX_Value OpBoolean(RunContext* c, EX_Operation op, bool (*opFunction)(DataType, ValUnion, ValUnion, bool*))
{
	EX_Value a = Evaluate(c, op.a);
	EX_Value b = Evaluate(c, op.b);

	if (a.type == b.type)
	{
		bool result;
		if (opFunction(a.type, a.value, b.value, &result))
			return (EX_Value) { .type = DAT_BOOL, .value.asBool = result };
	}

	return InvalidValue();
}

static bool OpGreaterThan(DataType type, ValUnion a, ValUnion b, bool* result)
{
	switch (type)
	{
	case DAT_INT: *result = a.asInt > b.asInt; break;
	case DAT_FLOAT: *result = a.asFloat > b.asFloat; break;
	default: *result = false; return false;
	}

	return true;
}

static bool OpGreaterEqual(DataType type, ValUnion a, ValUnion b, bool* result)
{
	switch (type)
	{
	case DAT_INT: *result = a.asInt >= b.asInt; break;
	case DAT_FLOAT: *result = a.asFloat >= b.asFloat; break;
	default: *result = false; return false;
	}

	return true;
}

static bool OpLessThan(DataType type, ValUnion a, ValUnion b, bool* result)
{
	switch (type)
	{
	case DAT_INT: *result = a.asInt < b.asInt; break;
	case DAT_FLOAT: *result = a.asFloat < b.asFloat; break;
	default: *result = false; return false;
	}

	return true;
}

static bool OpLessEqual(DataType type, ValUnion a, ValUnion b, bool* result)
{
	switch (type)
	{
	case DAT_INT: *result = a.asInt <= b.asInt; break;
	case DAT_FLOAT: *result = a.asFloat <= b.asFloat; break;
	default: *result = false; return false;
	}

	return true;
}

static bool OpEqual(DataType type, ValUnion a, ValUnion b, bool* result)
{
	switch (type)
	{
	case DAT_INT: *result = a.asInt == b.asInt; break;
	case DAT_FLOAT: *result = a.asFloat == b.asFloat; break;
	case DAT_BOOL: *result = a.asBool == b.asBool; break;
	// TODO: strcmp
	default: *result = false; return false;
	}

	return true;
}

static bool OpNotEq(DataType type, ValUnion a, ValUnion b, bool* result)
{
	switch (type)
	{
	case DAT_INT: *result = a.asInt == b.asInt; break;
	case DAT_FLOAT: *result = a.asFloat == b.asFloat; break;
	case DAT_BOOL: *result = a.asBool == b.asBool; break;
	// TODO: strcmp
	default: *result = false; return false;
	}

	return true;
}

static bool OpLogAnd(DataType type, ValUnion a, ValUnion b, bool* result)
{
	if (type == DAT_BOOL)
	{
		*result = a.asBool && b.asBool;
		return true;
	}

	*result = false;
	return false;
}

static bool OpLogOr(DataType type, ValUnion a, ValUnion b, bool* result)
{
	if (type == DAT_BOOL)
	{
		*result = a.asBool || b.asBool;
		return true;
	}

	*result = false;
	return false;
}

#pragma endregion

#pragma endregion

#pragma region Expressions

static Variable* FindVar(RunContext* c, char* name)
{
	for (int i = 0; i < c->numVars; i++)
	{
		Variable* var = c->vars + i;

		if (0 == strcmp(var->name, name))
			return var;
	}

	return NULL;
}

static EX_Value ReadVar(RunContext* c, EX_Read r)
{
	Variable* var = FindVar(c, r);
	if (var == NULL) return InvalidValue();

	return (EX_Value)
	{
		.type = var->type,
			.value = var->value // shallow copy
	};
}

static EX_Value EvaluateOperation(RunContext* c, EX_Operation op)
{
	switch (op.type)
	{
	case OP_ACCESS: break; // TODO?
	case OP_NEGATE: return OpNegate(c, op);
	case OP_NOT: return OpNot(c, op);
	case OP_MULTIPLY: return OpArithmetic(c, op, &OpMul);
	case OP_DIVIDE: return OpArithmetic(c, op, &OpDiv);
	case OP_MODULO: return OpArithmetic(c, op, &OpMod);
	case OP_ADD: return OpArithmetic(c, op, &OpAdd);
	case OP_SUBTRACT: return OpArithmetic(c, op, &OpSub);
	case OP_JOIN: break; // TODO
	case OP_GREATERTHAN: return OpBoolean(c, op, &OpGreaterThan);
	case OP_GREATEREQUAL: return OpBoolean(c, op, &OpGreaterEqual);
	case OP_LESSTHAN: return OpBoolean(c, op, &OpLessThan);
	case OP_LESSEQUAL: return OpBoolean(c, op, &OpLessEqual);
	case OP_EQUAL: return OpBoolean(c, op, &OpEqual);
	case OP_NOTEQUAL: return OpBoolean(c, op, &OpNotEq);
	case OP_LOG_AND: return OpBoolean(c, op, &OpLogAnd);
	case OP_LOG_OR: return OpBoolean(c, op, &OpLogOr);
	case OP_BW_AND: return OpArithmetic(c, op, &OpBwAnd);
	case OP_BW_OR: return OpArithmetic(c, op, &OpBwOr);
	}

	return InvalidValue();
}

static EX_Value CallFunction(RunContext* c, ST_Call call)
{
	// find the function
	Function* function = NULL;
	for (int i = 0; i < c->ast->numFunctions; i++)
	{
		Function* f = c->ast->functions + i;

		if (0 == strcmp(f->name, call.name))
		{
			function = f;
			break;
		}
	}

	if (function == NULL || call.numArguments != function->numParams)
		return InvalidValue();

	// give the function a new context
	RunContext subContext = { .ret = false };
	subContext.ast = c->ast;
	subContext.numVars = function->numParams + function->numLocals;
	subContext.vars = calloc(subContext.numVars, sizeof(Variable));
	subContext.rVal.type = function->rtype;
	subContext.rVal.value.asString = NULL;

	// evaluate args
	for (int i = 0; i < call.numArguments; i++)
	{
		EX_Value arg = Evaluate(c, call.arguments + i);
		if (arg.type != function->params[i].type) return InvalidValue();

		Variable* var = subContext.vars + i;
		var->name = function->params[i].name; // shallow copy
		var->type = arg.type;
		var->value = arg.value; // shallow copy
	}

	// initialize locals
	for (int i = 0; i < function->numLocals; i++)
	{
		Variable* var = subContext.vars + call.numArguments + i;
		var->name = function->locals[i].name; // shallow copy
		var->type = function->locals[i].type;
		var->value.asString = NULL;
	}

	// execute
	switch (function->id)
	{
	case EXTF_PRINT:
		if (subContext.numVars == 2 &&
			subContext.vars[0].type == DAT_STRING &&
			subContext.vars[1].type == DAT_INT)
		{
			char* arg1 = subContext.vars[0].value.asString;
			int arg2 = subContext.vars[1].value.asInt;
			printf("%s %d\n", arg1, arg2);
		}
		break;
	case EXTF_SLEEP:
		if (subContext.numVars == 1 &&
			subContext.vars[0].type == DAT_INT)
		{
			int arg = subContext.vars[0].value.asInt;
			SDL_Delay(arg);
		}
		break;
	case EXTF_MOVE:
		break;
	default:
		for (int i = 0; i < function->numStatements; i++)
		{
			if (!ExecuteStatement(&subContext, function->statements + i)) break;
			if (subContext.ret) break;
		}
		break;
	}

	EX_Value rval = subContext.ret
		? subContext.rVal
		: (EX_Value) { .type = DAT_VOID, .value.asString = NULL };

	free(subContext.vars);

	return rval;
}

static EX_Value Evaluate(RunContext* c, Expression* e)
{
	switch (e->type)
	{
	case EX_VALUE:
		return e->content.asValue; // shallow copy of asValue.value.asString
	case EX_READ:
		return ReadVar(c, e->content.asRead);
	case EX_OPERATION:
		return EvaluateOperation(c, e->content.asOperation);
	case EX_CALL:
		return CallFunction(c, e->content.asCall);
	}
}

#pragma endregion

#pragma region Statements

static bool Assign(RunContext* c, ST_Assign a)
{
	Variable* var = FindVar(c, a.left);
	if (var == NULL) return false;

	EX_Value val = Evaluate(c, a.right);
	if (val.type != var->type) return false;

	var->value = val.value; // shallow copy
	return true;
}

static bool Condition(RunContext* c, ST_Condition cond)
{
	EX_Value evaluatedCondition = Evaluate(c, cond.condition);
	if (evaluatedCondition.type != DAT_BOOL) return false;

	if (evaluatedCondition.value.asBool)
	{
		for (int i = 0; i < cond.numStatements; i++)
		{
			if (!ExecuteStatement(c, cond.statements + i)) return false;
		}
	}

	return true;
}

static bool Loop(RunContext* c, ST_Condition cond)
{
	EX_Value evaluatedCondition = Evaluate(c, cond.condition);
	if (evaluatedCondition.type != DAT_BOOL) return false;

	while (evaluatedCondition.value.asBool)
	{
		for (int i = 0; i < cond.numStatements; i++)
		{
			if (!ExecuteStatement(c, cond.statements + i)) return false;
		}

		evaluatedCondition = Evaluate(c, cond.condition);
	}

	return true;
}

static bool Return(RunContext* c, ST_Return r)
{
	EX_Value v = Evaluate(c, r);
	if (v.type != c->rVal.type) return false;

	c->rVal.value = v.value;
	c->ret = true;
	return true;
}

static bool ExecuteStatement(RunContext* c, Statement* s)
{
	switch (s->type)
	{
	case ST_ASSIGN:
		return Assign(c, s->content.asAssign);
	case ST_CONDITION:
		return Condition(c, s->content.asCondition);
	case ST_LOOP:
		return Loop(c, s->content.asCondition);
	case ST_CALL:
		return DAT_INVALID != CallFunction(c, s->content.asCall).type;
	case ST_RETURN:
		return Return(c, s->content.asReturn);
	}
}

#pragma endregion

void Runner_Execute(SyntaxTree* ast)
{
	// Create a root context that contains the functions as variables
	RunContext root;
	root.ast = ast;
	root.numVars = 0;
	root.vars = NULL;
	root.rVal.type = DAT_VOID;
	root.rVal.value.asString = NULL;
	root.ret = false;

	// find the main function
	Function* fMain = NULL;
	for (int i = 0; i < ast->numFunctions; i++)
	{
		Function* f = ast->functions + i;
		if (f->isMain)
		{
			fMain = f;
			break;
		}
	}

	if (fMain != NULL)
	{
		// Create a root statement that calls the main function
		ST_Call call;
		Expression args;
		call.name = fMain->name;
		call.numArguments = 0;
		call.arguments = &args;

		EX_Value rVal = CallFunction(&root, call);
		printf("Returned %d (DataType %d)\n", rVal.value.asInt, rVal.type);
	}
}
