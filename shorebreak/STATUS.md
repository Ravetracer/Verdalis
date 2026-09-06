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

## A wave in three stages

The shore breaks, some bubbles are heard, and after a while the foam starts
bursting as a sizzle. The engine now does that in order:

1. **The break**, deep coming in and opening up as it collapses.
2. **Bubbles**, a trickle at the break's tail and more in the foam that follows,
   individually audible at around a kilohertz.
3. **The sizzle**, arriving later again and outlasting everything: thousands of
   sub-millimetre bubbles bursting at once. At 0.25-0.6 mm they ring between 5
   and 13 kHz for a couple of milliseconds, and at that rate they overlap
   several deep, so they are not separately audible -- they merge. It is
   generated as a high band with a granular envelope rather than as thousands of
   oscillators, which is the same sum for one multiply a sample.

## A wave washes the last one's foam away

Each wave used to own a private foam timer that ignored the sea around it, so
with a long Foam Delay a wave's foam could still be hanging in the air when the
next one broke -- the one thing a beach never does. A breaking wave now runs
over whatever foam is already lying there: what has not sizzled yet is burst by
the arriving water within a few tens of milliseconds rather than waiting out its
delay, and what is already sizzling is carried back out and fades early.

Measured with Foam Delay at 2.5 s and waves every 1.2 s, so that every wave's
foam is still pending when the next breaks: the sizzle fires 0.06 s after each
break instead of at its 2.5 s delay.

## An open shore has no walls

Eight discrete early reflections at fixed fractions of a room dimension is what
makes a reverb sound like a bathroom, and no amount of tail will talk the ear
out of it. Outdoors there is nothing close enough to reflect. `Space::setEnclosure`
scales the early field, and the shore type sets it: 0.04 for open sand, 0.95 for
a harbour wall with something right there to bounce off. Space amount and size
went up across the library at the same time.

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

## Fitted numerically

`tools/analysis/fit.py` compares all 17 presets against their references.
12 have a single audible band 6-11 dB out, down from 15 with deviations up to
37 dB. The low end is set by the collective cloud mode rather than the swell,
which is what `Break Body` scales.

## What does not fit yet

Both are in `TODO.md` with what is known about them:

1. **Crest factor is 3–9 dB below the references.** Real surf has more silence
   between breaks than the synthesis leaves.
2. **The steadiest sources are still somewhat eventful**, though much less so:
   Distant Roar measures an envelope variation of 0.32 against the reference's
   0.10, having been 0.53 before distance began multiplying the events.
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
- The bubble chirp was applied as a per-sample factor when it was meant as a
  total rise over the bubble's life. At 1.006 per sample it compounded 300-fold
  within a thousand samples, sent the phase increment to infinity and read off
  the end of the sine table. The self-test caught it as a crash.
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
