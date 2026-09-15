# Measuring ThunderClap against real thunder

`measure.py` computes the same figures for a reference recording and for a
ThunderClap render, so the two can be put side by side. It is how the engine's
constants and the factory presets were set, and how they should be checked
again whenever the engine changes.

Nothing here ships with the plugin. It needs `numpy` and a built plugin plus
`thunderclap-render` (`./install.sh` produces both).

## Use

```sh
# A whole directory of recordings, one line each.
python3 tools/analysis/measure.py bands '!dev/reference'

# Render a preset and measure it the same way.
./build/thunderclap-render --plugin build/ThunderClap.clap \
      --preset close_strike --tail 15 --param "Random Seed=3" --out /tmp/close.wav
python3 tools/analysis/measure.py bands /tmp/close.wav

# How the spectrum moves through the thunder: the clap, then the rumble.
python3 tools/analysis/measure.py time /tmp/close.wav 0.5

# The first second and a half at 5 ms resolution: the attack.
python3 tools/analysis/measure.py onset /tmp/close.wav

# The strike alone: the octave bands of the 400 ms from the onset, and the
# figures that separate a hard clap from a crackle.
python3 tools/analysis/measure.py clap   '!dev/reference_new'
python3 tools/analysis/measure.py impact /tmp/close.wav

# How the onset's spectrum moves: five bands and a centroid, 40 ms window
# hopped by 10, which is what Bloom was set against. A render whose bloom
# resolves inside 20 ms needs a shorter window to show it.
python3 tools/analysis/measure.py onsetbands '!dev/reference_new'
python3 tools/analysis/measure.py onsetbands /tmp/close.wav 20 5
```

`contours.py` asks a different question: not how much energy sits where, but
what the curves look like, so that a measured contour could replace a
parametric one.

```sh
# The attack envelope and the shock front waveform.
python3 tools/analysis/contours.py front '!dev/reference'

# The spectral centroid of the flash over time, bed subtracted and SNR gated.
python3 tools/analysis/contours.py decay '!dev/reference'

# Transient arrivals through the flash: how many, how spaced, how clustered.
python3 tools/analysis/contours.py density '!dev/reference'

# The same, over a library, as C++ array initialisers.
python3 tools/analysis/contours.py fit   '!dev/reference'
```

It splits every summary into *close* and *distant* by how far the 1-2 kHz
octave sits under the loudest one over the 400 ms from the onset, because
absorption takes the top off first and a median over both distances describes
neither. See *What the contours say* below.

`bands` and `time` average over the whole file, which for a thunder means the
rumble and the tail decide the answer and the strike is averaged away. `clap`
and `impact` look at the strike alone and are what the hardness of the crack
was set against; see *What the measurements say* below.

`loudness.py` renders every factory preset over several seeds and rewrites
each preset's `gain` so that its loudest flash peaks at a chosen level, backing
distant presets off a little further because they are distant. It takes a few
steps per preset, because the compressor makes the peak a non-linear function
of the gain:

```sh
python3 tools/analysis/loudness.py --plugin build/ThunderClap.clap --presets presets
```

## What the measurements say

Everything below is measured with `bands` and `time` on the thirty-eight
recordings that were in `!dev/reference` when the engine was fitted, and all of
it is built into the engine.

**That library no longer exists.** It was deleted in September 2026 and
replaced by the twenty-four recordings now in `!dev/reference`, some of which
are like the old ones and five of which (`432101`-`432105`, `ravetracer`) are
the author's own, recorded in a city. The figures in this section were not
re-measured against the new set; *What the contours say* below was measured
against it and says so. Anything re-fitted from here on should be checked
against what is actually on disk.

**Close thunder peaks at 80 to 160 Hz and keeps its top end.** Measured
against their own loudest band, the recordings of strikes a few hundred metres
away sit 6 dB down at 320 Hz, 13 at 640, 23 at 1.3 kHz, 33 at 2.6 kHz and 42
at 5 kHz. A clean N-wave of the right length gets the bottom four of those
right and is short above 2 kHz, which is why every shock front carries a burst
for the fine roughness of the channel.

**Distant thunder is nothing but the bottom two octaves.** The far recordings
peak at 20 to 40 Hz and are 40 to 70 dB down by 1.3 kHz. That is what the
absorption law gives at ten kilometres, and the three staggered poles per shock
are there because one pole cannot be right at both 300 m and 10 km.

**The attack is 20 ms, the clap a second, the rumble ten.** A close strike is
within 30 dB of its peak 20 ms after onset and at its peak within 60. The first
second fluctuates by 5 dB at 20 ms resolution -- the return strokes -- and the
level then falls 15 dB over four seconds and another 15 over the next eight,
with swells along the way where another part of the channel arrives.

**Distant thunder swells.** The far recordings take 2 to 20 s to reach their
peak. The foot of a distant channel is in the ground's acoustic shadow, so the
first thing heard is the cloud, not the strike; `Swell` sets how much.

## What a hard clap measures like

Measured with `clap` and `impact` on the six clean recordings in
`!dev/reference_new`, which are close strikes rather than the whole storm. This
is what the crackle and `Impact` were set against.

