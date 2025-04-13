#pragma once

#include <stdbool.h>

typedef enum
{
	DAT_INVALID,
	DAT_VOID,
	DAT_INT,
	DAT_FLOAT,
	DAT_BOOL,
	DAT_STRING,
} DataType;

typedef enum
{
	OP_ACCESS,
	OP_DEREFERENCE,
	OP_NEGATE,
	OP_NOT,
	OP_MULTIPLY,
	OP_DIVIDE,
	OP_MODULO,
	OP_ADD,
	OP_SUBTRACT,
	OP_JOIN,
	OP_GREATERTHAN,
	OP_GREATEREQUAL,
	OP_LESSTHAN,
	OP_LESSEQUAL,
	OP_EQUAL,
	OP_NOTEQUAL,
	OP_LOG_AND,
	OP_LOG_OR,
	OP_BW_AND,
	OP_BW_OR,
} OperationType;

typedef enum
{
	EX_VALUE,
	EX_READ,
	EX_OPERATION,
	EX_CALL,
} ExpressionType;

typedef enum
{
	ST_ASSIGN,
	ST_CONDITION,
	ST_LOOP,
	ST_CALL,
	ST_RETURN,
	ST_INSTR,
} StatementType;

struct Function;
typedef struct Function Function;

typedef union
{
	int asInt;
	double asFloat;
	bool asBool;
	char* asString;
} ValUnion;

union ExpressionUnion;
typedef union ExpressionUnion ExpressionUnion;

union StatementUnion;
typedef union StatementUnion StatementUnion;

struct Expression;
typedef struct Expression Expression;

struct Statement;
typedef struct Statement Statement;

typedef struct
{
	char* name;
	DataType type;
} Parameter;

struct Function
{
	char* name;
	Parameter* params;
	Parameter* locals;
	Statement* statements;
	int numParams;
	int numLocals;
	int numStatements;
	DataType rtype;
	bool isMain;
};

typedef struct
{
	Expression* condition;
	Statement* statements;
	int numStatements;
} ST_Condition;

typedef struct
{
	char* name;
	Expression* arguments;
	int numArguments;
} ST_Call;

typedef Expression* ST_Return;

typedef struct
{
	DataType type;
	ValUnion value;
} EX_Value;

typedef char* EX_Read;

typedef struct
{
	OperationType type;
	bool isBinary;
	Expression* a;
	Expression* b;
} EX_Operation;

typedef struct
{
	char* left;
	Expression* right;
	bool derefLhs;
} ST_Assign;

union ExpressionUnion
{
	EX_Value asValue;
	EX_Read asRead;
	EX_Operation asOperation;
	ST_Call asCall;
};

struct Expression
{
	ExpressionType type;
	ExpressionUnion content;
};

union StatementUnion
{
	ST_Assign asAssign;
	ST_Condition asCondition;
	ST_Call asCall;
	ST_Return asReturn;
	int asInstr;
};

struct Statement
{
	StatementType type;
	StatementUnion content;
};

// Root node representing a source code file.
typedef struct
{
	Function *functions;
	char **imports;
	int numFunctions;
	int numImports;
} SyntaxTree;

char* DeepCopyStr(char* str);
SyntaxTree* Parser_ParseFile(char* filePath);
void Parser_Destroy(SyntaxTree* ast);
