// Rainforest group: OBJECT, CRITTER, SEEK, WANDER.
#include "Common.hpp"
#include "../dsp/Object.hpp"
#include "../dsp/Critter.hpp"
#include "../dsp/Seek.hpp"
#include "../dsp/Wander.hpp"
#include <atomic>

// ============================================================================ OBJECT
struct ObjectModule : Module, ObjectIds {
	tudor::ObjectModel model;
	tudor::ObjectModel::Params pr;
	dsp::BooleanTrigger findBtn;
	dsp::SchmittTrigger strikeTrig;
	std::atomic<bool> findRequest{false};
	float findFlash = 0.f;
	float strikeT = 1e9f;
	tudor::Smoother sDriveA, sDriveB;
	// FIND state, saved so a patch reopens with the same found object
	bool hasFound = false;
	uint64_t foundSeed = 1;
	int foundMaterial = 0;

	ObjectModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		std::vector<std::string> objs, mats;
		for (int i = 0; i <= tudor::OBJECT_PRESETS; i++) objs.push_back(tudor::OBJECT_NAMES[i]);
		for (int i = 0; i < tudor::MAT_COUNT; i++) mats.push_back(tudor::MATERIAL_NAMES[i]);
		configSwitch(OBJECT_PARAM, 0.f, (float)tudor::OBJECT_PRESETS, 1.f, "OBJECT", objs);
		configSwitch(MATERIAL_PARAM, 0.f, (float)(tudor::MAT_COUNT - 1), 0.f, "MATERIAL (used by FIND)", mats);
		configButton(FIND_PARAM, "FIND (build a new object of this material)");
		configParam(SIZE_PARAM, -2.f, 2.f, 0.f, "SIZE (+ = bigger and lower)", " oct");
		configParam(DAMPING_PARAM, 0.f, 1.f, 0.5f, "DAMPING", "%", 0.f, 100.f);
		configParam(BRIGHTNESS_PARAM, 0.f, 1.f, 0.5f, "BRIGHTNESS", "%", 0.f, 100.f);
		configParam(TICKLE_PARAM, 0.f, 1.f, 0.25f, "TICKLE (nonlinear response)", "%", 0.f, 100.f);
		configParam(RATTLE_PARAM, 0.f, 1.f, 0.f, "RATTLE (loose contact)", "%", 0.f, 100.f);
		configParam(PICKUP_POS_PARAM, 0.f, 1.f, 0.45f, "PICKUP POS", "%", 0.f, 100.f);
		configSwitch(PICKUP_TYPE_PARAM, 0.f, 1.f, 0.f, "PICKUP TYPE", {"Cartridge", "Contact"});
		configParam(DRIVE_A_PARAM, 0.f, 2.f, 1.f, "DRIVE A", "%", 0.f, 100.f);
		configParam(POINT_A_PARAM, 0.f, 1.f, 0.31f, "POINT A", "%", 0.f, 100.f);
		configSwitch(B_PHASE_PARAM, 0.f, 1.f, 0.f, "B PHASE", {"In", "Out (inverted)"});
		configParam(DRIVE_B_PARAM, 0.f, 2.f, 1.f, "DRIVE B", "%", 0.f, 100.f);
		configParam(POINT_B_PARAM, 0.f, 1.f, 0.67f, "POINT B", "%", 0.f, 100.f);
		configTrim(this, SIZE_CV_PARAM, "SIZE CV (1 oct/V at full)", 1.f);
		configTrim(this, DAMPING_CV_PARAM, "DAMPING CV (10%/V at full)", 1.f);
		configTrim(this, TICKLE_CV_PARAM, "TICKLE CV (10%/V at full)", 1.f);
		configTrim(this, PICKUP_CV_PARAM, "PICKUP CV (10%/V at full)", 1.f);
		configInput(SIZE_CV_INPUT, "SIZE CV");
		configInput(DAMPING_CV_INPUT, "DAMPING CV");
		configInput(TICKLE_CV_INPUT, "TICKLE CV");
		configInput(PICKUP_CV_INPUT, "PICKUP CV");
		configInput(STRIKE_INPUT, "STRIKE (trigger: a tap at POINT A)");
		configInput(TRANSDUCER_A_INPUT, "TRANSDUCER A");
		configInput(TRANSDUCER_B_INPUT, "TRANSDUCER B");
		configOutput(AIR_OUTPUT, "AIR (heard next to the object)");
		configOutput(PICKUP_OUTPUT, "PICKUP");
		configOutput(MODES_OUTPUT, "MODES (8 strongest modes, V/oct, strongest first)");
		configLight(FIND_LIGHT, "FIND");
		configBypass(TRANSDUCER_A_INPUT, AIR_OUTPUT);
	}
	void onSampleRateChange(const SampleRateChangeEvent& e) override { model.setSampleRate(e.sampleRate); }
	void onReset(const ResetEvent& e) override { Module::onReset(e); model.reset(); }

	void process(const ProcessArgs& args) override {
		if (findBtn.process(params[FIND_PARAM].getValue() > 0.f)) findRequest = true;
		pr.object = (int)std::round(params[OBJECT_PARAM].getValue());
		pr.size = math::clamp(params[SIZE_PARAM].getValue() + cvAmt(this, SIZE_CV_INPUT, SIZE_CV_PARAM), -2.f, 2.f);
		pr.damping = math::clamp(params[DAMPING_PARAM].getValue() + 0.1f * cvAmt(this, DAMPING_CV_INPUT, DAMPING_CV_PARAM), 0.f, 1.f);
		pr.brightness = params[BRIGHTNESS_PARAM].getValue();
		pr.tickle = math::clamp(params[TICKLE_PARAM].getValue() + 0.1f * cvAmt(this, TICKLE_CV_INPUT, TICKLE_CV_PARAM), 0.f, 1.f);
		pr.rattle = params[RATTLE_PARAM].getValue();
		pr.pickupPos = math::clamp(params[PICKUP_POS_PARAM].getValue() + 0.1f * cvAmt(this, PICKUP_CV_INPUT, PICKUP_CV_PARAM), 0.f, 1.f);
		pr.pickupType = (int)params[PICKUP_TYPE_PARAM].getValue();
		pr.driveA = sDriveA.process(params[DRIVE_A_PARAM].getValue(), args.sampleTime);
		pr.pointA = params[POINT_A_PARAM].getValue();
		pr.bInvert = params[B_PHASE_PARAM].getValue() > 0.5f;
		pr.driveB = sDriveB.process(params[DRIVE_B_PARAM].getValue(), args.sampleTime);
		pr.pointB = params[POINT_B_PARAM].getValue();

		// STRIKE: a 2 ms raised-cosine tap, 5 V peak, into transducer A
		if (strikeTrig.process(inputs[STRIKE_INPUT].getVoltage(), 0.1f, 1.f)) strikeT = 0.f;
		float strike = 0.f;
		if (strikeT < 0.002f) {
			strike = 5.f * (0.5f - 0.5f * std::cos(2.f * M_PI * strikeT / 0.002f));
			strikeT += args.sampleTime;
		}
		float inA = inputs[TRANSDUCER_A_INPUT].getVoltage() + strike;
		float inB = inputs[TRANSDUCER_B_INPUT].getVoltage();
		tudor::ObjectModel::Out o = model.process(inA, inB, pr);
		outputs[AIR_OUTPUT].setVoltage(o.air);
		outputs[PICKUP_OUTPUT].setVoltage(o.pickup);
		outputs[MODES_OUTPUT].setChannels(tudor::ObjectModel::TOP);
		for (int c = 0; c < tudor::ObjectModel::TOP; c++) outputs[MODES_OUTPUT].setVoltage(o.modes[c], c);

		findFlash = std::max(0.f, findFlash - args.sampleTime);
		lights[FIND_LIGHT].setBrightness(findFlash > 0.f ? 1.f : 0.f);
	}

	// Runs on the UI thread (from the widget), so building the object may allocate.
	void doFind() {
		foundSeed = random::u64();
		foundMaterial = (int)params[MATERIAL_PARAM].getValue();
		hasFound = true;
		model.find(foundMaterial, foundSeed);
		params[OBJECT_PARAM].setValue((float)tudor::OBJECT_PRESETS);
		findFlash = 0.3f;
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "hasFound", json_boolean(hasFound));
		json_object_set_new(root, "foundSeed", json_string(std::to_string(foundSeed).c_str()));
		json_object_set_new(root, "foundMaterial", json_integer(foundMaterial));
		return root;
	}
	void dataFromJson(json_t* root) override {
		json_t* h = json_object_get(root, "hasFound");
		hasFound = h && json_is_true(h);
		json_t* s = json_object_get(root, "foundSeed");
		if (s && json_is_string(s)) foundSeed = std::strtoull(json_string_value(s), nullptr, 10);
		foundMaterial = math::clamp(jsonInt(root, "foundMaterial", 0), 0, tudor::MAT_COUNT - 1);
		if (hasFound) model.find(foundMaterial, foundSeed);
	}
};

