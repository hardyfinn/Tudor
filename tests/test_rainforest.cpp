#include "util.hpp"
#include "../src/dsp/Object.hpp"
#include "../src/dsp/Critter.hpp"
#include "../src/dsp/Seek.hpp"
#include "../src/dsp/Wander.hpp"
using namespace tudor;
static const float FS = 48000.f;

static ObjectModel& sharedObject() { static ObjectModel o; return o; }

TEST("OBJECT: every preset responds at its modes more than between them") {
	ObjectModel& ob = sharedObject(); ob.setSampleRate(FS);
	for (int pre = 0; pre < OBJECT_PRESETS; pre++) {
		ObjectModel::Params p; p.object = pre; p.tickle = 0; p.rattle = 0;
		ob.reset(); ob.load(pre);
		// pick a mode well above the transducer high-pass, with a non-zero shape at point A
		int k = -1;
		for (int i = 0; i < ob.nm; i++) if (ob.cur->modes[i].f > 300 && std::fabs(ObjectModel::shape(ob.cur->modes[i], p.pointA)) > 0.5f) { k = i; break; }
		if (k < 0) continue;
		float f = ob.cur->modes[k].f;
		auto resp = [&](float g) { ob.load(pre); std::vector<float> y; for (int n = 0; n < FS; n++) y.push_back(ob.process(std::sin(TWO_PI * g * n / FS), 0, p).air); return tone(y, g, FS, 24000); };
		double on = resp(f);
		// halfway (in log frequency) to the neighbouring mode
		if (ob.cur->t60 < 0.2f || pre == 0) continue;   // styrofoam is too damped, the bedspring too dense, for separate peaks
		int j = k + 1;
		while (j < ob.nm && ob.cur->modes[j].f < f * 1.05f) j++;
		float g = j < ob.nm ? std::sqrt(f * ob.cur->modes[j].f) : f * 1.1f;
		double off = resp(g);
		if (!(on > off * 1.5)) std::printf("  preset %d (%s): on %.4f off %.4f\n", pre, OBJECT_NAMES[pre], on, off);
		CHECK(on > off * 1.5);
	}
}

TEST("OBJECT: a mode at a pickup node is quiet in PICKUP but present in AIR") {
	ObjectModel& ob = sharedObject(); ob.setSampleRate(FS);
	ObjectModel::Params p; p.object = 10; p.tickle = 0; p.rattle = 0; p.pickupPos = 0.5f; p.pickupType = 1;
	ob.reset(); ob.load(10);
	// glass plate: a mode with m = 2 has a node at x = 0.5; take one with no close neighbours
	int k = -1;
	for (int i = 1; i + 1 < ob.nm && k < 0; i++) {
		const ModeDef& m = ob.cur->modes[i];
		if (m.m == 2.f && m.f > 400 && ob.cur->modes[i - 1].f < m.f / 1.06f && ob.cur->modes[i + 1].f > m.f * 1.06f) k = i;
	}
	CHECK(k >= 0);
	if (k < 0) return;
	float f = ob.cur->modes[k].f;
	std::vector<float> air, pick;
	for (int n = 0; n < FS * 2; n++) { auto o = ob.process(5 * std::sin(TWO_PI * f * n / FS), 0, p); air.push_back(o.air); pick.push_back(o.pickup); }
	double a = tone(air, f, FS, 48000), pk = tone(pick, f, FS, 48000);
	CHECK(a > 0.05);
	if (!(db(a, pk) > 15)) std::printf("  air %.4f pickup %.4f\n", a, pk);
	CHECK(db(a, pk) > 15);
}

TEST("OBJECT: B PHASE OUT cancels identical drive at the same point") {
	ObjectModel& ob = sharedObject(); ob.setSampleRate(FS);
	ObjectModel::Params p; p.object = 10; p.tickle = 0; p.rattle = 0; p.pointB = p.pointA; p.bInvert = true;
	ob.reset(); ob.load(10); Rng r(4); std::vector<float> y, ref;
	for (int n = 0; n < FS; n++) { float x = r.gauss(); y.push_back(ob.process(x, x, p).air); }
	p.bInvert = false; ob.reset(); ob.load(10); Rng r2(4);
	for (int n = 0; n < FS; n++) { float x = r2.gauss(); ref.push_back(ob.process(x, 0, p).air); }
	CHECK(db(rms(y, 4800), rms(ref, 4800)) < -60);
}

TEST("OBJECT: MODES reports a driven mode as strongest; stress stays bounded") {
	ObjectModel& ob = sharedObject(); ob.setSampleRate(FS);
	ObjectModel::Params p; p.object = 3; p.tickle = 0;
	ob.load(3);
	int k = 6; float f = ob.cur->modes[k].f;
	ObjectModel::Out o;
	for (int n = 0; n < FS; n++) o = ob.process(5 * std::sin(TWO_PI * f * n / FS), 0, p);
	CHECK_NEAR(o.modes[0], std::log2(f / C4), 0.01);
	p.tickle = 1; p.rattle = 1; Rng r(9); float mx = 0; bool ok = true;
	for (int n = 0; n < FS * 20; n++) { auto q = ob.process(10 * r.bipolar(), 10 * r.bipolar(), p); mx = std::max(mx, std::fabs(q.air)); ok = ok && std::isfinite(q.air) && std::isfinite(q.pickup); }
	CHECK(ok); CHECK(mx <= 12.f);
}

TEST("OBJECT: FIND builds a new object of every material") {
	for (int m = 0; m < MAT_COUNT; m++) {
		ObjectDef d = buildFound(m, 1234 + m);
		CHECK(d.modes.size() >= 4 && d.modes.size() <= 64);
		for (auto& md : d.modes) { CHECK(std::isfinite(md.f)); CHECK(md.f > 0); }
	}
}

