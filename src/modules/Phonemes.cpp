// Phonemes group: SWELL, PHONEME, XGATE.
#include "Common.hpp"
#include "../dsp/Swell.hpp"
#include "../dsp/Phoneme.hpp"
#include "../dsp/XGate.hpp"

// ============================================================================ SWELL
struct SwellModule : Module, SwellIds {
	tudor::Swell model;
	tudor::Swell::Params pr;
	dsp::SchmittTrigger trig;
	dsp::PulseGenerator onsetPulse;
	tudor::Smoother sLevel;

	SwellModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configSwitch(SOURCE_PARAM, 0.f, 2.f, 1.f, "SOURCE", {"Noise", "Ping", "Capture"});
		configExp(this, ATTACK_PARAM, 0.001f, 8.f, 0.4f, "ATTACK", " ms", 1000.f);
		configExp(this, DECAY_PARAM, 0.005f, 10.f, 0.8f, "DECAY", " ms", 1000.f);
		configExp(this, TONE_PARAM, 40.f, 10000.f, 700.f, "TONE", " Hz");
		configParam(BODY_PARAM, 0.f, 1.f, 0.5f, "BODY", "%", 0.f, 100.f);
		configParam(SENSITIVITY_PARAM, 0.f, 1.f, 0.5f, "SENSITIVITY (onset detector)", "%", 0.f, 100.f);
		configParam(VELOCITY_PARAM, 0.f, 1.f, 0.7f, "VELOCITY", "%", 0.f, 100.f);
		configParam(SCATTER_PARAM, 0.f, 1.f, 0.2f, "SCATTER", "%", 0.f, 100.f);
		configParam(LEVEL_PARAM, 0.f, 1.f, 0.8f, "LEVEL", "%", 0.f, 100.f)->randomizeEnabled = false;
		configTrim(this, ATTACK_CV_PARAM, "ATTACK CV (1 oct of time per V at full)");
		configTrim(this, TONE_CV_PARAM, "TONE CV (1 V/oct at full)");
		configInput(ATTACK_CV_INPUT, "ATTACK CV");
		configInput(TONE_CV_INPUT, "TONE CV");
		configInput(TRIG_INPUT, "TRIG (fires a voice at full velocity)");
		configInput(IN_INPUT, "IN (onsets and CAPTURE source)");
		configOutput(OUT_L_OUTPUT, "OUT L");
		configOutput(OUT_R_OUTPUT, "OUT R");
		configOutput(ENV_OUTPUT, "ENV (sum of voice envelopes, 0 to 10 V)");
		configOutput(ONSET_OUTPUT, "ONSET (trigger)");
	}
	void onSampleRateChange(const SampleRateChangeEvent& e) override { model.setSampleRate(e.sampleRate); }
	void onReset(const ResetEvent& e) override { Module::onReset(e); model.reset(); }

	void process(const ProcessArgs& args) override {
		pr.source = (int)std::round(params[SOURCE_PARAM].getValue());
		pr.attack = math::clamp(expVal(this, ATTACK_PARAM, 0.001f, 8.f) * std::pow(2.f, math::clamp(cvAmt(this, ATTACK_CV_INPUT, ATTACK_CV_PARAM), -10.f, 10.f)), 0.001f, 8.f);
		pr.decay = expVal(this, DECAY_PARAM, 0.005f, 10.f);
		pr.toneHz = math::clamp(expVal(this, TONE_PARAM, 40.f, 10000.f) * std::pow(2.f, math::clamp(cvAmt(this, TONE_CV_INPUT, TONE_CV_PARAM), -10.f, 10.f)), 20.f, 16000.f);
		pr.body = params[BODY_PARAM].getValue();
		pr.sensitivity = params[SENSITIVITY_PARAM].getValue();
		pr.velocity = params[VELOCITY_PARAM].getValue();
		pr.scatter = params[SCATTER_PARAM].getValue();
		pr.level = sLevel.process(params[LEVEL_PARAM].getValue(), args.sampleTime);
		bool t = trig.process(inputs[TRIG_INPUT].getVoltage(), 0.1f, 1.f);
		tudor::Swell::Out o = model.process(inputs[IN_INPUT].getVoltage(), t, pr);
		outputs[OUT_L_OUTPUT].setVoltage(o.l);
		outputs[OUT_R_OUTPUT].setVoltage(o.r);
		outputs[ENV_OUTPUT].setVoltage(o.env);
		if (o.onset) onsetPulse.trigger(1e-3f);
		outputs[ONSET_OUTPUT].setVoltage(trigOut(onsetPulse, args.sampleTime));
	}
};

