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

void TestBitsEncoding(netcode::code_t value, int bits)
{
    // Encode the ranges into a buffer using arithmetic coding
    std::vector<uint8_t> buffer;
	ArithmeticEncoder encoder(buffer);
    EncodeBits(encoder, value, bits);
	encoder.Finish();

    // Ensure that we do not use more than one extra byte in encoding this value
    REQUIRE( buffer.size() <= (bits+7)/8 + 1 );

    // Decode the ranges and require that they match
    ArithmeticDecoder decoder(buffer);
    auto decodedValue = DecodeBits(decoder, bits);
    REQUIRE( decodedValue == value );
}

TEST_CASE( "Raw sequences of bits are encoded correctly", "[arithmetic coding]" )
{
    TestBitsEncoding(0x00000000,  0);
    TestBitsEncoding(0x00000001,  1);
    TestBitsEncoding(0x00000003,  2);
    TestBitsEncoding(0x00000007,  3);
    TestBitsEncoding(0x0000000F,  4);
    TestBitsEncoding(0x0000001F,  5);
    TestBitsEncoding(0x0000003F,  6);
    TestBitsEncoding(0x0000007F,  7);
    TestBitsEncoding(0x000000FF,  8);
    TestBitsEncoding(0x000001FF,  9);
    TestBitsEncoding(0x000003FF, 10);
    TestBitsEncoding(0x000007FF, 11);
    TestBitsEncoding(0x00000FFF, 12);
    TestBitsEncoding(0x00001FFF, 13);
    TestBitsEncoding(0x00003FFF, 14);
    TestBitsEncoding(0x00007FFF, 15);
    TestBitsEncoding(0x0000FFFF, 16);
    TestBitsEncoding(0x0001FFFF, 17);
    TestBitsEncoding(0x0003FFFF, 18);
    TestBitsEncoding(0x0007FFFF, 19);
    TestBitsEncoding(0x000FFFFF, 20);
    TestBitsEncoding(0x001FFFFF, 21);
    TestBitsEncoding(0x003FFFFF, 22);
    TestBitsEncoding(0x007FFFFF, 23);
    TestBitsEncoding(0x00FFFFFF, 24);
    TestBitsEncoding(0x01FFFFFF, 25);
    TestBitsEncoding(0x03FFFFFF, 26);
    TestBitsEncoding(0x07FFFFFF, 27);
    TestBitsEncoding(0x0FFFFFFF, 28);
    TestBitsEncoding(0x1FFFFFFF, 29);
    TestBitsEncoding(0x3FFFFFFF, 30);
    TestBitsEncoding(0x7FFFFFFF, 31);
    TestBitsEncoding(0xFFFFFFFF, 32);
}