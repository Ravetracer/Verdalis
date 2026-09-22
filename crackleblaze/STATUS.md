# CrackleBlaze status

Version 0.1.0. First working version: the engine, the parameter set, the window
and a first fitted preset library.

## Preset folders and packs (0.2.0)

User presets can be filed in folders. Saving as `Folder/Name` writes into a
folder one level under the user preset directory, creating it if it is not
there; a name with no slash saves into the root, as before. The browser lists
the library by shelf -- *All*, the factory set, each user folder, *Unfiled* --
with a count on each.

A whole folder exports as one **preset pack** (`.crackleblazepack`), written to a
`packs` directory beside `presets`, and imports back as a new folder. Nothing is
overwritten: the same pack imported twice gives two folders. A pack carries each
preset's text verbatim, so a round trip is lossless.

`EXPORT AS...`, *Other file...* and `REVEAL` use the desktop's own chooser
(`zenity`/`kdialog`, `xdg-open`, or the Windows dialogs) and are not drawn where
there is none; `EXPORT` into the plugin's own `packs` folder always works.

All of it is shared -- `shared/src/preset_library.cpp`, the browser in
`shared/src/gui/window.cpp`, `shared/src/gui/filedialog.cpp` -- and was ported
from SäureKiste in `Ravetracer/audio-plugins`. The offline renderer's preset
crawl is now recursive too, so `--list` agrees with the window's browser.


## A VST3 as well (2026-09-12)

CrackleBlaze ships as a VST3 alongside the CLAP, for Linux and Windows, in every
release archive. It is not a port: the clap-wrapper hosts the same
`CrackleBlaze-impl` static library the `.clap` is built from, so the engine, the
parameter table, the presets and the window are one copy behind both formats.

It passes Steinberg's validator 47/47 on Linux; the Windows build is checked by
loading it under wine and instantiating it, because the validator does not
cross-build. The window needed no change at all -- it was already an embedded
X11 window driven from `clap_host_timer_support`, which is what VST3's
`IRunLoop` embedding wants.

The factory presets are embedded in the binary, so the plugin's own browser is
fully stocked in a VST3 with no files on disk. What a VST3 host will *not* do is
list them in its own browser: the CLAP preset-discovery factory has no
equivalent in the wrapper release in use.

`./install.sh --vst3` builds and installs both. See *The VST3 builds* in the
suite `CLAUDE.md`.


## What works

- **The roar as a measured filterbank.** Eight octave-wide bandpasses per
  channel, driven at gains *solved* so that their sum is the measured
  octave-band curve rather than each band being set to it. Five colours, shipped
  as the centroids of the five clusters the usable reference library falls into,
  with `Blend` making them a continuum.
- **The crackles were gated out before that clustering was run**, and at a median
  crest factor of 31.7 dB that is not a refinement: the plain spectrum of a fire
  is substantially the spectrum of its crackles, and clustering on it sorts the
  references by microphone distance rather than by what kind of fire they are.
- **One flare, not eight.** The measured correlation between neighbouring bands'
  100 ms envelopes is 0.54-0.91, median 0.85, against RiverFlow's 0.07-0.25 on the
  same statistic. A common walk at sqrt(0.85) and a per-band one at sqrt(0.15)
  reproduce it. A fire surges as one flame; a river does not.
- **The three-level spawner**, which is the finding the whole engine rests on: a
  slowly wandering rate, a Poisson process at that instantaneous rate, and a 1-3
  pulse train per arrival. Measured at three timescales that disagree — Fano 3.90
  at one second, exactly Poisson at 50 ms, twice Poisson below 10 ms, branching
  ratio 0.02. No cascade.
- **The flare drives the crackle rate as well as the roar.** Not a design choice
  but a derivation: for a Poisson process at a modulated rate the Fano factor is
  1 + lambda·CV²·tau, and the library's Fano of 3.90 at 29/s with a 0.55 Hz corner
  gives CV = 0.59 — which is what a log-normal rate multiplier of width `Flare`
  produces at its default of 0.55. The same number sets both.
