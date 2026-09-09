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
| `setup-venv.sh` | a local venv with librosa, for the one thing that wanted it |

`wavio.py` comes from `shared/tools/analysis`.

## The starting point, and the wrong turn before it

**A bird syllable is its frequency contour.** That is the finding, and it took a
wrong version of the plugin to reach it.

The first attempt modelled the syrinx from first principles. The
Gardner–Laje–Mindlin labium

```
x' = y
y' = -ε·x - C·x²·y + B·y
```

driven by two gestures — air sac pressure and syringeal tension — with the phase
between them as the syllable's shape, which is what Gardner et al. show and what
Zysman et al. recover from recordings. It was fitted to the medians in the table
below. Every number agreed, and it sounded nothing like a bird.

The medians had been measured through a 21 ms analysis window, and a 21 ms
window cannot see what a syllable does:

| | through a 21 ms window | at 0.33 ms |
|---|---|---|
| peak pitch slew | **2.7 oct/s** | **20 … 440 oct/s** |
| direction changes per syllable | **0.25** | **2 … 40** |
| octaves travelled | 0.35, which is the span | 0.2 … 6.8, against a span of 0.2 … 1.0 |

A real syllable is a **scribble**: it travels three to seven times further than
its end-to-end range. One sinusoidal gesture cannot draw one, and no amount of
correct physics above it helps. Worse, the conclusion *"a trill is not a
modulation — only 10 of 4268 syllables have FM"* was written up as a finding
when it was purely an artefact of a detector that needed 27 ms per cycle and
capped at 30 Hz.

### What replaced it

The procedure van Hunter Adams uses to synthesise a northern cardinal: put a
spectrogram in front of you, read the frequency trace off it, fit a formula, and
drive a sine table with it.

```
f(x) = -260·sin(-πx/5200) + 1740        Hz, x in samples
```

That is one sine term, read off by hand. The Bitwig Grid patch in `!dev` does
the same thing with hand-drawn multi-segment curves on a sine's pitch. Both
work, because the contour is the bird.

`contours.py` does it automatically and over the whole library: pull the pitch
and level contour of every well-isolated syllable out of the WAV, fit each as a
cosine series in normalised syllable time, cluster them per species, keep the
medoids. The number of terms was chosen by measuring the fit error:

| terms | 8 | 16 | 24 | 32 | **40** | 48 | 64 |
|---|---|---|---|---|---|---|---|
| median error | 82 ¢ | 58 ¢ | 47 ¢ | 37 ¢ | **~30 ¢** | 27 ¢ | 24 ¢ |

40 is past the knee. **One term — which is what a single gesture is — is the
82-cent column and then some.** Refitting a real syllable and resynthesising it
as a bare sine lands within **41 cents of pitch and 0.5 dB of level**.

The table that ships is 71 archetypes, **9656 floats, 38 KB, no audio** — 40
pitch terms, 24 level terms and six harmonic-balance curves of 12 terms each per
archetype. A contour is a formula in exactly the sense Adams' is.

### The partial balance

The contour gives the pitch and the overall envelope. It does not give the
*timbre*, and a fixed valve through a fixed tract gives a timbre that never
changes shape. Measured over 1777 syllables in 57 files:

| | median | range |
|---|---|---|
| energy inside a 6–10 harmonic comb | **0.67** | 0.10 … 0.99 |
| **harmonic balance drift across one syllable** | **4.4 dB** | 0.8 … 7.3 |
| harmonics above −24 dB | 2.2 | 1.0 … 8.8 |

The middle row is what justified adding it: the balance *moves*, by 4.4 dB over
a single syllable, and nothing static can do that. So each archetype carries six
amplitude curves, normalised so the partials of each frame sum to unit power —
the overall envelope is the level curve's job, and the two multiply back
together.

Each archetype also stores **what share of its own energy the comb accounted
for**, and the engine scales its use of the measurement by that. An archetype
that was inharmonic, or had a second bird in it, simply does not respond much.

### The tonality gate is the most consequential number in the pipeline

It decides which syllables become archetypes, and it selects *for tonality* — so
a strict gate fills the table with each species' cleanest syllables and leaves
the corvids thinner than they are. Swept:

| gate | usable | fit error | Crow candidates | Crow harmonics |
|---|---|---|---|---|
| 6 dB | 808 | 32 ¢ | 56 | 1.9 |
| 2 dB | 967 | 34 ¢ | 79 | 2.1 |
| **0 dB** | **1026** | **35 ¢** | **83** | **2.2** |
| −3 dB | 1100 | 36 ¢ | 110 | 2.5 |

