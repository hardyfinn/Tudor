// Pulsers group: PULSER, SHIFT, TUNED, SIDEBAND, LOOP.
#include "Common.hpp"
#include "../dsp/Pulser.hpp"
#include "../dsp/Shift.hpp"
#include "../dsp/Tuned.hpp"
#include "../dsp/Sideband.hpp"
#include "../dsp/Loop.hpp"

// ============================================================================ PULSER
struct PulserModule : Module, PulserIds {
	tudor::Pulser model{random::u64()};
	tudor::Pulser::Params pr;
	tudor::Pulser::In pin;
	dsp::SchmittTrigger fireTrig;
	float flash = 0.f;
	static constexpr float TB_LO = 0.25f, TB_HI = 1000.f, H_LO = 0.001f, H_HI = 2.f;

	PulserModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configExp(this, TIME_BASE_PARAM, TB_LO, TB_HI, 6.f, "TIME BASE (loop resonance)", " Hz");
		configParam(GAIN_PARAM, 0.f, 3.f, 2.f, "GAIN (loop gain)");
		configParam(PHASE_PARAM, -180.f, 180.f, 0.f, "PHASE (extra shift in the loop)", "°");
		configParam(THRESHOLD_PARAM, 0.1f, 8.f, 2.5f, "THRESHOLD (firing level)", " V");
		configExp(this, HOLD_PARAM, H_LO, H_HI, 0.04f, "HOLD (quench time after each firing)", " ms", 1000.f);
		configParam(QUENCH_PARAM, 0.f, 1.f, 0.85f, "QUENCH", "%", 0.f, 100.f);
		configParam(INSTABILITY_PARAM, 0.f, 1.f, 0.15f, "INSTABILITY", "%", 0.f, 100.f);
		configParam(ACCENT_PARAM, 0.f, 1.f, 0.5f, "ACCENT", "%", 0.f, 100.f);
		configParam(COUPLE_PARAM, -1.f, 1.f, 0.f, "COUPLE (amount of COUPLE IN)", "%", 0.f, 100.f);
		configTrim(this, TIME_CV_PARAM, "TIME CV (1 oct/V at full)");
		configTrim(this, GAIN_CV_PARAM, "GAIN CV (0.3/V at full)");
		configTrim(this, PHASE_CV_PARAM, "PHASE CV (36°/V at full)");
		configTrim(this, INSTAB_CV_PARAM, "INSTAB CV (10%/V at full)");
		configInput(TIME_CV_INPUT, "TIME CV");
		configInput(GAIN_CV_INPUT, "GAIN CV");
		configInput(PHASE_CV_INPUT, "PHASE CV");
		configInput(INSTAB_CV_INPUT, "INSTAB CV");
		configInput(COUPLE_INPUT, "COUPLE IN (another PULSER's COUPLE OUT)");
		configInput(KICK_INPUT, "KICK (added into the loop)");
		configInput(FIRE_INPUT, "FIRE (trigger)");
		configOutput(CLICK_OUTPUT, "CLICK");
		configOutput(SNAP_OUTPUT, "SNAP");
		configOutput(THUMP_OUTPUT, "THUMP");
		configOutput(SPUTTER_OUTPUT, "SPUTTER");
		configOutput(LOOP_OUTPUT, "LOOP (raw loop signal)");
		configOutput(COUPLE_OUTPUT, "COUPLE OUT (for chaining)");
		configOutput(GATE_OUTPUT, "GATE (high during HOLD)");
		configLight(FIRE_LIGHT, "Fire");
	}
	void onSampleRateChange(const SampleRateChangeEvent& e) override { model.setSampleRate(e.sampleRate); }
	void onReset(const ResetEvent& e) override { Module::onReset(e); model.reset(); }

	void process(const ProcessArgs& args) override {
		float tb = expVal(this, TIME_BASE_PARAM, TB_LO, TB_HI) * std::pow(2.f, math::clamp(cvAmt(this, TIME_CV_INPUT, TIME_CV_PARAM), -10.f, 10.f));
		pr.timeBase = math::clamp(tb, TB_LO, TB_HI);
		pr.gain = math::clamp(params[GAIN_PARAM].getValue() + 0.3f * cvAmt(this, GAIN_CV_INPUT, GAIN_CV_PARAM), 0.f, 3.f);
		pr.phaseDeg = math::clamp(params[PHASE_PARAM].getValue() + 36.f * cvAmt(this, PHASE_CV_INPUT, PHASE_CV_PARAM), -180.f, 180.f);
		pr.threshold = params[THRESHOLD_PARAM].getValue();
		pr.hold = expVal(this, HOLD_PARAM, H_LO, H_HI);
		pr.quench = params[QUENCH_PARAM].getValue();
		pr.instability = math::clamp(params[INSTABILITY_PARAM].getValue() + 0.1f * cvAmt(this, INSTAB_CV_INPUT, INSTAB_CV_PARAM), 0.f, 1.f);
		pr.accent = params[ACCENT_PARAM].getValue();
		pr.couple = params[COUPLE_PARAM].getValue();
		pin.couple = inputs[COUPLE_INPUT].getVoltage();
		pin.kick = inputs[KICK_INPUT].getVoltage();
		pin.fire = fireTrig.process(inputs[FIRE_INPUT].getVoltage(), 0.1f, 1.f);
		tudor::Pulser::Out o = model.process(pin, pr);
		outputs[CLICK_OUTPUT].setVoltage(o.click);
		outputs[SNAP_OUTPUT].setVoltage(o.snap);
		outputs[THUMP_OUTPUT].setVoltage(o.thump);
		outputs[SPUTTER_OUTPUT].setVoltage(o.sputter);
		outputs[LOOP_OUTPUT].setVoltage(o.loop);
		outputs[COUPLE_OUTPUT].setVoltage(o.coupleOut);
		outputs[GATE_OUTPUT].setVoltage(o.gate);
		if (o.fired) flash = 0.05f;
		flash -= args.sampleTime;
		lights[FIRE_LIGHT].setBrightnessSmooth(flash > 0.f ? 1.f : 0.f, args.sampleTime);
	}
};

