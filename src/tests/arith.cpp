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

TEST_CASE( "Symbols are encoded and decoded correctly", "[symbol distribution]" )
{
    // Generate a list of symbols, each with a different probability
    std::vector<size_t> symbols;
    std::mt19937 engine(0);
    std::uniform_real_distribution<double> r(0, 1);
	for (int i = 0; i < 1000; ++i)
	{
        auto x = r(engine);
        if(x < 0.4) symbols.push_back(0);
        else if(x < 0.6) symbols.push_back(1);
        else if(x < 0.7) symbols.push_back(2);
        else if(x < 0.85) symbols.push_back(3);
        else if(x < 0.99) symbols.push_back(4);
        else symbols.push_back(5);
    }

    // Encode the symbols while building up a distribution
    std::vector<uint8_t> buffer;
	ArithmeticEncoder encoder(buffer);
    SymbolDistribution encoderDist(6);
    for(auto symbol : symbols)
    {
        encoderDist.EncodeAndTally(encoder, symbol);
    }
    encoder.Finish();

    // Decode the symbols, and build up the same distributoin
	ArithmeticDecoder decoder(buffer);
    SymbolDistribution decoderDist(6);
    for(auto originalSymbol : symbols)
    {
        auto decodedSymbol = decoderDist.DecodeAndTally(decoder);
        REQUIRE( decodedSymbol == originalSymbol );
    }
}