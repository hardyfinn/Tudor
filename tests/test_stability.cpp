// Every model: noise at +/-10 V into every input with random settings, at four sample rates.
#include "util.hpp"
#include "../src/dsp/Object.hpp"
#include "../src/dsp/Critter.hpp"
#include "../src/dsp/Seek.hpp"
#include "../src/dsp/Wander.hpp"
#include "../src/dsp/Pulser.hpp"
#include "../src/dsp/Shift.hpp"
#include "../src/dsp/Tuned.hpp"
#include "../src/dsp/Sideband.hpp"
#include "../src/dsp/Loop.hpp"
#include "../src/dsp/Swell.hpp"
#include "../src/dsp/Phoneme.hpp"
#include "../src/dsp/XGate.hpp"
#include <chrono>
using namespace tudor;

static bool okv(float v) { return std::isfinite(v) && std::fabs(v) <= 12.001f; }

TEST("Stability: all models, noise at every input, feedback, four sample rates") {
	static ObjectModel ob;
	for (float fs : {44100.f, 48000.f, 96000.f, 192000.f}) {
		Rng r(fs);
		int secs = fs > 100000 ? 2 : 4;
		int N = (int)(fs * secs);
		bool ok = true;
		{ Shift m; m.setSampleRate(fs); Shift::Params p; p.degrees = r.below(4); p.stages = 8; p.gain = 2; p.saturate = r.uniform(); float fb = 0;
		  for (int n = 0; n < N; n++) { if (n % 4800 == 0) p.phaseHz = expRange(r.uniform(), 20, 10000); fb = m.process(10 * r.bipolar() + fb, p); ok = ok && okv(fb); } }
		{ Tuned m; m.setSampleRate(fs); Tuned::Params p; p.q = 200; p.gainDb = 40; float fb = 0;
		  for (int n = 0; n < N; n++) { if (n % 4800 == 0) { p.mode = r.below(3); p.tuneHz = expRange(r.uniform(), 20, 10000); } fb = m.process(10 * r.bipolar() + fb, p); ok = ok && okv(fb); } }
		{ Sideband m; Sideband::Params p; p.carrierLeak = 1; p.signalLeak = 1; p.drive = 1; p.level = 2; float fb = 0;
		  for (int n = 0; n < N; n++) { p.sideband = r.bipolar(); auto o = m.process(10 * r.bipolar() + fb, 10 * r.bipolar(), p); fb = o.out; ok = ok && okv(o.out) && okv(o.upper) && okv(o.lower); } }
		{ Loop m; m.setSampleRate(fs); Loop::Params p; for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) p.m[i][j] = 1.5f * r.bipolar(); p.rotate = 1; p.edge = 1;
		  Loop::Io io; for (int n = 0; n < N; n++) { for (int j = 0; j < 4; j++) io.in[j] = 10 * r.bipolar(); m.process(io, p); for (int j = 0; j < 4; j++) ok = ok && okv(io.out[j]) && okv(io.send[j]); } }
		{ Pulser m(3); m.setSampleRate(fs); Pulser::Params p; p.gain = 3; p.instability = 1; p.couple = 1; float fb = 0;
		  for (int n = 0; n < N; n++) { if (n % 4800 == 0) { p.timeBase = expRange(r.uniform(), 0.25f, 1000); p.phaseDeg = 180 * r.bipolar(); p.hold = expRange(r.uniform(), 0.001f, 2); }
		    Pulser::In in; in.couple = fb; in.kick = 10 * r.bipolar(); auto o = m.process(in, p); fb = o.coupleOut;
		    ok = ok && okv(o.click) && okv(o.snap) && okv(o.thump) && okv(o.sputter) && okv(o.loop); } }
		{ ob.setSampleRate(fs); ObjectModel::Params p; p.tickle = 1; p.rattle = 1; p.damping = 0; float fb = 0;
		  for (int n = 0; n < N; n++) { if (n % 24000 == 0) { p.object = r.below(14); p.size = 2 * r.bipolar(); p.pickupType = r.below(2); }
		    auto o = ob.process(10 * r.bipolar() + fb, 10 * r.bipolar(), p); fb = o.air; ok = ok && okv(o.air) && okv(o.pickup); } }
		{ Critter m; m.setSampleRate(fs); Critter::Params p; for (auto& v : p.v) { v.rate = 20; v.warble = 1; v.chirp = 1; v.pitch = 8000; }
		  for (int n = 0; n < N; n++) { p.species = r.uniform(); auto o = m.process(p); ok = ok && okv(o.mix); } }
		{ Seek m; m.setSampleRate(fs); Seek::Params p; p.sweepTime = 2; float fb = 0;
		  for (int n = 0; n < N; n++) { if (n % (int)fs == 0) p.mode = r.below(2); auto o = m.process(10 * r.bipolar() + fb, p, n % (int)(fs * 2.5f) == 0); fb = o.probe; ok = ok && okv(o.probe) && okv(o.tease); } }
		{ Wander m; m.setSampleRate(fs); Wander::Params p; p.room = 1; p.speakers = 1; p.speed = 2; p.ear = true; float in[16]; bool con[16]; for (bool& c : con) c = true;
		  for (int n = 0; n < N; n++) { if (n % 24000 == 0) p.quad = r.below(2); for (float& v : in) v = 10 * r.bipolar(); auto o = m.process(in, con, p); ok = ok && okv(o.l) && okv(o.r) && okv(o.fl) && okv(o.rr); } }
		{ Swell m; m.setSampleRate(fs); Swell::Params p;
		  for (int n = 0; n < N; n++) { if (n % 12000 == 0) { p.source = r.below(3); p.attack = expRange(r.uniform(), 0.001f, 8); p.body = r.uniform(); }
		    auto o = m.process(10 * r.bipolar(), n % 3000 == 0, p); ok = ok && okv(o.l) && okv(o.r); } }
		{ Phoneme m; m.setSampleRate(fs); Phoneme::Params p; p.shuffle = 1; p.jitter = 1; float fb = 0;
		  for (int n = 0; n < N; n++) { if (n % 12000 == 0) { p.carrier = r.below(3); p.hold = r.below(2); p.formant = r.bipolar(); p.response = expRange(r.uniform(), 0.002f, 0.2f); }
		    auto o = m.process(10 * r.bipolar() + fb, 10 * r.bipolar(), true, 0, false, p); fb = o.out; ok = ok && okv(o.out) && okv(o.odd); } }
		{ XGate m; XGate::Params p; p.switchRate = 10; p.randomness = 1; p.glide = 0.001f; float in[4];
		  for (int n = 0; n < N; n++) { for (float& v : in) v = 10 * r.bipolar(); auto o = m.process(in, false, p); for (float v : o.out) ok = ok && okv(v); } }
		if (!ok) std::printf("  unstable at %g Hz\n", fs);
		CHECK(ok);
	}
}

