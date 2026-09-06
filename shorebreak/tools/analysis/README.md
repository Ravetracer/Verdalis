# ShoreBreak analysis

Every default and every factory preset in ShoreBreak comes from measurements of
a reference library of 55 field recordings of surf: 48 kHz stereo, 114 minutes
in total. The recordings themselves are not part of this repository and are not
ours to redistribute; they live outside it and are read, never copied.

The scripts here read a directory of references and print the numbers below.
`wavio.py` comes from `shared/tools/analysis`.

## What was measured, and what it decided

| Quantity | Measured across the library | What it set |
|---|---|---|
| Event rate | 9–18 breaks/min, median gap 2.6–4.2 s | `Wave Period` range and default |
| Gap scatter (IQR) | 0.3 s (metronomic) … 3.9 s (sets) | `Set Variation` |
| Break attack | 0.25–1.19 s | `Break Attack`; a wave rises, it does not strike |
| Break decay | 0.30–0.82 s to −10 dB | `Break Decay` |
| Foam vs break decay | 0.6× … **2.7×** | `Foam Decay`; foam outlives the crest |
| HF left when the mid band is −12 dB | −6.7 … −19.4 dB | why the gaps measure *brighter* than the breaks |
| Spectral peak | 400–1600 Hz | `Break Tone` |
| Foam low end | **−82 dB at 50 Hz** | the foam layer is highpassed hard and has no body |
| Crest factor | 14.5 dB (continuous roar) … 41.8 dB (isolated laps) | the `Swell` / `Surf` balance |
| Envelope variation | 0.10 (steady) … 1.86 (peaky) | `Swell Depth`, `Wave Size` |
| L/R correlation | 0.26–0.50 open coast, 0.71 in a harbour | `Width`, `Swell Width` |
| Distant HF rolloff | −50 dB at 12.5 kHz | `Distance` / `Air` |

## The cascade: what a break is actually made of

The first version of the engine built a break from a bandpass sweeping over
noise. It sounded like a slowed-down whip crack, because that is what it was: a
smooth spectral sweep. A break on a beach is not one sound but a **cascade of
discrete bubble events**, and that is measurable.

`bubbles.py` detects transient onsets in the 700 Hz - 9 kHz band by spectral
flux and characterises each one:

| Reference | onsets/s | onset pitch, median (10-90%) |
|---|---|---|
| Beach of Kattegat - Soft Waves | 27 | 1078 Hz (656-1880) |
| Beach of Kattegat - Soft Waves, first wave 1.16-2.24 s | 15 | 1172 Hz (844-1734) |
| Beach of Kattegat - Soft Waves, quiet gap 57.4-59.4 s | 21 | 1031 Hz (703-1889) |
| Beach of Kattegat - Sand and Foam | 26 | 1219 Hz (652-4509) |
| Kattegat Uproar - Big Waves Crashing | 24 | 914 Hz (422-1453) |
| Shores of Kattegat - Rolling Tide | 25 | 938 Hz (516-1645) |
| Beach of Kattegat - Rhythmic Tide | 30 | 1219 Hz (609-1833) |
| The City Coast - Gentle Waves | 25 | 1055 Hz (562-1875) |
| The City Harbor - Water Lapping on Pier | 17 | 469 Hz (422-848) |
| Northsea - Earth-Toned Roar | 7 | 609 Hz (422-891) |

Two things follow, and the second is the one that matters:

1. **Onset pitches cluster around 1 kHz**, spanning roughly an octave either
   side. By Minnaert that is a bubble about 3 mm across, and the spread is 1.7
   to 5 mm. So `Bubble Spread` belongs near one octave, not the two and a half
   the first version used.
2. **The rate is low enough that bubbles do not overlap.** 27 onsets a second
   with a 40 ms ring is a concurrency of about 1.1 -- roughly one bubble
   sounding at a time. That is what makes them separately audible.

The second point is what the first version got wrong twice over. Reasoning that
"only a fraction of bubbles are resolvable", `Bubble Rate` was allowed up to
6000/s and the presets used 1400/s. At a 38 ms ring that is **53 bubbles
overlapping**, and fifty-three resonators sum straight back into noise. The
cascade has to be sparse to be a cascade at all.

