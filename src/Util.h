#ifndef NETCODE_UTIL_H
#define NETCODE_UTIL_H

#include "Arith.h"

#include <algorithm>

namespace netcode
{
    template<class T> size_t GetIndex(const std::vector<T> & vec, T value) { return std::find(begin(vec), end(vec), value) - begin(vec); }
    template<class T, class F> void EraseIf(T & container, F f) { container.erase(remove_if(begin(container), end(container), f), end(container)); }

    void EncodeUniform(ArithmeticEncoder & encoder, code_t x, code_t d);
    code_t DecodeUniform(ArithmeticDecoder & decoder, code_t d);

    class SymbolDistribution
    {
        std::vector<code_t> counts;
    public:
        SymbolDistribution() {}
        SymbolDistribution(size_t symbols);

        void EncodeAndTally(ArithmeticEncoder & encoder, size_t symbol);
	    size_t DecodeAndTally(ArithmeticDecoder & decoder);
    };

    class IntegerDistribution
    {
        SymbolDistribution dist;
    public:
	    IntegerDistribution();

	    void EncodeAndTally(ArithmeticEncoder & encoder, int value);
	    int DecodeAndTally(ArithmeticDecoder & decoder);
    };

    class RangeAllocator
    {
        size_t totalCapacity;
        std::vector<std::pair<size_t,size_t>> freeList;
    public:
        RangeAllocator();

        size_t GetTotalCapacity() const { return totalCapacity; }

        size_t Allocate(size_t amount);
        void Free(size_t offset, size_t amount);
    };

    struct CurvePredictor 
    { 
        int c0,c1,c2,c3,denom;
        CurvePredictor() : c0(),c1(),c2(),c3(),denom(1) {}
        CurvePredictor(const int (&matrix)[4][4]);
        int operator()(const int (&samples)[4]) const { return (c0*samples[0] + c1*samples[1] + c2*samples[2] + c3*samples[3])/denom; }
    };

    CurvePredictor MakeConstantPredictor();
    CurvePredictor MakeLinearPredictor(int t0, int t1);
    CurvePredictor MakeQuadraticPredictor(int t0, int t1, int t2);
    CurvePredictor MakeCubicPredictor(int t0, int t1, int t2, int t3);

    struct FieldDistribution
    {
        IntegerDistribution dists[5];

        void EncodeAndTally(ArithmeticEncoder & encoder, int value, const int (&prevValues)[4], const CurvePredictor (&predictors)[5], int sampleCount);
        int DecodeAndTally(ArithmeticDecoder & decoder, const int (&prevValues)[4], const CurvePredictor (&predictors)[5], int sampleCount);
    };
}

#endif