**A hard clap is dense and low-crest.** Over the 50 ms around the peak the
recordings sit at 5 to 13 dB of crest factor; 30 to 75 % of the first 200 ms
after the onset stands within 6 dB of the peak, and the RMS of that 200 ms is
only 7 to 14 dB under the peak sample. The first version of the engine measured
11 to 16 dB of crest, 5 to 60 % density and 13 to 19 dB of peak-to-body: a
spray of separate spikes with the noise floor between them, which is heard as a
crackle and not as a blow. Finite-amplitude propagation is the difference, and
`Impact` is where it lives.

**The band tilt of a close strike is steep and it is steady.** From the onset,
over 400 ms: 20 to 160 Hz all within 5 dB of the loudest band, then 5 to 7 dB
down at 320 Hz, 14 to 18 at 640, 18 to 28 at 1.3 kHz, 26 to 44 at 2.6 kHz and
36 to 64 at 5 kHz. A white-noise crackle burst overshoots the last three of
those by 15 to 25 dB however the level is set, because white noise is flat to
Nyquist; a train of steps at the channel's roughness scale falls at 6 dB/oct
above its corner and lands on the curve.

**The bottom is not there when the clap starts.** With `onsetbands`, the close
recordings come in with the 30 to 120 Hz band 15 to 32 dB below where it ends
up and bring it there over the first 20 to 40 ms, while the total level rises
11 to 16 dB and the spectral centroid falls from 225-430 Hz to 76-100. That is
coherence: long wavelengths need many arrivals to add up and a clap has few at
its first millisecond. It is what `Bloom` models, and without it a strike has
no bright leading edge and reads as soft however loud it is.

**The hardest recordings hold their level.** `clean-hard-thunder-clap` is
within 3 dB of its peak for a hundred milliseconds without a gap: 78 % density,
7 dB of peak-to-body, 5.2 dB of crest. Part of that is the recording chain
limiting on the strike, and it is also what a listener's ear does at 130 dB
SPL, so it is the right target for what a close thunder should sound like even
where it is not strictly what the air delivered.

## What the contours say

Measured with `contours.py` on the 24 recordings in `!dev/reference`,
2026-09-15. This was a check on whether ThunderClap's parametric shapes should
be replaced by measured curves, the way ChirpParade replaced fitted statistics
with measured pitch contours. **The answer is that two of the three shapes are
not measurable from far-field recordings at all, and the third says the engine's
structure is right and two of its presets are not.**

**The shock front is not recoverable from a recording.** Aligning all 24
recordings on their steepest pressure rise, orienting each so the rise is
upward, and averaging gives a coherence of 0.44 -- and the alignment itself
guarantees a step at the centre, so almost nothing survives either side of it.
The individual figures scatter accordingly: the rise from the onset to the first
peak runs from 3.5 to 40 ms across the library with no cluster in it.

This is not a defect in the recordings. A clap is the caustic of thousands of
N-waves arriving from a tortuous channel at slightly different times (Few 1969),
so a microphone at 300 m never sees one front on its own. The engine's per-
element arrival model is that superposition, and there is no single measured
front for it to be built out of. A DDSP fit against a recording would be fitting
the sum, not the front.

**The attack envelope is not a reproducible shape either.** From the onset,
1 ms resolution, 60 ms, relative to the first 200 ms: the 10th-to-90th
percentile spread is 10 to 30 dB at every step, and stays that wide when the
library is split into close and distant. Every flash has its own channel
geometry, which is the thing the engine draws per flash, so a single baked
attack contour would be *less* true than the geometry that produces it.

**The spectral centroid does not sweep down.** This is the measurable one, and
it is the finding. Windows growing with the log of time from the onset, the
pre-onset bed subtracted, and any window that failed to stand 6 dB over that bed
dropped as rain rather than thunder:

| | 0.01 s | 0.04 | 0.08 | 0.15 | 0.30 | 0.60 | 1.2 | 2.4 | 4.8 | 9.6 |
|---|---|---|---|---|---|---|---|---|---|---|
| close, 4 files | 895 | 649 | 941 | 542 | 543 | 469 | 347 | 345 | 290 | — |
| distant, 10 files | 362 | 172 | 247 | 163 | 170 | 180 | 211 | 205 | 215 | 214 |

Fitted as `centroid ~ t^-a` past 50 ms, the exponent is **0.04** for the close
recordings (10th-90th percentile 0.00 to 0.24) and **-0.07** for the distant
ones. A swept lowpass following `1/sqrt(t)` would need a = 0.5. Nothing in this
library does that.

The reason is the same superposition: the late energy is not the early energy
lowpassed, it is other parts of the channel arriving from other distances, each
with its own absorption. A source-and-sweeping-filter model gets this wrong by
construction. ThunderClap's absorption is per arrival and per distance, so it
reproduces the flatness: its renders fit at a = 0.04 to 0.15.

**But the distant presets are too dark.** Held against the same measurement:

