# ChirpParade analysis

Every default, every species and every factory preset in ChirpParade comes from
measurements of a reference library of **58 field recordings of birds** — garden
chirps, robins, nightingales, budgies, woodpeckers both calling and drumming,
crows, a raven, a goose, cranes, and several multi-bird soundscapes. They are
read at their own sample rates (24, 44.1 and 48 kHz) and capped at the first
45 seconds of each; the figures that need one channel take the mean of the two.
The recordings themselves are not part of this repository and are not ours to
redistribute; they live in `!dev/references/`, outside the build, and are read,
never copied.

**4268 syllables** were segmented and measured across the library. That number
is what makes the medians below worth quoting.

| Script | What it measures |
|---|---|
| `syllables.py` | the segmenter and the per-syllable measurements everything else reads |
| `chirps.py` | the syllable census: duration, pitch, sweep, contour, harmonics, roughness |
| `phrases.py` | the temporal structure: gaps, phrases, syllables per phrase, density |
| `voices.py` | the timbre families, the tract resonance, and the bed the birds sit above |
| `species.py` | the same measurements per species, which *is* the engine's Species table |
| `drums.py` | woodpecker drumming: strikes, rate, drift, knock spectrum |
| `gestures.py` | the one prediction of the model the references can falsify |
| `fit.py` | the plugin's own output, measured back against all of the above |

`wavio.py` comes from `shared/tools/analysis`.

## The starting point: a bird is an oscillator at its bifurcation

Not a sine with a pitch envelope on it. The voice is the Gardner–Laje–Mindlin
model of a syringeal labium:

```
x' = y
y' = -ε·x - C·x²·y + B·y
```

`x` is how far the labium has moved from where it sits before phonation. `ε` is
the restitution of the tissue, so it sets the frequency, `f = √ε/2π`. `B` is the
**net** dissipation — what the airflow puts in through the interlabial pressure,
less what the tissue loses — so `B > 0` is a Hopf bifurcation and phonation
begins exactly there. `C` is the nonlinear loss that stops the labia passing
through each other.

Two consequences drive the whole design.

**A syllable is two gestures.** Zysman et al. establish that `B` is proportional
to the air sac pressure and `ε` to the tension of the syringeal muscle, and that
both can be recovered from a recording — the envelope gives one, the pitch gives
the other. Gardner et al. show that syllables "of quite diverse acoustic nature"
follow from nothing but the **phase** between the two. So the engine has no
shape menu. It has a `Contour` knob, which is that phase.

**Timbre is one number and it is not a filter.** Written as a van der Pol, the
equation has a single shape parameter `μ = B/√ε`: the ratio of pressure to
tension. Small `μ` and the labia move almost sinusoidally — a whistle, one
harmonic. Large `μ` and the oscillation goes into relaxation and the harmonic
stack fills in. There is nothing between the two because there is nothing
between them in the bird.

## What was measured, and what it decided

| Quantity | Measured across the library | What it set |
|---|---|---|
| Syllable duration | 27 … 481 ms, median **96** | `Length` range and default |
| Fundamental | 292 … 5925 Hz, median **2579** | `Pitch` range and default |
| Sweep, over the audible part | 0.03 … 1.01 oct, median **0.35** | `Sweep` default |
| Contour turns | 0.25 … 4.0, median **0.25** | `Turns` default: most syllables are a plain sweep |
| Sweep rate | 0.3 … 12.4 oct/s, median 2.7 | how fast the gesture may run |
| Net sweep, start to end | −0.66 … +0.54 oct, median **−0.01** | up and down are equally common |
| Rise / fall | 24 ms against 41 ms | `Skew` default **0.37**, not 0.5 |
| Harmonics above −24 dB | 1 … 12, median **1** | `Voice`, via the μ calibration below |
| Roughness (spectral flatness) | −35 … −16 dB, median **−28** | `Breath` default, via its calibration |
| Loudest harmonic, harmonic syllables | the **3rd**, at 1749 Hz; not the 1st in **80 %** | `Tract Length` **4.9 cm**, from c/4L |
| Within-syllable trill | **10 of 4268 syllables** | there is no trill oscillator; see below |
| Amplitude pulsing | 25 % of syllables, 8.5 … 66 Hz, median **13.2**, depth **0.82** | `Pulse Rate`, `Pulse Depth` |
| Syllables per minute | 87 … 711, median **273** | `Flock Rate` range and default |
| Syllables joined with no silence | **70 % of all gaps** | `Legato` default |
| Gap inside a phrase | 17 … 174 ms, median **52** | the phrase rate |
| Gap between phrases | 0.11 … 1.64 s, median **0.31** | `Phrase Gap` default |
| Syllables per phrase | 1 … 22, median **3** | `Syllables` default |
| Syllable rate inside a phrase | 2.0 … 22.8 /s, median **7.4** | `Syllable Rate` default |
| Birds above the recording's bed | 22 … 57 dB, median **33** | why the segmenter subtracts a noise floor |
| L/R correlation | −0.05 … 1.00, median **0.72** | `Width` — birds are point sources |
| Strikes per roll | 4 … 21, median **10** | `Strikes` default |
| Strike rate | 8.1 … 20.8 /s, median **15** | `Strike Rate` default |
| Roll duration | 0.21 … 1.46 s, median 0.56 | — |
| Interval drift across a roll | −62 … +36 %, median **−23** | `Accelerate` default **+0.23** |
| Strike centroid / bandwidth | 1251 Hz / 935 Hz | `Knock` **1 kHz**, and its second mode |
| Strike ring to −20 dB | 2 … 51 ms, median **8** | `Ring` **13 ms** |

