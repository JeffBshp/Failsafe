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
	KW_IF,
	KW_ELSE,
	KW_WHILE,
	KW_RETURN,
	KW_MAIN,
	KW_VOID,
	KW_NULL,
	KW_INT,
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
	SYM_1AND = '&',
	SYM_2AND = '&&',
	SYM_1OR = '|',
	SYM_2OR = '||',
	SYM_EXCL = '!',
	SYM_1EQUAL = '=',
	SYM_2EQUAL = '==',
	SYM_NOTEQ = '!=',
	SYM_LESS = '<',
	SYM_LESSEQ = '<=',
	SYM_GREATER = '>',
	SYM_GREATEREQ = '>=',
} TokenSymbol;

typedef enum
{
	TSS_ACTIVE,
	TSS_ENDOFFILE,
	TSS_ENDOFBUFFER,
	TSS_STRINGTOOLONG,
	TSS_NUMTOOLONG,
	TSS_FILEERROR,
	TSS_INDEXERROR,

	TSS_INVALIDTOKEN,
	TSS_INVALIDDATATYPE,
	TSS_TOOMANYPARAMS,
	TSS_TOOMANYSTATEMENTS,
} TokenStreamStatus;

enum
{
	CONST_BUFFERSIZE = 10000,
	CONST_SUBBUFSIZE = 100,
	CONST_MAXPARAMS = 10,
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
	char* buffer;		// characters read so far
	char* subbuffer;	// buffer for the current token
	int i;				// index of the current char (< n)
	int n;				// number of chars in the buffer
	int c;				// character at buffer[i]
	Token token;		// most recent token
	TokenStreamStatus status;
	// Keep the current token instead of reading the next one.
	// Resets after each use.
	bool keepToken;
} TokenStream;

void TokenStream_Open(TokenStream* ts, char* filePath);
void TokenStream_Next(TokenStream* ts);
void TokenStream_Close(TokenStream* ts);