| | tilt at 1-2 kHz | 0.04 s | 1.2 s | 4.8 s |
|---|---|---|---|---|
| real, close | -9 to -25 dB | 649 Hz | 347 | 290 |
| `overhead_crack` | -16 dB | 727 Hz | 456 | 315 |
| `close_strike` | -23 dB | 300 Hz | 284 | 232 |
| real, distant | -26 to -42 dB | 172 Hz | 211 | 215 |
| `distant_rumble` | -57 dB | 79 Hz | 87 | 52 |
| `far_horizon` | -58 dB | 54 Hz | 44 | 57 |

No recording in the library, at any distance, puts 1-2 kHz further than 42 dB
under its loudest octave or holds a centroid under 128 Hz. Both distant presets
do both, by a wide margin. Two readings are possible and the measurement cannot
separate them: their `Distance` is set beyond anything in the library, or the
absorption is too steep at the far end. `close_strike` is the milder version of
the same thing -- 300 Hz in its first 40 ms against 649 measured.

Worth stating where this is soft: the distant recordings mostly have rain in
them, and rain is broadband, so a residue would bias the measured centroid
upward. The bed sits 12 to 48 dB under the flash and is subtracted, and in the
files where the flash and the bed measure the same centroid -- `rain_thunder_03`
and `thunder_02` -- the row is weak evidence. The gap to the presets is a
factor of two to four, which is larger than that objection.

### Transient density

`density` counts the discrete arrivals in the first 8 seconds: the rise of a
2 ms energy envelope above 300 Hz over its own trailing 50 ms median, peak
picked with a 20 ms minimum gap.

**The threshold is calibrated, not chosen, and the calibration is the point.**
At a 4 dB rise the detector finds 134 arrivals in 8 seconds of *brown noise*,
which has none in it at all -- and the references then measure an
indistinguishable 20 a second with a Fano factor of 0.3, which looks like a
finding and is an artefact of the detector. At 10 dB both noise controls give
zero and the references give 2 to 13. Anything under 8 dB is measuring the
rumble's own wandering. The same control should be run again if the detector
is ever retuned.

At 10 dB:

| | arrivals in 8 s | median gap | p10 | p90 | Fano at 0.25/1/2 s |
|---|---|---|---|---|---|
| close, 8 files | 16 (5 to 20) | 273 ms | 34 ms | 1006 ms | 0.94 / 1.19 / 1.06 |
| distant, 13 files | 11 (3 to 13) | 449 ms | 59 ms | 2376 ms | 1.10 / 1.00 / 1.06 |

**Thunder arrivals are Poisson.** The Fano factor is 1.0 within the scatter at
every counting window from a quarter second to two seconds. That is worth
having as a number, because CrackleBlaze measures 3.90 at one second and had to
grow a three-level spawner to reproduce it; thunder needs nothing of the sort,
and the engine's arrival times come from channel geometry rather than a
process anyway. They are also front-loaded: twice the rate in the first second
as in the seventh.

**The close presets arrive four to seven times too often.** Held against the
same measurement, with the seed pinned:

| | arrivals in 8 s | median gap |
|---|---|---|
| real close | 16 | 273 ms |
| `close_strike` | 66 | 62 ms |
| `overhead_crack` | 116 | 46 ms |

A real close strike is a few discrete claps in a continuum; these are a spray of
separately audible cracks. Two parameters move it, and they move it a long way:
`Max Shocks` 2048 to 4096 and `Focus` 0.7 to 0.2 take `close_strike` to 18
arrivals at a 264 ms median gap with its band tilt unchanged at -23 dB. More
elements overlap into the caustic instead of standing apart; less directivity
stops a handful of them standing proud of the rest. `overhead_crack` needs its
tortuosity down as well to get under 30, which is a change to what the preset
is rather than a correction to it.

Whether that is *better* is not something the count can say -- A/B renders are
in `!dev/audition-density/` with a note listing what each pair changes.

## References

- Few, A. A., *Power spectrum of thunder*, J. Geophys. Res. 74, 1969. The
  channel as a string of N-wave sources, the clap as the caustic of their
  arrival times, and the peak frequency in the tens of hertz.
- Ribner, H. S. and Roy, D., *Acoustics of thunder: a quasilinear model for
  tortuous lightning*, J. Acoust. Soc. Am. 72, 1982. The tortuous channel
  synthesised and listened to; the N-wave lengthening as it travels.
- Kappus, M. E. and Vernon, F. L., *Acoustic signature of thunder from seismic
  records*, J. Geophys. Res. 96, 1991. Thirteen thunders from a storm about 5
  km off: 5 to 33 s long, 1 to 5 claps each, the first clap the largest only
  half the time, channels 3 to 7 km long, and a near strike peaking at 25 to
  55 Hz.
- Guo et al., *Study and analysis of the thunder source location error based on
  acoustic ray-tracing*, Remote Sensing 16, 2024, and Rusz et al., *Locating
  thunder source using a large-aperture micro-barometer array*, Front. Earth
  Sci. 9, 2021. Rays bend upward in a normal temperature lapse, which is the
  ground shadow behind `Swell`; interstroke intervals and infrasound pulse
  widths.
