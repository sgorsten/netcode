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