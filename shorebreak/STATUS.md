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
3. **The foam bed is smooth where the references' is granular.** Sand and Foam
   measures a grain CV of 0.63 against 1.08. The foam is broadband noise; in the
   references it is itself made of bubbles. It wants its own sparse cascade.

## Known bugs found and fixed in this version

Recorded because each was found by measurement or by looking at the window, not
by listening, and the same class of mistake is easy to repeat:

- The foam and wash envelopes rose and decayed *simultaneously*, so they settled
  at 0.89 and never decayed at all. The foam became a permanent bed rather than a
  tail, which flattened the envelope and made everything far too bright.
- `Bubble Rate` was a Log parameter with a lower display bound of zero, and
  `dispMin * (dispMax/dispMin)^raw` is NaN. The window showed `-nan /s`.
- `Bubble Rate` was allowed up to 6000/s on the reasoning that only some
  bubbles are separately audible. At 1400/s and a 38 ms ring that is 53 bubbles
  overlapping, which sums back into noise -- the cascade has to be sparse to be
  a cascade. The range is now 2-600/s and the presets sit at 12-60.
- `noteOn` drew its initial swell phase and first wave timer from the shared RNG.
  Note events can arrive before the Seed parameter has been applied, so a fixed
  Seed did not promise the same sea. Both are now derived from the voice's slot
  and key.
