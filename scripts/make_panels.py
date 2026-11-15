#!/usr/bin/env python3
"""Generate the Tudor panels (res/*.svg), component SVGs (res/components/*.svg) and the
widget placement header (src/generated/Layout.hpp) from one table per module.

Every control is placed from the same coordinates in both outputs, every control gets
its printed label, and the script fails if anything overlaps or leaves the panel.

    python3 scripts/make_panels.py            (needs: pip install fonttools)

Text is converted to paths here, because Rack does not render SVG text.
"""
import math
import os
import random
import sys

from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
if "--font" in sys.argv:
    FONT = sys.argv[sys.argv.index("--font") + 1]

# ---------------------------------------------------------------- look ------
GROUND = "#c9cbcc"
INK = "#1a1a1a"
PLATE = "#2a2a2a"
PLATE_INK = "#f2f2f2"
STYLE = "silver"   # silver | paper | graphite | block  (TUDOR_STYLE env var)
SECTION_BACK = "#cbcdce"
_style = os.environ.get("TUDOR_STYLE", "silver")
if _style == "paper":
    STYLE, GROUND, INK, PLATE, SECTION_BACK = "paper", "#f3f1ec", "#1b1b1b", "#262626", "#f3f1ec"
elif _style == "graphite":
    STYLE, GROUND, INK, PLATE, SECTION_BACK = "graphite", "#26292c", "#ecebe6", "#0f1011", "#26292c"
elif _style == "block":
    STYLE, GROUND, INK, PLATE, SECTION_BACK = "block", "#ebebe8", "#1b1b1b", "#262626", "#ebebe8"
GROUP_COLOURS = {"RAINFOREST": "#3f6b45", "PULSERS": "#a2522a", "PHONEMES": "#3e5a7e"}
H = 128.5
HP = 5.08
LABEL_CAP = 1.9
SMALL_CAP = 1.5
TITLE_CAP = 3.0
CONDENSE = 0.84

KNOBS = {  # size: (body radius, scale radius, check box half-size, C++ class)
    "L": (5.5, 6.6, 7.0, "TudorKnob"),
    "S": (4.0, 5.0, 5.4, "TudorKnobSmall"),
    "T": (3.0, 3.8, 4.1, "TudorTrim"),
}
JACK_R = 4.0
JACK_BOX = 4.2
LED_R = 1.2
BTN_R = 3.2
SW_W, SW_H = 5.0, 8.6
PORT_CLASSES = {"audio": "PortAudio", "cv": "PortCv", "gate": "PortGate", "poly": "PortPoly"}

# ---------------------------------------------------------------- text ------
_font = TTFont(FONT)
_glyphs = _font.getGlyphSet()
_cmap = _font.getBestCmap()
_cap = getattr(_font["OS/2"], "sCapHeight", 0) or 0.729 * _font["head"].unitsPerEm
_hmtx = _font["hmtx"]


def text_width(s, cap):
    scale = cap / _cap
    w = 0
    for ch in s:
        g = _cmap.get(ord(ch))
        if g is None:
            raise SystemExit(f"font has no glyph for {ch!r}")
        w += _hmtx[g][0]
    return w * scale * CONDENSE


def text_path(s, x, y, cap, anchor="middle"):
    scale = cap / _cap
    w = text_width(s, cap)
    if anchor == "middle":
        x -= w / 2
    elif anchor == "end":
        x -= w
    d = []
    pen_x = x
    for ch in s:
        g = _cmap[ord(ch)]
        sp = SVGPathPen(_glyphs)
        tp = TransformPen(sp, (scale * CONDENSE, 0, 0, -scale, pen_x, y))
        _glyphs[g].draw(tp)
        d.append(sp.getCommands())
        pen_x += _hmtx[g][0] * scale * CONDENSE
    return " ".join(p for p in d if p)


# ---------------------------------------------------------------- model -----
class Box:
    def __init__(self, x0, y0, x1, y1, what, owner=None):
        self.x0, self.y0, self.x1, self.y1, self.what, self.owner = x0, y0, x1, y1, what, owner

    def overlaps(self, o, pad=0.2):
        return not (self.x1 + pad <= o.x0 or o.x1 + pad <= self.x0 or self.y1 + pad <= o.y0 or o.y1 + pad <= self.y0)


