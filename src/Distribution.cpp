#include "Distribution.h"

static int CountSignificantBits(int value)
{
	int sign = value < 0 ? -1 : 0;
	for (int i = 0; i < 31; ++i) if (value >> i == sign) return i + 1;
	return 32;
}

IntegerDistribution::IntegerDistribution()
{ 
	for (int i = 0; i < 32; ++i) counts[i] = 1; 
}

void IntegerDistribution::EncodeAndTally(arith::Encoder & encoder, int value)
{
	int bits = CountSignificantBits(value);

	arith::code_t a = 0;
	for (int i = 0; i < bits; ++i) a += counts[i];
	arith::code_t b = a + counts[bits], d = b;
	for (int i = bits + 1; i < 32; ++i) d += counts[i];
	encoder.Encode(a, b, d);

	++counts[bits];

	arith::code_t mask = -1U >> (32 - bits);
	encoder.Encode(value & mask, (value & mask) + 1, mask + 1);
}

int IntegerDistribution::DecodeAndTally(arith::Decoder & decoder)
{
	arith::code_t d = 0;
	for (int i = 0; i < 32; ++i) d += counts[i];
	arith::code_t x = decoder.Decode(d);

	arith::code_t a = 0;
	int bits = 0;
	for (int i = 0; i < 32; ++i)
	{
		arith::code_t b = a + counts[i];
		if (b > x)
		{
			decoder.Confirm(a, b);
			bits = i;
			break;
		}
		a = b;
	}

	++counts[bits];

	int val = decoder.Decode(1 << bits);
	decoder.Confirm(val, val + 1);
	return (val << (32 - bits)) >> (32 - bits);
}
