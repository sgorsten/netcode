// Copyright (c) 2015 Sterling Orsten
//   This software is provided 'as-is', without any express or implied
// warranty. In no event will the author be held liable for any damages
// arising from the use of this software. You are granted a perpetual, 
// irrevocable, world-wide license to copy, modify, and redistribute
// this software for any purpose, including commercial applications.

#include "utility.h"

#include <cassert>

namespace netcode
{
    enum : code_t
    {
	    NUM_BITS = sizeof(code_t) * 8,
	    BOUND0 = 0,
	    BOUND1 = 1ULL << (NUM_BITS - 3),
	    BOUND2 = 1ULL << (NUM_BITS - 2),
	    BOUND3 = BOUND1 | BOUND2,
	    BOUND4 = 1ULL << (NUM_BITS - 1),	
	    MAX_DENOM = BOUND1 - 1
    };

    ///////////////////////
    // ArithmeticEncoder //
    ///////////////////////

    ArithmeticEncoder::ArithmeticEncoder(std::vector<uint8_t> & buffer) : buffer(buffer), bitIndex(7), underflow(), min(BOUND0), max(BOUND4)
    {

    }

    void ArithmeticEncoder::Write(int bit)
    {
	    if(++bitIndex == 8)
	    {
		    buffer.push_back(0);
		    bitIndex = 0;
	    }

	    buffer.back() |= bit << bitIndex;
    }

    void ArithmeticEncoder::Rescale(code_t window)
    {
	    min = (min - window) << 1;
	    max = (max - window) << 1;
    }

    void ArithmeticEncoder::Encode(code_t a, code_t b, code_t denom)
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

    void ArithmeticEncoder::Finish()
    {
	    Write(1);
    }

    void EncodeUniform(ArithmeticEncoder & encoder, code_t x, code_t d)
    {
        assert(x < d && d <= MAX_DENOM);
	    encoder.Encode(x, x + 1, d);
    }

    void EncodeBits(ArithmeticEncoder & encoder, code_t value, int n)
    {
        if(n > 28)
        {
            EncodeBits(encoder, value, 16);
            EncodeBits(encoder, value>>16, n-16);
        }
        else EncodeUniform(encoder, value & ~(code_t(-1) << n), 1 << n);
    }

    ///////////////////////
    // ArithmeticDecoder //
    ///////////////////////

    ArithmeticDecoder::ArithmeticDecoder(const std::vector<uint8_t> & buffer) : buffer(buffer), byteIndex(), bitIndex(), min(BOUND0), max(BOUND4), code(), step()
    {
	    for(int i=1; i<NUM_BITS; ++i) code = (code << 1) | Read();
    }

    code_t ArithmeticDecoder::Read()
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

    void ArithmeticDecoder::Rescale(code_t window)
    {
	    min = (min - window) << 1;
	    max = (max - window) << 1;
	    code = ((code - window) << 1) | Read();
    }

    code_t ArithmeticDecoder::Decode(code_t denom)
    {
	    assert(0 < denom && denom <= MAX_DENOM);
	    step = (max - min) / denom;
	    return (code - min) / step;
    }

    void ArithmeticDecoder::Confirm(code_t a, code_t b)
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

    code_t DecodeUniform(ArithmeticDecoder & decoder, code_t d)
    {
        assert(d <= MAX_DENOM);
	    auto x = decoder.Decode(d);
	    decoder.Confirm(x, x + 1);
	    return x;
    }

    code_t DecodeBits(ArithmeticDecoder & decoder, int n)
    {
        if(n > 28)
        {
            code_t lo = DecodeBits(decoder, 16);
            code_t hi = DecodeBits(decoder, n-16);
            return hi << 16 | lo;
        }
        return DecodeUniform(decoder, 1 << n);
    }

    ////////////////////////
    // SymbolDistribution //
    ////////////////////////

    SymbolDistribution::SymbolDistribution(size_t symbols) : counts(symbols,1)
    {

    }

    float SymbolDistribution::GetTrueProbability(size_t symbol) const
    {
        assert(symbol < counts.size());

        code_t a = 0;
	    for (size_t i = 0; i < symbol; ++i) a += counts[i]-1;
	    code_t b = a + counts[symbol]-1, d = b;
	    for (size_t i = symbol + 1; i < counts.size(); ++i) d += counts[i]-1;

	    return d > 0 ? float(b-a) / d : 0.0f;
    }

    float SymbolDistribution::GetProbability(size_t symbol) const
    {
        assert(symbol < counts.size());

        code_t a = 0;
	    for (size_t i = 0; i < symbol; ++i) a += counts[i];
	    code_t b = a + counts[symbol], d = b;
	    for (size_t i = symbol + 1; i < counts.size(); ++i) d += counts[i];

	    return float(b-a) / d;
    }

