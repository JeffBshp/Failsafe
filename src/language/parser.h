#pragma once

typedef enum
{
	DAT_INVALID,
	DAT_VOID,
	DAT_INT,
	DAT_FLOAT,
	DAT_BOOL,
	DAT_STRING,
	DAT_FUNCTION,
} DataType;

typedef enum
{
	OP_ACCESS,
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
	OP_AND,
	OP_OR,
} OperationType;

typedef enum
{
	EX_VALUE,
	EX_READ,
	EX_OPERATION,
	EX_CALL,
	EX_FUNCTION,
} ExpressionType;

typedef enum
{
	ST_ASSIGN,
	ST_CONDITION,
	ST_LOOP,
	ST_CALL,
	ST_RETURN,
} StatementType;

struct Function;
typedef struct Function Function;

typedef union
{
	int asInt;
	double asFloat;
	bool asBool;
	char* asString;
	Function* asFunction;
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
	ValUnion value;
} Variable;

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
	bool isExternal;
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
} ST_Assign;

struct Context;
typedef struct Context Context;
struct Context
{
	Context* parent;
	Variable* vars;
	int numVars;
	EX_Value rVal;
	bool ret;
};

union ExpressionUnion
{
	EX_Value asValue;
	EX_Read asRead;
	EX_Operation asOperation;
	ST_Call asCall;
	Function* asFunction;
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
};

struct Statement
{
	StatementType type;
	StatementUnion content;
};

Function* Parser_ParseFile(char* filePath);
