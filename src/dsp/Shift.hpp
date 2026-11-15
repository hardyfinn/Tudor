// SHIFT: variable phase-shift amplifier (switchable fixed shift + all-pass chain).
#pragma once
#include "Filters.hpp"
#include "Hilbert.hpp"

namespace tudor {

struct Shift {
	struct Params {
		int degrees = 0;      // 0..3 -> 0, 90, 180, 270
		float phaseHz = 600.f;
		int stages = 2;       // 0, 1, 2, 4, 8
		float spreadOct = 0.5f;
		float gain = 1.f;     // -2..2
		float saturate = 0.f; // 0..1
	};
	float fs = 48000.f;
	Hilbert hil;
	Allpass1 ap[8];
	int curDeg = 0, prevDeg = 0;
	float xfade = 1.f;   // 1 = done
	int coefCount = 0;

	void setSampleRate(float sr) { fs = sr; coefCount = 0; }
	void reset() { hil.reset(); for (auto& a : ap) a.reset(); }

	static float pick(int deg, float i, float q) {
		switch (deg & 3) { case 0: return i; case 1: return q; case 2: return -i; default: return -q; }
	}
	float process(float x, const Params& p) {
		if (p.degrees != curDeg) { prevDeg = curDeg; curDeg = p.degrees; xfade = 0.f; }
		if (coefCount-- <= 0) {
			coefCount = 16;
			for (int k = 0; k < p.stages && k < 8; k++) {
				float off = p.stages > 1 ? ((float)k / (p.stages - 1) - 0.5f) * p.spreadOct : 0.f;
				ap[k].set(p.phaseHz * std::pow(2.f, off), fs);
			}
		}
		float i, q;
		hil.process(x, i, q);
		float y = pick(curDeg, i, q);
		if (xfade < 1.f) {
			xfade = std::min(1.f, xfade + 1.f / (0.005f * fs));
			y = lerp(pick(prevDeg, i, q), y, xfade);
		}
		for (int k = 0; k < p.stages && k < 8; k++) y = ap[k].process(y);
		y *= p.gain;
		if (p.saturate > 0.f) {
			float drive = 1.f + 9.f * p.saturate;
			y = 5.f * fastTanh(y * drive / 5.f) * (1.f + 0.4f * p.saturate) / (1.f + 1.4f * p.saturate);
		}
		y = softClamp(y);
		if (!finite(y)) { reset(); y = 0.f; }
		return y;
	}
};

} // namespace tudor