class Panel:
    def __init__(self, slug, title, short, group, hp):
        self.slug, self.title, self.short, self.group, self.hp = slug, title, short, group, hp
        self.w = hp * HP
        self.items = []      # (kind, id, label, x, y, extra)
        self.displays = []   # (id, x0, y0, x1, y1)
        self.sections = []   # (title, x0, y0, x1, y1)
        self.lines = []      # (x0, y0, x1, y1)

    def knob(self, pid, label, x, y, size="L", bipolar=False, positions=None):
        assert pid.endswith("_PARAM"), pid
        self.items.append(("knob", pid, label, x, y, {"size": size, "bipolar": bipolar, "positions": positions}))

    def jack(self, pid, label, x, y, kind="audio"):
        assert pid.endswith("_INPUT") or pid.endswith("_OUTPUT"), pid
        self.items.append(("jack", pid, label, x, y, {"kind": kind, "out": pid.endswith("_OUTPUT")}))

    def switch(self, pid, positions, x, y, label=None):
        self.items.append(("switch", pid, label, x, y, {"positions": positions}))

    def button(self, pid, light, label, x, y, latch=False):
        self.items.append(("button", pid, label, x, y, {"light": light, "latch": latch}))

    def led(self, pid, label, x, y):
        self.items.append(("led", pid, label, x, y, {}))

    def display(self, did, x0, y0, x1, y1):
        self.displays.append((did, x0, y0, x1, y1))

    def section(self, title, x0, y0, x1, y1):
        self.sections.append((title, x0, y0, x1, y1))

    def line(self, x0, y0, x1, y1):
        self.lines.append((x0, y0, x1, y1))

    # labels -------------------------------------------------------------
    def label_lines(self, item):
        kind, pid, label, x, y, ex = item
        out = []
        if not label:
            return out
        lines = label.split("\n")
        if kind == "knob":
            r = KNOBS[ex["size"]][1]
            cap = LABEL_CAP if ex["size"] != "T" else SMALL_CAP
            base = y - r - 1.1
            for i, ln in enumerate(reversed(lines)):
                out.append((ln, x, base - i * (cap + 0.7), cap, "middle"))
        elif kind == "jack":
            base = y + JACK_R + 0.9 + LABEL_CAP
            for i, ln in enumerate(lines):
                out.append((ln, x, base + i * (LABEL_CAP + 0.7), LABEL_CAP, "middle"))
        elif kind == "led":
            out.append((lines[0], x, y + LED_R + 0.8 + SMALL_CAP, SMALL_CAP, "middle"))
        elif kind == "button":
            out.append((lines[0], x, y - BTN_R - 1.1, LABEL_CAP, "middle"))
        elif kind == "switch":
            base = y - SW_H / 2 - 1.0
            for i, ln in enumerate(reversed(lines)):
                out.append((ln, x, base - i * (LABEL_CAP + 0.7), LABEL_CAP, "middle"))
        return out

    def extra_labels(self, item):
        kind, pid, label, x, y, ex = item
        out = []
        if kind == "switch":
            pos = ex["positions"]
            ys = [y - 2.9, y + 2.9 + SMALL_CAP] if len(pos) == 2 else [y - 2.9, y + SMALL_CAP / 2, y + 2.9 + SMALL_CAP]
            out += [(p_, x + SW_W / 2 + 0.7, yy, SMALL_CAP, "start") for p_, yy in zip(pos, ys)]
        if kind == "knob" and ex.get("positions"):
            pos = ex["positions"]
            r = KNOBS[ex["size"]][1] + 2.2
            n = len(pos)
            for i, t in enumerate(pos):
                a = math.radians(-135 + 270 * i / (n - 1) - 90)
                px, py = x + r * math.cos(a), y + r * math.sin(a)
                anchor = "end" if math.cos(a) < -0.3 else ("start" if math.cos(a) > 0.3 else "middle")
                out.append((t, px, py + SMALL_CAP / 2, SMALL_CAP, anchor))
        return out

    def all_labels(self):
        out = []
        for it in self.items:
            out += [(t, it) for t in self.label_lines(it)]
            out += [(t, it) for t in self.extra_labels(it)]
        return out

    def boxes(self):
        b = []
        for it in self.items:
            kind, pid, label, x, y, ex = it
            r = {"jack": JACK_BOX, "led": LED_R + 0.2, "button": BTN_R + 0.2, "gled": 0.9}.get(kind)
            if kind == "knob":
                r = KNOBS[ex["size"]][2]
            if r:
                b.append(Box(x - r, y - r, x + r, y + r, pid, pid))
            else:
                b.append(Box(x - SW_W / 2, y - SW_H / 2, x + SW_W / 2, y + SW_H / 2, pid, pid))
        for (t, x, base, cap, anchor), it in self.all_labels():
            w = text_width(t, cap)
            x0 = x - w / 2 if anchor == "middle" else x if anchor == "start" else x - w
            b.append(Box(x0, base - cap, x0 + w, base, f"label '{t}' of {it[1]}", it[1] if it[0] == "knob" and it[5].get("positions") and t in it[5]["positions"] else None))
        for (did, x0, y0, x1, y1) in self.displays:
            b.append(Box(x0, y0, x1, y1, f"display {did}"))
        return b

    def check(self):
        errs = []
        bx = self.boxes()
        for i in range(len(bx)):
            a = bx[i]
            if a.x0 < 1.2 or a.x1 > self.w - 1.2 or a.y0 < 16.0 or a.y1 > 121.0:
                errs.append(f"{a.what} leaves the panel area ({a.x0:.1f},{a.y0:.1f})-({a.x1:.1f},{a.y1:.1f})")
            for j in range(i + 1, len(bx)):
                if a.owner and a.owner == bx[j].owner:
                    continue
                if a.overlaps(bx[j]):
                    errs.append(f"{a.what} overlaps {bx[j].what}")
        for it in self.items:
            if it[0] not in ("switch", "gled") and not it[2]:
                errs.append(f"{it[1]} has no label")
        ids = [it[1] for it in self.items]
        if len(set(ids)) != len(ids):
            errs.append("duplicate ids")
        return errs

    # output ----------------------------------------------------------------
    def svg(self):
        w = self.w
        out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{w:.2f}mm" height="{H}mm" viewBox="0 0 {w:.2f} {H}">']
        if STYLE == "silver":
            out.append('<defs><linearGradient id="g" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#d4d6d7"/><stop offset="1" stop-color="#bfc1c2"/></linearGradient></defs>')
            out.append(f'<rect x="0" y="0" width="{w:.2f}" height="{H}" fill="url(#g)"/>')
            rnd = random.Random(self.slug)
            brush = []
            for _ in range(int(40 + w)):
                yy = rnd.uniform(0, H)
                brush.append(f'<line x1="0" y1="{yy:.2f}" x2="{w:.2f}" y2="{yy:.2f}"/>')
            out.append(f'<g stroke="#ffffff" stroke-opacity="0.10" stroke-width="0.12">{"".join(brush)}</g>')
            out.append(f'<rect x="0.6" y="0.6" width="{w - 1.2:.2f}" height="{H - 1.2}" fill="none" stroke="#8d9092" stroke-width="0.4"/>')
        else:
            out.append(f'<rect x="0" y="0" width="{w:.2f}" height="{H}" fill="{GROUND}"/>')
            if STYLE == "block":
                out.append(f'<rect x="0" y="0" width="{w:.2f}" height="14.3" fill="{GROUP_COLOURS[self.group]}"/>')
        ink = []    # dark text paths
        white = []  # white text paths
        g = []
        # title
        cap = TITLE_CAP
        while text_width(self.title, cap) > w - 4:
            cap -= 0.1
        (white if STYLE == "block" else ink).append(text_path(self.title, w / 2, 7.0 + cap, cap))
        # group stripe
        col = GROUP_COLOURS[self.group]
        if STYLE == "silver":
            g.append(f'<rect x="0.6" y="11.3" width="{w - 1.2:.2f}" height="3.0" fill="{col}"/>')
        elif STYLE in ("paper", "graphite"):
            g.append(f'<rect x="0" y="11.3" width="{w:.2f}" height="3.0" fill="{col}"/>')
        gcap = 1.6
        txt = self.group if text_width(self.group, gcap) < w - 4 - text_width(self.short, gcap) else self.group[:4]
        white.append(text_path(txt, 2.2, 13.55, gcap, "start"))
        white.append(text_path(self.short, w - 2.2, 13.55, gcap, "end"))
        # footer
        ink.append(text_path("TUDOR", w / 2, 126.0, SMALL_CAP))
        # sections
        for (title, x0, y0, x1, y1) in self.sections:
            g.append(f'<rect x="{x0:.2f}" y="{y0:.2f}" width="{x1 - x0:.2f}" height="{y1 - y0:.2f}" rx="1.0" fill="none" stroke="{INK}" stroke-opacity="0.55" stroke-width="0.3"/>')
            if title:
                tw = text_width(title, SMALL_CAP) + 1.6
                g.append(f'<rect x="{(x0 + x1) / 2 - tw / 2:.2f}" y="{y0 - 1.2:.2f}" width="{tw:.2f}" height="2.4" fill="{SECTION_BACK}"/>')
                ink.append(text_path(title, (x0 + x1) / 2, y0 + SMALL_CAP / 2, SMALL_CAP))
        for (x0, y0, x1, y1) in self.lines:
            g.append(f'<line x1="{x0:.2f}" y1="{y0:.2f}" x2="{x1:.2f}" y2="{y1:.2f}" stroke="{INK}" stroke-opacity="0.6" stroke-width="0.3"/>')
        # output plates behind output jacks and their labels
        for it in self.items:
            kind, pid, label, x, y, ex = it
            if kind == "jack" and ex["out"]:
                nl = len(label.split("\n"))
                top = y - JACK_R - 0.9
                bot = y + JACK_R + 1.6 + nl * (LABEL_CAP + 0.7)
                wl = max(2 * JACK_R + 1.6, max(text_width(l, LABEL_CAP) for l in label.split("\n")) + 1.4)
                g.append(f'<rect x="{x - wl / 2:.2f}" y="{top:.2f}" width="{wl:.2f}" height="{bot - top:.2f}" rx="0.9" fill="{PLATE}"/>')
        # knob scales, rings
        for it in self.items:
            kind, pid, label, x, y, ex = it
            if kind == "knob":
                r = KNOBS[ex["size"]][1]
                a0, a1 = math.radians(-135 - 90), math.radians(135 - 90)
                sx, sy = x + r * math.cos(a0), y + r * math.sin(a0)
                ex_, ey = x + r * math.cos(a1), y + r * math.sin(a1)
                g.append(f'<path d="M{sx:.2f},{sy:.2f} A{r},{r} 0 1 1 {ex_:.2f},{ey:.2f}" fill="none" stroke="{INK}" stroke-width="0.3"/>')
                if ex.get("bipolar"):
                    g.append(f'<line x1="{x:.2f}" y1="{y - r - 0.8:.2f}" x2="{x:.2f}" y2="{y - r + 0.4:.2f}" stroke="{INK}" stroke-width="0.3"/>')
                    if ex["size"] != "T":
                        ink.append(text_path("−", sx - 0.6, sy + 2.4, SMALL_CAP, "end"))
                        ink.append(text_path("+", ex_ + 0.6, ey + 2.4, SMALL_CAP, "start"))
                if ex.get("positions"):
                    n = len(ex["positions"])
                    for i in range(n):
                        a = math.radians(-135 + 270 * i / (n - 1) - 90)
                        g.append(f'<circle cx="{x + r * math.cos(a):.2f}" cy="{y + r * math.sin(a):.2f}" r="0.35" fill="{INK}"/>')
        gl = [it for it in self.items if it[0] == "gled"]
        if gl:
            xs = sorted(set(round(it[3], 2) for it in gl)); ys = sorted(set(round(it[4], 2) for it in gl))
            for i, y in enumerate(ys):
                ink.append(text_path(str(i + 1), xs[0] - 3.2, y + 0.6, 1.3, "middle"))
            for o, x in enumerate(xs):
                ink.append(text_path(str(o + 1), x, ys[-1] + 3.2, 1.3, "middle"))
            ink.append(text_path("IN", xs[0] - 6.4, ys[0] + 0.6, 1.3, "middle"))
            ink.append(text_path("OUT", xs[0] - 6.4, ys[-1] + 3.2, 1.3, "middle"))
        for (did, x0, y0, x1, y1) in self.displays:
            g.append(f'<rect x="{x0:.2f}" y="{y0:.2f}" width="{x1 - x0:.2f}" height="{y1 - y0:.2f}" rx="0.8" fill="#101412" stroke="#7d8082" stroke-width="0.4"/>')
        for (t, x, base_, cap_, anchor), it in self.all_labels():
            is_out = it[0] == "jack" and it[5]["out"]
            (white if is_out else ink).append(text_path(t, x, base_, cap_, anchor))
        out.append("".join(g))
        out.append(f'<path fill="{INK}" d="{" ".join(ink)}"/>')
        out.append(f'<path fill="{PLATE_INK}" d="{" ".join(white)}"/>')
        out.append("</svg>")
        return "\n".join(out)

    def ids(self):
        par, inp, out, lig = [], [], [], []
        for kind, pid, label, x, y, ex in self.items:
            if kind in ("knob", "switch"):
                par.append(pid)
            elif kind == "button":
                par.append(pid); lig.append(ex["light"])
            elif kind == "jack":
                (out if ex["out"] else inp).append(pid)
            elif kind in ("led", "gled"):
                lig.append(pid)
        def en(name, xs, tail):
            return f"\tenum {name} {{ " + "".join(x + ", " for x in xs) + tail + " };"
        return "\n".join([f"struct {self.slug}Ids {{",
                          en("ParamId", par, "PARAMS_LEN"), en("InputId", inp, "INPUTS_LEN"),
                          en("OutputId", out, "OUTPUTS_LEN"), en("LightId", lig, "LIGHTS_LEN"), "};", ""])

    def cpp(self):
        lines = [f"template <class M, class W>\ninline void place{self.slug}(W* w, M* m) {{"]
        for kind, pid, label, x, y, ex in self.items:
            v = f"mm2px(Vec({x:.3f}f, {y:.3f}f))"
            if kind == "knob":
                lines.append(f"\tw->addParam(createParamCentered<{KNOBS[ex['size']][3]}>({v}, m, M::{pid}));")
            elif kind == "switch":
                cls = "TudorToggle2" if len(ex["positions"]) == 2 else "TudorToggle3"
                lines.append(f"\tw->addParam(createParamCentered<{cls}>({v}, m, M::{pid}));")
            elif kind == "button":
                cls = "VCVLightLatch<MediumSimpleLight<WhiteLight>>" if ex["latch"] else "VCVLightBezel<WhiteLight>"
                lines.append(f"\tw->addParam(createLightParamCentered<{cls}>({v}, m, M::{pid}, M::{ex['light']}));")
            elif kind == "jack":
                cls = PORT_CLASSES[ex["kind"]]
                if ex["out"]:
                    lines.append(f"\tw->addOutput(createOutputCentered<{cls}>({v}, m, M::{pid}));")
                else:
                    lines.append(f"\tw->addInput(createInputCentered<{cls}>({v}, m, M::{pid}));")
            elif kind == "led":
                lines.append(f"\tw->addChild(createLightCentered<SmallLight<WhiteLight>>({v}, m, M::{pid}));")
            elif kind == "gled":
                lines.append(f"\tw->addChild(createLightCentered<TinyLight<WhiteLight>>({v}, m, M::{pid}));")
        lines.append("}")
        for (did, x0, y0, x1, y1) in self.displays:
            lines.append(f"static const float {self.slug.upper()}_{did}_MM[4] = {{{x0:.3f}f, {y0:.3f}f, {x1 - x0:.3f}f, {y1 - y0:.3f}f}};")
        lines.append("")
        return "\n".join(lines)


