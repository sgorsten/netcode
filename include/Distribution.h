#ifndef DISTRIBUTION_H
#define DISTRIBUTION_H

#include "ArithCode.h"

class IntegerDistribution
{
	arith::code_t counts[32];
public:
	IntegerDistribution();

	void EncodeAndTally(arith::Encoder & encoder, int value);
	int DecodeAndTally(arith::Decoder & decoder);
};


#endif