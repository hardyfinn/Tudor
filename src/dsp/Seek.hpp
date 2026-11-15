// SEEK: probe an object, find its strongest resonances, then tease them.
// "Start from the other end and go back and arrive at a signal source." (Tudor)
#pragma once
#include "Analysis.hpp"
#include "Noise.hpp"

namespace tudor {

struct Seek {
	static constexpr int BINS = 208;            // 1/24 octave from 30 Hz (to about 12 kHz)
	static constexpr float F_LO = 30.f, F_HI = 12000.f;
	static constexpr int MAXN = 8;
	static constexpr int FFT_N = 4096, HOP = 1024;
	enum Mode { SWEEP, LISTEN };
	struct Params {
		int mode = SWEEP;
		float sweepTime = 8.f;
		int count = 4;
		float drift = 0.3f;
		float linger = 2.f, glide = 0.3f;
		float probeLevel = 0.7f, teaseLevel = 0.7f;
	};
	struct Out { float probe = 0, tease = 0; bool found = false, arrive = false; };

	float fs = 48000.f;
	float curve[BINS] = {};       // measured response, 0..1 normalised for display
	float raw[BINS] = {};
	// found nodes
	int nNodes = 0;
	float nodeHz[MAXN] = {}, nodeStr[MAXN] = {};
	bool scanning = false;
	// sweep state
	float sweepT = 0.f, sweepDur = 8.f;
	double sweepPhase = 0.0;
	float lpI1 = 0, lpI2 = 0, lpQ1 = 0, lpQ2 = 0;
	// listen state
	Spectrum spec;
	std::vector<float> ring;
	int ringPos = 0, hopCount = 0;
	float avg[FFT_N / 2] = {};
	// tease state
	Rng rng{0x5EE4ull};
	int teaseNode = -1, lastNode = -1;
	float teasePitch = 0.f, glideFrom = 0.f, glideT = 1.f, glideDur = 0.3f, lingerLeft = 0.f, lingerDur = 1.f;
	bool gliding = false;
	double teasePhase = 0.0;
	float vibPhase = 0.f;
	float teaseLevel = 0.f;

	Seek() { spec.init(FFT_N); ring.assign(FFT_N, 0.f); }
	void setSampleRate(float sr) { fs = sr; }

	static float binHz(float b) { return F_LO * std::pow(2.f, b / 24.f); }
	static float hzBin(float f) { return 24.f * std::log2(std::max(f, 1.f) / F_LO); }

	void startSweep(const Params& p) {
		scanning = true;
		sweepT = 0.f;
		sweepDur = clampf(p.sweepTime, 1.f, 60.f);
		sweepPhase = 0.0;
		lpI1 = lpI2 = lpQ1 = lpQ2 = 0.f;
		for (float& r : raw) r = 0.f;
	}
	void hunt(const Params& p) {
		if (p.mode == SWEEP) startSweep(p);
		else { nNodes = 0; for (float& a : avg) a = 0.f; for (float& c : curve) c = 0.f; }
	}

