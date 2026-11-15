// OBJECT: a resonant object driven by two cone-less transducers, heard in the air and
// through a contact pickup. Bank of up to 64 complex one-pole modes with nonlinear
// coupling (TICKLE) and a loose contact (RATTLE).
#pragma once
#include "Objects.hpp"
#include "Filters.hpp"
#include <atomic>
#include <complex>

namespace tudor {

struct ObjectModel {
	static constexpr int MAXM = 64;
	static constexpr int TOP = 8;
	struct Params {
		int object = 1;            // 0..13 presets, 14 = found
		float size = 0.f;          // octaves, + = bigger = lower
		float damping = 0.5f, brightness = 0.5f, tickle = 0.25f, rattle = 0.f;
		float driveA = 1.f, pointA = 0.31f, driveB = 1.f, pointB = 0.67f;
		bool bInvert = false;
		float pickupPos = 0.45f;
		int pickupType = 0;        // 0 cartridge, 1 contact
	};
	struct Out { float air = 0, pickup = 0; float modes[TOP] = {}; };

	float fs = 48000.f;
	std::vector<ObjectDef> presets;
	ObjectDef found[2];
	std::atomic<int> foundIdx{0};
	std::atomic<int> foundGen{0};
	int usedFoundGen = 0;
	const ObjectDef* cur = nullptr;
	int curObject = -1;

	// per-mode state
	int nm = 0;
	std::complex<float> z[MAXM], p[MAXM];
	float fCur[MAXM], phA[MAXM], phB[MAXM], phP[MAXM], phMid[MAXM], phR[MAXM], rad[MAXM], energy[MAXM], norm[MAXM];
	float eSmooth = 0.f, nlMean = 0.f, prevForce = 0.f;
	float jumpRef = 0.f; bool jumpArmed = true;
	int coefCount = 0;
	float fade = 1.f; int fadeDir = 0; int pendingObject = -1;
	int topIdx[TOP] = {}; float topHold = 0.f;
	Rng rng{0x0B1EC7ull};
	// transducers and pickup voicing
	Biquad tHp[2], tPeak[2];
	OnePoleHP leakHp[2];
	Biquad cartShelf, cartLp, contactPeak, contactLp;
	OnePoleLP cartLp1; OnePoleHP contactHp;
	OnePoleHP rattleHp;
	DcBlock dcAir, dcPick, dcT[2];
	float airCal = 1.f, pickCal = 1.f;   // per-object output calibration

	ObjectModel() {
		for (int i = 0; i < OBJECT_PRESETS; i++) presets.push_back(buildPreset(i));
		found[0] = buildFound(MAT_METAL, 1);
		found[1] = found[0];
		setSampleRate(48000.f);
	}
	void setSampleRate(float sr) {
		fs = sr;
		for (int i = 0; i < 2; i++) {
			tHp[i].highpass(140.f, 0.7f, sr);
			tPeak[i].peak(3200.f, 1.f, 4.f, sr);
			leakHp[i].set(1000.f, sr);
			dcT[i].setSampleRate(sr);
		}
		cartShelf.lowShelf(150.f, 4.f, sr);
		cartLp.lowpass(3200.f, 0.8f, sr);
		cartLp1.set(6000.f, sr);
		contactHp.set(60.f, sr);
		contactPeak.peak(4500.f, 1.2f, 3.f, sr);
		contactLp.lowpass(13000.f, 0.707f, sr);
		rattleHp.set(800.f, sr);
		dcAir.setSampleRate(sr);
		dcPick.setSampleRate(sr);
		coefCount = 0;
	}
	// UI thread: build a new found object into the spare buffer, then publish it.
	void find(int material, uint64_t seed) {
		int spare = 1 - foundIdx.load();
		found[spare] = buildFound(material, seed);
		foundIdx.store(spare);
		foundGen.fetch_add(1);
	}
	const ObjectDef& def(int idx) const {
		if (idx >= 0 && idx < OBJECT_PRESETS) return presets[idx];
		return found[foundIdx.load()];
	}
	void clearState() {
		for (int k = 0; k < MAXM; k++) { z[k] = 0.f; energy[k] = 0.f; }
		eSmooth = nlMean = prevForce = 0.f;
	}
	void reset() {
		clearState();
		for (int i = 0; i < 2; i++) { tHp[i].reset(); tPeak[i].reset(); leakHp[i].reset(); dcT[i].reset(); }
		cartShelf.reset(); cartLp.reset(); cartLp1.reset(); contactHp.reset(); contactPeak.reset(); contactLp.reset();
		rattleHp.reset(); dcAir.reset(); dcPick.reset();
	}
	void load(int idx) {
		cur = &def(idx);
		curObject = idx;
		nm = std::min((int)cur->modes.size(), MAXM);
		clearState();
		coefCount = 0;
		for (int k = 0; k < TOP; k++) topIdx[k] = std::min(k, std::max(nm - 1, 0));
		// calibrate: a 5 V sine on the most responsive mode (default points) gives about 5 V
		float ma = 1e-3f, mp = 1e-3f;
		for (int k = 0; k < nm; k++) {
			const ModeDef& m = cur->modes[k];
			float a = std::fabs(shape(m, 0.31f)) * m.gain;
			ma = std::max(ma, a * m.f / (m.f + cur->fRad));
			mp = std::max(mp, a * std::fabs(shape(m, 0.45f)));
		}
		airCal = 2.f / ma;
		pickCal = 2.f / mp;
	}