    float SymbolDistribution::GetExpectedCost() const
    {
        float cost = 0;
        for(size_t i=0; i<counts.size(); ++i)
        {
            float p = GetProbability(i);
            cost += p * -log(p);
        }
        return cost;
    }

    void SymbolDistribution::Tally(size_t symbol)
    {
        assert(symbol < counts.size());
        ++counts[symbol];
    }

    void SymbolDistribution::EncodeAndTally(ArithmeticEncoder & encoder, size_t symbol)
    {
        assert(symbol < counts.size());

        code_t a = 0;
	    for (size_t i = 0; i < symbol; ++i) a += counts[i];
	    code_t b = a + counts[symbol], d = b;
	    for (size_t i = symbol + 1; i < counts.size(); ++i) d += counts[i];
	    encoder.Encode(a, b, d);

	    Tally(symbol);
    }

    size_t SymbolDistribution::DecodeAndTally(ArithmeticDecoder & decoder)
    {
        code_t d = 0;
	    for (size_t i = 0; i < counts.size(); ++i) d += counts[i];
	    code_t x = decoder.Decode(d);

	    code_t a = 0;
	    for (size_t i = 0; i < counts.size(); ++i)
	    {
		    code_t b = a + counts[i];
		    if (b > x)
		    {
			    decoder.Confirm(a, b);
                Tally(i);
                return i;
		    }
		    a = b;
	    }

	    assert(false);
        return 0;
    }

    //////////////////////////
    // Integer distribution //
    //////////////////////////

    static int CountSignificantBits(int value)
    {
	    int sign = value < 0 ? -1 : 0;
	    for (int i = 0; i < 31; ++i) if (value >> i == sign) return i;
	    return 31;
    }

    IntegerDistribution::IntegerDistribution() : dist(64)
    { 

    }

    double IntegerDistribution::GetAverageValue() const
    {
        double totalValue=0, totalWeight=0;
        for(int bits=0; bits<32; ++bits)
        {
            int minValue = bits > 0 ? 0 | (1 << (bits-1)) : 0;
            int maxValue = bits > 0 ? ((1 << (bits-1))-1) | (1 << (bits-1)) : 0;
            double bucketAvgValue = ((double)minValue + (double)maxValue) / 2;
            float p = dist.GetTrueProbability(bits);
            totalValue += bucketAvgValue * p;
            totalWeight += p;

            minValue = ~minValue;
            maxValue = ~maxValue;
            bucketAvgValue = ((double)minValue + (double)maxValue) / 2;
            p = dist.GetTrueProbability(bits + 32);
            totalValue += bucketAvgValue * p;
            totalWeight += p;
        }
        return totalValue / totalWeight;
    }

    float IntegerDistribution::GetExpectedCost() const
    {
        float cost = 0;
        for(int bits=0; bits<32; ++bits)
        {
            float p = dist.GetProbability(bits);
            cost += p * (-log(p) + std::max(bits-1,0));

            p = dist.GetProbability(bits + 32);
            cost += p * (-log(p) + std::max(bits-1,0));
        }
        return cost;
    }

    void IntegerDistribution::Tally(int value)
    {
        int bits = CountSignificantBits(value)+1;
        int bucket = bits-1 + (value < 0 ? 32 : 0);
        dist.Tally(bucket);
    }

    void IntegerDistribution::EncodeAndTally(ArithmeticEncoder & encoder, int value)
    {
	    int bits = CountSignificantBits(value);
        int bucket = bits + (value < 0 ? 32 : 0);
        dist.EncodeAndTally(encoder, bucket);
        if(value < 0) value = ~value; // number will either be 0 or 0* 1 (0|1)*
        if(bits > 0) EncodeBits(encoder, value, bits-1); // encode the bits below the most significant bit
    }

    int IntegerDistribution::DecodeAndTally(ArithmeticDecoder & decoder)
    {
        int bucket = dist.DecodeAndTally(decoder);
        int bits = (bucket & 0x1F);
        int value = bits > 0 ? DecodeBits(decoder, bits-1) | (1 << (bits-1)) : 0; // decode the bits below the most significant bit
        return bucket & 0x20 ? ~value : value; // restore sign if this number belonged to a negative bucket
    }

    ////////////////////
    // CurvePredictor //
    ////////////////////