struct ObjectDisplay : TudorDisplay {
	ObjectModule* module = nullptr;
	void drawContents(const DrawArgs& args) override {
		NVGcontext* vg = args.vg;
		float W = box.size.x, H = box.size.y;
		if (!module) { text(vg, 4, 3, "WINE BARREL", 9.f); return; }
		tudor::ObjectModel& m = module->model;
		std::string name = m.name();
		for (auto& ch : name) ch = std::toupper(ch);
		text(vg, 4, 3, name, 9.f);
		// bars: x = log frequency (20 Hz .. 20 kHz), height = energy (dB, 60 dB range)
		float top = 14.f, strip = H - 8.f, barH = strip - top;
		float emax = 1e-9f;
		for (int k = 0; k < m.nm; k++) emax = std::max(emax, m.energy[k]);
		nvgBeginPath(vg);
		for (int k = 0; k < m.nm; k++) {
			float f = m.fCur[k];
			if (f < 20.f || f > 20000.f) continue;
			float x = 3.f + (W - 6.f) * std::log2(f / 20.f) / std::log2(1000.f);
			float db = 10.f * std::log10(std::max(m.energy[k], 1e-12f) / emax);
			float h = barH * math::clamp(1.f + db / 60.f, 0.f, 1.f);
			if (h < 0.5f) continue;
			nvgRect(vg, x - 0.6f, strip - h, 1.2f, h);
		}
		nvgFillColor(vg, ink(0.85f));
		nvgFill(vg);
		// position strip: POINT A, POINT B, PICKUP POS
		nvgBeginPath(vg);
		nvgMoveTo(vg, 3, H - 4);
		nvgLineTo(vg, W - 3, H - 4);
		nvgStrokeColor(vg, ink(0.3f));
		nvgStrokeWidth(vg, 0.7f);
		nvgStroke(vg);
		auto mark = [&](float pos, const char* s) {
			float x = 3.f + (W - 6.f) * pos;
			nvgBeginPath(vg);
			nvgCircle(vg, x, H - 4, 1.6f);
			nvgFillColor(vg, ink());
			nvgFill(vg);
			text(vg, x, H - 7.5f, s, 6.f, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM, 0.8f);
		};
		mark(module->pr.pointA, "A");
		mark(module->pr.pointB, "B");
		mark(module->pr.pickupPos, "P");
	}
};

