# Decisions

Where the build departs from `docs/BUILD-INSTRUCTIONS.md`, or fills in something it left open, and why.

## Scope and build
- **Plugin name "Tudor"**, all 12 modules, OBJECT uses modelled presets only (no WAV loading): Hardy's choices.
- **CI builds Apple silicon Macs only** (`mac-arm64`), plus the host model tests on Linux. Hardy only needs that build. Other platforms can be added back to `.github/workflows/build.yml` as matrix entries.
- **Shared DSP is grouped into fewer files** than the spec lists: `Common`, `Noise` (noise and Drift), `Filters` (SVF, one-poles, all-pass, biquad, DC blocker), `Hilbert`, `Analysis` (envelope follower, RMS, onset, FFT, spectrum), `DelayLine`, `Oversample`.
- **Param, port and light enums are generated** by `scripts/make_panels.py` together with the panels and positions, so the C++ cannot drift from the panels.

## Panels
- **Widths** differ from the spec so every label fits without overlap: OBJECT 18 HP, CRITTER 20, SEEK 12, WANDER 26, PULSER 12, SHIFT 8, TUNED 6, SIDEBAND 6, LOOP 26, SWELL 10, PHONEME 14, XGATE 24.
- **WANDER and LOOP group their jacks in labelled sections** (AIR, PICKUP on WANDER; BUS 1 to 4 on LOOP) so 16 inputs read clearly.
- **SHIFT's SATURATE knob is labelled DRIVE** on the panel: SATURATE did not fit at 8 HP. The tooltip says what it does.
- **PHONEME's MODULATOR jack is labelled MOD** for the same reason.
- Font: DejaVu Sans Bold converted to paths (the spec suggested Jost or Share Tech Mono; DejaVu was available offline and reads well at 2 mm).
- **CV trimpots default to 0** except OBJECT's four and TUNED's TUNE CV (+1), as the spec says.

## OBJECT
- **SIZE + makes the object bigger and lower.** The spec's formula `2^SIZE` would raise pitch; a knob called SIZE should make a bigger object sound lower, so frequencies scale by `2^−SIZE`.
- **Changing OBJECT fades out over 10 ms, swaps, then fades in**, instead of a 20 ms crossfade. Two mode banks would double the cost; the fade is inaudible as a click and costs nothing.
- **RATTLE is not oversampled.** The collision force is already smooth enough at 44.1 kHz with the 800 Hz high-pass; oversampling the whole mode bank to oversample it would double OBJECT's CPU.
- **RATTLE's default is 0** for every preset (one knob cannot follow a per-preset default without jumping when OBJECT changes). The manual names the presets where RATTLE suits.
- **STRIKE** is added to TRANSDUCER A's input, so it lands at POINT A through the transducer model.
- **FIND runs on the UI thread** (from the widget's `step()`), so the new mode list is built where allocation is allowed, then swapped in with atomics. Without a widget (headless Rack), FIND does nothing.
- Output calibration (`airCal`, `pickCal`) is computed per object at load from its mode list.

## PULSER
- **Resonator Q is 1.5, not 6**, and the extra phase is applied with a narrow-band quadrature pair (all-passes at fc·2.414 and fc/2.414) rotated by the PHASE angle. With Q 6 and two all-passes the loop rarely fired and the PHASE extremes killed it.
- **PHASE ±180° on the knob applies ±90° in the loop.** Beyond ±90° the loop cannot self-oscillate at all; at half scale the whole knob changes rhythm instead of having dead ends.
- **Defaults TIME BASE 6 Hz, GAIN 2.0** (spec: 4 Hz, 1.4) so a fresh PULSER fires every 0.1 to 1 s as the spec's test asks.
- Noise floor `0.003 + 0.01·INSTABILITY²` (larger than the spec's) so the loop restarts reliably after a strong quench.
- Each PULSER seeds its own noise from `random::u64()`, so six identical PULSERs still drift apart.

## TUNED, SHIFT, SIDEBAND, LOOP
- TUNED applies its saturation to the band-pass integrator scaled by the damping, and TUBE is normalised to unity slope at zero, so self-oscillation stays at a similar level at any Q.
- **LOOP's buses have a 3 kHz one-pole low-pass** (the bus amplifier's bandwidth) and a DC blocker. Without them a loop of direct returns screams at Nyquist.
- LOOP's keeper uses a 30 ms RMS on SEND; its test checks the geometric mean of that RMS against the target.

## SEEK
- **208 bins, not 240**: 1/24-octave bins from 30 Hz to 12 kHz is 208. The display spans exactly that range.
- With nothing found, NODES and STRENGTH carry one channel at 0 V (Rack ports cannot carry zero channels).

## WANDER
- If only one of X and Y is patched, the other holds the middle of the room.
- In QUAD, L and R carry the front pair, as the spec says.

## PHONEME, SWELL, XGATE
- PHONEME output gain is 19.5, calibrated so pink noise in SELF at SIZE 100% and TRACK comes out at about unity.
- XGATE's LEVEL is applied after the key detector, so turning a channel down does not change how it gates others.
