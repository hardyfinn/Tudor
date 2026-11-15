#include "util.hpp"
#include "../src/dsp/Hilbert.hpp"
#include "../src/dsp/Filters.hpp"
#include "../src/dsp/Analysis.hpp"
using namespace tudor;

TEST("Hilbert pair is 90 degrees apart from 50 Hz to 15 kHz") {
	for (double f : {50.0, 200.0, 1000.0, 5000.0, 15000.0}) {
		Hilbert h; std::vector<float> i, q;
		for (int n = 0; n < 48000; n++) { float a, b; h.process(std::sin(2 * M_PI * f * n / 48000.0), a, b); i.push_back(a); q.push_back(b); }
		double d = std::remainder(phaseOf(q, f, 48000, 4800) - phaseOf(i, f, 48000, 4800), 2 * M_PI);
		CHECK_NEAR(std::fabs(d) * 180 / M_PI, 90.0, 2.0);
		CHECK_NEAR(tone(i, f, 48000, 4800), 1.0, 0.02);
		CHECK_NEAR(tone(q, f, 48000, 4800), 1.0, 0.02);
	}
}

TEST("SVF band-pass peaks at its centre") {
	for (double fs : {44100.0, 48000.0, 96000.0, 192000.0})
		for (double fc : {100.0, 1000.0, 5000.0}) {
			Svf s; s.set((float)fc, 10.f, (float)fs);
			auto resp = [&](double f) { s.reset(); std::vector<float> y; for (int n = 0; n < (int)fs / 2; n++) { s.process(std::sin(2 * M_PI * f * n / fs)); y.push_back(s.bpNorm()); } return tone(y, f, fs, (size_t)fs / 4); };
			CHECK_NEAR(resp(fc), 1.0, 0.02);
			CHECK(resp(fc * 1.05) < resp(fc));
			CHECK(resp(fc / 1.05) < resp(fc));
		}
}

TEST("FFT finds a sine's bin") {
	Spectrum sp; sp.init(1024);
	std::vector<float> x(1024);
	for (int i = 0; i < 1024; i++) x[i] = std::sin(2 * M_PI * 100 * i / 1024.0);
	sp.compute(x.data(), 0);
	int best = 0; for (int i = 1; i < 512; i++) if (sp.mag[i] > sp.mag[best]) best = i;
	CHECK(best == 100);
}
