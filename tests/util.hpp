#pragma once
#include "check.hpp"
#include "../src/dsp/Common.hpp"
#include <vector>
#include <cmath>

// Goertzel magnitude (amplitude) of frequency f in x.
inline double tone(const std::vector<float>& x, double f, double fs, size_t start = 0) {
	double w = 2 * M_PI * f / fs, c = 2 * std::cos(w), s1 = 0, s2 = 0;
	for (size_t i = start; i < x.size(); i++) { double s = x[i] + c * s1 - s2; s2 = s1; s1 = s; }
	double n = (double)(x.size() - start);
	double re = s1 - s2 * std::cos(w), im = s2 * std::sin(w);
	return 2 * std::sqrt(re * re + im * im) / n;
}
inline double rms(const std::vector<float>& x, size_t start = 0) {
	double s = 0; for (size_t i = start; i < x.size(); i++) s += (double)x[i] * x[i];
	return std::sqrt(s / std::max<size_t>(1, x.size() - start));
}
inline double db(double a, double b) { return 20 * std::log10(std::max(a, 1e-12) / std::max(b, 1e-12)); }
// phase of x relative to a sine of frequency f (radians), via correlation
inline double phaseOf(const std::vector<float>& x, double f, double fs, size_t start) {
	double re = 0, im = 0;
	for (size_t i = start; i < x.size(); i++) { double w = 2 * M_PI * f * i / fs; re += x[i] * std::sin(w); im += x[i] * std::cos(w); }
	return std::atan2(im, re);
}
