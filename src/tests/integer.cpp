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

TEST_CASE( "Integers are encoded and decoded correctly", "[integer distribution]" )
{
    // Generate a list of integers, using both signs and spanning the range of the data type
    std::vector<int> values;
    std::mt19937 engine(0);
    std::uniform_real_distribution<double> r(-1, 1);
	for (int i = 0; i < 1000; ++i)
	{
        values.push_back(static_cast<int>(r(engine) * r(engine) * r(engine) * INT_MAX));
    }

    // Encode the integers while building up a distribution
    std::vector<uint8_t> buffer;
	ArithmeticEncoder encoder(buffer);
    IntegerDistribution encoderDist;
    for(auto value : values)
    {
        encoderDist.EncodeAndTally(encoder, value);
    }
    encoder.Finish();

    // Decode the integers, and build up the same distribution
	ArithmeticDecoder decoder(buffer);
    IntegerDistribution decoderDist;
    for(auto originalValue : values)
    {
        auto decodedValue = decoderDist.DecodeAndTally(decoder);
        REQUIRE( decodedValue == originalValue );
    }
}

void TestSingleInt(int value)
{
    // Encode the integer into a buffer
    std::vector<uint8_t> buffer;
    ArithmeticEncoder encoder(buffer);
    IntegerDistribution encoderDist;
    encoderDist.EncodeAndTally(encoder, value);
    encoder.Finish();

    // Decode the integer and check that it has the same value
    ArithmeticDecoder decoder(buffer);
    IntegerDistribution decoderDist;
    int decodedValue = decoderDist.DecodeAndTally(decoder);
    REQUIRE( decodedValue == value );
}

TEST_CASE( "The full range of integers can be encoded losslessly", "[integer distribution]" )
{
    TestSingleInt(0);
    TestSingleInt(+1);
    TestSingleInt(-1);
    TestSingleInt(5);
    TestSingleInt(-452);
    TestSingleInt(29873123);
    TestSingleInt(INT_MAX);
    TestSingleInt(INT_MIN);
}