- **A crackle is a click, not a ring.** Measured spectral flatness 0.67, so no
  resonator is fitted. `Body` carries its low end, which the measurement says is
  still within 8 dB of the peak at 250 Hz.
- **A sizzle is a crackle held open**: 19 ms against 2.5 to fall 10 dB, same
  colour to within 2 dB per octave. `Sap` moves events between the two
  populations, over the measured range 0.03-0.57.
- **The settle layer**, found by an onset detector on 80-300 Hz that rejects
  anything also jumping in the top — because a loud crackle leaks into every band
  and would otherwise be counted twice.
- **Distance** as air absorption plus a downward tilt, with the events multiplied
  and their edges smeared — and *bypassed outright* at zero, because no distance
  means no air to absorb.
- **The hearths** set the early reflected field: 0.03 for a fire in the open
  against 0.80 for a stove. ShoreBreak's lesson, reused rather than relearned.
- **Preset discovery**, state save/load, sample-accurate parameters, host
  modulation, bounded voice and event pools.
- **The window**, in CrackleBlaze's own ember-orange theme, with tongues of flame
  along the header and an ember thrown up wherever a crackle happens,
  deterministic in the crackle number.
- **Self-test**: 0 failures, including a fixed Random Seed rendering identically
  after a reset, every parameter at its extremes staying finite and bounded, and
  preset round-tripping. Output is byte-identical across runs at 48 and 96 kHz
  with the seed pinned, over all 18 presets.
- **Headroom is measured, not judged.** Each preset was rendered 24 dB down with
  the seed pinned, its true peak read off, and its gain set so the peak lands at
  −3 dBFS, capped at −3 dB so quiet presets stay quiet. Nothing clips: the
  library spans −2.8 to −15.3 dBFS peak.

## What is measured

Full numbers in `tools/analysis/README.md`. Fitted against 25 field recordings,
35 minutes, of which **17 proved usable** — measured for the octave-band colour
of the bed with the crackles gated out, for the rate, amplitude law, decay and
spectrum of the crackles, and for the three timescales on which they arrive.

The default patch against the library median:

| | render | library median |
|---|---|---|
| crest factor | 29.8 dB | 31.7 dB |
| spectral centroid | 2531 Hz | 2175 Hz |
| envelope CV, 50 ms | 0.41 | 0.56 |
| envelope CV, 4 ms | 0.56 | 0.85 |
| L/R correlation | 0.61 | 0.69 |
| crackle rate | 31.3 /s | 29 /s |
| crackle prominence | 12.3 dB | 13.2 dB |
| crackle amplitude sd | 4.2 dB | 6.1 dB |
| crackle decay, −10 dB | 3.0 ms | 3.0 ms |
| crackle centroid | 3746 Hz | 3049 Hz |
| interval CV | 1.49 | 1.45 |
| Fano factor, 1 s | 3.55 | 3.90 |
| P(gap < 10 ms) / exponential | 1.82x | 1.97x |
| P(gap < 50 ms) / exponential | 1.01x | 1.03x |
| branching ratio | 0.01 | 0.02 |

The arrival statistics — the three rows the engine was built around — are the
closest of everything here. The two that are furthest out are the envelope CVs,
and `TODO.md` says why.

## Eight of the twenty-five references are not fires

`ambiance_campfire_loop_stereo`, `big-fire-loop`, `sauna-fireplace-loop`,
`fireplace_01`, `fireplace_02`, `fireplace_03`, `fireplace_08` and
`fire-crackle-and-flames` are a steep low-frequency ramp that dies by 400-800 Hz,
above which they carry a flat −45 dB plateau out to 12 kHz — a dither floor, not
content. Their crest factors of 11.8 to 16.5 dB say the same thing: nothing
impulsive is in them.

They are rumble beds with a fire somewhere underneath, and averaging them in
drags every centroid towards a low-pass no listener would call a fire. Seventeen
references carry the whole fit. A larger library, particularly of large open
fires, would narrow every number above.

## Nothing has been validated by ear

Every number here is a measurement, and the suite's rule is that a number
agreeing with a number proves nothing about the sound. This is the first item in
`TODO.md`.