TEST("Silence: processors output exactly 0 V with nothing patched") {
	const float fs = 48000;
	auto quiet = [](float v) { return v == 0.f || std::fabs(v) < 1e-20f; };
	{ Shift m; m.setSampleRate(fs); Shift::Params p; float y = 1; for (int n = 0; n < fs; n++) y = m.process(0, p); CHECK(quiet(y)); }
	{ Tuned m; m.setSampleRate(fs); Tuned::Params p; float y = 1; for (int n = 0; n < fs; n++) y = m.process(0, p); CHECK(quiet(y)); }
	{ Sideband m; Sideband::Params p; float y = 1; for (int n = 0; n < fs; n++) y = m.process(0, 0, p).out; CHECK(quiet(y)); }
	{ static ObjectModel ob; ob.setSampleRate(fs); ObjectModel::Params p; ObjectModel::Out o; for (int n = 0; n < fs; n++) o = ob.process(0, 0, p); CHECK(quiet(o.air)); CHECK(quiet(o.pickup)); }
	{ Swell m; m.setSampleRate(fs); Swell::Params p; float y = 1; for (int n = 0; n < fs; n++) y = m.process(0, false, p).l; CHECK(quiet(y)); }
	{ Phoneme m; m.setSampleRate(fs); Phoneme::Params p; p.carrier = Phoneme::SELF; float y = 1; for (int n = 0; n < fs; n++) y = m.process(0, 0, false, 0, false, p).out; CHECK(quiet(y)); }
	{ XGate m; XGate::Params p; float in[4] = {}; float y = 1; for (int n = 0; n < fs; n++) y = m.process(in, false, p).out[0]; CHECK(quiet(y)); }
	{ Wander m; m.setSampleRate(fs); Wander::Params p; float in[16] = {}; bool con[16] = {}; float y = 1; for (int n = 0; n < fs; n++) y = m.process(in, con, p).l; CHECK(quiet(y)); }
}

