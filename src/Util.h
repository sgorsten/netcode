#ifndef DISTRIBUTION_H
#define DISTRIBUTION_H

#include "Arith.h"

#include <algorithm>

template<class T> size_t GetIndex(const std::vector<T> & vec, T value) { return std::find(begin(vec), end(vec), value) - begin(vec); }
template<class T, class F> void EraseIf(T & container, F f) { container.erase(remove_if(begin(container), end(container), f), end(container)); }

void EncodeUniform(arith::Encoder & encoder, arith::code_t x, arith::code_t d);
arith::code_t DecodeUniform(arith::Decoder & decoder, arith::code_t d);

class SymbolDistribution
{
    std::vector<arith::code_t> counts;
public:
    SymbolDistribution() {}
    SymbolDistribution(size_t symbols);

    void EncodeAndTally(arith::Encoder & encoder, size_t symbol);
	size_t DecodeAndTally(arith::Decoder & decoder);
};

class IntegerDistribution
{
    SymbolDistribution dist;
public:
	IntegerDistribution();

	void EncodeAndTally(arith::Encoder & encoder, int value);
	int DecodeAndTally(arith::Decoder & decoder);
};

struct FieldDistribution
{
    IntegerDistribution dists[5];
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
    int operator()(int y0, int y1, int y2, int y3) const { return (c0*y0 + c1*y1 + c2*y2 + c3*y3)/denom; }
};

CurvePredictor MakeConstantPredictor();
CurvePredictor MakeLinearPredictor(int t0, int t1);
CurvePredictor MakeQuadraticPredictor(int t0, int t1, int t2);
CurvePredictor MakeCubicPredictor(int t0, int t1, int t2, int t3);

#endif