P = []


def cols(n, x0, x1):
    if n == 1:
        return [(x0 + x1) / 2]
    return [x0 + i * (x1 - x0) / (n - 1) for i in range(n)]


# ================================================================ panels ======
# ---- 1. OBJECT (16 HP) -------------------------------------------------------
p = Panel("Object", "OBJECT", "OBJ", "RAINFOREST", 18)
p.display("DISPLAY", 3.0, 17.0, 88.44, 32.5)
X5 = [10.0, 27.85, 45.7, 63.55, 81.4]
p.knob("OBJECT_PARAM", "OBJECT", X5[0], 45)
p.knob("SIZE_PARAM", "SIZE", X5[1], 45, bipolar=True)
p.knob("DAMPING_PARAM", "DAMPING", X5[2], 45)
p.knob("BRIGHTNESS_PARAM", "BRIGHTNESS", X5[3], 45)
p.knob("TICKLE_PARAM", "TICKLE", X5[4], 45)
p.knob("MATERIAL_PARAM", "MATERIAL", X5[0], 63, size="S")
p.button("FIND_PARAM", "FIND_LIGHT", "FIND", X5[1], 63.5)
p.knob("RATTLE_PARAM", "RATTLE", X5[2], 63, size="S")
p.knob("PICKUP_POS_PARAM", "PICKUP\nPOS", X5[3], 63, size="S")
p.switch("PICKUP_TYPE_PARAM", ["CARTRIDGE", "CONTACT"], 75.5, 63.5, "PICKUP\nTYPE")
p.knob("DRIVE_A_PARAM", "DRIVE A", X5[0], 78, size="S")
p.knob("POINT_A_PARAM", "POINT A", X5[1], 78, size="S")
p.switch("B_PHASE_PARAM", ["IN", "OUT"], X5[2] - 1.5, 78.5, "B PHASE")
p.knob("DRIVE_B_PARAM", "DRIVE B", X5[3], 78, size="S")
p.knob("POINT_B_PARAM", "POINT B", X5[4], 78, size="S")
for x, k, lab in [(X5[0], "SIZE", "SIZE CV"), (X5[1], "DAMPING", "DAMPING CV"), (X5[2], "TICKLE", "TICKLE CV"), (X5[3], "PICKUP", "PICKUP CV")]:
    p.knob(f"{k}_CV_PARAM", lab, x, 90, size="T", bipolar=True)
    p.jack(f"{k}_CV_INPUT", lab, x, 98.5, "cv")