struct ObjectWidget : ModuleWidget {
	ObjectWidget(ObjectModule* m) {
		tudorScaffold(this, m, "Object");
		auto* d = new ObjectDisplay;
		d->module = m;
		d->setRectMm(OBJECT_DISPLAY_MM);
		addChild(d);
		placeObject<ObjectModule>(this, m);
	}
	void step() override {
		ObjectModule* m = dynamic_cast<ObjectModule*>(module);
		if (m && m->findRequest.exchange(false)) m->doFind();
		ModuleWidget::step();
	}
};
Model* modelObject = createModel<ObjectModule, ObjectWidget>("Object");

// ============================================================================ CRITTER
struct CritterModule : Module, CritterIds {
	tudor::Critter model;
	tudor::Critter::Params pr;
	dsp::SchmittTrigger callTrig;
	tudor::Smoother sLevel;
	float follow[16] = {};
	static constexpr float P_LO = 30.f, P_HI = 8000.f, R_LO = 0.1f, R_HI = 20.f;

	CritterModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		const float pitch[4] = {1800.f, 900.f, 3200.f, 420.f};
		const float chirp[4] = {0.4f, -0.3f, 0.7f, -0.5f};
		const float rate[4] = {2.f, 0.7f, 5.f, 0.3f};
		for (int i = 0; i < 4; i++) {
			std::string n = string::f("Voice %d: ", i + 1);
			int o = 5 * i;
			configExp(this, PITCH1_PARAM + o, P_LO, P_HI, pitch[i], n + "PITCH", " Hz");
			configParam(WARBLE1_PARAM + o, 0.f, 1.f, 0.3f, n + "WARBLE", "%", 0.f, 100.f);
			configParam(CHIRP1_PARAM + o, -1.f, 1.f, chirp[i], n + "CHIRP (pitch sweep across each call)", "%", 0.f, 100.f);
			configExp(this, RATE1_PARAM + o, R_LO, R_HI, rate[i], n + "RATE", " calls/s");
			configParam(DENSITY1_PARAM + o, 0.f, 1.f, 0.6f, n + "DENSITY (chance a call happens)", "%", 0.f, 100.f);
			configInput(PITCH1_INPUT + i, n + "PITCH (1 V/oct)");
			configOutput(OUT1_OUTPUT + i, n + "OUT");
		}
		configParam(SPECIES_PARAM, 0.f, 1.f, 0.2f, "SPECIES (0 bird, 0.5 insect, 1 frog)", "%", 0.f, 100.f);
		configParam(FOLLOW_PARAM, 0.f, 1.f, 0.f, "FOLLOW", "%", 0.f, 100.f);
		configParam(LEVEL_PARAM, 0.f, 1.f, 0.8f, "LEVEL", "%", 0.f, 100.f);
		configInput(SPECIES_INPUT, "SPECIES (0.1/V)");
		configInput(FOLLOW_INPUT, "FOLLOW (poly pitches, e.g. OBJECT MODES or SEEK NODES)");
		configInput(CALL_INPUT, "CALL (trigger: every voice calls)");
		configOutput(MIX_OUTPUT, "MIX");
		configOutput(CALL_OUTPUT, "CALL (poly 4 gates, high while each voice calls)");
	}
	void onSampleRateChange(const SampleRateChangeEvent& e) override { model.setSampleRate(e.sampleRate); }

	void process(const ProcessArgs& args) override {
		for (int i = 0; i < 4; i++) {
			int o = 5 * i;
			auto& v = pr.v[i];
			v.pitch = expVal(this, PITCH1_PARAM + o, P_LO, P_HI) * std::pow(2.f, math::clamp(inputs[PITCH1_INPUT + i].getVoltage(), -6.f, 6.f));
			v.warble = params[WARBLE1_PARAM + o].getValue();
			v.chirp = params[CHIRP1_PARAM + o].getValue();
			v.rate = expVal(this, RATE1_PARAM + o, R_LO, R_HI);
			v.density = params[DENSITY1_PARAM + o].getValue();
		}
		pr.species = math::clamp(params[SPECIES_PARAM].getValue() + 0.1f * inputs[SPECIES_INPUT].getVoltage(), 0.f, 1.f);
		pr.follow = params[FOLLOW_PARAM].getValue();
		pr.level = sLevel.process(params[LEVEL_PARAM].getValue(), args.sampleTime);
		int n = inputs[FOLLOW_INPUT].getChannels();
		for (int c = 0; c < n; c++) follow[c] = inputs[FOLLOW_INPUT].getVoltage(c);
		pr.followPitches = n > 0 ? follow : nullptr;
		pr.followCount = n;
		pr.callAll = callTrig.process(inputs[CALL_INPUT].getVoltage(), 0.1f, 1.f);
		tudor::Critter::Out o = model.process(pr);
		for (int i = 0; i < 4; i++) outputs[OUT1_OUTPUT + i].setVoltage(o.v[i]);
		outputs[MIX_OUTPUT].setVoltage(o.mix);
		outputs[CALL_OUTPUT].setChannels(4);
		for (int i = 0; i < 4; i++) outputs[CALL_OUTPUT].setVoltage(o.calling[i] ? 10.f : 0.f, i);
	}
};

