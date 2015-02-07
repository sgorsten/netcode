#include "Util.h"
#include <iostream>
#include <random>

using namespace netcode;

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
	ArithmeticEncoder encoder(buffer);
	IntegerDistribution dist1;
	for (int i = 0; i < numValues; ++i)
	{
		dist1.EncodeAndTally(encoder, values[i]);
	}
	encoder.Finish();

	ArithmeticDecoder decoder(buffer);
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
	ArithmeticEncoder encoder(buffer);
	for (auto & range : ranges) encoder.Encode(range.a, range.b, range.d);
	encoder.Finish();

	ArithmeticDecoder decoder(buffer);
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