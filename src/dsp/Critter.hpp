// CRITTER: four animal-like signal generators (Rainforest I's "oscillators that made
// animal-like and bird-like sounds").
#pragma once
#include "Noise.hpp"
#include <vector>

namespace tudor {

struct Critter {
	static constexpr int V = 4;
	struct Voice {
		float pitch = 1000.f, warble = 0.3f, chirp = 0.f, rate = 1.f, density = 0.6f;
	};
	struct Params {
		Voice v[V];
		float species = 0.2f;   // 0 bird, 0.5 insect, 1 frog
		float follow = 0.f;
		float level = 0.8f;
		const float* followPitches = nullptr;   // V/oct
		int followCount = 0;
		bool callAll = false;
	};
	struct Out { float v[V] = {}; float mix = 0; bool calling[V] = {}; };

	float fs = 48000.f;
	struct State {
		Rng rng;
		Drift wob;
		double phase = 0, phase2 = 0;
		float untilNext = 0.f;   // s
		float callT = 1e9f, callLen = 0.2f;
		float callOffset = 0.f;  // semitone jitter per call
		int syllables = 1;
		float amPhase = 0.f;
		float followPitch = 0.f; bool followInit = false;
		float vibPhase = 0.f;
		explicit State(uint64_t s = 1) : rng(s), wob(s * 3 + 1) {}
	};
	State st[V] = {State(101), State(202), State(303), State(404)};

	void setSampleRate(float sr) { fs = sr; for (auto& s : st) s.wob.setSampleRate(sr); }

	static float bell(float x) { return x <= 0.f || x >= 1.f ? 0.f : 0.5f - 0.5f * std::cos(TWO_PI * x); }

	Out process(const Params& p) {
		Out o;
		float dt = 1.f / fs;
		float sp = clampf(p.species, 0.f, 1.f);
		float wBird = sp < 0.5f ? std::cos(sp * PI) : 0.f;
		float wInsect = std::sin(sp * PI);
		float wFrog = sp > 0.5f ? -std::cos(sp * PI) : 0.f;
		// assign follow targets greedily (strongest first) so voices take different pitches
		bool taken[16] = {};
		for (int i = 0; i < V; i++) {
			const Voice& v = p.v[i];
			State& s = st[i];
			float base = std::log2(std::max(v.pitch, 1.f) / C4);
			if (p.followPitches && p.followCount > 0 && p.follow > 0.f) {
				int best = -1; float bd = 1e9f;
				for (int c = 0; c < p.followCount && c < 16; c++) {
					float d = std::fabs(p.followPitches[c] - base);
					if (!taken[c] && d < bd) { bd = d; best = c; }
				}
				if (best < 0) best = 0;
				taken[best] = true;
				if (!s.followInit) { s.followPitch = base; s.followInit = true; }
				float tau = 0.1f + 9.9f * (1.f - p.follow);
				s.followPitch += (p.followPitches[best] - s.followPitch) * (1.f - std::exp(-dt / tau));
				base = (1.f - p.follow) * base + p.follow * s.followPitch;
			} else s.followInit = false;

			// scheduling: irregular gaps around 1/RATE, each call happens with DENSITY
			float rate = clampf(v.rate, 0.05f, 40.f);
			s.untilNext -= dt;
			if (s.untilNext <= 0.f || p.callAll) {
				s.untilNext = s.rng.expo(1.f / rate);
				if (p.callAll || s.rng.uniform() < v.density) {
					s.callLen = clampf(0.35f / rate, 0.02f, 1.5f) * (wInsect > 0.6f ? 3.f : 1.f) * (wFrog > 0.5f ? 1.6f : 1.f);
					s.callT = 0.f;
					s.callOffset = 1.5f * s.rng.bipolar();
					s.syllables = 1 + s.rng.below(std::min(6, 1 + (int)(rate * 0.6f) + 2));
				}
			}
			float y = 0.f;
			if (s.callT < s.callLen) {
				float u = s.callT / s.callLen;
				s.callT += dt;
				// pitch: base + call jitter + chirp sweep + warble
				float vib = s.wob.next();
				s.vibPhase += (6.f + 34.f * v.warble) * (1.f + 0.3f * vib) * dt;
				s.vibPhase -= std::floor(s.vibPhase);
				float warble = v.warble * std::sin(TWO_PI * s.vibPhase) * (0.5f + 0.5f * std::fabs(vib));
				// bird: chirp across each syllable; insect/frog: across the call, smaller
				float syl = u * s.syllables; float su = syl - std::floor(syl);
				float chirpBird = 2.f * v.chirp * (std::pow(2.f, 3.f * su) - 1.f) / 7.f;
				float chirpCall = 2.f * v.chirp * u;
				float oct = base + s.callOffset / 12.f + warble
					+ wBird * chirpBird + wInsect * 0.1f * chirpCall + wFrog * 0.3f * chirpCall
					- wFrog * 2.f;
				float f = clampf(C4 * std::pow(2.f, oct), 20.f, 0.45f * fs);
				s.phase += f * dt; s.phase -= std::floor(s.phase);
				s.phase2 += 2.f * f * dt; s.phase2 -= std::floor(s.phase2);
				float ph = (float)s.phase * TWO_PI;
				// three call generators
				float att = std::min(1.f, s.callT / 0.003f);
				float bird = std::sin(ph) * bell(su) * att;
				s.amPhase += (40.f + 160.f * v.warble) * dt; s.amPhase -= std::floor(s.amPhase);
				float stri = s.amPhase < 0.3f ? std::min(1.f, s.amPhase / 0.02f) : std::max(0.f, 1.f - (s.amPhase - 0.3f) / 0.02f);
				float insect = std::sin(ph + 0.6f * std::sin(ph)) * stri * bell(u * 0.5f + 0.25f) * att;
				float croakRate = 12.f + 18.f * v.warble;
				float cp = s.callT * croakRate; cp -= std::floor(cp);
				float frog = (std::sin(ph) + 0.35f * std::sin((float)s.phase2 * TWO_PI)) * std::exp(-cp * 5.f) * std::min(1.f, cp / 0.02f) * bell(u * 0.5f + 0.25f);
				y = wBird * bird + wInsect * insect + wFrog * frog;
				o.calling[i] = true;
			}
			o.v[i] = 5.f * p.level * y;
			o.mix += 0.5f * o.v[i];
		}
		return o;
	}
};

} // namespace tudor