p.jack("STRIKE_INPUT", "STRIKE", X5[4], 98.5, "gate")
p.jack("TRANSDUCER_A_INPUT", "TRANSDUCER\nA", X5[0], 110.3, "audio")
p.jack("TRANSDUCER_B_INPUT", "TRANSDUCER\nB", X5[1], 110.3, "audio")
p.jack("AIR_OUTPUT", "AIR", X5[2], 110.3, "audio")
p.jack("PICKUP_OUTPUT", "PICKUP", X5[3], 110.3, "audio")
p.jack("MODES_OUTPUT", "MODES", X5[4], 110.3, "poly")
P.append(p)

# ---- 2. CRITTER (20 HP) ------------------------------------------------------
p = Panel("Critter", "CRITTER", "CRT", "RAINFOREST", 20)
for v in range(4):
    c = 9.0 + 15.0 * v
    p.section(str(v + 1), c - 7.3, 18.0, c + 7.3, 118.5)
    p.knob(f"PITCH{v + 1}_PARAM", "PITCH", c, 28, size="S")
    p.knob(f"WARBLE{v + 1}_PARAM", "WARBLE", c, 42, size="S")
    p.knob(f"CHIRP{v + 1}_PARAM", "CHIRP", c, 56, size="S", bipolar=True)
    p.knob(f"RATE{v + 1}_PARAM", "RATE", c, 70, size="S")
    p.knob(f"DENSITY{v + 1}_PARAM", "DENSITY", c, 84, size="S")
    p.jack(f"PITCH{v + 1}_INPUT", "PITCH", c, 95, "cv")
    p.jack(f"OUT{v + 1}_OUTPUT", "OUT", c, 108.5, "audio")
p.knob("SPECIES_PARAM", "SPECIES", 74, 30)
p.knob("FOLLOW_PARAM", "FOLLOW", 92, 30)
p.knob("LEVEL_PARAM", "LEVEL", 74, 50)
p.jack("SPECIES_INPUT", "SPECIES", 92, 51, "cv")
p.jack("FOLLOW_INPUT", "FOLLOW", 74, 72, "poly")
p.jack("CALL_INPUT", "CALL", 92, 72, "gate")
p.jack("MIX_OUTPUT", "MIX", 74, 95, "audio")
p.jack("CALL_OUTPUT", "CALL", 92, 95, "poly")
P.append(p)

