# ChirpParade TODO

## Contours

- **Relaxing the tonality gate further is probably still worth it.** It moved
  from 6 dB to 0 dB, which bought 27 % more usable contours and 48 % more corvid
  candidates for three cents of fit error. At −3 dB there were 1100 usable and
  110 corvid candidates, but the harmonic comb share started falling — so the
  contours keep improving while the partial measurement degrades. They may want
  separate gates.
- **Warbler and Goose render about half the length their archetype median says.**
  Both species now have archetypes of very mixed duration, and stretching a
  short curve to a long `Length` turns its internal amplitude modulation into
  separate notes, which the segmenter then counts separately. Choosing the
  archetype partly by how close its own duration is to `Length` would fix it.
- **`Partials` thins the corvids.** Turning it up makes a crow's timbre evolve
  the way a real one's does — its balance drift goes from 1.7 to 2.7 dB against
  the references' 4.4 — while dropping its harmonic count from 4.3 to 3.0. That
  is not the engine exaggerating: the archetype's spectrum is what that syllable
  actually had, and the gate selected the cleanest ones. The corvid presets
  therefore ship with `Partials` at 35 % rather than 60 %. A wider gate is the
  real fix.
- **The archetype sets are still uneven.** Goose has **three** contours and
  Raven four,
  because only one goose and one raven recording exist in the library and few of
  their syllables passed the quality gate. `Contour` does nothing on a Goose.
  More references for the waterfowl and the corvids would be worth more than any
  further fitting.
- **Nothing morphs between archetypes.** `Contour` steps from one measured curve
  to the next. Interpolating the coefficients would give a continuous control —
  but naively, since two contours that zig-zag out of phase interpolate to a
  smooth glide, which is the exact failure this whole rewrite was about. It
  needs alignment first (dynamic time warping on the curves, then interpolate),
  which is real work.
- **The quality gate throws away 61 % of the library.** 4268 syllables
  segmented, 1641 usable. Most rejections are the dense multi-bird files where
  the tracker is following two birds at once. A better tracker — or a source
  separation pass — would widen every species' set, especially the corvids.
- **Level and pitch are fitted independently.** They are not independent in a
  bird: the pitch tends to peak where the level does. Fitting them jointly, or
  storing their correlation, would let `Variation` perturb them in a way that
  stays plausible instead of drifting apart.
- **Six partials is where the measurement stops, not the bird.** Above the sixth
  the engine has only the valve, so a syllable with real energy at the eighth or
  tenth harmonic gets a synthetic version of it. Storing more is cheap; whether
  the measurement is trustworthy that high is the question.
- **`Detail` is a one-pole.** It smooths the contour as it is read, which is
  cheap and does the right thing perceptually, but it also delays it. A
  zero-phase smoother over the table at spawn would be more honest and costs
  nothing at audio rate.

## Voice

- **The valve saturates around five harmonics.** The noisiest references measure
  twelve. Closing further is not the answer — it aliases. What is probably
  needed is a second oscillator: a bird has two syringeal sides, controlled
  independently, which is how some species produce two notes at once and the
  most likely route to a corvid's density.
- **Roughness cannot be met for the rich species.** Crow measures −13 dB out of
  the engine against −21 in the library, Raven −13 against −30. A harmonic stack
  is spectrally flat whether or not any noise is present, and spectral flatness
  cannot tell the two apart — so this may be a limit of the *statistic* rather
  than of the engine. Deciding that needs a better roughness measure, probably
  a harmonic-to-noise ratio on a resolved partial rather than band flatness.
- **`Rasp` is per-cycle closure jitter only.** Corvid roughness has structure —
  subharmonics and period doubling — that random jitter does not reproduce.
- **The tract is one resonance.** A trachea has a series at odd multiples of
  `c/4L`. The third and fifth would cost two filters and are probably audible on
  the low, harmonic-rich voices.

## Fit

- **Screech does not fit and cannot.** It has no reference of its own, borrows
  the library's most extreme contours, and its measured pitch runs 54 % low
  because those contours swing so far that the fundamental estimate is
  meaningless. It is kept as an effect, not a bird.
- **Spectral flatness is a poor fit target for this engine.** These contours
  sweep at hundreds of octaves a second, and a pure sine doing that smears
  across a 21 ms analysis window and reads as rough. The `rough` tolerance in
  `fit.py` is 6 dB for that reason, and the figure carries the contour's motion
  as well as the breath.
- **No preset was fitted band-by-band against a specific recording.** The fit is
  per quantity rather than spectral, unlike SkyHowl's third-octave comparison.

## Structure

- **`Motif` is a fixed interval per syllable.** Real song has figures, not ramps.
  A short pitch sequence per phrase — even four steps — would be a large gain.
- **Phrase-level contours are not measured.** The syllable contours are; how a
  species' *phrase* moves is not, and it could be extracted the same way.
- **The drum has no body size.** `Knock` sets the first mode and the second is a
  fixed 2.3× above it. Whether a branch is solid or hollow changes that ratio,
  and it is what separates a live tree from a dead one.
- **Distance is inverse-distance plus air absorption, nothing else.** No ground
  reflection, and birds are usually above the listener.

## Rejected, with the reason recorded

- **Mel spectrograms with Griffin-Lim** (SoundPlot). 92.9 ms analysis window at
  librosa's defaults, Griffin-Lim's worst case is exactly this material, and its
  own metrics report a negative SNR. Also the wrong side of the suite's line: a
  mel spectrogram at usable resolution is the recording with the phase removed.
- **pYIN for the pitch track.** Swept over three frame lengths and three
  confidence gates; it discards 80 % of the contour path and cuts the peak slew
  from 424 to 16 oct/s. Its Viterbi smoothing assumes slowly-varying pitch.
  `setup-venv.sh` still installs librosa, because its spectral features may yet
  be useful for choosing archetypes that differ in timbre as well as in shape.

## Verification

- **Listen, properly, against the references.** The numbers are in place; the
  ear is the gate. `!dev/listen-v2/` and `contours.py --wav`.
- **Drive the window's pointer interaction.** Not exercised here — clicks were
  landing on another window on top of the plugin's.
- **Run `clap-validator`**, which needs building in `CLAP/` first.
- **Look for aliasing at 96 kHz and at high `Voice`.** The hinge width is
  derived, not measured.

## Suite

- **The contour extractor belongs in `shared/tools`.** The suite has just widened
  its rule to allow measured tables, and RainyDay's droplet resonances,
  ShoreBreak's breaker envelopes and ThunderClap's N-wave shapes are all
  parametric where a measured curve would be truer. The pipeline here —
  segment, track, fit a cosine series, cluster, keep medoids, emit a header — is
  general.
- **The one-sided valve belongs in `shared/dsp`.** Any plugin modelling a valve
  needs it, and the odd-harmonic trap it avoids is easy to fall into twice.
- **The phrase scheduler with per-individual identities is reusable.** RainyDay,
  ShoreBreak and CrackleBlaze spawn events at a rate; this is the first that
  models individuals, and NightLife will want exactly that.
- **`src/plugin.cpp` is now duplicated five ways.** ~1140 lines of which ~128
  differ, and the differences are the engine type and `syncEngineParams`.
