// PULSER: rhythm from phase-shifted feedback. A resonant loop builds up from its own
// noise floor, fires when it crosses a threshold, is quenched, and builds again.
#pragma once
#include "Filters.hpp"
#include "Noise.hpp"
#include "Oversample.hpp"

namespace tudor {

struct Pulser {
	struct Params {
		float timeBase = 6.f;     // Hz, loop resonance
		float gain = 2.0f;        // 0..3
		float phaseDeg = 0.f;     // -180..180
		float threshold = 2.5f;   // V
		float hold = 0.04f;       // s
		float quench = 0.85f;     // 0..1
		float instability = 0.15f;
		float accent = 0.5f;
		float couple = 0.f;       // -1..1
	};
	struct In { float couple = 0, kick = 0; bool fire = false; };
	struct Out { float click = 0, snap = 0, thump = 0, sputter = 0, loop = 0, coupleOut = 0, gate = 0; bool fired = false; };

	static constexpr float Q = 1.5f;
	float fs = 48000.f;
	Svf res;
	Allpass1 a1, a2;
	Rng rng;
	Drift drTime{11}, drThresh{23};
	float loopPrev = 0.f;
	float q = 1.f;           // quench multiplier
	float holdLeft = 0.f, recoverLeft = 0.f, recoverTime = 0.f;
	float level = 1.f;
	int polarity = 1;
	// output voices
	float clickT = 1e9f; float clickLen = 0.0003f; float clickAmp = 0.f;
	Svf snap; float snapF = 2200.f;
	float thumpPhase = 0.f, thumpT = 1e9f, thumpAmp = 0.f, thumpF = 55.f;
	float sputEnv = 0.f, sputDecay = 0.f;
	Up2 up; Down2 down;
	DcBlock dcs[4];
	int coefCount = 0;
	float fcCur = 4.f;
	float rotC = 1.f, rotS = 0.f;
	static constexpr float PHASE_SCALE = 0.5f;   // knob +/-180 -> applied +/-90 deg (see DECISIONS)

	explicit Pulser(uint64_t seed = 1) : rng(seed), drTime(seed * 31 + 11), drThresh(seed * 17 + 23) { setSampleRate(48000.f); }
	void setSampleRate(float sr) {
		fs = sr;
		drTime.setSampleRate(sr);
		drThresh.setSampleRate(sr);
		for (auto& d : dcs) d.setSampleRate(sr);
		coefCount = 0;
	}
	void reset() {
		res.reset(); a1.reset(); a2.reset(); snap.reset();
		loopPrev = 0; q = 1; holdLeft = recoverLeft = 0; sputEnv = 0;
		up.reset(); down.reset();
		for (auto& d : dcs) d.reset();
	}