TEST("CPU at 48 kHz (printed)") {
	const float fs = 48000; const int N = 48000 * 2;
	auto bench = [&](const char* name, auto&& fn) {
		auto t0 = std::chrono::steady_clock::now();
		for (int n = 0; n < N; n++) fn(n);
		double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
		std::printf("  cpu %-9s %.2f%% of one core\n", name, s / 2 * 100);
	};
	Rng r(1); volatile float sink = 0;
	{ static ObjectModel ob; ob.setSampleRate(fs); ObjectModel::Params p; p.object = 11; bench("OBJECT", [&](int) { sink = ob.process(r.bipolar(), r.bipolar(), p).air; }); }
	{ Critter m; m.setSampleRate(fs); Critter::Params p; bench("CRITTER", [&](int) { sink = m.process(p).mix; }); }
	{ Seek m; m.setSampleRate(fs); Seek::Params p; p.mode = Seek::LISTEN; bench("SEEK", [&](int) { sink = m.process(r.bipolar(), p, false).tease; }); }
	{ Wander m; m.setSampleRate(fs); Wander::Params p; float in[16]; bool con[16]; for (bool& c : con) c = true; bench("WANDER", [&](int) { for (float& v : in) v = r.bipolar(); sink = m.process(in, con, p).l; }); }
	{ Pulser m(1); m.setSampleRate(fs); Pulser::Params p; bench("PULSER", [&](int) { sink = m.process({}, p).snap; }); }
	{ Shift m; m.setSampleRate(fs); Shift::Params p; p.stages = 8; bench("SHIFT", [&](int) { sink = m.process(r.bipolar(), p); }); }
	{ Tuned m; m.setSampleRate(fs); Tuned::Params p; p.mode = Tuned::HARD; bench("TUNED", [&](int) { sink = m.process(r.bipolar(), p); }); }
	{ Sideband m; Sideband::Params p; bench("SIDEBAND", [&](int) { sink = m.process(r.bipolar(), r.bipolar(), p).out; }); }
	{ Loop m; m.setSampleRate(fs); Loop::Params p; p.rotate = 1; Loop::Io io; bench("LOOP", [&](int) { m.process(io, p); sink = io.mixL; }); }
	{ Swell m; m.setSampleRate(fs); Swell::Params p; bench("SWELL", [&](int n) { sink = m.process(r.bipolar(), n % 12000 == 0, p).l; }); }
	{ Phoneme m; m.setSampleRate(fs); Phoneme::Params p; bench("PHONEME", [&](int) { sink = m.process(r.bipolar(), r.bipolar(), true, 0, false, p).out; }); }
	{ XGate m; XGate::Params p; float in[4]; bench("XGATE", [&](int) { for (float& v : in) v = r.bipolar(); sink = m.process(in, false, p).out[0]; }); }
	(void)sink;
}