# ---- 3. SEEK (12 HP) ---------------------------------------------------------
p = Panel("Seek", "SEEK", "SEK", "RAINFOREST", 12)
p.display("DISPLAY", 3.0, 17.0, 57.96, 33.0)
X4 = [8.5, 22.5, 38.5, 52.5]
p.switch("MODE_PARAM", ["SWEEP", "LISTEN"], 6.0, 45, "MODE")
p.button("HUNT_PARAM", "HUNT_LIGHT", "HUNT", 24.5, 45.5)
p.knob("COUNT_PARAM", "COUNT", 44, 45, size="S")
p.knob("SWEEP_TIME_PARAM", "SWEEP TIME", 10, 61, size="S")
p.knob("DRIFT_PARAM", "DRIFT", 30.5, 61, size="S")
p.knob("PROBE_LEVEL_PARAM", "PROBE LEVEL", 51, 61, size="S")
p.knob("LINGER_PARAM", "LINGER", 10, 76.5, size="S")
p.knob("GLIDE_PARAM", "GLIDE", 30.5, 76.5, size="S")
p.knob("TEASE_LEVEL_PARAM", "TEASE LEVEL", 51, 76.5, size="S")
p.jack("RETURN_INPUT", "RETURN", X4[0], 92, "audio")
p.jack("HUNT_INPUT", "HUNT", X4[1], 92, "gate")
p.jack("PROBE_OUTPUT", "PROBE", X4[2], 92, "audio")
p.jack("TEASE_OUTPUT", "TEASE", X4[3], 92, "audio")
p.jack("NODES_OUTPUT", "NODES", X4[0], 108, "poly")
p.jack("STRENGTH_OUTPUT", "STRENGTH", X4[1], 108, "poly")
p.jack("FOUND_OUTPUT", "FOUND", X4[2], 108, "gate")
p.jack("ARRIVE_OUTPUT", "ARRIVE", X4[3], 108, "gate")
P.append(p)

# ---- 4. WANDER (26 HP) -------------------------------------------------------
p = Panel("Wander", "WANDER", "WND", "RAINFOREST", 26)
p.display("MAP", 3.0, 17.0, 78.0, 76.0)
p.section("AIR", 2.0, 80.5, 79.0, 96.5)
p.section("PICKUP", 2.0, 100.5, 79.0, 117.0)
for i in range(8):
    x = 7.0 + 9.6 * i
    p.jack(f"AIR{i + 1}_INPUT", str(i + 1), x, 87, "audio")
    p.jack(f"PICKUP{i + 1}_INPUT", str(i + 1), x, 107, "audio")
RX = [90.0, 106.0, 122.0]
p.knob("SPEED_PARAM", "SPEED", RX[0], 28)
p.knob("CURIOSITY_PARAM", "CURIOSITY", RX[1], 28)
p.knob("ROOM_PARAM", "ROOM", RX[2], 28)
p.knob("SPEAKERS_PARAM", "SPEAKERS", RX[0], 47)
p.knob("SIZE_PARAM", "SIZE", RX[1], 47)
p.switch("OUTPUT_PARAM", ["STEREO", "QUAD"], RX[2] - 2.0, 47.5, "OUTPUT")
p.button("EAR_PARAM", "EAR_LIGHT", "EAR TO OBJECT", RX[0], 64.5, latch=True)
p.jack("X_INPUT", "X", RX[1], 64, "cv")
p.jack("Y_INPUT", "Y", RX[2], 64, "cv")
p.jack("EAR_INPUT", "EAR", RX[0], 87, "gate")
p.jack("L_OUTPUT", "L", RX[1], 87, "audio")
p.jack("R_OUTPUT", "R", RX[2], 87, "audio")
for i, nm in enumerate(["FL", "FR", "RL", "RR"]):
    p.jack(f"{nm}_OUTPUT", nm, 86.0 + 12.0 * i, 107, "audio")
P.append(p)

# ---- 5. PULSER (12 HP) -------------------------------------------------------
p = Panel("Pulser", "PULSER", "PLS", "PULSERS", 12)
C3 = [10.0, 30.48, 50.96]
p.knob("TIME_BASE_PARAM", "TIME BASE", C3[0], 28)
p.knob("GAIN_PARAM", "GAIN", C3[1], 28)
p.knob("PHASE_PARAM", "PHASE", C3[2], 28, bipolar=True)
p.knob("THRESHOLD_PARAM", "THRESHOLD", C3[0], 47)
p.knob("HOLD_PARAM", "HOLD", C3[1], 47)
p.knob("QUENCH_PARAM", "QUENCH", C3[2], 47)
p.led("FIRE_LIGHT", "FIRE", 20.24, 50)
p.knob("INSTABILITY_PARAM", "INSTABILITY", C3[0], 64, size="S")
p.knob("ACCENT_PARAM", "ACCENT", C3[1], 64, size="S")
p.knob("COUPLE_PARAM", "COUPLE", C3[2], 64, size="S", bipolar=True)
C4x = [8.0, 22.3, 36.7, 51.0]
for x, k, lab in zip(C4x, ["TIME", "GAIN", "PHASE", "INSTAB"], ["TIME CV", "GAIN CV", "PHASE CV", "INSTAB CV"]):
    p.knob(f"{k}_CV_PARAM", lab, x, 77, size="T", bipolar=True)
    p.jack(f"{k}_CV_INPUT", lab, x, 85.5, "cv")
C5 = [7.0, 18.7, 30.48, 42.2, 54.0]
p.jack("COUPLE_INPUT", "COUPLE IN", C5[0] + 0.9, 98.5, "audio")
p.jack("KICK_INPUT", "KICK", C5[1], 98.5, "audio")
p.jack("FIRE_INPUT", "FIRE", C5[2], 98.5, "gate")
p.jack("COUPLE_OUTPUT", "COUPLE\nOUT", C5[3], 98.5, "audio")
p.jack("GATE_OUTPUT", "GATE", C5[4], 98.5, "gate")
for x, k in zip(C5, ["CLICK", "SNAP", "THUMP", "SPUTTER", "LOOP"]):
    p.jack(f"{k}_OUTPUT", k, x, 112.5, "audio")