The engine therefore now builds the break *from* bubbles, with `Bubble Mix`
setting how much of it is the cascade and how much the turbulence underneath,
and the spawn rate following the break envelope so bubbles form fastest as the
crest collapses. A bubble's ringing time is derived from its pitch rather than
set separately -- a fixed number of cycles, which is how a real bubble behaves
and which reproduces the measured 2-62 ms spread from one control.

`graininess.py` measures whether the result is grainy or smooth: the variation
of the 4 ms envelope inside the loudest fifth of the signal, band-limited to
where bubbles live.

| | grain CV | p99/median |
|---|---|---|
| ref Soft Waves, within the first wave | 0.33 | 1.99 |
| ref Soft Waves, within the quiet gap | 0.35 | 2.23 |
| ref Gentle Waves | 0.61 | 3.89 |
| ref Big Waves Crashing | 0.76 | 5.22 |
| ref Sand and Foam | 1.08 | 8.36 |
| Gentle Waves | 0.53 | 3.29 |
| Big Waves Crashing | 0.67 | 3.70 |
| Sand and Foam | 0.63 | 3.32 |

Note the reference figures for whole files span several breaks and their gaps,
while a six-second render holds about two waves, so the whole-file numbers are
not strictly comparable; the within-wave rows are. On that comparison the
synthesis is now at least as grainy as the reference. Sand and Foam is still
the smoothest outlier, and the reason is that its foam bed is broadband noise
where the reference's foam is itself granular.

## Slowed references, and the two phases of a wave

Two references were slowed to 40% speed in Audacity, which lowers the pitch by
the same factor and pulls the individual bubbles far enough apart to be counted.
The factor was checked rather than assumed: the spectral centroid and the 90%
rolloff of the slowed files sit at 0.42-0.45 of the originals', so frequencies
scale by 2.5 and durations by 0.4 to recover real values.

Slowing them apart shows that a wave has **two distinct bubble phases**, and
they are not the same sound:

| Beach of Kattegat - Rhythmic Tide | onsets/s | pitch median | 10-90% |
|---|---|---|---|
| break / bubbling, 1.10 s | 9 | 850 Hz | 691-1301 |
| foam fizzle, 2.57 s | **29** | **2168 Hz** | 1312-3516 |

| Beach of Kattegat - Soft Waves | onsets/s | pitch median | 10-90% |
|---|---|---|---|
| break, 0.84 s | 20 | 1289 Hz | 832-1770 |
| foam, 1.95 s | 24 | 1113 Hz | 645-1904 |

The foam fizzle is **higher pitched and three times faster** than the break that
left it -- finer bubbles, exactly as Minnaert would have it, since a break
entrains large pockets and what survives is the small ones. The first version
had both phases drawing from one distribution that slid *downwards*, which is
backwards. The foam now runs its own cascade, higher and faster, controlled by
`Foam Bubbles` and pitched by `Fizz`.

## Why the crest factor is not chased

The presets measure a crest factor 3-9 dB below their references, which looked
like the obvious remaining gap. Measuring where the difference actually sits
says otherwise. Comparing the peak of the 50 ms envelope against its tenth
percentile -- the range between a break and the quiet between breaks:

| | p99/q10 |
|---|---|
| Big Waves Crashing | **34.6** vs reference 27.2 |
| Gentle Waves | **34.9** vs reference 25.8 |
| Sand and Foam | **41.8** vs reference 36.4 |
| Rhythmic Tide | 26.9 vs reference 29.0 |

The synthesis has a *wider* dynamic range than the references in three cases out
of four, so the gaps are not too loud. The crest difference is in instantaneous
transient sharpness: the references have short spikes that exceed their own
envelope percentiles, and the synthesis does not. Sharpening those is exactly
what had to be undone when the bubbles were too prominent, so this is left
alone deliberately rather than fixed.

## A distant shore is a coastline, not a quiet one

At distance the references are nearly steady -- Earth-Toned Roar measures an
envelope variation of 0.10 against a shore break's 1.86 -- and simply turning a
sequence of breaks down does not produce that. What is heard from far away is a
whole coastline of breaks arriving over a wide arc and smeared by the air they
crossed, so distance multiplies the number of events and divides their size,
holding energy roughly constant at n events of 1/sqrt(n) each, and lengthens
each one's rise and fall. Distant Roar's envelope variation went from 0.44 to
0.32 that way, with its crest factor landing at 14.1 against the reference's
14.5.

## The cloud has a spectrum of modes