struct PulserWidget : ModuleWidget {
	PulserWidget(PulserModule* m) {
		tudorScaffold(this, m, "Pulser");
		placePulser<PulserModule>(this, m);
	}
};
Model* modelPulser = createModel<PulserModule, PulserWidget>("Pulser");

// ============================================================================ SHIFT
struct ShiftModule : Module, ShiftIds {
	tudor::Shift model;
	tudor::Shift::Params pr;
	tudor::Smoother sGain;
	static constexpr int STAGES[5] = {0, 1, 2, 4, 8};

	ShiftModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configSwitch(DEGREES_PARAM, 0.f, 3.f, 0.f, "DEGREES (fixed broadband shift)", {"0°", "90°", "180°", "270°"});
		configExp(this, PHASE_PARAM, 20.f, 10000.f, 600.f, "PHASE (centre of the all-pass chain)", " Hz");
		configSwitch(STAGES_PARAM, 0.f, 4.f, 2.f, "STAGES", {"0", "1", "2", "4", "8"});
		configParam(SPREAD_PARAM, 0.f, 3.f, 0.5f, "SPREAD", " oct");
		configParam(GAIN_PARAM, -2.f, 2.f, 1.f, "GAIN (negative inverts)", "×");
		configParam(SATURATE_PARAM, 0.f, 1.f, 0.f, "DRIVE (soft saturation)", "%", 0.f, 100.f);
		configTrim(this, PHASE_CV_PARAM, "PHASE CV (1 oct/V at full)");
		configTrim(this, GAIN_CV_PARAM, "GAIN CV (0.4/V at full)");
		configInput(PHASE_CV_INPUT, "PHASE CV");
		configInput(GAIN_CV_INPUT, "GAIN CV");
		configInput(IN1_INPUT, "IN 1");
		configInput(IN2_INPUT, "IN 2");
		configOutput(OUT_OUTPUT, "OUT");
		configOutput(INV_OUTPUT, "INV (−OUT)");
		configBypass(IN1_INPUT, OUT_OUTPUT);
	}
	void onSampleRateChange(const SampleRateChangeEvent& e) override { model.setSampleRate(e.sampleRate); }
	void onReset(const ResetEvent& e) override { Module::onReset(e); model.reset(); }

	void process(const ProcessArgs& args) override {
		pr.degrees = (int)std::round(params[DEGREES_PARAM].getValue());
		pr.phaseHz = math::clamp(expVal(this, PHASE_PARAM, 20.f, 10000.f) * std::pow(2.f, math::clamp(cvAmt(this, PHASE_CV_INPUT, PHASE_CV_PARAM), -10.f, 10.f)), 20.f, 20000.f);
		pr.stages = STAGES[math::clamp((int)std::round(params[STAGES_PARAM].getValue()), 0, 4)];
		pr.spreadOct = params[SPREAD_PARAM].getValue();
		pr.gain = sGain.process(math::clamp(params[GAIN_PARAM].getValue() + 0.4f * cvAmt(this, GAIN_CV_INPUT, GAIN_CV_PARAM), -2.f, 2.f), args.sampleTime);
		pr.saturate = params[SATURATE_PARAM].getValue();
		float y = model.process(inputs[IN1_INPUT].getVoltage() + inputs[IN2_INPUT].getVoltage(), pr);
		outputs[OUT_OUTPUT].setVoltage(y);
		outputs[INV_OUTPUT].setVoltage(-y);
	}
};

