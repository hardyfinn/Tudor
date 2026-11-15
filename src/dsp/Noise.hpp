// Random numbers, noise colours and slow component drift.
#pragma once
#include "Common.hpp"

namespace tudor {

// xoshiro128+
struct Rng {
	uint32_t s[4] = {0x9E3779B9u, 0x243F6A88u, 0xB7E15162u, 0x12345678u};
	explicit Rng(uint64_t seed = 1) { seedWith(seed); }
	void seedWith(uint64_t seed) {
		uint64_t z = seed + 0x9E3779B97F4A7C15ull;
		for (int i = 0; i < 4; i++) {
			z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
			z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
			z ^= z >> 31;
			s[i] = (uint32_t)z | 1u;
			z += 0x9E3779B97F4A7C15ull;
		}
	}
	static uint32_t rotl(uint32_t x, int k) { return (x << k) | (x >> (32 - k)); }
	uint32_t next() {
		uint32_t r = s[0] + s[3];
		uint32_t t = s[1] << 9;
		s[2] ^= s[0]; s[3] ^= s[1]; s[1] ^= s[2]; s[0] ^= s[3];
		s[2] ^= t; s[3] = rotl(s[3], 11);
		return r;
	}
	float uniform() { return (next() >> 8) * (1.f / 16777216.f); }          // [0,1)
	float bipolar() { return uniform() * 2.f - 1.f; }                         // [-1,1)
	float gauss() { return (uniform() + uniform() + uniform() + uniform() - 2.f) * 1.7320508f; } // ~N(0,1)
	int below(int n) { return (int)(uniform() * n) % std::max(n, 1); }
	float expo(float mean) { return -mean * std::log(std::max(1e-7f, 1.f - uniform())); }
};

// Paul Kellet's economy pink filter.
struct Pink {
	float b0 = 0, b1 = 0, b2 = 0;
	float process(float white) {
		b0 = 0.99765f * b0 + white * 0.0990460f;
		b1 = 0.96300f * b1 + white * 0.2965164f;
		b2 = 0.57000f * b2 + white * 1.0526913f;
		return (b0 + b1 + b2 + white * 0.1848f) * 0.2f;
	}
};

// Slow 1/f-like wander for component instability: about N(0,1).
struct Drift {
	Rng rng;
	float y[3] = {0, 0, 0};
	float sr = 48000.f;
	float c[3] = {0, 0, 0};
	static constexpr float fc[3] = {0.05f, 0.4f, 3.f};
	static constexpr float w[3] = {0.6f, 0.3f, 0.1f};
	explicit Drift(uint64_t seed = 7) : rng(seed) { setSampleRate(48000.f); }
	void setSampleRate(float fs) {
		sr = fs;
		for (int i = 0; i < 3; i++) c[i] = 1.f - std::exp(-TWO_PI * fc[i] / fs);
	}
	float next() {
		float out = 0.f;
		for (int i = 0; i < 3; i++) {
			// a one-pole driven by white noise has variance c/(2-c); rescale to unit
			float g = rng.gauss() * std::sqrt((2.f - c[i]) / c[i]);
			y[i] += c[i] * (g - y[i]);
			out += w[i] * y[i];
		}
		return out * 1.4744f;   // 1/sqrt(0.36+0.09+0.01)
	}
};

} // namespace tudor
