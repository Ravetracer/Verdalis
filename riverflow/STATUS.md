# RiverFlow status

Version 0.1.0. First working version: the engine, the parameter set, the window
and a first fitted preset library.

## Preset folders and packs (0.3.0)

User presets can be filed in folders. Saving as `Folder/Name` writes into a
folder one level under the user preset directory, creating it if it is not
there; a name with no slash saves into the root, as before. The browser lists
the library by shelf -- *All*, the factory set, each user folder, *Unfiled* --
with a count on each.

A whole folder exports as one **preset pack** (`.riverflowpack`), written to a
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

RiverFlow ships as a VST3 alongside the CLAP, for Linux and Windows, in every
release archive. It is not a port: the clap-wrapper hosts the same
`RiverFlow-impl` static library the `.clap` is built from, so the engine, the
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

- **The bed as a measured filterbank.** Eight octave-wide bandpasses per
  channel, driven at gains *solved* so that their sum is the measured
  octave-band curve rather than each band being set to it. Six colours, shipped
  as the centroids of the six clusters the reference library falls into, with
  `Blend` making them a continuum.
- **Per-band, independent surge.** Each band's level walks on its own, deeper at
  the bottom than the top, from the measured 2.43x at 125-250 Hz falling to
  0.84x at 4-8 kHz and a measured band-to-band envelope correlation of 0.07-0.25.
  It is a filtered random walk, not an LFO, because the references have no
  reproducible rhythm.
- **Frequency-weighted grain**, for the part of the graininess that is too fine
  to count: the measured excess over a Gaussian control grows from 30% at
  200-800 Hz to 220% at 6-14 kHz, so `Grain` does far more to the top of the bed
  than the bottom.
- **Two event populations** — dabbles and drops — sharing one pooled resonator,
  each with its Minnaert pitch, its physical damping after Xue et al., its
  displacement noise and its impact off whatever it struck.
- **A dabble is a cluster**, not a bubble. Measured, and the measurement is
  unambiguous: the event-triggered spectrum says one millisecond of ring and the
  event-triggered envelope says 7-34 ms.
- **The plunge pool** as three collective cloud modes at measured ratios.
- **Distance** as air absorption plus a downward tilt, with the events
  multiplied and their edges smeared — and *bypassed outright* at zero, because
  no distance means no air to absorb.
- **The banks** set the early reflected field: 0.03 for an open river against
  0.90 for a culvert. ShoreBreak's lesson, reused rather than relearned.
- **Preset discovery**, state save/load, sample-accurate parameters, host
  modulation, bounded voice and event pools.
- **The window**, in RiverFlow's own river-green theme, with streamlines
  drifting across the header and a ring spreading wherever a dabble happens,
  deterministic in the dabble number.
- **Self-test**: 0 failures, including a fixed Random Seed rendering identically
  after a reset, every parameter at its extremes staying finite and bounded, and
  preset round-tripping. Output is byte-identical across runs at 48 and 96 kHz
  with the seed pinned.

## What is measured

Full numbers in `tools/analysis/README.md`. Fitted against 77 field recordings,
52 minutes, measured for octave-band colour, envelope statistics **against a
Gaussian white-noise control**, per-band surge depth and independence, and the
rate, pitch, ring time and prominence of the discrete events.

The default patch against the library median:

| | render | library median |
|---|---|---|
| spectral centroid | 2095 Hz | 2146 Hz |
| crest factor | 19.5 dB | 19.6 dB |
| envelope CV, 50 ms | 0.12 | 0.12 |
| envelope CV, 4 ms | 0.23 | 0.26 |
| L/R correlation | 0.57 | 0.60 |
| band cv, 200-800 Hz | 0.39 | 0.39 |
| band cv, 0.8-2 kHz | 0.31 | 0.33 |
| band cv, 2-6 kHz | 0.27 | 0.30 |
| band cv, 6-14 kHz | 0.25 | 0.29 |

The bed reproduces its own shapes to within 0.8-1.6 dB per octave band for four
of the six colours. `fit.py` compares all 20 presets against their references:
15 match within 6 dB in every band.

## Some rivers are white noise, and the plugin says so

