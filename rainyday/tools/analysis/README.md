# Matching RainyDay against real rain

These scripts measure a set of reference recordings and RainyDay's own output
with the same code, then move preset values until the two agree. They are how
the factory library was tuned, and how it should be re-tuned whenever the
engine changes.

Nothing here ships with the plugin. It needs `numpy` and a built plugin plus
`rainyday-fithost` (`./install.sh` produces both).

## Use

```sh
export RAINYDAY_SOUNDS=/path/to/reference/wavs
export PYTHONPATH=../shared/tools/analysis             # wavio.py is the suite's
python3 tools/analysis/match.py report                 # how far off is the library
python3 tools/analysis/match.py fit --out /tmp/tuned   # tune preset values
python3 tools/analysis/match.py loudness --src /tmp/tuned --out presets
```

The recordings are not in the repository. Supply your own mono WAVs named after
the keys in `pairs.py` — `rain_on_concrete.wav`, `multiple_water_drops.wav` and
so on. Any sample rate and any of 16/24/32-bit PCM or 32-bit float will do.
`rain_on_wet_concrete` is the natural take described under *The smack* below.

`--only <preset>` restricts `report` and `fit` to the presets named, which is
the only way to run either while the rest of the reference set is missing.

## What is measured

`feat.py` computes, identically for a recording and for a render:

| Feature | What it pins down |
|---|---|
| Energy in nine log bands | Spectral shape — the spine of the match |
| Per-frame spectral flatness | Noise against a cloud of tones |
| Impulsiveness per band | Continuous wash against separate impacts |
| Temporal flatness per band | The texture in between those two extremes |
| Windowed crest factor | Dynamic contrast, independent of duration |
| Slow envelope modulation | Surges and lulls |

Flatness is measured **per frame and then averaged**, never on the average
spectrum, and this is the single most important detail here. A few hundred
randomly pitched droplets per second average out to a perfectly smooth
spectrum, so long-term flatness cannot tell them from a noise band of the same
shape — `multiple_water_drops` measures 0.03 over its whole length and 0.13 per
frame. Fitting against the long-term figure drove Tonality up until the rain
buzzed: Gutter Trickle reached 0.92, which is 245 pitched plinks a second in a
1.7-octave band, and sounds like it. Per-frame flatness is now the most heavily
weighted single number in the objective, and the long-term one is not used at
all.

Two further details matter more than they look. References are **noise-floor
compensated**: a per-band floor is estimated from the 10th percentile of the
short-time energy and subtracted, because field recordings carry room tone and
preamp hiss that RainyDay is not trying to reproduce, and without this the fit
chases the hiss instead of the rain. And crest is measured over **fixed
windows**, because the peak of a 100-second recording is drawn from far more
impacts than the peak of a 7-second render, so a whole-signal crest would
compare lengths rather than textures.

## How the fit works

`fit.py` runs a coordinate descent: each parameter in turn is tried at a few
values around its current one, the best is kept, and the step shrinks each
pass. Every candidate renders with the same fixed seeds, so two candidates
differ by their parameters and not by which droplets happened to fall. Two
seeds are averaged — one is enough to rank candidates but lets the fit chase
the accidents of a single realisation. The winner is then re-scored on seeds it
never saw, and that number is the one worth believing.

Surface, Chirp, the space controls and the envelope are never fitted. Those are
what give a preset its identity; the fit is there to correct tone and texture,
not to redesign the sound.

## The smack: what one drop does on wet concrete

Everything above describes a texture. `smack.py` looks at the events in it, and
it exists because a texture can measure right while every event in it is wrong:
the splat cascade that RainyDay carried from 1.5 to 1.10 survived two re-fits
that way.

```sh
python3 tools/analysis/smack.py compare reference.wav render.wav
python3 tools/analysis/smack.py shape reference.wav
```

It needs `scipy` as well as `numpy`. The docstring at the top of the file says
how a strike is defined and why the medians are medians.

