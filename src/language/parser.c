#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
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

static void SkipToken(Lexer* lex, Token tok)
{
	Lexer_NextToken(lex);

	if (lex->status == LEX_ACTIVE && !SameTokenType(lex->token, tok))
		lex->status = LEX_INVALIDTOKEN;
}

static bool TrySkipToken(Lexer* lex, Token tok)
{
	Lexer_NextToken(lex);

	if (SameTokenType(lex->token, tok))
		return true;

	lex->keepToken = true;
	return false;
}

static inline void SkipSymbol(Lexer* lex, TokenSymbol sym)
{
	SkipToken(lex, (Token) { .type = TOK_SYMBOL, .value = { .asSymbol = sym } });
}

static inline bool TrySkipSymbol(Lexer* lex, TokenSymbol sym)
{
	return TrySkipToken(lex, (Token) { .type = TOK_SYMBOL, .value = { .asSymbol = sym } });
}

static Token PeekToken(Lexer* lex)
{
	Lexer_NextToken(lex);
	lex->keepToken = true;
	return lex->token;
}

static bool HasToken(Lexer* lex)
{
	PeekToken(lex);
	return lex->status == LEX_ACTIVE;
}

#pragma endregion

#pragma region BasicParsing

static DataType ReadType(Lexer* lex)
{
	Lexer_NextToken(lex);

	DataType dt = TokenToDataType(lex->token);

	if (lex->status == LEX_ACTIVE)
	{
		if (lex->token.type != TOK_KEYWORD)
			lex->status = LEX_INVALIDTOKEN;
		else if (dt == DAT_INVALID)
			lex->status = LEX_INVALIDDATATYPE;
	}

	return dt;
}

static char* ReadIdentifier(Lexer* lex)
{
	char* identifier = NULL;

	Lexer_NextToken(lex);

	if (lex->status == LEX_ACTIVE)
	{
		if (lex->token.type == TOK_IDENTIFIER)
			identifier = DeepCopyStr(lex->token.value.asString);
		else
			lex->status = LEX_INVALIDTOKEN;
	}

	return identifier;
}

static bool TryReadParameter(Lexer* lex, Parameter* p)
{
	Token next = PeekToken(lex);
	DataType dt = TokenToDataType(next);

	if (dt != DAT_INVALID)
	{
		Lexer_NextToken(lex);
		p->name = ReadIdentifier(lex);
		p->type = dt;
		return true;
	}

	return false;
}

