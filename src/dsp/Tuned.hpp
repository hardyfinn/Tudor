// TUNED: resonant band-pass amplifier with saturation inside the resonator.
#pragma once
#include "Filters.hpp"
#include "Oversample.hpp"

namespace tudor {

struct Tuned {
	enum Mode { CLEAN, TUBE, HARD };
	struct Params {
		float tuneHz = 440.f;
		float q = 8.f;
		float gainDb = 0.f;
		int mode = TUBE;
		float level = 0.8f;
	};
	float fs = 48000.f;
	Svf svf;
	DcBlock dc;
	Up2 up; Down2 down;
	int coefCount = 0;
	float lastF = -1, lastQ = -1;
	bool lastOs = false;

	void setSampleRate(float sr) { fs = sr; dc.setSampleRate(sr); coefCount = 0; lastF = -1; }
	void reset() { svf.reset(); dc.reset(); up.reset(); down.reset(); }

	static float sat(float s, int mode) {
		switch (mode) {
			case TUBE: return 4.f * (std::tanh(s / 4.f + 0.15f) - std::tanh(0.15f)) / 0.97775f;   // unity slope at 0
			case HARD: return clampf(s, -4.f, 4.f);
			default: return softClamp(s, LIM);
		}
	}
	float step(float x, int mode) {
		svf.process(x);
		// saturate the band-pass integrator state (nonlinear SVF), scaled so the normalised
		// output saturates at the same level at any Q: bounded ringing that stays audible
		float sc = 0.5f * svf.k;
		svf.ic1 = sat(svf.ic1 * sc, mode) / sc;
		svf.ic2 = softClamp(svf.ic2, 2.f * LIM);
		return svf.bpNorm();
	}
	float process(float x, const Params& p) {
		bool os = p.mode == HARD;
		float rate = os ? 2.f * fs : fs;
		if (coefCount-- <= 0 || os != lastOs) {
			coefCount = 16;
			if (p.tuneHz != lastF || p.q != lastQ || os != lastOs) { svf.set(p.tuneHz, p.q, rate); lastF = p.tuneHz; lastQ = p.q; }
			lastOs = os;
		}
		float in = x * dbToGain(p.gainDb);
		float y;
		if (os) {
			float a, b;
			up.process(in, a, b);
			float ya = step(a, p.mode), yb = step(b, p.mode);
			y = down.process(ya, yb);
		} else {
			y = step(in, p.mode);
		}
		y = dc.process(y * p.level);
		y = softClamp(y);
		if (!finite(y) || !svf.ok()) { reset(); y = 0.f; }
		return y;
	}
};

} // namespace tudor