The smoothest third of the library is statistically indistinguishable from
shaped Gaussian noise, and the event detector finds too few discrete events in
those recordings to characterise at all. Several presets — `wild_river`,
`distant_river`, `big_falls` — therefore ship with `Grain` at zero and both
event layers off. That is not a preset left unfinished. It is the fit.

## Four corrections the bed needed

Recorded because each was found by measurement rather than by ear, and each is
the kind of mistake that is easy to make twice:

- **The bands' drives have to be independent.** Overlapping bandpasses fed one
  noise sample add coherently, so the solved gains and the render disagreed by
  7 dB in the outermost octave. One noise source per band fixed it, and it also
  matches the measured 0.07-0.25 band-to-band envelope correlation, so the
  faithful arrangement and the tractable one turned out to be the same.
- **Octave-band energy is not density at a band centre.** Matching the centre
  value tilts the whole bed up by 3 dB/octave. The response is integrated across
  each band instead, on a grid fixed at `prepare()`.
- **A skirt filter must be inside the model, not corrected for.** Dividing the
  target by the filter's response asks the bank for exactly the gain the filter
  removes; the two cancel and the steep skirt never appears.
- **The drive's spectral density belongs in the normalisation.** Leaving it out
  is a 30 dB level error with the shape exactly right — which no amount of
  looking at a spectrum will show you.

## The trickle layer is shared with RainyDay

`Pocket`, `TrickleSpec`, `spawnTricklePocket` and `processPocketPool` moved into
`shared/include/verdalis/dsp/pocket.h`, and RainyDay's Trickle is the same code
rather than a second implementation of it. Driven identically the two plugins
render the same event rate, pitch, Q, decay and spectral flatness.

The order in which that function draws its random numbers is part of its
contract, and the header says so: two plugins render identically from one seed
only while it holds.

Verified against the whole preset library: nine of twenty renders differ by
exactly one 16-bit LSB, -90.3 dBFS, and none by more. That is the quantisation
floor, and it comes from consolidating two spellings of ln(1000) -- the engine
carried its own `decayCoefFor` with three digits fewer than `fastmath`'s
`decayCoef`. The duplicate is gone.

## Dabble Size was fitted from the microphone, not the water

The size a preset uses is fitted from the peak of its reference's
event-triggered spectrum, by Minnaert. For `forest_creek` that returned 9.94 mm
-- a 328 Hz pocket, which no recording in the library shows and which does not
sound like water. A listener put it at 3 mm.

Two faults, both real. The event detector's low band started at 150 Hz, and
`lowend.py` had already established that the references carry wind and handling
noise below that -- uncorrelated with the water in every file. Given 150 Hz the
detector locked onto it: the "event" it found had a Q of 1.4 and a spectral
flatness of 0.63, which is not a resonance at all. The band now starts at
400 Hz, well clear of the contamination and well below the measured population's
562 Hz.

And the fitter believed any peak it was handed. It now requires a flatness below
0.55 before treating one as a pitch, and clamps the radius to the 2.0-6.5 mm the
library actually measures rather than the 1.0-12.0 it allowed. `forest_creek`
falls back to the library median of 3.30 mm, which is the listener's 3 mm.

Seventeen of the twenty presets now sit at that median, which is honest: the
measurement only supports a per-preset size where the event really is a
resonance.

## Grain was the noise, twice over

Grain had two faults, and the first fix was necessary but not sufficient.

**It stepped.** A sample-and-hold multiplying a band's amplitude steps it, and a
step is a discontinuity: eight bands in two channels stepping 250 times a second
is four thousand discontinuities a second. It now ramps to each new value across
the hold.

**And it was the wrong model.** Ramped, it still sounded like noise, because it
is a *continuously modulated bed* where the references' graininess is a
population of *discrete events*. Both satisfy the 4 ms variance it was fitted
to: 0.29 at 6-14 kHz against a Gaussian control's 0.09. Neither the variance nor
five other statistics separated them. The kurtosis of the same envelopes does,
and it was sitting in the next column of `grain.py`'s own output the whole time
-- the references measure 38 where Grain produces 7.