struct CritterWidget : ModuleWidget {
	CritterWidget(CritterModule* m) {
		tudorScaffold(this, m, "Critter");
		placeCritter<CritterModule>(this, m);
	}
};
Model* modelCritter = createModel<CritterModule, CritterWidget>("Critter");

// ============================================================================ SEEK
struct SeekModule : Module, SeekIds {
	tudor::Seek model;
	tudor::Seek::Params pr;
	dsp::BooleanTrigger huntBtn;
	dsp::SchmittTrigger huntTrig;
	dsp::PulseGenerator foundPulse, arrivePulse;

	SeekModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configSwitch(MODE_PARAM, 0.f, 1.f, 0.f, "MODE", {"Sweep (probe actively)", "Listen (analyse only)"});
		configButton(HUNT_PARAM, "HUNT (new scan, or clear the list in LISTEN)");
		configSwitch(COUNT_PARAM, 1.f, 8.f, 4.f, "COUNT (nodes to find)", {"1", "2", "3", "4", "5", "6", "7", "8"});
		configExp(this, SWEEP_TIME_PARAM, 2.f, 30.f, 8.f, "SWEEP TIME", " s");
		configParam(DRIFT_PARAM, 0.f, 1.f, 0.3f, "DRIFT (LISTEN: how fast the list follows changes)", "%", 0.f, 100.f);
		configParam(PROBE_LEVEL_PARAM, 0.f, 1.f, 0.7f, "PROBE LEVEL", "%", 0.f, 100.f);
		configExp(this, LINGER_PARAM, 0.2f, 10.f, 2.f, "LINGER", " s");
		configParam(GLIDE_PARAM, 0.f, 2.f, 0.3f, "GLIDE", " s");
		configParam(TEASE_LEVEL_PARAM, 0.f, 1.f, 0.7f, "TEASE LEVEL", "%", 0.f, 100.f);
		configInput(RETURN_INPUT, "RETURN (the object's PICKUP or AIR)");
		configInput(HUNT_INPUT, "HUNT (trigger)");
		configOutput(PROBE_OUTPUT, "PROBE (to a TRANSDUCER)");
		configOutput(TEASE_OUTPUT, "TEASE (to a TRANSDUCER)");
		configOutput(NODES_OUTPUT, "NODES (poly V/oct, strongest first)");
		configOutput(STRENGTH_OUTPUT, "STRENGTH (poly, strongest = 10 V)");
		configOutput(FOUND_OUTPUT, "FOUND (trigger)");
		configOutput(ARRIVE_OUTPUT, "ARRIVE (trigger)");
		configLight(HUNT_LIGHT, "Scanning");
	}
	void onSampleRateChange(const SampleRateChangeEvent& e) override { model.setSampleRate(e.sampleRate); }

	void process(const ProcessArgs& args) override {
		pr.mode = (int)params[MODE_PARAM].getValue();
		pr.sweepTime = expVal(this, SWEEP_TIME_PARAM, 2.f, 30.f);
		pr.count = (int)std::round(params[COUNT_PARAM].getValue());
		pr.drift = params[DRIFT_PARAM].getValue();
		pr.linger = expVal(this, LINGER_PARAM, 0.2f, 10.f);
		pr.glide = params[GLIDE_PARAM].getValue();
		pr.probeLevel = params[PROBE_LEVEL_PARAM].getValue();
		pr.teaseLevel = params[TEASE_LEVEL_PARAM].getValue();
		bool hunt = huntBtn.process(params[HUNT_PARAM].getValue() > 0.f);
		hunt |= huntTrig.process(inputs[HUNT_INPUT].getVoltage(), 0.1f, 1.f);
		tudor::Seek::Out o = model.process(inputs[RETURN_INPUT].getVoltage(), pr, hunt);
		outputs[PROBE_OUTPUT].setVoltage(o.probe);
		outputs[TEASE_OUTPUT].setVoltage(o.tease);
		int n = model.nNodes;
		outputs[NODES_OUTPUT].setChannels(std::max(n, 1));
		outputs[STRENGTH_OUTPUT].setChannels(std::max(n, 1));
		if (n == 0) { outputs[NODES_OUTPUT].setVoltage(0.f); outputs[STRENGTH_OUTPUT].setVoltage(0.f); }
		for (int i = 0; i < n; i++) {
			outputs[NODES_OUTPUT].setVoltage(std::log2(std::max(model.nodeHz[i], 1.f) / tudor::C4), i);
			outputs[STRENGTH_OUTPUT].setVoltage(10.f * math::clamp(model.nodeStr[i], 0.f, 1.f), i);
		}
		if (o.found) foundPulse.trigger(1e-3f);
		if (o.arrive) arrivePulse.trigger(1e-3f);
		outputs[FOUND_OUTPUT].setVoltage(trigOut(foundPulse, args.sampleTime));
		outputs[ARRIVE_OUTPUT].setVoltage(trigOut(arrivePulse, args.sampleTime));
		lights[HUNT_LIGHT].setBrightness(model.scanning ? 1.f : 0.f);
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "nodes", json_integer(model.nNodes));
		json_object_set_new(root, "nodeHz", jsonFloats(model.nodeHz, tudor::Seek::MAXN));
		json_object_set_new(root, "nodeStr", jsonFloats(model.nodeStr, tudor::Seek::MAXN));
		json_object_set_new(root, "curve", jsonFloats(model.curve, tudor::Seek::BINS));
		return root;
	}
	void dataFromJson(json_t* root) override {
		model.nNodes = math::clamp(jsonInt(root, "nodes", 0), 0, tudor::Seek::MAXN);
		readFloats(root, "nodeHz", model.nodeHz, tudor::Seek::MAXN);
		readFloats(root, "nodeStr", model.nodeStr, tudor::Seek::MAXN);
		readFloats(root, "curve", model.curve, tudor::Seek::BINS);
		model.teaseNode = -1;
	}
};

