#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tokens.h"
#include "parser.h"

#pragma region Helpers

static DataType TokenToDataType(Token tok)
{
	if (tok.type == TOK_KEYWORD)
	{
		switch (tok.value.asKeyword)
		{
		case KW_VOID:
			return DAT_VOID;
		case KW_INT:
			return DAT_INT;
		case KW_FLOAT:
			return DAT_FLOAT;
		case KW_BOOL:
			return DAT_BOOL;
		case KW_STRING:
			return DAT_STRING;
		}
	}

	return DAT_INVALID;
}

static char* DeepCopyStr(char* str)
{
	int n = strlen(str) + 1;
	char* dest = malloc(n * sizeof(char));
	strncpy(dest, str, n);
	return dest;
}

static inline bool SameTokenType(Token a, Token b)
{
	if (a.type == b.type)
	{
		if (a.type == TOK_KEYWORD)
			return a.value.asKeyword == b.value.asKeyword;

		if (a.type == TOK_SYMBOL)
			return a.value.asSymbol == b.value.asSymbol;

		return true;
	}

	return false;
}

static void SkipToken(TokenStream* ts, Token tok)
{
	TokenStream_Next(ts);

	if (ts->status == TSS_ACTIVE && !SameTokenType(ts->token, tok))
		ts->status = TSS_INVALIDTOKEN;
}

static bool TrySkipToken(TokenStream* ts, Token tok)
{
	TokenStream_Next(ts);

	if (SameTokenType(ts->token, tok))
		return true;

	ts->keepToken = true;
	return false;
}

static inline void SkipSymbol(TokenStream* ts, TokenSymbol sym)
{
	SkipToken(ts, (Token) { .type = TOK_SYMBOL, .value = { .asSymbol = sym } });
}

static inline bool TrySkipSymbol(TokenStream* ts, TokenSymbol sym)
{
	return TrySkipToken(ts, (Token) { .type = TOK_SYMBOL, .value = { .asSymbol = sym } });
}

static Token PeekToken(TokenStream* ts)
{
	TokenStream_Next(ts);
	ts->keepToken = true;
	return ts->token;
}

static bool HasToken(TokenStream* ts)
{
	PeekToken(ts);
	return ts->status == TSS_ACTIVE;
}

#pragma endregion

#pragma region BasicParsing

static DataType ReadType(TokenStream* ts)
{
	TokenStream_Next(ts);

	DataType dt = TokenToDataType(ts->token);

	if (ts->status == TSS_ACTIVE)
	{
		if (ts->token.type != TOK_KEYWORD)
			ts->status = TSS_INVALIDTOKEN;
		else if (dt == DAT_INVALID)
			ts->status = TSS_INVALIDDATATYPE;
	}

	return dt;
}

static char* ReadIdentifier(TokenStream* ts)
{
	char* identifier = NULL;

	TokenStream_Next(ts);

	if (ts->status == TSS_ACTIVE)
	{
		if (ts->token.type == TOK_IDENTIFIER)
			identifier = DeepCopyStr(ts->token.value.asString);
		else
			ts->status = TSS_INVALIDTOKEN;
	}
	
	return identifier;
}

static bool TryReadParameter(TokenStream* ts, Parameter* p)
{
	Token next = PeekToken(ts);
	DataType dt = TokenToDataType(next);

	if (dt != DAT_INVALID)
	{
		TokenStream_Next(ts);
		p->name = ReadIdentifier(ts);
		p->type = dt;
		return true;
	}

	return false;
}