	// peak-pick a log-binned response into the node list (strongest first)
	void pickPeaks(const float* r, int count) {
		float sm[BINS];
		for (int b = 0; b < BINS; b++) {
			float a = r[std::max(b - 1, 0)], c = r[std::min(b + 1, BINS - 1)];
			sm[b] = 0.25f * a + 0.5f * r[b] + 0.25f * c;
		}
		struct Pk { float bin, v; };
		Pk pk[BINS]; int np = 0;
		for (int b = 1; b < BINS - 1; b++) {
			if (sm[b] > sm[b - 1] && sm[b] >= sm[b + 1] && sm[b] > 1e-5f) {
				float y0 = sm[b - 1], y1 = sm[b], y2 = sm[b + 1];
				float den = y0 - 2 * y1 + y2;
				float off = std::fabs(den) > 1e-12f ? 0.5f * (y0 - y2) / den : 0.f;
				pk[np++] = {b + clampf(off, -0.5f, 0.5f), y1};
			}
		}
		std::sort(pk, pk + np, [](const Pk& a, const Pk& b) { return a.v > b.v; });
		float nh[MAXN], ns[MAXN]; int n = 0;
		for (int i = 0; i < np && n < count; i++) {
			bool close = false;
			for (int j = 0; j < n; j++) if (std::fabs(hzBin(nh[j]) - pk[i].bin) < 4.f) close = true;
			if (close) continue;
			nh[n] = binHz(pk[i].bin); ns[n] = pk[i].v; n++;
		}
		float top = n > 0 ? ns[0] : 1.f;
		for (int i = 0; i < n; i++) ns[i] /= top;
		// keep channels stable: match new nodes to old ones by nearest log frequency
		if (nNodes > 0 && n > 0) {
			float oh[MAXN], os[MAXN]; bool used[MAXN] = {};
			int on = 0;
			for (int i = 0; i < nNodes; i++) {
				int best = -1; float bd = 4.f;
				for (int j = 0; j < n; j++) {
					float d = std::fabs(hzBin(nh[j]) - hzBin(nodeHz[i]));
					if (!used[j] && d < bd) { bd = d; best = j; }
				}
				if (best >= 0) { used[best] = true; oh[on] = nh[best]; os[on] = ns[best]; on++; }
			}
			for (int j = 0; j < n && on < MAXN; j++) if (!used[j]) { oh[on] = nh[j]; os[on] = ns[j]; on++; }
			// strongest first
			int idx[MAXN];
			for (int i = 0; i < on; i++) idx[i] = i;
			std::sort(idx, idx + on, [&](int a, int b) { return os[a] > os[b]; });
			for (int i = 0; i < on && i < count; i++) { nodeHz[i] = oh[idx[i]]; nodeStr[i] = os[idx[i]]; }
			nNodes = std::min(on, count);
		} else {
			for (int i = 0; i < n; i++) { nodeHz[i] = nh[i]; nodeStr[i] = ns[i]; }
			nNodes = n;
		}
		if (teaseNode >= nNodes) teaseNode = -1;
	}

	Out process(float ret, const Params& p, bool huntTrig) {
		Out o;
		float dt = 1.f / fs;
		if (huntTrig) hunt(p);
		if (p.mode == SWEEP) {
			if (scanning) {
				float u = sweepT / sweepDur;
				float f = F_LO * std::pow(F_HI / F_LO, u);
				sweepPhase += f * dt;
				sweepPhase -= std::floor(sweepPhase);
				float s = std::sin(TWO_PI * (float)sweepPhase), c = std::cos(TWO_PI * (float)sweepPhase);
				float fade = std::min(1.f, std::min(sweepT, sweepDur - sweepT) / 0.05f);
				float amp = 5.f * p.probeLevel;
				o.probe = amp * s * std::max(fade, 0.f);
				// synchronous detection (two-pole low-pass on I and Q)
				float octPerSec = std::log2(F_HI / F_LO) / sweepDur;
				float lpf = std::max(5.f, octPerSec * 3.f);
				float k = 1.f - std::exp(-TWO_PI * lpf * dt);
				lpI1 += k * (ret * s - lpI1); lpI2 += k * (lpI1 - lpI2);
				lpQ1 += k * (ret * c - lpQ1); lpQ2 += k * (lpQ1 - lpQ2);
				float mag = 2.f * std::sqrt(lpI2 * lpI2 + lpQ2 * lpQ2) / std::max(amp, 1e-3f);
				// compensate the detector lag: credit the frequency of ~2 time constants ago
				float lag = 2.f / (TWO_PI * lpf);
				float fLag = F_LO * std::pow(F_HI / F_LO, std::max(0.f, (sweepT - lag) / sweepDur));
				int b = (int)std::round(hzBin(fLag));
				if (b >= 0 && b < BINS && sweepT > lag) raw[b] = std::max(raw[b], mag);
				sweepT += dt;
				if (sweepT >= sweepDur) {
					scanning = false;
					pickPeaks(raw, clampi(p.count));
					float mx = 1e-9f; for (float r : raw) mx = std::max(mx, r);
					for (int i = 0; i < BINS; i++) curve[i] = raw[i] / mx;
					o.found = true;
					teaseNode = -1;
				}
			}
		} else {
			scanning = false;
			ring[ringPos] = ret;
			ringPos = (ringPos + 1) % FFT_N;
			if (++hopCount >= HOP) {
				hopCount = 0;
				spec.compute(ring.data(), ringPos);
				float tau = 0.2f + 20.f * (1.f - clampf(p.drift, 0.f, 1.f));
				float a = 1.f - std::exp(-(float)HOP / (fs * tau));
				for (int i = 0; i < FFT_N / 2; i++) avg[i] += a * (spec.mag[i] - avg[i]);
				float lb[BINS] = {};
				for (int i = 1; i < FFT_N / 2; i++) {
					float f = i * fs / FFT_N;
					int b = (int)std::round(hzBin(f));
					if (b >= 0 && b < BINS) lb[b] = std::max(lb[b], avg[i]);
				}
				// fill empty low bins by interpolation from FFT bins
				for (int b = 0; b < BINS; b++) if (lb[b] == 0.f) {
					float fi = binHz((float)b) * FFT_N / fs;
					int i = (int)fi; float fr = fi - i;
					if (i >= 0 && i + 1 < FFT_N / 2) lb[b] = avg[i] * (1 - fr) + avg[i + 1] * fr;
				}
				int before = nNodes; float h0 = nNodes ? nodeHz[0] : 0.f;
				pickPeaks(lb, clampi(p.count));
				float mx = 1e-9f; for (float v : lb) mx = std::max(mx, v);
				for (int i = 0; i < BINS; i++) curve[i] = lb[i] / mx;
				if (nNodes != before || (nNodes && std::fabs(hzBin(nodeHz[0]) - hzBin(h0)) > 1.f)) o.found = true;
			}
		}
		o.tease = tease(p, o.arrive);
		return o;
	}
	static int clampi(int c) { return std::min(std::max(c, 1), MAXN); }