### The recordings

Three, of rain on wet concrete from close by, in `!dev/newNewSounds`:

| File | Length | What it is |
|---|---|---|
| `smacking_squishing_trickling_water_02` | 28 s | the natural recording: strikes over a dense bed. `..._water` is 24 s of the same take |
| `..._isolated` | 41 s | strikes with the bed taken out |
| `..._isolated_02` | 64 s | the same, from another take |

The isolated two are the reason any of this could be measured -- 142 and 176
strong strikes with nothing within 10 dB of them, against one in the natural
take -- and they come with two limits that are worth more than the numbers:

- **They have been through a separation or gating step, and it leaves a smear.**
  The level 3 ms *before* a strike is 25 to 28 dB under its peak whether the
  strike is 32 dB over the floor or 46, which a real noise floor would not be.
  It is the processing's own analysis frame spread round every loud event. So
  these files describe a strike down to about 25 dB under its click and say
  nothing below that -- including how long the wet tail really runs.
- **Their two channels are one recording.** The envelopes correlate at 0.98 and
  the waveforms at 0.004. Summing them to mono is not safe; only the first
  channel is read.

Two more files, `freesound_community-rain-on-concrete-*`, are 22 kHz MP3s
limited to a crest of 2 to 4 dB. They were looked at and not used.

### What a strike is

Over the two hundred loudest strikes of each isolated recording, median across
strikes, in dB under each strike's own peak:

    ms after          0.3    0.6    1.0    2.0    3.0    5.0    7.0   10.0   20.0   40.0
    take 1           -9.3  -15.2  -17.1  -16.8  -17.0  -21.4  -24.5  -27.9  -27.1  -29.0
    take 2           -8.0  -13.6  -15.9  -19.0  -21.0  -22.9  -24.7  -24.9  -29.6  -30.4
    1.10.1 cascade   -5.8   -7.6  -11.0  -16.3  -21.2  -15.1  -20.6  -18.8  -17.8  -17.7
    1.10.1 trickle   -4.9   -6.9   -8.8  -11.8  -12.7  -16.1  -18.9  -19.1  -19.7  -20.5
    1.11.0          -10.5  -13.5  -16.7  -18.1  -19.4  -20.6  -22.5  -24.2  -26.5  -26.7

The two 1.10.1 rows are the two places it had a drop on a hard surface: the
droplet's cascade (Concrete Alley close and dry, trickle and bed off) and the
Trickle layer by itself. 1.11.0 is the first of those again; the Trickle is now
the same voice and measures the same to within a decibel.

Three things are in that, and they are the model:

| Part | Measured | Built |
|---|---|---|
| the click | 0.18 ms wide at -6 dB in both takes; one bipolar pulse, a cycle and a half | one impulse through the surface band |
| the snap | the level 0.3 to 1 ms on: 9 dB down, then 15, then 17 | noise 2.7 times the body, falling 60 dB in 3.2 ms |
| the body | a line through 1.5 to 12 ms gives 44 ms to fall 60 dB in one take and 81 in the other | noise falling 60 dB in 54 ms |

The three constants were fitted together, by coordinate descent on a numpy
prototype of the voice, against the mean of the two takes from 0.3 to 7 ms; the
result is within 1.5 dB of it at every point. The click stands 4.26 to 1 against
the snap in amplitude at 48 kHz, and is scaled by the root of the sample rate so
that it keeps that place at any rate: a band's response to one sample falls as
the rate rises, and its response to noise falls as the root of it. Rendered at
44.1, 48 and 96 kHz the median strike agrees to half a decibel from 0.3 to 7 ms.

