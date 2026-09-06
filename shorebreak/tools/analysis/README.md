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
