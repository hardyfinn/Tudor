// Power-of-two ring buffer with Hermite fractional read. Allocate outside process().
#pragma once
#include "Common.hpp"
#include <vector>

namespace tudor {

struct DelayLine {
	std::vector<float> buf;
	int mask = 0, w = 0;
	void init(int minLen) {
		int n = 1;
		while (n < minLen + 4) n <<= 1;
		buf.assign(n, 0.f);
		mask = n - 1;
		w = 0;
	}
	void push(float x) { buf[w] = x; w = (w + 1) & mask; }
	float at(int d) const { return buf[(w - 1 - d) & mask]; }
	// delay in samples (>= 1 means at least one sample old)
	float read(float d) const {
		d = clampf(d, 1.f, (float)(mask - 3));
		int i = (int)d;
		float f = d - i;
		float xm1 = at(i - 1), x0 = at(i), x1 = at(i + 1), x2 = at(i + 2);
		float c1 = 0.5f * (x1 - xm1);
		float c2 = xm1 - 2.5f * x0 + 2.f * x1 - 0.5f * x2;
		float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
		return ((c3 * f + c2) * f + c1) * f + x0;
	}
	void clear() { std::fill(buf.begin(), buf.end(), 0.f); }
};

} // namespace tudor
