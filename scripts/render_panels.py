"""Compose each panel with its knobs, jacks and switches into docs/img/<Slug>.svg for the manual.
Then scripts/manual/topng.js renders PNGs. Needs only Python 3."""
import os, re
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RES = os.environ.get("TUDOR_RES", os.path.join(ROOT, "res"))
IMG = os.environ.get("TUDOR_IMG", os.path.join(ROOT, "docs/img"))
lay = open(os.path.join(ROOT, "src/generated/Layout.hpp")).read()
COMP = {"TudorKnob": "TudorKnob", "TudorKnobSmall": "TudorKnobSmall", "TudorTrim": "TudorTrim",
        "TudorToggle2": "Toggle2_0", "TudorToggle3": "Toggle3_0", "PortAudio": "PortAudio",
        "PortCv": "PortCv", "PortGate": "PortGate", "PortPoly": "PortPoly"}

def inner(svg):
    m = re.search(r'viewBox="([^"]+)"', svg)
    body = re.sub(r"^.*?<svg[^>]*>", "", svg, flags=re.S)
    body = re.sub(r"</svg>\s*$", "", body.strip())
    return m.group(1), body

for blk in re.finditer(r"inline void place(\w+)\(W\* w, M\* m\) \{(.*?)\n\}", lay, re.S):
    slug, body = blk.group(1), blk.group(2)
    panel = open(os.path.join(RES, slug + ".svg")).read()
    W, H = re.search(r'width="([\d.]+)mm" height="([\d.]+)mm"', panel).groups()
    vb, pbody = inner(panel)
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="{vb}">', pbody]
    for line in body.splitlines():
        m = re.search(r"Vec\(([\d.]+)f, ([\d.]+)f\)", line)
        if not m:
            continue
        x, y = float(m.group(1)), float(m.group(2))
        cls = None
        for k in sorted(COMP, key=len, reverse=True):
            if f"<{k}>" in line:
                cls = k; break
        if cls:
            csvg = open(os.path.join(ROOT, "res/components", COMP[cls] + ".svg")).read()
            cw, ch = map(float, re.search(r'width="([\d.]+)mm" height="([\d.]+)mm"', csvg).groups())
            cvb, cbody = inner(csvg)
            parts.append(f'<svg x="{x - cw / 2:.3f}" y="{y - ch / 2:.3f}" width="{cw}" height="{ch}" viewBox="{cvb}">{cbody}</svg>')
        elif "LightLatch" in line or "LightBezel" in line:
            parts.append(f'<circle cx="{x}" cy="{y}" r="3" fill="#2a2a2a" stroke="#777" stroke-width="0.4"/><circle cx="{x}" cy="{y}" r="1.6" fill="#ddd"/>')
        elif "TinyLight" in line:
            parts.append(f'<circle cx="{x}" cy="{y}" r="0.8" fill="#444"/>')
        elif "SmallLight" in line:
            parts.append(f'<circle cx="{x}" cy="{y}" r="1.1" fill="#444"/>')
    for m in re.finditer(r"static const float " + slug.upper() + r"_\w+_MM\[4\] = \{([^}]*)\}", lay):
        x, y, w, h = [float(v.strip().rstrip("f")) for v in m.group(1).split(",")]
        parts.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="0.8" fill="#101412"/>')
    parts.append("</svg>")
    open(os.path.join(IMG, slug + ".svg"), "w").write("\n".join(parts))
print("ok")
