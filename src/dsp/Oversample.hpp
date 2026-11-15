// x2 oversampling with a 31-tap windowed-sinc half-band (used for hard clipping and rattle).
#pragma once
#include "Common.hpp"

namespace tudor {

struct HalfBand {
	static constexpr int N = 31;
	float h[N];
	float z[N] = {};
	int p = 0;
	HalfBand() {
		int c = N / 2;
		for (int i = 0; i < N; i++) {
			int k = i - c;
			float s = k == 0 ? 0.5f : std::sin(PI * k * 0.5f) / (PI * k);
			float win = 0.42f - 0.5f * std::cos(TWO_PI * i / (N - 1)) + 0.08f * std::cos(2 * TWO_PI * i / (N - 1));
			h[i] = s * win;
		}
		float sum = 0; for (float v : h) sum += v;
		for (float& v : h) v /= sum;
	}
	float process(float x) {
		z[p] = x;
		float y = 0;
		int idx = p;
		for (int i = 0; i < N; i++) { y += h[i] * z[idx]; idx = idx == 0 ? N - 1 : idx - 1; }
		p = (p + 1) % N;
		return y;
	}
	void reset() { for (float& v : z) v = 0; }
};

// Usage: up.process(x, a, b) gives two samples at 2x; run f on both; down.process(fa, fb).
struct Up2 {
	HalfBand f;
	void process(float x, float& a, float& b) { a = 2.f * f.process(x); b = 2.f * f.process(0.f); }
	void reset() { f.reset(); }
};
struct Down2 {
	HalfBand f;
	float process(float a, float b) { f.process(a); return f.process(b); }
	void reset() { f.reset(); }
};

} // namespace tudor
