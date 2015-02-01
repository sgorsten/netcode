#include "ArithCode.h"

#include <iostream>
#include <random>

int CountSignificantBits(int value)
{
	int sign = value < 0 ? -1 : 0;
	for (int i = 0; i < 31; ++i) if (value >> i == sign) return i + 1;
	return 32;
}

class IntegerDistribution
{
	arith::code_t counts[32];
public:
	IntegerDistribution() { for (int i = 0; i < 32; ++i) counts[i] = 1; }

	void EncodeAndTally(arith::Encoder & encoder, int value)
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

	int DecodeAndTally(arith::Decoder & decoder)
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
};

void TestRanges();
void TestIntegerCoding();

int main(int argc, char * argv[])
{
	TestRanges();
	TestIntegerCoding();
}

void CompressFrame(const int * values, size_t numValues);

void TestIntegerCoding()
{
	std::mt19937 engine;
	int frame1[64], frame2[64];
	for (int i = 0; i < 64; ++i)
	{
		frame1[i] = std::uniform_int_distribution<int>(0, 1000)(engine);
		frame2[i] = frame1[i] + std::uniform_int_distribution<int>(-10, +10)(engine);
	}

	CompressFrame(frame1, 64);
	CompressFrame(frame2, 64);
	for (int i = 0; i < 64; ++i) frame2[i] -= frame1[i];
	CompressFrame(frame2, 64);
}

void CompressFrame(const int * values, size_t numValues)
{
	std::vector<uint8_t> buffer;
	arith::Encoder encoder(buffer);
	IntegerDistribution dist1;
	for (int i = 0; i < numValues; ++i)
	{
		dist1.EncodeAndTally(encoder, values[i]);
	}
	encoder.Finish();

	arith::Decoder decoder(buffer);
	IntegerDistribution dist2;
	for (int i = 0; i < numValues; ++i)
	{
		int value = dist2.DecodeAndTally(decoder);
		if (value != values[i])
		{
			std::cerr << "Integer encoding error." << std::endl;
			return;
		}
	}

	std::cout << "Integer encoding success. Compressed from " << sizeof(*values) * numValues << " B to " << buffer.size() << " B." << std::endl;
	return;
}

void TestRanges()
{
	struct Range { uint32_t a, b, d; };
	std::vector<Range> ranges;

	std::mt19937 engine;
	for (int i = 0; i < 10000; ++i)
	{
		auto denom = std::uniform_int_distribution<uint32_t>(1, 10000)(engine);
		auto num1 = std::uniform_int_distribution<uint32_t>(0, denom - 1)(engine);
		auto num2 = std::uniform_int_distribution<uint32_t>(0, denom - 1)(engine);
		if (num2 < num1) std::swap(num1, num2);
		ranges.push_back({ num1, num2 + 1, denom });
	}

	std::vector<uint8_t> buffer;
	arith::Encoder encoder(buffer);
	for (auto & range : ranges) encoder.Encode(range.a, range.b, range.d);
	encoder.Finish();

	arith::Decoder decoder(buffer);
	for (auto & range : ranges)
	{
		uint32_t i = decoder.Decode(range.d);
		if (i < range.a || i >= range.b)
		{
			std::cerr << "Decoding error" << std::endl;
			return;
		}
		decoder.Confirm(range.a, range.b);
	}
	std::cout << "Success!" << std::endl;
}