	static float shape(const ModeDef& m, float x) {
		float xx = x + m.off;
		switch (m.shape) {
			case SH_LINE: return std::sin(PI * m.m * xx);
			case SH_RECT: return std::sin(PI * m.m * xx) * std::sin(PI * m.n * (0.15f + 0.7f * x));
			case SH_CIRC: return std::sin(PI * (m.m + 0.5f) * xx) * std::cos(m.n * 2.4f * x);
			default: return 1.f;
		}
	}
	float transducer(int i, float x) {
		float y = tPeak[i].process(tHp[i].process(x)) / 5.f;
		y = y + 0.06f * y * y - 0.03f * y * y * y;
		return dcT[i].process(5.f * y);
	}
	void updateCoefs(const Params& pr) {
		const ObjectDef& d = *cur;
		float sizeMul = std::pow(2.f, -clampf(pr.size, -3.f, 3.f));
		float damp = std::pow(2.f, 4.f * (0.5f - clampf(pr.damping, 0.f, 1.f)));
		float beta = 0.2f + 1.6f * (1.f - clampf(pr.brightness, 0.f, 1.f)) * d.betaScale;
		float t = clampf(pr.tickle, 0.f, 1.f);
		float glide = 1.f + std::min(0.06f, 0.04f * t * d.glide * eSmooth / 25.f);
		float fRad = d.fRad * sizeMul;
		for (int k = 0; k < nm; k++) {
			const ModeDef& m = d.modes[k];
			float f = m.f * sizeMul * glide;
			fCur[k] = f;
			if (f > 0.45f * fs || f < 15.f) { p[k] = 0.f; norm[k] = 0.f; rad[k] = 0.f; continue; }
			float t60 = clampf(d.t60 * (m.t60 / std::max(d.t60, 1e-3f)) * damp * std::pow(m.fRef / std::max(m.f, 1.f), beta), 0.01f, 30.f);
			float r = std::exp(-6.9078f / (t60 * fs));
			float w = TWO_PI * f / fs;
			p[k] = std::complex<float>(r * std::cos(w), r * std::sin(w));
			norm[k] = (1.f - r) * m.gain;
			rad[k] = f / (f + fRad);
			phA[k] = shape(m, pr.pointA);
			phB[k] = shape(m, pr.pointB);
			phP[k] = shape(m, pr.pickupPos);
			phMid[k] = shape(m, 0.5f);
			phR[k] = shape(m, 0.83f);
		}
	}