**The band.** The click's spectral centroid is 8.0 and 7.5 kHz in the two takes,
scattered by 0.42 and 0.47 octaves from strike to strike, and the body sits in
the same place. It does not move with level: 8.6 kHz for strikes 10 dB over the
floor and 8.2 kHz for strikes 30 dB over it, so nothing in these recordings
says a quiet strike is a dull one. Both takes stop at 16 kHz, so nothing is
known above that. A band centred on 6.5 kHz and scattered by 0.6 octaves
measures 7.3 to 8.4 kHz through the same analysis.

**Nothing is pitched.** The body's spectral flatness is 0.23 to 0.43 against
0.57 for white noise through the same window, which is a band and not a tone.
Rain on stone traps no bubble, and Trickle Impact at 100 % is now all strike.

**One smack is often two strikes.** The density of strikes after a strike, over
the mean rate:

    lag, ms      3      5      7     11     15     21     30     42
    take 1     2.00   1.35   1.19   1.15   1.16   1.10   1.05   0.98
    take 2     1.79   1.30   1.24   1.13   1.05   1.02   1.00   0.99
    1.10.1     1.32   1.52   1.35   1.33   1.36   1.30   1.24   1.18
    1.11.0     1.87   1.50   1.39   1.26   1.17   1.07   1.01   0.98

Twice the mean rate in the 2 to 4 ms after a strike, and back to it by 30 ms:
about one more strike per drop, a few milliseconds on. The cascade had it the
other way round -- too little at 3 ms and an excess that ran on past 40. Both
engine rows are Concrete Alley as shipped in that version, close and dry.

### What the cascade got wrong

It was built from counting envelope peaks after strikes in one recording: six
bursts in 40 ms, 5 ms apart, each about 11 dB under the first, stragglers out to
300 ms. Each of those numbers can be read off a recording, and the sum of them
is not a drop. The cascade row of the first table is what it rendered: no click
-- 6 dB down after 0.3 ms where the recordings are 9 down and falling -- a
second burst arriving at 5 ms as loud as the first had become, and then a
plateau 18 dB under the peak for as long as anyone cared to look, where a real
strike is 25 dB down inside 10 ms. Heard, that is a rattle.

The Trickle's drop was wrong differently. Its strike was a 2 ms burst of noise
with no click in front of it, 1.2 ms wide at -20 dB against the recordings'
0.6, and at the presets' settings most of it was the pocket: its click centroid
measured 3.4 kHz where a strike on concrete is at 8.

### Where the drops are

The loudest strike in a hundred is 21 and 20 dB over the median one in the two
takes, and the loudest in ten 11 dB over it. No spread of drop sizes gives that:
the size law in `spawnDroplet` spans 45 to 1 in amplitude end to end, and its
ninety-ninth percentile is a few dB over its median. It is geometry. Drops land
all over a surface and the microphone is at one point above it, so with drops
uniform over the patch twice as many fall at twice the range and each is half as
loud -- a power law.

So a trickle drop at a fraction `u` of the patch's area is at a range of
`sqrt(n^2 + (1 - n^2) u)` and as loud as one over that, where `n` is how near
the nearest can land. `n` is 0.06 with Distance at zero and rises to 1 at full
Distance, where every drop is as far away as every other; what is left over for
the drops themselves is a log-normal spread of 4 dB. The whole is divided by its
own RMS, `sqrt(2 ln(1/n) / (1 - n^2))`, so the layer's level does not move with
any of it: over fifteen presets and eight seeds the trickle is within 0.14 dB
of where 1.10.1 had it at the same settings, and its peaks are several dB
higher.

**The top of the distribution is what set those two numbers.** A nearest of 0.04
with 5 dB of spread fits the strike percentiles as well. What separates them is
how far the loudest samples stand over the layer's own RMS, which the takes can
be read for directly:

                          99.9 %   99.99 %   99.999 %   the loudest
    take 1                 20.6      28.7       32.7        34.9
    take 2                 20.8      27.5       31.0        34.3
    n 0.04, 5 dB           20.8      28.5       34.7        39.9
    n 0.06, 4 dB           20.2      27.3       33.0        38.0

