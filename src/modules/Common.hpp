// Helpers shared by the Rack wrappers.
#pragma once
#include "../plugin.hpp"
#include "../generated/Layout.hpp"
#include "../dsp/Common.hpp"

// Exponential knob: stored value 0..1, shown as lo·(hi/lo)^v in `unit`.
inline ParamQuantity* configExp(Module* m, int id, float lo, float hi, float def, const std::string& name, const std::string& unit, float mult = 1.f) {
	return m->configParam(id, 0.f, 1.f, tudor::expNorm(def, lo, hi), name, unit, hi / lo, lo * mult);
}
inline float expVal(Module* m, int id, float lo, float hi) { return tudor::expRange(m->params[id].getValue(), lo, hi); }

// CV through its trimpot: volts × trim (−1..1).
inline float cvAmt(Module* m, int input, int trim) {
	if (!m->inputs[input].isConnected()) return 0.f;
	return m->inputs[input].getVoltage() * m->params[trim].getValue();
}

inline ParamQuantity* configTrim(Module* m, int id, const std::string& name, float def = 0.f) {
	return m->configParam(id, -1.f, 1.f, def, name, "%", 0.f, 100.f);
}

// json helpers
inline float jsonFloat(json_t* root, const char* key, float def) {
	json_t* j = json_object_get(root, key);
	return j ? (float)json_number_value(j) : def;
}
inline int jsonInt(json_t* root, const char* key, int def) {
	json_t* j = json_object_get(root, key);
	return j ? (int)json_integer_value(j) : def;
}
inline json_t* jsonFloats(const float* v, int n) {
	json_t* a = json_array();
	for (int i = 0; i < n; i++) json_array_append_new(a, json_real(v[i]));
	return a;
}
inline void readFloats(json_t* root, const char* key, float* v, int n) {
	json_t* a = json_object_get(root, key);
	if (!a || !json_is_array(a)) return;
	for (int i = 0; i < n && i < (int)json_array_size(a); i++) v[i] = (float)json_number_value(json_array_get(a, i));
}

inline float trigOut(dsp::PulseGenerator& g, float dt) { return g.process(dt) ? 10.f : 0.f; }
