// Envelope followers, onset detection and a small real FFT.
#pragma once
#include "Common.hpp"
#include <vector>
#include <complex>

namespace tudor {

struct EnvFollow {
	float y = 0, ca = 1, cr = 1;
	void set(float attack, float release, float fs) {
		ca = 1.f - std::exp(-1.f / (std::max(attack, 1e-5f) * fs));
		cr = 1.f - std::exp(-1.f / (std::max(release, 1e-5f) * fs));
	}
	float process(float x) {
		float a = std::fabs(x);
		y += (a > y ? ca : cr) * (a - y);
		return y;
	}
	void reset() { y = 0; }
};

// RMS follower with one time constant.
struct Rms {
	float m = 0, c = 1;
	void set(float tau, float fs) { c = 1.f - std::exp(-1.f / (std::max(tau, 1e-5f) * fs)); }
	float process(float x) { m += c * (x * x - m); return std::sqrt(std::max(m, 0.f)); }
	void reset() { m = 0; }
};

struct Onset {
	EnvFollow fast, slow;
	float refractory = 0, fs = 48000.f, strength = 0;
	void setSampleRate(float sr) {
		fs = sr;
		fast.set(0.001f, 0.010f, sr);
		slow.set(0.040f, 0.300f, sr);
	}
	Onset() { setSampleRate(48000.f); }
	// returns true on an onset; strength in 0..1
	bool process(float x, float sensitivity) {
		float f = fast.process(x), s = slow.process(x);
		if (refractory > 0.f) { refractory -= 1.f / fs; return false; }
		float ratio = 1.2f + 4.f * (1.f - clampf(sensitivity, 0.f, 1.f));
		if (f > s * ratio && f > 0.02f) {
			refractory = 0.030f;
			strength = clampf(f / 5.f, 0.f, 1.f);
			return true;
		}
		return false;
	}
	void reset() { fast.reset(); slow.reset(); refractory = 0; }
};

// In-place iterative radix-2 complex FFT.
inline void fft(std::complex<float>* a, int n) {
	for (int i = 1, j = 0; i < n; i++) {
		int bit = n >> 1;
		for (; j & bit; bit >>= 1) j ^= bit;
		j ^= bit;
		if (i < j) std::swap(a[i], a[j]);
	}
	for (int len = 2; len <= n; len <<= 1) {
		float ang = -TWO_PI / len;
		std::complex<float> wl(std::cos(ang), std::sin(ang));
		for (int i = 0; i < n; i += len) {
			std::complex<float> w(1.f, 0.f);
			for (int k = 0; k < len / 2; k++) {
				std::complex<float> u = a[i + k], v = a[i + k + len / 2] * w;
				a[i + k] = u + v;
				a[i + k + len / 2] = u - v;
				w *= wl;
			}
		}
	}
}

// Magnitude spectrum of a Hann-windowed real block. Buffers allocated once.
struct Spectrum {
	int n = 0;
	std::vector<float> window, mag;
	std::vector<std::complex<float>> buf;
	void init(int size) {
		n = size;
		window.resize(n); buf.resize(n); mag.assign(n / 2, 0.f);
		for (int i = 0; i < n; i++) window[i] = 0.5f - 0.5f * std::cos(TWO_PI * i / n);
	}
	// x is a ring buffer of length n, start = index of the oldest sample
	void compute(const float* x, int start) {
		for (int i = 0; i < n; i++) buf[i] = std::complex<float>(x[(start + i) % n] * window[i], 0.f);
		fft(buf.data(), n);
		for (int i = 0; i < n / 2; i++) mag[i] = std::abs(buf[i]) * (4.f / n);
	}
};

} // namespace tudor
