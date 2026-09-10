# RiverFlow TODO

## Listen to it

- [x] ~~Listen for the octave bank ringing, and for Grain reading as a pitch.~~
      Neither, but Grain *was* audibly wrong: it stepped each band's amplitude
      every 4 ms and a step is a discontinuity. Fixed by ramping. Three
      measurements missed it and the user found it by turning the knob down;
      see STATUS.md for which measurements failed and why, and note that the
      one that works -- Grain must not move the spectrum -- is now in the
      recipe.
- [ ] **Validate by ear, then by measurement.** Nothing here has been listened
      to. The suite has already been burned by this once — ChirpParade 0.1.0 was
      11,000 lines validated entirely against statistics and it was wrong — and
      the rule that came out of it is that measurement catches what the ear
      cannot quantify but does not replace it. Render A/B pairs against the
      references and listen before calling any of the fit good.
- [ ] In particular, listen for the octave bank ringing. The rendered spectra
      are smooth at third-octave resolution and show no comb at octave spacing,
      but eight bandpasses an octave apart is exactly the arrangement that
      would, and a measurement at that resolution could miss a narrow one.
- [ ] The other half of the impulsiveness fit is now the opposite problem. The
      library's median event kurtosis is 30 and the presets' is 1.7: the
      references have sparse strong events that the 4 ms band statistic cannot
      see, so the fitter turns the event layers off for them. `winter_river`'s
      reference measures a kurtosis of 157 against the preset's 2.0. Fitting
      the event levels from the impulsiveness as well as from the band variance
      would close it -- `crackle.py` is the measurement, it just is not wired
      into `fitpresets.py` yet.

## Fit

- [ ] **The 16 kHz octave.** `Creek` and `Deep Rush` come out 5 dB high there
      and within 2 dB everywhere else, and all five presets `fit.py` flags have
      references that peak in that band. The cause is understood: an octave-wide
      bandpass leaks 6-8 dB into its neighbour, so a 15 dB step between the last
      two bands is out of reach, and one two-pole skirt at the band edge does not
      close the gap. Options, in order of appeal: a second skirt pole; a
      half-octave pair of bands at the top only; a search over the skirt corner
      rather than the closed form (the model in `updateFilters` is now accurate
      enough to search against, but it would run in the parameter path).
- [x] ~~Raise the crest factor on the peaky presets by 10 dB.~~ It was the
      plugin's own soft clipper: ten presets shipped saturating, and clipping
      takes the peaks off a crest factor by definition. Trimmed, `bubbly_falls`
      measures 29.1 against its reference's 32.5 and `hanging_trickle` 34.2
      against 31.1. What is left is scatter either side rather than a
      systematic gap, and `forest_creek` (26.3 against 30.1) and `river_rush`
      (14.5 against 19.2) are the two worth a second look.
- [ ] **The most correlated references cannot be reached.** Four measure an L/R
      correlation of 1.00 — one recording of one place — and the renders give
      0.77-0.88 even with the event pan width at zero, because a pooled event
      still lands at one pan and the bed's own decorrelation is separate. A
      genuinely mono path for the events, or panning a whole cluster from the
      bed's correlation, would close it.
- [ ] `bubbling_creek` and `rapids_close` sit at the fitting loop's ±0.35
      correction bound, meaning the bed's four degrees of freedom cannot reach
      them. Both are close, loud water with a lot of low-band grain; the shape
      match may be picking the wrong cluster for them.
- [ ] `bubbly_falls` and `gorge_cascade` have references whose spectral centroid
      is 8-12 kHz, and the renders reach 3.5-4.7 kHz. The `Trickle` shape peaks
      at 16 kHz but the bank cannot get it far enough above its neighbour --
      the same 16 kHz problem, showing up as a centroid instead of a band.

## Model

- [ ] **Measured contours, as the suite's widened rule allows.** Everything in
      RiverFlow is currently parametric. Two things would be truer as measured
      curves: the shape of one dabble's *envelope* (a cluster of pockets is
      modelled as a uniform scatter over the spill time, where the
      event-triggered envelopes have a shape), and the size distribution within
      a cluster (a fixed 0.22-octave jitter stands in for whatever the real
      distribution of trapped pockets is).
- [ ] **The dabble cluster's spill is uniform.** Real water folding over a stone
      probably traps its pockets front-loaded rather than evenly. The
      event-triggered envelopes in `events.py` would answer it.
- [ ] `Trickle Decay` is a single control over a measured *bimodal* population —
      8-13 ms for a tick against 56-86 ms where the water is deeper. `Splash`
      approximates the second mode as a tail on the first. Two populations with
      their own rates would be closer to what was measured.
- [ ] Turbulence does not change the bed's colour, only its level per band. More
      energetic turbulence entrains smaller bubbles, so there is a physical
      reason to expect a spectral tilt with the surge -- but `bandsurge.py`
      measures the depth *falling* with frequency, not rising, so if there is
      such a tilt it is small and this should stay measured before it is added.
- [ ] The plunge pool's tone, depth and Q are chosen rather than measured, and
      so are the space controls and `Banks`. Separating a plunge pool's
      resonance from the bed's own low end needs references that isolate one;
      the space controls need a reverberation measurement the library cannot
      give, having no impulse and no known source position.
- [ ] Shore/bank traits are six tables of four factors chosen by ear. They
      should be fitted against references that actually name their surroundings.

## Suite

- [ ] **The bubble resonator is now built three times** — RainyDay's droplets,
      ShoreBreak's foam bubbles and RiverFlow's pockets — and all three are the
      same thing: a decaying sinusoid at the Minnaert pitch for a radius, with a
      pinch-off transient and a band of displacement noise. `CLAUDE.md` named
      this as the obvious next extraction before RiverFlow existed, and RiverFlow
      confirms it. Take `Pocket` into `shared/include/verdalis/dsp/bubble.h`
      along with `minnaertHz` and `bubbleDelta`.
- [ ] **The octave filterbank and its solver are worth sharing too.** A bank
      solved so its sum matches a measured curve is not river-specific: SkyHowl's
      wind and CrackleBlaze's fire beds are both broadband and both have measured
      colours. The four corrections it took to get right are recorded in
      `tools/analysis/README.md` and should not have to be rediscovered.
- [ ] `plugin.cpp` is duplicated six ways now (~1140 lines, ~128 differing).
      Nothing new has been learned about it: the CLAP lifecycle is identical and
      only the engine type and `syncEngineParams` differ. Extract it, templated
      on the engine.
- [ ] `tools/render.cpp` and `tools/guihost.cpp` are six copies as well.