static Parameter* ReadParameterList(TokenStream* ts, int* n)
{
	Parameter buffer[CONST_MAXPARAMS];
	*n = 0;

	SkipSymbol(ts, SYM_LPAREN);

	if (TryReadParameter(ts, buffer + *n))
	{
		(*n)++;

		while (ts->status == TSS_ACTIVE
			&& *n < CONST_MAXPARAMS
			&& TrySkipSymbol(ts, SYM_COMMA))
		{
			if (TryReadParameter(ts, buffer + *n))
			{
				(*n)++;
			}
			else
			{
				ts->status = TSS_INVALIDTOKEN;
				break;
			}
		}
	}

	SkipSymbol(ts, SYM_RPAREN);

	Parameter* list = calloc(*n, sizeof(Parameter));

	for (int i = 0; i < *n; i++)
	{
		list[i] = buffer[i];
	}

	return list;
}

#pragma endregion

#pragma region Expressions

static Expression* MakeExpression(ExpressionType type)
{
	Expression* e = malloc(sizeof(Expression));
	e->type = type;
	return e;
}

static Expression* TryReadBinaryExpression(TokenStream* ts, Expression* left);
static Expression* ReadExpressionAtom(TokenStream* ts);

static Expression* ReadExpression(TokenStream* ts)
{
	return TryReadBinaryExpression(ts, ReadExpressionAtom(ts));
}

static Expression* ReadArgumentList(TokenStream* ts, int* n)
{
	Expression* buffer[CONST_MAXPARAMS];
	*n = 0;

	if (!TrySkipSymbol(ts, SYM_RPAREN))
	{
		buffer[(*n)++] = ReadExpression(ts);

		while (ts->status == TSS_ACTIVE
			&& *n < CONST_MAXPARAMS
			&& TrySkipSymbol(ts, SYM_COMMA))
		{
			buffer[(*n)++] = ReadExpression(ts);
		}

		SkipSymbol(ts, SYM_RPAREN);
	}

	Expression* list = malloc((*n) * sizeof(Expression));

	for (int i = 0; i < *n; i++)
	{
		list[i] = *(buffer[i]);
		free(buffer[i]);
	}

	return list;
}

static Expression* ReadIdentifierExpression(TokenStream* ts)
{
	Expression* e;
	char* identifier = DeepCopyStr(ts->token.value.asString);

	if (TrySkipSymbol(ts, SYM_LPAREN))
	{
		e = MakeExpression(EX_CALL);
		e->content.asCall.name = identifier;
		e->content.asCall.arguments = ReadArgumentList(ts, &(e->content.asCall.numArguments));
	}
	else
	{
		e = MakeExpression(EX_READ);
		e->content.asRead = identifier;
	}

	return e;
}

static Expression* ReadExpressionAtom(TokenStream* ts)
{
	Expression* e = NULL;
	TokenStream_Next(ts);

	switch (ts->token.type)
	{
	case TOK_LIT_INT:
		e = MakeExpression(EX_VALUE);
		e->content.asValue.type = DAT_INT;
		e->content.asValue.value.asInt = ts->token.value.asInt;
		break;
	case TOK_LIT_FLOAT:
		e = MakeExpression(EX_VALUE);
		e->content.asValue.type = DAT_FLOAT;
		e->content.asValue.value.asFloat = ts->token.value.asFloat;
		break;
	case TOK_LIT_BOOL:
		e = MakeExpression(EX_VALUE);
		e->content.asValue.type = DAT_BOOL;
		e->content.asValue.value.asBool = ts->token.value.asBool;
		break;
	case TOK_LIT_STRING:
		e = MakeExpression(EX_VALUE);
		e->content.asValue.type = DAT_STRING;
		e->content.asValue.value.asString = DeepCopyStr(ts->token.value.asString);
		break;
	case TOK_IDENTIFIER:
		e = ReadIdentifierExpression(ts);
		break;
	case TOK_SYMBOL:
		switch (ts->token.value.asSymbol)
		{
		case SYM_LPAREN:
			e = ReadExpression(ts);
			SkipSymbol(ts, SYM_RPAREN);
			break;
		case SYM_MINUS:
			e = MakeExpression(EX_OPERATION);
			e->content.asOperation.type = OP_NEGATE;
			e->content.asOperation.isBinary = false;
			e->content.asOperation.a = ReadExpressionAtom(ts);
			e->content.asOperation.b = NULL;
			break;
		case SYM_EXCL:
			e = MakeExpression(EX_OPERATION);
			e->content.asOperation.type = OP_NOT;
			e->content.asOperation.isBinary = false;
			e->content.asOperation.a = ReadExpressionAtom(ts);
			e->content.asOperation.b = NULL;
			break;
		default:
			ts->status = TSS_INVALIDTOKEN;
			break;
		}
		break;
	case TOK_KEYWORD: // fall through
	default:
		ts->status = TSS_INVALIDTOKEN;
		break;
	}

	return e;
}

