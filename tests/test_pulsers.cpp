#include "util.hpp"
#include "../src/dsp/Shift.hpp"
#include "../src/dsp/Tuned.hpp"
#include "../src/dsp/Sideband.hpp"
#include "../src/dsp/Loop.hpp"
#include "../src/dsp/Pulser.hpp"
using namespace tudor;
static const float FS = 48000.f;

TEST("SHIFT: DEGREES 90 with no stages shifts by 90 degrees, response flat") {
	Shift s, s0; s.setSampleRate(FS); s0.setSampleRate(FS);
	Shift::Params p, p0; p.stages = 0; p.degrees = 1; p0.stages = 0;
	std::vector<float> y, x;
	for (int n = 0; n < 48000; n++) { float v = std::sin(2 * M_PI * 1000 * n / FS); x.push_back(s0.process(v, p0)); y.push_back(s.process(v, p)); }
	double d = std::remainder(phaseOf(y, 1000, FS, 4800) - phaseOf(x, 1000, FS, 4800), 2 * M_PI);
	CHECK_NEAR(std::fabs(d) * 180 / M_PI, 90.0, 2.0);
	for (double f : {40.0, 500.0, 5000.0, 15000.0}) {
		Shift t; t.setSampleRate(FS); Shift::Params q; q.stages = 4;
		std::vector<float> z; for (int n = 0; n < 48000; n++) z.push_back(t.process(std::sin(2 * M_PI * f * n / FS), q));
		CHECK_NEAR(db(tone(z, f, FS, 9600), 1.0), 0.0, 0.3);
	}
}

TEST("SHIFT patched into itself stays bounded") {
	Shift s; s.setSampleRate(FS); Shift::Params p; p.gain = 1.2f; p.stages = 2;
	float y = 0.01f, mx = 0;
	for (int n = 0; n < 48000 * 5; n++) { y = s.process(y, p); mx = std::max(mx, std::fabs(y)); }
	CHECK(std::isfinite(y)); CHECK(mx <= 12.f);
}

TEST("TUNED: centre frequency and bounded self-feedback") {
	for (float f : {100.f, 1000.f, 5000.f}) {
		Tuned t; t.setSampleRate(FS); Tuned::Params p; p.tuneHz = f; p.q = 30; p.mode = Tuned::CLEAN; p.level = 1;
		auto resp = [&](double g) { t.reset(); std::vector<float> y; for (int n = 0; n < 48000; n++) y.push_back(t.process(0.1f * std::sin(2 * M_PI * g * n / FS), p)); return tone(y, g, FS, 24000); };
		double c = resp(f);
		CHECK(resp(f * 1.01) < c); CHECK(resp(f / 1.01) < c);
	}
	for (int m = 0; m < 3; m++) {
		Tuned t; t.setSampleRate(FS); Tuned::Params p; p.tuneHz = 440; p.q = 50; p.gainDb = 20; p.mode = m; p.level = 1;
		float y = 0.01f, mx = 0;
		std::vector<float> rec;
		for (int n = 0; n < 48000 * 2; n++) { y = t.process(y, p); mx = std::max(mx, std::fabs(y)); rec.push_back(y); }
		CHECK(std::isfinite(y)); CHECK(mx <= 12.f); CHECK(rms(rec, 48000) > 0.1);
	}
}

TEST("SIDEBAND: upper, lower and both") {
	auto run = [](float s, float& up, float& lo) {
		Sideband sb; Sideband::Params p; p.sideband = s;
		std::vector<float> y;
		for (int n = 0; n < 48000; n++) y.push_back(sb.process(5 * std::sin(2 * M_PI * 200 * n / FS), 5 * std::sin(2 * M_PI * 1000 * n / FS), p).out);
		up = (float)tone(y, 1200, FS, 4800); lo = (float)tone(y, 800, FS, 4800);
	};
	float u, l;
	run(1.f, u, l); CHECK(db(u, l) > 35);
	run(-1.f, u, l); CHECK(db(l, u) > 35);
	run(0.f, u, l); CHECK_NEAR(db(u, l), 0.0, 0.5);
	Sideband sb; Sideband::Params p; float mx = 0;
	for (int n = 0; n < 4800; n++) mx = std::max(mx, std::fabs(sb.process(5 * std::sin(n * 0.05f), 0.f, p).out));
	CHECK(mx < 1e-6f);
}

