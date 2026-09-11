# CrackleBlaze — what is still to do

Ordered by what would change the sound most.

## 1. Listen to it

**Nothing in this plugin has been validated by ear.** Every figure in
`STATUS.md` is a measurement against the reference library, and ChirpParade
0.1.0 is the suite's standing proof that a plugin can agree with twenty
statistics and still sound wrong.

What to do: render A/B pairs of each factory preset against the reference each
was fitted to — `crackle_close` against `fire_near`, `open_fireplace` against
`fire-in-fireplace_close-up_reverberant`, `wet_wood` against
`fire_near_open_close`, `wood_stove` against
`fireplace-woodstove-with-the-lid-open` — and listen before anything below is
touched. Several of the items here may turn out to be the wrong things to fix.

## 2. The fire is too even, minute to minute

The default patch renders an envelope CV of **0.56 at 4 ms against the library's
0.85**, and **0.41 at 50 ms against 0.56**. The crest factor is 29.8 dB against a
median of 31.7. Those three say the same thing: the render varies less than a
real fire does.

The likeliest cause is item 3.

## 3. The flare depth above 500 Hz is extrapolated, not measured

`kFlareCv` in `fire_engine.cpp` holds the bed's measured 100 ms envelope CV per
band. The bottom three entries are the measurement outright — 0.39, 0.45, 0.57 at
125, 250 and 500 Hz. The upper five are not.

The problem is real and not laziness: above 500 Hz no gate separates the bed from
the crackle and sizzle layers riding on it, and the measured values there run to
1.28 in exactly the band the crackles live in. Since this engine generates those
layers separately, shipping 1.28 would make the bed wobble by an amount that in
the reference *was* the crackles, and then add the crackles on top. So the upper
bands continue the measured trend and flatten.

The right fix is a closed loop: fit the *render's* per-band CV against the
library's rather than fitting the bed's, with the event layers running. That is
a search over five numbers with one render per evaluation, which is affordable —
`tools/analysis/bed.py` already produces the statistic.

## 4. The crackles are not unequal enough

The rendered amplitude spread measures **4.2 dB against the library's 6.1**, even
though `Spread` is set to the measured 6.0 dB and the draw is a log-normal of
exactly that width.

Something between the draw and the detector narrows it. **Three hypotheses have
been tested and all three are wrong**, which is worth recording so they are not
tried again:

| Hypothesis | Test | Result |
|---|---|---|
| the bed is too steady, so the prominence denominator varies too little | render at Flare 0.55 / 0.75 / 0.95 | sd **3.8 / 3.8 / 3.9** — flat. Not the bed. |
| the 3-sigma clamp in `logNormalDb` removes the tail the statistic is most sensitive to | raise it to 4 sigma | sd **4.6 → 4.6** — identical. The clamp is not binding. |
| the per-pulse `0.6 + 0.8 * uniform` jitter pulls the loudest pulse of a train away from the arrival's own level | remove it | sd **4.6 → 4.3** — it *widens* the distribution slightly. Not the cause. |

What is left, and untested:

- the `1/sqrt(n)` share across a burst, which makes a 3-pulse arrival quieter
  than a 1-pulse one and so mixes two distributions. At the default `Burst` of
  exactly 2.0 the count never varies, so this cannot be the cause *at the
  default* — but it would be worth checking at a fractional Burst.
- the detector's own truncation interacting with rate: the render is detected at
  32/s against the library's 29, so it is finding *more* events, which should
  widen the measured spread rather than narrow it. That it does not is itself a
  clue.
- that the library's 6.1 dB is partly variation between physical sources within
  one recording — several logs at several distances — which a single-source
  model cannot produce and should perhaps not try to.

## 5. The crackles are a little bright

Rendered event centroid **3746 Hz against a library median of 3049**. `Crackle
Tone` and the `Body` mix both move it; which of them is wrong is not yet known,
because the measured event spectrum was fitted by its octave shape and not by its
centroid.

## 6. The bed's octave shape is within about 1 dB, not exact

The bandpasses run at a Q of 2.2, chosen by modelling rather than by taste:

| Q | worst octave-band error | ripple between centres (sd / peak-peak) |
|---|---|---|
| 1.4 | 4.0 dB | 0.15 / 0.8 dB |
| 1.8 | 2.4 dB | 0.24 / 1.4 dB |
| **2.2** | **1.1 dB** | **0.34 / 2.0 dB** |
| 2.8 | 0.1 dB | 0.50 / 2.8 dB |

At the octave-wide Q of 1.4 the bank cannot reach the contrast Log Fire and Stove
Draught ask for — the non-negativity clamp in the solve bites and the band sits
on its neighbour's leakage, 3 to 4 dB high. 2.2 is where the error stops mattering
before the ripple starts to.

A proper fix is a cascade of shelving filters carrying the curve's average slope,
with the bank solving only the residual. That would let the bank stay at Q 1.4
*and* reach any contrast. It is the same problem RiverFlow's 16 kHz octave has,
so it is worth solving in `shared/` rather than here.

## 7. Steam, the hearths and the space controls are chosen, not measured

`Steam` models the whistle of steam leaving a split in the wood; nothing in the
library isolates it. The hearth enclosure figures and the space controls need a
reverberation measurement the library cannot give, having neither an impulse nor
a known source position.

## 8. What is worth taking back into `shared/`

- **The three-level spawner.** A wandering rate, a Poisson process at it, and a
  short pulse train per arrival is not fire-specific. RainyDay's droplets and
  ShoreBreak's bubbles are both currently plain Poisson, and the Fano factor is
  the statistic that would say whether they should not be.
- **The shaped noise burst.** A click with a tone, a body and a decay, and no
  resonator, is the cheapest event generator in the suite and nothing else has
  one. ShoreBreak's foam and RainyDay's impacts are both close to it.
- **The measured-bed filterbank with a solved gain vector** is now in four
  plugins in four copies. It was already noted in RiverFlow's `TODO.md`; this is
  the fourth copy and the case is stronger.

## 9. The library is small

Seventeen usable recordings, against RiverFlow's 77 and SkyHowl's larger set. The
large-fire end is the thinnest — `big-fire-burning-flames-humming-and-crackling`
is essentially the only usable example — so `big_blaze` and `burning_roof` are
extrapolations from it and from the physics, not fits. More references of large
open fires would change those two presets more than anything else here.
