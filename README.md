# Tudor for VCV Rack

Twelve VCV Rack modules after David Tudor's *Rainforest*, *Pulsers* and *Phonemes*.

| Group | Modules |
|---|---|
| Rainforest | OBJECT (OBJ), CRITTER (CRT), SEEK (SEK), WANDER (WND) |
| Pulsers | PULSER (PLS), SHIFT (SHF), TUNED (TND), SIDEBAND (SBM), LOOP (LOP) |
| Phonemes | SWELL (SWL), PHONEME (PHN), XGATE (XGT) |

- Manual with patch recipes: [docs/MANUAL.md](docs/MANUAL.md), also as [docs/Tudor-Manual.pdf](docs/Tudor-Manual.pdf).
- Design choices and where the build departs from the spec: [docs/DECISIONS.md](docs/DECISIONS.md).
- Sources: [docs/CREDITS.md](docs/CREDITS.md).

## Install

GitHub Actions builds the plugin for Apple silicon Macs (`mac-arm64`) on every push. Download the `Tudor-mac-arm64` artifact from the latest run under Actions, unzip it, and copy the `.vcvplugin` file into Rack's plugins folder (`~/Library/Application Support/Rack2/plugins-mac-arm64/`). Rack unpacks it on the next start. Tagged versions (`v*`) also appear under Releases.

## Build

```
export RACK_DIR=/path/to/Rack-SDK
make && make install     # or: make dist
make test                # host tests of the DSP models, no Rack SDK needed
make panels              # regenerate res/*.svg and src/generated/Layout.hpp (Python 3 + fonttools)
```

The DSP lives in `src/dsp` as plain C++ with no Rack includes, so it is tested on the host (`tests/`). The panels, the C++ control positions and the param/port enums all come from one table in `scripts/make_panels.py`.

License: GPL-3.0-or-later.