## The two gap classes are what makes a phrase a phrase

34 of the 58 references have a **bimodal** distribution of inter-syllable gaps,
found per file by Otsu's method on log gap rather than with a fixed threshold —
a budgie's phrase gap is a nightingale's syllable gap. The two classes sit a
factor of **6.0** apart in the median: 52 ms inside a phrase against 0.31 s
between them.

That factor is why ChirpParade separates `Syllable Rate` from `Phrase Gap`
instead of having one rate control, and it is what makes a phrase audible as a
unit rather than as a stream.

## Three families, found rather than chosen

Sorting every syllable by harmonic count alone splits the library into three
groups that barely overlap:

| Family | Share | Fundamental | Roughness | Duration |
|---|---|---|---|---|
| whistle, ≤ 1 harmonic | 59 % | median 3329 Hz | −30 dB | 101 ms |
| stack, 2–5 harmonics | 25 % | median 1325 Hz | −26 dB | 85 ms |
| rich, ≥ 6 harmonics | 17 % | median 399 Hz | −22 dB | 87 ms |

Harmonic count, fundamental and roughness move together and downwards: the rich
voices are the low ones. That is exactly what the model says, since `μ = B/√ε`
falls as `ε` rises — a bird singing high cannot sustain a relaxation
oscillation. So the engine does not apply a separate 1/f law to `μ`: the
relation is already in the species table, because the references measured it.

## The one prediction the references can falsify

Everything above is a description. This is the one place the model makes a claim
a recording could contradict.

If a syllable really is one turn of two coupled gestures, then its envelope and
its pitch contour are two sinusoids of the same period with a phase between
them — and the shape a sonogram reader names follows from that phase alone:

| phase | what it does | the shape |
|---|---|---|
| in phase | the pitch rises and falls with the level | arch |
| a quarter turn | the pitch sweeps through the loud part | up or down |
| anti-phase | the pitch dips where the level peaks | valley |

So the correlation between a syllable's envelope and its pitch contour must be
strongly positive for arches, strongly negative for dips, and near zero for
sweeps. If the shapes came from independent mechanisms there would be no such
ordering. `gestures.py` measures that correlation for every syllable long enough
to have one — 3600 of them — and groups it by the shape the contour was named.

| shape | n | r, envelope vs pitch | r > +0.5 | r < −0.5 |
|---|---|---|---|---|
| arch | 214 | **+0.27** | 29 % | 3 % |
| down | 1057 | +0.16 | 17 % | 10 % |
| up | 939 | +0.15 | 19 % | 9 % |
| flat | 241 | +0.11 | 13 % | 10 % |
| wobble | 880 | +0.08 | 8 % | 3 % |
| valley | 269 | **−0.10** | 4 % | 18 % |

**The ordering holds, monotonically, across all six shapes**, and the arch/valley
separation is 0.38. Arches are positive as predicted and sweeps sit near zero as
predicted. The share above +0.5 is seven times higher for arches than for dips,
and the share below −0.5 six times higher for dips than for arches.