Xue et al.'s Figure 3 decomposes one pour into modes at 386, 589, 732, 1121 and
1579 Hz -- ratios of 1.00, 1.53, 1.90. A single resonator at f0/cbrt(N) sounds
like a tuned pipe; three at those ratios, progressively weaker, sound like a
body of water. For a 3.7 mm bubble in a cloud of 2500 the first mode lands at
65 Hz, which is where surf rumble belongs, and it is also what sets the 50 Hz
band -- `Break Body` scales these, not the swell, which is worth knowing when
the low end is wrong.

## Bubble size is a size

`Bubble Size` reads in millimetres rather than hertz, because that is what it
physically is: Minnaert gives f0 = 3.26/r, so 3 mm rings at about a kilohertz.
The references' audible bubbles measure 1.7 to 5 mm, and the presets now sit in
that range.

## Fitting the library

`fit.py` compares every preset against the reference it was fitted to and flags
third-octave bands more than 6 dB out. It ignores bands more than 35 dB below
the reference's peak: a recording can sit at -120 dB at 50 Hz, and being 30 dB
above nothing is still nothing, so flagging it sends you chasing content neither
signal has.

Currently 12 of 17 presets have at least one audible band outside 6 dB, mostly a
single band by 6-11 dB, down from 15 of 17 with deviations as large as 37 dB.

One of them should not be chased: Forest Ocean - Shallow Clear Water measures
-13.5 dB at 50 Hz against -27 at 200 Hz, which is not what a shore does -- it is
handling rumble in the recording. The preset fits the surf band and leaves the
rumble alone.

## Foam does not wait for the next wave

Foam is quiet -- close to a sound you have to put your ear near the sand to
hear -- and it does not survive the next break. A wave arriving over standing
foam bursts it rather than letting it keep its delay, which is why the sizzle is
triggered early by the following wave instead of firing on its own clock.

## Deep coming in, brighter breaking

A wave sounds deep as it approaches and brighter as it breaks, "like opening the
cutoff of a noise generator but not fully". The spectral centroid does not show
this -- it actually *dips* at the break, because the break adds so much low end
that the energy-weighted average falls. What does show it is the balance between
bands, measured at the mid-band envelope peak and half a second either side:

| HF (2-10k) minus LF (100-400), dB | approach | break | after |
|---|---|---|---|
| Kattegat Uproar - Big Waves Crashing | 2.3 | **9.1** | -0.9 |
| Eyrarsund - Crashing Tides | -3.0 | **2.6** | -3.5 |
| Shores of Kattegat - Rolling Tide | 1.5 | **4.9** | 2.2 |
| Beach of Kattegat - Rhythmic Tide | 6.4 | 7.5 | 7.9 |
| Eyrarsund - Uproar Waves | 2.2 | -0.8 | -1.2 |
| Beach of Kattegat - Soft Waves | 19.9 | 16.6 | 15.8 |

Most brighten at the break by 1 to 7 dB. The engine had this backwards: its band
started high and swept *down*, so a wave got darker as it broke. It now opens
upward -- `Crest Open` is how much darker the approach is than the break -- and
the precursor runs through the body filter, because what is heard coming is the
mass of water, not bubbles.

Two things were fighting it and both were bugs:

1. **The slope filter was applied twice.** The break's bandpass already falls at
   6 dB/octave, the paper's nominal figure, but the code then blended between
   *one* and *two* extra poles, so the minimum was 12 dB/octave. The break lost
   its top end and measured darker than the wave before it. The extra pole is
   now mixed in rather than always applied.
2. **The band was too narrow.** The references sit only about 4 dB down at
   3 kHz relative to their 800 Hz peak, which no resonant bandpass can do. It is
   now opened right out and the tilt is left to the slope filter.

The size-to-tone coupling was also too strong: at 0.55 per unit size the biggest
waves' band fell into the low band entirely, which is why Big Waves Crashing
measured darker at the break where its reference gets 6.8 dB brighter. At 0.78 it
brightens by 6.4 dB.

## How much bubbling is right

Bubbles were made far too prominent in the previous pass. Three corrections, and
the first came from outside the measurements:

