// Copyright (c) 2015 Sterling Orsten
//   This software is provided 'as-is', without any express or implied
// warranty. In no event will the author be held liable for any damages
// arising from the use of this software. You are granted a perpetual, 
// irrevocable, world-wide license to copy, modify, and redistribute
// this software for any purpose, including commercial applications.

#include "thirdparty/catch.hpp"
#include "utility.h"
#include <random>

using namespace netcode;

TEST_CASE( "A sequence of ranges are encoded and decoded correctly", "[arithmetic coding]" )
{
    // Generate a list of ranges in the form [a/d, b/d)
	struct Range { uint32_t a, b, d; };
	std::vector<Range> ranges;
	std::mt19937 engine(0);
	for (int i = 0; i < 500; ++i)
	{
		auto denom = std::uniform_int_distribution<uint32_t>(1, 10000)(engine);
		auto num1 = std::uniform_int_distribution<uint32_t>(0, denom - 1)(engine);
		auto num2 = std::uniform_int_distribution<uint32_t>(0, denom - 1)(engine);
		if (num2 < num1) std::swap(num1, num2);
		ranges.push_back({ num1, num2 + 1, denom });
	}

    // Encode the ranges into a buffer using arithmetic coding
    std::vector<uint8_t> buffer;
	ArithmeticEncoder encoder(buffer);
	for (auto & range : ranges) encoder.Encode(range.a, range.b, range.d);
	encoder.Finish();

    // Decode the ranges and require that they match
    ArithmeticDecoder decoder(buffer);
	for (auto & range : ranges)
	{
		uint32_t i = decoder.Decode(range.d);
        REQUIRE( i >= range.a );
        REQUIRE( i < range.b );
		decoder.Confirm(range.a, range.b);
	}
}