    CurvePredictor::CurvePredictor(const int (&m)[4][4]) :
        c0(m[1][1]*m[2][2]*m[3][3] + m[3][1]*m[1][2]*m[2][3] + m[2][1]*m[3][2]*m[1][3] - m[1][1]*m[3][2]*m[2][3] - m[2][1]*m[1][2]*m[3][3] - m[3][1]*m[2][2]*m[1][3]),
        c1(m[0][1]*m[3][2]*m[2][3] + m[2][1]*m[0][2]*m[3][3] + m[3][1]*m[2][2]*m[0][3] - m[3][1]*m[0][2]*m[2][3] - m[2][1]*m[3][2]*m[0][3] - m[0][1]*m[2][2]*m[3][3]),
        c2(m[0][1]*m[1][2]*m[3][3] + m[3][1]*m[0][2]*m[1][3] + m[1][1]*m[3][2]*m[0][3] - m[0][1]*m[3][2]*m[1][3] - m[1][1]*m[0][2]*m[3][3] - m[3][1]*m[1][2]*m[0][3]),
        c3(m[0][1]*m[2][2]*m[1][3] + m[1][1]*m[0][2]*m[2][3] + m[2][1]*m[1][2]*m[0][3] - m[0][1]*m[1][2]*m[2][3] - m[2][1]*m[0][2]*m[1][3] - m[1][1]*m[2][2]*m[0][3]),
        denom(m[0][0]*(m[1][1]*m[2][2]*m[3][3] + m[3][1]*m[1][2]*m[2][3] + m[2][1]*m[3][2]*m[1][3] - m[1][1]*m[3][2]*m[2][3] - m[2][1]*m[1][2]*m[3][3] - m[3][1]*m[2][2]*m[1][3])
            + m[0][1]*(m[1][2]*m[3][3]*m[2][0] + m[2][2]*m[1][3]*m[3][0] + m[3][2]*m[2][3]*m[1][0] - m[1][2]*m[2][3]*m[3][0] - m[3][2]*m[1][3]*m[2][0] - m[2][2]*m[3][3]*m[1][0]) 
            + m[0][2]*(m[1][3]*m[2][0]*m[3][1] + m[3][3]*m[1][0]*m[2][1] + m[2][3]*m[3][0]*m[1][1] - m[1][3]*m[3][0]*m[2][1] - m[2][3]*m[1][0]*m[3][1] - m[3][3]*m[2][0]*m[1][1])
            + m[0][3]*(m[1][0]*m[3][1]*m[2][2] + m[2][0]*m[1][1]*m[3][2] + m[3][0]*m[2][1]*m[1][2] - m[1][0]*m[2][1]*m[3][2] - m[3][0]*m[1][1]*m[2][2] - m[2][0]*m[3][1]*m[1][2]))
    {

    }

    CurvePredictor MakeZeroPredictor() 
    { 
        return CurvePredictor(); 
    }

    CurvePredictor MakeConstantPredictor()
    {
        const int matrix[4][4] = {
            {1,0,0,0},
            {0,1,0,0},
            {0,0,1,0},
            {0,0,0,1}
        };
        return CurvePredictor(matrix);
    }

    CurvePredictor MakeLinearPredictor(int t0, int t1)
    {
        const int matrix[4][4] = {
            {1,t0,0,0},
            {1,t1,0,0},
            {0,0,1,0},
            {0,0,0,1}
        };
        return CurvePredictor(matrix);
    }

    CurvePredictor MakeQuadraticPredictor(int t0, int t1, int t2)
    {
        const int matrix[4][4] = {
            {1,t0,t0*t0,0},
            {1,t1,t1*t1,0},
            {1,t2,t2*t2,0},
            {0,0,0,1}
        };
        return CurvePredictor(matrix);
    }

    CurvePredictor MakeCubicPredictor(int t0, int t1, int t2, int t3)
    {
        const int matrix[4][4] = {
            {1,t0,t0*t0,t0*t0*t0},
            {1,t1,t1*t1,t1*t1*t1},
            {1,t2,t2*t2,t2*t2*t2},
            {1,t3,t3*t3,t3*t3*t3}
        };
        return CurvePredictor(matrix);
    }

    ///////////////////////
    // FieldDistribution //
    ///////////////////////

    int FieldDistribution::GetBestDistribution(int sampleCount) const
    {
        int bestDist = 0;
        float bestCost = dists[0].GetExpectedCost();
        for(int i=1; i<=sampleCount; ++i)
        {
            float cost = dists[i].GetExpectedCost();
            if(cost < bestCost)
            {
                bestDist = i;
                bestCost = cost;
            }
        }
        return bestDist;
    }

    void FieldDistribution::EncodeAndTally(ArithmeticEncoder & encoder, int value, const int (&prevValues)[4], const CurvePredictor (&predictors)[5], int sampleCount)
    {
        int best = GetBestDistribution(sampleCount);
        dists[best].EncodeAndTally(encoder, value - predictors[best](prevValues));
        for(int i=0; i<=sampleCount; ++i) if(i != best) dists[i].Tally(value - predictors[i](prevValues));
    }

    int FieldDistribution::DecodeAndTally(ArithmeticDecoder & decoder, const int (&prevValues)[4], const CurvePredictor (&predictors)[5], int sampleCount)
    {
        int best = GetBestDistribution(sampleCount);
        int value = dists[best].DecodeAndTally(decoder) + predictors[best](prevValues);
        for(int i=0; i<=sampleCount; ++i) if(i != best) dists[i].Tally(value - predictors[i](prevValues));
        return value;
    }

    ////////////////////
    // RangeAllocator //
    ////////////////////

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
}