0 dB buys 27 % more usable contours and 48 % more corvid candidates for three
cents of fit error, and it took Goose from one archetype to three and Crane from
six to eight. Below that the comb share starts falling, so the contours keep
improving while the *partial* measurement degrades — they may eventually want
separate gates.

### Medoids, not means

The archetypes are medoids of their clusters — one real measured syllable each —
and never averages. Two contours that zig-zag out of phase average to a smooth
glide, which is precisely the failure this whole rewrite was about.

### What survived from the physical model

One thing, and it matters. The equation above is **odd-symmetric**: flip the
displacement and it is unchanged, because the nonlinear loss goes as `u²`. An
odd-symmetric oscillator has only odd harmonics — energy at f, 3f, 5f and
nothing between — and no drive will make it a crow. A 234 Hz fundamental came
out as 234, 656 and 1125 Hz.

The missing physics is that the sound is not the labium moving; it is the **air
that gets past it**, and air passes only while the labia are apart. Rectifying
at the point of closure is where every even harmonic comes from, and it is the
same step that makes a glottal pulse rich rather than sinusoidal. Zysman et al.
flag the gap in as many words: *"more realistic models for this force lead to
signals with different harmonic contents"*.

So the engine keeps the valve and throws away the oscillator. `Voice` is the
fraction of each cycle the valve is shut; the contour supplies the frequency.

## What was measured, and what it decided

| Quantity | Measured across the library | What it set |
|---|---|---|
| Syllable duration | 27 … 481 ms, median **96** | `Length` range and default |
| Fundamental | 292 … 5925 Hz, median **2579** | `Pitch` range and default |
| Sweep, through a 21 ms window | 0.03 … 1.01 oct, median 0.35 | **nothing** — see the wrong turn above |
| Contour turns, same window | 0.25 … 4.0, median 0.25 | **nothing** — the real figure is 2 … 40 |
| Sweep rate | 0.3 … 12.4 oct/s, median 2.7 | how fast the gesture may run |
| Net sweep, start to end | −0.66 … +0.54 oct, median **−0.01** | up and down are equally common |
| Rise / fall | 24 ms against 41 ms | already in the measured level contours; `Skew` warps them |
| Harmonics above −24 dB | 1 … 12, median **1** | `Voice`, via the closure calibration below |
| Roughness (spectral flatness) | −35 … −16 dB, median **−28** | `Breath` default, via its calibration |
| Loudest harmonic, harmonic syllables | the **3rd**, at 1749 Hz; not the 1st in **80 %** | `Tract Length` **4.9 cm**, from c/4L |
| Within-syllable trill | **10 of 4268 syllables** — an artefact | see *A trill is not a modulation* |
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

## The prediction that was confirmed, and did not save the model

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

So the phase control is justified and the claim that it is *sufficient* is not —
and this is the part worth keeping in mind. **The prediction passed and the
model still failed.** A confirmed ordering over 3600 syllables says the two
gestures are real; it says nothing about whether two *sinusoidal* gestures can
draw a syllable, and they cannot. A correct prediction about a mechanism is not
evidence that a particular parameterisation of it is enough.

`Contour` is now the phase between nothing: it selects a measured curve. What the
measurement above still buys is confidence that the curves are not arbitrary —
they are two coupled gestures, and that is why forty cosine terms describe one
to within 30 cents while a hundred random numbers would not.

## The Species table is a measurement, and it is now shorter

The library is named by what is in it, so it can be grouped by species and each
group measured on its own. `species.py` prints the grouping it used, so it can
be checked rather than trusted. Nothing is left ungrouped.

Four of its columns were deleted in the rewrite. Sweep, contour shape, turns and
skew were in the table and they were **the wrong four numbers**: a syllable's
shape is not summarised by a sweep width and a turn count. What is left is the
five a median genuinely describes, plus the archetype set.

| Species | files | syllables | usable contours | archetypes | f0 Hz | length | harm | rough | /min |
|---|---|---|---|---|---|---|---|---|---|
| Whistler | 3 | 44 | 38 | 8 | 4748 | 89 ms | 1 | −34 | 288 |
| Sparrow | 25 | 2616 | 1282 | 8 | 3148 | 60 ms | 1 | −29 | 276 |
| Warbler | 2 | 182 | 60 | 8 | 1128 | 107 ms | 2 | −27 | 369 |
| Budgie | 5 | 473 | 126 | 8 | 1351 | 37 ms | 3 | −28 | 293 |
| Woodpecker | 2 | 48 | 29 | 8 | 3312 | 99 ms | 2 | −25 | 301 |
| Crane | 2 | 24 | 6 | 6 | 982 | 48 ms | 4 | −26 | 140 |
| Goose | 1 | 12 | **1** | **1** | 566 | 90 ms | 4 | −32 | 239 |
| Crow | 8 | 481 | 95 | 8 | 806 | 114 ms | 5 | −21 | 132 |
| Raven | 1 | 6 | **4** | **4** | 1171 | 197 ms | 1\* | −30 | 88 |