struct SwellWidget : ModuleWidget {
	SwellWidget(SwellModule* m) {
		tudorScaffold(this, m, "Swell");
		placeSwell<SwellModule>(this, m);
	}
};
Model* modelSwell = createModel<SwellModule, SwellWidget>("Swell");

// ============================================================================ PHONEME
struct PhonemeModule : Module, PhonemeIds {
	static constexpr int B = tudor::Phoneme::B;
	tudor::Phoneme model;
	tudor::Phoneme::Params pr;
	tudor::Smoother sMix;
	float env[B] = {};
	bool gateOpen = false;

	PhonemeModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configSwitch(CARRIER_PARAM, 0.f, 2.f, 1.f, "CARRIER", {"Input (CARRIER jack)", "Self (the modulator)", "Noise"});
		configExp(this, CHOP_RATE_PARAM, 1.f, 60.f, 9.f, "CHOP RATE", " Hz");
		configParam(CHOP_SIZE_PARAM, 0.05f, 1.f, 0.55f, "CHOP SIZE (open part of each chop)", "%", 0.f, 100.f);
		configParam(JITTER_PARAM, 0.f, 1.f, 0.25f, "JITTER", "%", 0.f, 100.f);
		configParam(SHUFFLE_PARAM, 0.f, 1.f, 0.f, "SHUFFLE", "%", 0.f, 100.f);
		configExp(this, RESPONSE_PARAM, 0.002f, 0.2f, 0.015f, "RESPONSE (envelope release)", " ms", 1000.f);
		configParam(FORMANT_PARAM, -1.f, 1.f, 0.f, "FORMANT", " oct");
		configSwitch(HOLD_PARAM, 0.f, 1.f, 1.f, "HOLD", {"Track", "Hold"});
		configSwitch(FREEZE_PARAM, 0.f, 1.f, 0.f, "FREEZE", {"Off", "On"});
		configParam(MIX_PARAM, 0.f, 1.f, 1.f, "MIX (dry to vocoded)", "%", 0.f, 100.f);
		configTrim(this, RATE_CV_PARAM, "RATE CV (1 oct/V at full)");
		configTrim(this, SIZE_CV_PARAM, "SIZE CV (10%/V at full)");
		configTrim(this, SHUFFLE_CV_PARAM, "SHUF CV (10%/V at full)");
		configInput(RATE_CV_INPUT, "RATE CV");
		configInput(SIZE_CV_INPUT, "SIZE CV");
		configInput(SHUFFLE_CV_INPUT, "SHUFFLE CV");
		configInput(FREEZE_INPUT, "FREEZE (gate)");
		configInput(CHOP_INPUT, "CHOP (external chop clock)");
		configInput(CARRIER_INPUT, "CARRIER");
		configInput(MODULATOR_INPUT, "MODULATOR");
		configOutput(OUT_OUTPUT, "OUT");
		configOutput(ODD_OUTPUT, "ODD (odd bands)");
		configOutput(EVEN_OUTPUT, "EVEN (even bands)");
		configOutput(ENVELOPES_OUTPUT, "ENVELOPES (poly 16, chopped band levels)");
		configLight(FREEZE_LIGHT, "Freeze");
		configBypass(MODULATOR_INPUT, OUT_OUTPUT);
	}
	void onSampleRateChange(const SampleRateChangeEvent& e) override { model.setSampleRate(e.sampleRate); }
	void onReset(const ResetEvent& e) override { Module::onReset(e); model.reset(); }

	void process(const ProcessArgs& args) override {
		pr.carrier = (int)std::round(params[CARRIER_PARAM].getValue());
		pr.chopRate = math::clamp(expVal(this, CHOP_RATE_PARAM, 1.f, 60.f) * std::pow(2.f, math::clamp(cvAmt(this, RATE_CV_INPUT, RATE_CV_PARAM), -10.f, 10.f)), 0.25f, 200.f);
		pr.chopSize = math::clamp(params[CHOP_SIZE_PARAM].getValue() + 0.1f * cvAmt(this, SIZE_CV_INPUT, SIZE_CV_PARAM), 0.05f, 1.f);
		pr.jitter = params[JITTER_PARAM].getValue();
		pr.shuffle = math::clamp(params[SHUFFLE_PARAM].getValue() + 0.1f * cvAmt(this, SHUFFLE_CV_INPUT, SHUFFLE_CV_PARAM), 0.f, 1.f);
		pr.response = expVal(this, RESPONSE_PARAM, 0.002f, 0.2f);
		pr.formant = params[FORMANT_PARAM].getValue();
		pr.hold = params[HOLD_PARAM].getValue() > 0.5f;
		pr.freeze = params[FREEZE_PARAM].getValue() > 0.5f || inputs[FREEZE_INPUT].getVoltage() >= 1.f;
		pr.mix = sMix.process(params[MIX_PARAM].getValue(), args.sampleTime);
		tudor::Phoneme::Out o = model.process(inputs[MODULATOR_INPUT].getVoltage(), inputs[CARRIER_INPUT].getVoltage(), inputs[CARRIER_INPUT].isConnected(),
		                                      inputs[CHOP_INPUT].getVoltage(), inputs[CHOP_INPUT].isConnected(), pr);
		outputs[OUT_OUTPUT].setVoltage(o.out);
		outputs[ODD_OUTPUT].setVoltage(o.odd);
		outputs[EVEN_OUTPUT].setVoltage(o.even);
		outputs[ENVELOPES_OUTPUT].setChannels(B);
		for (int b = 0; b < B; b++) {
			outputs[ENVELOPES_OUTPUT].setVoltage(o.env[b], b);
			env[b] = o.env[b];
		}
		gateOpen = o.gateOpen;
		lights[FREEZE_LIGHT].setBrightness(pr.freeze ? 1.f : 0.f);
	}
};

