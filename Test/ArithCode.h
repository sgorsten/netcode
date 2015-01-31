#ifndef ARITH_CODE_H
#define ARITH_CODE_H

#include <cstdint>
#include <vector>

namespace arith
{
	typedef uint32_t code_t; // Can use uint64_t for a 64-bit coder

	class Encoder
	{
		std::vector<uint8_t> & buffer;
		int		bitIndex, underflow;
		code_t	min, max;

		void	Write(int bit);
		void	Rescale(code_t window);
	public:
				Encoder(std::vector<uint8_t> & buffer);

		void	Encode(code_t a, code_t b, code_t denom);		// Encodes the range [a/denom, b/denom)
		void	Finish();										// Finishes off the stream
	};

	class Decoder
	{
		const std::vector<uint8_t> & buffer;
		int		byteIndex, bitIndex;
		code_t	min, max, code, step;

		code_t	Read();
		void	Rescale(code_t window);
	public:
				Decoder(const std::vector<uint8_t> & buffer);

		code_t	Decode(code_t denom);							// Returns x where x/denom is in [a/denom, b/denom) from original Encode(a,b,denom) call
		void	Confirm(code_t a, code_t b);					// Call with a,b from original Encode(a,b,denom) call, where a <= Decode(denom) < b
	};
}

#endif