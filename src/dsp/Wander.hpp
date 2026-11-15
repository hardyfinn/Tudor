// WANDER: a listener walking among up to 8 objects, each heard directly (AIR) at its
// position and through its own loudspeaker (PICKUP) on the nearest wall.
#pragma once
#include "DelayLine.hpp"
#include "Filters.hpp"
#include "Noise.hpp"
#include "Analysis.hpp"

namespace tudor {

struct Wander {
	static constexpr int N = 8;
	static constexpr int SRC = 2 * N;
	struct Params {
		float speed = 0.25f, curiosity = 0.5f, room = 0.3f, speakers = 0.5f, size = 12.f;
		bool ear = false;
		bool quad = false;
		bool xyPatched = false;
		float x = 0.5f, y = 0.5f;   // 0..1 when patched
	};
	struct Out { float l = 0, r = 0, fl = 0, fr = 0, rl = 0, rr = 0; };

	float fs = 48000.f;
	// positions in normalised room units 0..1 (x east, y north)
	float objX[N], objY[N], spkX[N], spkY[N];
	float lx = 0.5f, ly = 0.5f, heading = 0.f, vel = 0.f;
	float trailX[32], trailY[32]; int trailPos = 0; float trailT = 0.f;
	float level[N] = {};
	DelayLine dl[SRC];
	OnePoleLP absorb[SRC], shadowL[SRC], shadowR[SRC];
	float gainS[SRC] = {}, delS[SRC] = {}, panS[SRC] = {}, panQ[SRC][4] = {}, absF[SRC] = {}, shL[SRC] = {}, shR[SRC] = {}, itd[SRC] = {};
	Rms rms[N];
	Drift turn{77};
	Rng rng{0xA1A1ull};
	int ctl = 0;
	static constexpr int CTL_DIV = 32;
	float earAmt = 0.f;
	// room FDN
	DelayLine fdn[4];
	OnePoleLP fdnLp[4];
	float fdnLen[4] = {}, fdnG = 0.5f;

	Wander() { defaultLayout(); setSampleRate(48000.f); }

	void defaultLayout() {
		Rng r(0x7EE5ull);
		for (int i = 0; i < N; i++) {
			float a = TWO_PI * i / N + 0.2f * r.bipolar();
			float rad = 0.3f * (1.f + 0.1f * r.bipolar()) * (i % 2 ? 0.8f : 1.1f);   // staggered
			objX[i] = 0.5f + rad * std::cos(a);
			objY[i] = 0.5f + rad * std::sin(a);
		}
		placeSpeakers();
	}
	// each object's loudspeaker on the nearest wall, nudged so no two coincide
	void placeSpeakers() {
		for (int i = 0; i < N; i++) {
			float x = objX[i], y = objY[i];
			float d[4] = {x, 1 - x, y, 1 - y};
			int w = 0; for (int k = 1; k < 4; k++) if (d[k] < d[w]) w = k;
			float along = (w < 2 ? y : x) + 0.03f * (i - 3.5f);
			along = clampf(along, 0.05f, 0.95f);
			float inset = 0.02f;
			if (w == 0) { spkX[i] = inset; spkY[i] = along; }
			else if (w == 1) { spkX[i] = 1 - inset; spkY[i] = along; }
			else if (w == 2) { spkX[i] = along; spkY[i] = inset; }
			else { spkX[i] = along; spkY[i] = 1 - inset; }
		}
	}
	void setSampleRate(float sr) {
		fs = sr;
		for (int s = 0; s < SRC; s++) dl[s].init((int)(0.15f * sr));   // up to ~50 m of path
		for (int i = 0; i < N; i++) rms[i].set(0.2f, sr);
		for (int k = 0; k < 4; k++) { fdn[k].init((int)(0.4f * sr)); fdnLp[k].set(6000.f, sr); }
		turn.setSampleRate(sr / CTL_DIV);
	}
	void reset() {
		for (auto& d : dl) d.clear();
		for (auto& f : fdn) f.clear();
	}

