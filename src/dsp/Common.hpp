// Shared helpers for the Tudor models. Plain C++17, no Rack includes.
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace tudor {

constexpr float PI = 3.14159265358979323846f;
constexpr float TWO_PI = 6.28318530717958647692f;
constexpr float C4 = 261.6256f;
constexpr float LIM = 12.f;   // every output and growing state is soft-clamped to this

inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float dbToGain(float db) { return std::pow(10.f, db / 20.f); }
inline float gainToDb(float g) { return 20.f * std::log10(std::max(g, 1e-9f)); }
inline float expRange(float t, float lo, float hi) { return lo * std::pow(hi / lo, clampf(t, 0.f, 1.f)); }
inline float expNorm(float v, float lo, float hi) { return std::log(v / lo) / std::log(hi / lo); }
inline bool finite(float x) { return std::isfinite(x); }
inline float sanitize(float x) { return std::isfinite(x) ? x : 0.f; }

// Rational tanh, |err| < 1e-4 for |x| < 5, exact clamp beyond.
inline float fastTanh(float x) {
	if (x > 5.f) return 1.f;
	if (x < -5.f) return -1.f;
	float x2 = x * x;
	float a = x * (135135.f + x2 * (17325.f + x2 * (378.f + x2)));
	float b = 135135.f + x2 * (62370.f + x2 * (3150.f + x2 * 28.f));
	return clampf(a / b, -1.f, 1.f);
}

// Linear to 0.9*lim, smooth knee up to lim.
inline float softClamp(float x, float lim = LIM) {
	float k = 0.9f * lim, r = 0.1f * lim;
	float a = std::fabs(x);
	if (a <= k) return x;
	float y = k + r * std::tanh((a - k) / r);
	return x < 0.f ? -y : y;
}

// One-pole parameter smoother (default 10 ms).
struct Smoother {
	float y = 0.f;
	bool init = false;
	float process(float x, float dt, float tau = 0.01f) {
		if (!init) { y = x; init = true; return y; }
		y += (x - y) * (1.f - std::exp(-dt / tau));
		return y;
	}
	void reset(float v) { y = v; init = true; }
};

// Rising-edge detector, Rack thresholds: fires above 1 V, re-arms below 0.1 V.
struct Trigger {
	bool high = false;
	bool process(float v) {
		if (high) { if (v <= 0.1f) high = false; return false; }
		if (v >= 1.f) { high = true; return true; }
		return false;
	}
};

// Fixed-length pulse.
struct Pulse {
	float t = 0.f;
	void fire(float len) { t = std::max(t, len); }
	bool process(float dt) { if (t > 0.f) { t -= dt; return true; } return false; }
};

// Equal-power crossfade weights.
inline void eqPower(float t, float& a, float& b) {
	t = clampf(t, 0.f, 1.f);
	a = std::cos(t * PI * 0.5f);
	b = std::sin(t * PI * 0.5f);
}

inline float smoothstep(float e0, float e1, float x) {
	float t = clampf((x - e0) / (e1 - e0), 0.f, 1.f);
	return t * t * (3.f - 2.f * t);
}

} // namespace tudor