P.append(p)

# ---- 6. SHIFT (6 HP) ---------------------------------------------------------
p = Panel("Shift", "SHIFT", "SHF", "PULSERS", 8)
C = [7.5, 20.32, 33.14]
p.knob("DEGREES_PARAM", "DEGREES", 11.0, 28, size="S", positions=["0", "90", "180", "270"])
p.knob("PHASE_PARAM", "PHASE", 30.5, 29)
p.knob("STAGES_PARAM", "STAGES", C[0], 48, size="S")
p.knob("SPREAD_PARAM", "SPREAD", C[1], 48, size="S")
p.knob("GAIN_PARAM", "GAIN", C[2], 48, size="S", bipolar=True)
p.knob("PHASE_CV_PARAM", "PHASE CV", C[0], 63, size="T", bipolar=True)
p.knob("GAIN_CV_PARAM", "GAIN CV", C[1], 63, size="T", bipolar=True)
p.knob("SATURATE_PARAM", "DRIVE", C[2], 63, size="S")
p.jack("PHASE_CV_INPUT", "PHASE CV", C[0], 72, "cv")
p.jack("GAIN_CV_INPUT", "GAIN CV", C[1], 72, "cv")
p.jack("IN1_INPUT", "IN 1", C[0], 89, "audio")
p.jack("IN2_INPUT", "IN 2", C[1], 89, "audio")
p.jack("OUT_OUTPUT", "OUT", C[0], 106, "audio")
p.jack("INV_OUTPUT", "INV", C[1], 106, "audio")
P.append(p)

# ---- 7. TUNED (6 HP) ---------------------------------------------------------
L, R = 8.0, 22.48
p = Panel("Tuned", "TUNED", "TND", "PULSERS", 6)
p.knob("TUNE_PARAM", "TUNE", 15.24, 27)
p.knob("Q_PARAM", "Q", L, 45, size="S")
p.knob("GAIN_PARAM", "GAIN", R, 45, size="S")
p.switch("SATURATE_PARAM", ["CLEAN", "TUBE", "HARD"], 8.0, 62, "SATURATE")
p.knob("LEVEL_PARAM", "LEVEL", 23.5, 62, size="S")
p.knob("TUNE_CV_PARAM", "TUNE CV", 15.24, 77, size="T", bipolar=True)
p.jack("TUNE_CV_INPUT", "TUNE CV", 15.24, 86, "cv")
p.jack("IN_INPUT", "IN", 15.24, 99.5, "audio")
p.jack("OUT_OUTPUT", "OUT", L, 113, "audio")
p.jack("INV_OUTPUT", "INV", R, 113, "audio")
P.append(p)

# ---- 8. SIDEBAND (6 HP) ------------------------------------------------------
p = Panel("Sideband", "SIDEBAND", "SBM", "PULSERS", 6)
p.knob("SIDEBAND_PARAM", "SIDEBAND", 15.24, 27, bipolar=True)
p.knob("CARRIER_LEAK_PARAM", "CARRIER\nLEAK", L, 47, size="S")
p.knob("SIGNAL_LEAK_PARAM", "SIGNAL\nLEAK", R, 47, size="S")
p.knob("DRIVE_PARAM", "DRIVE", L, 62.5, size="S")
p.knob("LEVEL_PARAM", "LEVEL", R, 62.5, size="S")
p.knob("SIDEBAND_CV_PARAM", "SIDEBAND CV", 15.24, 74.5, size="T", bipolar=True)
p.jack("SIDEBAND_CV_INPUT", "SB CV", L, 85.5, "cv")
p.jack("SIGNAL_INPUT", "SIGNAL", R, 85.5, "audio")
p.jack("CARRIER_INPUT", "CARRIER", L, 99, "audio")
p.jack("OUT_OUTPUT", "OUT", R, 99, "audio")
p.jack("UPPER_OUTPUT", "UPPER", L, 112.5, "audio")
p.jack("LOWER_OUTPUT", "LOWER", R, 112.5, "audio")
P.append(p)

# ---- 9. LOOP (26 HP) ---------------------------------------------------------
p = Panel("Loop", "LOOP", "LOP", "PULSERS", 26)
GX = [9.0, 23.0, 37.0, 51.0]
GY = [27.0, 41.0, 55.0, 69.0]
p.section("FROM BUS ↓  TO BUS →", 2.0, 18.0, 58.0, 76.0)
for i in range(4):
    for j in range(4):
        p.knob(f"M{i + 1}{j + 1}_PARAM", f"{i + 1}→{j + 1}", GX[j], GY[i], size="S", bipolar=True)
p.display("DISPLAY", 61.0, 17.0, 129.0, 41.0)
LX = [68.0, 84.0, 100.0, 116.0]
for k in range(4):
    p.knob(f"LEVEL{k + 1}_PARAM", f"LEVEL {k + 1}", LX[k], 52, size="S")
    p.switch(f"KEEP{k + 1}_PARAM", ["ON", "OFF"], LX[k] - 3.0, 67, f"KEEP {k + 1}")
p.knob("EDGE_PARAM", "EDGE", 68.0, 84, size="S")
p.knob("SPEED_PARAM", "KEEPER SPEED", 84.0, 84, size="S")
p.knob("ROTATE_PARAM", "ROTATE", 100.0, 84, size="S")
p.knob("EDGE_CV_PARAM", "EDGE CV", 116.0, 84, size="T", bipolar=True)
for k in range(4):
    x0 = 2.0 + 25.5 * k
    p.section(f"BUS {k + 1}", x0, 91.5, x0 + 24.0, 120.0)
    a, b = x0 + 6.5, x0 + 17.5
    p.jack(f"IN{k + 1}_INPUT", "IN", a, 98.5, "audio")
    p.jack(f"RETURN{k + 1}_INPUT", "RETURN", b, 98.5, "audio")
    p.jack(f"SEND{k + 1}_OUTPUT", "SEND", a, 111.5, "audio")
    p.jack(f"OUT{k + 1}_OUTPUT", "OUT", b, 111.5, "audio")