struct SeekDisplay : TudorDisplay {
	SeekModule* module = nullptr;
	void drawContents(const DrawArgs& args) override {
		NVGcontext* vg = args.vg;
		float W = box.size.x, H = box.size.y;
		float x0 = 3.f, x1 = W - 3.f, y0 = 4.f, y1 = H - 4.f;
		auto xOf = [&](float hz) { return x0 + (x1 - x0) * std::log2(hz / tudor::Seek::F_LO) / std::log2(tudor::Seek::F_HI / tudor::Seek::F_LO); };
		// grid at 100 Hz, 1 kHz, 10 kHz
		nvgBeginPath(vg);
		for (float f : {100.f, 1000.f, 10000.f}) { nvgMoveTo(vg, xOf(f), y0); nvgLineTo(vg, xOf(f), y1); }
		nvgStrokeColor(vg, ink(0.15f));
		nvgStrokeWidth(vg, 0.6f);
		nvgStroke(vg);
		if (!module) return;
		tudor::Seek& m = module->model;
		// response curve
		nvgBeginPath(vg);
		for (int b = 0; b < tudor::Seek::BINS; b++) {
			float x = xOf(tudor::Seek::binHz((float)b));
			float y = y1 - (y1 - y0) * math::clamp(m.curve[b], 0.f, 1.f);
			if (b == 0) nvgMoveTo(vg, x, y);
			else nvgLineTo(vg, x, y);
		}
		nvgStrokeColor(vg, ink(0.8f));
		nvgStrokeWidth(vg, 1.f);
		nvgStroke(vg);
		// nodes
		for (int i = 0; i < m.nNodes; i++) {
			float x = xOf(m.nodeHz[i]);
			nvgBeginPath(vg);
			nvgMoveTo(vg, x, y1);
			nvgLineTo(vg, x, y1 - (y1 - y0) * math::clamp(m.nodeStr[i], 0.f, 1.f));
			nvgStrokeColor(vg, ink(0.5f));
			nvgStrokeWidth(vg, 1.5f);
			nvgStroke(vg);
			text(vg, x, y1 + 0.5f, std::to_string(i + 1), 6.f, NVG_ALIGN_CENTER | NVG_ALIGN_TOP, 0.7f);
		}
		// tease position
		if (m.nNodes > 0 && m.teaseNode >= 0) {
			float hz = tudor::C4 * std::pow(2.f, m.teasePitch);
			nvgBeginPath(vg);
			nvgCircle(vg, xOf(math::clamp(hz, tudor::Seek::F_LO, tudor::Seek::F_HI)), y0 + 2.f, 2.f);
			nvgFillColor(vg, ink());
			nvgFill(vg);
		}
		if (m.scanning) {
			float u = m.sweepT / std::max(m.sweepDur, 0.1f);
			float x = x0 + (x1 - x0) * u;
			nvgBeginPath(vg);
			nvgMoveTo(vg, x, y0);
			nvgLineTo(vg, x, y1);
			nvgStrokeColor(vg, ink(0.6f));
			nvgStrokeWidth(vg, 0.8f);
			nvgStroke(vg);
		}
	}
};