The library's smoothest third settles it: those recordings sit *on* the Gaussian
control, so a river's bed is smooth noise and its grain belongs to the events
above it. Grain now ships at zero in all twenty presets and the parameter stays
for anyone who wants it. Bubbling Creek's 6-14 kHz variance drops from 0.51 to
0.29 and Glacier Falls' from 0.34 to 0.18, while their 200-800 Hz character --
the bubbles -- is unchanged.

**Six measurements failed to find this**, and the list is the useful part: a
line-spectrum search at the hold rate; a peak-to-median ratio above 5 kHz; a
layer-by-layer elimination scored with that ratio, which blamed the wrong layer
outright; a spectral-flatness comparison; a per-event pitch-spread comparison;
and a broadband-coincidence count. Every one of them returned "close to the
reference". A listener with a spectrogram found it by looking at the picture and
turning one knob.

## The presets shipped clipping## The presets shipped clipping, and it was the first thing anyone heard

Ten of the twenty factory presets peaked at exactly 1.000, and the four worst
had over a tenth of a per cent of their samples past the soft clipper's knee.
On a noise bed that is audible as crackle, and it read as distorted rain.

It was not subtle once looked for -- `hanging_trickle` would have peaked at
+12.7 dBFS unclipped and `bubbly_falls` at +10.6 -- and nothing in the fitting
loop was watching for it, because every statistic it did watch is a *relative*
one that clipping barely moves. `fitpresets.py` now ends with a trim pass that
renders each preset 20 dB down, measures where its peak would land at unity,
and writes the trim into the preset's own output gain. The layer levels carry
the fit and never move.

Two smaller faults were found in the same pass and are worth recording:

- A struck resonator was started at a random phase, which steps the output by
  the pocket's full amplitude -- a click per pocket, on top of the pinch-off
  transient that is supposed to be the only one. A damped oscillator's impulse
  response starts at zero, so it now does.
- A drop's chirp was spread over the life its *physical damping* implies while
  its decay came from the parameter. For a small high pocket the two differ by
  a factor of forty, which ran the phase increment into its Nyquist clamp and
  held it there -- a near-Nyquist tone read out of an interpolated sine table,
  which is broadband hash rather than a drop.

## Two things the crest factor taught

`Grain` was normalised to unit *mean*, and a spiky positive multiplier with unit
mean still has E[h²] > 1, so full Grain was adding up to 2.1 dB of real band
energy in the bands it weights most. It is normalised in power now: Grain changes
a band's texture and not its level.

Event levels were drawn uniformly, and the renders sat 4 dB below the references'
crest factor with every other statistic correct. A uniform draw has no tail; an
event's energy follows the volume of water in it, which is log-normal. That took
the crest factor from 15.4 to 17.6 — and the last 2 dB turned out to be the
plugin's own soft clipper, since the same render 6 dB down measured 19.5 against
the library's 19.6.

## What does not fit yet

All in `TODO.md` with what is known about them:

1. **The 16 kHz octave.** `Creek` and `Deep Rush` are 5 dB out there and
   nowhere else, and the five presets `fit.py` flags all have references that
   peak in that octave. An octave-wide bank plus one two-pole skirt cannot put a
   15 dB step between its last two bands.
2. ~~**The very peaky references are not reached.**~~ **This was the plugin's own
   soft clipper, not the model.** Ten of the twenty presets were driven past the
   clipper's knee -- `hanging_trickle` would have peaked at +12.7 dBFS and was
   being bent by nearly 19 dB -- and clipping is precisely what takes the peaks
   off a crest factor. Trimmed to a -6 dBFS peak, `bubbly_falls` measures 29.1
   against its reference's 32.5 and `hanging_trickle` 34.2 against 31.1. The
   systematic gap is gone; what remains is scatter either side.
3. **Renders are narrower than the most correlated references.** Where a
   reference measures an L/R correlation of 1.00 the render gives 0.77-0.88,
   because the events decorrelate whatever they are added to even with their pan
   width at zero.
4. **`bubbling_creek` and `rapids_close` reach the fitting loop's correction
   bound.** They cannot be reached inside the bed's four degrees of freedom, and
   the bound is what stops them shipping with a tone control at its end stop.
5. **Nothing has been validated by ear.** Every number above is a measurement,
   and the suite's own rule is that a number agreeing with a number proves
   nothing about the sound. This is the first thing to do next.