p.jack("EDGE_CV_INPUT", "EDGE CV", 116.0, 98.5, "cv")
p.jack("MIX_L_OUTPUT", "MIX L", 111.0, 111.5, "audio")
p.jack("MIX_R_OUTPUT", "MIX R", 123.0, 111.5, "audio")
P.append(p)

# ---- 10. SWELL (10 HP) -------------------------------------------------------
p = Panel("Swell", "SWELL", "SWL", "PHONEMES", 10)
p.switch("SOURCE_PARAM", ["NOISE", "PING", "CAPTURE"], 6.6, 28, "SOURCE")
p.knob("ATTACK_PARAM", "ATTACK", 27.5, 28)
p.knob("DECAY_PARAM", "DECAY", 42.4, 28)
p.knob("TONE_PARAM", "TONE", 9.0, 47)
p.knob("BODY_PARAM", "BODY", 25.4, 47)
p.knob("SENSITIVITY_PARAM", "SENSITIVITY", 42.0, 47, size="S")
p.knob("VELOCITY_PARAM", "VELOCITY", 9.0, 64, size="S")
p.knob("SCATTER_PARAM", "SCATTER", 25.4, 64, size="S")
p.knob("LEVEL_PARAM", "LEVEL", 42.0, 64, size="S")
p.knob("ATTACK_CV_PARAM", "ATTACK CV", 9.0, 76.5, size="T", bipolar=True)
p.knob("TONE_CV_PARAM", "TONE CV", 25.4, 76.5, size="T", bipolar=True)
p.jack("ATTACK_CV_INPUT", "ATTACK CV", 9.0, 85.5, "cv")
p.jack("TONE_CV_INPUT", "TONE CV", 25.4, 85.5, "cv")
p.jack("TRIG_INPUT", "TRIG", 42.0, 85.5, "gate")
p.jack("IN_INPUT", "IN", 9.0, 99.5, "audio")
p.jack("ENV_OUTPUT", "ENV", 25.4, 99.5, "cv")
p.jack("ONSET_OUTPUT", "ONSET", 42.0, 99.5, "gate")
p.jack("OUT_L_OUTPUT", "OUT L", 17.0, 113, "audio")
p.jack("OUT_R_OUTPUT", "OUT R", 34.0, 113, "audio")
P.append(p)

# ---- 11. PHONEME (14 HP) -----------------------------------------------------
p = Panel("Phoneme", "PHONEME", "PHN", "PHONEMES", 14)
p.display("DISPLAY", 3.0, 17.0, 68.12, 31.0)
p.switch("CARRIER_PARAM", ["INPUT", "SELF", "NOISE"], 6.8, 43, "CARRIER")
p.knob("CHOP_RATE_PARAM", "CHOP RATE", 31.0, 43)
p.knob("CHOP_SIZE_PARAM", "CHOP SIZE", 52.0, 43)
X4p = [9.0, 25.0, 41.0, 57.0]
p.knob("JITTER_PARAM", "JITTER", X4p[0], 60, size="S")
p.knob("SHUFFLE_PARAM", "SHUFFLE", X4p[1], 60, size="S")
p.knob("RESPONSE_PARAM", "RESPONSE", X4p[2], 60, size="S")
p.knob("FORMANT_PARAM", "FORMANT", X4p[3], 60, size="S", bipolar=True)
p.switch("HOLD_PARAM", ["TRACK", "HOLD"], 6.8, 74, "HOLD")
p.button("FREEZE_PARAM", "FREEZE_LIGHT", "FREEZE", 41.0, 74.5, latch=True)
p.knob("MIX_PARAM", "MIX", 57.0, 74, size="S")
J6 = [6.5, 18.1, 29.7, 41.3, 52.9, 64.5]
for x, k, lab in zip(J6[:3], ["RATE", "SIZE", "SHUFFLE"], ["RATE CV", "SIZE CV", "SHUF CV"]):
    p.knob(f"{k}_CV_PARAM", lab, x, 85.5, size="T", bipolar=True)
    p.jack(f"{k}_CV_INPUT", lab, x, 95, "cv")
p.jack("FREEZE_INPUT", "FREEZE", J6[3], 95, "gate")
p.jack("CHOP_INPUT", "CHOP", J6[4], 95, "gate")
p.jack("CARRIER_INPUT", "CARRIER", J6[5], 95, "audio")
p.jack("MODULATOR_INPUT", "MOD", J6[0], 109.5, "audio")
p.jack("OUT_OUTPUT", "OUT", J6[1], 109.5, "audio")
p.jack("ODD_OUTPUT", "ODD", J6[2], 109.5, "audio")
p.jack("EVEN_OUTPUT", "EVEN", J6[3], 109.5, "audio")
p.jack("ENVELOPES_OUTPUT", "ENVELOPES", J6[5] - 3.0, 109.5, "poly")
P.append(p)

# ---- 12. XGATE (24 HP) -------------------------------------------------------
p = Panel("XGate", "XGATE", "XGT", "PHONEMES", 24)
for k in range(4):
    c = 11.0 + 19.0 * k
    p.section(f"CHANNEL {k + 1}", c - 9.0, 18.5, c + 9.0, 96.0)
    p.switch(f"MODE{k + 1}_PARAM", ["OPEN", "DUCK", "OFF"], c - 3.2, 30, "MODE")
    p.knob(f"KEY{k + 1}_PARAM", "KEY", c, 48.5, size="S", positions=["1", "2", "3", "4"])
    p.knob(f"LEVEL{k + 1}_PARAM", "LEVEL", c, 64, size="S")
    p.led(f"GATE{k + 1}_LIGHT", "GATE", c, 73.5)
    p.jack(f"IN{k + 1}_INPUT", "IN", c, 85, "audio")
