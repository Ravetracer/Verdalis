# VerdaliScene status

Version 0.5.1, the suite's eleventh plugin and the first one that synthesises
nothing of its own: every sound comes from one of the nine nature instruments,
run as a layer. 0.1.0 shipped in suite 0.25.0.

## What changed in 0.5.1

- Rain layers carry RainyDay 1.10.1's fix: with a layer's *Random Seed* at 0,
  its trickle no longer falls back to the same drips whenever the host resets
  the plugin.

## What changed in 0.5.0

- **Preset collections**, in the scene library and in every layer's: `NEW`,
  `RENAME` and `DELETE` in the browser, a preset dragged onto a collection moves
  there, a right-click renames, describes or deletes one of your own, and `SAVE`
  picks the collection from a list and takes a description. Renaming a scene
  rewrites only its name and description lines, so every layer section stays as
  it was; the scene's and each layer's current preset follow a move or a rename.
  Same code as every other plugin; the self-test runs the shared checks on the
  scene library and a birds layer's, and checks that a renamed scene keeps every
  layer.

## What changed in 0.4.0

- **FX Tails**, a scene parameter on the OUTPUT panel, decides where the scene's
  envelope sits against the effects. *Ring Out*, the default, envelopes each
  layer between its placement and its effects, and the scene's effects come
  after that, so every reverb and delay rings on past the scene's release;
  layers stopped by a run-out release keep running their effects with silence
  going in until the chains are quiet, and the host's tail length adds the
  longest layer chain and the scene's. *Release* envelopes the output after the
  scene's effects, so every tail fades with the scene and the scene's chain is
  cleared once the release has run out. Before this the layers' tails were cut
  by the release and the scene's rang on: neither of the two. The self-test
  checks both modes on a layer's and the scene's reverb, and each check was
  seen to fail with its mode broken.
- **The cuckoo**: ChirpParade 0.8.0's tenth species is in every birds layer, its
  two presets in every birds layer's browser, and a scene is built on it:
  **Cuckoo Wood**, a near cuckoo and a far pair answering through a long reverb.
  Nineteen factory scenes.
- 4,484 host parameters: one more scene parameter, appended, so no id moved.

## What changed in 0.3.0

- **Effects on every layer and on the scene**: reverb, delay, chorus, flanger,
  phaser, widener and auto-pan, run in the order phaser, chorus, flanger, delay,
  reverb, widener, auto-pan. 61 parameters per
  channel, in a block of ids of their own after the layers' (`kFxIdBase`), so no
  released id moved. Designed from Pirkle (*Designing Audio Effect Plugins in
  C++*, 2nd ed.), Zoelzer (*DAFX*, 2nd ed.) and the musicdsp archive; the
  sources are cited where each design decision is made, in `src/fx/`.
  - Reverb: 16-line FDN, Hadamard matrix, per-line RT60 gains and a Jot damping
    pole solved exactly at the damping frequency, Dattorro's input diffusers,
    Hermite-read drift on every line, freeze. Measured: RT60 within 6 % of the
    knob at 1, 4 and 20 s and two sizes; a frozen tail holds within 0.5 dB over
    30 s (frozen lines snap to whole samples and stop drifting, because no
    fractional read is perfectly flat and a lossless loop repeats it forever).
  - Delay: Hermite reads, tape glide (faster shortening than lengthening) or a
    two-head crossfade, Butterworth loop filters, a bounded rational-tanh
    saturator so 150 % feedback settles near 0 dBFS, wow plus random drift
    scaled with the delay, diffusion inside the delay time, ducking. Measured:
    echoes land on the sample in all three modes and come back at unity.
- **Effect buffers are made on demand**, on the main thread, the first time an
  effect is switched on, and kept until deactivation; the audio thread waits for
  an acquire-load of the effect's ready flag. Nothing is allocated for effects
  nobody uses.
- **The effects view**: a strip's row of letters or a layer's *FX* opens one
  channel's effects as a page of the suite's own panels.
- **Two scenes built around the effects**, *Cave Mouth* and *Canyon Echo*, with
  their faders set from `--layers` renders of the finished scenes (a reverb adds
  level a solo measurement does not see).
- **Cost**: about 1.5 to 2.5 % of a core per channel with reverb, delay and
  chorus on at 48 kHz. A five-layer scene with all three on every layer and a
  reverb and delay on the scene renders at 5x real time on one core.