static Parameter* ReadParameterList(Lexer* lex, int* n)
{
	Parameter buffer[CONST_MAXPARAMS];
	*n = 0;

	SkipSymbol(lex, SYM_LPAREN);

	if (TryReadParameter(lex, buffer + *n))
	{
		(*n)++;

		while (lex->status == LEX_ACTIVE
			&& *n < CONST_MAXPARAMS
			&& TrySkipSymbol(lex, SYM_COMMA))
		{
			if (TryReadParameter(lex, buffer + *n))
			{
				(*n)++;
			}
			else
			{
				lex->status = LEX_INVALIDTOKEN;
				break;
			}
		}
	}

	SkipSymbol(lex, SYM_RPAREN);

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

static Expression* TryReadBinaryExpression(Lexer* lex, Expression* left);
static Expression* ReadExpressionAtom(Lexer* lex);

static Expression* ReadExpression(Lexer* lex)
{
	return TryReadBinaryExpression(lex, ReadExpressionAtom(lex));
}

static Expression* ReadArgumentList(Lexer* lex, int* n)
{
	Expression* buffer[CONST_MAXPARAMS];
	*n = 0;

	if (!TrySkipSymbol(lex, SYM_RPAREN))
	{
		buffer[(*n)++] = ReadExpression(lex);

		while (lex->status == LEX_ACTIVE
			&& *n < CONST_MAXPARAMS
			&& TrySkipSymbol(lex, SYM_COMMA))
		{
			buffer[(*n)++] = ReadExpression(lex);
		}

		SkipSymbol(lex, SYM_RPAREN);
	}

	Expression* list = malloc((*n) * sizeof(Expression));

	for (int i = 0; i < *n; i++)
	{
		list[i] = *(buffer[i]);
		free(buffer[i]);
	}

	return list;
}

static Expression* ReadIdentifierExpression(Lexer* lex)
{
	Expression* e;
	char* identifier = DeepCopyStr(lex->token.value.asString);

	if (TrySkipSymbol(lex, SYM_LPAREN))
	{
		e = MakeExpression(EX_CALL);
		e->content.asCall.name = identifier;
		e->content.asCall.arguments = ReadArgumentList(lex, &(e->content.asCall.numArguments));
	}
	else
	{
		e = MakeExpression(EX_READ);
		e->content.asRead = identifier;
	}

	return e;
}

static Expression* ReadExpressionAtom(Lexer* lex)
{
	Expression* e = NULL;
	Lexer_NextToken(lex);

	switch (lex->token.type)
	{
	case TOK_LIT_INT:
		e = MakeExpression(EX_VALUE);
		e->content.asValue.type = DAT_INT;
		e->content.asValue.value.asInt = lex->token.value.asInt;
		break;
	case TOK_LIT_FLOAT:
		e = MakeExpression(EX_VALUE);
		e->content.asValue.type = DAT_FLOAT;
		e->content.asValue.value.asFloat = lex->token.value.asFloat;
		break;
	case TOK_LIT_BOOL:
		e = MakeExpression(EX_VALUE);
		e->content.asValue.type = DAT_BOOL;
		e->content.asValue.value.asBool = lex->token.value.asBool;
		break;
	case TOK_LIT_STRING:
		e = MakeExpression(EX_VALUE);
		e->content.asValue.type = DAT_STRING;
		e->content.asValue.value.asString = DeepCopyStr(lex->token.value.asString);
		break;
	case TOK_IDENTIFIER:
		e = ReadIdentifierExpression(lex);
		break;
	case TOK_SYMBOL:
		switch (lex->token.value.asSymbol)
		{
		case SYM_LPAREN:
			e = ReadExpression(lex);
			SkipSymbol(lex, SYM_RPAREN);
			break;
		case SYM_MINUS:
			e = MakeExpression(EX_OPERATION);
			e->content.asOperation.type = OP_NEGATE;
			e->content.asOperation.isBinary = false;
			e->content.asOperation.a = ReadExpressionAtom(lex);
			e->content.asOperation.b = NULL;
			break;
		case SYM_EXCL:
			e = MakeExpression(EX_OPERATION);
			e->content.asOperation.type = OP_NOT;
			e->content.asOperation.isBinary = false;
			e->content.asOperation.a = ReadExpressionAtom(lex);
			e->content.asOperation.b = NULL;
			break;
		default:
			lex->status = LEX_INVALIDTOKEN;
			break;
		}
		break;
	case TOK_KEYWORD: // fall through
	default:
		lex->status = LEX_INVALIDTOKEN;
		break;
	}

	return e;
}

bool TryReadOperator(Lexer* lex, OperationType* op)
{
	Token tok = PeekToken(lex);

	if (tok.type != TOK_SYMBOL)
		return false;

	switch (tok.value.asSymbol)
	{
	case SYM_PERIOD: *op = OP_ACCESS; break;
	case SYM_STAR: *op = OP_MULTIPLY; break;
	case SYM_SLASH: *op = OP_DIVIDE; break;
	case SYM_PERCENT: *op = OP_MODULO; break;
	case SYM_PLUS: *op = OP_ADD; break;
	case SYM_MINUS: *op = OP_SUBTRACT; break;
	// TODO: OP_JOIN for strings
	case SYM_GREATER: *op = OP_GREATERTHAN; break;
	case SYM_GREATEREQ: *op = OP_GREATEREQUAL; break;
	case SYM_LESS: *op = OP_LESSTHAN; break;
	case SYM_LESSEQ: *op = OP_LESSEQUAL; break;
	case SYM_2EQUAL: *op = OP_EQUAL; break;
	case SYM_NOTEQ: *op = OP_NOTEQUAL; break;
	case SYM_2AND: *op = OP_LOG_AND; break;
	case SYM_2OR: *op = OP_LOG_OR; break;
	case SYM_1AND: *op = OP_BW_AND; break;
	case SYM_1OR: *op = OP_BW_OR; break;
	default: return false;
	}

	Lexer_NextToken(lex);
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

static Expression* TryReadBinaryExpression(Lexer* lex, Expression* left)
{
	OperationType op;
	if (!TryReadOperator(lex, &op)) return left;

	// there's an operator, so there should be another expression
	Expression* next = ReadExpressionAtom(lex);
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
	return TryReadBinaryExpression(lex, binary);
}

#pragma endregion

#pragma region Statements

static bool TryReadStatement(Lexer* lex, Statement* s);

static Statement* ReadBlock(Lexer* lex, int* n)
{
	Statement buffer[CONST_MAXSTATEMENTS];
	*n = 0;

	SkipSymbol(lex, SYM_LBRACE);

	while (lex->status == LEX_ACTIVE && TryReadStatement(lex, buffer + *n))
	{
		(*n)++;

		if (*n > CONST_MAXSTATEMENTS)
		{
			lex->status = LEX_TOOMANYSTATEMENTS;
			return NULL;
		}
	}

	SkipSymbol(lex, SYM_RBRACE);

	Statement* list = calloc(*n, sizeof(Statement));

	for (int i = 0; i < *n; i++)
	{
		list[i] = buffer[i];
	}

	return list;
}

static void ReadConditionStatement(Lexer* lex, Statement* s, StatementType type)
{
	Lexer_NextToken(lex);
	SkipSymbol(lex, SYM_LPAREN);
	s->type = type;
	ST_Condition* cStatement = &(s->content.asCondition);
	cStatement->condition = ReadExpression(lex);
	SkipSymbol(lex, SYM_RPAREN);
	cStatement->statements = ReadBlock(lex, &(cStatement->numStatements));
}

static bool TryReadKeywordStatement(Lexer* lex, Statement* s)
{
	switch (lex->token.value.asKeyword)
	{
	case KW_IF:
		ReadConditionStatement(lex, s, ST_CONDITION);
		return true;
	case KW_WHILE:
		ReadConditionStatement(lex, s, ST_LOOP);
		return true;
	case KW_RETURN:
		Lexer_NextToken(lex);
		s->type = ST_RETURN;
		s->content.asReturn = ReadExpression(lex);
		SkipSymbol(lex, SYM_SEMICOLON);
		return true;
	}

	return false;
}

static bool TryReadIdentifierStatement(Lexer* lex, Statement* s)
{
	Lexer_NextToken(lex);
	char* identifier = DeepCopyStr(lex->token.value.asString);
	Token next = PeekToken(lex);

	if (TrySkipSymbol(lex, SYM_1EQUAL))
	{
		s->type = ST_ASSIGN;
		s->content.asAssign.left = identifier;
		s->content.asAssign.right = ReadExpression(lex);
		SkipSymbol(lex, SYM_SEMICOLON);
		return true;
	}
	else if (TrySkipSymbol(lex, SYM_LPAREN))
	{
		s->type = ST_CALL;
		s->content.asCall.name = identifier;
		s->content.asCall.arguments = ReadArgumentList(lex, &(s->content.asCall.numArguments));
		SkipSymbol(lex, SYM_SEMICOLON);
		return true;
	}

	// TODO: member access, etc.
	return false;
}

static bool TryReadStatement(Lexer* lex, Statement* s)
{
	Token tok = PeekToken(lex);

	if (tok.type == TOK_KEYWORD)
		return TryReadKeywordStatement(lex, s);

	// TODO: Are there any statements that begin with a symbol?
	if (tok.type == TOK_SYMBOL)
		return false;

	if (tok.type == TOK_IDENTIFIER)
		return TryReadIdentifierStatement(lex, s);

	return false;
}

#pragma endregion

#pragma region Functions

static void ParseFunction(Lexer* lex, Function* f)
{
	f->address = -1;
	f->id = -1;
	f->rtype = ReadType(lex);

	const Token mainKeyword = { .type = TOK_KEYWORD, .value = { .asKeyword = KW_MAIN } };

	if (TrySkipToken(lex, mainKeyword))
	{
		f->isMain = true;
		f->name = "main";
	}
	else
	{
		f->isMain = false;
		f->name = ReadIdentifier(lex);
	}

	f->params = ReadParameterList(lex, &(f->numParams));
	f->locals = ReadParameterList(lex, &(f->numLocals));
	f->statements = ReadBlock(lex, &(f->numStatements));
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
	Lexer* lex = Lexer_OpenFile(filePath);

	ast->numFunctions = 4; // number of external functions

	// "print" function
	Parameter* p = calloc(2, sizeof(Parameter));
	p[0] = (Parameter){ .name = "str", .type = DAT_STRING };
	p[1] = (Parameter){ .name = "val", .type = DAT_INT };
	AddExternalFunction(functionPtr++, "print", p, 2, EXTF_PRINT);

	// "sleep" function
	p = calloc(1, sizeof(Parameter));
	p[0] = (Parameter){ .name = "ticks", .type = DAT_INT };
	AddExternalFunction(functionPtr++, "sleep", p, 1, EXTF_SLEEP);

	// "move" function
	p = calloc(1, sizeof(Parameter));
	p[0] = (Parameter){ .name = "direction", .type = DAT_INT };
	AddExternalFunction(functionPtr++, "move", p, 1, EXTF_MOVE);

	// "break" function
	p = calloc(1, sizeof(Parameter));
	p[0] = (Parameter){ .name = "direction", .type = DAT_INT }; // TODO: not used yet
	AddExternalFunction(functionPtr++, "break", p, 1, EXTF_BREAK);

	while (ast->numFunctions <= MAX_FUNCTIONS && HasToken(lex))
	{
		ParseFunction(lex, functionPtr++);
		ast->numFunctions++;
	}

	ast->functions = calloc(ast->numFunctions, sizeof(Function));
	memcpy(ast->functions, functionBuffer, ast->numFunctions * sizeof(Function));
	Lexer_Destroy(lex);

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
