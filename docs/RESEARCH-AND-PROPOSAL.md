# David Tudor-style modules for VCV Rack: research and proposed set

Draft 1, 2026-11-15. Nothing here is decided until Hardy picks. Plugin name: **Tudor** (Hardy's choice, 2026-11-15). See "Choices for Hardy" at the end.

Note on titles: the Tudor piece is **Pulsers** (1976). "Pulsars" is a common slip.

---

## Part 1. How the three pieces actually worked

### Rainforest (1968, versions I to IV; IV from 1973)

- **The principle.** Sound is not played through loudspeakers. It is sent through **transducers** ("basically a speaker without a cone") fixed to physical objects, so the object itself vibrates and radiates. Tudor: the source sounds "will be modified by the resonant nodes of those materials"; "a kind of mechanical filter."
- **Two outputs per object.** You hear the object directly in the room, and a **pickup** on it (originally piezoelectric phono cartridges, later contact mics) is amplified through a conventional speaker "as an enhancement". Driscoll calls the pickup sound "a harmonically shifted reflection" of what you hear directly, because the pickup sits at one point on the object and hears a different mix of modes.
- **Hardware character.** Transducers "do not transmit bass frequencies efficiently". Phono cartridges have good bass but roll off the highs quickly. The object response is "highly non-linear", like "tickling someone": push a node and it can jump, rattle or bloom.
- **Objects** (Chocorua 1973 and later): metal bedspring, large wine barrel, toilet floats, cast-iron wagon wheel rims, a stainless milk-can lid, lawn sprinklers, a copper still, a styrofoam box, heavy metal cable, a 55-gallon drum, a gourd with fishing-pole blanks, a 5 ft hard-drive platter, a sheet of mica, cardboard disks with plastic whiskers. 16 to 40 objects, suspended in a room the audience walks through.
- **Sources.** Version I: "oscillators that made animal-like and bird-like sounds" (Tudor's note: "use only signal generators, any kind, as inputs, at least eight"). Version II: voice. Version III: tapes (birdsong, whale song, satellite data, mosquitoes in a jar). Version IV: each performer makes material for their own objects, from electronic sounds to "a fly walking on paper". One rule: no pre-recorded music.
- **Method.** Work backwards from the object: "start from the other end and go back and arrive at a signal source." Investigate an object, then make material "which will tease the resonant nodes into strong vibration." Driving one object with **two transducers out of phase** gives results "that did not exist in the source material."
- **Listening.** 3 to 6 hours, at least four performers, no two listeners hear the same piece because each walks a different path; you can put your ear against an object.

### Pulsers (1976)

- **Core:** a "complex modulator" designed by Gordon Mumma for the Expo '70 Pepsi Pavilion, which Tudor altered at its **trigger mechanisms** and **output ports** "to enhance the variety of rhythms possible, and to allow multiple outputs having different characters."
- **No input signal.** "No primary input signal is used; rather, **phase-shifted feedback is the trigger source.**"
- **Unstable time.** The "time-base common to the rhythms can be varied in many different ways by a performer, and can eventually become unstable." Tudor stresses analog, not digital.
- **In performance** (Tom Johnson, Village Voice, 1977): six channels, 15 to 20 small homemade components on a table; "with the touch of a dial any one of them might speed up, slow down, shift its accent, fade in, or fade out." The speakers "sputter and snap", "trying to clear their throats". Mostly about rhythm, little care for tuning or polish.
- **On record:** Nicolas Collins' microcomputer-automated mixer **cross-gated** the source tapes, live recordings and the 8-track result.

### Untitled (1972), the method behind both

Not asked for, but it is Tudor's clearest description of his feedback practice, so the module set uses it: "two chains of components, each chain linked together with **multiple feedback loops having variable gain and variable phase-shift**", conceived "as a 'giant oscillator'", with no oscillators or recorded sounds at all. Components, "mostly home-brew": amplifiers (fixed or variable gain, fixed or variable phase shift, **tuned**, **saturating**), attenuators, filters, switches, and "**modulators with variable side-band capability**". "No control voltages, as such, were employed." A third chain switched its outputs "amongst four channels in the performance space."

### Phonemes (1981)

- For Merce Cunningham's *Channels/Inserts* (stage premiere, City Center, 24 March 1981).
- Two processes feed "an array of sound modifying electronics, thus creating a multitude of outputs":
  1. "a customized **percussion generator** with the ability of **extending attacks up to several seconds** in length";
  2. "a **vocoder capable of chopping a sound into small pieces**."
- "Tudor takes short sounds and lengthens them and in turn, long sounds and shortens them." The overlapping results "reminded him of speech", hence the name.

### Threads that run through all three

1. The electronics are the composition. "Electronic components have their own personality" (the reason for the group name Composers Inside Electronics).
2. Feedback and resonance replace oscillators.
3. The performer rides the edge of instability by hand.
4. Many outputs of different character, spread through space.

Sources: [DRAM liner notes, *Three Works for Live Electronics*](https://www.dramonline.org/albums/david-tudor-three-works-for-live-electronics/notes) (Tudor's own notes on Untitled, Pulsers, Phonemes); [Tom Johnson, "David Tudor's Homemade Pulsers", 1977](https://editions75.com/tvonm/articles/1977/david-tudors-homemade-pulsers.html); [Driscoll and Rogalsky, Getty symposium paper](https://getty.edu/research/exhibitions_events/events/david_tudor_symposium/pdf/driscollrogalsky.pdf); [CDM on Rainforest, 2024](https://cdm.link/2024/07/david-tudor-rainforest/); [EARS realization of Rainforest IV, 2017](https://www.oliverschneller.net/rainforest). Gaps: no source gives schematics for the Pulsers modulator or the Phonemes vocoder; davidtudor.org has electronics pages I have not yet read and the build spec will check them.

---

## Part 2. Proposed set: 12 modules

Short names in brackets, findable in Rack's browser search as in the Serge set. Accurate text labels on every control. Default levels: ±5 V audio, ±5 V or 0 to 10 V CV, standard VCV.

### Rainforest group: objects that speak

**1. OBJECT [OBJ]. A resonant object with transducer and pickup.** The heart of the set. A physical model (bank of up to 64 damped modes with nonlinear coupling), not a reverb.
- *Object* selector with modelled presets from the real lists: Bedspring, Wine Barrel, Oil Drum, Wagon Rim, Milk-Can Lid, Copper Still, Styrofoam Box, Steel Cable, Mica Sheet, Gourd, Glass Plate, Aluminium Sheet, Hard-Drive Platter, Whisker Disk. Plus **Find**: a button that generates a new random object from a chosen material.
- *Transducer A* and *Transducer B* inputs, each at its own drive point, with a *B Phase* switch (in/out). The transducer model has the weak bass and mild cone-less distortion.
- *Size*, *Damping*, *Brightness* (how fast high modes decay), *Tickle* (amplitude-dependent pitch glide, mode jumps and energy transfer), *Rattle* (contact buzz, strong on bedspring, cable, whisker disk).
- *Pickup Position*: where on the object the cartridge sits. *Pickup Type*: Cartridge (bass, high roll-off) or Contact Mic.
- Outputs: **Air** (what you hear standing next to it), **Pickup** (the "harmonically shifted reflection"), and **Modes** (poly V/oct of the 8 strongest current modes, for other modules to chase).
- CV on Size, Damping, Tickle, Pickup Position.

**2. CRITTER [CRT]. Rainforest I's animal-like signal generators.** Four warbling sine voices, as in Tudor's first version. Per voice: *Pitch*, *Warble* (fast wobbly vibrato), *Chirp* (pitch sweep per call), *Call Rate*, *Call Density* (random gaps). Shared *Species* knob morphs between bird, insect (buzz, pulse train) and frog-like (amplitude-modulated croak) behaviour. Each voice out plus a mix. A *Follow* poly input pulls the voices toward pitches from OBJECT's Modes or SEEK.

**3. SEEK [SEK]. Find an object's voice, "start from the other end".** Patch an object's Pickup into SEEK. It sends a slow probe sweep or noise burst out, listens to what comes back, and locks onto the strongest resonant nodes. Outputs: *Probe* audio (to the transducer), *Nodes* (poly V/oct of up to 8 found nodes), *Strength* (poly 0 to 10 V per node), and *Tease*: a sine that wanders between the found nodes, lingering on each. *Hunt* button re-scans; *Drift* lets it keep tracking while the object changes. This automates the Rainforest workflow: investigate the object, then make material that excites it.

**4. WANDER [WND]. Walk through the forest.** An 8-input spatial mixer that models a listener moving among suspended objects. Each input has a fixed position on a small map on the panel, plus an *Air/Pickup* pair (so each object has both its direct sound and its amplified pickup at a different place). The listener follows a slow random path, or *X/Y* CV. *Ear To Object* button pulls the listener close to the nearest object. Outputs stereo and quad. Distance gives level drop, air-absorption high cut and a little delay.

### Pulsers group: feedback electronics with their own rhythms

**5. PULSER [PLS]. Rhythm from phase-shifted feedback, no input needed.** One cell modelled on Tudor's altered Mumma modulator: a loop of a gain stage and a variable phase-shift network that rings, and a trigger mechanism (comparator, one-shot, refractory hold) that fires when the loop crosses a threshold and kicks the loop again. Rhythms come out of the loop itself.
- *Gain*, *Phase*, *Threshold*, *Hold*, *Time Base*, *Instability* (component drift and noise in the time base; at high settings the rhythm drifts, stutters and breaks up), *Accent*.
- Several outputs "having different characters": **Click** (raw trigger), **Snap** (short ringing burst), **Sputter** (gated noisy loop signal), **Thump** (low ringing pulse), **Loop** (the raw feedback signal), **Gate**.
- *Couple In* and *Couple Out* let several PULSERs perturb each other's loops, so a rack of 3 to 6 cells becomes Tudor's tableful of components with six channels.
- Optional *Kick In* (no input is needed, but any signal can disturb the loop).

**6. SHIFT [SHF]. Variable phase-shift amplifier.** A Tudor component: 1 to 8 all-pass stages, *Phase* (centre), *Spread*, *Gain* (−2 to +2, so it can invert), *Saturate*. Two inputs mixed. Built to sit inside feedback loops.

**7. TUNED [TND]. Tuned and saturating amplifier.** A resonant band-pass amplifier with soft clipping: *Tune*, *Q*, *Gain*, *Saturate* (clean, tube-like, hard). In a loop it becomes a ringing, howling, self-limiting oscillator.

**8. SIDEBAND [SBM]. Modulator with variable sideband capability.** Two inputs, no internal oscillator. Morphs continuously between amplitude modulation (carrier present), balanced ring modulation (both sidebands), and single sideband (upper or lower only, via a Hilbert pair, i.e. a frequency shift). *Carrier Leak*, *Sideband* (lower, both, upper), *Mix*.

**9. LOOP [LOP]. The "giant oscillator".** A 4 by 4 feedback matrix: four inputs, four outputs, four send/return pairs for patching SHIFT, TUNED, SIDEBAND, OBJECT and so on into the loops. Each crosspoint has *Gain* and a *Phase* invert. A *Keeper* (slow, soft automatic gain riding per loop) holds the system at the edge of oscillation, the way Tudor rode it by hand, with an *Edge* knob for how close to the cliff it sits. Outputs 1 to 4 plus a stereo mix. This is what makes the Untitled method practical in Rack: without it, feedback patches either die or explode.

### Phonemes group: speech from long and short

**10. SWELL [SWL]. Percussion generator with extendable attacks.** Detects onsets in an input (or takes a trigger) and fires a percussion voice (tuned noise, pinged resonator, or the input's own transient captured) with *Attack* from 1 ms to 8 s, *Decay*, *Tone*, *Body*. Also a *Lengthen* section: each onset of the input is captured and stretched (grain freeze) so short sounds become long. Four-voice round-robin so overlaps pile up.

**11. PHONEME [PHN]. The chopping vocoder.** A 16-band vocoder (modulator in, carrier in, no internal oscillator; with nothing in Carrier it uses the modulator against itself through the band filters). A *Chop* clock (internal rate or external trigger) sample-and-holds and gates the band envelopes, so long sounds are cut into small pieces. *Chop Size*, *Chop Jitter*, *Band Shuffle* (re-routes analysis bands to other synthesis bands per chop), *Freeze*. Outputs: mix, odd and even bands, and a poly output of the band envelopes. This is where "long sounds" get shortened and the speech-like quality comes from.

**12. XGATE [XGT]. Cross-gating and channel switching.** Four inputs. Each channel's level can open or duck any other (cross-gating, like the Collins mixer used on Pulsers). An automatic switcher spreads the results over 4 or 6 output channels, moving at random or on a trigger, like Untitled's third chain. *Threshold*, *Depth*, *Switch Rate*, *Randomness*, mode per channel (open, duck, off).

### How the groups connect

A minimal Rainforest: CRITTER → OBJECT (Transducer A), SEEK on the Pickup, both OBJECT outs into WANDER.
A minimal Pulsers: three PULSERs coupled, outputs through XGATE to six channels.
A minimal Phonemes: SWELL and PHONEME on the same source (one lengthening, one shortening), through LOOP with a SIDEBAND and an OBJECT in its returns.

---

## Part 3. Where existing VCV modules already do the job

Use these alongside the set rather than duplicating them:

| Need | Existing module | Why use it |
|---|---|---|
| Room reverb after WANDER | Valley Plateau, Surge XT Reverb 2 | A room is not Tudor-specific; good free reverbs exist |
| A second kind of resonant body | Audible Instruments Resonator (Rings) | Different, more pitched resonator; good contrast to OBJECT |
| Spring-like objects | Befaco Spring Reverb | Real spring character in one module |
| Plain mixing and channel strips | MindMeld MixMaster, VCV Mixer | Not worth reinventing |
| Multitrack capture of the 6 or 8 outputs | VCV Recorder (stereo) or NYSTHI multitrack recorder | So material can be recorded, then performed with later |
| A standard vocoder to compare with PHONEME | Surge XT Vocoder | Not chopping, so not a substitute |

Not covered by any existing module I know of: transducer-and-pickup objects with two outputs, the node-seeking prober, phase-shift-triggered pulsers, a feedback matrix with gain keeping, and a chopping vocoder. Those are the reasons for the set.

---

## Choices for Hardy

Defaults are marked. Everything else is decided above and can change later.

1. **Plugin name.** Decided: *Tudor*.
2. **Real objects.** Decided: modelled presets only (Hardy, 2026-11-15). No WAV loading in OBJECT.
3. **Size of the set.** Decided: all 12 (Hardy, 2026-11-15).
4. **Repository.** A new GitHub repo is needed for the build, for example `hardyfinn/Tudor`. Please create it and I will add it to the project.