**A shore preset with no bubbles at all.** `AZ Low-FI Shore` for u-he Hive 2 is
one noise oscillator (Osc2's volume is zero), two filters in series, an amp
envelope of attack 61 / decay 78 / **sustain 9** / release 62 so each note swells
and falls, an LFO on volume, pan and phase, and reverb at **100% wet** with size
114 and decay 70. There are no discrete events in it whatsoever. A shore patch
that people like is filtered noise, slow modulation and a large space -- so
bubbles are a detail, not the substance. Space went up across the preset library
on that evidence.

**They belong after the break, not on it.** The slowed references measure 9
onsets a second during the break against 29 in the foam that follows, and the
engine was spawning them hardest at the peak of the break. The break now gets a
trickle, weighted towards its tail, and the cascade lives in the foam -- which
arrives 220-900 ms later depending on the preset.

**Not every preset should have them.** Distant Roar and Open Sea Murmur now have
none: distant surf has no separately audible bubbles, and pretending otherwise
was wrong in a way no metric complained about.

Graininess accordingly came down from 0.93-1.26 to 0.32-0.71, against the
references' 0.61-1.08 -- deliberately at or below them now, because a synthesised
sinusoidal bubble is far more salient than a real one buried in moving water at
the same measured graininess.

## Bubble physics, from Xue et al.

Xue, Aronson, Wang, Langlois & James, *Improved Water Sound Synthesis using
Coupled Bubbles*, ACM TOG 42(4), 2023. Their `FluidSound` implementation is an
offline coupled-oscillator solver over bubble tracks from a fluid simulation, so
none of it runs in a plugin -- but the analytic parts do.

**A bubble is a damped harmonic oscillator**, so its impulse response is a
decaying sinusoid. That is now generated directly rather than by ringing a
filter on noise: noise through a resonator is a hissing tone, where a bubble is
struck once and rings down.

**The damping is computable.** Their equation 3-5 gives
delta = delta_rad + delta_vis + delta_th with

- `delta_rad = omega0 * r / c`, and Minnaert's `r = 3.26 / f0` makes this
  `2 pi * 3.26 / c = 0.01368` for **every** size -- radiative loss is the same
  fraction whatever the bubble.
- `delta_vis = 4 mu / (rho omega0 r^2)`, below 4e-4 for anything audible.
- `delta_th`, which for audible bubbles is well approximated by
  `2/sqrt(psi) = 4.743e-4 * sqrt(f0)`.

So `delta(f) ~= 0.01368 + 4.743e-4 sqrt(f)` -- two constants and a square root,
cheap enough per bubble. It predicts:

| radius | f0 | delta | Q | ring to -20 dB |
|---|---|---|---|---|
| 0.5 mm | 6566 Hz | 0.051 | 20 | 2.2 ms |
| 1.7 mm | 1931 Hz | 0.034 | 29 | 11 ms |
| 3 mm | 1094 Hz | 0.029 | 34 | 23 ms |
| 5 mm | 657 Hz | 0.026 | 39 | 43 ms |
| 12 mm | 274 Hz | 0.022 | 46 | 124 ms |

Against **5-100 ms measured** across the references' 650-1930 Hz onsets, which
is a good agreement from first principles. `Bubble Damping` multiplies this,
1 being physical.

**The low-frequency roar is a coupled effect.** They show the lowest mode of a
cloud of N bubbles falls as `f0 / cbrt(N)`, so a thousandfold increase in
bubbles drops the frequency tenfold. Surf rumble is therefore not a lowpass of
the break -- it is a resonance of the cloud oscillating in phase, and it lands
below 400 Hz where Schindall & Heitmeyer put collective oscillations. `Break
Body` now drives a resonator at `bubblePitch / cbrt(N)` with N following wave
size, which for a large break puts it near 90 Hz.

## Calibration against their published results

The paper's supplementary video was measured with the same scripts. Its densest
bubble passages run **38-52 onsets/s** with a grain CV of **0.44-0.63**, which
is a useful sanity check that the metric tracks what people call good bubble
sound. ShoreBreak's presets now measure 0.93-1.26.

## What the literature added

`!dev/` holds the papers (gitignored, not ours to ship).

Klusek & Lisimenka, *Acoustic noise generation under plunging breaking waves*,
Oceanologia 55(4), 2013 — measurements under 1.6–2.8 m plungers in a large wave
flume. Directly useful, and it agrees with the library:

- Broad acoustic maximum **500–2000 Hz, peaking at 1 kHz**, "commonly reported
  for open sea observations". The library measures 400–1600 Hz.
- Slope above 1.5 kHz about **−6 dB/octave**, momentarily steepening to −10 in
  the first second of breaking and relaxing to −5…−6 afterwards. Means &
  Heitmeyer (2002) measure −10 dB/oct for plungers against −8.3 for spillers.
  This is what `Breaker` does, and why the slope is animated rather than fixed:
  one pole is −6 dB/oct, two are −12, and the mix between them is the slope.
- Decay after the maximum: **−7 dB/s** for H = 1.6–2.0 m against **−4.5 dB/s**
  for H = 2.4–2.7 m, so bigger breakers decay *more slowly*. `Wave Size`
  therefore scales `Break Decay` as well as level.
- "The increase in sound intensity is faster than the decay" — the engine's
  attack is shorter than its decay for the same reason.
- A **precursor**: "the noise level began to increase as the wave was
  approaching, before breaking occurred… the crest of an incipient plunger was
  bubbling slightly". That is the `Precursor` parameter.
- Collective oscillations of a bubble plume sit **below 400 Hz**, individual
  bubbles **above 1 kHz** (Schindall & Heitmeyer 1996). The engine splits these:
  the body filter is capped at 400 Hz, the bubbles are their own layer.

Kim & Lee, *Data-driven and physics-inspired sound synthesis of ocean waves…*,
Scientific Reports, 2026 — synthesises surf by triggering **recorded clips**
whose intensity follows screened foam-particle density, and states it "relies
solely on screened foam particles rather than full 3D fluid dynamics". That
method cannot apply to a plugin that ships no samples. Its related-work section
is still useful: it points at **Minnaert's** relation between a bubble's radius
and its ringing frequency, f₀ ≈ 3.26 / r (r in metres), which is what makes
`Bubble Pitch` a physical quantity — 2.6 kHz is a bubble about a millimetre
across.

## Fit of the factory presets

Rendered presets measured with the same script as the references. Third-octave
levels are relative to the broadband level, so the comparison is of shape rather
than loudness.

| Preset | crest (dB) | envelope CV | centroid (Hz) | 800 Hz | 3150 Hz | 12.5 kHz |
|---|---|---|---|---|---|---|
| Distant Roar / ref | 13.6 / 14.5 | 0.53 / 0.10 | 1391 / 1394 | −7.7 / −6.4 | −19.7 / −19.6 | −47.7 / −45.3 |
| Rhythmic Tide / ref | 23.9 / 25.3 | 1.16 / 1.24 | 4896 / 4365 | −6.3 / −4.8 | −5.4 / −8.8 | −15.4 / −17.2 |
| Sand and Foam / ref | 23.7 / 34.2 | 1.82 / 1.86 | 6583 / 4910 | −10.0 / −4.8 | −4.5 / −6.8 | −11.7 / −15.3 |
| Big Waves / ref | 24.9 / 27.7 | 1.48 / 1.18 | 4902 / 4279 | −8.2 / −4.6 | −6.2 / −8.9 | −15.6 / −18.0 |
| Gentle Waves / ref | 20.8 / 29.4 | 0.89 / 1.04 | 5016 / 4311 | −6.0 / −5.1 | −5.8 / −8.7 | −12.7 / −17.1 |
| Harbour Lapping / ref | 20.8 / 27.8 | 0.66 / 0.24 | 1591 / 2831 | −11.7 / −10.7 | −17.9 / −22.8 | −42.3 / −30.0 |
| Tidal Swells / ref | 17.6 / 29.3 | 0.50 / 0.59 | 1849 / 3460 | −4.9 / −4.1 | −15.0 / −12.9 | −30.2 / −20.7 |

Spectral shape is a good fit throughout, and Distant Roar tracks its reference
within about a decibel from 800 Hz up, which says the distance and air-absorption
model is right. Two things are known not to fit yet and are listed in TODO.md:

1. **Crest factor is consistently 3–9 dB below the references.** The synthesis
   is not quite peaky enough: real surf has more silence between its breaks.
2. **The steadiest sources are still too eventful** — Distant Roar measures an
   envelope variation of 0.53 against the reference's 0.10.

## Excluded references

The library contains recordings with birds, crows, geese and aircraft in them
(`Chipping Birds`, `Tiny Waves and Seagulls`, `Crows and Plane Passing`,
`Geese Fly Overhead`, `Gentle Tides & Birdsong`, `Coastal Wildscape`, and
others). ShoreBreak models **waves only** — wind and rain belong to SkyHowl and
RainyDay, and birdsong to ChirpParade — so those files were not used for fitting
anything.
