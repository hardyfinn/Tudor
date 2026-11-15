// SWELL: percussion generator with attacks extendable to 8 s ("short sounds lengthened").
#pragma once
#include "Filters.hpp"
#include "Noise.hpp"
#include "Analysis.hpp"
#include <vector>

namespace tudor {

struct Swell {
	static constexpr int V = 4;
	enum Source { NOISE, PING, CAPTURE };
	struct Params {
		int source = PING;
		float attack = 0.4f, decay = 0.8f, toneHz = 700.f, body = 0.5f;
		float sensitivity = 0.5f, velocity = 0.7f, scatter = 0.2f, level = 0.8f;
	};
	struct Out { float l = 0, r = 0, env = 0; bool onset = false; };

	struct Voice {
		bool active = false;
		float t = 0, att = 0.4f, dec = 0.8f, vel = 1.f, tone = 700.f, pan = 0.5f;
		float env = 0.f, peakEnv = 0.f; bool decaying = false;
		Svf f1, f2;
		bool struck = false;
		// capture
		std::vector<float> buf; int len = 0, rec = 0; bool recording = false;
		float grainPos[4] = {}, grainT[4] = {}; float grainLen = 0.05f;
	};
	float fs = 48000.f;
	Voice v[V];
	Onset onset;
	Rng rng{0x5E11ull};
	int next = 0;
	DcBlock dcL, dcR;

	Swell() { setSampleRate(48000.f); }
	void setSampleRate(float sr) {
		fs = sr;
		onset.setSampleRate(sr);
		for (auto& vc : v) { vc.buf.assign((int)sr + 1, 0.f); vc.len = (int)(0.25f * sr); vc.active = false; }
		dcL.setSampleRate(sr); dcR.setSampleRate(sr);
	}
	void reset() { for (auto& vc : v) { vc.active = false; vc.f1.reset(); vc.f2.reset(); } }

	void fire(float strength, const Params& p) {
		// quietest voice if all busy
		int k = -1;
		for (int i = 0; i < V; i++) { int j = (next + i) % V; if (!v[j].active) { k = j; break; } }
		if (k < 0) { k = 0; for (int i = 1; i < V; i++) if (v[i].env < v[k].env) k = i; }
		next = (k + 1) % V;
		Voice& vc = v[k];
		float sc = clampf(p.scatter, 0.f, 1.f);
		vc.active = true; vc.t = 0; vc.env = 0; vc.decaying = false; vc.struck = false;
		vc.att = clampf(p.attack * std::pow(2.f, sc * rng.bipolar()), 0.001f, 16.f);
		vc.dec = p.decay;
		vc.tone = clampf(p.toneHz * std::pow(2.f, sc * rng.bipolar()), 20.f, 0.4f * fs);
		vc.pan = clampf(0.5f + 0.5f * sc * rng.bipolar() * 1.8f, 0.f, 1.f);
		vc.vel = 1.f - p.velocity + p.velocity * strength;
		vc.recording = true; vc.rec = 0;
		for (int g = 0; g < 4; g++) { vc.grainT[g] = -0.25f * g; vc.grainPos[g] = 0; }
		vc.f1.reset(); vc.f2.reset();
	}

	Out process(float in, bool trig, const Params& p) {
		Out o;
		float dt = 1.f / fs;
		if (onset.process(in, p.sensitivity)) { o.onset = true; fire(onset.strength, p); }
		if (trig) fire(1.f, p);
		float body = clampf(p.body, 0.f, 1.f);
		for (auto& vc : v) {
			if (vc.recording) {
				vc.buf[vc.rec++] = in;
				if (vc.rec >= vc.len) vc.recording = false;
			}
			if (!vc.active) continue;
			// envelope: gentle rise 1-(1-u)^2.5, then exponential decay to -80 dB over DECAY
			float e;
			if (!vc.decaying) {
				float u = vc.t / vc.att;
				if (u >= 1.f) { vc.decaying = true; vc.t = 0; e = 1.f; }
				else e = 1.f - std::pow(1.f - u, 2.5f);
			} else {
				e = std::exp(-9.21f * vc.t / std::max(vc.dec, 0.005f));
				if (e < 1e-4f) { vc.active = false; continue; }
			}
			vc.t += dt;
			vc.env = e;
			float y = 0.f;
			switch (p.source) {
				case NOISE: {
					vc.f1.set(vc.tone, 0.7f + 29.3f * body * body, fs);
					vc.f1.process(rng.gauss());
					y = vc.f1.bpNorm() * 2.f;
				} break;
				case PING: {
					// resonator driven continuously by noise under the envelope; a strike at the peak for short attacks
					float q = 20.f + 400.f * body;
					vc.f1.set(vc.tone, q, fs);
					vc.f2.set(vc.tone * 2.76f, q * 0.7f, fs);
					float exc = rng.gauss() * 0.6f;
					if (vc.decaying && !vc.struck) { vc.struck = true; exc += 40.f * (1.f - clampf(expNorm(vc.att, 0.001f, 8.f), 0.f, 1.f)); }
					vc.f1.process(exc); vc.f2.process(exc);
					y = (vc.f1.bpNorm() + dbToGain(-6.f * (1.f - body)) * 0.7f * vc.f2.bpNorm()) * 4.f;
				} break;
				default: {
					// CAPTURE: four overlapping Hann grains read from the captured onset
					float glen = 0.015f + 0.185f * body;
					float ratio = std::pow(2.f, (std::log2(vc.tone / 700.f)) / 4.f);
					int avail = vc.recording ? vc.rec : vc.len;
					for (int g = 0; g < 4; g++) {
						vc.grainT[g] += dt / glen * 1.f;
						if (vc.grainT[g] >= 1.f) { vc.grainT[g] -= 1.f; vc.grainPos[g] = rng.uniform() * 0.6f * vc.len; }
						if (vc.grainT[g] < 0.f) continue;
						float pos = vc.grainPos[g] + vc.grainT[g] * glen * fs * ratio;
						int i = (int)pos;
						if (i + 1 >= avail) continue;
						float fr = pos - i;
						float s = vc.buf[i] * (1 - fr) + vc.buf[i + 1] * fr;
						y += s * (0.5f - 0.5f * std::cos(TWO_PI * vc.grainT[g]));
					}
					y *= 0.5f;
				} break;
			}
			float a = y * e * vc.vel * p.level;
			float wl, wr; eqPower(vc.pan, wl, wr);
			o.l += a * wl * 1.4142f; o.r += a * wr * 1.4142f;
			o.env += e * vc.vel;
		}
		o.l = softClamp(dcL.process(o.l)); o.r = softClamp(dcR.process(o.r));
		o.env = clampf(o.env * 10.f, 0.f, 10.f);
		if (!finite(o.l) || !finite(o.r)) { reset(); o = Out(); }
		return o;
	}
};

} // namespace tudor
