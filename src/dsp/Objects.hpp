// OBJECT mode tables: the 14 Rainforest presets and the FIND generator.
// Plausible physics, not measurements of the real objects (see docs/DECISIONS.md).
#pragma once
#include "Noise.hpp"
#include <vector>
#include <string>

namespace tudor {

enum Shape : uint8_t { SH_LINE, SH_RECT, SH_CIRC, SH_CAVITY };

struct ModeDef {
	float f = 100.f;       // Hz at SIZE 0
	float fRef = 100.f;    // reference frequency of its component (for BRIGHTNESS slope)
	float t60 = 1.f;       // s at fRef
	float gain = 1.f;      // component weight
	uint8_t shape = SH_LINE;
	float m = 1.f, n = 0.f, off = 0.f;   // spatial shape indices and position offset
};

struct ObjectDef {
	std::string name;
	float f0 = 100.f;
	float t60 = 1.f;
	float betaScale = 1.f;
	float fRad = 200.f;
	float rattle = 0.f;
	float glide = 1.f;      // TICKLE pitch-glide multiplier
	std::vector<ModeDef> modes;
};

enum Material { MAT_METAL, MAT_WOOD, MAT_GLASS, MAT_PLASTIC, MAT_CARD, MAT_COUNT };
static const char* const MATERIAL_NAMES[MAT_COUNT] = {"Metal", "Wood", "Glass", "Plastic", "Card"};

constexpr int OBJECT_PRESETS = 14;
static const char* const OBJECT_NAMES[OBJECT_PRESETS + 1] = {
	"Bedspring", "Wine Barrel", "Oil Drum", "Wagon Rim", "Milk-Can Lid", "Copper Still", "Styrofoam Box",
	"Steel Cable", "Mica Sheet", "Gourd", "Glass Plate", "Aluminium Sheet", "Hard-Drive Platter", "Whisker Disk", "Found"};

struct ObjectBuilder {
	ObjectDef d;
	Rng rng;
	explicit ObjectBuilder(uint64_t seed) : rng(seed) {}
	float detune() { return 1.f + 0.008f * rng.bipolar(); }
	void add(float f, float fRef, float t60, float gain, uint8_t shape, float m, float n, float off) {
		ModeDef md;
		md.f = f * detune(); md.fRef = fRef; md.t60 = t60; md.gain = gain;
		md.shape = shape; md.m = m; md.n = n; md.off = off;
		d.modes.push_back(md);
	}
	// stiff string / cable / coil: f = f0 m sqrt(1 + B m^2)
	void string(float f0, int count, float B, float t60, float gain = 1.f, float off = 0.f) {
		for (int m = 1; m <= count; m++) add(f0 * m * std::sqrt(1.f + B * m * m), f0, t60, gain / std::sqrt((float)m), SH_LINE, (float)m, 0, off);
	}
	// free-free bar
	void bar(float f0, int count, float t60, float gain = 1.f, float off = 0.f) {
		static const float beta1 = 4.7300f;
		for (int k = 1; k <= count; k++) {
			float b = k == 1 ? beta1 : (2 * k + 1) * PI / 2.f;
			add(f0 * (b / beta1) * (b / beta1), f0, t60, gain / std::sqrt((float)k), SH_LINE, (float)k + 1.f, 0, off);
		}
	}
	// rectangular plate, aspect a: f ~ m^2 + (n/a)^2
	void rect(float f0, float a, int count, float t60, float gain = 1.f, float off = 0.f) {
		struct C { float r; int m, n; };
		std::vector<C> cs;
		float base = 1.f + 1.f / (a * a);
		for (int m = 1; m <= 12; m++)
			for (int n = 1; n <= 12; n++) cs.push_back({(m * m + (n * n) / (a * a)) / base, m, n});
		std::sort(cs.begin(), cs.end(), [](const C& x, const C& y) { return x.r < y.r; });
		for (int i = 0; i < count && i < (int)cs.size(); i++)
			add(f0 * cs[i].r, f0, t60, gain / std::pow(cs[i].r, 0.25f), SH_RECT, (float)cs[i].m, (float)cs[i].n, off);
	}
	// circular membrane-like series (drum heads, end caps)
	void membrane(float f0, int count, float t60, float gain = 1.f, float off = 0.f) {
		static const float r[] = {1.f, 1.594f, 2.136f, 2.296f, 2.653f, 2.918f, 3.156f, 3.501f, 3.600f, 3.652f, 4.060f, 4.154f, 4.601f, 4.832f, 4.903f, 5.131f};
		static const int nn[] = {0, 1, 2, 0, 3, 1, 4, 2, 0, 5, 3, 1, 4, 2, 0, 6};
		static const int mm[] = {1, 1, 1, 2, 1, 2, 1, 2, 3, 1, 2, 3, 2, 3, 4, 1};
		for (int i = 0; i < count; i++) {
			int j = i % 16, rep = i / 16;
			add(f0 * r[j] * (1.f + 1.15f * rep), f0, t60, gain / std::sqrt(1.f + i * 0.3f), SH_CIRC, (float)mm[j] + rep, (float)nn[j], off);
		}
	}
	// clamped circular plate (lids, platters)
	void clampedPlate(float f0, int count, float t60, float gain = 1.f, float off = 0.f, bool split = false) {
		static const float r[] = {1.f, 2.08f, 3.41f, 3.89f, 5.00f, 5.95f, 6.82f, 7.79f, 8.72f, 9.15f, 10.6f, 11.6f};
		for (int i = 0; i < count; i++) {
			float ratio = i < 12 ? r[i] : 11.6f + (i - 11) * 1.25f + 0.08f * (i - 11) * (i - 11);
			float n = (float)(i % 4), m = (float)(i / 4);
			add(f0 * ratio, f0, t60, gain / std::sqrt(ratio), SH_CIRC, m, n, off);
			if (split && n > 0 && (int)d.modes.size() < 64) add(f0 * ratio * 1.002f, f0, t60, gain / std::sqrt(ratio), SH_CIRC, m, n, off + 0.21f);
		}
	}
	// rings and shells: f ~ n(n^2-1)/sqrt(n^2+1), normalised at n = 2
	void ring(float f0, int nMax, float t60, float gain = 1.f, float split = 0.003f, int families = 1, float famScale = 1.6f, float off = 0.f) {
		for (int fam = 0; fam < families; fam++)
			for (int n = 2; n <= nMax; n++) {
				float r = n * (n * n - 1) / std::sqrt((float)n * n + 1) / 2.6833f;
				float f = f0 * r * std::pow(famScale, (float)fam);
				add(f, f0, t60, gain / std::sqrt(r) / (1 + fam), SH_CIRC, (float)fam, (float)n, off);
				if (split > 0.f) add(f * (1.f + split), f0, t60, gain / std::sqrt(r) / (1 + fam), SH_CIRC, (float)fam, (float)n, off + 0.17f);
			}
	}
	void cavity(float f, float t60, float gain = 1.f) { add(f, f, t60, gain, SH_CAVITY, 0, 0, 0); }