struct SeekWidget : ModuleWidget {
	SeekWidget(SeekModule* m) {
		tudorScaffold(this, m, "Seek");
		auto* d = new SeekDisplay;
		d->module = m;
		d->setRectMm(SEEK_DISPLAY_MM);
		addChild(d);
		placeSeek<SeekModule>(this, m);
	}
};
Model* modelSeek = createModel<SeekModule, SeekWidget>("Seek");

// ============================================================================ WANDER
struct WanderModule : Module, WanderIds {
	tudor::Wander model;
	tudor::Wander::Params pr;
	float in[tudor::Wander::SRC] = {};
	bool connected[tudor::Wander::SRC] = {};
	tudor::Smoother sRoom, sSpeakers;

	WanderModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(SPEED_PARAM, 0.f, 1.f, 0.25f, "SPEED (walking)", " m/s");
		configParam(CURIOSITY_PARAM, 0.f, 1.f, 0.5f, "CURIOSITY (pull toward loud objects)", "%", 0.f, 100.f);
		configParam(ROOM_PARAM, 0.f, 1.f, 0.3f, "ROOM", "%", 0.f, 100.f);
		configParam(SPEAKERS_PARAM, 0.f, 1.f, 0.5f, "SPEAKERS (pickup loudspeakers)", "%", 0.f, 100.f);
		configParam(SIZE_PARAM, 6.f, 30.f, 12.f, "SIZE (room side)", " m");
		configSwitch(OUTPUT_PARAM, 0.f, 1.f, 0.f, "OUTPUT", {"Stereo", "Quad"});
		configSwitch(EAR_PARAM, 0.f, 1.f, 0.f, "EAR TO OBJECT", {"Off", "On"});
		for (int i = 0; i < 8; i++) {
			configInput(AIR1_INPUT + 2 * i, string::f("AIR %d (object %d direct sound)", i + 1, i + 1));
			configInput(PICKUP1_INPUT + 2 * i, string::f("PICKUP %d (object %d loudspeaker)", i + 1, i + 1));
		}
		configInput(X_INPUT, "X (0 V west wall, 10 V east wall)");
		configInput(Y_INPUT, "Y (0 V south wall, 10 V north wall)");
		configInput(EAR_INPUT, "EAR (gate)");
		configOutput(L_OUTPUT, "L (front left in QUAD)");
		configOutput(R_OUTPUT, "R (front right in QUAD)");
		configOutput(FL_OUTPUT, "FL (quad)");
		configOutput(FR_OUTPUT, "FR (quad)");
		configOutput(RL_OUTPUT, "RL (quad)");
		configOutput(RR_OUTPUT, "RR (quad)");
		configLight(EAR_LIGHT, "Ear to object");
	}
	void onSampleRateChange(const SampleRateChangeEvent& e) override { model.setSampleRate(e.sampleRate); }
	void onReset(const ResetEvent& e) override { Module::onReset(e); model.defaultLayout(); model.reset(); }

	void process(const ProcessArgs& args) override {
		const int N = tudor::Wander::N;
		for (int i = 0; i < N; i++) {
			connected[i] = inputs[AIR1_INPUT + 2 * i].isConnected();
			connected[N + i] = inputs[PICKUP1_INPUT + 2 * i].isConnected();
			in[i] = connected[i] ? inputs[AIR1_INPUT + 2 * i].getVoltage() : 0.f;
			in[N + i] = connected[N + i] ? inputs[PICKUP1_INPUT + 2 * i].getVoltage() : 0.f;
		}
		pr.speed = params[SPEED_PARAM].getValue();
		pr.curiosity = params[CURIOSITY_PARAM].getValue();
		pr.room = sRoom.process(params[ROOM_PARAM].getValue(), args.sampleTime);
		pr.speakers = sSpeakers.process(params[SPEAKERS_PARAM].getValue(), args.sampleTime);
		pr.size = params[SIZE_PARAM].getValue();
		pr.quad = params[OUTPUT_PARAM].getValue() > 0.5f;
		pr.ear = params[EAR_PARAM].getValue() > 0.5f || inputs[EAR_INPUT].getVoltage() >= 1.f;
		pr.xyPatched = inputs[X_INPUT].isConnected() || inputs[Y_INPUT].isConnected();
		pr.x = inputs[X_INPUT].isConnected() ? inputs[X_INPUT].getVoltage() / 10.f : 0.5f;
		pr.y = inputs[Y_INPUT].isConnected() ? inputs[Y_INPUT].getVoltage() / 10.f : 0.5f;
		tudor::Wander::Out o = model.process(in, connected, pr);
		outputs[L_OUTPUT].setVoltage(o.l);
		outputs[R_OUTPUT].setVoltage(o.r);
		outputs[FL_OUTPUT].setVoltage(o.fl);
		outputs[FR_OUTPUT].setVoltage(o.fr);
		outputs[RL_OUTPUT].setVoltage(o.rl);
		outputs[RR_OUTPUT].setVoltage(o.rr);
		lights[EAR_LIGHT].setBrightness(pr.ear ? 1.f : 0.f);
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "objX", jsonFloats(model.objX, tudor::Wander::N));
		json_object_set_new(root, "objY", jsonFloats(model.objY, tudor::Wander::N));
		return root;
	}
	void dataFromJson(json_t* root) override {
		readFloats(root, "objX", model.objX, tudor::Wander::N);
		readFloats(root, "objY", model.objY, tudor::Wander::N);
		for (int i = 0; i < tudor::Wander::N; i++) {
			model.objX[i] = math::clamp(model.objX[i], 0.03f, 0.97f);
			model.objY[i] = math::clamp(model.objY[i], 0.03f, 0.97f);
		}
		model.placeSpeakers();
	}
};