Where it is weaker than the idealised model: **the dips are only −0.10**, not the
strongly negative figure two clean anti-phase sinusoids would give. Two reasons,
and only one of them is the model's fault. The library carries a general positive
bias — the whole-library median is +0.11, because a bird's pitch and level do
broadly rise together, which is the same model with the gestures broadly in
phase — and a real syllable is not one clean cycle of anything.

So: the phase control is justified, and the claim that it is *sufficient* is not.
ChirpParade therefore exposes `Turns` beside `Contour`, which lets a syllable
span more or less than one turn of the gesture, and `Variation`, which stops a
phrase being the same gesture repeated. Neither is in the papers.

## The Species table is a measurement

The library is named by what is in it, so it can be grouped by species and each
group measured on its own. `species.py` prints exactly the table that is in
`src/dsp/chirp_engine.cpp`, along with the grouping it used, so the grouping can
be checked rather than trusted. Nothing is left ungrouped.

| Species | files | syllables | f0 Hz | sweep | length | skew | harm | rough | /min | turns | shape |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Whistler | 3 | 44 | 4748 | 0.21 | 128 ms | 0.35 | 1 | −34 | 288 | 0.25 | down |
| Sparrow | 25 | 2616 | 3148 | 0.34 | 99 ms | 0.40 | 1 | −29 | 276 | 0.25 | down |
| Warbler | 2 | 182 | 1128 | 0.37 | 87 ms | 0.43 | 2 | −27 | 369 | 0.25 | down |
| Budgie | 5 | 473 | 1351 | 0.47 | 99 ms | 0.26 | 3 | −28 | 293 | 0.25 | down |
| Woodpecker | 2 | 48 | 3312 | 0.31 | 96 ms | 0.35 | 2 | −25 | 301 | 0.50 | arch |
| Crane | 2 | 24 | 982 | 0.36 | 133 ms | 0.33 | 4 | −26 | 140 | 0.75 | wobble |
| Goose | 1 | 12 | 566 | 0.51 | 206 ms | 0.60 | 4 | −32 | 239 | 0.50 | up |
| Crow | 8 | 481 | 806 | 0.45 | 144 ms | 0.41 | 5 | −21 | 132 | 1.50 | wobble |
| Raven | 1 | 6 | 1171 | 0.38 | 267 ms | 0.42 | 1\* | −30 | 88 | 0.50 | arch |

`Screech` is the tenth entry and has no reference behind it. It is every range
at once, kept because a plugin for birds should be able to make a noise no bird
makes.

\* **Raven's harmonic count is the one figure the engine overrides**, and the
reason is recorded rather than quietly applied. The measurement says 1, over the
six syllables of the single raven recording, and the estimator reported one
harmonic in one frame and nine in the next for that same file. A raven is
audibly rougher than a crow, so the table carries 6. Its roughness is left at
the measured −30 dB, which is the conservative choice and means the engine's
raven is less noisy than its harmonics would suggest.

The engine applies the table as **ratios to the library-wide medians** rather
than as absolute values, so choosing a species moves the knobs' meaning without
taking them away. One consequence is worth knowing: `Pitch` at its default
*is* the library median, so every species at the default sings in its own
register — Crow at 2600 Hz gives 812 Hz, which is a crow.

## Two calibrations, measured on the plugin's own output

Both were fitted by rendering the engine and measuring the result with the same
estimators that measured the references. `fit.py --voice` and `fit.py --breath`
reprint them.

**Harmonics against the drive.** `μ` is the model's own parameter; how many
harmonics it produces is not something the model tells you.

| μ | 0.15 | 0.58 | 1.43 | 3.52 |
|---|---|---|---|---|
| harmonics | 1 | 2 | 4 | 9 |

which is `harmonics = 1 + 1.70·μ^1.22`. Inverting it is what turns a species'
measured harmonic count into a drive setting, and it is the only fit in the
species path — the table itself stays a measurement.

**Roughness against breath.**

| Breath | 0 | 2 % | 5 % | 10 % | 18 % | 30 % | 50 % |
|---|---|---|---|---|---|---|---|
| roughness | −37.4 | −36.5 | −33.0 | −28.1 | −23.6 | −19.5 | −15.3 dB |

17.7 dB per decade above about 3 %. The library's median roughness is −28 dB,
which is why `Breath` defaults to 10 %, and the same curve is what converts each
species' measured roughness into a breath multiplier.