bool TryReadOperator(TokenStream* ts, OperationType* op)
{
	Token tok = PeekToken(ts);

	if (tok.type != TOK_SYMBOL)
		return false;

	switch (tok.value.asSymbol)
	{
	case SYM_PERIOD:
		*op = OP_ACCESS;
		break;
	case SYM_STAR:
		*op = OP_MULTIPLY;
		break;
	case SYM_SLASH:
		*op = OP_DIVIDE;
		break;
	case SYM_PERCENT:
		*op = OP_MODULO;
		break;
	case SYM_PLUS:
		*op = OP_ADD;
		break;
	case SYM_MINUS:
		*op = OP_SUBTRACT;
		break;
	// return ADD, then check data types, then change to JOIN
	// TODO: case join:
		//*op = OP_JOIN;
		//break;
	case SYM_GREATER:
		*op = OP_GREATERTHAN;
		break;
	case SYM_GREATEREQ:
		*op = OP_GREATEREQUAL;
		break;
	case SYM_LESS:
		*op = OP_LESSTHAN;
		break;
	case SYM_LESSEQ:
		*op = OP_LESSEQUAL;
		break;
	case SYM_2EQUAL:
		*op = OP_EQUAL;
		break;
	case SYM_NOTEQ:
		*op = OP_NOTEQUAL;
		break;
	case SYM_2AND:
		*op = OP_AND;
		break;
	case SYM_2OR:
		*op = OP_OR;
		break;
	default:
		return false;
	}

	TokenStream_Next(ts);
	return true;
}

static Expression* MakeBinaryExpression(OperationType op, Expression* left, Expression* right)
{
	Expression* e = malloc(sizeof(Expression));
	e->type = EX_OPERATION;

	EX_Operation* o = &(e->content.asOperation);
	o->type = op;
	o->isBinary = true;
	o->a = left;
	o->b = right;
	
	return e;
}

static Expression* TryReadBinaryExpression(TokenStream* ts, Expression* left)
{
	OperationType op;
	if (!TryReadOperator(ts, &op)) return left;

	// there's an operator, so there should be another expression
	Expression* next = ReadExpressionAtom(ts);
	Expression* binary;

	if (left->type == EX_OPERATION && left->content.asOperation.isBinary) // TODO: && compare operation order
	{
		// change order of operations because the current op takes priority over the previous (left) op
		EX_Operation* leftOp = &(left->content.asOperation);
		Expression* right = MakeBinaryExpression(op, leftOp->b, next);
		binary = MakeBinaryExpression(leftOp->type, leftOp->a, right);
	}
	else
	{
		// wrap the two operand expressions in a binary operation expression
		binary = MakeBinaryExpression(op, left, next);
	}

	// recurse, in case there's another operator
	return TryReadBinaryExpression(ts, binary);
}

#pragma endregion

#pragma region Statements

static bool TryReadStatement(TokenStream* ts, Statement* s);

static Statement* ReadBlock(TokenStream* ts, int* n)
{
	Statement buffer[CONST_MAXSTATEMENTS];
	*n = 0;

	SkipSymbol(ts, SYM_LBRACE);

	while (ts->status == TSS_ACTIVE && TryReadStatement(ts, buffer + *n))
	{
		(*n)++;

		if (*n > CONST_MAXSTATEMENTS)
		{
			ts->status = TSS_TOOMANYSTATEMENTS;
			return NULL;
		}
	}

	SkipSymbol(ts, SYM_RBRACE);

	Statement* list = calloc(*n, sizeof(Statement));

	for (int i = 0; i < *n; i++)
	{
		list[i] = buffer[i];
	}

	return list;
}