## What changed in 0.2.0

- **A key starts and ends the scene.** *Gate* defaults to *Notes*, and so do the
  sixteen factory scenes. The gate is now every layer's drone note as well as the
  scene envelope's: opening it strikes each layer's note, closing it lets each
  go, so a layer's own *Attack* and *Release* play inside the scene's. *Always*
  is the 0.1.0 behaviour and *Transport* follows the host; both stay. Projects
  and scenes from 0.1.0 carry their own *Gate* and sound as they did.
- **Channel strips.** Each mixer strip carries its layer's envelope as a graph
  and as knobs (the plugin's own *Attack*, *Decay*, *Sustain*, *Release*) and
  its filter (*Filter Type*, *Highpass*, *Filter Cutoff*, *Filter Resonance*).
  The master strip is the scene's equivalent, with a light for the gate. More
  layers than fit (eleven at 1548 px) scroll.
- **Envelope curves.** *Attack Curve*, *Decay Curve* and *Release Curve* for the
  scene and for every layer (thunder has no decay, so no decay curve). They bend
  the shared `verdalis::Adsr`, whose curve of exactly 0 is the old code path:
  all ten plugins render bit-identically to before (776 renders: every preset,
  two rates, two seeds). A layer's curves are scene parameters, like its level,
  carried to its engine through `EngineParams` fields every nature plugin now
  has and none of them sets itself.

## What works

- **Nine layer types, four of each.** Rain (RainyDay), thunder (ThunderClap),
  waves (ShoreBreak), wind (SkyHowl), birds (ChirpParade), river (RiverFlow), fire
  (CrackleBlaze), insects (InsectSwarm) and night (NightLife). WhooshPact is left
  out on purpose: it makes production sounds, not a place.
- **A layer is its plugin.** Each is the plugin's own engine, driven through the
  plugin's own `engineParams()` (`<plugin>/src/engine_params.cpp`, extracted from
  each plugin's `syncEngineParams` for this), with the plugin's own parameter
  table, preset library and panel layout. Nothing is copied.
- **Every parameter, at a stable id.** 4,484 host parameters: the 15 scene ones and
  the scene's 61 effect ones, then for each of 36 slots its plugin's whole table
  plus level, pan, stereo, the envelope curves, for birds and night shot rate,
  and its 61 effect ones. A layer parameter's id keeps the plugin's own
  `ParamId` in its low part (`params.h`), so a plugin that appends a parameter
  gains it here at a new id without moving any other. Names carry the layer for
  the host ("River 1 Water"); the window shows the plugin's own.
- **Plugin presets in layers, both ways.** All 163 factory presets of the nine
  plugins load into a layer and write back out as the same preset, value for
  value — checked by the self-test. A layer saves into its plugin's own user
  folder, folders and packs included, and shows up in the plugin.
- **Drone only, held by the gate.** Every layer is played by one held note,
  struck when the scene's gate opens and let go when it closes: middle C, at
  velocity 0.9 — the plugins disagree on a neutral velocity (1.0 for five of them, 0.5 for
  the other four), but every fit and demo in the suite was rendered at 0.9.
  Thunder is ThunderClap's Storm mode with *Mode* pinned and hidden. Its first
  flash lands at a random moment within the storm's first average interval --
  not on the layer's first sample, and not after a full Poisson wait either,
  which with the sound's travel time from kilometres away left a storm silent
  for most of a minute (measured: first thunder heard at 33-50 s at 2-3 flashes
  a minute; now 4-6 s). Birds and night fire their plugin's own `triggerShot()`
  at random at the layer's *Shot Rate*, so a one-bird preset still has its bird.
- **The scene mixer** — a channel strip per layer: envelope graph and knobs,
  filter, level, pan, a stereo balance or a mono point, mute and solo (runtime
  only, never saved), a peak meter per layer, and the scene's own strip.
- **The scene envelope, filter and output** — gate on held notes, with the
  transport or always; ADSR to 30 s with a curve per stage; the suite's SVF and
  12 dB/oct highpass; width and gain; the suite's soft clip at the end. A closed
  gate whose release has run out stops the layers -- cutting what is left of
  their own releases -- rather than running them unheard, and the next opening
  starts them afresh.
- **Layers come and go safely.** Engines are built and prepared on the main
  thread, handed to the audio thread through an exchange, and handed back the
  same way to be deleted once a removed layer has faded out on its own release
  (capped at 30 s). The self-test switches all 36 slots on and off through host
  parameter events with no NaN, no overflow and nothing leaked.
- **Nineteen factory scenes**, written by `tools/analysis/make_scenes.py` from one
  table and balanced by measurement (below).
- **Repeatable with a seed.** With every layer's *Random Seed* fixed, a scene
  renders identically after a reset — the engines' randomness and the scene's
  own (first flash, shot clock) alike.

## What is measured

- **Levels.** `verdaliscene-render --layers` renders a scene whole and each layer
  alone. Every start preset used by a factory scene was measured solo at 0 dB
  (`tools/analysis/levels.tsv`, 30 s each, 90 s for thunder), and each layer's
  fader is the distance from that level to a target set by its role: bed −27,
  second −30.5, texture −33.5, accent −37, faint −40 dBFS RMS; thunder by its
  flashes' peaks, −5 to −12 dBFS. Birds that only sing now and then were placed
  from their peaks over long renders of the finished scene instead, because one
  or two phrases in a probe say little. Re-measured in place, layers land within
  about 2 dB of their targets; the scenes sit at −23 to −27 dBFS RMS.
- **Cost.** About 2 MB of memory per layer. Four layers render at roughly 25×
  real time on one core, thirteen at 3.5×.
- **Validation.** Self-test 0 failures, including: every effect, switched on for
  a layer through host parameter events, changing the sound and staying finite,
  and the scene's reverb ringing on 11 s after the key (both checked to fail with
  the effects stubbed out); effects through the scene format and a preset pack; every factory scene silent
  until a key and playing on one, the scene fading to exact silence after the key
  and starting again on the next, and both a scene's and a layer's release curve
  changing the energy after the key (each checked to fail with the curves cut). clap-validator: 38 passed, 0 failed,
  6 skipped. Steinberg's VST3 validator: 47/47, and 537/537 with `-e`, with no
  CLAP installed — see *The VST3 entry* below.