struct ShiftWidget : ModuleWidget {
	ShiftWidget(ShiftModule* m) {
		tudorScaffold(this, m, "Shift");
		placeShift<ShiftModule>(this, m);
	}
};
Model* modelShift = createModel<ShiftModule, ShiftWidget>("Shift");

// ============================================================================ TUNED
struct TunedModule : Module, TunedIds {
	tudor::Tuned model;
	tudor::Tuned::Params pr;
	tudor::Smoother sTune, sGain, sLevel;

	TunedModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configExp(this, TUNE_PARAM, 20.f, 10000.f, 440.f, "TUNE", " Hz");
		configExp(this, Q_PARAM, 0.5f, 200.f, 8.f, "Q", "");
		configParam(GAIN_PARAM, -12.f, 40.f, 0.f, "GAIN (input)", " dB");
		configSwitch(SATURATE_PARAM, 0.f, 2.f, 1.f, "SATURATE", {"Clean", "Tube", "Hard"});
		configParam(LEVEL_PARAM, 0.f, 1.f, 0.8f, "LEVEL", "%", 0.f, 100.f);
		configTrim(this, TUNE_CV_PARAM, "TUNE CV (1 V/oct at full)", 1.f);
		configInput(TUNE_CV_INPUT, "TUNE CV");
		configInput(IN_INPUT, "IN");
		configOutput(OUT_OUTPUT, "OUT (band-pass)");
		configOutput(INV_OUTPUT, "INV (−OUT)");
		configBypass(IN_INPUT, OUT_OUTPUT);
	}
	void onSampleRateChange(const SampleRateChangeEvent& e) override { model.setSampleRate(e.sampleRate); }
	void onReset(const ResetEvent& e) override { Module::onReset(e); model.reset(); }

	void process(const ProcessArgs& args) override {
		// smooth in octaves so sweeps are even
		float oct = std::log2(expVal(this, TUNE_PARAM, 20.f, 10000.f)) + cvAmt(this, TUNE_CV_INPUT, TUNE_CV_PARAM);
		pr.tuneHz = math::clamp(std::pow(2.f, sTune.process(oct, args.sampleTime)), 20.f, 0.45f * args.sampleRate);
		pr.q = expVal(this, Q_PARAM, 0.5f, 200.f);
		pr.gainDb = sGain.process(params[GAIN_PARAM].getValue(), args.sampleTime);
		pr.mode = (int)std::round(params[SATURATE_PARAM].getValue());
		pr.level = sLevel.process(params[LEVEL_PARAM].getValue(), args.sampleTime);
		float y = model.process(inputs[IN_INPUT].getVoltage(), pr);
		outputs[OUT_OUTPUT].setVoltage(y);
		outputs[INV_OUTPUT].setVoltage(-y);
	}
};

struct TunedWidget : ModuleWidget {
	TunedWidget(TunedModule* m) {
		tudorScaffold(this, m, "Tuned");
		placeTuned<TunedModule>(this, m);
	}
};
Model* modelTuned = createModel<TunedModule, TunedWidget>("Tuned");

// ============================================================================ SIDEBAND
struct SidebandModule : Module, SidebandIds {
	tudor::Sideband model;
	tudor::Sideband::Params pr;
	tudor::Smoother sSb, sCl, sSl, sLevel;

	SidebandModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(SIDEBAND_PARAM, -1.f, 1.f, 0.f, "SIDEBAND (−1 lower, 0 both, +1 upper)");
		configParam(CARRIER_LEAK_PARAM, 0.f, 1.f, 0.f, "CARRIER LEAK", "%", 0.f, 100.f);
		configParam(SIGNAL_LEAK_PARAM, 0.f, 1.f, 0.f, "SIGNAL LEAK", "%", 0.f, 100.f);
		configParam(DRIVE_PARAM, 0.f, 1.f, 0.f, "DRIVE", "%", 0.f, 100.f);
		configParam(LEVEL_PARAM, 0.f, 2.f, 1.f, "LEVEL", "%", 0.f, 100.f);
		configTrim(this, SIDEBAND_CV_PARAM, "SIDEBAND CV (0.2/V at full)");
		configInput(SIDEBAND_CV_INPUT, "SIDEBAND CV");
		configInput(SIGNAL_INPUT, "SIGNAL");
		configInput(CARRIER_INPUT, "CARRIER");
		configOutput(OUT_OUTPUT, "OUT");
		configOutput(UPPER_OUTPUT, "UPPER (upper sideband only)");
		configOutput(LOWER_OUTPUT, "LOWER (lower sideband only)");
	}
	void onReset(const ResetEvent& e) override { Module::onReset(e); model.reset(); }

	void process(const ProcessArgs& args) override {
		float dt = args.sampleTime;
		pr.sideband = sSb.process(math::clamp(params[SIDEBAND_PARAM].getValue() + 0.2f * cvAmt(this, SIDEBAND_CV_INPUT, SIDEBAND_CV_PARAM), -1.f, 1.f), dt);
		pr.carrierLeak = sCl.process(params[CARRIER_LEAK_PARAM].getValue(), dt);
		pr.signalLeak = sSl.process(params[SIGNAL_LEAK_PARAM].getValue(), dt);
		pr.drive = params[DRIVE_PARAM].getValue();
		pr.level = sLevel.process(params[LEVEL_PARAM].getValue(), dt);
		tudor::Sideband::Out o = model.process(inputs[SIGNAL_INPUT].getVoltage(), inputs[CARRIER_INPUT].getVoltage(), pr);
		outputs[OUT_OUTPUT].setVoltage(o.out);
		outputs[UPPER_OUTPUT].setVoltage(o.upper);
		outputs[LOWER_OUTPUT].setVoltage(o.lower);
	}
};

struct SidebandWidget : ModuleWidget {
	SidebandWidget(SidebandModule* m) {
		tudorScaffold(this, m, "Sideband");
		placeSideband<SidebandModule>(this, m);
	}
};
Model* modelSideband = createModel<SidebandModule, SidebandWidget>("Sideband");

// ============================================================================ LOOP
struct LoopModule : Module, LoopIds {
	static constexpr int N = tudor::Loop::N;
	tudor::Loop model;
	tudor::Loop::Params pr;
	tudor::Loop::Io io;
	tudor::Smoother sM[N][N], sLevel[N];

	int mParam(int from, int to) const { return M11_PARAM + 4 * from + to; }
	int levelParam(int j) const { return LEVEL1_PARAM + 2 * j; }
	int keepParam(int j) const { return KEEP1_PARAM + 2 * j; }

	LoopModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		for (int i = 0; i < N; i++)
			for (int j = 0; j < N; j++) {
				bool ring = j == (i + 1) % N;
				configParam(mParam(i, j), -1.5f, 1.5f, ring ? 0.7f : 0.f, string::f("%d→%d (bus %d return into bus %d)", i + 1, j + 1, i + 1, j + 1));
			}
		for (int j = 0; j < N; j++) {
			configParam(levelParam(j), 0.f, 1.f, 0.8f, string::f("LEVEL %d", j + 1), "%", 0.f, 100.f);
			configSwitch(keepParam(j), 0.f, 1.f, 0.f, string::f("KEEP %d (keeper)", j + 1), {"On", "Off"});
			configInput(IN1_INPUT + 2 * j, string::f("IN %d", j + 1));
			configInput(RETURN1_INPUT + 2 * j, string::f("RETURN %d (unpatched = SEND %d)", j + 1, j + 1));
			configOutput(SEND1_OUTPUT + 2 * j, string::f("SEND %d", j + 1));
			configOutput(OUT1_OUTPUT + 2 * j, string::f("OUT %d", j + 1));
		}
		configParam(EDGE_PARAM, 0.f, 1.f, 0.4f, "EDGE (keeper target)", "%", 0.f, 100.f);
		configExp(this, SPEED_PARAM, 0.1f, 10.f, 1.5f, "KEEPER SPEED", " s");
		configParam(ROTATE_PARAM, 0.f, 1.f, 0.f, "ROTATE", "%", 0.f, 100.f);
		configTrim(this, EDGE_CV_PARAM, "EDGE CV (10%/V at full)");
		configInput(EDGE_CV_INPUT, "EDGE CV");
		configOutput(MIX_L_OUTPUT, "MIX L (buses 1 and 3)");
		configOutput(MIX_R_OUTPUT, "MIX R (buses 2 and 4)");
	}
	void onSampleRateChange(const SampleRateChangeEvent& e) override { model.setSampleRate(e.sampleRate); }
	void onReset(const ResetEvent& e) override { Module::onReset(e); model.reset(); }

	void process(const ProcessArgs& args) override {
		float dt = args.sampleTime;
		for (int i = 0; i < N; i++)
			for (int j = 0; j < N; j++) {
				float g = sM[i][j].process(params[mParam(i, j)].getValue(), dt, 0.02f);
				pr.m[i][j] = std::fabs(g) < 1e-4f ? 0.f : g;
			}
		for (int j = 0; j < N; j++) {
			pr.level[j] = sLevel[j].process(params[levelParam(j)].getValue(), dt);
			pr.keep[j] = params[keepParam(j)].getValue() < 0.5f;
			io.in[j] = inputs[IN1_INPUT + 2 * j].getVoltage();
			io.retPatched[j] = inputs[RETURN1_INPUT + 2 * j].isConnected();
			io.ret[j] = io.retPatched[j] ? inputs[RETURN1_INPUT + 2 * j].getVoltage() : 0.f;
			if (!std::isfinite(io.ret[j])) io.ret[j] = 0.f;
		}
		pr.edge = math::clamp(params[EDGE_PARAM].getValue() + 0.1f * cvAmt(this, EDGE_CV_INPUT, EDGE_CV_PARAM), 0.f, 1.f);
		pr.speed = expVal(this, SPEED_PARAM, 0.1f, 10.f);
		pr.rotate = params[ROTATE_PARAM].getValue();
		model.process(io, pr);
		for (int j = 0; j < N; j++) {
			outputs[SEND1_OUTPUT + 2 * j].setVoltage(io.send[j]);
			outputs[OUT1_OUTPUT + 2 * j].setVoltage(io.out[j]);
		}
		outputs[MIX_L_OUTPUT].setVoltage(io.mixL);
		outputs[MIX_R_OUTPUT].setVoltage(io.mixR);
	}
};