static void ReadConditionStatement(TokenStream* ts, Statement* s, StatementType type)
{
	TokenStream_Next(ts);
	SkipSymbol(ts, SYM_LPAREN);
	s->type = type;
	ST_Condition* cStatement = &(s->content.asCondition);
	cStatement->condition = ReadExpression(ts);
	SkipSymbol(ts, SYM_RPAREN);
	cStatement->statements = ReadBlock(ts, &(cStatement->numStatements));
}

static bool TryReadKeywordStatement(TokenStream* ts, Statement* s)
{
	switch (ts->token.value.asKeyword)
	{
	case KW_IF:
		ReadConditionStatement(ts, s, ST_CONDITION);
		return true;
	case KW_WHILE:
		ReadConditionStatement(ts, s, ST_LOOP);
		return true;
	case KW_RETURN:
		TokenStream_Next(ts);
		s->type = ST_RETURN;
		s->content.asReturn = ReadExpression(ts);
		SkipSymbol(ts, SYM_SEMICOLON);
		return true;
	}

	return false;
}

static bool TryReadIdentifierStatement(TokenStream* ts, Statement* s)
{
	TokenStream_Next(ts);
	char* identifier = DeepCopyStr(ts->token.value.asString);
	Token next = PeekToken(ts);

	if (TrySkipSymbol(ts, SYM_1EQUAL))
	{
		s->type = ST_ASSIGN;
		s->content.asAssign.left = identifier;
		s->content.asAssign.right = ReadExpression(ts);
		SkipSymbol(ts, SYM_SEMICOLON);
		return true;
	}
	else if (TrySkipSymbol(ts, SYM_LPAREN))
	{
		s->type = ST_CALL;
		s->content.asCall.name = identifier;
		s->content.asCall.arguments = ReadArgumentList(ts, &(s->content.asCall.numArguments));
		SkipSymbol(ts, SYM_SEMICOLON);
		return true;
	}

	// TODO: member access, etc.
	return false;
}

static bool TryReadStatement(TokenStream* ts, Statement* s)
{
	Token tok = PeekToken(ts);

	if (tok.type == TOK_KEYWORD)
		return TryReadKeywordStatement(ts, s);
	
	// TODO: Are there any statements that begin with a symbol?
	if (tok.type == TOK_SYMBOL)
		return false;
	
	if (tok.type == TOK_IDENTIFIER)
		return TryReadIdentifierStatement(ts, s);

	return false;
}

#pragma endregion

#pragma region Functions

static void ParseFunction(TokenStream* ts, Function* f)
{
	f->address = -1;
	f->id = -1;
	f->rtype = ReadType(ts);

	const Token mainKeyword = { .type = TOK_KEYWORD, .value = { .asKeyword = KW_MAIN } };
	
	if (TrySkipToken(ts, mainKeyword))
	{
		f->isMain = true;
		f->name = "main";
	}
	else
	{
		f->isMain = false;
		f->name = ReadIdentifier(ts);
	}

	f->params = ReadParameterList(ts, &(f->numParams));
	f->locals = ReadParameterList(ts, &(f->numLocals));
	f->statements = ReadBlock(ts, &(f->numStatements));
	f->isExternal = false;
}

static void AddExternalFunction(Function* f, char* name, Parameter* params, int numParams, ExternalFunctionID id)
{
	f->rtype = DAT_VOID;
	f->name = name;
	f->numParams = numParams;
	f->params = params;
	f->numLocals = 0;
	f->locals = NULL;
	f->numStatements = 0;
	f->statements = NULL;
	f->isMain = false;
	f->isExternal = true;
	f->id = id;
}