	ObjectDef finish(int maxModes) {
		std::sort(d.modes.begin(), d.modes.end(), [](const ModeDef& a, const ModeDef& b) { return a.f < b.f; });
		if ((int)d.modes.size() > maxModes) d.modes.resize(maxModes);
		return d;
	}
};

inline ObjectDef buildPreset(int idx) {
	ObjectBuilder b(0xBEEF + idx * 7919);
	ObjectDef& d = b.d;
	d.name = OBJECT_NAMES[std::min(std::max(idx, 0), OBJECT_PRESETS - 1)];
	switch (idx) {
	case 0: // BEDSPRING: eight coils, slightly different, strong rattle
		d.f0 = 95; d.t60 = 1.8f; d.betaScale = 0.8f; d.fRad = 150; d.rattle = 0.7f;
		for (int c = 0; c < 8; c++) b.string(95.f * (1.f + 0.04f * b.rng.bipolar()), 6, 0.002f, 1.8f, 1.f, c * 0.137f);
		return b.finish(48);
	case 1: // WINE BARREL: two air modes plus a wooden shell
		d.f0 = 70; d.t60 = 0.9f; d.betaScale = 1.2f; d.fRad = 120; d.rattle = 0.f;
		b.cavity(0.62f * 70, 0.4f, 1.2f); b.cavity(1.9f * 70, 0.4f, 0.8f);
		b.ring(140.f, 12, 0.9f, 1.f, 0.f, 3, 1.6f);   // n = 2 shell mode at 2.0 f0
		return b.finish(40);
	case 2: // OIL DRUM: end cap plus shell ring modes from 4.5 f0
		d.f0 = 55; d.t60 = 3.f; d.betaScale = 0.7f; d.fRad = 90; d.rattle = 0.1f;
		b.membrane(55, 36, 3.f);
		b.ring(55 * 4.5f, 11, 3.f, 0.7f, 0.002f, 1, 1.f, 0.3f);
		return b.finish(56);
	case 3: // WAGON RIM: cast-iron ring, in-plane and out-of-plane sets, split pairs
		d.f0 = 140; d.t60 = 4.5f; d.betaScale = 0.6f; d.fRad = 200;
		b.ring(140, 10, 4.5f, 1.f, 0.003f, 1, 1.f);
		b.ring(140 * 0.93f, 10, 4.5f, 0.8f, 0.003f, 1, 1.f, 0.4f);
		return b.finish(36);
	case 4: // MILK-CAN LID: clamped stainless disc
		d.f0 = 310; d.t60 = 2.2f; d.betaScale = 0.7f; d.fRad = 300; d.rattle = 0.15f;
		b.clampedPlate(310, 32, 2.2f, 1.f, 0.f, true);
		return b.finish(40);
	case 5: // COPPER STILL: air mode plus a softer, wider-spaced shell
		d.f0 = 90; d.t60 = 2.6f; d.betaScale = 0.88f; d.fRad = 140; d.rattle = 0.05f;
		b.cavity(45, 0.6f, 1.2f);
		b.membrane(90 * 1.2f, 30, 2.6f);
		b.ring(90 * 4.5f * 1.2f, 12, 2.6f, 0.7f, 0.002f, 1, 1.f, 0.3f);
		return b.finish(56);
	case 6: // STYROFOAM BOX: five faces, very short decay, one air mode
		d.f0 = 180; d.t60 = 0.08f; d.betaScale = 1.5f; d.fRad = 400;
		b.cavity(0.7f * 180, 0.1f, 1.f);
		for (int face = 0; face < 5; face++) b.rect(180.f * (1.f + 0.15f * face), 1.f + 0.15f * face, 6, 0.08f, 1.f, face * 0.19f);
		return b.finish(30);
	case 7: // STEEL CABLE: long stiff string, two polarisations beating slowly
		d.f0 = 38; d.t60 = 5.f; d.betaScale = 0.4f; d.fRad = 250; d.rattle = 0.5f;
		b.string(38, 32, 0.0004f, 5.f);
		b.string(38 * 1.004f, 32, 0.0004f, 5.f, 0.9f, 0.23f);
		return b.finish(64);
	case 8: // MICA SHEET: thin, crisp, crackly
		d.f0 = 240; d.t60 = 0.7f; d.betaScale = 0.9f; d.fRad = 500; d.rattle = 0.4f;
		b.rect(240, 1.3f, 48, 0.7f);
		return b.finish(48);
	case 9: // GOURD: air modes and three fishing-pole blanks
		d.f0 = 160; d.t60 = 1.2f; d.betaScale = 0.9f; d.fRad = 180; d.rattle = 0.2f;
		b.cavity(160, 0.3f, 1.3f); b.cavity(2.7f * 160, 0.3f, 0.8f);
		b.bar(22, 12, 3.f, 1.f, 0.1f); b.bar(29, 12, 3.f, 1.f, 0.4f); b.bar(37, 12, 3.f, 1.f, 0.7f);
		return b.finish(40);
	case 10: // GLASS PLATE
		d.f0 = 420; d.t60 = 2.f; d.betaScale = 0.5f; d.fRad = 600;
		b.rect(420, 1.4f, 48, 2.f);
		return b.finish(48);
	case 11: // ALUMINIUM SHEET: thunder sheet, dense, strong glide
		d.f0 = 35; d.t60 = 3.5f; d.betaScale = 0.6f; d.fRad = 160; d.rattle = 0.1f; d.glide = 1.5f;
		b.rect(35, 1.6f, 64, 3.5f);
		return b.finish(64);
	case 12: // HARD-DRIVE PLATTER: centre-clamped aluminium disc, very long ring
		d.f0 = 260; d.t60 = 7.f; d.betaScale = 0.4f; d.fRad = 450;
		{
			static const float r[] = {1.f, 1.09f, 1.41f, 2.11f, 3.09f, 4.12f, 5.34f, 6.82f};
			for (int i = 0; i < 24; i++) {
				float ratio = i < 8 ? r[i] : 6.82f + (i - 7) * 1.6f + 0.05f * (i - 7) * (i - 7);
				b.add(260 * ratio, 260, 7.f, 1.f / std::sqrt(ratio), SH_CIRC, (float)(i / 4), (float)(i % 4 + 1), 0.f);
				b.add(260 * ratio * 1.002f, 260, 7.f, 1.f / std::sqrt(ratio), SH_CIRC, (float)(i / 4), (float)(i % 4 + 1), 0.21f);
			}
		}
		return b.finish(48);
	default: // 13 WHISKER DISK: damped card disc plus twelve plastic whiskers
		d.name = OBJECT_NAMES[13];
		d.f0 = 130; d.t60 = 0.25f; d.betaScale = 1.3f; d.fRad = 300; d.rattle = 0.8f;
		b.membrane(130, 20, 0.25f);
		for (int w = 0; w < 12; w++) {
			float f = 700.f * std::pow(4.f, b.rng.uniform());
			b.bar(f, 3, 0.5f, 0.6f, 0.05f + w * 0.075f);
		}
		return b.finish(56);
	}
}

// FIND: a new random object of a material. Allocates: call off the audio thread.
inline ObjectDef buildFound(int material, uint64_t seed) {
	ObjectBuilder b(seed);
	Rng& r = b.rng;
	ObjectDef& d = b.d;
	d.name = OBJECT_NAMES[OBJECT_PRESETS];
	material = std::min(std::max(material, 0), MAT_COUNT - 1);
	struct Range { float t0, t1, b0, b1, r0, r1; };
	static const Range R[MAT_COUNT] = {
		{1.5f, 8.f, 0.4f, 0.8f, 0.f, 0.4f},   // metal
		{0.3f, 1.5f, 1.0f, 1.4f, 0.f, 0.2f},  // wood
		{1.f, 3.f, 0.4f, 0.6f, 0.f, 0.f},     // glass
		{0.2f, 1.f, 0.9f, 1.3f, 0.f, 0.5f},   // plastic
		{0.05f, 0.3f, 1.2f, 1.6f, 0.3f, 0.9f} // card
	};
	const Range& m = R[material];
	d.f0 = 30.f * std::pow(20.f, r.uniform());
	d.t60 = lerp(m.t0, m.t1, r.uniform());
	d.betaScale = lerp(m.b0, m.b1, r.uniform());
	d.rattle = lerp(m.r0, m.r1, r.uniform());
	d.fRad = d.f0 * lerp(1.f, 4.f, r.uniform());
	int count = 24 + r.below(41);
	int parts = 1 + r.below(3);
	for (int p = 0; p < parts; p++) {
		float f = d.f0 * (p == 0 ? 1.f : std::pow(2.f, r.bipolar() * 2.f));
		int n = count / parts;
		float off = p * 0.31f;
		switch (r.below(6)) {
			case 0: b.string(f, n, 0.0005f + 0.003f * r.uniform(), d.t60, 1.f, off); break;
			case 1: b.bar(f, std::min(n, 14), d.t60, 1.f, off); break;
			case 2: b.rect(f, 1.f + r.uniform(), n, d.t60, 1.f, off); break;
			case 3: b.membrane(f, n, d.t60, 1.f, off); break;
			case 4: b.ring(f, 2 + n / 2, d.t60, 1.f, 0.003f, 1, 1.f, off); break;
			default: b.cavity(f, d.t60 * 0.3f); b.clampedPlate(f * 1.7f, n, d.t60, 1.f, off); break;
		}
	}
	return b.finish(64);
}

} // namespace tudor
