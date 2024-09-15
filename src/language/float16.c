#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include "float16.h"

static inline float16 FromLargerType(int sign, int exponent, uint64_t mantissa, int mantissaBits, int maxExp, int bias)
{
	float16 result = { .bits = 0 };
	result.sign = sign;

	if (exponent == 0)
	{
		// zero or subnormal: round to zero
		// any subnormal value of a larger type is too small for a float16
		// (assuming its minimum biased exponent is less than that of float16, which is true of native floats and doubles)
		result.mantissa = 0;
		result.exponent = 0;
	}
	else if (exponent == maxExp)
	{
		// INF or NaN
		// no distinction between quiet and signaling NaN
		result.mantissa = mantissa == 0 ? 0 : 1;
		result.exponent = FLOAT16_MAX_EXP;
	}
	else
	{
		// number of additional mantissa bits in the other type vs float16
		const int mdiff = mantissaBits - FLOAT16_MANTISSA_BITS;

		// convert stored bits to the actual biased exponent
		int biasedExp = exponent - bias;

		if (biasedExp < FLOAT16_MIN_SUBNORMAL_EXP)
		{
			// low exponent: round to zero
			result.mantissa = 0;
			result.exponent = 0;
		}
		else if (biasedExp < FLOAT16_MIN_NORMAL_EXP)
		{
			// subnormal
			result.exponent = 0;

			// number of bits to shift right due to this being a subnormal number
			const int expDiff = FLOAT16_MIN_NORMAL_EXP - biasedExp;
			// right-shift in the leading 1 that is implied but not stored in normal numbers
			const int leading1 = FLOAT16_LEADING_1 >> expDiff;
			// right-shift the mantissa the same amount, plus the number of extra bits that won't fit
			const int newFrac = mantissa >> (expDiff + mdiff);

			result.mantissa = leading1 + newFrac;
		}
		else if (biasedExp > 15)
		{
			// high exponent: round to infinity
			result.mantissa = 0;
			result.exponent = FLOAT16_MAX_EXP;
		}
		else
		{
			// shift right to discard less significant mantissa bits that won't fit
			result.mantissa = mantissa >> mdiff;
			// the exp has been confirmed to be in the normal range, so now un-bias it for storage
			result.exponent = biasedExp + FLOAT16_EXP_BIAS;
		}
	}

	return result;
}

float16 Float16_FromFloat(float f)
{
	float32 other = { .native = f };
	return FromLargerType(other.sign, other.exponent, other.mantissa,
		FLOAT_MANTISSA_BITS, FLOAT_MAX_EXP, FLOAT_EXP_BIAS);
}

float16 Float16_FromDouble(double d)
{
	float64 other = { .native = d };
	return FromLargerType(other.sign, other.exponent, other.mantissa,
		DOUBLE_MANTISSA_BITS, DOUBLE_MAX_EXP, DOUBLE_EXP_BIAS);
}

float16 Float16_FromInt(int i)
{
	double d = i;
	return Float16_FromDouble(d);
}

float Float16_ToFloat(float16 f)
{
	float32 result = { .bits = 0 };
	result.sign = f.sign;

	if (f.exponent == 0)
	{
		if (f.mantissa == 0)
		{
			result.mantissa = 0;
			result.exponent = 0;
		}
		else // subnormal
		{
			// the number with no exponent applied
			float val = (float)(f.sign == 0 ? f.mantissa : -f.mantissa);
			// this will divide the value, so shifting left is like multiplying val by 2 to a negative power
			const int divisor = FLOAT16_LEADING_1 << (-FLOAT16_MIN_NORMAL_EXP);
			// divide to apply the exponent
			result.native = val / (float)divisor;
		}
	}
	else
	{
		const int mdiff = FLOAT_MANTISSA_BITS - FLOAT16_MANTISSA_BITS;
		result.mantissa = ((uint32_t)f.mantissa) << mdiff;
		result.exponent = f.exponent == FLOAT16_MAX_EXP
			? FLOAT_MAX_EXP
			: ((uint32_t)f.exponent) - FLOAT16_EXP_BIAS + FLOAT_EXP_BIAS;
	}

	return result.native;
}

double Float16_ToDouble(float16 f)
{
	float64 result = { .bits = 0 };
	result.sign = f.sign;

	if (f.exponent == 0)
	{
		if (f.mantissa == 0)
		{
			result.mantissa = 0;
			result.exponent = 0;
		}
		else // subnormal
		{
			// the number with no exponent applied
			double val = (double)(f.sign == 0 ? f.mantissa : -f.mantissa);
			// this will divide the value, so shifting left is like multiplying val by 2 to a negative power
			const int divisor = FLOAT16_LEADING_1 << (-FLOAT16_MIN_NORMAL_EXP);
			// divide to apply the exponent
			result.native = val / (double)divisor;
		}
	}
	else
	{
		const int mdiff = DOUBLE_MANTISSA_BITS - FLOAT16_MANTISSA_BITS;
		result.mantissa = ((uint64_t)f.mantissa) << mdiff;
		result.exponent = f.exponent == FLOAT16_MAX_EXP
			? DOUBLE_MAX_EXP
			: ((uint64_t)f.exponent) - FLOAT16_EXP_BIAS + DOUBLE_EXP_BIAS;
	}

	return result.native;
}

int Float16_ToInt(float16 f)
{
	double d = Float16_ToDouble(f);
	int i = d;
	return i;
}

float16 Float16_Add(float16 left, float16 right)
{
	double dLeft = Float16_ToDouble(left);
	double dRight = Float16_ToDouble(right);
	return Float16_FromDouble(dLeft + dRight);
}

