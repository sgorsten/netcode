#include "Util.h"

#include <cassert>

namespace netcode
{
    void EncodeUniform(ArithmeticEncoder & encoder, code_t x, code_t d)
    {
	    encoder.Encode(x, x + 1, d);
    }

    code_t DecodeUniform(ArithmeticDecoder & decoder, code_t d)
    {
	    auto x = decoder.Decode(d);
	    decoder.Confirm(x, x + 1);
	    return x;
    }

    void EncodeBits(ArithmeticEncoder & encoder, code_t value, int n)
    {
        if(n > 16)
        {
            EncodeBits(encoder, value, 16);
            EncodeBits(encoder, value>>16, n-16);
        }
        else EncodeUniform(encoder, value & ~(-1U << n), 1 << n);
    }

    code_t DecodeBits(ArithmeticDecoder & decoder, int n)
    {
        if(n > 16)
        {
            code_t lo = DecodeBits(decoder, 16);
            code_t hi = DecodeBits(decoder, n-16);
            return hi << 16 | lo;
        }
        return DecodeUniform(decoder, 1 << n);
    }

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

	    return float(b-a) / d;    
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
	    for (int i = 0; i < counts.size(); ++i) d += counts[i];
	    code_t x = decoder.Decode(d);

	    code_t a = 0;
	    for (int i = 0; i < counts.size(); ++i)
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
}