// Top-down map. Drag an object to move it (its loudspeaker follows to the nearest wall).
struct WanderMap : TudorDisplay {
	WanderModule* module = nullptr;
	int dragging = -1;
	Vec dragPos;

	float pad() const { return 5.f; }
	float side() const { return std::min(box.size.x, box.size.y) - 2 * pad(); }
	Vec origin() const { return Vec((box.size.x - side()) / 2, (box.size.y - side()) / 2); }
	// room y runs north = up
	Vec toPx(float x, float y) const { Vec o = origin(); return Vec(o.x + x * side(), o.y + (1.f - y) * side()); }
	Vec toRoom(Vec p) const { Vec o = origin(); return Vec((p.x - o.x) / side(), 1.f - (p.y - o.y) / side()); }

	void drawContents(const DrawArgs& args) override {
		NVGcontext* vg = args.vg;
		Vec o = origin();
		float s = side();
		nvgBeginPath(vg);
		nvgRect(vg, o.x, o.y, s, s);
		nvgStrokeColor(vg, ink(0.4f));
		nvgStrokeWidth(vg, 0.8f);
		nvgStroke(vg);
		if (!module) return;
		tudor::Wander& m = module->model;
		const int N = tudor::Wander::N;
		for (int i = 0; i < N; i++) {
			// loudspeaker
			Vec sp = toPx(m.spkX[i], m.spkY[i]);
			nvgBeginPath(vg);
			nvgRect(vg, sp.x - 2.f, sp.y - 2.f, 4.f, 4.f);
			nvgStrokeColor(vg, ink(module->connected[N + i] ? 0.8f : 0.25f));
			nvgStrokeWidth(vg, 0.8f);
			nvgStroke(vg);
			// object, lit by level
			Vec p = toPx(m.objX[i], m.objY[i]);
			float lit = math::clamp(1.f + tudor::gainToDb(m.level[i] / 5.f) / 40.f, 0.f, 1.f);
			nvgBeginPath(vg);
			nvgCircle(vg, p.x, p.y, 3.2f);
			nvgFillColor(vg, ink(0.15f + 0.85f * lit));
			nvgFill(vg);
			text(vg, p.x + 4.f, p.y - 4.f, std::to_string(i + 1), 7.f, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE, 0.7f);
		}
		// trail, oldest first
		for (int k = 0; k < 32; k++) {
			int idx = (m.trailPos + k) % 32;
			Vec p = toPx(m.trailX[idx], m.trailY[idx]);
			nvgBeginPath(vg);
			nvgCircle(vg, p.x, p.y, 0.8f);
			nvgFillColor(vg, ink(0.05f + 0.4f * k / 32.f));
			nvgFill(vg);
		}
		// listener with heading
		Vec l = toPx(m.dispX, m.dispY);
		nvgBeginPath(vg);
		nvgCircle(vg, l.x, l.y, 2.4f);
		nvgFillColor(vg, nvgRGB(0xff, 0xff, 0xff));
		nvgFill(vg);
		float h = module->pr.quad ? (float)M_PI * 0.5f : m.heading;
		nvgBeginPath(vg);
		nvgMoveTo(vg, l.x, l.y);
		nvgLineTo(vg, l.x + 6.f * std::cos(h), l.y - 6.f * std::sin(h));
		nvgStrokeColor(vg, nvgRGB(0xff, 0xff, 0xff));
		nvgStrokeWidth(vg, 1.f);
		nvgStroke(vg);
	}

