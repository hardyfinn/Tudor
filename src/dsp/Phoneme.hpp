// PHONEME: 16-band vocoder whose band levels are chopped into small pieces
// ("long sounds shortened").
#pragma once
#include "Filters.hpp"
#include "Noise.hpp"
#include "Analysis.hpp"

namespace tudor {

struct Phoneme {
	static constexpr int B = 16;
	enum Carrier { INPUT, SELF, NOISE };
	struct Params {
		int carrier = SELF;
		float chopRate = 9.f, chopSize = 0.55f, jitter = 0.25f, shuffle = 0.f, response = 0.015f;
		bool hold = true, freeze = false;
		float formant = 0.f, mix = 1.f;
	};
	struct Out { float out = 0, odd = 0, even = 0; float env[B] = {}; bool gateOpen = false; };

	float fs = 48000.f;
	float fc[B];
	Svf an1[B], an2[B], sy1[B], sy2[B];
	EnvFollow ef[B];
	float held[B] = {}, frozen[B] = {}, makeup[B];
	int map[B], prevMap[B], inv[B], invPrev[B];   // map: analysis -> synthesis; inv: synthesis -> analysis
	bool drop[B] = {};
	float mapX = 1.f;   // crossfade old->new mapping
	Rng rng{0xF0E3ull};
	// chop clock
	float phase = 0.f, period = 1.f / 9.f, extPeriod = 0.1f, sinceExt = 0.f;
	float windowT = 1e9f, windowLen = 0.05f;
	bool wasFrozen = false;
	float lastResp = -1.f, lastFormant = 1e9f;
	Trigger ext;
	static constexpr float OUT_GAIN = 19.5f;   // pink noise in SELF, SIZE 100%, TRACK: about unity
	DcBlock dc;

	Phoneme() {
		for (int b = 0; b < B; b++) { fc[b] = 120.f * std::pow(7500.f / 120.f, b / (float)(B - 1)); map[b] = prevMap[b] = inv[b] = invPrev[b] = b; makeup[b] = 1.f; }
		setSampleRate(48000.f);
	}
	void setSampleRate(float sr) {
		fs = sr;
		for (int b = 0; b < B; b++) {
			an1[b].set(fc[b], 5.f, sr); an2[b].set(fc[b], 5.f, sr);
			makeup[b] = 1.f;
		}
		dc.setSampleRate(sr);
		lastResp = -1.f; lastFormant = 1e9f;
	}
	void reset() {
		for (int b = 0; b < B; b++) { an1[b].reset(); an2[b].reset(); sy1[b].reset(); sy2[b].reset(); ef[b].reset(); held[b] = 0; }
	}
	void newChop(const Params& p) {
		float j = clampf(p.jitter, 0.f, 1.f);
		for (int b = 0; b < B; b++) { held[b] = ef[b].y; drop[b] = rng.uniform() < 0.5f * j; prevMap[b] = map[b]; map[b] = b; }
		float sh = clampf(p.shuffle, 0.f, 1.f);
		if (sh > 0.f) {
			int reach = 1 + (int)std::round(7.f * sh);
			for (int b = 0; b < B; b++) if (rng.uniform() < sh) {
				int o = clampi(b + rng.below(2 * reach + 1) - reach);
				std::swap(map[b], map[o]);
			}
		}
		for (int b = 0; b < B; b++) { invPrev[b] = inv[b]; }
		for (int a = 0; a < B; a++) inv[map[a]] = a;
		mapX = 0.f;
		windowT = 0.f;
	}
	static int clampi(int b) { return std::min(std::max(b, 0), B - 1); }

	// chop: external rising edge, or the internal clock when ext is not patched
	Out process(float mod, float car, bool carPatched, float chopIn, bool chopPatched, const Params& p) {
		Out o;
		float dt = 1.f / fs;
		if (p.response != lastResp) { lastResp = p.response; for (int b = 0; b < B; b++) ef[b].set(0.002f, p.response, fs); }
		if (p.formant != lastFormant) {
			lastFormant = p.formant;
			float m = std::pow(2.f, clampf(p.formant, -1.f, 1.f));
			for (int b = 0; b < B; b++) { sy1[b].set(fc[b] * m, 5.f, fs); sy2[b].set(fc[b] * m, 5.f, fs); }
		}
		// clock
		bool chop = false;
		float j = clampf(p.jitter, 0.f, 1.f);
		if (chopPatched) {
			sinceExt += dt;
			if (ext.process(chopIn)) { extPeriod = clampf(sinceExt, 0.005f, 4.f); sinceExt = 0.f; chop = true; period = extPeriod; }
		} else {
			phase += dt / period;
			if (phase >= 1.f) {
				phase -= 1.f;
				chop = true;
				float base = 1.f / clampf(p.chopRate, 0.2f, 200.f);
				period = base * (1.f + 0.5f * j * rng.bipolar());
			}
		}
		if (chop) newChop(p);
		windowLen = clampf(p.chopSize, 0.05f, 1.f) * period;
		windowT += dt;
		// gate: open for windowLen with 2 ms raised-cosine edges
		float edge = 0.002f, g;
		if (clampf(p.chopSize, 0.f, 1.f) >= 0.999f) g = 1.f;
		else if (windowT < edge) g = 0.5f - 0.5f * std::cos(PI * windowT / edge);
		else if (windowT < windowLen - edge) g = 1.f;
		else if (windowT < windowLen) g = 0.5f - 0.5f * std::cos(PI * (windowLen - windowT) / edge);
		else g = 0.f;
		o.gateOpen = g > 0.5f;
		mapX = std::min(1.f, mapX + dt / edge);

		float carrier = p.carrier == NOISE ? rng.gauss() * 2.f : ((p.carrier == INPUT && carPatched) ? car : mod);
		if (p.freeze && !wasFrozen) for (int b = 0; b < B; b++) frozen[b] = (p.hold ? held[b] : ef[b].y);
		wasFrozen = p.freeze;
		float lev[B];
		for (int b = 0; b < B; b++) {
			an1[b].process(mod); an2[b].process(an1[b].bpNorm());
			ef[b].process(an2[b].bpNorm());
		}
		for (int b = 0; b < B; b++) {
			float l = p.freeze ? frozen[b] : (p.hold ? held[b] : ef[b].y);
			lev[b] = drop[b] ? 0.f : l * g;
		}
		float sum = 0, odd = 0, even = 0;
		for (int s = 0; s < B; s++) {
			sy1[s].process(carrier); sy2[s].process(sy1[s].bpNorm());
			float band = sy2[s].bpNorm();
			// synthesis band s gets the analysis band mapped onto it (crossfaded at each chop)
			float l = lev[inv[s]] * mapX + lev[invPrev[s]] * (1.f - mapX);
			float y = band * l * makeup[s] / 5.f;
			sum += y;
			if (s & 1) even += y; else odd += y;   // band 1 is index 0: odd
			o.env[s] = clampf(lev[s] * 2.f, 0.f, 10.f);
		}
		sum *= OUT_GAIN; odd *= OUT_GAIN * 1.4142f; even *= OUT_GAIN * 1.4142f;
		o.out = softClamp(dc.process(p.mix * sum + (1.f - p.mix) * mod));
		o.odd = softClamp(odd); o.even = softClamp(even);
		if (!finite(o.out) || !finite(o.odd) || !finite(o.even)) { reset(); o = Out(); }
		return o;
	}
};

} // namespace tudor