## What is not measured

**Nothing here has been listened to.** The balance is numbers. The suite's own
rule — validate by ear, then by measurement — has only had its second half.

## Changes this needed elsewhere in the suite

- `<plugin>/src/engine_params.{h,cpp}` in all nine nature plugins: the
  parameter-to-engine mapping, moved out of `plugin.cpp` unchanged. Every factory
  preset of every plugin renders bit-identical to before at two rates and two
  seeds.
- `windowSpec()` and `createOrnament()` in each plugin's `gui.cpp`, so a layer page
  is the plugin's own layout. Windows pixel-identical to before.
- `shared/include/verdalis/gui/plugin_window.h`: the shared window's class, moved
  out of `window.cpp` and given a few virtual hooks so this window can extend it
  rather than copy it. Every other plugin's window pixel-identical to before.
- `SKYHOWL_WITHOUT_VENTS` in SkyHowl's engine, which VerdaliScene sets: the wind
  layer is built without SkyHowl's hidden function and the recordings it plays,
  so VerdaliScene contains no sampled audio at all.
- **The VST3 entry, in every plugin.** On Linux the clap-wrapper's search for the
  CLAP entry inside its own binary never worked, so every `.vst3` quietly loaded a
  separately installed `.clap` of the same name from `~/.clap` — and failed where
  there was none. Each plugin now builds the wrapper with
  `STATICALLY_LINKED_CLAP_ENTRY`, takes its entry by address, and loads on its own.

## Limits worth knowing

- Four layers of a kind is part of the id layout and cannot grow without new ids.
- A layer's own internal mixer (RainyDay's DISTANT / TRICKLE, say) is not offered
  as an overlay on its page; its levels are on the panels.
- Removing a layer cannot be undone, except by the host's own undo of the state.
- Scene presets switch layers' parameters at once; a layer kept across a preset
  change is not crossfaded.
- A layer's envelope curves are the scene's, not the plugin's: a layer saved as a
  plugin preset leaves them behind, and the plugins themselves have no curve
  controls yet. Their engines already take curves (`EngineParams`), so exposing
  them in a plugin is a parameter and a panel cell.
- The strips' knobs have no typed entry; the same parameters on the layer's page
  and the scene's panels do.