	void control(const Params& p) {
		float dt = (float)CTL_DIV / fs;
		float S = clampf(p.size, 2.f, 60.f);
		// object levels for CURIOSITY
		float cx = 0, cy = 0, cw = 0;
		for (int i = 0; i < N; i++) {
			float db = gainToDb(level[i] / 5.f);
			if (db > -30.f) { float w = (db + 30.f) / 30.f; cx += w * objX[i]; cy += w * objY[i]; cw += w; }
		}
		earAmt += ((p.ear ? 1.f : 0.f) - earAmt) * (1.f - std::exp(-dt / 0.6f));
		if (p.xyPatched) {
			float k = 1.f - std::exp(-dt / 0.1f);
			lx += (clampf(p.x, 0.f, 1.f) - lx) * k;
			ly += (clampf(p.y, 0.f, 1.f) - ly) * k;
		} else {
			heading += 0.9f * turn.next() * dt;
			if (cw > 0.f && p.curiosity > 0.f) {
				float want = std::atan2(cy / cw - ly, cx / cw - lx);
				float diff = std::remainder(want - heading, TWO_PI);
				heading += clampf(diff, -1.f, 1.f) * p.curiosity * 0.6f * dt;
			}
			float spd = clampf(p.speed, 0.f, 2.f) / S;   // room units per second
			vel += (spd - vel) * (1.f - std::exp(-dt / 1.5f));
			float nx = lx + std::cos(heading) * vel * dt, ny = ly + std::sin(heading) * vel * dt;
			// walls repel within 0.5 m, objects within 0.4 m
			float wm = 0.5f / S;
			if (nx < wm || nx > 1 - wm) heading = PI - heading;
			if (ny < wm || ny > 1 - wm) heading = -heading;
			for (int i = 0; i < N; i++) {
				float dx = nx - objX[i], dy = ny - objY[i], d = std::sqrt(dx * dx + dy * dy) * S;
				if (d < 0.4f && earAmt < 0.5f) {
					float away = std::atan2(dy, dx);
					float diff = std::remainder(away - heading, TWO_PI);
					heading += clampf(diff, -1.f, 1.f) * 0.5f;
					nx = lx; ny = ly;
				}
			}
			lx = clampf(nx, wm, 1 - wm); ly = clampf(ny, wm, 1 - wm);
		}
		// EAR TO OBJECT: glide toward the nearest object, to 0.1 m
		float ex = lx, ey = ly;
		if (earAmt > 0.001f) {
			int best = 0; float bd = 1e9f;
			for (int i = 0; i < N; i++) { float d = std::hypot(lx - objX[i], ly - objY[i]); if (d < bd) { bd = d; best = i; } }
			float tx = objX[best], ty = objY[best];
			float dx = lx - tx, dy = ly - ty, d = std::max(std::hypot(dx, dy), 1e-4f);
			float stop = 0.1f / S;
			ex = lerp(lx, tx + dx / d * stop, earAmt);
			ey = lerp(ly, ty + dy / d * stop, earAmt);
		}
		float face = p.quad ? PI * 0.5f : heading;   // quad: fixed orientation, north = front
		for (int s = 0; s < SRC; s++) {
			int i = s % N;
			bool isAir = s < N;
			float sx = isAir ? objX[i] : spkX[i], sy = isAir ? objY[i] : spkY[i];
			float dx = (sx - ex) * S, dy = (sy - ey) * S;
			float d = std::max(std::sqrt(dx * dx + dy * dy), 0.1f);
			float g = 1.f / std::max(d, 0.5f);
			if (isAir && d < 0.5f) g *= std::min(4.f, 1.f + 3.f * (0.5f - d) / 0.4f);
			if (!isAir) g *= 2.f * clampf(p.speakers, 0.f, 1.f);
			gainS[s] = g;
			delS[s] = d / 343.f * fs;
			absF[s] = 20000.f / (1.f + d / 5.f);
			float az = std::remainder(std::atan2(dy, dx) - face, TWO_PI);   // 0 = straight ahead, + = left
			panS[s] = az;
			float sn = std::sin(az);
			itd[s] = 0.0006f * fs * sn;   // + = source on the left: right ear later
			float behind = std::cos(az) < 0.f ? 0.8f : 1.f;
			shL[s] = (sn < 0 ? lerp(18000.f, 2000.f, -sn) : 18000.f) * behind;
			shR[s] = (sn > 0 ? lerp(18000.f, 2000.f, sn) : 18000.f) * behind;
			// quad: pairwise equal power between speakers FL +45, FR -45, RL +135, RR -135 degrees
			for (int k = 0; k < 4; k++) panQ[s][k] = 0.f;
			// find adjacent pair
			float a = az;
			int order[4] = {1, 0, 2, 3};   // FR(-45) FL(45) RL(135) RR(-135) going counter-clockwise
			float angs[4] = {-PI / 4, PI / 4, 3 * PI / 4, 5 * PI / 4};
			if (a < -PI / 4) a += TWO_PI;
			for (int k = 0; k < 4; k++) {
				float a0 = angs[k], a1 = angs[(k + 1) % 4] + (k == 3 ? TWO_PI : 0.f);
				if (a >= a0 && a < a1) {
					float t = (a - a0) / (a1 - a0), w0, w1;
					eqPower(t, w0, w1);
					panQ[s][order[k]] = w0; panQ[s][order[(k + 1) % 4]] = w1;
					break;
				}
			}
		}
		float rt = 0.4f + 0.15f * S;
		static const float base[4] = {0.037f, 0.043f, 0.053f, 0.061f};
		for (int k = 0; k < 4; k++) fdnLen[k] = base[k] * S / 12.f * fs;
		fdnG = std::pow(10.f, -3.f * (base[1] * S / 12.f) / rt);
		// trail for the display
		trailT += dt;
		if (trailT > 0.25f) { trailT = 0.f; trailX[trailPos] = ex; trailY[trailPos] = ey; trailPos = (trailPos + 1) % 32; }
		dispX = ex; dispY = ey;
	}
	float dispX = 0.5f, dispY = 0.5f;

