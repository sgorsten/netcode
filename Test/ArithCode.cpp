#include "ArithCode.h"

#include <cassert>

using namespace arith;

enum
{
	NUM_BITS = sizeof(code_t) * 8,
	BOUND0 = 0,
	BOUND1 = 1 << (NUM_BITS - 3),
	BOUND2 = 1 << (NUM_BITS - 2),
	BOUND3 = BOUND1 | BOUND2,
	BOUND4 = 1 << (NUM_BITS - 1),	
	MAX_DENOM = BOUND1 - 1
};

/////////////
// Encoder //
/////////////

Encoder::Encoder(std::vector<uint8_t> & buffer) : buffer(buffer), bitIndex(7), underflow(), min(BOUND0), max(BOUND4)
{

}

void Encoder::Write(int bit)
{
	if(++bitIndex == 8)
	{
		buffer.push_back(0);
		bitIndex = 0;
	}

	buffer.back() |= bit << bitIndex;
}

void Encoder::Rescale(code_t window)
{
	min = (min - window) << 1;
	max = (max - window) << 1;
}

void Encoder::Encode(code_t a, code_t b, code_t denom)
{
	assert(0 <= a && a < b && b <= denom && denom <= MAX_DENOM);
	const code_t step = (max - min) / denom;
	max = min + step * b;
	min = min + step * a;

	while(true)
	{
		if(max <= BOUND2)
		{
			Write(0);
			for(; underflow > 0; --underflow) Write(1);
			Rescale(BOUND0);
		}
		else if(BOUND2 <= min)
		{
			Write(1);
			for(; underflow > 0; --underflow) Write(0);
			Rescale(BOUND2);
		}
		else break;
	}

	while(BOUND1 <= min && max <= BOUND3)
	{
		Rescale(BOUND1);
		++underflow;
	}
}

void Encoder::Finish()
{
	Write(1);
}

/////////////
// Decoder //
/////////////

Decoder::Decoder(const std::vector<uint8_t> & buffer) : buffer(buffer), byteIndex(), bitIndex(), min(BOUND0), max(BOUND4), code(), step()
{
	for(int i=1; i<NUM_BITS; ++i) code = (code << 1) | Read();
}

code_t Decoder::Read()
{
	if(byteIndex == buffer.size()) return 0;
	int r = (buffer[byteIndex] >> bitIndex) & 1;
	if(++bitIndex == 8)
	{
		++byteIndex;
		bitIndex = 0;
	}
	return r;
}

void Decoder::Rescale(code_t window)
{
	min = (min - window) << 1;
	max = (max - window) << 1;
	code = ((code - window) << 1) | Read();
}

code_t Decoder::Decode(code_t denom)
{
	assert(0 < denom && denom <= MAX_DENOM);
	step = (max - min) / denom;
	return (code - min) / step;
}

void Decoder::Confirm(code_t a, code_t b)
{
	assert(0 <= a && a < b);
	max = min + step * b;
	min = min + step * a;

	while(true)
	{
		if(max <= BOUND2)
		{
			Rescale(BOUND0);
		}
		else if(BOUND2 <= min)
		{
			Rescale(BOUND2);
		}
		else break;
	}

	while(BOUND1 <= min && max <= BOUND3)
	{
		Rescale(BOUND1);
	}
}