#pragma endregion

// TODO: lots of error handling
SyntaxTree* Parser_ParseFile(char* filePath)
{
	enum { MAX_FUNCTIONS = 100 };

	SyntaxTree* ast = calloc(1, sizeof(SyntaxTree));
	Function functionBuffer[MAX_FUNCTIONS];
	Function* functionPtr = functionBuffer;
	TokenStream ts;
	TokenStream_Open(&ts, filePath);

	ast->numFunctions = 2; // number of external functions

	// "print" function
	Parameter* p = calloc(2, sizeof(Parameter));
	p[0] = (Parameter){ .name = "str", .type = DAT_STRING };
	p[1] = (Parameter){ .name = "val", .type = DAT_INT };
	AddExternalFunction(functionPtr++, "print", p, 2, EXTF_PRINT);

	// "sleep" function
	p = calloc(1, sizeof(Parameter));
	p[0] = (Parameter){ .name = "ticks", .type = DAT_INT };
	AddExternalFunction(functionPtr++, "sleep", p, 1, EXTF_SLEEP);

	while (ast->numFunctions <= MAX_FUNCTIONS && HasToken(&ts))
	{
		ParseFunction(&ts, functionPtr++);
		ast->numFunctions++;
	}

	ast->functions = calloc(ast->numFunctions, sizeof(Function));
	memcpy(ast->functions, functionBuffer, ast->numFunctions * sizeof(Function));
	TokenStream_Close(&ts);

	return ast;
}

#pragma region Destroy

static void FreeExpression(Expression* e);
static void FreeStatement(Statement* s);

static void FreeCondition(ST_Condition cond)
{
	for (int i = 0; i < cond.numStatements; i++)
		FreeStatement(cond.statements + i);

	FreeExpression(cond.condition);
}

static void FreeCall(ST_Call call)
{
	for (int i = 0; i < call.numArguments; i++)
		FreeExpression(call.arguments + i);

	free(call.name);
	free(call.arguments);
}

static void FreeExpression(Expression* e)
{
	if (e == NULL) return;

	switch (e->type)
	{
	case EX_VALUE:
		if (e->content.asValue.type == DAT_STRING)
			free(e->content.asValue.value.asString);
		break;
	case EX_READ:
		free(e->content.asRead);
		break;
	case EX_OPERATION:
		FreeExpression(e->content.asOperation.a);
		FreeExpression(e->content.asOperation.b);
		break;
	case EX_CALL:
		FreeCall(e->content.asCall);
		break;
	}
}

static void FreeStatement(Statement* s)
{
	switch (s->type)
	{
	case ST_ASSIGN:
		free(s->content.asAssign.left);
		FreeExpression(s->content.asAssign.right);
		break;
	case ST_CONDITION: // fall through
	case ST_LOOP: FreeCondition(s->content.asCondition); break;
	case ST_CALL: FreeCall(s->content.asCall); break;
	case ST_RETURN: FreeExpression(s->content.asReturn); break;
	}
}

static void FreeFunction(Function* f)
{
	// The external functions are initialized with string literals, which cannot be freed.
	if (!f->isExternal)
	{
		for (int i = 0; i < f->numParams; i++)
			free(f->params[i].name);

		for (int i = 0; i < f->numLocals; i++)
			free(f->locals[i].name);

		// Name of "main" is lexed as a keyword and then points to a literal
		if (!f->isMain) free(f->name);
	}

	for (int i = 0; i < f->numStatements; i++)
		FreeStatement(f->statements + i);

	free(f->params);
	free(f->locals);
	free(f->statements);
}

void Parser_Destroy(SyntaxTree* ast)
{
	if (ast != NULL)
	{
		for (int i = 0; i < ast->numFunctions; i++)
			FreeFunction(ast->functions + i);

		free(ast->functions);
		free(ast);
	}
}

#pragma endregion
