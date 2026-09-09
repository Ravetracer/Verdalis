# ChirpParade TODO

## Fit

- **Roughness and harmonic count cannot both be met for three species.** Goose,
  Raven and Budgie measure spectrally peaked *and* harmonically rich in the
  library, and a relaxation waveform is flat whether or not noise is added.
  Goose is 10 dB out, Raven 9, Budgie 6. What might close it: the roughness the
  library measures for these three may be the *envelope* of a multi-mode source
  rather than noise, in which case a second labium — the syrinx has two sides,
  independently controlled — would give the harmonic density without the
  flatness. That is a real feature, not a fudge; see *Model* below.
- **Sweep is off by more than 10 % for the three species whose contours turn
  more than once** (Woodpecker −33 %, Crow +37 %, Raven −53 %). The audible
  window and the turning points interact, and the excursion is scaled over that
  window as one number. Scaling it per turning point would be closer.
- **Syllable duration measures 6–33 % above `Length`.** The measurement includes
  the ramps and the tail; the setting is the gesture. Defensible, but it means a
  preset aiming at a measured duration has to set `Length` below it, which is a
  trap. Worth deciding whether `Length` should mean the audible duration
  instead — the same decision that was already taken for `Sweep`.
- **Screech's measured pitch runs 38 % high** because the anti-alias clamp on
  the drive bites and the relaxation-pull compensation then overshoots. Only
  affects the one species with no reference behind it.
- **The library is not evenly sampled.** 25 of 58 files are ordinary small birds;
  Goose and Raven have one file each, 12 and 6 syllables. Those two rows of the
  species table are the weakest numbers in it, and the Raven harmonic override
  is the direct consequence. More references for the corvids and the waterfowl
  would be worth more than any amount of further fitting.
- **No preset was fitted band-by-band against a specific recording.** The fit
  is per-quantity (pitch, sweep, length, harmonics, roughness) rather than
  spectral, unlike SkyHowl's third-octave comparison. A third-octave fit would
  catch tract errors the current one cannot see.

## Model

- **The second syringeal side.** A bird has two, controlled independently, which
  is how some species produce two notes at once — and it is the most likely
  explanation for the roughness/harmonics conflict above. One more oscillator
  per syllable with its own gestures and a coupling term. The expensive part is
  not the DSP, it is the parameter surface: it needs a *Bilateral* amount and a
  detune at minimum, and probably its own contour phase.
- **The pressure gesture is a raised cosine.** Zysman et al. recover real
  pressure gestures from recordings and they are not cosines — they have
  plateaus. A two-segment gesture with a hold would be closer, and would make
  long syllables less obviously synthetic.
- **Source–tract coupling is one number.** `Rasp` feeds a scaled tract output
  back into the labia, which is the documented route to period doubling, but the
  real coupling is a delayed, frequency-dependent back-pressure from a tube of a
  known length. The tract length is already a parameter; the delay could come
  from it rather than being a gain.
- **The tract is one resonance.** A trachea has a series of them at odd
  multiples of `c/4L`. Adding the third and fifth would cost two more filters
  and is probably audible on the low, harmonic-rich voices where the second
  formant would fall inside the stack.
- **`Motif` is a fixed interval per syllable.** Real song has figures, not
  ramps. A short pitch sequence per phrase — even four steps — would be a large
  gain for a small parameter cost.
- **The drum has no body size.** `Knock` sets the first mode and the second is a
  fixed 2.3× above it. A real branch's mode ratio depends on whether it is
  solid or hollow, which is what separates a live tree from a dead one — and
  *Hollow Tree* currently fakes it with `Ring`.
- **Distance is inverse-distance plus air absorption, and nothing else.** No
  ground reflection, and birds are usually above the listener. A single delayed
  reflection with a height parameter would be cheap and is a real cue.

## Interface

- **Species does not reset anything.** Choosing Crow biases the syllable
  controls but leaves `Voice`, `Breath` and `Rasp` where they were, so a preset
  built for one species can sound wrong under another. That is deliberate — a
  parameter that jumps when a neighbouring one is changed is worse — but it does
  mean the species names promise more than they deliver on their own.
- **`Flock Rate` is in syllables a minute and divided by `Syllables`** to get a
  phrase rate, so changing `Syllables` changes how often phrases start while
  keeping the syllable density. That is the right invariant and it is not
  obvious from the label.

## Verification

- **Drive the window's pointer interaction.** It was not exercised here: clicks
  were landing on another window on top of the plugin's. Knob drags, chip
  dropdowns and typed value entry are shared code verified in the other four
  plugins, but they have not been driven against this window's own layout.
- **Run `clap-validator`**, which needs building in `CLAP/` first.
- **Measure the CPU cost.** The oscillator takes up to 8 substeps per sample for
  a hard-driven low voice, and nothing has profiled 24 of those at once against
  the `Max Voices` ceiling.
- **Render the presets at 96 kHz and listen for aliasing.** The anti-alias clamp
  on the drive is derived rather than measured; the harmonic content that
  actually reaches Nyquist has not been looked at.

## Suite

- **`src/plugin.cpp` is now duplicated five ways.** ~1140 lines of which ~128
  differ, and the differences are the engine type and the parameter mapping.
  Five copies is where the suite note said the case would become clear: it wants
  a template on the engine, with `syncEngineParams` staying per-plugin.
- **The Poisson scheduler and the individuals-in-a-flock model are reusable.**
  RainyDay spawns droplets, ShoreBreak spawns bubbles and CrackleBlaze will
  spawn sparks; ChirpParade's phrase scheduler with its per-bird identities and
  its answer behaviour is the first one that models *individuals* rather than a
  rate, and NightLife will want exactly that.
- **The one-sided flow source belongs in `shared/dsp`.** Any plugin that models
  a valve — a syrinx, a glottis, a reed — needs it, and the odd-harmonic trap it
  fixes is easy to fall into twice.
