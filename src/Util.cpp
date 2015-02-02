#include "Util.h"

#include <cassert>

void EncodeUniform(arith::Encoder & encoder, arith::code_t x, arith::code_t d)
{
	encoder.Encode(x, x + 1, d);
}

arith::code_t DecodeUniform(arith::Decoder & decoder, arith::code_t d)
{
	auto x = decoder.Decode(d);
	decoder.Confirm(x, x + 1);
	return x;
}

SymbolDistribution::SymbolDistribution(size_t symbols) : counts(symbols,1)
{

}

void SymbolDistribution::EncodeAndTally(arith::Encoder & encoder, size_t symbol)
{
    assert(symbol < counts.size());

    arith::code_t a = 0;
	for (size_t i = 0; i < symbol; ++i) a += counts[i];
	arith::code_t b = a + counts[symbol], d = b;
	for (size_t i = symbol + 1; i < counts.size(); ++i) d += counts[i];
	encoder.Encode(a, b, d);

	++counts[symbol];
}

size_t SymbolDistribution::DecodeAndTally(arith::Decoder & decoder)
{
    arith::code_t d = 0;
	for (int i = 0; i < counts.size(); ++i) d += counts[i];
	arith::code_t x = decoder.Decode(d);

	arith::code_t a = 0;
	for (int i = 0; i < counts.size(); ++i)
	{
		arith::code_t b = a + counts[i];
		if (b > x)
		{
			decoder.Confirm(a, b);
			++counts[i];
            return i;
		}
		a = b;
	}

	assert(false);
    return 0;
}

static int CountSignificantBits(int value)
{
	int sign = value < 0 ? -1 : 0;
	for (int i = 0; i < 31; ++i) if (value >> i == sign) return i + 1;
	return 32;
}

IntegerDistribution::IntegerDistribution() : dist(32)
{ 

}

void IntegerDistribution::EncodeAndTally(arith::Encoder & encoder, int value)
{
	int bits = CountSignificantBits(value);
    dist.EncodeAndTally(encoder, bits);
	EncodeUniform(encoder, value & (-1U >> (32 - bits)), 1 << bits);
}

int IntegerDistribution::DecodeAndTally(arith::Decoder & decoder)
{
    int bits = dist.DecodeAndTally(decoder);
	return static_cast<int>(DecodeUniform(decoder, 1 << bits) << (32 - bits)) >> (32 - bits);
}

RangeAllocator::RangeAllocator() : totalCapacity()
{

}

size_t RangeAllocator::Allocate(size_t amount)
{
    for(auto it = freeList.rbegin(); it != freeList.rend(); ++it)
    {
        if(it->second == amount)
        {
            auto offset = it->first;
            freeList.erase(freeList.begin() + (&*it - freeList.data()));
            return offset;
        }
    }

    auto offset = totalCapacity;
    totalCapacity += amount;
    return offset;
}

void RangeAllocator::Free(size_t offset, size_t amount)
{
    freeList.push_back({offset,amount});
}