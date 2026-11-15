// Knobs, jacks, toggles and the display base. SVGs come from scripts/make_panels.py.
#pragma once
#include "../plugin.hpp"

inline std::shared_ptr<window::Svg> tudorSvg(const std::string& name) {
	return window::Svg::load(asset::plugin(pluginInstance, "res/components/" + name + ".svg"));
}

struct TudorKnobBase : app::SvgKnob {
	TudorKnobBase() {
		minAngle = -0.75f * M_PI;
		maxAngle = 0.75f * M_PI;
		shadow->opacity = 0.2f;
	}
};
struct TudorKnob : TudorKnobBase { TudorKnob() { setSvg(tudorSvg("TudorKnob")); } };
struct TudorKnobSmall : TudorKnobBase { TudorKnobSmall() { setSvg(tudorSvg("TudorKnobSmall")); } };
struct TudorTrim : TudorKnobBase { TudorTrim() { setSvg(tudorSvg("TudorTrim")); shadow->opacity = 0.1f; } };

struct PortAudio : app::SvgPort { PortAudio() { setSvg(tudorSvg("PortAudio")); } };
struct PortCv : app::SvgPort { PortCv() { setSvg(tudorSvg("PortCv")); } };
struct PortGate : app::SvgPort { PortGate() { setSvg(tudorSvg("PortGate")); } };
struct PortPoly : app::SvgPort { PortPoly() { setSvg(tudorSvg("PortPoly")); } };

// Value 0 is the top position, which is the first printed label.
struct TudorToggle2 : app::SvgSwitch {
	TudorToggle2() {
		addFrame(tudorSvg("Toggle2_0"));
		addFrame(tudorSvg("Toggle2_1"));
		shadow->opacity = 0.f;
	}
};
struct TudorToggle3 : app::SvgSwitch {
	TudorToggle3() {
		addFrame(tudorSvg("Toggle3_0"));
		addFrame(tudorSvg("Toggle3_1"));
		addFrame(tudorSvg("Toggle3_2"));
		shadow->opacity = 0.f;
	}
};

// Panel, screws. Placement of controls comes from generated/Layout.hpp.
template <class W>
inline void tudorScaffold(W* w, Module* m, const std::string& slug) {
	w->setModule(m);
	w->setPanel(createPanel(asset::plugin(pluginInstance, "res/" + slug + ".svg")));
	float width = w->box.size.x;
	w->addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
	w->addChild(createWidget<ScrewSilver>(Vec(width - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	if (width > 6 * RACK_GRID_WIDTH) {
		w->addChild(createWidget<ScrewSilver>(Vec(width - 2 * RACK_GRID_WIDTH, 0)));
		w->addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	}
}

// Display: ground drawn normally, contents drawn in layer 1 so they glow when the room lights dim.
struct TudorDisplay : widget::Widget {
	static constexpr int INK_R = 0xcf, INK_G = 0xe8, INK_B = 0xd5;
	void setRectMm(const float* mm) {
		box.pos = mm2px(Vec(mm[0], mm[1]));
		box.size = mm2px(Vec(mm[2], mm[3]));
	}
	static NVGcolor ink(float a = 1.f) { return nvgRGBA(INK_R, INK_G, INK_B, (unsigned char)(255 * math::clamp(a, 0.f, 1.f))); }
	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 2.f);
		nvgFillColor(args.vg, nvgRGB(0x10, 0x14, 0x12));
		nvgFill(args.vg);
		Widget::draw(args);
	}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			nvgSave(args.vg);
			nvgScissor(args.vg, 0, 0, box.size.x, box.size.y);
			drawContents(args);
			nvgRestore(args.vg);
		}
		Widget::drawLayer(args, layer);
	}
	virtual void drawContents(const DrawArgs& args) {}
	bool setFont(NVGcontext* vg, float size) {
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!font || font->handle < 0) return false;
		nvgFontFaceId(vg, font->handle);
		nvgFontSize(vg, size);
		return true;
	}
	void text(NVGcontext* vg, float x, float y, const std::string& s, float size, int align = NVG_ALIGN_LEFT | NVG_ALIGN_TOP, float alpha = 1.f) {
		if (!setFont(vg, size)) return;
		nvgFillColor(vg, ink(alpha));
		nvgTextAlign(vg, align);
		nvgText(vg, x, y, s.c_str(), NULL);
	}
};
