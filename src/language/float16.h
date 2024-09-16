#pragma once

#include <stdint.h>
#include <float.h>

enum
{
	// Number of bits in the mantissa.
	FLOAT16_MANTISSA_BITS = 10,
	// The implied 1 that is not stored in normal numbers.
	FLOAT16_LEADING_1 = 1 << FLOAT16_MANTISSA_BITS,
	// Exponent with all bits set to 1, which corresponds to INF or NaN.
	FLOAT16_MAX_EXP = 0x001F,
	// The number subtracted from the exponent bits to get the actual exponent, which may be negative.
	FLOAT16_EXP_BIAS = 15,
	// The maximum biased exponent of a normal float16:
	// First subtract 1 from the value with all bits set, because that value is reserved for INF or NaN.
	// Then subtract the bias.
	FLOAT16_MAX_NORMAL_EXP = FLOAT16_MAX_EXP - 1 - FLOAT16_EXP_BIAS,
	// The minimum biased exponent of a normal float16:
	// This is simply the minimum unbiased non-subnormal exponent (which is 1) minus the bias.
	FLOAT16_MIN_NORMAL_EXP = 1 - FLOAT16_EXP_BIAS,
	// The minimum biased exponent of a subnormal float16:
	// This corresponds to a number with zero in the exp and 1 in the mantissa,
	// which is 1 * 2^(-10) * 2^(-14) = 2^(-24)
	FLOAT16_MIN_SUBNORMAL_EXP = FLOAT16_MIN_NORMAL_EXP - FLOAT16_MANTISSA_BITS,

	FLOAT_MANTISSA_BITS = 23,
	FLOAT_MAX_EXP = 0x00FF,
	FLOAT_EXP_BIAS = 127,

	DOUBLE_MANTISSA_BITS = 52,
	DOUBLE_MAX_EXP = 0x07FF,
	DOUBLE_EXP_BIAS = 1023,
};

typedef union
{
	uint16_t bits;

	struct
	{
		uint16_t mantissa : FLOAT16_MANTISSA_BITS;
		uint16_t exponent : 5;
		uint16_t sign : 1;
	};
} float16;

typedef union
{
	float native;
	uint32_t bits;

	struct
	{
		uint32_t mantissa : FLOAT_MANTISSA_BITS;
		uint32_t exponent : 8;
		uint32_t sign : 1;
	};
} float32;

typedef union
{
	double native;
	uint64_t bits;

	struct
	{
		uint64_t mantissa : DOUBLE_MANTISSA_BITS;
		uint64_t exponent : 11;
		uint64_t sign : 1;
	};
} float64;

float16 Float16_FromFloat(float f);
float16 Float16_FromDouble(double d);
float16 Float16_FromInt(int i);

float Float16_ToFloat(float16 f);
double Float16_ToDouble(float16 f);
int Float16_ToInt(float16 f);

float16 Float16_Add(float16 left, float16 right);
float16 Float16_Sub(float16 left, float16 right);
float16 Float16_Mul(float16 left, float16 right);
float16 Float16_Div(float16 left, float16 right);
float16 Float16_Sqrt(float16 f);
float16 Float16_Pow(float16 x, float16 y);

bool Float16_Equal(float16 left, float16 right);
bool Float16_Greater(float16 left, float16 right);
bool Float16_GreaterEqual(float16 left, float16 right);
bool Float16_Less(float16 left, float16 right);
bool Float16_LessEqual(float16 left, float16 right);

int Float16_Classify(float16 f);
bool Float16_IsFinite(float16 f);
bool Float16_IsNormal(float16 f);
bool Float16_IsInf(float16 f);
bool Float16_IsNaN(float16 f);
bool Float16_IsNegative(float16 f);