TEST("CRITTER: call rate follows RATE; FOLLOW converges") {
	Critter c; c.setSampleRate(FS); Critter::Params p;
	for (auto& v : p.v) { v.rate = 4; v.density = 1; }
	for (int n = 0; n < FS * 60; n++) c.process(p);
	CHECK_NEAR(c.st[0].calls / 60.0, 4.0, 0.6);
	Critter d; d.setSampleRate(FS); Critter::Params q; q.follow = 1;
	float pts[3] = {std::log2(500.f / C4), std::log2(1500.f / C4), std::log2(3000.f / C4)};
	q.followPitches = pts; q.followCount = 3;
	for (int n = 0; n < FS * 5; n++) d.process(q);
	int hits = 0;
	for (int i = 0; i < 4; i++) for (float t : pts) if (std::fabs(d.st[i].followPitch - t) < 10.f / 1200.f) hits++;
	CHECK(hits >= 3);
}

// A synthetic 3-mode object for SEEK.
struct ThreeModes {
	Svf s[3];
	ThreeModes() { s[0].set(220, 40, FS); s[1].set(517, 40, FS); s[2].set(1430, 40, FS); }
	float process(float x) { float y = 0; float g[3] = {1.f, 0.7f, 0.45f}; for (int i = 0; i < 3; i++) { s[i].process(x); y += g[i] * s[i].bpNorm(); } return y; }
};

TEST("SEEK: SWEEP finds three resonances in strength order") {
	Seek sk; sk.setSampleRate(FS); Seek::Params p; p.count = 3; p.sweepTime = 6;
	ThreeModes obj; float ret = 0; bool found = false;
	for (int n = 0; n < FS * 7; n++) { auto o = sk.process(ret, p, n == 0); ret = obj.process(o.probe); found = found || o.found; }
	CHECK(found); CHECK(sk.nNodes == 3);
	float want[3] = {220, 517, 1430};
	for (int i = 0; i < 3 && i < sk.nNodes; i++) CHECK_NEAR(24 * std::log2(sk.nodeHz[i] / want[i]), 0.0, 1.0);
}

TEST("SEEK: LISTEN finds the resonances from noise; TEASE sits on nodes") {
	Seek sk; sk.setSampleRate(FS); Seek::Params p; p.mode = Seek::LISTEN; p.count = 3; p.drift = 0.5f;
	ThreeModes obj; Rng r(2); Pink pk;
	for (int n = 0; n < FS * 5; n++) sk.process(obj.process(pk.process(r.gauss()) * 5), p, false);
	CHECK(sk.nNodes == 3);
	float want[3] = {220, 517, 1430};
	for (float w : want) { bool hit = false; for (int i = 0; i < sk.nNodes; i++) if (std::fabs(24 * std::log2(sk.nodeHz[i] / w)) < 1.5) hit = true; CHECK(hit); }
	// tease frequency while lingering is one of the nodes
	int lingering = 0, onNode = 0;
	for (int n = 0; n < FS * 4; n++) {
		sk.process(0.f, p, false);
		if (!sk.gliding && sk.teaseNode >= 0) { lingering++; float f = C4 * std::pow(2.f, sk.teasePitch); if (std::fabs(1200 * std::log2(f / sk.nodeHz[sk.teaseNode])) < 1) onNode++; }
	}
	CHECK(lingering > 0); CHECK(onNode == lingering);
}

TEST("WANDER: 6 dB per doubling of distance, delay d/343, panning flips") {
	auto measure = [](float lx, float ly, float& l, float& r, float& delay) {
		Wander w; w.setSampleRate(FS);
		for (int i = 0; i < Wander::N; i++) { w.objX[i] = 0.5f; w.objY[i] = 0.9f; }
		w.objX[0] = 0.5f; w.objY[0] = 0.5f; w.placeSpeakers();
		Wander::Params p; p.xyPatched = true; p.x = lx; p.y = ly; p.room = 0; p.speakers = 0; p.size = 20;
		w.lx = lx; w.ly = ly;
		float in[Wander::SRC] = {}; bool con[Wander::SRC] = {}; con[0] = true;
		std::vector<float> L, R; int first = -1;
		for (int n = 0; n < FS; n++) {
			in[0] = n == 24000 ? 1.f : (n > 30000 ? std::sin(TWO_PI * 500 * n / FS) : 0.f);
			auto o = w.process(in, con, p);
			if (n >= 24000 && first < 0 && (std::fabs(o.l) > 1e-3f || std::fabs(o.r) > 1e-3f)) first = n - 24000;
			if (n > 36000) { L.push_back(o.l); R.push_back(o.r); }
		}
		l = (float)tone(L, 500, FS); r = (float)tone(R, 500, FS); delay = (float)first;
	};
	float l1, r1, d1, l2, r2, d2, l3, r3, d3;
	measure(0.5f + 2.f / 20, 0.5f, l1, r1, d1);   // 2 m east of the object
	measure(0.5f + 4.f / 20, 0.5f, l2, r2, d2);   // 4 m east
	float l4, r4, d4;
	measure(0.5f, 0.5f - 2.f / 20, l3, r3, d3);   // 2 m south: object on the left (facing east)
	measure(0.5f, 0.5f + 2.f / 20, l4, r4, d4);   // 2 m north: object on the right
	CHECK_NEAR(db(std::hypot(l1, r1), std::hypot(l2, r2)), 6.0, 1.0);
	CHECK_NEAR(d1, 2.f / 343 * FS, 30);
	CHECK(l3 > r3 * 1.5f); CHECK(r4 > l4 * 1.5f);
}
