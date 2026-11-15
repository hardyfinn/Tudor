// Filters: TPT state-variable, one-poles, first-order allpass, DC blocker, RBJ biquads.
#pragma once
#include "Common.hpp"

namespace tudor {

// Zero-delay-feedback state-variable filter (Zavalishin).
struct Svf {
	float g = 0.1f, k = 1.f, a1 = 0, a2 = 0, a3 = 0;
	float ic1 = 0, ic2 = 0;
	float lp = 0, bp = 0, hp = 0;
	void set(float f, float q, float fs) {
		f = clampf(f, 0.01f, 0.45f * fs);
		g = std::tan(PI * f / fs);
		k = 1.f / std::max(q, 0.05f);
		a1 = 1.f / (1.f + g * (g + k));
		a2 = g * a1;
		a3 = g * a2;
	}
	void process(float x) {
		float v3 = x - ic2;
		float v1 = a1 * ic1 + a2 * v3;
		float v2 = ic2 + a2 * ic1 + a3 * v3;
		ic1 = 2.f * v1 - ic1;
		ic2 = 2.f * v2 - ic2;
		lp = v2; bp = v1; hp = x - k * v1 - v2;
	}
	float bpNorm() const { return bp * k; }   // unity gain at centre
	void reset() { ic1 = ic2 = lp = bp = hp = 0.f; }
	void scaleState(float s) { ic1 *= s; ic2 *= s; }
	void clampState(float lim) { ic1 = softClamp(ic1, lim); ic2 = softClamp(ic2, lim); }
	bool ok() const { return finite(ic1) && finite(ic2); }
};

struct OnePoleLP {
	float y = 0, c = 1;
	void set(float f, float fs) { c = 1.f - std::exp(-TWO_PI * clampf(f, 0.01f, 0.49f * fs) / fs); }
	float process(float x) { y += c * (x - y); return y; }
	void reset() { y = 0; }
};

struct OnePoleHP {
	OnePoleLP lp;
	void set(float f, float fs) { lp.set(f, fs); }
	float process(float x) { return x - lp.process(x); }
	void reset() { lp.reset(); }
};

struct DcBlock {
	float x1 = 0, y1 = 0, r = 0.999f;
	void setSampleRate(float fs) { r = std::exp(-TWO_PI * 8.f / fs); }
	float process(float x) { float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
	void reset() { x1 = y1 = 0; }
};

// First-order allpass: 90 degrees at f.
struct Allpass1 {
	float a = 0, x1 = 0, y1 = 0;
	void set(float f, float fs) {
		float t = std::tan(PI * clampf(f, 0.01f, 0.45f * fs) / fs);
		a = (t - 1.f) / (t + 1.f);
	}
	float process(float x) { float y = a * x + x1 - a * y1; x1 = x; y1 = y; return y; }
	void reset() { x1 = y1 = 0; }
};

// RBJ cookbook biquad.
struct Biquad {
	float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
	float process(float x) {
		float y = b0 * x + z1;
		z1 = b1 * x - a1 * y + z2;
		z2 = b2 * x - a2 * y;
		return y;
	}
	void reset() { z1 = z2 = 0; }
	void norm(float B0, float B1, float B2, float A0, float A1, float A2) {
		b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
	}
	void lowpass(float f, float q, float fs) {
		float w = TWO_PI * clampf(f, 1.f, 0.45f * fs) / fs, cs = std::cos(w), al = std::sin(w) / (2 * q);
		norm((1 - cs) / 2, 1 - cs, (1 - cs) / 2, 1 + al, -2 * cs, 1 - al);
	}
	void highpass(float f, float q, float fs) {
		float w = TWO_PI * clampf(f, 1.f, 0.45f * fs) / fs, cs = std::cos(w), al = std::sin(w) / (2 * q);
		norm((1 + cs) / 2, -(1 + cs), (1 + cs) / 2, 1 + al, -2 * cs, 1 - al);
	}
	void peak(float f, float q, float db, float fs) {
		float A = std::pow(10.f, db / 40.f);
		float w = TWO_PI * clampf(f, 1.f, 0.45f * fs) / fs, cs = std::cos(w), al = std::sin(w) / (2 * q);
		norm(1 + al * A, -2 * cs, 1 - al * A, 1 + al / A, -2 * cs, 1 - al / A);
	}
	void lowShelf(float f, float db, float fs) {
		float A = std::pow(10.f, db / 40.f);
		float w = TWO_PI * clampf(f, 1.f, 0.45f * fs) / fs, cs = std::cos(w), sn = std::sin(w);
		float al = sn / 2 * std::sqrt(2.f), sA = 2 * std::sqrt(A) * al;
		norm(A * ((A + 1) - (A - 1) * cs + sA), 2 * A * ((A - 1) - (A + 1) * cs), A * ((A + 1) - (A - 1) * cs - sA),
		     (A + 1) + (A - 1) * cs + sA, -2 * ((A - 1) + (A + 1) * cs), (A + 1) + (A - 1) * cs - sA);
	}
};

} // namespace tudor
