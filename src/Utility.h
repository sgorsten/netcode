#ifndef NETCODE_UTILITY_H
#define NETCODE_UTILITY_H

#include <cstdint>
#include <vector>
#include <algorithm>

namespace netcode
{
    typedef uint32_t code_t; // Can use uint64_t for a 64-bit coder

	class ArithmeticEncoder
	{
		std::vector<uint8_t> & buffer;
		int		bitIndex, underflow;
		code_t	min, max;

		void	Write(int bit);
		void	Rescale(code_t window);
	public:
				ArithmeticEncoder(std::vector<uint8_t> & buffer);

		void	Encode(code_t a, code_t b, code_t denom);		// Encodes the range [a/denom, b/denom)
		void	Finish();										// Finishes off the stream
	};
    void EncodeUniform(ArithmeticEncoder & encoder, code_t x, code_t d);
    void EncodeBits(ArithmeticEncoder & encoder, code_t value, int n); // Encode the least significant n bits of value

	class ArithmeticDecoder
	{
		const std::vector<uint8_t> & buffer;
		int		byteIndex, bitIndex;
		code_t	min, max, code, step;

		code_t	Read();
		void	Rescale(code_t window);
	public:
				ArithmeticDecoder(const std::vector<uint8_t> & buffer);

		code_t	Decode(code_t denom);							// Returns x where x/denom is in [a/denom, b/denom) from original Encode(a,b,denom) call
		void	Confirm(code_t a, code_t b);					// Call with a,b from original Encode(a,b,denom) call, where a <= Decode(denom) < b
	}; 
    code_t DecodeUniform(ArithmeticDecoder & decoder, code_t d);
    code_t DecodeBits(ArithmeticDecoder & decoder, int n); // Decode n bits and pack them into the least significant bits of an integer

    class SymbolDistribution
    {
        std::vector<code_t> counts;
    public:
        SymbolDistribution() {}
        SymbolDistribution(size_t symbols);

        float GetTrueProbability(size_t symbol) const;
        float GetProbability(size_t symbol) const;
        float GetExpectedCost() const;

        void Tally(size_t symbol);
        void EncodeAndTally(ArithmeticEncoder & encoder, size_t symbol);
	    size_t DecodeAndTally(ArithmeticDecoder & decoder);
    };

    class IntegerDistribution
    {
        SymbolDistribution dist;
    public:
	    IntegerDistribution();

        double GetAverageValue() const;
        float GetExpectedCost() const;

        void Tally(int value);
	    void EncodeAndTally(ArithmeticEncoder & encoder, int value);
	    int DecodeAndTally(ArithmeticDecoder & decoder);
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

        int GetBestDistribution(int sampleCount) const;
        void EncodeAndTally(ArithmeticEncoder & encoder, int value, const int (&prevValues)[4], const CurvePredictor (&predictors)[5], int sampleCount);
        int DecodeAndTally(ArithmeticDecoder & decoder, const int (&prevValues)[4], const CurvePredictor (&predictors)[5], int sampleCount);
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

    template<class T> size_t GetIndex(const std::vector<T> & vec, T value) { return std::find(begin(vec), end(vec), value) - begin(vec); }
    template<class T, class F> void EraseIf(T & container, F f) { container.erase(remove_if(begin(container), end(container), f), end(container)); }
}

#endif