struct PhonemeDisplay : TudorDisplay {
	PhonemeModule* module = nullptr;
	void drawContents(const DrawArgs& args) override {
		NVGcontext* vg = args.vg;
		float W = box.size.x, H = box.size.y;
		const int B = PhonemeModule::B;
		float x0 = 4.f, x1 = W - 14.f, colW = (x1 - x0) / B;
		float barTop = 3.f, barBot = H * 0.55f;
		float rowA = barBot + 4.f, rowS = H - 4.f;
		for (int b = 0; b < B; b++) {
			float cx = x0 + colW * (b + 0.5f);
			float v = module ? math::clamp(module->env[b] / 10.f, 0.f, 1.f) : 0.f;
			v = std::sqrt(v);
			nvgBeginPath(vg);
			nvgRect(vg, cx - colW * 0.35f, barBot - (barBot - barTop) * v, colW * 0.7f, (barBot - barTop) * v);
			nvgFillColor(vg, ink(0.85f));
			nvgFill(vg);
		}
		// shuffle map: analysis row (top) to synthesis row (bottom)
		nvgBeginPath(vg);
		for (int a = 0; a < B; a++) {
			int s = module ? math::clamp(module->model.map[a], 0, B - 1) : a;
			nvgMoveTo(vg, x0 + colW * (a + 0.5f), rowA);
			nvgLineTo(vg, x0 + colW * (s + 0.5f), rowS);
		}
		nvgStrokeColor(vg, ink(0.45f));
		nvgStrokeWidth(vg, 0.7f);
		nvgStroke(vg);
		// chop gate
		bool open = module && module->gateOpen;
		nvgBeginPath(vg);
		nvgCircle(vg, W - 7.f, H * 0.5f, 3.f);
		if (open) {
			nvgFillColor(vg, ink());
			nvgFill(vg);
		} else {
			nvgStrokeColor(vg, ink(0.5f));
			nvgStrokeWidth(vg, 0.8f);
			nvgStroke(vg);
		}
	}
};

struct PhonemeWidget : ModuleWidget {
	PhonemeWidget(PhonemeModule* m) {
		tudorScaffold(this, m, "Phoneme");
		auto* d = new PhonemeDisplay;
		d->module = m;
		d->setRectMm(PHONEME_DISPLAY_MM);
		addChild(d);
		placePhoneme<PhonemeModule>(this, m);
	}
};
Model* modelPhoneme = createModel<PhonemeModule, PhonemeWidget>("Phoneme");

// ============================================================================ XGATE
struct XGateModule : Module, XGateIds {
	static constexpr int C = tudor::XGate::C, O = tudor::XGate::O;
	tudor::XGate model;
	tudor::XGate::Params pr;
	dsp::SchmittTrigger switchTrig;
	dsp::PulseGenerator movedPulse;
	tudor::Smoother sLevel[C];
	float in[C] = {};
	int lightDiv = 0;

	XGateModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		for (int c = 0; c < C; c++) {
			configSwitch(MODE1_PARAM + 3 * c, 0.f, 2.f, 0.f, string::f("MODE %d", c + 1), {"Open (sounds while key is loud)", "Duck (sounds while key is quiet)", "Off (no gating)"});
			configSwitch(KEY1_PARAM + 3 * c, 0.f, 3.f, (float)((c + 1) % C), string::f("KEY %d (channel whose level gates this one)", c + 1), {"1", "2", "3", "4"});
			configParam(LEVEL1_PARAM + 3 * c, 0.f, 1.f, 0.8f, string::f("LEVEL %d", c + 1), "%", 0.f, 100.f)->randomizeEnabled = false;
			configInput(IN1_INPUT + c, string::f("IN %d", c + 1));
			configLight(GATE1_LIGHT + c, string::f("Gate %d", c + 1));
		}
		configParam(THRESHOLD_PARAM, -60.f, 0.f, -24.f, "THRESHOLD", " dBFS");
		configParam(DEPTH_PARAM, 0.f, 1.f, 1.f, "DEPTH", "%", 0.f, 100.f);
		configExp(this, SPEED_PARAM, 0.001f, 1.f, 0.03f, "SPEED (gate attack, release is 4×)", " ms", 1000.f);
		configExp(this, SWITCH_RATE_PARAM, 0.02f, 10.f, 0.3f, "SWITCH RATE", " Hz");
		configParam(RANDOMNESS_PARAM, 0.f, 1.f, 0.5f, "RANDOMNESS", "%", 0.f, 100.f);
		configExp(this, GLIDE_PARAM, 0.001f, 4.f, 0.15f, "GLIDE", " ms", 1000.f);
		configSwitch(OUTPUTS_PARAM, 0.f, 1.f, 1.f, "OUTPUTS", {"4", "6"});
		configTrim(this, RATE_CV_PARAM, "RATE CV (1 oct/V at full)");
		configInput(RATE_CV_INPUT, "RATE CV");
		configInput(SWITCH_INPUT, "SWITCH (trigger: force a move)");
		for (int k = 0; k < O; k++) configOutput(OUT1_OUTPUT + k, string::f("OUT %d", k + 1));
		configOutput(MOVED_OUTPUT, "MOVED (trigger)");
	}
	void onSampleRateChange(const SampleRateChangeEvent& e) override { model.setSampleRate(e.sampleRate); }
	void onReset(const ResetEvent& e) override { Module::onReset(e); model.reset(); }

	void process(const ProcessArgs& args) override {
		for (int c = 0; c < C; c++) {
			pr.mode[c] = (int)std::round(params[MODE1_PARAM + 3 * c].getValue());
			pr.key[c] = (int)std::round(params[KEY1_PARAM + 3 * c].getValue());
			pr.level[c] = sLevel[c].process(params[LEVEL1_PARAM + 3 * c].getValue(), args.sampleTime);
			in[c] = inputs[IN1_INPUT + c].getVoltage();
		}
		pr.thresholdDb = params[THRESHOLD_PARAM].getValue();
		pr.depth = params[DEPTH_PARAM].getValue();
		pr.speed = expVal(this, SPEED_PARAM, 0.001f, 1.f);
		pr.outputs = params[OUTPUTS_PARAM].getValue() > 0.5f ? 6 : 4;
		pr.switchRate = math::clamp(expVal(this, SWITCH_RATE_PARAM, 0.02f, 10.f) * std::pow(2.f, math::clamp(cvAmt(this, RATE_CV_INPUT, RATE_CV_PARAM), -10.f, 10.f)), 0.005f, 50.f);
		pr.randomness = params[RANDOMNESS_PARAM].getValue();
		pr.glide = expVal(this, GLIDE_PARAM, 0.001f, 4.f);
		bool sw = switchTrig.process(inputs[SWITCH_INPUT].getVoltage(), 0.1f, 1.f);
		tudor::XGate::Out o = model.process(in, sw, pr);
		for (int k = 0; k < O; k++) outputs[OUT1_OUTPUT + k].setVoltage(o.out[k]);
		if (o.moved) movedPulse.trigger(1e-3f);
		outputs[MOVED_OUTPUT].setVoltage(trigOut(movedPulse, args.sampleTime));

		if (lightDiv-- <= 0) {
			lightDiv = 64;
			for (int c = 0; c < C; c++) {
				lights[GATE1_LIGHT + c].setBrightness(o.gate[c]);
				for (int k = 0; k < O; k++) {
					float b = 0.f;
					float x = math::clamp(model.xf[c], 0.f, 1.f);
					if (k == model.dest[c]) b = x < 1.f ? std::sin(x * (float)M_PI * 0.5f) : 1.f;
					else if (k == model.from[c] && x < 1.f) b = std::cos(x * (float)M_PI * 0.5f);
					lights[ROUTE11_LIGHT + O * c + k].setBrightness(b);
				}
			}
		}
	}
};

struct XGateWidget : ModuleWidget {
	XGateWidget(XGateModule* m) {
		tudorScaffold(this, m, "XGate");
		placeXGate<XGateModule>(this, m);
	}
};
Model* modelXGate = createModel<XGateModule, XGateWidget>("XGate");