`Screech` is the tenth entry and has no references. It borrows the eight most
extreme contours in the whole library — the ones whose pitch travels furthest,
7.5 to 10.3 octaves of path — which is what "every range at once" honestly means
when there is nothing to measure.

**`length` is the median duration of that species' archetypes**, not of all its
measured syllables. The two differ where only some of a species' syllables
passed the quality gate — Crane's usable contours are its short ones — and using
the wrong one stretches a 48 ms curve to 133 ms, which turns its internal
amplitude modulation into separate notes.

**Goose has one contour and Raven four**, so `Contour` does nothing on a Goose.
Those two rows are the weakest numbers here and the reason the override below
exists.

\* **Raven's harmonic count is the one figure the engine overrides.** The
measurement says 1, over the six syllables of the single raven recording, and
the estimator reported one harmonic in one frame and nine in the next for that
same file. A raven is audibly rougher than a crow, so the table carries 6. Its
roughness is left at the measured −30 dB.

The engine applies the table as **ratios to the library-wide medians** rather
than as absolute values, so choosing a species moves the knobs' meaning without
taking them away. One consequence is worth knowing: `Pitch` at its default *is*
the library median, so every species at the default sings in its own register —
Crow at 2600 Hz gives 812 Hz, which is a crow.

## Two calibrations, measured on the plugin's own output

Both fitted by rendering the engine and measuring the result with the same
estimators that measured the references. `fit.py --voice` and `fit.py --breath`
reprint them.

**Harmonics against the valve.** How much of each cycle the syrinx is shut, at
900 Hz with no breath:

| closure | 0.00 | 0.13 | 0.26 | 0.39 | 0.52 | 0.65 | 0.78 | 0.91 |
|---|---|---|---|---|---|---|---|---|
| harmonics | 1 | 2 | 3 | 3 | 3 | 4 | 5 | 5 |

which fits `harmonics = 1 + 5.5·closure^0.6`. Inverting it turns a species'
measured harmonic count into a drive setting. It **saturates near five**, and
the noisiest references measure twelve — see *Where the model is not there yet*.

**Roughness against breath**, on a Sparrow:

| effective Breath | 0 | 4.3 % | 12.9 % | 25.9 % |
|---|---|---|---|---|
| roughness | −31.5 | −27.8 | −22.0 | −17.7 dB |

13 dB per decade above the floor. The library's median roughness is −28 dB,
which is where a sparrow lands at **5 %**, and the same curve converts each
species' measured roughness into a breath multiplier — 0.42 for the cleanest,
2.8 for the roughest.

**"Effective" is doing work in that table.** The first version of this
calibration swept the *parameter* against roughness on a Whistler, whose species
multiplier is 0.42, so it was really measuring 0.42× what it thought. The slope
came out at 17.7 dB/decade instead of 13 and the default landed four times too
high. It now reports both numbers.

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

## A trill is not a modulation — but the evidence for that was rotten

The claim is probably true and the measurement behind it was not.

Only **10 of 4268 syllables** carry a periodic wobble of their own pitch
contour, so what the ear hears as a trill here is syllables arriving too fast to
separate — up to 22.8 a second, with no silence between them. The engine has no
trill oscillator for that reason: a trill is `Syllable Rate` high and `Legato`
high.

But the detector that produced the figure of 10 needed five analysis frames per
cycle and capped at 30 Hz, so it **structurally could not see** the modulation a
bird actually has. The number is a lower bound and nothing more. What the
0.33 ms tracker shows is that the fine motion is there in abundance — 2 to 40
direction changes a syllable — it simply is not *periodic*, which is a different
statement and the one the contours capture directly.

Amplitude pulsing within a syllable is a separate and real thing: a quarter of
the library's syllables have it, at a median 13 Hz and a depth of 0.82. That is
now carried by the measured level contours, which is why `Pulse Depth` defaults
to nothing.

## What the measurements found in the version that was replaced

Every one of these was a bug or a wrong model that the analysis caught after it
had already been listened to and accepted. They are kept because the model they
belonged to is gone but the traps are not: anyone building a syrinx oscillator
will meet all of them.

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

## Two things tried and rejected

