// LOOP: 4x4 feedback matrix with send/return pairs and a KEEPER per bus that rides
// the bus gain to hold the network at the edge of oscillation.
#pragma once
#include "Filters.hpp"
#include "Noise.hpp"
#include "Analysis.hpp"

namespace tudor {

struct Loop {
	static constexpr int N = 4;
	struct Params {
		float m[N][N] = {};      // m[i][j]: from bus i's return into bus j
		float level[N] = {0.8f, 0.8f, 0.8f, 0.8f};
		bool keep[N] = {true, true, true, true};
		float edge = 0.4f;       // 0..1
		float speed = 1.5f;      // keeper time, seconds
		float rotate = 0.f;      // 0..1
	};
	struct Io {
		float in[N] = {};
		float ret[N] = {};
		bool retPatched[N] = {};
		float send[N] = {}, out[N] = {};
		float mixL = 0, mixR = 0;
	};
	float fs = 48000.f;
	float k[N] = {1, 1, 1, 1};
	float prevSend[N] = {};
	Rms rms[N];
	DcBlock dc[N];
	OnePoleLP bw[N];             // the bus amplifier's own bandwidth
	Rng rng{0x10095EEDull};
	Drift drift[N][N];
	float driftVal[N][N] = {};
	int driftCount = 0;
	static constexpr int DRIFT_DIV = 32;

	Loop() {
		for (int i = 0; i < N; i++)
			for (int j = 0; j < N; j++) drift[i][j] = Drift(1000 + i * 17 + j);
		setSampleRate(48000.f);
	}
	void setSampleRate(float sr) {
		fs = sr;
		for (int j = 0; j < N; j++) {
			rms[j].set(0.03f, sr);
			dc[j].setSampleRate(sr);
			bw[j].set(3000.f, sr);
			for (int i = 0; i < N; i++) drift[i][j].setSampleRate(sr / DRIFT_DIV);
		}
	}
	void reset() {
		for (int j = 0; j < N; j++) { k[j] = 1.f; prevSend[j] = 0; rms[j].reset(); dc[j].reset(); bw[j].reset(); }
	}
	static float target(float edge) { return 5.f * dbToGain(-36.f + 36.f * clampf(edge, 0.f, 1.f)); }

	void process(Io& io, const Params& p) {
		if (driftCount-- <= 0) {
			driftCount = DRIFT_DIV;
			for (int i = 0; i < N; i++)
				for (int j = 0; j < N; j++) driftVal[i][j] = drift[i][j].next();
		}
		float r[N];
		for (int i = 0; i < N; i++) r[i] = io.retPatched[i] ? io.ret[i] : prevSend[i];
		float T = target(p.edge);
		float dt = 1.f / fs;
		for (int j = 0; j < N; j++) {
			float v = io.in[j] + rng.gauss() * 1e-5f;
			for (int i = 0; i < N; i++) {
				float g = p.m[i][j];
				if (g == 0.f) continue;
				if (p.rotate > 0.f) g *= 1.f + p.rotate * 0.8f * clampf(driftVal[i][j], -2.5f, 2.5f);
				v += g * r[i];
			}
			v = dc[j].process(bw[j].process(v));
			v = softClamp(6.f * fastTanh(v / 6.f));
			float s = (p.keep[j] ? k[j] : 1.f) * v;
			s = softClamp(s);
			float level = rms[j].process(s);
			if (p.keep[j]) {
				float lk = std::log(k[j]) + (std::log(T) - std::log(std::max(level, 1e-6f))) * dt / std::max(p.speed, 0.05f);
				k[j] = clampf(std::exp(lk), 0.02f, 8.f);
			}
			if (!finite(s) || !finite(k[j])) { reset(); s = 0.f; }
			io.send[j] = s;
			io.out[j] = p.level[j] * s;
		}
		for (int j = 0; j < N; j++) prevSend[j] = io.send[j];
		io.mixL = softClamp(0.7071f * (io.out[0] + io.out[2]));
		io.mixR = softClamp(0.7071f * (io.out[1] + io.out[3]));
	}
};

} // namespace tudor
