# WhooshPact

**Synthesised transitions and impacts — a CLAP and VST3 instrument for Linux and
Windows.**

WhooshPact makes the sounds that glue a track together: whooshes, risers,
braams, booms, impacts, accents and downshifters. It contains no samples. Every
gesture is built from noise, oscillators, filters and envelopes while the plugin
plays — and **no two triggers are alike**, which is the whole point of it.

Part of the [Verdalis](https://github.com/Ravetracer/Verdalis) suite.

## Why not just use samples

Because a sample plays the same transition every time. Four whooshes in a row
out of one file is one sound repeated four times, and the usual fix is to keep
twenty variants of the same whoosh on disk and remember which one was used
where.

Here every note draws its own span, peak position, filter cutoff, pitch, pan,
flutter rate and level around the settings on screen. `Variation` is the knob
that decides how far, and setting it to zero makes the plugin deterministic
again — which is what a transition that has to land on a picture cut needs.

## The one number the design rests on

The reference library is 211 production sounds in six families. `Peak` — where
the loudest moment sits, as a fraction of the whole gesture — separates them
more sharply than anything else measured:

| family | peak sits at | span | <100 Hz | sweep |
|---|---|---|---|---|
| booms | **2.4 %** | 5.50 s | 0.97 | −0.12 oct |
| downshifters | **2.4 %** | 4.57 s | 0.98 | −1.07 oct |
| accents | **4.3 %** | 3.55 s | 0.69 | −0.69 oct |
| impacts | **6.0 %** | 4.34 s | 0.92 | −0.63 oct |
| braams | **14.8 %** | 5.63 s | 0.74 | **+0.60 oct** |
| transitions | **32.8 %** | 3.71 s | 0.62 | −0.67 oct |

A hit peaks in the first few per cent of itself. A whoosh peaks a third of the
way in. That is the difference between the two, and it is one parameter.

So WhooshPact has no modes. The gesture is five numbers — `Span`, `Peak`,
`Hold`, `Rise`, `Fall` — and those five reproduce every measured envelope
contour in the library to within the library's own spread. `Type` selects the
family's measured *spectral* profile, and nothing else.

## How a gesture is made

**Air** — noise of a chosen colour (white, pink, brown, blue, violet, green)
through one swept filter. Resonance narrows the filter as well as sharpening
it, so at the bottom it is a wall of air closing down and at the top it is a
band with a pitch to follow. On its own, this is the plain white-noise whoosh.

**The sweep happens late.** A transition's spectral centroid is flat for its
first third and then falls 0.8 octaves. An envelope that moves the filter evenly
across the gesture sounds like none of these, which is why there is a `Curve` as
well as a `Sweep`.

**Tone** — a three-oscillator stack with a pitch glide, band-limited with
polyBLEP because a braam climbs an octave or more and a naive saw edge aliases
on the way up. Braams are the only family in the library whose spectrum *rises*.

**Sub** — a driven sine with a pitch drop and a click, **struck** at the
gesture's peak and then decaying on its own. A boom is one event with a decay,
not a curve with a low end under it. The measured median fundamental of the
pitched families is 41–49 Hz, all within a tone of G1.

**Hit** — three inharmonic resonators morphing towards filtered noise, struck at
its own point in the span: at the peak for an impact, at the end for a
whoosh-hit. An impact's bottom rings 0.78 s where its top rings 0.21 s, and that
ratio is the difference between an impact and a snare.

**Flutter** — a start speed and an end speed, gliding exponentially between
them. It is off by default, and that is a measurement rather than a taste: five
of the six families have no periodic amplitude modulation at all. Where it does
appear it is the downshifters — `Downshifter - Stutter Scream` accelerates from
4.1 Hz to 25.0 Hz across its span — and it is worth a feature of its own because
reaching that with an LFO means drawing an automation curve.

**EQ** — a highpass, a lowpass and three bands, on the summed output. The
highpass defaults to 25 Hz where the rest of the suite starts at 60: in a field
recording everything below that is traffic and handling noise, and here it is
the instrument.

## The window

One layout, its own impact-magenta theme, and a streak across the header for
every gesture triggered — deterministic in the gesture number, so a pinned
`Random Seed` gives a repeatable picture as well as a repeatable sound. `MIXER`
balances the four layers against each other.

## Presets

Thirty-one: five in each of the six families, plus *Dry Snap*, a deliberately
thin accent. Generated from `tools/analysis/makepresets.py` so that all of them
carry every parameter. `verify.py` holds a rendered preset against the family it
was fitted to.

The Sub layer has a base of -60 dB, like Tone and Hit, so a preset opts into it:
six presets have no low layer at all, and *Simple Whoosh* is white noise through
one swept filter and nothing else.

## Build and install

```sh
./install.sh              # configure, build, self-test, install to ~/.clap
./install.sh --vst3       # and a VST3 into ~/.vst3
```

See the suite [`CLAUDE.md`](../CLAUDE.md) for the CLAP SDK checkout and the
Windows cross-build.

## Licence

MIT, suite-wide. The reference library it was fitted against is not ours and is
not in this repository.
