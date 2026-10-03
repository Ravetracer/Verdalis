# VerdaliScene status

Version 0.1.0, the suite's eleventh plugin and the first one that synthesises
nothing of its own: every sound comes from one of the nine nature instruments,
run as a layer. Nothing has been released yet.

## What works

- **Nine layer types, four of each.** Rain (RainyDay), thunder (ThunderClap),
  waves (ShoreBreak), wind (SkyHowl), birds (ChirpParade), river (RiverFlow), fire
  (CrackleBlaze), insects (InsectSwarm) and night (NightLife). WhooshPact is left
  out on purpose: it makes production sounds, not a place.
- **A layer is its plugin.** Each is the plugin's own engine, driven through the
  plugin's own `engineParams()` (`<plugin>/src/engine_params.cpp`, extracted from
  each plugin's `syncEngineParams` for this), with the plugin's own parameter
  table, preset library and panel layout. Nothing is copied.
- **Every parameter, at a stable id.** 2,119 host parameters: the 11 scene ones,
  then for each of 36 slots its plugin's whole table plus level, pan, stereo and,
  for birds and night, shot rate. A layer parameter's id keeps the plugin's own
  `ParamId` in its low part (`params.h`), so a plugin that appends a parameter
  gains it here at a new id without moving any other. Names carry the layer for
  the host ("River 1 Water"); the window shows the plugin's own.
- **Plugin presets in layers, both ways.** All 163 factory presets of the nine
  plugins load into a layer and write back out as the same preset, value for
  value — checked by the self-test. A layer saves into its plugin's own user
  folder, folders and packs included, and shows up in the plugin.
- **Drone only.** Every layer is played by one held note: middle C, at velocity
  0.9 — the plugins disagree on a neutral velocity (1.0 for five of them, 0.5 for
  the other four), but every fit and demo in the suite was rendered at 0.9.
  Thunder is ThunderClap's Storm mode with *Mode* pinned and hidden. Its first
  flash lands at a random moment within the storm's first average interval --
  not on the layer's first sample, and not after a full Poisson wait either,
  which with the sound's travel time from kilometres away left a storm silent
  for most of a minute (measured: first thunder heard at 33-50 s at 2-3 flashes
  a minute; now 4-6 s). Birds and night fire their plugin's own `triggerShot()`
  at random at the layer's *Shot Rate*, so a one-bird preset still has its bird.
- **The scene mixer** — level, pan, a stereo balance or a mono point, mute and
  solo (runtime only, never saved), a peak meter per layer and the scene's master.
- **The scene envelope, filter and output** — gate always, with the transport or
  with held notes; ADSR to 30 s; the suite's SVF and 12 dB/oct highpass; width
  and gain; the suite's soft clip at the end. A closed gate whose release has run
  out stops running the layers rather than running them unheard.
- **Layers come and go safely.** Engines are built and prepared on the main
  thread, handed to the audio thread through an exchange, and handed back the
  same way to be deleted once a removed layer has faded out on its own release
  (capped at 30 s). The self-test switches all 36 slots on and off through host
  parameter events with no NaN, no overflow and nothing leaked.
- **Sixteen factory scenes**, written by `tools/analysis/make_scenes.py` from one
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
- **Validation.** Self-test 0 failures. clap-validator: 38 passed, 0 failed,
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