	void chooseNext(const Params& p) {
		if (nNodes <= 0) { teaseNode = -1; return; }
		float tot = 0.f;
		for (int i = 0; i < nNodes; i++) if (i != lastNode || nNodes == 1) tot += 0.1f + nodeStr[i];
		float r = rng.uniform() * tot;
		int pick = 0;
		for (int i = 0; i < nNodes; i++) {
			if (i == lastNode && nNodes > 1) continue;
			r -= 0.1f + nodeStr[i];
			if (r <= 0.f) { pick = i; break; }
			pick = i;
		}
		glideFrom = teaseNode >= 0 ? teasePitch : std::log2(nodeHz[pick] / C4);
		teaseNode = pick;
		glideT = 0.f;
		glideDur = std::max(p.glide, 0.001f);
		gliding = true;
	}
	float tease(const Params& p, bool& arrive) {
		float dt = 1.f / fs;
		if (nNodes <= 0 || scanning) { teaseLevel = std::max(0.f, teaseLevel - dt / 0.05f); }
		else {
			if (teaseNode < 0) chooseNext(p);
			float target = std::log2(nodeHz[teaseNode] / C4);
			if (gliding) {
				glideT += dt;
				float u = clampf(glideT / glideDur, 0.f, 1.f);
				teasePitch = lerp(glideFrom, target, u);
				teaseLevel = 0.4f + 0.6f * smoothstep(0.f, 0.4f, u);
				if (u >= 1.f) {
					gliding = false; arrive = true;
					lingerDur = p.linger * (0.5f + nodeStr[teaseNode]);
					lingerLeft = lingerDur;
				}
			} else {
				teasePitch = target;
				teaseLevel = std::min(1.f, teaseLevel + dt / 0.05f);
				lingerLeft -= dt;
				if (lingerLeft <= 0.f) { lastNode = teaseNode; chooseNext(p); }
			}
		}
		vibPhase += 0.3f * dt; vibPhase -= std::floor(vibPhase);
		float cents = gliding ? 0.f : 8.f * std::sin(TWO_PI * vibPhase);
		float f = C4 * std::pow(2.f, teasePitch + cents / 1200.f);
		teasePhase += clampf(f, 1.f, 0.45f * fs) * dt;
		teasePhase -= std::floor(teasePhase);
		return 5.f * p.teaseLevel * teaseLevel * std::sin(TWO_PI * (float)teasePhase);
	}
};

} // namespace tudor
