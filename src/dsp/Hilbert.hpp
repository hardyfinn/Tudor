// 90-degree phase-difference network (Olli Niemitalo's IIR allpass pair).
#pragma once
#include "Common.hpp"

namespace tudor {

struct HilbertChain {
	float c[4];
	float x1[4] = {0, 0, 0, 0}, x2[4] = {0, 0, 0, 0}, y1[4] = {0, 0, 0, 0}, y2[4] = {0, 0, 0, 0};
	void init(const float* a) { for (int i = 0; i < 4; i++) c[i] = a[i] * a[i]; }
	float process(float x) {
		for (int i = 0; i < 4; i++) {
			float y = c[i] * (x + y2[i]) - x2[i];
			x2[i] = x1[i]; x1[i] = x; y2[i] = y1[i]; y1[i] = y;
			x = y;
		}
		return x;
	}
	void reset() { for (int i = 0; i < 4; i++) x1[i] = x2[i] = y1[i] = y2[i] = 0; }
};

struct Hilbert {
	HilbertChain ci, cq;
	float d = 0;   // one-sample delay on chain I
	Hilbert() {
		static const float a[4] = {0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f};
		static const float b[4] = {0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f};
		ci.init(a);
		cq.init(b);
	}
	// i lags... the two outputs are 90 degrees apart; q leads i.
	void process(float x, float& i, float& q) {
		float yi = ci.process(x);
		i = d; d = yi;
		q = cq.process(x);
	}
	void reset() { ci.reset(); cq.reset(); d = 0; }
};

} // namespace tudor