float16 Float16_Sub(float16 left, float16 right)
{
	double dLeft = Float16_ToDouble(left);
	double dRight = Float16_ToDouble(right);
	return Float16_FromDouble(dLeft - dRight);
}

float16 Float16_Mul(float16 left, float16 right)
{
	double dLeft = Float16_ToDouble(left);
	double dRight = Float16_ToDouble(right);
	return Float16_FromDouble(dLeft * dRight);
}

float16 Float16_Div(float16 left, float16 right)
{
	double dLeft = Float16_ToDouble(left);
	double dRight = Float16_ToDouble(right);
	return Float16_FromDouble(dLeft / dRight);
}

float16 Float16_Sqrt(float16 f)
{
	double d = Float16_ToDouble(f);
	d = sqrt(d);
	return Float16_FromDouble(d);
}

float16 Float16_Pow(float16 x, float16 y)
{
	double dx = Float16_ToDouble(x);
	double dy = Float16_ToDouble(x);
	dx = pow(dx, dy);
	return Float16_FromDouble(dx);
}

static int ClassifyForOrdering(float16 f)
{
	bool neg = f.sign != 0;

	if (f.exponent == 0)
		return f.mantissa == 0
			? 0	// zero
			: (neg ? -1 : 1); // subnormal

	if (f.exponent == FLOAT16_MAX_EXP)
		return f.mantissa == 0
			? (neg ? -3 : 3) // inf
			: -100; // nan

	return neg ? -2 : 2;
}

bool Float16_Equal(float16 left, float16 right)
{
	int a = ClassifyForOrdering(left);
	int b = ClassifyForOrdering(right);

	if (a < b || a > b || a < -3 || b < -3) return false; // rule out different category or NaN
	if (a == 0 || a == -3 || a == 3) return true; // rule out zero and inf

	if (a == -2 || a == 2)
	{
		// normal
		if (left.exponent != right.exponent) return false;
	}

	// subnormal, or normal with equal exponents
	return left.mantissa == right.mantissa;
}

bool Float16_Greater(float16 left, float16 right)
{
	int a = ClassifyForOrdering(left);
	int b = ClassifyForOrdering(right);

	if (a < b || a < -3 || b < -3) return false; // rule out lesser category or NaN
	if (a > b) return true;	// rule out greater category
	if (a == 0 || a == -3 || a == 3) return false; // rule out zero and inf

	if (a == -2 || a == 2)
	{
		// normal
		if (left.exponent > right.exponent) return true;
		if (left.exponent < right.exponent) return false;
	}

	// subnormal, or normal with equal exponents
	return left.mantissa > right.mantissa;
}

bool Float16_GreaterEqual(float16 left, float16 right)
{
	int a = ClassifyForOrdering(left);
	int b = ClassifyForOrdering(right);

	if (a < b || a < -3 || b < -3) return false; // rule out lesser category or NaN
	if (a > b) return true;	// rule out greater category
	if (a == 0 || a == -3 || a == 3) return true; // rule out zero and inf

	if (a == -2 || a == 2)
	{
		// normal
		if (left.exponent > right.exponent) return true;
		if (left.exponent < right.exponent) return false;
	}

	// subnormal, or normal with equal exponents
	return left.mantissa >= right.mantissa;
}

bool Float16_Less(float16 left, float16 right)
{
	int a = ClassifyForOrdering(left);
	int b = ClassifyForOrdering(right);

	if (a > b || a < -3 || b < -3) return false; // rule out greater category or NaN
	if (a < b) return true;	// rule out lesser category
	if (a == 0 || a == -3 || a == 3) return false; // rule out zero and inf

	if (a == -2 || a == 2)
	{
		// normal
		if (left.exponent < right.exponent) return true;
		if (left.exponent > right.exponent) return false;
	}

	// subnormal, or normal with equal exponents
	return left.mantissa < right.mantissa;
}

bool Float16_LessEqual(float16 left, float16 right)
{
	int a = ClassifyForOrdering(left);
	int b = ClassifyForOrdering(right);

	if (a > b || a < -3 || b < -3) return false; // rule out greater category or NaN
	if (a < b) return true;	// rule out lesser category
	if (a == 0 || a == -3 || a == 3) return true; // rule out zero and inf

	if (a == -2 || a == 2)
	{
		// normal
		if (left.exponent < right.exponent) return true;
		if (left.exponent > right.exponent) return false;
	}

	// subnormal, or normal with equal exponents
	return left.mantissa <= right.mantissa;
}

int Float16_Classify(float16 f)
{
	if (f.exponent == 0)
		return f.mantissa == 0 ? FP_ZERO : FP_SUBNORMAL;

	if (f.exponent == FLOAT16_MAX_EXP)
		return f.mantissa == 0 ? FP_INFINITE : FP_NAN;

	return FP_NORMAL;
}

bool Float16_IsFinite(float16 f)
{
	return f.exponent < FLOAT16_MAX_EXP;
}

bool Float16_IsNormal(float16 f)
{
	return 0 < f.exponent && f.exponent < FLOAT16_MAX_EXP;
}

bool Float16_IsInf(float16 f)
{
	return f.exponent == FLOAT16_MAX_EXP && f.mantissa == 0;
}

bool Float16_IsNaN(float16 f)
{
	return f.exponent == FLOAT16_MAX_EXP && f.mantissa != 0;
}

bool Float16_IsNegative(float16 f)
{
	return f.sign != 0;
}
