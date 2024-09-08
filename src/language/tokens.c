#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "tokens.h"

static void NextChar(TokenStream* ts)
{
	if (ts->status != TSS_ACTIVE)
		return;

	ts->i++;

	if (ts->i < 0 || ts->i > ts->n || ts->n < 0)
	{
		ts->status = TSS_INDEXERROR;
	}
	else if (ts->i < ts->n)
	{
		ts->c = ts->buffer[ts->i];
	}
	else // i == n
	{
		if (ts->n >= CONST_BUFFERSIZE)
		{
			ts->i--;
			ts->status = TSS_ENDOFBUFFER;
		}
		else
		{
			ts->n++;
			ts->c = fgetc(ts->file);
			ts->buffer[ts->i] = (char)ts->c;

			if (ts->c == EOF)
				ts->status = TSS_ENDOFFILE;
		}
	}
}

void TokenStream_Open(TokenStream* ts, char* filePath)
{
	ts->file = fopen(filePath, "r");

	if (ts->file == NULL)
	{
		ts->status = TSS_FILEERROR;
	}
	else
	{
		// initialize struct
		ts->buffer = malloc(CONST_BUFFERSIZE * sizeof(char));
		ts->subbuffer = malloc(CONST_SUBBUFSIZE * sizeof(char));
		memset(ts->buffer, 0, CONST_BUFFERSIZE * sizeof(char));
		memset(ts->subbuffer, 0, CONST_SUBBUFSIZE * sizeof(char));
		ts->i = -1;
		ts->n = 0;
		ts->token.type = TOK_NULL;
		ts->token.value.asString = NULL;
		ts->status = TSS_ACTIVE;
		ts->keepToken = false;

		// read first char to increment i to 0 and establish a value for c
		NextChar(ts);
	}
}

static inline bool IsNotNewLine(int c)
{
	return (char)c != '\n';
}

static inline bool IsNotQuote(int c)
{
	return c != SYM_QUOTE;
}

static int SkipWhile(TokenStream* ts, bool (*test)(int))
{
	int start = ts->i;

	while (ts->status == TSS_ACTIVE && test(ts->c))
		NextChar(ts);

	// return number of chars skipped
	// i now points to the first char that failed
	return ts->i - start;
}

static void ReadWord(TokenStream* ts)
{
	int start = ts->i;
	int n = 1;

	static const char* keywords[] =
	{
		"if",
		"else",
		"while",
		"return",
		"void",
		"null",
		"int",
		"float",
		"bool",
		"string"
	};

	NextChar(ts);
	n += SkipWhile(ts, &isalnum);

	if (n >= CONST_SUBBUFSIZE)
	{
		ts->status = TSS_STRINGTOOLONG;
	}
	else if (n > 0)
	{
		// copy the string to the other buffer
		strncpy(ts->subbuffer, ts->buffer + start, n);
		ts->subbuffer[n] = '\0';
		
		// treat it as an identifier by default
		ts->token.type = TOK_IDENTIFIER;
		ts->token.value.asString = ts->subbuffer;

		// search for a matching keyword
		for (int kw = 0; kw < KEYWORD_COUNT; kw++)
		{
			if (strcmp(ts->subbuffer, keywords[kw]) == 0)
			{
				ts->token.type = TOK_KEYWORD;
				ts->token.value.asKeyword = kw;
			}
		}
	}
}

static bool TryReadNumber(TokenStream* ts)
{
	int start = ts->i;
	bool isNegative = false;
	bool isInteger = true;

	if (ts->c == SYM_MINUS)
	{
		isNegative = true;
		NextChar(ts);
	}

	while (ts->status == TSS_ACTIVE && (ts->c == SYM_PERIOD || isdigit(ts->c)))
	{
		if (ts->c == SYM_PERIOD)
		{
			if (isInteger) isInteger = false;
			else break;
		}

		NextChar(ts);
	}

	int n = ts->i - start;

	if (isNegative)
	{
		if (n == 1 || (n == 2 && ts->buffer[start + 1] == SYM_PERIOD))
		{
			ts->i = start;
			return false;
		}
	}

	if (n >= CONST_SUBBUFSIZE)
	{
		ts->status = TSS_NUMTOOLONG;
		// return true because we are done trying to read the current token
		return true;
	}

	strncpy(ts->subbuffer, ts->buffer + start, n);
	ts->subbuffer[n] = '\0';

	if (isInteger)
	{
		ts->token.type = TOK_LIT_INT;
		ts->token.value.asInt = atoi(ts->subbuffer);
	}
	else
	{
		ts->token.type = TOK_LIT_FLOAT;
		ts->token.value.asFloat = atof(ts->subbuffer);
	}

	return true;
}