	// in[0..7] = AIR, in[8..15] = PICKUP; connected[] says which to process
	Out process(const float* in, const bool* connected, const Params& p) {
		Out o;
		for (int i = 0; i < N; i++) level[i] = rms[i].process(connected[i] ? in[i] : 0.f);
		if (ctl-- <= 0) { ctl = CTL_DIV; control(p); for (int s = 0; s < SRC; s++) { absorb[s].set(absF[s], fs); shadowL[s].set(shL[s], fs); shadowR[s].set(shR[s], fs); } }
		float roomIn = 0.f;
		for (int s = 0; s < SRC; s++) {
			if (!connected[s]) continue;
			dl[s].push(in[s]);
			float g = gainS[s];
			if (p.quad) {
				float y = absorb[s].process(dl[s].read(std::max(delS[s], 1.f))) * g;
				o.fl += y * panQ[s][0]; o.fr += y * panQ[s][1]; o.rl += y * panQ[s][2]; o.rr += y * panQ[s][3];
				roomIn += y;
			} else {
				float dL = delS[s] + std::max(0.f, -itd[s]), dR = delS[s] + std::max(0.f, itd[s]);
				float yl = dl[s].read(std::max(dL, 1.f)), yr = dl[s].read(std::max(dR, 1.f));
				float wl, wr;
				eqPower(0.5f - 0.5f * std::sin(panS[s]), wl, wr);
				yl = shadowL[s].process(yl) * g * wl;
				yr = shadowR[s].process(yr) * g * wr;
				o.l += yl; o.r += yr;
				roomIn += 0.5f * (yl + yr);
			}
		}
		// room: 4-line FDN with a Householder matrix
		float room = clampf(p.room, 0.f, 1.f);
		float y[4];
		for (int k = 0; k < 4; k++) y[k] = fdnLp[k].process(fdn[k].read(std::max(fdnLen[k], 2.f)));
		float sum = 0.5f * (y[0] + y[1] + y[2] + y[3]);
		for (int k = 0; k < 4; k++) fdn[k].push(softClamp((y[k] - sum) * fdnG + roomIn * 0.25f));
		if (p.quad) {
			o.fl += room * y[0]; o.fr += room * y[1]; o.rl += room * y[2]; o.rr += room * y[3];
			o.l = o.fl; o.r = o.fr;
		} else {
			o.l += room * (y[0] + y[2]) * 0.7f;
			o.r += room * (y[1] + y[3]) * 0.7f;
		}
		o.l = softClamp(o.l); o.r = softClamp(o.r);
		o.fl = softClamp(o.fl); o.fr = softClamp(o.fr); o.rl = softClamp(o.rl); o.rr = softClamp(o.rr);
		if (!finite(o.l) || !finite(o.r) || !finite(o.fl) || !finite(o.rr)) { reset(); o = Out(); }
		return o;
	}
};

} // namespace tudor