**Mel spectrograms with Griffin-Lim resynthesis**, from the SoundPlot framework
(MIT, so the licence was never the obstacle). Its synthesis is Griffin-Lim over
a 128-band mel spectrogram, and librosa's defaults put the analysis window at
**92.9 ms** at 22.05 kHz — four times coarser than the 21 ms that broke the
first version of this plugin. Griffin-Lim's worst case is frequency-modulated
transient material, which is what birdsong is, and the framework's own published
figures are `SNR −0.81 ± 0.42 dB` (negative: the error exceeds the signal) and
`Spectral Corr 0.57`; its headline `Mel Correlation 0.929` is measured in the
same mel domain it reconstructs from. Separately, a mel spectrogram at a
resolution that *would* work is the recording with its phase discarded, which is
a different thing from a coefficient table.

**librosa's pYIN** for the pitch track. This one was worth testing properly,
because the numpy tracker rejects most of the library and pYIN is a better
fundamental estimator in general. Swept over three frame lengths and three
confidence gates; on identical syllables:

| tracker | span | path | peak slew | turns |
|---|---|---|---|---|
| numpy STFT | 0.518 | 2.013 | **424 oct/s** | 23 |
| pYIN, 1024 | 0.198 | 0.388 | 16 | 11 |
| pYIN, 2048 | 0.142 | 0.205 | 7 | 5 |

It discards 80 % of the contour's path and cuts the peak slew by a factor of 26.
Its Viterbi smoothing assumes slowly-varying pitch, so it does to the contour
exactly what a long analysis window does — and its own confidence figures agree,
since gating at 0.55 drops the usable yield to 20 %. A fit error of 3 cents
against the numpy tracker's 42 looked like a win until the path length was
checked, which is the same trap as before: a smooth curve fits beautifully and
is the wrong curve.

`setup-venv.sh` still installs librosa, because its spectral features may yet be
useful for choosing archetypes that differ in timbre as well as in shape.

## Two bugs in the measurement code itself

Both produced numbers that were then written into documentation as findings, so
they get their own heading.

**`smooth()` zero-padded.** numpy's `mode="same"` pads with zeros, and these
series are log2 of a frequency — around 11.6 — so the first and last samples
were dragged towards nothing and every contour got an invented three-octave
excursion at each end. *Everything* measured through it came out with a span of
about 3 octaves and a peak slew of 8000 oct/s, **including a constant sine**. The
original diagnosis of the first version was made with a separate, correctly
padded estimator, so that finding stands — but every figure printed by
`describe()` before this was fixed was the artefact and not the bird.

**The breath calibration measured the wrong quantity.** It swept the parameter
against roughness while the species multiplier was silently scaling it. See
*Two calibrations* above.

The lesson both share with the original failure: a measurement is a piece of
software and it needs testing against a case whose answer is known. A constant
sine should measure as a constant, and neither of these was ever asked to.

## Where the model is not there yet

`fit.py --species` renders each species as one isolated syllable and measures it
back. Nine of the ten land within ±7 % on pitch and ±3 % on length. What does
not fit:

**The valve saturates around five harmonics** and the noisiest references
measure twelve. Closing it further aliases. The likely answer is a second
syringeal side — a bird has two, controlled independently — which is also the
most plausible route to a corvid's density.

**Roughness cannot be met for the rich species.** Crow measures −13 dB out of
the engine against −21 in the library, Raven −13 against −30. A harmonic stack
is spectrally flat whether or not any noise is present, and spectral flatness
cannot tell the two apart — so this may be a limit of the *statistic* rather
than of the engine. Deciding that needs a harmonic-to-noise ratio on a resolved
partial instead of band flatness.

**Spectral flatness is a poor fit target for these contours at all.** They sweep
at hundreds of octaves a second, and a pure sine doing that smears across a
21 ms analysis window and reads as rough. `fit.py`'s tolerance is 6 dB for that
reason.

**Screech does not fit and cannot.** No reference of its own, borrowed extreme
contours, and a measured pitch 54 % low because those contours swing so far that
the fundamental estimate is meaningless. It is an effect, not a bird.

**The quality gate throws away 61 % of the library** — 4268 syllables segmented,
1641 usable. Most rejections are the dense multi-bird files where the tracker is
following two birds at once. A better tracker would widen every species' set.

**The library is not evenly sampled.** 25 of the 58 files are ordinary small
birds; Goose and Raven have one file each.

**And none of this says whether it sounds like a bird.** That was the entire
failure of the first version: 61 parameters agreeing with 4268 syllables' worth
of statistics, and it sounded like nothing. `contours.py --wav` writes
reference/resynthesis pairs and `!dev/listen-v2/` holds the presets. Listen
first; these numbers only catch what the ear cannot quantify.
