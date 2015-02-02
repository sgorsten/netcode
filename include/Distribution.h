#ifndef DISTRIBUTION_H
#define DISTRIBUTION_H

#include "ArithCode.h"

void EncodeUniform(arith::Encoder & encoder, arith::code_t x, arith::code_t d);
arith::code_t DecodeUniform(arith::Decoder & decoder, arith::code_t d);

class SymbolDistribution
{
    std::vector<arith::code_t> counts;
public:
    SymbolDistribution() {}
    SymbolDistribution(size_t symbols);

    void EncodeAndTally(arith::Encoder & encoder, size_t symbol);
	size_t DecodeAndTally(arith::Decoder & decoder);
};

class IntegerDistribution
{
    SymbolDistribution dist;
public:
	IntegerDistribution();

	void EncodeAndTally(arith::Encoder & encoder, int value);
	int DecodeAndTally(arith::Decoder & decoder);
};


#endif