	void onButton(const event::Button& e) override {
		if (module && e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			tudor::Wander& m = module->model;
			int best = -1;
			float bd = 8.f;
			for (int i = 0; i < tudor::Wander::N; i++) {
				float d = toPx(m.objX[i], m.objY[i]).minus(e.pos).norm();
				if (d < bd) { bd = d; best = i; }
			}
			if (best >= 0) {
				dragging = best;
				dragPos = e.pos;
				e.consume(this);
				return;
			}
		}
		TudorDisplay::onButton(e);
	}
	void onDragMove(const event::DragMove& e) override {
		if (!module || dragging < 0) return;
		dragPos = dragPos.plus(e.mouseDelta.div(getAbsoluteZoom()));
		Vec r = toRoom(dragPos);
		module->model.objX[dragging] = math::clamp(r.x, 0.03f, 0.97f);
		module->model.objY[dragging] = math::clamp(r.y, 0.03f, 0.97f);
		module->model.placeSpeakers();
	}
	void onDragEnd(const event::DragEnd& e) override { dragging = -1; }
};

struct WanderWidget : ModuleWidget {
	WanderWidget(WanderModule* m) {
		tudorScaffold(this, m, "Wander");
		auto* d = new WanderMap;
		d->module = m;
		d->setRectMm(WANDER_MAP_MM);
		addChild(d);
		placeWander<WanderModule>(this, m);
	}
	void appendContextMenu(Menu* menu) override {
		WanderModule* m = dynamic_cast<WanderModule*>(module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuItem("Default object layout", "", [=]() { m->model.defaultLayout(); }));
	}
};
Model* modelWander = createModel<WanderModule, WanderWidget>("Wander");