## Woodpecker rolls accelerate. They are not supposed to.

A great spotted woodpecker's roll is usually described as *decelerating* towards
its end. The library says the opposite, and clearly:

- the interval across a roll drifts by a median **−23 %** from a straight-line
  fit — a shortening interval, so a rising rate
- **22 of 35 rolls speed up**, 9 slow down, 4 are steady
- asked without a fit, the last third of a roll runs at **0.79** of the interval
  of the first

The check matters because an onset detector that loses quiet late strikes would
*lengthen* the late intervals, which can only bias this the other way. So the
finding survives the obvious objection to it, and `Accelerate` defaults to
+0.23 rather than to a deceleration.

## A trill is not a modulation

Only **10 of 4268 syllables** carry a periodic wobble of their own pitch
contour. What the ear hears as a trill in this library is syllables arriving too
fast to separate — the syllable rate inside a phrase reaches 22.8 a second — with
no silence between them.

So there is no trill oscillator in the engine. A trill is `Syllable Rate` high
and `Legato` high, which is what the measurement says it is. Amplitude pulsing
*within* a syllable is a separate and real thing: a quarter of the library's
syllables have it, at a median 13 Hz and a depth of 0.82.

## What the measurements found, and what had survived sounding plausible

Every one of these was a bug or a wrong model that the analysis caught after it
had already been listened to and accepted.

**The oscillator could only make odd harmonics.** The equation is
odd-symmetric — `u → −u, v → −v` leaves it unchanged, because the nonlinear loss
goes as `u²` — so its displacement has energy at f, 3f, 5f and nothing between.
No amount of drive would make a crow: the spectrum came out at 234, 656 and
1125 Hz for a 234 Hz fundamental. The fix is the physics that was missing: the
source is not the labial displacement, it is the **air that gets past**, and air
only gets past while the labia are apart. Rectifying the gap at the point of
closure is the same step that makes a glottal pulse rich rather than sinusoidal,
and Zysman et al. flag the gap in as many words — "more realistic models for
this force lead to signals with different harmonic contents".

**The oscillator ran flat, by up to eighty per cent.** Discretised as written,
the nonlinear term is a *scaling of the velocity*, which is a shear rather than
a rotation — and a shear moves the frequency. Nor does it average out over a
cycle: around the limit cycle the mean of `(1 − u²/h)` is −1, not zero. A 4.7 kHz
whistle came out 4.5 % flat and a hard-driven crow was out by a fifth. The fix
is a change of coordinates rather than of the equation: in **Liénard form** the
nonlinearity becomes an *additive* term, which forces the oscillator instead of
shearing it, so the magic-circle step `d = 2·sin(πf/sr)` keeps its exact
frequency. The residual is now within ±3 % across the whole range of `Voice`.

**`Sweep` and `Turns` fought each other.** The gesture is a sinusoid over the
syllable, so its raw excursion depends on how much of a cycle the syllable
spans: a crow with two turns swept twice as far as its label said. Now the
excursion is measured at spawn over the **audible** part of the syllable and
scaled to `Sweep`, so the two controls are independent.

**`Pitch` was not the pitch.** The pressure gesture peaks at 0.37 of the
syllable, not the middle, so the pitch of a swept syllable at its loudest is not
the middle of its contour. Anchoring the contour so that the pitch at the peak
*is* `Pitch` is what makes the instrument play in tune from a keyboard.

**A syllable had a 400 ms tail.** With `μ` allowed near zero, the oscillation
sits at the bifurcation and neither grows nor decays: a whistle took hundreds of
milliseconds to start and as long again to stop, against a measured fall of
41 ms. `μ` has a floor, and the bifurcation threshold `bth` is 0.10 rather than
an epsilon — `B` below threshold is the tissue's own passive loss, which is a
real fraction of the drive.

**`Ring` did nothing to the drum.** The measured strike bandwidth of 935 Hz at a
1251 Hz centroid is a Q near 1.3, and a resonator that broad has rung out in a
fifth of a millisecond — yet the same strikes take 8 ms to fall 20 dB. Both
figures are true: the body is broad and the *contact* is not instantaneous. Set
on the resonators, `Ring` gave a 1 ms knock however it was turned; set on the
excitation, it does what it says.