TEST("LOOP: self-starts and the keeper holds the bus near target") {
	Loop L; Loop::Params p; p.m[0][1] = p.m[1][2] = p.m[2][3] = p.m[3][0] = 0.7f;
	// the keeper holds the (geometric) mean of the bus's 30 ms level at the target; the loop breathes around it
	Loop::Io io; double logSum = 0; int cnt = 0;
	for (int n = 0; n < 48000 * 15; n++) { L.process(io, p); if (n > 48000 * 10) { logSum += std::log(std::max(1e-6, std::sqrt((double)L.rms[0].m))); cnt++; } }
	CHECK_NEAR(db(std::exp(logSum / cnt), Loop::target(p.edge)), 0.0, 4.0);
	Loop M; Loop::Params q; for (int i = 0; i < 4; i++) { q.keep[i] = false; for (int j = 0; j < 4; j++) q.m[i][j] = 1.5f; }
	Loop::Io jo; float mx = 0;
	for (int n = 0; n < 48000 * 2; n++) { M.process(jo, q); for (int j = 0; j < 4; j++) mx = std::max(mx, std::fabs(jo.out[j])); }
	CHECK(mx <= 12.f);
	// a NaN in a return resets cleanly
	jo.retPatched[0] = true; jo.ret[0] = NAN; M.process(jo, q); jo.ret[0] = 0; M.process(jo, q);
	CHECK(std::isfinite(jo.out[0]));
}

static double meanInterval(Pulser::Params p, double secs, int* count = nullptr) {
	Pulser pu(5); pu.setSampleRate(FS);
	int n = 0; double first = -1, last = 0;
	for (int i = 0; i < FS * secs; i++) if (pu.process({}, p).fired) { n++; double t = i / FS; if (first < 0) first = t; last = t; }
	if (count) *count = n;
	return n > 1 ? (last - first) / (n - 1) : 1e9;
}

TEST("PULSER: no firing at gain 0; default rhythm; time base and phase move it") {
	Pulser::Params z; z.gain = 0; int n = 0; meanInterval(z, 10, &n); CHECK(n == 0);
	Pulser::Params d; d.instability = 0;
	double m = meanInterval(d, 60);
	CHECK(m > 0.1 && m < 1.0);
	Pulser::Params d2 = d; d2.timeBase *= 2;
	double m2 = meanInterval(d2, 60);
	CHECK(m2 < m * 0.75 && m2 > m * 0.3);
	double lo = 1e9, hi = 0;
	for (float ph : {-150.f, -90.f, 0.f, 90.f, 150.f}) { Pulser::Params q = d; q.phaseDeg = ph; double v = meanInterval(q, 30); lo = std::min(lo, v); hi = std::max(hi, v); }
	CHECK(hi > lo * 1.5);
}

TEST("PULSER: coupled cells influence each other") {
	Pulser a(1), b(2); a.setSampleRate(FS); b.setSampleRate(FS);
	Pulser::Params pa, pb; pa.timeBase = 6; pb.timeBase = 7.3f; pb.couple = 1.f;
	Pulser c(2); c.setSampleRate(FS); Pulser::Params pc = pb; pc.couple = 0;
	Pulser a2(1); a2.setSampleRate(FS);
	int diff = 0;
	for (int i = 0; i < FS * 20; i++) {
		auto oa = a.process({}, pa);
		Pulser::In ib; ib.couple = oa.coupleOut;
		auto ob = b.process(ib, pb);
		auto oc = c.process({}, pc);
		if (ob.fired != oc.fired) diff++;
	}
	CHECK(diff > 10);
}
