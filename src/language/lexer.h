#pragma once

#include <stdio.h>
#include <stdbool.h>

typedef enum
{
	TOK_NULL,
	TOK_KEYWORD,
	TOK_SYMBOL,
	TOK_IDENTIFIER,
	TOK_LIT_INT,
	TOK_LIT_FLOAT,
	TOK_LIT_BOOL,
	TOK_LIT_STRING,
} TokenType;

typedef enum
{
	KW_IMPORT,
	KW_INSTR,
	KW_GETREG,
	KW_SETREG,
	KW_IF,
	KW_ELSE,
	KW_WHILE,
	KW_BREAK,
	KW_RETURN,
	KW_MAIN,
	KW_VOID,
	KW_NULL,
	KW_INT,
	KW_UINT,
	KW_FLOAT,
	KW_BOOL,
	KW_STRING,

	// These are keywords, but they get converted directly into TOK_LIT_BOOL:
	KW_TRUE,
	KW_FALSE,

	KEYWORD_COUNT
} TokenKeyword;

typedef enum
{
	SYM_NUM = '#',
	SYM_QUOTE = '"',
	SYM_PERIOD = '.',
	SYM_COMMA = ',',
	SYM_SEMICOLON = ';',
	SYM_LPAREN = '(',
	SYM_RPAREN = ')',
	SYM_LBRACE = '{',
	SYM_RBRACE = '}',
	SYM_LBRACKET = '[',
	SYM_RBRACKET = ']',
	SYM_PLUS = '+',
	SYM_MINUS = '-',
	SYM_STAR = '*',
	SYM_SLASH = '/',
	SYM_PERCENT = '%',
	SYM_AT = '@',
	SYM_1AND = '&',
	SYM_2AND = '&' | ('&' << 8),
	SYM_1OR = '|',
	SYM_2OR = '|' | ('|' << 8),
	SYM_EXCL = '!',
	SYM_1EQUAL = '=',
	SYM_2EQUAL = '=' | ('=' << 8),
	SYM_NOTEQ = '!' | ('=' << 8),
	SYM_LESS = '<',
	SYM_LESSEQ = '<' | ('=' << 8),
	SYM_GREATER = '>',
	SYM_GREATEREQ = '>' | ('=' << 8),
} TokenSymbol;

typedef enum
{
	LEX_ACTIVE,
	LEX_ENDOFFILE,
	LEX_ENDOFBUFFER,
	LEX_STRINGTOOLONG,
	LEX_NUMTOOLONG,
	LEX_FILEERROR,
	LEX_INDEXERROR,
	LEX_INVALIDTOKEN,
	LEX_INVALIDDATATYPE,
	LEX_TOOMANYPARAMS,
	LEX_TOOMANYSTATEMENTS,
} LexStatus;

enum
{
	CONST_BUFFERSIZE = 10000,
	CONST_SUBBUFSIZE = 100,
	CONST_MAXPARAMS = 20,
	CONST_MAXSTATEMENTS = 100,
};

typedef union
{
	TokenKeyword asKeyword;
	TokenSymbol asSymbol;
	int asInt;
	double asFloat;
	bool asBool;
	char* asString;
} TokenValue;

typedef struct
{
	TokenType type;
	TokenValue value;
} Token;

typedef struct
{
	FILE* file;			// the file to read
	char buffer[CONST_BUFFERSIZE];		// characters read so far
	char subbuffer[CONST_SUBBUFSIZE];	// buffer for the current token
	int i;				// index of the current char (< n)
	int n;				// number of chars in the buffer
	int c;				// character at buffer[i]
	Token token;		// most recent token
	LexStatus status;
	// Keep the current token instead of reading the next one.
	// Resets after each use.
	bool keepToken;
} Lexer;

Lexer* Lexer_OpenFile(char* filePath);
void Lexer_NextToken(Lexer* lex);
void Lexer_Destroy(Lexer* lex);
