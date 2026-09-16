# WhooshPact status

Version 0.2.1. The engine, the 64-parameter control surface, the window and a
fitted preset library of thirty-one, with the Sub layer opted into rather than
always on.

## What this plugin is, and how it differs from the rest of the suite

Every other Verdalis plugin models a natural sound source from first principles.
This one does not, and cannot: its reference library is 211 **finished
production sounds** rather than recordings of the world. There is no physics
underneath them to find, and fitting a physical model to them would be inventing
a mechanism the material does not contain.

So the measurement describes the *target* rather than the mechanism, and the
engine is built from what a gesture demonstrably is — a shape in time, a
spectrum that moves, a bottom end, and sometimes a chopped amplitude. That is a
deliberate departure and it is the first thing to know about this plugin.

The suite's other rules are unchanged. Nothing is sampled: every layer is
computed at run time.

## What works

- **The gesture as five numbers.** `Span`, `Peak`, `Hold`, `Rise` and `Fall`
  reproduce the measured median envelope contour of all six families to within
  the library's own spread. The transitions' contour is `(t/0.33)^1.8` up and
  about `(1−u)^2.5` down, and the fitted defaults are exactly that.
- **`Peak` is the axis the families separate on.** Booms and downshifters peak
  2.4 % into themselves, impacts at 6.0 %, braams at 14.8 %, transitions at
  32.8 %. One parameter, no modes.
- **`Type` carries a measured spectral profile and nothing else**: the slope of
  the family's octave curve above 125 Hz and how far 63 Hz stands over 125 Hz,
  both relative to Transition, with `Blend` making the six a continuum. It
  deliberately does *not* touch the shape in time, which the presets set.
- **Four layers, divided by how they are driven.** Air and Tone follow the
  gesture envelope; Sub and Hit are *struck* at a point in the span and then
  decay on their own. A boom is one event with a decay, and modelling it as a
  curve with a low end was the first thing that had to be undone.
- **The sweep is late, not even.** `Air Curve` is why: a transition's centroid
  is flat for its first third and then falls 0.80 octaves. Rendering the fitted
  default measures −0.56 octaves of centroid from −1.6 octaves of cutoff, which
  is the ratio between what the filter does and what a centroid shows.
- **Six noise colours**, each normalised to the RMS of white by a constant
  measured from 400k samples of it, so the Noise knob changes colour and not
  level.
- **Band-limited Tone.** polyBLEP saw and square, because a braam glides up an
  octave or more and a naive edge aliases audibly on the way.
- **Flutter as a start speed and an end speed**, gliding exponentially between
  them, with five shapes and three targets. Off by default.
- **Variation**, which is the reason the plugin exists. Every trigger draws its
  own span, peak, cutoff, sweep, pitch, level, pan, flutter rate, hit tone and
  decay from a two-sigma-clipped gaussian around the settings on screen. At zero
  the plugin is bit-exactly reproducible, which the self-test checks.

  Every one of those draws is a **ratio**, and the Peak draw was the exception
  until 0.2.1. As an additive offset it moved the strike by a fixed number of
  span-fractions whatever Peak was set to, which is proportionate for a
  transition at 0.33 and several times the whole value for the hit families at
  0.012 to 0.03. Worse, the clamp at zero rectified the draw, so setting Peak to
  0 did not remove the jitter -- half the notes landed on the beat and half
  straggled out behind it. Played from a sequencer it read as erratic latency.
  Measured on the sub layer over 24 seeds, Gate Closed at Peak 0 spread 0.1 to
  152.7 ms and now sits at 0.1 ms on every note; War Drum went from sd 53.3 ms
  to 5.0 ms. The coefficient is set so that a transition keeps the spread it was
  fitted with: Simple Whoosh measures sd 90.1 ms before and 89.8 ms after.
- **A three-band EQ plus both ends**, on a new shared `Biquad`/`Tilt` — the
  first shelving and peaking filters in the suite, which had only corners
  before.
- **Thirty-one presets**, five in each of the six families plus one deliberately
  thin accent, generated so that all of them carry every parameter, and verified
  by rendering and re-measuring against the family medians. Six of them have no
  sub layer at all: the Sub, like Tone and Hit, has a base of -60 dB and has to
  be opted into.

## What is measured

The library, the method and every number are in `tools/analysis/README.md`.
The short version:

| family | span | peak | fall | sweep | <100 Hz | crest | corr |
|---|---|---|---|---|---|---|---|
| accents | 3.55 s | 0.043 | 1.40 s | −0.69 oct | 0.69 | 19.0 dB | 0.58 |
| booms | 5.50 s | 0.024 | 2.84 s | −0.12 oct | 0.97 | 14.2 dB | 0.89 |
| braams | 5.63 s | 0.148 | 2.37 s | +0.60 oct | 0.74 | 12.4 dB | 0.74 |
| downshifters | 4.57 s | 0.024 | 3.02 s | −1.07 oct | 0.98 | 7.7 dB | 0.99 |
| impacts | 4.34 s | 0.060 | 2.57 s | −0.63 oct | 0.92 | 14.1 dB | 0.87 |
| transitions | 3.71 s | 0.328 | 1.29 s | −0.67 oct | 0.62 | 17.1 dB | 0.60 |

Plus: per-band decay (an impact's 63 Hz rings 0.78 s against its 8 kHz 0.21 s),
the median low fundamental per family (41–49 Hz), and the flutter survey, which
is mostly a negative result — five of the six families have no periodic
amplitude modulation, and saying so is the finding.

## Verified

- `render --selftest` passes, including the round trip of every parameter
  through the preset writer and the reproducibility of a pinned `Random Seed`.
- The 0.2.1 Peak-variation change renders **byte-identical** to 0.2.0 at
  Variation 0, which bounds it to the variation draw. Across the library at
  Variation 0.35 it tightened the onset spread of sixteen presets -- Shattered
  Earth from 123 to 29 ms, Bottomless from 101 to 14, Analog Fall from 96 to 31
  -- and loosened none.
- The preset library renders and measures inside its families on every quantity
  bar the deliberate exceptions named in `tools/analysis/README.md`, and every
  preset renders between -7.7 and -8.8 dBFS peak bar the two named quiet ones.
- The window opens at 1296×740, every parameter is placed, and the layout
  assertions are compile-time.
- The shared `biquad.h` addition was checked against RainyDay, CrackleBlaze and
  ChirpParade: all three rebuild and self-test unchanged. Nothing else includes
  it yet.

## Known gaps

See `TODO.md`. The largest are:

- **The sub pass is measurement-led and not yet heard.** The layer's base is now
  -60 dB, six presets have no sub and three more had theirs trimmed, and every
  change was decided by A/B rendering and measuring the low fraction against the
  family median. The renders are in `!dev/audition-sub/`; the calls that the
  measurement left close - the braams especially - want a listen.
- **Validated by measurement, not yet by ear against the references.** The
  suite's own rule is ear first, then measurement, and ChirpParade 0.1.0 is the
  cautionary tale. A/B rendering against the library has not been done.
- **The Hit layer's resonators do not ring on their own.** Their decay comes
  from an applied envelope rather than from their Q, which is why a very long
  `Hit Decay` sounds like enveloped noise rather than like metal.
- **No tempo sync anywhere**, including the flutter. `Span` is in seconds.
- **Crest factors run 3–5 dB above the references** in the downshifter family,
  which measures 7.7 dB. Those are heavily limited masters and the output stage
  here is a soft clipper.
