/*  Written in 2019 by David Blackman and Sebastiano Vigna (vigna@acm.org)

To the extent possible under law, the author has dedicated all copyright
and related and neighboring rights to this software to the public domain
worldwide.

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

//------------------------------------------------------------------------
// 2025-05-13
// Customized for Arduino sketch by Shapoco.
// original: https://prng.di.unimi.it/xoshiro128plusplus.c
//------------------------------------------------------------------------

#include <stdint.h>

static inline uint32_t rotl(uint32_t x, uint8_t k) {
	if (k >= 8) {
		x = ((x << 24) & 0xff000000) | ((x >> 8) & 0x00ffffff);
		k -= 8;
	}
	return (x << k) | (x >> (32 - k));
}

class Xoshiro128plusplus {
public:
	uint32_t state[4] = { 0x12345678 };
	static constexpr uint8_t STATE_BYTES = sizeof(state);

	/* This is xoshiro128++ 1.0, one of our 32-bit all-purpose, rock-solid
   generators. It has excellent speed, a state size (128 bits) that is
   large enough for mild parallelism, and it passes all tests we are aware
   of.

   For generating just single-precision (i.e., 32-bit) floating-point
   numbers, xoshiro128+ is even faster.

   The state must be seeded so that it is not everywhere zero. */

	uint32_t next(void) {
		const uint32_t result = rotl(state[0] + state[3], 7) + state[0];

		const uint32_t t = state[1] << 9;

		state[2] ^= state[0];
		state[3] ^= state[1];
		state[1] ^= state[2];
		state[0] ^= state[3];

		state[2] ^= t;

		state[3] = rotl(state[3], 11);

		return result;
	}


	/* This is the jump function for the generator. It is equivalent
   to 2^64 calls to next(); it can be used to generate 2^64
   non-overlapping subsequences for parallel computations. */

	void jump(void) {
		constexpr uint32_t JUMP[] = { 0x8764000b, 0xf542d2d3, 0x6fa035c3, 0x77f2db5b };

		uint32_t s0 = 0;
		uint32_t s1 = 0;
		uint32_t s2 = 0;
		uint32_t s3 = 0;
		for (int i = 0; i < sizeof JUMP / sizeof *JUMP; i++)
			for (int b = 0; b < 32; b++) {
				if (JUMP[i] & UINT32_C(1) << b) {
					s0 ^= state[0];
					s1 ^= state[1];
					s2 ^= state[2];
					s3 ^= state[3];
				}
				next();
			}

		state[0] = s0;
		state[1] = s1;
		state[2] = s2;
		state[3] = s3;
	}


	/* This is the long-jump function for the generator. It is equivalent to
   2^96 calls to next(); it can be used to generate 2^32 starting points,
   from each of which jump() will generate 2^32 non-overlapping
   subsequences for parallel distributed computations. */

	void long_jump(void) {
		constexpr uint32_t LONG_JUMP[] = { 0xb523952e, 0x0b6f099f, 0xccf5a0ef, 0x1c580662 };

		uint32_t s0 = 0;
		uint32_t s1 = 0;
		uint32_t s2 = 0;
		uint32_t s3 = 0;
		for (int i = 0; i < sizeof LONG_JUMP / sizeof *LONG_JUMP; i++)
			for (int b = 0; b < 32; b++) {
				if (LONG_JUMP[i] & UINT32_C(1) << b) {
					s0 ^= state[0];
					s1 ^= state[1];
					s2 ^= state[2];
					s3 ^= state[3];
				}
				next();
			}

		state[0] = s0;
		state[1] = s1;
		state[2] = s2;
		state[3] = s3;
	}
};
