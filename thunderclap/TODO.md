# ThunderClap — open items

In rough order of what would matter most to somebody using it.

## Listening

- [ ] **Listen to the library in a DAW and correct by ear.** Every constant
  was set against measurements; nobody has heard the result on speakers yet.
  The things most likely to want moving: the crackle level (`kCrackleLevel`)
  and the scale it is drawn on (`kRoughnessM` / `kRoughnessMinM`), the
  steepening threshold (`kSteepenThreshold`), the N-wave base length
  (`kNwaveBaseSec`), the echo loop gain, and the balance of rumble against
  shocks.
- [ ] **`Impact` is now on in every preset**, set from the preset's Distance.
  The mapping is a judgement, not a fit: a real one would come from the crest
  factor and the 200 ms density of a recording at a known range, which
  `measure.py impact` computes but which no recording here carries a range for.
- [ ] Check the close presets for soft-clipper colouration. Overhead Crack
  and Storm Front peak within a decibel of full scale on some seeds and the
  clipper above 0.8 is doing real work there; if it sounds squashed, take
  their gains down a couple of dB.
- [ ] The late in-cloud swell in the user's own recording (432104) is louder
  than the crack; City Thunder's is about 10 dB under it even with the cloud
  radiating half again as hard (`kCloudGain`). Either push that constant, or
  make it a parameter ("Sky") if the window can find a cell for it.

## Fitting

- [ ] **A numerical fit of presets against individual recordings**, as
  RainyDay has. `tools/fithost.cpp` renders presets back to back for exactly
  this; the missing piece is the objective and the coordinate descent, which
  RainyDay's `tools/analysis/fit.py` could be adapted from. The features that
  matter for thunder: band energies, envelope shape (time to peak, decay rate),
  clap count and spacing, 20 ms fluctuation depth.
- [ ] The far family of reference recordings (distant-04 to 09) sits 5 to 10
  dB brighter at 320 Hz than the engine's 10 km default. Either their storms
  were nearer than ten kilometres or the absorption exponent is a touch high
  at low frequencies. Unresolved; the Air knob covers it.

## Engine

- [ ] The compressor is a peak follower with a fixed 6 dB knee. An RMS
  detector option and a sidechain highpass would stop the sub-20 Hz of the
  cloud waves driving the gain.
- [ ] **Ground reflection.** Each shock arrives twice, direct and off the
  ground, a few milliseconds apart for elevated elements. It is a comb filter
  on the near channel and is not modelled. It is also the cheapest remaining
  way to make the onset denser -- it doubles the arrival count outright -- and
  after the 1.2.0 work density is the measure the engine is still furthest
  from at the sparse end. Cost: either half the element budget or a bigger
  pool, since the arrival count doubles.
- [ ] **The onset does not build.** The two hardest reference recordings ramp
  over 10 to 20 ms and reach their peak 40 to 50 ms after the onset; every
  render peaks in its first 5 ms. Some of that is the recorder's gain riding,
  but not all -- a caustic takes time to assemble, and the nearest element
  being the loudest is an artefact of the 1/r weighting meeting a channel that
  starts at the listener's feet.
- [ ] **Wind and temperature gradient** bend the rays and shift the shadow; the
  shadow is a fixed function of distance and Swell. A wind parameter would move
  it and skew the stereo image.
- [ ] Parameters are read at the moment a flash fires. Automating Distance
  during a long rumble does nothing to it. Per-flash is physically right, but a
  live Rumble Tone or Crack would be fun.
- [ ] `Max Shocks` is per flash and the pool is a fixed 4096: three overlapping
  4096-element flashes in Storm mode can drop shocks. Inaudible so far; a
  larger pool costs 64 bytes a shock.

## Window

- [ ] The activity meter's scale is inherited from RainyDay (compressed on the
  ratio to the pool). With a pool of 4096 and typical concurrency of tens, it
  reads low; a log scale on the absolute count would show more.
- [ ] No visual for which mode is active beyond the selector. A storm voice
  could pulse the panel title.

## Platforms

- [ ] The Windows build (`build-win/ThunderClap.clap`) cross-compiles with the
  window but has not been run on Windows. Cairo has to be cross-built first;
  `cmake/build-windows-cairo.sh` does it but needs `meson` from a venv on this
  machine.
- [ ] `clap-validator` has not been run (version 0.4.1 needs rustc ≥ 1.95).

## Housekeeping

- [ ] `tools/fithost.cpp` is carried over from RainyDay and builds, but nothing
  uses it until the fit exists.
