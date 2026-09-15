# ThunderClap — open items

In rough order of what would matter most to somebody using it.

## The VST3

ThunderClap builds and ships a VST3 for both platforms. What is left is suite-wide
rather than this plugin's:

- [ ] **Pin the VST3 class id.** The wrapper hashes the TUID from the CLAP id.
      It is stable, but it is now released, so it can never change -- set it
      explicitly with `CLAP_VST3_TUID_STRING` before anything touches plugin ids.
- [ ] **Factory presets do not reach a VST3 host's own browser.** The plugin's
      browser lists them all; the host's does not. clap-wrapper's `next` branch
      has *preset discovery: let wrapped formats browse a CLAP's presets*.
      Revisit when that releases.
- [ ] **The VST3 3.8 build break is not reported upstream.** clap-wrapper 0.16.0
      does not compile against VST 3.8, which renamed the `DEFINE_INTERFACES`
      macro parameter to `_iid` while adding `FObject::iid`. Six lines, carried
      in `shared/patches/clap-wrapper-vst3-sdk-3.8.patch`.


## Listening

- [ ] **Listen to the library in a DAW and correct by ear.** Every constant
  was set against measurements; nobody has heard the result on speakers yet.
  The things most likely to want moving: the crackle level (`kCrackleLevel`)
  and the scale it is drawn on (`kRoughnessM` / `kRoughnessMinM`), the
  steepening threshold (`kSteepenThreshold`), the N-wave base length
  (`kNwaveBaseSec`), the echo loop gain, and the balance of rumble against
  shocks.
- [ ] **`Impact` and `Bloom` are now on in every preset**, set from the preset's
  Distance.
  The mapping is a judgement, not a fit: a real one would come from the crest
  factor and the 200 ms density of a recording at a known range, which
  `measure.py impact` computes but which no recording here carries a range for.
- [ ] **The rumble does not bloom.** It is fed by the arriving shock energy with
  an instant rise, so it puts its band in at full level from the first
  millisecond while the shock sum is still highpassed. The coherence argument
  applies to it too -- it is the incoherent sum, which also has to assemble --
  and it is the main thing still filling the bottom during a clap's first
  20 ms.
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
- [ ] The far family of the *old* reference recordings (distant-04 to 09) sat 5
  to 10 dB brighter at 320 Hz than the engine's 10 km default. Either their
  storms were nearer than ten kilometres or the absorption exponent is a touch
  high at low frequencies. Unresolved; the Air knob covers it. The new library
  says the same thing more loudly -- see the next item.
- [ ] **`distant_rumble` and `far_horizon` are darker than any real recording
  in the library.** Measured with `contours.py decay`: no reference, at any
  distance, puts its 1-2 kHz octave further than 42 dB under its loudest one or
  holds a spectral centroid under 128 Hz. Both presets do both -- tilt -57 and
  -58 dB, centroid 44 to 87 Hz against a measured 170 to 215. `close_strike` is
  the same error smaller: 300 Hz over its first 40 ms against 649 measured.
  Either their `Distance` is set beyond anything in the library or the
  absorption is too steep at the far end; the measurement cannot separate the
  two, and it is an ear question which is wrong. The full table is in
  `tools/analysis/README.md`, *What the contours say*.
- [ ] **The close presets fire four to seven times too many discrete arrivals.**
  `contours.py density` counts 66 in the first 8 s of `close_strike` and 116 in
  `overhead_crack`, against 16 in real close thunder, at median gaps of 62 and
  46 ms against 273. `Max Shocks` 2048 -> 4096 with `Focus` 0.7 -> 0.2 lands
  `close_strike` on 18 at 264 ms with its band tilt unchanged; `overhead_crack`
  also needs its tortuosity down, which changes what the preset is. A/B renders
  are in `!dev/audition-density/`. **Not applied: it is an ear decision**, and
  Max Shocks at 4096 doubles the per-flash element count, so the CPU cost wants
  checking before it goes into a factory preset.
- [x] **Thunder arrivals are Poisson, and the plugin need not care.** Fano
  factor 1.0 within the scatter at every window from 0.25 to 2 s, against
  CrackleBlaze's 3.90 at 1 s. No clustered spawner is wanted here; the engine's
  arrival times come out of channel geometry, which is more specific than a
  process, and the measurement says nothing is missing.
- [x] **Measured contours instead of parametric shapes -- tried, and it does not
  transfer.** The suite `CLAUDE.md` lists ThunderClap's N-wave shapes as
  parametric where a measured curve would be truer, on the strength of what
  measured pitch contours did for ChirpParade. `contours.py` was written to get
  those curves and they are not there to get: the shock front averages to a
  coherence of 0.44 across the library and the attack envelope has a 10 to 30 dB
  spread at every step, because a clap is the caustic of thousands of arrivals
  rather than one repeatable gesture. What the exercise did produce is the
  centroid measurement above, which says the engine's *structure* -- per-arrival
  absorption rather than one swept filter -- is right, and that its far presets
  are not. Worth knowing before anyone builds a DDSP fitting rig: it would be
  fitting the sum, not the mechanism.

## Engine

- [ ] The compressor is a peak follower with a fixed 6 dB knee. An RMS
  detector option and a sidechain highpass would stop the sub-20 Hz of the
  cloud waves driving the gain.
- [x] **Ground reflection.** Done in 1.4.0 as `Ground`; see `STATUS.md`.
- [ ] **The ground is flat, hard and infinite.** Its reflection coefficient is
  one number rather than a frequency-dependent impedance, and it does not fall
  with grazing angle. Real porous ground over-reflects at the bottom and
  scatters the top incoherently, which is roughly what the constant plus a
  lowpass gives, but the ground effect's dip is measurably shallower and sits
  a little higher than a rigid plane predicts.
- [ ] **The onset's level still does not build, only its spectrum does.**
  `Bloom` (1.3.0) gets the spectral half right -- the centroid falls from about
  300 Hz to 120 over 20 ms, against the recordings' 225-430 down to 76-100 --
  but the recordings also ramp their total level by 11 to 16 dB over the same
  window and the renders ramp 2 to 5. Some of that is the recorder's gain
  riding, but not all: a caustic takes time to assemble, and the nearest
  element being the loudest is an artefact of the 1/r weighting meeting a
  channel that starts at the listener's feet.
- [ ] **Wind and temperature gradient** bend the rays and shift the shadow; the
  shadow is a fixed function of distance and Swell. A wind parameter would move
  it and skew the stereo image.
- [ ] Parameters are read at the moment a flash fires. Automating Distance
  during a long rumble does nothing to it. Per-flash is physically right, but a
  live Rumble Tone or Crack would be fun.
- [ ] `Max Shocks` is per flash and the pool is now twice the element ceiling
  (8192) because `Ground` doubles the arrivals. Three overlapping
  4096-element flashes in Storm mode can still drop shocks; a larger pool
  costs 64 bytes a shock.

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