The two agree to the ten-thousandth sample and part company above it, and the
loudest sample of a take is the one that reaches an output stage first.

### The fit that was refused

Wet Pavement was fitted to the natural take with `match.py fit`, all of its
droplet, bed and Trickle controls free. It converged from a distance of 41.7 to
10.7 and held on unseen seeds, 37.7 to 11.2 against a noise of 1.4, which is the
largest improvement any preset here has shown. `smack.py` on the result:

                                 natural   hand-set    fitted    as shipped
    strikes a second                  65         70        18         67
    over floor p50/90/99, dB    12/16/21   13/18/22  12/18/26   12/15/20
    clustering at 3 / 11 / 21 ms  1.2/0.9/0.9  1.6/1.3/1.1  3.8/2.7/2.1  1.8/1.2/1.1
    energy at 3.2-6.3 kHz, %        25.7       33.3      37.0       31.5

It had bought its distance with a denser, brighter wash -- Density to 2160,
Drop Pitch to the top of its range -- and the strikes went under it. Nothing in
`fitlib.distance` counts strikes, so nothing in it could object. This is the
third time a fit here has improved its number by removing the thing the preset
is of (Concrete Alley's splat and Light Drizzle's drops were the first two, in
TODO section 3), and the first time there was a measurement to say so with.

So the preset was tuned on the events instead: a coordinate descent over the
same controls whose loss is `smack.texture()` against the natural take -- the
five band energies in dB, the strike rate, the three over-floor percentiles, the
clustering at the first six lags, and how far the loudest thousandth,
ten-thousandth and hundred-thousandth of samples stand over the RMS. Two things
about how it was run are worth keeping:

- **One channel against one channel.** `fithost` hands back a mono mix, and a
  mono mix of a wide field is not what the recording's left channel is: the
  decorrelated bed loses 3 dB in it and a hard-panned drop loses 6. Tuned on
  the mix, the preset measured 48 strikes a second on its own left channel.
- **Forty decibels under the clipper.** Rendered at its own gain, a candidate
  with strikes that were too loud scored well, because the output stage was
  flattening them to where the recording's are. That is how the level law's
  top was found to be too hot at all.

It is a scratch tool and is not in the repository. If a second preset needs it,
its loss belongs in `fitlib.distance` behind the `CLOSE` mode.

### What it could not say

- **How long the wet tail runs.** Below the smear, nothing. The 54 ms is the
  early slope, and anything slower under it is a guess the Splash and Decay
  controls are there to make.
- **Wood and glass.** Both recordings are concrete. The other two columns of
  the surface table are still concrete's moved by reasoning.
- **Stereo.** See above.
- **Whether the natural take agrees.** It has one strong isolated strike. Its
  median strike falls like the isolated ones for the first millisecond and then
  sits on its own bed at -15 dB, which neither confirms the body nor
  contradicts it.

### RiverFlow's drop

RiverFlow's Trickle took the smack in its 0.5.0 by the same call, and nothing
else: not the field, because its own references measure a stream's spray as
close to smooth, and no preset value. Its presets hold Impact at 45 %, where
most of a drop is still the pocket, so the change there is the leading edge and
not the body. The Trickle on its own, at Hanging Trickle's settings:

                               0.4.0    0.5.0    rain on concrete
    click width at -6 dB, ms    0.50     0.25       0.18
    click width at -20 dB, ms   1.71     1.29       0.56 / 0.68
    click centroid, Hz          4619     5348       8017 / 7479
    median strike 0.3 ms on    -3.8     -5.2       -9.3 / -8.0
    median strike 1 ms on      -9.9    -11.2      -17.1 / -15.9

There is no recording of a drop coming off a ledge to say whether that is where
it should stop. What this plugin's history does say is that the old drop was
too tonal for the stone drops in RiverFlow's own references -- a spectral
flatness of 0.11 against their 0.34 -- and turning Impact up moves it the right
way.
