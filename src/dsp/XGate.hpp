// XGATE: four channels cross-gated by each other's levels, then moved among 4 or 6 outputs.
#pragma once
#include "Analysis.hpp"
#include "Noise.hpp"

namespace tudor {

struct XGate {
	static constexpr int C = 4, O = 6;
	enum Mode { OPEN, DUCK, OFF };
	struct Params {
		int mode[C] = {OPEN, OPEN, OPEN, OPEN};
		int key[C] = {1, 2, 3, 0};
		float level[C] = {0.8f, 0.8f, 0.8f, 0.8f};
		float thresholdDb = -24.f, depth = 1.f, speed = 0.03f;
		int outputs = 6;
		float switchRate = 0.3f, randomness = 0.5f, glide = 0.15f;
	};
	struct Out { float out[O] = {}; bool moved = false; float gate[C] = {}; };

	float fs = 48000.f;
	Rms keyRms[C];
	float gain[C] = {1, 1, 1, 1};
	int dest[C] = {0, 1, 2, 3}, from[C] = {0, 1, 2, 3};
	float xf[C] = {1, 1, 1, 1};
	float untilSwitch = 2.f;
	Rng rng{0x7A7Eull};

	XGate() { setSampleRate(48000.f); }
	void setSampleRate(float sr) { fs = sr; for (auto& r : keyRms) r.set(0.01f, sr); }
	void reset() { for (auto& r : keyRms) r.reset(); }

	void move(const Params& p) {
		int nOut = p.outputs <= 4 ? 4 : 6;
		int count = 1 + (int)std::floor(clampf(p.randomness, 0.f, 1.f) * 3.99f * rng.uniform());
		for (int n = 0; n < count; n++) {
			int c = rng.below(C);
			int d = rng.below(nOut - 1);
			if (d >= dest[c]) d++;
			if (d >= nOut) d = 0;
			from[c] = dest[c]; dest[c] = d; xf[c] = 0.f;
		}
	}
	Out process(const float* in, bool switchTrig, const Params& p) {
		Out o;
		float dt = 1.f / fs;
		int nOut = p.outputs <= 4 ? 4 : 6;
		// keep destinations valid when switching to 4 outputs
		for (int c = 0; c < C; c++) { if (dest[c] >= nOut) { from[c] = dest[c]; dest[c] = c; xf[c] = 0.f; } }
		float keyDb[C];
		for (int c = 0; c < C; c++) keyDb[c] = gainToDb(keyRms[c].process(in[c]) / 5.f);
		float sp = std::max(p.speed, 0.0005f);
		for (int c = 0; c < C; c++) {
			float target = 1.f;
			if (p.mode[c] != OFF) {
				int k = std::min(std::max(p.key[c], 0), C - 1);
				float g = smoothstep(p.thresholdDb - 6.f, p.thresholdDb + 6.f, keyDb[k]);
				if (p.mode[c] == DUCK) g = 1.f - g;
				target = 1.f - clampf(p.depth, 0.f, 1.f) * (1.f - g);
			}
			float tau = target > gain[c] ? sp : 4.f * sp;
			gain[c] += (target - gain[c]) * (1.f - std::exp(-dt / tau));
			o.gate[c] = gain[c];
		}
		// switcher: exponential intervals blended with regular ones by RANDOMNESS
		untilSwitch -= dt;
		if (untilSwitch <= 0.f || switchTrig) {
			float mean = 1.f / clampf(p.switchRate, 0.005f, 50.f);
			float r = clampf(p.randomness, 0.f, 1.f);
			untilSwitch = (1.f - r) * mean + r * rng.expo(mean);
			move(p);
			o.moved = true;
		}
		float gl = std::max(p.glide, 0.001f);
		for (int c = 0; c < C; c++) {
			xf[c] = std::min(1.f, xf[c] + dt / gl);
			float a, b; eqPower(xf[c], a, b);
			float s = in[c] * gain[c] * p.level[c];
			if (xf[c] < 1.f && from[c] < nOut) o.out[from[c]] += s * a;
			o.out[dest[c]] += s * (xf[c] < 1.f ? b : 1.f);
		}
		for (int k = 0; k < O; k++) o.out[k] = k < nOut ? softClamp(o.out[k]) : 0.f;
		return o;
	}
};

} // namespace tudor