RX = [88.0, 101.5, 115.0]
p.knob("THRESHOLD_PARAM", "THRESHOLD", RX[0], 28, size="S")
p.knob("DEPTH_PARAM", "DEPTH", RX[1], 28, size="S")
p.knob("SPEED_PARAM", "SPEED", RX[2], 28, size="S")
p.knob("SWITCH_RATE_PARAM", "SWITCH\nRATE", RX[0], 46, size="S")
p.knob("RANDOMNESS_PARAM", "RANDOM-\nNESS", RX[1], 46, size="S")
p.knob("GLIDE_PARAM", "GLIDE", RX[2], 46, size="S")
p.switch("OUTPUTS_PARAM", ["4", "6"], RX[0] - 2.0, 62, "OUTPUTS")
p.knob("RATE_CV_PARAM", "RATE CV", RX[1], 60.5, size="T", bipolar=True)
p.jack("RATE_CV_INPUT", "RATE CV", RX[1], 70, "cv")
p.jack("SWITCH_INPUT", "SWITCH", RX[2], 70, "gate")
p.section("ROUTING", 82.0, 80.0, 120.0, 96.0)
for c in range(4):
    for o in range(6):
        p.items.append(("gled", f"ROUTE{c + 1}{o + 1}_LIGHT", "", 92.0 + 5.0 * o, 83.5 + 3.6 * c, {}))
for i, x in enumerate([9.0, 25.0, 41.0, 57.0, 73.0, 89.0]):
    p.jack(f"OUT{i + 1}_OUTPUT", f"OUT {i + 1}", x, 109, "audio")
p.jack("MOVED_OUTPUT", "MOVED", 107.0, 109, "gate")
P.append(p)


# ---------------------------------------------------------------- components
def components():
    c = {}
    rings = {"PortAudio": "#8a8f94", "PortCv": "#3a6ea5", "PortGate": "#b33a2e", "PortPoly": "#d1a823"}
    for name, col in rings.items():
        nut = []
        for i in range(6):
            a = math.pi / 3 * i + math.pi / 6
            nut.append(f"{4 + 3.55 * math.cos(a):.3f},{4 + 3.55 * math.sin(a):.3f}")
        c[name] = f"""<svg xmlns="http://www.w3.org/2000/svg" width="8mm" height="8mm" viewBox="0 0 8 8">
<circle cx="4" cy="4" r="3.95" fill="{col}"/>
<polygon points="{" ".join(nut)}" fill="#6f7377" stroke="#4a4d50" stroke-width="0.2"/>
<circle cx="4" cy="4" r="2.35" fill="#9ea2a5"/>
<circle cx="4" cy="4" r="1.7" fill="#0b0b0b"/>
</svg>"""
    def knob(name, r, line_w):
        d = 2 * r
        c[name] = f"""<svg xmlns="http://www.w3.org/2000/svg" width="{d}mm" height="{d}mm" viewBox="0 0 {d} {d}">
<circle cx="{r}" cy="{r}" r="{r}" fill="#141414"/>
<circle cx="{r}" cy="{r}" r="{r * 0.82:.3f}" fill="#1e1e1e" stroke="#2e2e2e" stroke-width="0.25"/>
<circle cx="{r * 0.85:.3f}" cy="{r * 0.8:.3f}" r="{r * 0.5:.3f}" fill="#ffffff" fill-opacity="0.05"/>
<line x1="{r}" y1="{r * 0.15:.3f}" x2="{r}" y2="{r * 0.75:.3f}" stroke="#f2f2f2" stroke-width="{line_w}" stroke-linecap="round"/>
</svg>"""
    knob("TudorKnob", 5.5, 0.7)
    knob("TudorKnobSmall", 4.0, 0.6)
    knob("TudorTrim", 3.0, 0.5)

    def toggle(d):
        cx, cy = SW_W / 2, SW_H / 2
        if d == 0:
            handle = f'<circle cx="{cx}" cy="{cy}" r="1.2" fill="#f0f0f0" stroke="#999" stroke-width="0.2"/>'
        else:
            ty = cy + d * 3.1
            handle = (f'<line x1="{cx}" y1="{cy}" x2="{cx}" y2="{ty}" stroke="#d8d8d8" stroke-width="1.2" stroke-linecap="round"/>'
                      f'<circle cx="{cx}" cy="{ty}" r="0.85" fill="#f0f0f0" stroke="#999" stroke-width="0.15"/>')
        return f"""<svg xmlns="http://www.w3.org/2000/svg" width="{SW_W}mm" height="{SW_H}mm" viewBox="0 0 {SW_W} {SW_H}">
<rect x="0.25" y="0.25" width="{SW_W - 0.5}" height="{SW_H - 0.5}" rx="0.6" fill="#2b2b2b"/>
<circle cx="{cx}" cy="{cy}" r="1.9" fill="#9a9a9a" stroke="#6a6a6a" stroke-width="0.25"/>
<circle cx="{cx}" cy="{cy}" r="1.3" fill="#bdbdbd"/>
{handle}
</svg>"""
    # value 0 is the TOP position (the first label)
    c["Toggle2_0"] = toggle(-1)
    c["Toggle2_1"] = toggle(1)
    c["Toggle3_0"] = toggle(-1)
    c["Toggle3_1"] = toggle(0)
    c["Toggle3_2"] = toggle(1)
    return c


def main():
    errs = []
    for p in P:
        for e in p.check():
            errs.append(f"{p.slug}: {e}")
    if errs:
        print("\n".join(errs))
        raise SystemExit(f"{len(errs)} layout problems")
    out_root = os.environ.get("TUDOR_OUT", ROOT)
    if out_root != ROOT:
        os.makedirs(os.path.join(out_root, "res"), exist_ok=True)
        for p in P:
            with open(os.path.join(out_root, "res", f"{p.slug}.svg"), "w") as f:
                f.write(p.svg())
        print(f"wrote {len(P)} {STYLE} panels to {out_root}")
        return
    os.makedirs(os.path.join(ROOT, "res", "components"), exist_ok=True)
    os.makedirs(os.path.join(ROOT, "src", "generated"), exist_ok=True)
    for p in P:
        with open(os.path.join(ROOT, "res", f"{p.slug}.svg"), "w") as f:
            f.write(p.svg())
    for name, svg in components().items():
        with open(os.path.join(ROOT, "res", "components", f"{name}.svg"), "w") as f:
            f.write(svg)
    with open(os.path.join(ROOT, "src", "generated", "Layout.hpp"), "w") as f:
        f.write("// Generated by scripts/make_panels.py. Do not edit.\n#pragma once\n#include \"../widgets/TudorComponents.hpp\"\n\n")
        for p in P:
            f.write(p.ids())
            f.write(p.cpp())
    print(f"wrote {len(P)} panels")


if __name__ == "__main__":
    main()
