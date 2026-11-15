# Build instructions: TUDOR, modules after David Tudor's Rainforest, Pulsers and Phonemes

**Audience:** an Opus model implementing this plugin from scratch.
**Owner:** Hardy (also Haer). Fluent in VCV Rack, does not read C++. Build the whole plugin in one go, test it yourself, and hand over the finished result. No milestone check-ins.

**What it is:** a VCV Rack 2 plugin, brand and slug `Tudor`, with 12 modules that capture how David Tudor's live-electronic works behaved: sound sent through resonant objects and picked up again (*Rainforest*), rhythm born from phase-shifted feedback with no input (*Pulsers*), and short sounds lengthened while long sounds are chopped up (*Phonemes*). Background research and the reasons for each module are in `docs/RESEARCH-AND-PROPOSAL.md` (copy of the proposal file). Read it first.

**Repository:** https://github.com/hardyfinn/Tudor (empty at the start).

**Hardy's decisions (2026-11-15), which override anything else:**

| Question | Answer |
|---|---|
| Plugin name | **Tudor** (slug, brand and name). Personal plugin, not for the VCV Library. |
| Module set | **All 12** modules in §2. |
| OBJECT | **Modelled presets only** (plus the Find generator). It does **not** load WAV recordings or impulse responses. No file dialogs anywhere in the plugin. |
| Labels | Accurate text labels on every control and jack, no symbols (carried over from Hardy's Serge plugin). |
| Search | Short module names findable in Rack's browser (§2.1). |

---

## 0. Ground rules

1. **Spirit of the source.** Tudor built feedback chains from small home-made parts and rode them by hand at the edge of instability. Modules must stay stable and musical when patched into each other in loops, including into themselves. Rack adds one sample of delay per cable; that is fine.
2. **No internal oscillators except where listed.** CRITTER (Rainforest I really used oscillators), SEEK's probe and tease tones, and the noise sources. PULSER, LOOP, SHIFT, TUNED, SIDEBAND, PHONEME and SWELL make sound only from what is patched in, from their own feedback, or from noise.
3. **Never blow up.** Every state variable is soft-clamped (§3.2). Every `process()` ends with an `isfinite` guard that resets that module's state to zero if anything is non-finite. Feedback in any patch must saturate, never produce NaN, inf or DC lock-up.
4. **DSP separate from Rack.** All models live in `src/dsp/` as plain C++17 with no Rack includes, so they can be unit tested on the host. `src/modules/*.cpp` are thin wrappers.
5. **No allocation, locks, file I/O or logging in `process()`.** Allocate buffers in the constructor or on sample-rate change (`onSampleRateChange`).
6. **Every param has a tooltip and unit; every port has a name** (`configParam`, `configInput`, `configOutput`, `configSwitch` with labels for each position). Tooltip names use the panel label wording, spelled out in full.
7. **Ambiguity:** choose what best serves the Tudor behaviour described in this file, write it in `docs/DECISIONS.md`, and list it in the hand-over.
8. **CPU at 48 kHz, one core:** OBJECT under 2.5%, WANDER under 2%, SEEK and PHONEME under 2%, everything else under 1%.
9. Licence GPL-3.0-or-later. Do not copy code from other plugins unless the licence is compatible; credit it in `docs/CREDITS.md`.
10. **Polyphony:** ports are mono unless a section says "poly". Poly ports are for lists of pitches or band levels, not for running several copies of a module.

---

## 1. Setup

- **VCV Rack 2.x**, newest 2.x Rack SDK. Read https://vcvrack.com/manual/PluginDevelopmentTutorial, https://vcvrack.com/manual/Building, https://vcvrack.com/manual/Panel and https://vcvrack.com/manual/DSP first.
- `export RACK_DIR=/path/to/Rack-SDK`, then `make`, `make dist`, `make install`.
- Unit tests with **doctest** (single header) in `tests/`. `make test` builds `tests/*.cpp` with `src/dsp/*.cpp` as a host executable and runs it.
- `.github/workflows/build.yml` builds Windows x64, macOS x64 and arm64, and Linux x64 with the official `rack-plugin-toolchain` Docker image (or the community `build-plugin` action based on it), runs `make test` on Linux, uploads the `.vcvplugin` files as artifacts on every push, and as release assets on tags `v*`. In the Serge project the Rack SDK download was blocked (403) from the cloud container, so Rack builds may only be possible on GitHub Actions. Plan for that: keep `make test` host-only.
- `README.md`: what the plugin is, and install steps (drop the `.vcvplugin` into Rack's `plugins-<os>-<cpu>` user folder, or run `make install`).

```
plugin.json   Makefile   README.md   LICENSE
docs/   BUILD-INSTRUCTIONS.md (this file)  RESEARCH-AND-PROPOSAL.md  DECISIONS.md  CREDITS.md  MANUAL.md  Tudor-Manual.pdf
res/    <Module>.svg ×12,  components/ (jack, knob, switch, button, LED SVGs)
scripts/  make_panels.py  make_manual_pdf.sh
src/
  plugin.hpp  plugin.cpp
  dsp/      Common.hpp  Filters.hpp (SVF, one-pole, allpass, DC block)  Hilbert.hpp  Noise.hpp  Drift.hpp
            EnvFollow.hpp  Onset.hpp  Fft.hpp  DelayLine.hpp  Oversample.hpp
            ModalBank.hpp  Objects.hpp (preset tables)  Object.hpp  Critter.hpp  Seek.hpp  Wander.hpp
            Pulser.hpp  Shift.hpp  Tuned.hpp  Sideband.hpp  Loop.hpp  Swell.hpp  Phoneme.hpp  XGate.hpp
  modules/  one .cpp per module
  widgets/  TudorComponents.hpp  (ports, knobs, switches, buttons, displays)
tests/
```

`plugin.json`: slug `Tudor`, name `Tudor`, brand `Tudor`, version `2.0.0`, licence `GPL-3.0-or-later`, author Hardy. Module names end with the short name in brackets so Rack's search finds both, e.g. `"name": "Object (OBJ)"`. Tags only from Rack's allowed tag list (e.g. `Physical modeling`, `Filter`, `Oscillator`, `Random`, `Mixer`, `Panning`, `Quad`, `Vocoder`, `Ring modulator`, `Frequency shifter`, `Phasing`, `Clock generator`, `Envelope generator`, `Granular`, `Effect`). Rack rejects unknown tags.

---

## 2. The module set

### 2.1 Modules

| # | Slug | Panel title | Short | Group | Width |
|---|---|---|---|---|---|
| 1 | `Object` | OBJECT | OBJ | Rainforest | 16 HP |
| 2 | `Critter` | CRITTER | CRT | Rainforest | 14 HP |
| 3 | `Seek` | SEEK | SEK | Rainforest | 10 HP |
| 4 | `Wander` | WANDER | WND | Rainforest | 26 HP |
| 5 | `Pulser` | PULSER | PLS | Pulsers | 10 HP |
| 6 | `Shift` | SHIFT | SHF | Pulsers | 6 HP |
| 7 | `Tuned` | TUNED | TND | Pulsers | 6 HP |
| 8 | `Sideband` | SIDEBAND | SBM | Pulsers | 6 HP |
| 9 | `Loop` | LOOP | LOP | Pulsers | 24 HP |
| 10 | `Swell` | SWELL | SWL | Phonemes | 10 HP |
| 11 | `Phoneme` | PHONEME | PHN | Phonemes | 12 HP |
| 12 | `XGate` | XGATE | XGT | Phonemes | 16 HP |

Widths are a guide. Widen in 2 HP steps rather than crowd labels; note it in `DECISIONS.md`.

---

## 3. System conventions

### 3.1 Signal levels (standard Rack)

| Kind | Level | Port ring colour (§4.2) |
|---|---|---|
| Audio | ±5 V nominal | grey |
| Bipolar CV | ±5 V | blue |
| Unipolar CV | 0 to 10 V | blue |
| Pitch | 1 V/oct, 0 V = C4 = 261.63 Hz | blue |
| Gate / trigger | 0 or 10 V; inputs fire on a rise through +1.0 V, re-arm below +0.1 V (`dsp::SchmittTrigger` semantics) | red |
| Poly lists (Modes, Nodes, Strength, Envelopes) | as their kind, N channels | yellow |

Inputs accept any signal. Unpatched inputs read 0 V unless a section says otherwise (normalling).

**CV convention:** a CV input marked "+CV" adds `volts × amount` to the knob in the knob's own units, where the amount is a small attenuverter trimpot next to the jack (−1 to +1, default 0 for most, +1 where noted). "Units per volt" is given per control. Every knob plus CV is clamped to the knob's range.

### 3.2 Shared DSP

- `softClamp(x, lim)`: linear to `0.9·lim`, then `0.9·lim + 0.1·lim·tanh((|x| − 0.9·lim)/(0.1·lim))` with sign. Use `lim = 12` V on every output and on every internal state that can grow.
- `fastTanh`: Padé or rational approximation, max error 1e-4 for |x| < 5, exact clamp beyond.
- **SVF** (`Filters.hpp`): zero-delay-feedback (TPT, Zavalishin) state-variable filter, `g = tan(π f / fs)`, `k = 1/Q`, outputs LP, BP, HP, and BP normalised (BP·k). Clamp `f` to `[1 Hz, 0.45·fs]`.
- **One-pole LP/HP**, **first-order allpass** (`y = a·x + x1 − a·y1`, `a = (tan(πf/fs) − 1)/(tan(πf/fs) + 1)`), **DC blocker** (one-pole HP at 8 Hz).
- **Hilbert pair** (`Hilbert.hpp`): two chains of four second-order allpass sections, each `y[n] = c·(x[n] + y[n−2]) − x[n−2]` with `c = a²`. Coefficients (Olli Niemitalo):
  - Chain I: `a = 0.6923878, 0.9360654322959, 0.9882295226860, 0.9987488452737`, followed by a one-sample delay.
  - Chain Q: `a = 0.4021921162426, 0.8561710882420, 0.9722909545651, 0.9952884791278`.
  - Outputs I and Q are 90° apart (±1°) from about 20 Hz to 0.45·fs at 44.1/48 kHz. At 96 kHz and above the low edge moves up proportionally; that is acceptable; note it.
- **Noise** (`Noise.hpp`): xoshiro128+ per instance, seeded from `random::u64()`; uniform, Gaussian (sum of 4 uniforms, scaled), and pink (Paul Kellet's filter).
- **Drift** (`Drift.hpp`): slow wandering value for component instability. Sum of three one-pole-filtered Gaussian noises at corner frequencies 0.05, 0.4 and 3 Hz with weights 0.6, 0.3, 0.1, normalised to unit standard deviation. `drift.next()` returns ≈ N(0,1) with a 1/f-like spectrum.
- **Envelope follower** (`EnvFollow.hpp`): rectify, then one-pole with separate attack and release time constants.
- **Onset detector** (`Onset.hpp`): fast follower (1 ms / 10 ms) and slow follower (40 ms / 300 ms) on |x|. Onset when `fast > slow·ratio` and `fast > floor`, with `ratio = 1.2 + 4·(1 − sensitivity)`, `floor = 0.02 V`, refractory 30 ms. Reports a strength = `fast / 5 V` clamped 0..1.
- **FFT** (`Fft.hpp`): a small real radix-2 FFT written in plain C++ (testable), sizes 512 to 4096, Hann window. Run it in `process()` block-wise at hop boundaries; cost must fit the CPU budget. Do not use threads.
- **Delay line** (`DelayLine.hpp`): power-of-two ring buffer, cubic (Hermite) fractional read.
- **Oversampling** (`Oversample.hpp`): ×2 polyphase half-band, used only where stated (hard clipping and the OBJECT rattle).
- **Parameter smoothing:** one-pole, 10 ms, on every continuous knob that scales audio or sets a frequency. Switches are not smoothed but must not click: crossfade 5 ms where a switch changes a signal path.

### 3.3 Saved state

State that is not a param (OBJECT Find seed, WANDER object positions, SEEK found nodes) is saved in `dataToJson` / restored in `dataFromJson`, so a patch reopens sounding the same.

---

## 4. Panels

### 4.1 Look

Tudor housed his circuits in small plastic boxes and silver aluminium cases, with nothing labelled. These panels nod to the aluminium case but label everything, as Hardy wants.

- **Ground:** brushed aluminium, `#c9cbcc` with a very faint horizontal brushing (a few dozen 0.1 mm lines at 4% opacity, or a subtle linear gradient; keep the SVG small). 0.4 mm dark border inset 1 mm. Rack's `ScrewSilver`.
- **Ink:** `#1a1a1a`. Lines 0.35 mm; section dividers 0.6 mm.
- **Group stripe:** a 3 mm band under the title, colour by piece: Rainforest green `#3f6b45`, Pulsers rust `#a2522a`, Phonemes slate blue `#3e5a7e`. The group name (`RAINFOREST`, `PULSERS`, `PHONEMES`) is printed small in white inside the band.
- **Title:** the panel title from §2.1, capitals, 3 mm cap height. The short name is printed small under the stripe (e.g. `OBJ`).
- **Labels are text and exact.** Every knob, switch position, button, jack, light and display gets a printed label. The label text is the **Label** column of that module's tables, verbatim, in capitals. No pictograms, waveform icons or arrows-as-labels. Bipolar knobs get small `−` and `+` at the ends of the scale. Switch positions are labelled next to each position.
- **Placement:** a knob's label centred above it; a jack's label centred below it. Never overlapping, never rotated. Outputs sit in a darker inset plate (`#2a2a2a` with white labels) so inputs and outputs are told apart at a glance, as in common Rack practice.
- **Type:** an OFL upright sans (e.g. "Jost" or "Share Tech Mono"), capitals, 2.2 mm cap height, 1.8 mm minimum. **Convert all text to paths.**
- `TUDOR` in small capitals at the bottom of each panel.
- **The check:** `scripts/make_panels.py` builds the SVGs and the C++ position table from one shared table per module, and fails if any control lacks a label. Render each panel to PNG and look at it before hand-over.

### 4.2 Components (`res/components/`, `src/widgets/TudorComponents.hpp`)
- **Ports:** a dark knurled-nut jack, 8 mm, with a coloured ring by signal kind (§3.1): grey `#8a8f94`, blue `#3a6ea5`, red `#b33a2e`, yellow `#d1a823`.
- **Knobs:** `TudorKnob` 11 mm black bakelite with a white line; `TudorKnobSmall` 8 mm; `TudorTrim` 6 mm trimpot for CV amounts.
- **Switches:** two- and three-position toggles. **Buttons:** momentary with LED (`VCVLightBezel` style is fine).
- **Displays:** OBJECT, SEEK, WANDER, LOOP and PHONEME have small displays drawn with `NanoVG` in a `Widget::drawLayer(layer 1)` so they glow in the dark. Display ground `#101412`, ink `#cfe8d5`.

### 4.3 Layout rules
- Knobs upper, jacks lower; outputs bottom right in the dark plate.
- Minimum centre spacing: jack to jack 10 mm, knob to knob 14 mm, knob to jack 12 mm, trimpot to its jack 8 mm.
- A CV jack sits directly below its trimpot, which sits below or beside the knob it modulates, joined by a thin line.

---
## 5. Rainforest group

Notation: **K** knob, **T** trimpot, **S** switch, **B** button, **I** input, **O** output, **L** light, **D** display.

### 5.1 OBJECT (OBJ): a resonant object with transducers and a pickup

**What it does.** Models one of the hanging objects of *Rainforest IV*. Audio patched into a transducer input makes the object vibrate. Two outputs: **AIR**, what you hear standing next to the object, and **PICKUP**, what the contact pickup at one point on its surface hears. Tudor called the object "a kind of mechanical filter"; Driscoll calls the pickup sound "a harmonically shifted reflection" of the direct sound. Both must be true here: AIR and PICKUP share modes but weight them differently.

#### Controls

| Kind | Label | Range / positions | Default | Notes |
|---|---|---|---|---|
| K (stepped) | OBJECT | 15 positions: the 14 presets of §5.1.3, then FOUND | WINE BARREL | Name of the current object shown on the display |
| K (stepped) | MATERIAL | METAL, WOOD, GLASS, PLASTIC, CARD | METAL | Used by FIND only |
| B + L | FIND | momentary | | Generates a new random object of the chosen MATERIAL, switches OBJECT to FOUND |
| K | SIZE | −2 to +2 oct | 0 | Scales every mode frequency by 2^SIZE. +CV 1 oct/V |
| K | DAMPING | 0 to 1 | 0.5 | T60 multiplier `2^(4·(0.5 − DAMPING)·2)`: 4× longer at 0, 4× shorter at 1. +CV 0.1/V |
| K | BRIGHTNESS | 0 to 1 | 0.5 | High-mode decay slope, §5.1.2 |
| K | TICKLE | 0 to 1 | 0.25 | Nonlinear response, §5.1.2. +CV 0.1/V |
| K | RATTLE | 0 to 1 | preset default | Contact buzz, §5.1.2 |
| K | DRIVE A | 0 to 2 | 1 | Transducer A level |
| K | POINT A | 0 to 1 | 0.31 | Drive position of A |
| K | DRIVE B | 0 to 2 | 1 | Transducer B level |
| K | POINT B | 0 to 1 | 0.67 | Drive position of B |
| S | B PHASE | IN, OUT | IN | OUT inverts B before it drives the object (Tudor's out-of-phase trick) |
| K | PICKUP POS | 0 to 1 | 0.45 | Where the pickup sits. +CV 0.1/V |
| S | PICKUP TYPE | CARTRIDGE, CONTACT | CARTRIDGE | |
| T + I | SIZE CV, DAMPING CV, TICKLE CV, PICKUP CV | | trims default +1 | |
| I | TRANSDUCER A, TRANSDUCER B | audio | | |
| I | STRIKE | trigger | | Applies a 2 ms raised-cosine impulse (5 V peak) at POINT A, for testing objects without a source |
| O | AIR | audio | | |
| O | PICKUP | audio | | |
| O | MODES | poly 8 ch, pitch | | The 8 currently strongest modes as V/oct, strongest first |
| D | | | | Object name, and a bar per mode (height = current energy, x = log frequency), plus markers for POINT A, POINT B, PICKUP POS on a strip under the bars |

#### 5.1.2 Model

**Mode bank.** Up to 64 modes. Mode *k* has base frequency `f_k`, decay time `T60_k`, a spatial shape `φ_k(x)` for x in 0..1, and a radiation weight `ρ_k`. Each mode is a complex one-pole resonator:

```
p_k = exp(−6.9078 / (T60_k·fs)) · exp(j·2π·f_k/fs)       // 6.9078 = ln(1000)
z_k[n] = p_k·z_k[n−1] + (1 − |p_k|)·u_k[n]              // u_k = drive into mode k
displacement contribution of mode k at x = φ_k(x) · Im(z_k)
```

Coefficients are recomputed every 16 samples (TICKLE and SIZE move them). Drop any mode with `f_k > 0.45·fs` or `f_k < 15 Hz` (set its output to zero, keep its slot).

**Spatial shapes.** Each preset has a geometry type that sets `φ_k`:
- `BAR`, `STRING`, `CABLE`: `φ_k(x) = sin(π·m_k·x)` with integer `m_k`.
- `PLATE_RECT`: modes `(m,n)`; `φ(x) = sin(π·m·x)·sin(π·n·(0.15 + 0.7·x))` (a diagonal path across the plate).
- `PLATE_CIRC`, `SHELL`, `RING`: modes `(m, n)` with `n` nodal diameters; `φ(x) = J-like radial term · angular term`, approximated as `sin(π·(m + 0.5)·x)·cos(n·2.4·x)`.
- `CAVITY` modes (Helmholtz air modes of barrels and gourds): `φ(x) = 1`.
These are deliberately approximate; what matters is that different positions weight modes differently and some positions sit on nodes.

**Drive.** Each transducer input passes through the transducer model, then excites each mode with its shape at its drive point:
```
transducer(x) = HP2(x, 140 Hz, Q 0.7) → peaking +4 dB at 3.2 kHz, Q 1 → y + 0.06·y² − 0.03·y³ (on y/5, rescaled)
u_k = DRIVE_A·φ_k(POINT_A)·tA + sB·DRIVE_B·φ_k(POINT_B)·tB + N_k        // sB = −1 when B PHASE = OUT
```
(The cone-less transducer passes little bass and adds mild distortion, per Rogalsky.) `N_k` is the nonlinear term below. Scale so that a ±5 V sine at a strong mode's frequency, DRIVE 1, gives about ±5 V at AIR with default settings.

**Damping and brightness.** `T60_k = T60_0 · damp · (f_0 / f_k)^β`, with `T60_0` and `f_0` from the preset, `damp` from DAMPING, and `β = 0.2 + 1.6·(1 − BRIGHTNESS)·preset_β_scale`. Clamp `T60_k` to `[0.01 s, 30 s]`.

**TICKLE (nonlinear, "highly non-linear... like tickling someone").** Three effects, all scaled by `t = TICKLE`:
1. *Tension pitch glide:* `E = Σ |z_k|²` smoothed (5 ms). Every mode frequency is multiplied by `1 + 0.04·t·E/25` (clamp at +6%), so loud drive bends pitch up and it settles back as the object rings down, like a gong or a struck sheet.
2. *Mode coupling:* the total displacement at the drive point A, `d = Σ φ_k(POINT_A)·Im(z_k)`, feeds back as `N_k = 0.15·t·φ_k(0.5)·(d²/5 − mean)` (a quadratic term; `mean` is a 50 ms running mean of `d²/5` so it adds no DC). This transfers energy between modes, so strong drive at one node makes other, unrelated modes bloom.
3. *Threshold jump:* when `E` crosses `25·(1 − 0.6·t)` upward, add a random phase kick of size `0.3·t` to the 4 strongest modes once (refractory 50 ms). This gives the sudden "it caught" moments.
At TICKLE 0 the object is linear (tests rely on this).

**RATTLE (loose contact buzz).** A loose contact sits at x = 0.83 with a gap `g = 4 V·(1 − RATTLE) + 0.05`. Compute displacement there, `r = Σ φ_k(0.83)·Im(z_k)`. When `|r| > g`, a collision force `F = −K·(|r| − g)·sign(r)`, `K = 0.8`, is added to every mode's drive with weight `φ_k(0.83)`, and the same force (high-passed at 800 Hz, ×0.5) is added to AIR directly. Run this part ×2 oversampled. Strong on BEDSPRING, STEEL CABLE, WHISKER DISK presets.

**Outputs.**
- **AIR** = `Σ ρ_k · Im(z_k)` where `ρ_k = f_k / (f_k + f_rad)` (small objects radiate low modes poorly; `f_rad` from the preset, scaled by 2^−SIZE), plus `0.03 ×` the transducer signals high-passed at 1 kHz (the driver itself buzzing), then DC block and `softClamp`.
- **PICKUP** = `Σ φ_k(PICKUP_POS)·Im(z_k)`, then the pickup voicing:
  - CARTRIDGE (piezoelectric phono cartridge: "good bass response but roll off quickly in higher frequencies"): low shelf +4 dB below 150 Hz, then LP2 at 3.2 kHz Q 0.8, then LP1 at 6 kHz.
  - CONTACT (contact mic): HP1 at 60 Hz, peak +3 dB at 4.5 kHz Q 1.2, LP2 at 13 kHz.
  Then DC block, level match so PICKUP and AIR have similar RMS on pink noise drive, `softClamp`.
- **MODES** = for the 8 modes with highest smoothed energy (20 ms), `log2(f_k_current / 261.63)` V, strongest first. Channel count is always 8. Update every 16 samples, and hold a mode's slot for at least 50 ms to avoid flicker.

#### 5.1.3 Object presets (`Objects.hpp`)

Each preset generates its mode list from a rule with a fixed seed (so it is identical every time) and ±0.8% random detune per mode, because real objects are never perfect. "Ratios" are relative to `f0`. Table values are starting points; tune by ear and by the tests in §9 so every preset sounds distinct and lively with CRITTER, pink noise and STRIKE. Record final values in `DECISIONS.md`.

| Preset | Geometry | f0 (Hz) | Modes | T60 at f0 (s) | β scale | f_rad (Hz) | Rattle default | Rule |
|---|---|---|---|---|---|---|---|---|
| BEDSPRING | 8 coils of `STRING`, each with f0 × (1 ± 4% random), 6 harmonics each with stiffness B = 0.002 | 95 | 48 | 1.8 | 0.8 | 150 | 0.7 | `f = f0c·m·sqrt(1 + B·m²)` |
| WINE BARREL | `SHELL` + 2 `CAVITY` | 70 | 40 | 0.9 | 1.2 | 120 | 0.0 | cavity at 0.62·f0 and 1.9·f0 (T60 0.4 s), plus wood shell `f = f0·n(n²−1)/sqrt(n²+1)/1.34` for n = 2..12, m = 0..2 (×1, ×1.6, ×2.3), wood damping |
| OIL DRUM | `SHELL` + end-cap `PLATE_CIRC` | 55 | 56 | 3.0 | 0.7 | 90 | 0.1 | end cap: circular-membrane-like ratios 1, 1.59, 2.14, 2.30, 2.65, 2.92, 3.16, 3.50, 3.60, 3.65, 4.06, 4.15, 4.60, 4.83, 4.90, 5.13 (and ×2, ×3 series), shell ring modes from 4.5·f0 up |
| WAGON RIM | `RING` (cast iron) | 140 | 36 | 4.5 | 0.6 | 200 | 0.0 | in-plane ring `f = f0·n(n²−1)/sqrt(n²+1)/1.34` n = 2..10, out-of-plane set ×0.93, pairs split by 0.3% (beating) |
| MILK-CAN LID | `PLATE_CIRC` clamped (stainless) | 310 | 40 | 2.2 | 0.7 | 300 | 0.15 | clamped circular plate ratios 1, 2.08, 3.41, 3.89, 5.00, 5.95, 6.82, 7.79, 8.72, 9.15, 10.6, 11.6... |
| COPPER STILL | `SHELL` + `CAVITY` | 90 | 56 | 2.6 | 0.8 | 140 | 0.05 | cavity at 0.5·f0 T60 0.6 s; shell modes as OIL DRUM but ×1.2 spacing; copper slightly softer (β ×1.1) |
| STYROFOAM BOX | `PLATE_RECT` ×5 faces + `CAVITY` | 180 | 30 | 0.08 | 1.5 | 400 | 0.0 | each face aspect 1.0 to 1.6, `f ∝ m² + (n/a)²`, very short decay, low Q, cavity 0.7·f0 |
| STEEL CABLE | `CABLE` | 38 | 64 | 5.0 | 0.4 | 250 | 0.5 | stiff string, B = 0.0004, plus a second polarisation at +0.4% (slow beating) |
| MICA SHEET | `PLATE_RECT` thin | 240 | 48 | 0.7 | 0.9 | 500 | 0.4 | aspect 1.3, crisp, crackly via rattle |
| GOURD | `CAVITY` + 3 `BAR` (fishing-pole blanks) | 160 | 40 | 1.2 | 0.9 | 180 | 0.2 | cavity at f0 (T60 0.3 s) and 2.7·f0; three free-free bars with f0 of 22, 29, 37 Hz, ratios 1, 2.756, 5.404, 8.933, 13.34, 19.93 ... (bars ring long, T60 3 s) |
| GLASS PLATE | `PLATE_RECT` | 420 | 48 | 2.0 | 0.5 | 600 | 0.0 | aspect 1.4, bright, glassy |
| ALUMINIUM SHEET | `PLATE_RECT` thin, large | 35 | 64 | 3.5 | 0.6 | 160 | 0.1 | aspect 1.6, dense, thunder-sheet; TICKLE glide is strongest here (×1.5) |
| HARD-DRIVE PLATTER | `PLATE_CIRC` clamped at centre | 260 | 48 | 7.0 | 0.4 | 450 | 0.0 | ratios of a centre-clamped disc: 1, 1.09, 1.41, 2.11, 3.09, 4.12, 5.34, 6.82... with nodal-diameter pairs split 0.2% |
| WHISKER DISK | `PLATE_CIRC` (card) + 12 tiny `BAR`s (plastic whiskers) | 130 | 56 | 0.25 | 1.3 | 300 | 0.8 | card disc heavily damped; whiskers f0 random 700 to 2800 Hz, T60 0.5 s, coupled through RATTLE |

**FIND.** Builds a random object from MATERIAL: pick 1 to 3 geometries at random; `f0` log-uniform 30 to 600 Hz; mode count 24 to 64; material sets ranges: METAL (T60 1.5 to 8 s, β scale 0.4 to 0.8, rattle 0 to 0.4), WOOD (0.3 to 1.5 s, 1.0 to 1.4, 0 to 0.2), GLASS (1 to 3 s, 0.4 to 0.6, 0), PLASTIC (0.2 to 1 s, 0.9 to 1.3, 0 to 0.5), CARD (0.05 to 0.3 s, 1.2 to 1.6, 0.3 to 0.9). Seed from `random::u64()`; store the seed and material in JSON. Rebuild the mode list outside `process()` (build into a second buffer on the UI thread, then swap with an atomic flag) so the audio thread never allocates. Crossfade the swap over 20 ms.

**Tests:** with TICKLE 0 and RATTLE 0, an impulse at POINT A gives spectral peaks within 1% of each preset's mode list; a mode whose `φ(PICKUP_POS)` is 0 is at least 30 dB down in PICKUP but present in AIR; B PHASE OUT with identical A and B inputs at the same point cancels to silence (−60 dB); 60 s of 10 V white noise into both inputs at TICKLE 1, RATTLE 1 stays finite and within ±12 V; MODES reports the correct strongest mode for a sine driven at that mode.

### 5.2 CRITTER (CRT): animal-like signal generators (Rainforest I)

**What it does.** Tudor's first *Rainforest* used "oscillators that made animal-like and bird-like sounds" as inputs to the objects ("use only signal generators... at least eight"). CRITTER is four of them. They are meant to drive OBJECTs, not to be heard dry, though they work dry too.

#### Controls

Per voice 1 to 4 (four identical columns):

| Kind | Label | Range | Default (voice 1/2/3/4) | Notes |
|---|---|---|---|---|
| K | PITCH | 30 Hz to 8 kHz, exponential | 1800 / 900 / 3200 / 420 Hz | +PITCH input, 1 V/oct |
| K | WARBLE | 0 to 1 | 0.3 | Fast irregular vibrato: rate 6 to 40 Hz (Drift-modulated), depth 0 to 1 oct |
| K (bipolar) | CHIRP | −1 to +1 | +0.4 / −0.3 / +0.7 / −0.5 | Pitch sweep across each call: ±2 oct at the extremes, exponential curve |
| K | RATE | 0.1 to 20 calls/s, exponential | 2 / 0.7 / 5 / 0.3 | |
| K | DENSITY | 0 to 1 | 0.6 | Probability that a scheduled call happens |
| I | PITCH | 1 V/oct | | |
| O | OUT | audio | | |

Shared:

| Kind | Label | Range | Default | Notes |
|---|---|---|---|---|
| K | SPECIES | 0 to 1 (BIRD at 0, INSECT at 0.5, FROG at 1) | 0.2 | Morph between three call generators |
| K | FOLLOW | 0 to 1 | 0 | How strongly voices are pulled toward FOLLOW pitches |
| K | LEVEL | 0 to 1 | 0.8 | |
| I | FOLLOW | poly pitch | | e.g. OBJECT MODES or SEEK NODES |
| I | CALL | trigger | | Forces a call on all voices |
| I | SPECIES | CV, 0.1/V | | |
| O | MIX | audio | | Sum of the four voices, ×0.5 |
| O | CALL | poly 4 ch gates | | High during each voice's call |

#### Model

Each voice is a sine (phase accumulator, double precision phase). Calls are scheduled with exponentially distributed gaps around `1/RATE` (so timing is irregular), and each scheduled call fires with probability DENSITY. A call has duration `D = clamp(0.35/RATE, 0.02 s, 1.5 s)`.

- **BIRD:** a call is split into 1 to 6 syllables (random, more at high RATE); each syllable has a 3 ms attack, a raised-cosine body, and the CHIRP sweep applied across the syllable; WARBLE applies throughout. Small random pitch offset per call (±1.5 semitones).
- **INSECT:** the call is continuous (D stretched ×3) with amplitude modulated by a pulse train at `40 + 160·WARBLE` Hz (stridulation), duty 30%, edges 0.5 ms; pitch nearly fixed (CHIRP scaled ×0.1); a little phase distortion `sin(φ + 0.6·sin φ)` for buzz.
- **FROG:** pitch divided by 4 (clamp ≥ 30 Hz), amplitude pulsed at 12 to 30 Hz with sharp attack and exponential decay per pulse, second harmonic at −9 dB, calls longer and grouped in pairs or threes.
- SPECIES crossfades the three generators' outputs with equal-power weights between neighbouring species (computed per voice, all three always running is fine at this cost).

**FOLLOW.** When the FOLLOW input is patched with N channels, each voice's base pitch glides (time constant `0.1 + 9.9·(1 − FOLLOW)` s) toward the nearest FOLLOW pitch in log frequency, blended by FOLLOW: `pitch = (1 − FOLLOW)·knob + FOLLOW·nearest`. Voices pick different targets where possible (assign greedily, strongest list order first). This is how CRITTERs learn an object's voice.

**Tests:** call rate within 10% of RATE over 60 s at DENSITY 1; FOLLOW 1 with a fixed 3-channel input converges to those pitches (±10 cents); no output DC.

### 5.3 SEEK (SEK): find the object's voice

**What it does.** Tudor: "start from the other end and go back and arrive at a signal source." Driscoll: after investigation "the composer creates material which will tease the resonant nodes into strong vibration." SEEK automates that. It probes an object, finds its strongest resonances, and then plays a tone that wanders between them.

#### Controls

| Kind | Label | Range / positions | Default | Notes |
|---|---|---|---|---|
| S | MODE | SWEEP, LISTEN | SWEEP | SWEEP probes actively; LISTEN only analyses what comes back |
| B + L | HUNT | momentary | | Starts a new scan (SWEEP) or clears the node list (LISTEN). Light on while scanning |
| K | SWEEP TIME | 2 to 30 s | 8 s | |
| K | COUNT | 1 to 8, stepped | 4 | Number of nodes to find |
| K | DRIFT | 0 to 1 | 0.3 | LISTEN: how quickly the node list follows changes (0 = frozen) |
| K | LINGER | 0.2 to 10 s | 2 s | TEASE dwell time per node |
| K | GLIDE | 0 to 2 s | 0.3 s | TEASE glide between nodes |
| K | PROBE LEVEL | 0 to 1 | 0.7 | |
| K | TEASE LEVEL | 0 to 1 | 0.7 | |
| I | RETURN | audio | | The object's PICKUP (or AIR) |
| I | HUNT | trigger | | |
| O | PROBE | audio | | The sweep or burst, to TRANSDUCER |
| O | TEASE | audio | | The node-seeking sine, to TRANSDUCER |
| O | NODES | poly 1 to 8 ch, pitch | | Found nodes, strongest first. Channel count = number found |
| O | STRENGTH | poly 1 to 8 ch, 0 to 10 V | | Relative strength of each node, strongest = 10 V |
| O | FOUND | trigger | | When a scan completes or the LISTEN list changes |
| O | ARRIVE | trigger | | When TEASE reaches a node |
| D | | | | Measured response curve (log f, 30 Hz to 12 kHz), found nodes marked, TEASE position as a moving dot |

#### Model

**SWEEP.** On HUNT, output an exponential sine sweep from 30 Hz to 12 kHz over SWEEP TIME at PROBE LEVEL × 5 V, with 50 ms fades. Measure the response by synchronous detection: multiply RETURN by the probe's sine and cosine, low-pass each product (one-pole, corner = sweep rate in octaves/s × 3 Hz, at least 5 Hz), magnitude = `2·sqrt(I² + Q²)/probeAmp`. Store into 240 log-spaced bins (1/24 octave). Compensate the detector lag by shifting the curve by the low-pass group delay. At sweep end: smooth over 3 bins, find local maxima, keep peaks at least 1/6 octave apart, refine each peak's frequency by parabolic interpolation, take the COUNT highest. Emit FOUND. PROBE is silent between sweeps.

**LISTEN.** No probe. FFT 4096, hop 1024, Hann, on RETURN. Magnitude spectrum averaged with time constant `0.2 + 20·(1 − DRIFT)` s. Peak-pick as above every hop. Match new peaks to old by nearest log frequency (max 1/6 octave move per update) so channels stay stable; emit FOUND when the set changes.

**TEASE.** A sine that visits nodes. On arrival it stays for LINGER × (0.5 + strength), with a slow ±8 cent vibrato (0.3 Hz) so it rocks across the peak, then picks the next node at random, weighted by strength, never the same twice in a row, and glides there (exponential in pitch, time GLIDE). Its level swells in over 40% of GLIDE and is at full level while lingering. If no nodes are known, TEASE is silent.

Nodes, strengths and the response curve are saved in JSON.

**Tests:** RETURN = a synthetic 3-mode resonator (220, 517, 1430 Hz) driven by PROBE through one sample of delay: SWEEP finds all three within 1/24 octave in the right strength order; LISTEN finds them from pink noise driving the same resonator within 5 s at DRIFT 0.5; TEASE output frequency sits at found nodes while lingering.

### 5.4 WANDER (WND): walk through the forest

**What it does.** In *Rainforest IV* the objects hang in a room, each sounding directly and also through a loudspeaker fed by its pickup. The audience walks among them, so "the work is never heard the same by any audience member." WANDER places up to 8 objects in a virtual room and moves a listener through it.

#### Controls

| Kind | Label | Range / positions | Default | Notes |
|---|---|---|---|---|
| I ×8 | AIR 1 ... AIR 8 | audio | | Direct sound of object n, placed at its object position |
| I ×8 | PICKUP 1 ... PICKUP 8 | audio | | Pickup of object n, played from its loudspeaker position |
| K | SPEED | 0 to 1 m/s | 0.25 | Walking speed of the random path |
| K | CURIOSITY | 0 to 1 | 0.5 | How strongly the path is drawn toward objects that are currently loud |
| K | ROOM | 0 to 1 | 0.3 | Diffuse room sound |
| K | SPEAKERS | 0 to 1 | 0.5 | Level of all PICKUP loudspeakers relative to AIR |
| K | SIZE | 6 to 30 m | 12 m | Room side length; object and speaker layout scale with it |
| B + L | EAR TO OBJECT | latching | off | Listener glides (2 s) to 0.1 m from the nearest object and stays there while on |
| S | OUTPUT | STEREO, QUAD | STEREO | |
| I | X, Y | 0 to 10 V | | When patched, override the path (0 V = west/south wall, 10 V = east/north wall), smoothed 100 ms |
| I | EAR | gate | | Same as the button while high |
| O | L, R | audio | | Stereo (in QUAD mode also front pair) |
| O | FL, FR, RL, RR | audio | | Quad, fixed room orientation |
| D | | | | Top-down map: room square, 8 object dots (lit by level), speaker squares, listener dot with a short trail |

Object positions can be **dragged on the display** with the mouse and are saved in JSON. Default layout: objects on a loose ring at 0.3 of SIZE from the centre with ±10% jitter (fixed seed), staggered "to create open paths"; object n's loudspeaker on the wall nearest to it, offset along the wall so no two coincide.

#### Model

- **Path:** the listener has position and heading. Heading turns by a Drift-driven amount each second; CURIOSITY adds a pull toward the energy-weighted centroid of objects with smoothed AIR level above −30 dBFS; walls repel within 0.5 m; objects repel within 0.4 m (you walk around them, not through). Speed SPEED, eased.
- **Per source** (8 AIR at object positions, 8 PICKUP at speaker positions): distance `d` (min 0.1 m), gain `1/max(d, 0.5)` normalised to 1 at 1 m, with an extra ×(1 + 3·(0.5 − d)/0.4) boost below 0.5 m for AIR sources only (ear against the object, capped at ×4); propagation delay `d/343` s on a fractional delay line (natural Doppler while walking); air absorption one-pole LP at `20 kHz/(1 + d/5)`.
- **STEREO:** listener faces the heading. Azimuth θ → equal-power pan, interaural delay up to 0.6 ms, far-ear head shadow LP at 2 to 18 kHz by |sin θ|. Sources behind are low-passed a further 20%.
- **QUAD:** listener orientation fixed (north = front). Pairwise equal-power panning between the four virtual speakers at ±45° and ±135°. No head shadow.
- **ROOM:** a 4-line FDN (Householder matrix, delays 37, 43, 53, 61 ms × SIZE/12, RT60 `0.4 + 0.15·SIZE` s, LP damping 6 kHz) fed by the sum of all sources at their distance gain; outputs decorrelated to L/R or the four quad outputs.
- Unpatched inputs cost nothing (skip them).

**Tests:** with one AIR source and a fixed X/Y, level falls 6 dB per doubling of distance; panning flips when the listener is moved across the source; delay equals d/343 within one sample; output stays finite with 16 inputs of full-scale noise.

---
## 6. Pulsers group

These modules follow Tudor's own description of his components for *Untitled* (1972) and *Pulsers* (1976): "amplifiers (fixed or variable gain, fixed or variable phase-shift, tuned, saturating types), attenuators, filters... switches, and modulators with variable side-band capability", linked "with multiple feedback loops having variable gain and variable phase-shift", the whole conceived "as a 'giant oscillator'".

### 6.1 PULSER (PLS): rhythm from phase-shifted feedback

**What it does.** Models the trigger behaviour of the Gordon Mumma "complex modulator" that Tudor altered for *Pulsers*: "No primary input signal is used; rather, phase-shifted feedback is the trigger source." A resonant loop builds up on its own, crosses a threshold, fires, is quenched, and builds up again. How fast it builds depends on loop gain and on how well the phase shift lines up with the loop's resonance, so PHASE changes tempo and accent as much as TIME BASE does. INSTABILITY makes the time base wander "and eventually become unstable." Several outputs "having different characters."

The original circuit is not documented in any source found; this is a behavioural model of what Tudor and Tom Johnson describe. Note that in the accuracy ledger.

#### Controls

| Kind | Label | Range / positions | Default | Notes |
|---|---|---|---|---|
| K | TIME BASE | 0.25 Hz to 1 kHz, exponential | 4 Hz | Loop resonance. +CV 1 oct/V |
| K | GAIN | 0 to 3 | 1.4 | Loop gain. Below about 1 the loop does not self-start (it still responds to KICK, COUPLE, FIRE). +CV 0.3/V |
| K (bipolar) | PHASE | −180° to +180° | 0° | Extra phase shift in the loop at TIME BASE. +CV 36°/V |
| K | THRESHOLD | 0.1 to 8 V | 2.5 V | Firing level |
| K | HOLD | 1 ms to 2 s, exponential | 40 ms | Refractory and quench time after each firing |
| K | QUENCH | 0 to 1 | 0.85 | How much of the loop energy each firing removes |
| K | INSTABILITY | 0 to 1 | 0.15 | Time-base drift, noise floor, threshold jitter. +CV 0.1/V |
| K | ACCENT | 0 to 1 | 0.5 | How much the output level follows the loop's overshoot at firing |
| K (bipolar) | COUPLE | −1 to +1 | 0 | Amount of COUPLE IN added to the loop |
| T + I | TIME CV, GAIN CV, PHASE CV, INSTAB CV | | | |
| I | COUPLE IN | audio | | Normally another PULSER's COUPLE OUT |
| I | KICK | audio | | Added straight into the loop (any signal can disturb it) |
| I | FIRE | trigger | | Forces a firing |
| O | CLICK | audio | | |
| O | SNAP | audio | | |
| O | THUMP | audio | | |
| O | SPUTTER | audio | | |
| O | LOOP | audio | | Raw loop signal |
| O | COUPLE OUT | audio | | Same as LOOP but before output scaling, for chaining |
| O | GATE | gate | | High during HOLD |
| L | FIRE | | | Flashes on each firing |

#### Model

```
state: SVF bandpass R (TPT), first-order allpass A1, A2, gain stage, quench multiplier q
fc   = TIME_BASE · 2^(INSTAB·0.5·drift1.next())          // drift up to ±0.5 oct at INSTAB 1
R:   bandpass at fc, Q = 6
A1, A2: first-order allpass at fc·2^(φ),  φ from PHASE: −180° → +3 oct ... 0° → 0 ... +180° → −3 oct (monotonic map; tune so PHASE sweeps the loop phase at fc through about ±180°)
x_in = loop_prev·GAIN·q + COUPLE·couple_in/5 + kick/5 + noise·(1e−5 + 3e−3·INSTAB²)
loop = tanh(A2(A1(R(x_in))))·6          // saturates at about ±6 V
fire when |loop| > THRESHOLD·(1 + 0.3·INSTAB·drift2.next()) and not in HOLD, or on FIRE
on fire:   q ramps to (1 − QUENCH) over 1 ms and stays there for HOLD, then ramps back to 1 over HOLD/4
           also scale R's internal state by (1 − QUENCH)
           overshoot o = |loop|/THRESHOLD (clamped 1..4); level = 1 − ACCENT + ACCENT·(o − 1)/3
```

Quench plus the noise floor gives the relaxation rhythm: after each firing the loop has to grow again from what is left, so the interval depends on GAIN, PHASE, QUENCH and what is coupled in. At high TIME BASE the firings run together into sputter and buzz, which is intended.

**Outputs** (all scaled by `level` at their firing and DC-blocked):
- **CLICK:** a 0.3 ms raised-cosine pulse, 5 V × level, polarity alternating when ACCENT > 0.5.
- **SNAP:** each firing pings a resonator at `2.2 kHz × 2^(0.5·loop_sign·level)` with decay 25 to 60 ms (longer with higher level), 5 V peak.
- **THUMP:** each firing pings a sine at 55 Hz × 2^(TIME_BASE octave/4, capped at 160 Hz) with an exponential pitch drop from ×2.5 to ×1 over 30 ms and decay 180 ms.
- **SPUTTER:** `hardClip(3·loop + 2·noise·INSTAB)` multiplied by an envelope that opens at each firing and decays over HOLD; ×2 oversampled; 5 V peak. "The loudspeakers seem to be trying to clear their throats. They sputter and snap."
- **LOOP:** loop × 0.8. **COUPLE OUT:** loop. **GATE:** 10 V during HOLD.

Six PULSERs coupled in a ring (each COUPLE OUT into the next COUPLE IN) should give six channels of related but drifting rhythms. Put that in the manual as the Pulsers patch.

**Tests:** GAIN 0 and nothing patched: no firings in 10 s. Default settings: firings every 0.1 to 1 s, stable for 60 s at INSTAB 0. TIME BASE ×2 roughly halves the mean interval. PHASE sweep changes the mean interval by at least ×1.5 somewhere in its range. Two coupled PULSERs at equal settings drift into a relation (cross-correlation of their gates rises versus uncoupled). 10 V noise at every input stays finite.

### 6.2 SHIFT (SHF): variable phase-shift amplifier

**What it does.** One of Tudor's "vast number of phase shifting devices", many "switchable" between "90, 180, 270, or 360 degrees out of phase", whose shift was "stationary and did not impose the predictable characteristic of the sine". Used in feedback paths "with very unpredictable results" (davidtudor.org, *Electronics*: phase). A building block for loops.

| Kind | Label | Range / positions | Default | Notes |
|---|---|---|---|---|
| S | DEGREES | 0, 90, 180, 270 | 0 | Fixed broadband shift via the Hilbert pair (§3.2): 0 → I, 90 → Q, 180 → −I, 270 → −Q. (360 is 0.) |
| K | PHASE | 20 Hz to 10 kHz, exponential | 600 Hz | Centre of the variable all-pass chain. +CV 1 oct/V |
| K (stepped) | STAGES | 0, 1, 2, 4, 8 | 2 | Number of first-order all-pass stages (0 = only the fixed shift) |
| K | SPREAD | 0 to 3 oct | 0.5 oct | Stage corners spread evenly in log frequency around PHASE |
| K (bipolar) | GAIN | −2 to +2 | 1 | Output gain, negative inverts. +CV 0.4/V |
| K | SATURATE | 0 to 1 | 0 | Soft clipping: `tanh(x·(1 + 9·SAT)/5)·5/(1 + 2·SAT)`-style, level-compensated |
| I | IN 1, IN 2 | audio | | Summed |
| T + I | PHASE CV, GAIN CV | | | |
| O | OUT | audio | | |
| O | INV | audio | | −OUT |

Signal: `sum → Hilbert (DEGREES) → all-pass chain → GAIN → saturate → softClamp`. DEGREES switching crossfades 5 ms. **Tests:** at STAGES 0, DEGREES 90 a 1 kHz sine comes out shifted 90° ±2°; magnitude response flat ±0.3 dB from 40 Hz to 15 kHz at SAT 0; a SHIFT patched into itself with GAIN 1.2 settles into a bounded oscillation.

### 6.3 TUNED (TND): tuned and saturating amplifier

**What it does.** Tudor's "tuned" and "saturating" amplifier types. A resonant band-pass amplifier with saturation inside the resonator, so in a feedback loop it rings, howls and limits itself instead of exploding.

| Kind | Label | Range / positions | Default | Notes |
|---|---|---|---|---|
| K | TUNE | 20 Hz to 10 kHz, exponential | 440 Hz | +CV 1 V/oct (trim default +1) |
| K | Q | 0.5 to 200, exponential | 8 | |
| K | GAIN | −12 to +40 dB | 0 dB | Input gain |
| S | SATURATE | CLEAN, TUBE, HARD | TUBE | |
| K | LEVEL | 0 to 1 | 0.8 | Output level |
| I | IN | audio | | |
| T + I | TUNE CV | | | |
| O | OUT | audio | | Band-pass |
| O | INV | audio | | −OUT |

Model: TPT SVF band-pass (normalised peak gain 1), with the saturation applied to the band-pass integrator state each sample (as in a nonlinear SVF): CLEAN = `softClamp(·, 12)` only; TUBE = `tanh(s/4 + 0.15) − tanh(0.15)` scaled ×4 (asymmetric, adds even harmonics); HARD = clip at ±4 V, ×2 oversampled. Then LEVEL and DC block. At Q 200 and SATURATE TUBE, white noise in gives a near-sine ring at TUNE with a stable level. **Tests:** centre frequency within 1% at 100 Hz, 1 kHz, 5 kHz; self-feedback (OUT → IN) at GAIN +20 dB gives a bounded oscillation near TUNE in all three modes.

### 6.4 SIDEBAND (SBM): modulator with variable sideband capability

**What it does.** Tudor's "modulators with variable side-band capability". Two inputs, no internal oscillator. One knob moves continuously from lower sideband only, through both sidebands (ring modulation), to upper sideband only; leak knobs bring back carrier and signal, so it can also behave as AM.

| Kind | Label | Range | Default | Notes |
|---|---|---|---|---|
| K (bipolar) | SIDEBAND | LOWER (−1) ... BOTH (0) ... UPPER (+1) | 0 | +CV 0.2/V |
| K | CARRIER LEAK | 0 to 1 | 0 | Adds the carrier input |
| K | SIGNAL LEAK | 0 to 1 | 0 | Adds the signal input |
| K | DRIVE | 0 to 1 | 0 | Diode-ring style soft saturation on both inputs before multiplying |
| K | LEVEL | 0 to 2 | 1 | |
| I | SIGNAL | audio | | |
| I | CARRIER | audio | | |
| T + I | SIDEBAND CV | | | |
| O | OUT | audio | | |
| O | UPPER | audio | | Upper sideband only |
| O | LOWER | audio | | Lower sideband only |

Model: inputs (after DRIVE: `tanh(x·(1 + 4·DRIVE)/5)·5/(1 + DRIVE)`) through two Hilbert pairs giving `xI, xQ, cI, cQ`. `USB = (xI·cI − xQ·cQ)/5`, `LSB = (xI·cI + xQ·cQ)/5`. With `s = SIDEBAND`: `mod = ((1 − s)/2)·LSB + ((1 + s)/2)·USB` (at 0 this equals `xI·cI/5`, the ring product). `OUT = LEVEL·(mod + CARRIER_LEAK·c + SIGNAL_LEAK·x)`, softClamp. **Tests:** SIGNAL 200 Hz, CARRIER 1 kHz, both 5 V: at s = +1 the 1.2 kHz component is at least 35 dB above 800 Hz, at s = −1 the reverse, at s = 0 both within 0.5 dB; with an unpatched CARRIER the output is silent at zero leaks.

### 6.5 LOOP (LOP): the giant oscillator

**What it does.** Makes Tudor's method practical in Rack: "multiple feedback loops having variable gain and variable phase-shift", ridden at the edge of oscillation. The amplifier essay on davidtudor.org says his "manipulation of gain stages is legendary", keeping many stages going "without the system 'taking off'". LOOP is a 4 × 4 feedback matrix with four send/return pairs for external components (SHIFT, TUNED, SIDEBAND, OBJECT, anything), and a KEEPER per bus that rides each bus's gain automatically to hold it at the edge.

#### Controls

| Kind | Label | Range / positions | Default | Notes |
|---|---|---|---|---|
| K ×16 | grid labelled by row and column, e.g. `1→2` | −1.5 to +1.5 (bipolar; negative = phase inverted) | 0 except `1→2`, `2→3`, `3→4`, `4→1` at +0.7 | Gain from bus *i*'s return into bus *j* |
| K ×4 | LEVEL 1 ... LEVEL 4 | 0 to 1 | 0.8 | Output level per bus |
| S ×4 | KEEP 1 ... KEEP 4 | ON, OFF | ON | KEEPER per bus |
| K | EDGE | 0 to 1 | 0.4 | Keeper target: 0 = barely alive (about −36 dBFS), 1 = pushed into saturation (about 0 dBFS) |
| K | KEEPER SPEED | 0.1 to 10 s, exponential | 1.5 s | Slower = longer swells and collapses (the loop "breathes") |
| K | ROTATE | 0 to 1 | 0 | Slowly rotates which crosspoints are strongest (Drift-driven, 0.01 to 0.2 Hz), so the network changes by itself |
| I ×4 | IN 1 ... IN 4 | audio | | External signal into each bus. Optional: a LOOP self-starts from a −100 dB noise floor |
| O ×4 | SEND 1 ... SEND 4 | audio | | Bus signal out to an external component |
| I ×4 | RETURN 1 ... RETURN 4 | audio | | Back from the component. **If unpatched, RETURN = SEND** (internal direct loop) |
| O ×4 | OUT 1 ... OUT 4 | audio | | |
| O | MIX L, MIX R | audio | | Buses 1 and 3 left, 2 and 4 right |
| T + I | EDGE CV | | | |
| D | | | | 4 × 4 grid of crosspoint dots (size = |gain|, colour = sign), four bus meters, and each keeper's current gain as a bar |

#### Model

Per sample, with `r_i` = previous sample's returns (one-sample delay, like a cable):
```
v_j   = IN_j + Σ_i M_ij·r_i + noise_j·1e−5
v_j   = softClamp(6·tanh(v_j/6), 12)                     // gentle bus saturation
SEND_j = k_j · v_j                                       // k_j = keeper gain (1 when KEEP off)
OUT_j  = LEVEL_j · SEND_j
r_j    = RETURN_j patched ? RETURN_j : SEND_j
```
**KEEPER:** RMS follower on `SEND_j` (window 30 ms). Target `T = 5 V·10^((−36 + 36·EDGE)/20)`. Each sample, `log k_j += (log T − log max(rms_j, 1e−6)) · dt / KEEPER_SPEED`, with `k_j` clamped to `[0.02, 8]`. Silent bus → gain climbs until the loop takes off; loud bus → gain falls. Because the keeper is slow and loops have their own dynamics, the system swells, holds, collapses and restarts by itself. Smoothing of crosspoint knobs 20 ms. ROTATE multiplies each `M_ij` by `1 + ROTATE·0.8·drift_ij` (16 independent slow drifts).

**Tests:** default patch with nothing connected self-starts within 5 s and each bus RMS settles within ±4 dB of T; with all KEEP off and gains at +1.5 the output is bounded (≤ 12 V); a return patched through an inverting unity module behaves as a −1 crosspoint; NaN injected into a RETURN resets cleanly.

---
## 7. Phonemes group

*Phonemes* (1981, for Cunningham's *Channels/Inserts*) had two processes feeding "an array of sound modifying electronics, thus creating a multitude of outputs": "a customized percussion generator with the ability of extending attacks up to several seconds in length" and "a vocoder capable of chopping a sound into small pieces". "Tudor takes short sounds and lengthens them and in turn, long sounds and shortens them." The overlap "reminded him of speech".

### 7.1 SWELL (SWL): percussion generator with extendable attacks

**What it does.** Listens for onsets in its input (or takes triggers) and fires percussion voices whose attack can be stretched from a click to 8 seconds, so a short hit becomes a slow swell. The CAPTURE source grabs the input's own onset and stretches it: short sounds lengthened.

| Kind | Label | Range / positions | Default | Notes |
|---|---|---|---|---|
| S | SOURCE | NOISE, PING, CAPTURE | PING | |
| K | ATTACK | 1 ms to 8 s, exponential | 400 ms | +CV 1 oct/V of time |
| K | DECAY | 5 ms to 10 s, exponential | 800 ms | |
| K | TONE | 40 Hz to 10 kHz, exponential | 700 Hz | NOISE: band centre. PING: resonator pitch. CAPTURE: gentle detune of the grains (see model). +CV 1 V/oct |
| K | BODY | 0 to 1 | 0.5 | NOISE: Q 0.7 to 30. PING: decay/ring and second mode level. CAPTURE: grain size 15 to 200 ms |
| K | SENSITIVITY | 0 to 1 | 0.5 | Onset detector (§3.2) |
| K | VELOCITY | 0 to 1 | 0.7 | How much onset strength sets voice level |
| K | SCATTER | 0 to 1 | 0.2 | Random variation per hit of ATTACK (±1 oct), TONE (±1 oct) and pan of the two outputs |
| K | LEVEL | 0 to 1 | 0.8 | |
| I | IN | audio | | Source for onsets and for CAPTURE |
| I | TRIG | trigger | | Fires a voice at full velocity; with IN unpatched, CAPTURE uses silence |
| T + I | ATTACK CV, TONE CV | | | |
| O | OUT L, OUT R | audio | | Voices spread across the pair by SCATTER |
| O | ENV | 0 to 10 V | | Sum of voice envelopes, clamped |
| O | ONSET | trigger | | Each detected onset |

**Model.** Four voices, round robin; a new hit takes the quietest voice if all are busy. Envelope: attack curve `1 − (1 − t)^2.5` (rises gently, important for long attacks), then exponential decay to −80 dB over DECAY. Sources:
- **NOISE:** white noise through a TPT SVF band-pass at TONE, Q from BODY.
- **PING:** a two-mode resonator (TONE and 2.76 × TONE, second mode level 0 to −6 dB by BODY) **driven continuously by low-level noise under the envelope**, not just struck once, so a long attack swells like a bowed gong instead of fading in a decayed ping. Add a single strike impulse at the envelope peak scaled by `(1 − ATTACK_norm)`, so short attacks still click.
- **CAPTURE:** on each onset copy the next 250 ms of IN into the voice's buffer (preallocated, 1 s max at any sample rate). Play it as overlapping Hann grains (4 grains, size from BODY, read position random within the first 60% of the buffer, pitch ratio `2^((TONE_oct − log2(700/261.63))/4)` so the default plays at original pitch and TONE detunes gently) under the envelope. This freezes and stretches the hit.

**Tests:** ATTACK 4 s gives a peak 4 s ± 2% after the trigger; four overlapping hits all sound; CAPTURE of a 1 kHz tone burst outputs mostly 1 kHz at default TONE; onset detection fires once per drum hit on a test signal of clicks spaced 250 ms.

### 7.2 PHONEME (PHN): the chopping vocoder

**What it does.** A 16-band channel vocoder whose band envelopes are chopped: sampled, held and gated in small pieces by a chop clock, so long sounds are cut up and re-voiced into speech-like fragments. Long sounds shortened.

| Kind | Label | Range / positions | Default | Notes |
|---|---|---|---|---|
| S | CARRIER | INPUT, SELF, NOISE | SELF | INPUT uses the CARRIER jack; SELF uses the modulator itself; NOISE uses internal white noise. If INPUT is selected and the jack is unpatched, behave as SELF |
| K | CHOP RATE | 1 to 60 Hz, exponential | 9 Hz | +CV 1 oct/V. Ignored when CHOP input is patched |
| K | CHOP SIZE | 5% to 100% of the period | 55% | Open portion of each chop. +CV 10%/V |
| K | JITTER | 0 to 1 | 0.25 | Random timing of chops (up to ±50% of the period) and random dropping of individual bands per chop (up to 50%) |
| K | SHUFFLE | 0 to 1 | 0 | Per chop, re-routes analysis bands to other synthesis bands. +CV 0.1/V |
| K | RESPONSE | 2 ms to 200 ms, exponential | 15 ms | Envelope follower release |
| S | HOLD | TRACK, HOLD | HOLD | HOLD: band levels are sampled at the start of each chop and held. TRACK: they follow continuously but are still gated |
| B + L | FREEZE | latching | off | Holds the current band levels indefinitely (gating continues) |
| K | FORMANT | −1 to +1 oct | 0 | Shifts synthesis band centres relative to analysis bands |
| K | MIX | 0 to 1 | 1 | Dry modulator to vocoded output |
| I | MODULATOR | audio | | |
| I | CARRIER | audio | | |
| I | CHOP | trigger | | External chop clock |
| I | FREEZE | gate | | |
| T + I | RATE CV, SIZE CV, SHUFFLE CV | | | |
| O | OUT | audio | | |
| O | ODD, EVEN | audio | | Odd and even bands separately (natural stereo pair) |
| O | ENVELOPES | poly 16 ch, 0 to 10 V | | Chopped band levels, for driving other things |
| D | | | | 16 band bars, chop gate indicator, shuffle mapping as lines between an analysis row and a synthesis row |

**Model.**
- Band centres: 16 log-spaced from 120 Hz to 7.5 kHz. Analysis and synthesis filters: two cascaded TPT SVF band-passes per band, Q 5 (≈ 1/3-octave-ish skirts).
- Analysis envelopes: full-wave rectify, attack 2 ms, release RESPONSE.
- Chop clock: internal phase accumulator at CHOP RATE with per-period jitter, or rising edges at CHOP. Each chop *n* opens a window of CHOP SIZE × period (measured period when external) with 2 ms raised-cosine edges. JITTER also drops each band for this chop with probability `0.5·JITTER`.
- HOLD: at window start, sample each band envelope. TRACK: use live envelopes. FREEZE overrides both with the frozen values.
- SHUFFLE: at each chop, build a mapping from analysis band *a* to synthesis band *s*: identity, then for each band with probability SHUFFLE swap it with a band up to `1 + round(7·SHUFFLE)` bands away. Crossfade between mappings across the 2 ms edge.
- Synthesis: carrier through the synthesis bank (centres × 2^FORMANT), each band multiplied by its mapped, gated level, summed with per-band make-up gain so pink noise into both gives roughly unity level.
- OUT = MIX·vocoded + (1 − MIX)·modulator; ODD/EVEN are the respective band sums (×√2); all softClamped.

**Tests:** a sustained vowel-like input (sum of harmonics with a formant envelope) at CHOP RATE 10 Hz, SIZE 50% gives 10 bursts per second with silence between; SHUFFLE 0, JITTER 0, TRACK, SIZE 100% behaves as a plain vocoder (band level error < 2 dB on a test tone in each band); CHOP input overrides the internal rate.

### 7.3 XGATE (XGT): cross-gating and channel switching

**What it does.** On the *Pulsers* record, Nicolas Collins' automated mixer "cross-gated" source tapes, live recordings and the 8-track result. In *Untitled*, a third chain switched its outputs "amongst four channels in the performance space". XGATE does both: four channels, each opened or ducked by another channel's level, then spread over 4 or 6 outputs by a switcher that moves them around.

| Kind | Label | Range / positions | Default | Notes |
|---|---|---|---|---|
| I ×4 | IN 1 ... IN 4 | audio | | |
| S ×4 | MODE 1 ... MODE 4 | OPEN, DUCK, OFF | OPEN | OPEN: this channel sounds while its key is loud. DUCK: it sounds while its key is quiet. OFF: no gating |
| K ×4 (stepped) | KEY 1 ... KEY 4 | 1, 2, 3, 4 | the next channel (1→2, 2→3, 3→4, 4→1) | Which channel's level gates this one. A channel keyed to itself acts as a plain noise gate |
| K ×4 | LEVEL 1 ... LEVEL 4 | 0 to 1 | 0.8 | |
| K | THRESHOLD | −60 to 0 dBFS | −24 dBFS | Key level at the middle of the gate curve |
| K | DEPTH | 0 to 1 | 1 | How far closed a closed gate is (1 = silent) |
| K | SPEED | 1 ms to 1 s, exponential | 30 ms | Gate attack; release is 4 × SPEED |
| S | OUTPUTS | 4, 6 | 6 | |
| K | SWITCH RATE | 0.02 to 10 Hz, exponential | 0.3 Hz | How often a channel moves to another output. +CV 1 oct/V |
| K | RANDOMNESS | 0 to 1 | 0.5 | Irregularity of switching times, and how many channels move at once (1 at 0, up to all at 1) |
| K | GLIDE | 1 ms to 4 s, exponential | 150 ms | Crossfade time when a channel moves (equal power) |
| I | SWITCH | trigger | | Forces a move |
| T + I | RATE CV | | | |
| O ×6 | OUT 1 ... OUT 6 | audio | | OUT 5 and 6 silent in 4-output mode |
| O | MOVED | trigger | | Each move |
| L ×4 | GATE 1 ... GATE 4 | | | Current gate opening |
| L ×24 | 4 × 6 routing grid of small LEDs | | | Which output each channel is on |

**Model.** Key level: RMS follower (10 ms) in dBFS (5 V = 0 dBFS). Gate curve: smoothstep over ±6 dB around THRESHOLD, giving `g ∈ [0,1]`; OPEN uses `g`, DUCK uses `1 − g`; applied gain `1 − DEPTH·(1 − g')`, slewed by SPEED (attack) / 4·SPEED (release). Switcher: each channel starts on output = its own number (5 and 6 empty at first); at each switch event (intervals exponentially distributed around `1/SWITCH_RATE`, mixed with regular intervals by `1 − RANDOMNESS`), choose `1 + floor(RANDOMNESS·3.99·u)` channels and move each to a random different output (two channels may share an output). Moves crossfade over GLIDE with equal-power curves.

**Tests:** a loud IN 2 opens channel 1 in OPEN mode and silences it in DUCK mode; total power across outputs is constant (±0.5 dB) during a glide; in 4-output mode OUT 5 and 6 stay at 0 V.

---

## 8. Leaning on existing VCV modules

Do not build these; mention them in the manual where they help, with the reason:

| Need | Existing module | Why |
|---|---|---|
| Room reverb after WANDER or on PULSER outputs | Valley Plateau, Surge XT Reverb 2 | WANDER's ROOM is a small FDN on purpose; a big hall is not Tudor-specific |
| A pitched resonator as a contrast to OBJECT | Audible Instruments Resonator (Mutable Rings) | Different, tonal resonator; useful in LOOP returns |
| Real spring character | Befaco Spring Reverb | One module, sounds right in a LOOP return |
| Channel strips and plain mixing | MindMeld MixMaster, VCV Mixer | Not worth reinventing |
| Recording 4, 6 or 8 outputs as stems | VCV Recorder (stereo), NYSTHI multitrack recorders | Tudor and Collins built the records from multitrack tapes |
| A clean vocoder to compare with PHONEME | Surge XT Vocoder | It does not chop, so it is not a substitute |
| Hand control of many loop gains | any MIDI-mapping module (VCV MIDI-Map, Stoermelder MIDI-CAT) | Tudor rode dozens of knobs by hand; mapping LOOP and PULSER knobs to a controller is the real way to play this set |

---

## 9. Tests (`tests/`)

For every module, the tests in its section, plus:
- **Stability:** 10 s of white noise at ±10 V into every input with random knob settings, at 44.1, 48, 96 and 192 kHz: all outputs finite and within ±12 V. Repeat with each module's main output fed back into its first audio input through one sample of delay.
- **Silence:** with nothing patched and default knobs, non-generator modules (SHIFT, TUNED, SIDEBAND, SWELL, PHONEME, XGATE, WANDER, OBJECT) output exactly 0 V after 1 s (flush denormals). PULSER and LOOP are self-starting by design and are exempt.
- **Sample-rate independence:** OBJECT mode frequencies, TUNED centre, PULSER mean interval (at INSTAB 0), PHONEME chop rate and SEEK found-node frequencies within 1% across the four rates.
- **CPU:** a host benchmark per module at 48 kHz, printed by `make test`, checked against §0.8.

In Rack itself (manual check before hand-over, if a Rack build is possible): load every module, run each manual patch from §10, change sample rate while running, save and reload, check CPU meters, drag WANDER objects, press FIND repeatedly.

---

## 10. Manual (`docs/MANUAL.md` and `docs/Tudor-Manual.pdf`)

Hardy liked the Serge manual as a PDF with patch recipes. Same here. `scripts/make_manual_pdf.sh` builds the PDF from the Markdown (pandoc with a LaTeX or wkhtmltopdf engine, whichever CI can install; commit the PDF).

Structure:
1. A one-page introduction: who David Tudor was, the three pieces in a paragraph each (from `RESEARCH-AND-PROPOSAL.md`), and the idea of "composing inside electronics".
2. Levels and conventions.
3. One section per module: what it is, where it comes from in Tudor's work (one or two quoted phrases, with source), a control table, and two or three patches as cable lists.
4. **Patch recipes** (each as a cable list plus knob settings and what to listen for):
   - *Rainforest, one object:* CRITTER MIX → OBJECT TRANSDUCER A; OBJECT AIR and PICKUP → WANDER AIR 1 and PICKUP 1.
   - *Finding the voice:* OBJECT PICKUP → SEEK RETURN; SEEK PROBE → OBJECT TRANSDUCER A; press HUNT; then SEEK TEASE → TRANSDUCER A and SEEK NODES → CRITTER FOLLOW.
   - *Out of phase:* the same CRITTER voice into TRANSDUCER A and B at different POINTs, B PHASE OUT, sweep POINT B.
   - *A small forest:* four OBJECTs (different presets), each fed by a CRITTER voice or a SEEK, all into WANDER, three to six hours if you like.
   - *Pulsers, six channels:* six PULSERs coupled in a ring, mixed outputs (CLICK, SNAP, THUMP, SPUTTER per cell) into XGATE in 6-output mode; ride TIME BASE, PHASE and INSTABILITY by hand.
   - *Untitled, the giant oscillator:* LOOP with SHIFT in return 1, TUNED in return 2, SIDEBAND (signal = bus 3, carrier = bus 1 via a SEND split) in return 3, OBJECT in return 4; EDGE 0.4; let it breathe; play the crosspoints.
   - *Phonemes:* one source (a PULSER SNAP, or any recording) into SWELL (lengthening) and PHONEME (shortening), both into LOOP inputs with a SIDEBAND and an OBJECT in the returns; XGATE to four channels.
5. A short "playing it" section: map the important knobs to a MIDI controller; record stems; perform later.
6. Credits and sources (§12).

---

## 11. Order of work

1. Copy this file to `docs/BUILD-INSTRUCTIONS.md` and the proposal to `docs/RESEARCH-AND-PROPOSAL.md`. Repo skeleton, Makefile, CI workflow, components, panel script, one empty module that loads.
2. Shared DSP (§3.2) with tests, including the Hilbert pair and FFT.
3. SHIFT, TUNED, SIDEBAND (small, and they validate the shared DSP).
4. LOOP, then PULSER.
5. OBJECT with all presets and FIND, then SEEK, CRITTER, WANDER.
6. SWELL, PHONEME, XGATE.
7. All panels drawn, rendered and inspected with the label check. Manual and PDF. Full test pass. CI green on all platforms.
8. Hand-over note: what was built, the contents of `DECISIONS.md`, what in §12 could or could not be verified, and install steps.

---

## 12. Accuracy ledger

**From sources:**
- Rainforest: coneless transducers on objects; pickups (piezo phono cartridges with good bass and fast high roll-off, later contact mics); direct sound plus amplified pickup "as an enhancement"; the object lists; transducers weak in the bass; "highly non-linear" response; two transducers out of phase; animal-like oscillators as Rainforest I sources; objects suspended in a space the audience walks through, with ears against objects.
- Pulsers: Mumma's complex modulator altered at its trigger mechanisms and output ports for rhythm variety and multiple outputs of different character; no primary input; phase-shifted feedback as the trigger source; performer-variable time base that can become unstable; six channels; sputter and snap; Collins' cross-gating mixer.
- Untitled: chains with multiple feedback loops of variable gain and phase shift, a "giant oscillator"; the component types (fixed/variable gain and phase, tuned, saturating amplifiers, attenuators, filters, switches, variable-sideband modulators); a third chain switching outputs among four channels.
- Phase shifters switchable to 90, 180, 270, 360 degrees, stationary, used in feedback paths (davidtudor.org).
- Phonemes: percussion generator with attacks extended to several seconds; vocoder chopping sound into small pieces; short sounds lengthened, long sounds shortened; speech-like result.

**This spec's own design (no schematic was found; behaviour only):**
- Every DSP model, constant and range in §5 to §7.
- The OBJECT mode rules and preset numbers: plausible physics, not measurements of the real objects.
- PULSER's loop topology (resonator, two all-passes, saturation, comparator, quench). Mumma's circuit is undocumented in the sources found.
- PHONEME's band count and chop method; SWELL's three sources; SEEK and WANDER entirely (they automate Rainforest practice; Tudor had no such devices); LOOP's keeper (it stands in for Tudor's hands).

**Sources:**
- DRAM liner notes, *David Tudor: Three Works for Live Electronics* (Lovely Music): https://www.dramonline.org/albums/david-tudor-three-works-for-live-electronics/notes
- Tom Johnson, "David Tudor's Homemade Pulsers", Village Voice, 24 Jan 1977: https://editions75.com/tvonm/articles/1977/david-tudors-homemade-pulsers.html
- John Driscoll and Matt Rogalsky, Getty symposium paper on Rainforest: https://getty.edu/research/exhibitions_events/events/david_tudor_symposium/pdf/driscollrogalsky.pdf
- CDM on Rainforest (2024): https://cdm.link/2024/07/david-tudor-rainforest/
- EARS realization of Rainforest IV (2017): https://www.oliverschneller.net/rainforest
- davidtudor.org Electronics pages (phase shifters, amplifiers, Rainforest diagrams): https://davidtudor.org/Electronics/electronics.html