**The trill detector was measuring the analysis frame rate.** Autocorrelating a
112 ms syllable's contour means autocorrelating twenty noisy samples, which
peaks at its shortest allowed lag almost every time: the first version reported
a 37–43 Hz trill for a fifth of the library, which is the frame rate divided by
five. Measuring the envelope in the time domain and requiring the spectral peak
to stand four times above the median of the search band took the figure from
20 % to 0.2 %.

**The pitch tracker jumped between harmonics.** Taking the tallest bin per frame
made a green woodpecker's call sweep from 1.2 kHz to 7.1 kHz, because two frames
in the middle were loudest at the third harmonic. Tracking from the loudest
frame outwards and refusing a step of more than half an octave is what makes the
sweep measurement mean anything.

**The pitch range included the parts nobody hears.** A syllable's onset and
offset ramps are voiced but nearly silent, and the tracked partial wanders
freely there. On a clean synthetic render — where no background masks the
ramps — a syllable with *no sweep at all* measured 1.5 octaves of it. The range
is now measured only within 12 dB of the syllable's own peak, which is also the
only part a listener hears a pitch in.

**The segmenter's floor test failed on clean audio.** It is relative to the
quietest fifth of the file, which works for a field recording and fails for a
synthetic one: a render holding a single chirp in four seconds of digital
silence has a "quiet level" near nothing, so the whole 60 dB decay tail passed
and one 200 ms chirp was measured as a 603 ms syllable. A frame now also has to
be within 45 dB of the loudest frame in the file. Re-running the whole library
with the gate changed 4287 syllables to 4268 and moved no median by more than
3 %, which is the check that it did not quietly rewrite the references.

**A harmonic product spectrum is the wrong tool for a library with whistles in
it.** It always names some fundamental, including for a pure whistle that has
none: a robin's 8 kHz whistle came out at 2.8 kHz, a third of 8 kHz being a bin
like any other. Dividing the dominant partial by 1 through 6 and scoring each
candidate by the share of band energy inside a comb of its harmonics — with a
penalty on the lower candidates, since a subharmonic's comb contains the one
above it — gives both a fundamental and a harmonic count.

**Forgetting `Pitch Spread` cost an afternoon.** The first bird of a flock
carries its own pitch offset, so a "nominal" isolated render came out a third of
an octave off and looked convincingly like an oscillator running sharp. It is in
`fit.py`'s isolation list now, with a note.

## Where the model is not there yet

`fit.py --species` renders each species as one isolated syllable and measures it
back. Nine of the ten land within ±4 % on pitch and within one or two harmonics.
What does not fit:

**Roughness and harmonic count are not independent, and for three species the
references ask for both at once.** A relaxation waveform is spectrally flat
whether or not any noise has been added, so a voice with four to six harmonics
cannot also be spectrally peaked. Goose measures −32 dB in the library and −22
out of the engine; Raven −30 against −21; Budgie −28 against −22. The engine
carries the measured roughness as *noise*, which is the conservative reading,
because spectral flatness cannot tell turbulent noise apart from a rich source.
The chaotic route is `Rasp`, and it is deliberately left as a control the presets
set by ear rather than something a species applies on the strength of a
statistic that cannot see it.

**Syllable duration measures 6–33 % longer than `Length`.** The measurement
includes the onset and offset ramps and the oscillator's own tail; the setting is
the length of the gesture. Both are defensible and the references were measured
the same way, so the offset is a property of the comparison rather than an error
in either — but a preset aiming at a particular measured duration sets `Length`
below it.

**`Sweep` is off by more than 10 % for three species.** Woodpecker −33 %, Crow
+37 %, Raven −53 %. All three have a contour that turns more than once, where
the audible window and the turning points interact; the six species with a plain
sweep are within 10 %.

**Screech does not fit and cannot.** It asked for eight harmonics at 5.2 kHz,
which is a fortieth harmonic above Nyquist, and the engine's own anti-alias
clamp refused. Six at 1.8 kHz is what the band allows and what it now asks for;
its measured pitch still runs 38 % high because the clamp bites and its contour
is deliberately wild. It is the one entry with no reference to be wrong about.

**The library is not evenly sampled.** 25 of the 58 files are ordinary small
birds, and Goose and Raven have one file each — 12 and 6 syllables. Those two
rows of the species table are the weakest numbers in it, and the Raven override
above is the direct consequence.