	Out process(const In& in, const Params& p) {
		Out o;
		float dt = 1.f / fs;
		float inst = clampf(p.instability, 0.f, 1.f);
		float dTime = drTime.next(), dThr = drThresh.next();
		if (coefCount-- <= 0) {
			coefCount = 8;
			fcCur = clampf(p.timeBase * std::pow(2.f, inst * 0.5f * dTime), 0.05f, 0.4f * fs);
			res.set(fcCur, Q, fs);
			// narrow-band quadrature pair around TIME BASE: a1 lags 45 deg, a2 lags 135 deg at fc
			a1.set(fcCur * 2.4142f, fs);
			a2.set(fcCur / 2.4142f, fs);
			float a = (clampf(p.phaseDeg, -180.f, 180.f) * PHASE_SCALE - 45.f) * PI / 180.f;
			rotC = std::cos(a);
			rotS = std::sin(a);
		}
		// quench envelope
		if (holdLeft > 0.f) {
			holdLeft -= dt;
			if (holdLeft <= 0.f) { recoverTime = std::max(p.hold * 0.25f, 0.001f); recoverLeft = recoverTime; }
		} else if (recoverLeft > 0.f) {
			recoverLeft -= dt;
			q = lerp(1.f - p.quench, 1.f, 1.f - std::max(recoverLeft, 0.f) / recoverTime);
		} else {
			q = 1.f;
		}
		float noise = rng.gauss() * (0.003f + 0.01f * inst * inst);
		float x = loopPrev * p.gain * q + p.couple * in.couple / 5.f + in.kick / 5.f + noise;
		res.process(x);
		float bp = res.bpNorm();
		float y = rotC * a1.process(bp) + rotS * a2.process(bp);   // phase-shifted feedback, PHASE 0 = aligned
		float loop = 6.f * fastTanh(y / 6.f);
		res.clampState(2.f * LIM);
		loopPrev = loop;

		float thr = std::max(0.05f, p.threshold * (1.f + 0.3f * inst * dThr));
		bool fire = in.fire || (holdLeft <= 0.f && recoverLeft <= 0.f && std::fabs(loop) > thr);
		if (fire) {
			o.fired = true;
			float over = clampf(std::fabs(loop) / thr, 1.f, 4.f);
			level = 1.f - p.accent + p.accent * (over - 1.f) / 3.f;
			level = clampf(level, 0.05f, 1.f);
			holdLeft = p.hold;
			q = 1.f - p.quench;
			res.scaleState(1.f - p.quench);
			if (p.accent > 0.5f) polarity = -polarity; else polarity = 1;
			clickT = 0.f; clickAmp = 5.f * level * polarity;
			// snap: a short ring, pitch nudged by loop sign and level
			snapF = 2200.f * std::pow(2.f, 0.5f * (loop > 0 ? 1.f : -1.f) * level);
			float snapDecay = 0.025f + 0.035f * level;
			snap.set(snapF, PI * snapF * snapDecay / 6.9f, fs);
			snap.process(60.f * level);
			thumpF = std::min(160.f, 55.f * std::pow(2.f, std::log2(std::max(p.timeBase, 0.25f)) / 4.f));
			thumpT = 0.f; thumpAmp = 5.f * level; thumpPhase = 0.f;
			sputEnv = level;
			sputDecay = std::max(p.hold, 0.005f);
		} else {
			snap.process(0.f);
		}
		o.gate = holdLeft > 0.f ? 10.f : 0.f;

		// CLICK: 0.3 ms raised cosine
		if (clickT < clickLen) {
			o.click = clickAmp * 0.5f * (1.f - std::cos(TWO_PI * clickT / clickLen));
			clickT += dt;
		}
		o.snap = 5.f * snap.bpNorm();
		// THUMP: sine with a pitch drop from x2.5 to x1 over 30 ms, decay 180 ms
		if (thumpT < 1.5f) {
			float f = thumpF * (1.f + 1.5f * std::exp(-thumpT / 0.010f));
			thumpPhase += f * dt;
			thumpPhase -= std::floor(thumpPhase);
			o.thump = thumpAmp * std::exp(-thumpT / 0.18f) * std::sin(TWO_PI * thumpPhase) * std::min(1.f, thumpT / 0.0007f);
			thumpT += dt;
		}
		// SPUTTER: hard-clipped loop plus noise, x2 oversampled, gated by a decaying envelope
		{
			float s = 3.f * loop + 2.f * rng.gauss() * inst;
			float a, b;
			up.process(s, a, b);
			a = clampf(a, -5.f, 5.f); b = clampf(b, -5.f, 5.f);
			o.sputter = down.process(a, b) * sputEnv;
			sputEnv *= std::exp(-dt / sputDecay);
		}
		o.click = dcs[0].process(o.click) + 0.f;
		o.snap = softClamp(dcs[1].process(o.snap));
		o.thump = softClamp(dcs[2].process(o.thump));
		o.sputter = softClamp(dcs[3].process(o.sputter));
		o.loop = softClamp(loop * 0.8f);
		o.coupleOut = loop;
		if (!finite(loop) || !finite(o.snap) || !finite(o.sputter) || !res.ok()) { reset(); o = Out(); }
		return o;
	}
};

} // namespace tudor
