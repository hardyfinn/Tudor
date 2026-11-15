// SIDEBAND: modulator with variable sideband (lower .. both .. upper) and leaks.
#pragma once
#include "Hilbert.hpp"

namespace tudor {

struct Sideband {
	struct Params {
		float sideband = 0.f;   // -1 lower .. 0 both .. +1 upper
		float carrierLeak = 0.f, signalLeak = 0.f, drive = 0.f, level = 1.f;
	};
	struct Out { float out = 0, upper = 0, lower = 0; };
	Hilbert hx, hc;

	void reset() { hx.reset(); hc.reset(); }

	static float drv(float v, float d) {
		if (d <= 0.f) return v;
		return 5.f * fastTanh(v * (1.f + 4.f * d) / 5.f) * (1.f + 0.6f * d);
	}
	Out process(float x, float c, const Params& p) {
		x = drv(x, p.drive);
		c = drv(c, p.drive);
		float xi, xq, ci, cq;
		hx.process(x, xi, xq);
		hc.process(c, ci, cq);
		Out o;
		o.upper = (xi * ci - xq * cq) / 5.f;
		o.lower = (xi * ci + xq * cq) / 5.f;
		float s = clampf(p.sideband, -1.f, 1.f);
		float mod = 0.5f * (1.f - s) * o.lower + 0.5f * (1.f + s) * o.upper;
		o.out = softClamp(p.level * (mod + p.carrierLeak * c + p.signalLeak * x));
		o.upper = softClamp(o.upper);
		o.lower = softClamp(o.lower);
		if (!finite(o.out) || !finite(o.upper) || !finite(o.lower)) { reset(); o = Out(); }
		return o;
	}
};

} // namespace tudor
