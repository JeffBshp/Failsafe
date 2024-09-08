#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "runner.h"
#include "parser.h"

static inline EX_Value InvalidValue()
{
	return (EX_Value) { .type = DAT_INVALID, .value = NULL };
}

static Variable* FindVar(Context* c, char* name)
{
	for (int i = 0; i < c->numVars; i++)
	{
		Variable* var = c->vars + i;

		if (0 == strcmp(var->name, name))
			return var;
	}

	if (c->parent != NULL)
	{
		return FindVar(c->parent, name); // recurse
	}

	return NULL;
}

static EX_Value ReadVar(Context* c, EX_Read r)
{
	Variable* var = FindVar(c, r);
	if (var == NULL) return InvalidValue();

	return (EX_Value)
	{
		.type = var->type,
		.value = var->value // shallow copy
	};
}

static EX_Value Evaluate(Context* c, Expression* e);
static bool ExecuteStatement(Context* c, Statement* s);

#pragma region Operations

static EX_Value OpNegate(Context* c, EX_Operation op)
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

static EX_Value OpNot(Context* c, EX_Operation op)
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

static EX_Value OpArithmetic(Context* c, EX_Operation op, bool (opFunction)(DataType, ValUnion, ValUnion, ValUnion*))
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

#pragma endregion

#pragma region Boolean

static EX_Value OpBoolean(Context* c, EX_Operation op, bool (*opFunction)(DataType, ValUnion, ValUnion, bool*))
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

#pragma endregion

#pragma endregion

static EX_Value EvaluateOperation(Context* c, EX_Operation op)
{
	switch (op.type)
	{
	case OP_NEGATE: return OpNegate(c, op);
	case OP_NOT: return OpNot(c, op);
	case OP_MULTIPLY: return OpArithmetic(c, op, &OpMul);
	case OP_DIVIDE: return OpArithmetic(c, op, &OpDiv);
	case OP_ADD: return OpArithmetic(c, op, &OpAdd);
	case OP_SUBTRACT: return OpArithmetic(c, op, &OpSub);
	case OP_GREATERTHAN: return OpBoolean(c, op, &OpGreaterThan);
	case OP_LESSTHAN: return OpBoolean(c, op, &OpLessThan);
	case OP_EQUAL: return OpBoolean(c, op, &OpEqual);
	case OP_NOTEQUAL: return OpBoolean(c, op, &OpNotEq);
	// TODO: implement the rest
	}

	return InvalidValue();
}

static EX_Value CallFunction(Context* c, ST_Call call)
{
	Variable* v = FindVar(c, call.name);
	if (v == NULL || v->type != DAT_FUNCTION) return InvalidValue();

	Function* function = v->value.asFunction;
	if (call.numArguments != function->numParams) return InvalidValue();

	Context subContext = { .parent = c, .ret = false };
	subContext.numVars = function->numParams + function->numLocals;
	subContext.vars = malloc(subContext.numVars * sizeof(Variable));
	subContext.rVal.type = function->rtype;
	subContext.rVal.value.asString = NULL;

	for (int i = 0; i < call.numArguments; i++)
	{
		EX_Value arg = Evaluate(c, call.arguments + i);
		if (arg.type != function->params[i].type) return InvalidValue();

		Variable* var = subContext.vars + i;
		var->name = function->params[i].name; // shallow copy
		var->type = arg.type;
		var->value = arg.value; // shallow copy
	}

	for (int i = 0; i < function->numLocals; i++)
	{
		Variable* var = subContext.vars + call.numArguments + i;
		var->name = function->locals[i].name; // shallow copy
		var->type = function->locals[i].type;
		var->value.asString = NULL;
	}

	if (function->isExternal)
	{
		// There is one "external" function named "print" that the program can call.
		//
		if (subContext.numVars == 2 &&
			subContext.vars[0].type == DAT_STRING &&
			subContext.vars[1].type == DAT_INT)
		{
			char* arg1 = subContext.vars[0].value.asString;
			int arg2 = subContext.vars[1].value.asInt;
			printf("%s %d\n", arg1, arg2);
		}
		else
		{
			printf("\n");
		}
	}
	else
	{
		for (int i = 0; i < function->numStatements; i++)
		{
			if (!ExecuteStatement(&subContext, function->statements + i)) break;
			if (subContext.ret) return subContext.rVal;
		}
	}

	return (EX_Value) { .type = DAT_VOID, .value.asString = NULL };
}

static EX_Value Evaluate(Context* c, Expression* e)
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

static bool Assign(Context* c, ST_Assign a)
{
	Variable* var = FindVar(c, a.left);
	if (var == NULL) return false;

	EX_Value val = Evaluate(c, a.right);
	if (val.type != var->type) return false;

	var->value = val.value; // shallow copy
	return true;
}

static bool Condition(Context* c, ST_Condition cond)
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

static bool Loop(Context* c, ST_Condition cond)
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

static bool Return(Context* c, ST_Return r)
{
	EX_Value v = Evaluate(c, r);
	if (v.type != c->rVal.type) return false;

	c->rVal.value = v.value;
	c->ret = true;
	return true;
}

static bool ExecuteStatement(Context* c, Statement* s)
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

void Runner_Execute(Function* f)
{
	// Create a root context that contains the main function as a variable
	Context root;
	root.parent = NULL;
	root.numVars = 2;
	root.vars = malloc(2 * sizeof(Variable));
	root.vars->name = f->name;
	root.vars->type = DAT_FUNCTION;
	root.vars->value.asFunction = f;
	root.rVal.type = DAT_VOID;
	root.rVal.value.asString = NULL;
	root.ret = false;

	// Create a root statement that calls the main function
	ST_Call call;
	Expression args;
	call.name = f->name;
	call.numArguments = 0;
	call.arguments = &args;

	// Params for the "print" function
	Parameter* p = malloc(2 * sizeof(Parameter));
	p[0] = (Parameter){ .name = "str", .type = DAT_STRING };
	p[1] = (Parameter){ .name = "val", .type = DAT_INT };

	// Create an "external" function, which is how the virtual program
	// will interface with the game world.
	Function fp =
	{
		.rtype = DAT_VOID,
		.name = "print",
		.numParams = 2,
		.params = p,
		.numLocals = 0,
		.locals = NULL,
		.numStatements = 0,
		.statements = NULL,
		.isExternal = true,
	};

	// make the function available as a variable to the virtual program
	Variable* fv = root.vars + 1;
	fv->name = fp.name;
	fv->type = DAT_FUNCTION;
	fv->value.asFunction = &fp;

	EX_Value rVal = CallFunction(&root, call);
	printf("Returned %d (DataType %d)\n", rVal.value.asInt, rVal.type);
}
