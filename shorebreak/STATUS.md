# ShoreBreak status

Version 0.1.0. First working version: the engine, the parameter set, the window
and a first fitted preset library.

## What works

- **The four-layer wave model** — break, foam, wash, bubbles — with the swell
  bed under it. All 49 parameters are wired and audible.
- **Breaker types.** Galvin's spilling / plunging / collapsing / surging, each
  with its own spectral slope above 1.5 kHz, attack, level, foam share and body
  weight. The slope is animated: steepest while the crest collapses, relaxing
  afterwards, as measured.
- **Size-dependent decay.** Bigger breakers decay more slowly, after the
  −7 dB/s at 1.8 m against −4.5 dB/s at 2.6 m in the literature.
- **The precursor.** A crest bubbles before it collapses; the parameter is how
  much of that is audible.
- **Distance.** Air absorption plus a downward tilt. This is the best-fitting
  part of the model: Distant Roar tracks its reference within about a decibel
  from 800 Hz up.
- **Preset discovery**, state save/load, sample-accurate parameters, host
  modulation, bounded voice/wave/bubble pools.
- **The window**, in ShoreBreak's own sea-green theme with a surf-line ornament
  in the header. Typed value entry, preset browser, wave-activity meter.
- **Self-test**: 0 failures, including a fixed Random Seed rendering identically
  after a reset, all parameters at their extremes staying finite and bounded,
  and preset round-tripping.

## What is measured

Full numbers in `tools/analysis/README.md`. Fitted against 55 field recordings
(48 kHz stereo, 114 minutes), of which the ones containing birds or aircraft
were excluded — this plugin models waves only.

Spectral shape is a good fit across the library. Envelope character is right for
shore breaks (Sand and Foam measures an envelope variation of 1.82 against the
reference's 1.86; Rhythmic Tide 1.16 against 1.24).

## A wave comes in deep and brightens as it breaks

The band opens upward into the break instead of sweeping down out of it, which
is what it does in the references: the HF-to-LF balance rises by 1 to 7 dB at
the break in most of the library. `Crest Open` is how much darker the approach
is. The precursor is the low sound of water arriving, not bubbles.

## Bubbles are a detail

They are quiet, they sit behind the break rather than on it, and several presets
have none at all -- distant surf has no separately audible bubbles. The slowed
references put 9 onsets a second in the break against 29 in the foam that
follows, so the cascade belongs in the foam, which arrives a few hundred
milliseconds later.

## Bubbles are oscillators, not filtered noise

A bubble is generated as a decaying sinusoid with a short pinch-off transient,
which is the impulse response of the damped harmonic oscillator Xue et al.
model. Its damping is computed from the physics rather than set by hand:
radiative loss is a constant 0.01368 for every size and the thermal term goes as
sqrt(f), giving Q from 20 to 46 and ring times of 2 to 124 ms across the size
range -- against 5-100 ms measured. `Bubble Damping` multiplies that, 1 being
physical.

The foam runs a **second, finer cascade**: the slowed references measure the
foam fizzle at 2.2 kHz and 29 onsets a second against the break's 850 Hz and 9,
so foam bubbles are smaller, higher and faster than the ones a break makes.
`Foam Bubbles` sets how much of the foam is that cascade rather than hiss.

`Break Body` now drives a resonator at the cloud's collective mode,
`bubblePitch / cbrt(N)` with N from wave size, which is where surf rumble
physically comes from.

## The break is a cascade, not a sweep

The first version built a break by sweeping a bandpass over noise, which sounds
like a slowed-down whip crack rather than a wave. The references say a break is
a cascade of discrete bubble events -- 15-30 separately audible onsets a second,
clustered around 1 kHz -- so the engine now builds the break from bubbles and
uses the noise band only as what surrounds them. `Bubble Mix` sets the balance.

The rate matters more than anything: at the references' 27 onsets a second with
a 40 ms ring, about one bubble sounds at a time. Push the rate up and they
overlap into noise, which is how the first version ended up with a whoosh
despite having a bubble layer all along.

Measured graininess is now inside the reference range; see
`tools/analysis/README.md`.

## What does not fit yet

Both are in `TODO.md` with what is known about them:

1. **Crest factor is 3–9 dB below the references.** Real surf has more silence
   between breaks than the synthesis leaves.
2. **The steadiest sources are too eventful.** Distant Roar measures an
   envelope variation of 0.53 against the reference's 0.10; the individual
   waves still punch through what should be a continuous roar.
3. **Rhythmic Tide is darker than its reference throughout** -- it brightens at
   the break in the right direction and by a plausible amount, but its reference
   sits about 3 dB brighter overall in every phase.
4. **Uproar Waves brightens at the break where its reference darkens slightly.**
   Every other preset measured moves the right way.
5. **Previously: several presets were grainier than their references.** Sand and Foam
   matches almost exactly (0.99 against 1.08), but Big Waves measures 1.18
   against 0.76 and Rhythmic Tide 1.19 against a comparable figure. Erring
   toward more bubbling was deliberate after the first version was too smooth,
   and Bubble Mix dials it back, but the presets should be settled by ear.

## Known bugs found and fixed in this version

Recorded because each was found by measurement or by looking at the window, not
by listening, and the same class of mistake is easy to repeat:

- The foam and wash envelopes rose and decayed *simultaneously*, so they settled
  at 0.89 and never decayed at all. The foam became a permanent bed rather than a
  tail, which flattened the envelope and made everything far too bright.
- `Bubble Rate` was a Log parameter with a lower display bound of zero, and
  `dispMin * (dispMax/dispMin)^raw` is NaN. The window showed `-nan /s`.
- Q values were passed to `Svf::setCutoff`, whose resonance argument is 0..1.
  Anything above 1 silently clamped to maximum resonance, so the wash band was a
  maximum-Q bandpass on continuous noise -- a whistle, loudest in Receding Sand
  because it has the most Sand and the loudest wash. Resonance is now converted
  from a Q properly, and the wash is barely resonant on purpose.
- The break's slope filter was applied unconditionally rather than mixed in, so
  the minimum was 12 dB/octave against the 6 the bandpass already gave. The
  break lost its top end twice over and measured darker than the wave before it,
  which is the opposite of what a wave does.
- `Bubble Rate` was allowed up to 6000/s on the reasoning that only some
  bubbles are separately audible. At 1400/s and a 38 ms ring that is 53 bubbles
  overlapping, which sums back into noise -- the cascade has to be sparse to be
  a cascade. The range is now 2-600/s and the presets sit at 12-60.
- `noteOn` drew its initial swell phase and first wave timer from the shared RNG.
  Note events can arrive before the Seed parameter has been applied, so a fixed
  Seed did not promise the same sea. Both are now derived from the voice's slot
  and key.
