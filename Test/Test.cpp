#include "ArithCode.h"

#include <iostream>
#include <random>

struct Range { uint32_t a, b, d; };

int main(int argc, char * argv[])
{
	std::vector<Range> ranges;

	std::mt19937 engine;
	for (int i = 0; i < 10000; ++i)
	{
		auto denom = std::uniform_int_distribution<uint32_t>(1, 10000)(engine);
		auto num1 = std::uniform_int_distribution<uint32_t>(0, denom-1)(engine);
		auto num2 = std::uniform_int_distribution<uint32_t>(0, denom-1)(engine);
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
			return EXIT_FAILURE;
		}
		decoder.Confirm(range.a, range.b);
	}
	std::cout << "Success!" << std::endl;
	return EXIT_SUCCESS;
}