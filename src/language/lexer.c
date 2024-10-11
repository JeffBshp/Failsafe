#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lexer.h"

static void NextChar(Lexer* lex)
{
	if (lex->status != LEX_ACTIVE)
		return;

	lex->i++;

	if (lex->i < 0 || lex->i > lex->n || lex->n < 0)
	{
		lex->status = LEX_INDEXERROR;
	}
	else if (lex->i < lex->n)
	{
		lex->c = lex->buffer[lex->i];
	}
	else // i == n
	{
		if (lex->n >= CONST_BUFFERSIZE)
		{
			lex->i--;
			lex->status = LEX_ENDOFBUFFER;
		}
		else
		{
			lex->n++;
			lex->c = fgetc(lex->file);
			lex->buffer[lex->i] = (char)lex->c;

			if (lex->c == EOF)
				lex->status = LEX_ENDOFFILE;
		}
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

static int SkipWhile(Lexer* lex, int (*test)(int))
{
	int start = lex->i;

	while (lex->status == LEX_ACTIVE && 0 != test(lex->c))
		NextChar(lex);

	// return number of chars skipped
	// i now points to the first char that failed
	return lex->i - start;
}

static void ReadWord(Lexer* lex)
{
	int start = lex->i;
	int n = 1;

	static const char* keywords[] =
	{
		"if",
		"else",
		"while",
		"return",
		"main",
		"void",
		"null",
		"int",
		"float",
		"bool",
		"string",

		"true",
		"false",
	};

	NextChar(lex);
	n += SkipWhile(lex, &isalnum);

	if (n >= CONST_SUBBUFSIZE)
	{
		lex->status = LEX_STRINGTOOLONG;
	}
	else if (n > 0)
	{
		// copy the string to the other buffer
		strncpy(lex->subbuffer, lex->buffer + start, n);
		lex->subbuffer[n] = '\0';
		
		// treat it as an identifier by default
		lex->token.type = TOK_IDENTIFIER;
		lex->token.value.asString = lex->subbuffer;

		// search for a matching keyword
		for (int kw = 0; kw < KEYWORD_COUNT; kw++)
		{
			if (strcmp(lex->subbuffer, keywords[kw]) == 0)
			{
				switch (kw)
				{
				case KW_TRUE:
					lex->token.type = TOK_LIT_BOOL;
					lex->token.value.asBool = true;
					break;
				case KW_FALSE:
					lex->token.type = TOK_LIT_BOOL;
					lex->token.value.asBool = false;
					break;
				default:
					lex->token.type = TOK_KEYWORD;
					lex->token.value.asKeyword = kw;
					break;
				}
			}
		}
	}
}

static bool TryReadNumber(Lexer* lex)
{
	int start = lex->i;
	bool isNegative = false;
	bool isInteger = true;

	if (lex->c == SYM_MINUS)
	{
		isNegative = true;
		NextChar(lex);
	}

	while (lex->status == LEX_ACTIVE && (lex->c == SYM_PERIOD || isdigit(lex->c)))
	{
		if (lex->c == SYM_PERIOD)
		{
			if (isInteger) isInteger = false;
			else break;
		}

		NextChar(lex);
	}

	int n = lex->i - start;

	if (isNegative)
	{
		if (n == 1 || (n == 2 && lex->buffer[start + 1] == SYM_PERIOD))
		{
			lex->i = start;
			lex->c = lex->buffer[lex->i];
			return false;
		}
	}

	if (n >= CONST_SUBBUFSIZE)
	{
		lex->status = LEX_NUMTOOLONG;
		// return true because we are done trying to read the current token
		return true;
	}

	strncpy(lex->subbuffer, lex->buffer + start, n);
	lex->subbuffer[n] = '\0';

	if (isInteger)
	{
		lex->token.type = TOK_LIT_INT;
		lex->token.value.asInt = atoi(lex->subbuffer);
	}
	else
	{
		lex->token.type = TOK_LIT_FLOAT;
		lex->token.value.asFloat = atof(lex->subbuffer);
	}

	return true;
}

static void ReadStringLiteral(Lexer* lex)
{
	NextChar(lex);
	int start = lex->i;
	int n = SkipWhile(lex, &IsNotQuote);

	if (lex->status == LEX_ACTIVE && lex->c == SYM_QUOTE)
	{
		lex->token.type = TOK_LIT_STRING;
		lex->token.value.asString = NULL;

		if (n >= 100)
		{
			lex->status = LEX_STRINGTOOLONG;
		}
		else if (n > 0)
		{
			lex->token.value.asString = lex->subbuffer;
			strncpy(lex->subbuffer, lex->buffer + start, n);
			lex->subbuffer[n] = '\0';
		}
	}

	NextChar(lex);
}

static void CheckCompoundSymbol(Lexer* lex, TokenSymbol s1, TokenSymbol s2, TokenSymbol s3)
{
	NextChar(lex);
	lex->token.type = TOK_SYMBOL;

	if (lex->c == s2)
	{
		NextChar(lex);
		lex->token.value.asSymbol = s3;
	}
	else
	{
		lex->token.value.asSymbol = s1;
	}
}

Lexer* Lexer_OpenFile(char* filePath)
{
	Lexer* lex = calloc(1, sizeof(Lexer));
	lex->file = fopen(filePath, "r");

	if (lex->file == NULL)
	{
		lex->status = LEX_FILEERROR;
	}
	else
	{
		// initialize struct
		lex->buffer = calloc(CONST_BUFFERSIZE, sizeof(char));
		lex->subbuffer = calloc(CONST_SUBBUFSIZE, sizeof(char));
		lex->i = -1;
		lex->n = 0;
		lex->token.type = TOK_NULL;
		lex->token.value.asString = NULL;
		lex->status = LEX_ACTIVE;
		lex->keepToken = false;

		// read first char to increment i to 0 and establish a value for c
		NextChar(lex);
	}

	return lex;
}

void Lexer_NextToken(Lexer* lex)
{
	if (lex->keepToken)
	{
		lex->keepToken = false;
		return;
	}

	lex->token.type = TOK_NULL;
	lex->token.value.asString = NULL;

	SkipWhile(lex, &isspace);

	if (lex->status != LEX_ACTIVE)
		return;

	int c = lex->c;

	if (c == SYM_MINUS || c == SYM_PERIOD || isdigit(c))
	{
		if (TryReadNumber(lex)) return;
	}
	else if (isalpha(c))
	{
		ReadWord(lex);
		return;
	}

	switch ((TokenSymbol)c)
	{
	case SYM_NUM:	// # comment
		// loop to handle consecutive comments without recursing
		while (lex->status == LEX_ACTIVE && lex->c == SYM_NUM)
		{
			SkipWhile(lex, &IsNotNewLine);
			SkipWhile(lex, &isspace);
		}

		// now recurse to get an actual token
		Lexer_NextToken(lex);
		break;

	case SYM_QUOTE:
		ReadStringLiteral(lex);
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
	case SYM_MINUS:
	case SYM_STAR:
	case SYM_SLASH:
	case SYM_PERCENT:
		lex->token.type = TOK_SYMBOL;
		lex->token.value.asSymbol = c;
		NextChar(lex);
		break;

	case SYM_1AND:
		CheckCompoundSymbol(lex, SYM_1AND, SYM_1AND, SYM_2AND);
		break;
	case SYM_1OR:
		CheckCompoundSymbol(lex, SYM_1OR, SYM_1OR, SYM_2OR);
		break;
	case SYM_EXCL:
		CheckCompoundSymbol(lex, SYM_EXCL, SYM_1EQUAL, SYM_NOTEQ);
		break;
	case SYM_1EQUAL:
		CheckCompoundSymbol(lex, SYM_1EQUAL, SYM_1EQUAL, SYM_2EQUAL);
		break;
	case SYM_LESS:
		CheckCompoundSymbol(lex, SYM_LESS, SYM_1EQUAL, SYM_LESSEQ);
		break;
	case SYM_GREATER:
		CheckCompoundSymbol(lex, SYM_GREATER, SYM_1EQUAL, SYM_GREATEREQ);
		break;
	}
}

void Lexer_Destroy(Lexer* lex)
{
	if (lex != NULL)
	{
		fclose(lex->file);
		free(lex->buffer);
		free(lex->subbuffer);
		free(lex);
	}
}
