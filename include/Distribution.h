#ifndef DISTRIBUTION_H
#define DISTRIBUTION_H

#include "ArithCode.h"

void EncodeUniform(arith::Encoder & encoder, arith::code_t x, arith::code_t d);
arith::code_t DecodeUniform(arith::Decoder & decoder, arith::code_t d);

class IntegerDistribution
{
	arith::code_t counts[32];
public:
	IntegerDistribution();

	void EncodeAndTally(arith::Encoder & encoder, int value);
	int DecodeAndTally(arith::Decoder & decoder);
};


#endif