	Out process(float inA, float inB, const Params& pr) {
		Out o;
		// object changes: fade out, swap, fade in
		int gen = foundGen.load();
		bool wantFound = pr.object >= OBJECT_PRESETS;
		if (curObject < 0) load(pr.object);
		else if ((pr.object != curObject || (wantFound && gen != usedFoundGen)) && fadeDir == 0) { fadeDir = -1; pendingObject = pr.object; }
		if (fadeDir != 0) {
			fade += fadeDir / (0.010f * fs);
			if (fade <= 0.f) { fade = 0.f; load(pendingObject); usedFoundGen = gen; fadeDir = 1; }
			if (fade >= 1.f) { fade = 1.f; fadeDir = 0; }
		}
		if (coefCount-- <= 0) { coefCount = 16; updateCoefs(pr); }

		float tA = transducer(0, inA) * pr.driveA;
		float tB = transducer(1, inB) * pr.driveB * (pr.bInvert ? -1.f : 1.f);
		float t = clampf(pr.tickle, 0.f, 1.f);
		float rattleAmt = clampf(pr.rattle, 0.f, 1.f);

		// nonlinear coupling from last sample's displacement at point A
		float dA = 0.f, dR = 0.f, e = 0.f;
		for (int k = 0; k < nm; k++) { float im = z[k].imag(); dA += phA[k] * im; dR += phR[k] * im; }
		float sq = dA * dA / 5.f;
		nlMean += (sq - nlMean) * (1.f - std::exp(-1.f / (0.05f * fs)));
		float nl = 0.15f * t * (sq - nlMean);
		nl = clampf(nl, -20.f, 20.f);
		// loose contact
		float gap = 4.f * (1.f - rattleAmt) + 0.05f;
		float force = 0.f;
		if (rattleAmt > 0.f && std::fabs(dR) > gap) force = -0.8f * (std::fabs(dR) - gap) * (dR > 0 ? 1.f : -1.f);
		force = clampf(force, -20.f, 20.f);

		float air = 0.f, pick = 0.f;
		for (int k = 0; k < nm; k++) {
			float u = phA[k] * tA + phB[k] * tB + phMid[k] * nl + phR[k] * force * 40.f;
			z[k] = p[k] * z[k] + norm[k] * u;
			float im = z[k].imag();
			air += rad[k] * im;
			pick += phP[k] * im;
			float en = std::norm(z[k]);
			e += en;
			energy[k] += (en - energy[k]) * 0.001f;
		}
		eSmooth += (e - eSmooth) * (1.f - std::exp(-1.f / (0.005f * fs)));
		// threshold jump: "it caught"
		float jumpLevel = 25.f * (1.f - 0.6f * t);
		if (t > 0.f) {
			if (jumpArmed && eSmooth > jumpLevel) {
				jumpArmed = false;
				for (int j = 0; j < 4 && j < nm; j++) {
					int k = topIdx[j];
					float ang = 0.3f * t * PI * rng.bipolar();
					z[k] *= std::complex<float>(std::cos(ang), std::sin(ang));
				}
			} else if (eSmooth < 0.7f * jumpLevel) jumpArmed = true;
		}
		// keep every mode bounded
		if (e > 4.f * LIM * LIM * nm) {
			float s = std::sqrt(4.f * LIM * LIM * nm / e);
			for (int k = 0; k < nm; k++) z[k] *= s;
		}
				air = air * airCal + 0.03f * (leakHp[0].process(tA) + leakHp[1].process(tB)) + 0.5f * rattleHp.process(force);
		air = dcAir.process(air);
		if (pr.pickupType == 0) pick = cartLp1.process(cartLp.process(cartShelf.process(pick)));
		else pick = contactLp.process(contactPeak.process(contactHp.process(pick)));
		pick = dcPick.process(pick * pickCal);
		o.air = softClamp(air * fade);
		o.pickup = softClamp(pick * fade);

		// strongest modes, re-ranked every 50 ms
		topHold -= 1.f / fs;
		if (topHold <= 0.f) {
			topHold = 0.05f;
			int idx[MAXM];
			for (int k = 0; k < nm; k++) idx[k] = k;
			int n = std::min(nm, TOP);
			std::partial_sort(idx, idx + n, idx + nm, [&](int a, int b) { return energy[a] > energy[b]; });
			for (int j = 0; j < TOP; j++) topIdx[j] = j < n ? idx[j] : (n > 0 ? idx[0] : 0);
		}
		for (int j = 0; j < TOP; j++) o.modes[j] = nm > 0 ? std::log2(std::max(fCur[topIdx[j]], 1.f) / C4) : 0.f;

		if (!finite(o.air) || !finite(o.pickup) || !finite(eSmooth)) { reset(); o.air = o.pickup = 0.f; }
		return o;
	}
	const char* name() const { return cur ? cur->name.c_str() : ""; }
};

} // namespace tudor