static void ReadStringLiteral(TokenStream* ts)
{
	NextChar(ts);
	int start = ts->i;
	int n = SkipWhile(ts, &IsNotQuote);

	if (ts->status == TSS_ACTIVE && ts->c == SYM_QUOTE)
	{
		ts->token.type = TOK_LIT_STRING;
		ts->token.value.asString = NULL;

		if (n >= 100)
		{
			ts->status = TSS_STRINGTOOLONG;
		}
		else if (n > 0)
		{
			ts->token.value.asString = ts->subbuffer;
			strncpy(ts->subbuffer, ts->buffer + start, n);
			ts->subbuffer[n] = '\0';
		}
	}

	NextChar(ts);
}

static void CheckCompoundSymbol(TokenStream* ts, TokenSymbol s1, TokenSymbol s2, TokenSymbol s3)
{
	NextChar(ts);
	ts->token.type = TOK_SYMBOL;

	if (ts->c == s2)
	{
		NextChar(ts);
		ts->token.value.asSymbol = s3;
	}
	else
	{
		ts->token.value.asSymbol = s1;
	}
}

void TokenStream_Next(TokenStream* ts)
{
	if (ts->keepToken)
	{
		ts->keepToken = false;
		return;
	}

	ts->token.type = TOK_NULL;
	ts->token.value.asString = NULL;

	SkipWhile(ts, &isspace);

	if (ts->status != TSS_ACTIVE)
		return;

	int c = ts->c;

	if (c == SYM_MINUS || c == SYM_PERIOD || isdigit(c))
	{
		if (TryReadNumber(ts))
			return;
	}
	else if (isalpha(c))
	{
		ReadWord(ts);
		return;
	}

	switch ((TokenSymbol)c)
	{
	case SYM_NUM:	// # comment
		// loop to handle consecutive comments without recursing
		while (ts->status == TSS_ACTIVE && ts->c == SYM_NUM)
		{
			SkipWhile(ts, &IsNotNewLine);
			SkipWhile(ts, &isspace);
		}

		// now recurse to get an actual token
		TokenStream_Next(ts);
		break;

	case SYM_QUOTE:
		ReadStringLiteral(ts);
		break;

	case SYM_PERIOD:
	case SYM_COMMA:
	case SYM_SEMICOLON:
	case SYM_LPAREN:
	case SYM_RPAREN:
	case SYM_LBRACE:
	case SYM_RBRACE:
	case SYM_LBRACKET:
	case SYM_RBRACKET:
	case SYM_PLUS:
	// Handled above: case SYM_MINUS:
	case SYM_STAR:
	case SYM_SLASH:
	case SYM_PERCENT:
		ts->token.type = TOK_SYMBOL;
		ts->token.value.asSymbol = c;
		NextChar(ts);
		break;

	case SYM_1AND:
		CheckCompoundSymbol(ts, SYM_1AND, SYM_1AND, SYM_2AND);
		break;
	case SYM_1OR:
		CheckCompoundSymbol(ts, SYM_1OR, SYM_1OR, SYM_2OR);
		break;
	case SYM_EXCL:
		CheckCompoundSymbol(ts, SYM_EXCL, SYM_1EQUAL, SYM_NOTEQ);
		break;
	case SYM_1EQUAL:
		CheckCompoundSymbol(ts, SYM_1EQUAL, SYM_1EQUAL, SYM_2EQUAL);
		break;
	case SYM_LESS:
		CheckCompoundSymbol(ts, SYM_LESS, SYM_1EQUAL, SYM_LESSEQ);
		break;
	case SYM_GREATER:
		CheckCompoundSymbol(ts, SYM_GREATER, SYM_1EQUAL, SYM_GREATEREQ);
		break;
	}
}
