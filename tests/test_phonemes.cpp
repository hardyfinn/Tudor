#include "util.hpp"
#include "../src/dsp/Swell.hpp"
#include "../src/dsp/Phoneme.hpp"
#include "../src/dsp/XGate.hpp"
using namespace tudor;
static const float FS = 48000.f;

TEST("SWELL: a 4 s attack peaks 4 s after the trigger; four hits overlap") {
	Swell s; s.setSampleRate(FS); Swell::Params p; p.attack = 4.f; p.scatter = 0; p.decay = 2;
	int peakAt = 0; float best = 0;
	for (int n = 0; n < FS * 6; n++) { auto o = s.process(0.f, n == 0, p); if (o.env > best) { best = o.env; peakAt = n; } }
	CHECK_NEAR(peakAt / FS, 4.0, 0.08);
	Swell t; t.setSampleRate(FS); Swell::Params q; q.attack = 1; q.scatter = 0;
	int active = 0;
	for (int n = 0; n < FS * 1; n++) { t.process(0.f, n % 12000 == 0, q); }
	for (auto& v : t.v) if (v.active) active++;
	CHECK(active == 4);
}

TEST("SWELL: CAPTURE of a 1 kHz burst plays mostly 1 kHz; one onset per click") {
	Swell s; s.setSampleRate(FS); Swell::Params p; p.source = Swell::CAPTURE; p.attack = 0.05f; p.decay = 2; p.scatter = 0;
	std::vector<float> y;
	for (int n = 0; n < FS * 2; n++) { float x = n < 12000 ? 4 * std::sin(TWO_PI * 1000 * n / FS) : 0.f; y.push_back(s.process(x, false, p).l); }
	CHECK(tone(y, 1000, FS, 15000) > 3 * tone(y, 700, FS, 15000));
	Swell c; c.setSampleRate(FS); Swell::Params q; int onsets = 0;
	for (int n = 0; n < FS * 2; n++) { float x = (n % 12000) < 48 ? 5.f : 0.f; if (c.process(x, false, q).onset) onsets++; }
	CHECK(onsets == 8);
}

TEST("PHONEME: 10 Hz chop gives 10 bursts per second; plain mode is a vocoder") {
	Phoneme ph; ph.setSampleRate(FS); Phoneme::Params p; p.chopRate = 10; p.chopSize = 0.5f; p.jitter = 0;
	int opens = 0; bool was = false; Rng r(1);
	for (int n = 0; n < FS * 3; n++) { float x = r.gauss() * 2; auto o = ph.process(x, 0, false, 0, false, p); if (o.gateOpen && !was) opens++; was = o.gateOpen; }
	CHECK_NEAR(opens, 30, 1);
	// plain vocoder: a tone in any band comes out at the same level as in the others
	double first = 0;
	for (int b : {2, 5, 8, 11, 13}) {
		Phoneme v; v.setSampleRate(FS); Phoneme::Params q; q.chopSize = 1; q.jitter = 0; q.hold = false; q.carrier = Phoneme::SELF;
		std::vector<float> y; float f = v.fc[b];
		for (int n = 0; n < FS; n++) y.push_back(v.process(3 * std::sin(TWO_PI * f * n / FS), 0, false, 0, false, q).out);
		double g = db(tone(y, f, FS, 24000), 3.0);
		if (b == 2) first = g;
		if (std::fabs(g - first) > 2) std::printf("  band %d gain %.1f dB (band 2: %.1f)\n", b, g, first);
		CHECK(std::fabs(g - first) < 2);
	}
	// external chop overrides the internal clock
	Phoneme e; e.setSampleRate(FS); Phoneme::Params q; q.chopRate = 30; q.jitter = 0; opens = 0; was = false;
	for (int n = 0; n < FS * 2; n++) { float clk = (n % 12000) < 100 ? 10.f : 0.f; auto o = e.process(r.gauss(), 0, false, clk, true, q); if (o.gateOpen && !was) opens++; was = o.gateOpen; }
	CHECK_NEAR(opens, 8, 1);
}

TEST("XGATE: open and duck follow the key; power constant during a glide; 4-output mode") {
	XGate x; XGate::Params p; p.switchRate = 0.01f; p.randomness = 0; x.untilSwitch = 1e9f;
	float in[4] = {};
	float g = 0;
	for (int n = 0; n < FS; n++) { in[0] = std::sin(n * 0.1f); in[1] = 5 * std::sin(n * 0.03f); g = x.process(in, false, p).gate[0]; }
	CHECK(g > 0.95f);
	p.mode[0] = XGate::DUCK;
	for (int n = 0; n < FS; n++) { in[0] = std::sin(n * 0.1f); in[1] = 5 * std::sin(n * 0.03f); g = x.process(in, false, p).gate[0]; }
	CHECK(g < 0.05f);
	// constant power while channel 0 glides between outputs
	XGate y; XGate::Params q; q.mode[0] = XGate::OFF; q.glide = 0.5f; q.randomness = 0; y.untilSwitch = 1e9f;
	float in2[4] = {2.f, 0, 0, 0};
	float lo = 1e9f, hi = 0;
	y.from[0] = 0; y.dest[0] = 3; y.xf[0] = 0.f;
	for (int n = 0; n < FS * 0.5f; n++) {
		auto o = y.process(in2, false, q);
		float pw = 0; for (int k = 0; k < 6; k++) pw += o.out[k] * o.out[k];
		lo = std::min(lo, pw); hi = std::max(hi, pw);
	}
	CHECK(db(std::sqrt(hi), std::sqrt(lo)) < 0.5);
	XGate z; XGate::Params r; r.outputs = 4; r.switchRate = 20; r.randomness = 1;
	float in3[4] = {1, 1, 1, 1}; float mx56 = 0;
	for (int n = 0; n < FS * 2; n++) { auto o = z.process(in3, false, r); mx56 = std::max(mx56, std::max(std::fabs(o.out[4]), std::fabs(o.out[5]))); }
	CHECK(mx56 == 0.f);
}