struct LoopDisplay : TudorDisplay {
	LoopModule* module = nullptr;
	void drawContents(const DrawArgs& args) override {
		NVGcontext* vg = args.vg;
		float H = box.size.y, W = box.size.x;
		const int N = LoopModule::N;
		// crosspoint grid (left): size = |gain|, filled = positive, ring = negative
		float g0 = 6.f, cell = (H - 10.f) / N;
		for (int i = 0; i < N; i++)
			for (int j = 0; j < N; j++) {
				float x = g0 + cell * (j + 0.5f), y = 5.f + cell * (i + 0.5f);
				float v = module ? module->pr.m[i][j] : (j == (i + 1) % N ? 0.7f : 0.f);
				float r = 0.5f + 0.45f * cell * std::min(std::fabs(v) / 1.5f, 1.f);
				nvgBeginPath(vg);
				nvgCircle(vg, x, y, r);
				if (v >= 0.f) {
					nvgFillColor(vg, ink(0.85f));
					nvgFill(vg);
				} else {
					nvgStrokeColor(vg, ink(0.85f));
					nvgStrokeWidth(vg, 1.f);
					nvgStroke(vg);
				}
			}
		// per bus: level meter (dB) and keeper gain bar
		float x0 = g0 + cell * N + 10.f;
		float colW = (W - x0 - 4.f) / N;
		for (int j = 0; j < N; j++) {
			float x = x0 + colW * j;
			text(vg, x + colW * 0.5f, H - 1.f, std::to_string(j + 1), 6.f, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM, 0.6f);
			if (!module) continue;
			float rms = std::sqrt(std::max(module->model.rms[j].m, 0.f));
			float lv = math::clamp(1.f + tudor::gainToDb(rms / 5.f) / 60.f, 0.f, 1.f);
			float kv = math::clamp((std::log2(module->model.k[j]) + 5.64f) / (5.64f + 3.f), 0.f, 1.f);   // 0.02 .. 8
			float top = 4.f, bot = H - 9.f;
			nvgBeginPath(vg);
			nvgRect(vg, x + 1.f, bot - (bot - top) * lv, colW * 0.35f, (bot - top) * lv);
			nvgFillColor(vg, ink(0.85f));
			nvgFill(vg);
			nvgBeginPath(vg);
			nvgRect(vg, x + 1.f + colW * 0.42f, bot - (bot - top) * kv, colW * 0.25f, (bot - top) * kv);
			nvgFillColor(vg, ink(params(j) ? 0.45f : 0.12f));
			nvgFill(vg);
		}
	}
	bool params(int j) { return module && module->pr.keep[j]; }
};

struct LoopWidget : ModuleWidget {
	LoopWidget(LoopModule* m) {
		tudorScaffold(this, m, "Loop");
		auto* d = new LoopDisplay;
		d->module = m;
		d->setRectMm(LOOP_DISPLAY_MM);
		addChild(d);
		placeLoop<LoopModule>(this, m);
	}
};
Model* modelLoop = createModel<LoopModule, LoopWidget>("Loop");
