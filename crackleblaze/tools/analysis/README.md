# CrackleBlaze analysis

Every default and every factory preset in CrackleBlaze comes from measurements
of a reference library of **25 field recordings** of fires: campfires,
fireplaces, wood stoves and large open blazes, 44.1 to 96 kHz, mostly stereo,
35 minutes in total. The recordings are not part of this repository and are not
ours to redistribute; they live in `!dev/references`, which is gitignored, and
are read, never copied.

The scripts here read that directory and print the numbers below. `wavio.py`
comes from `shared/tools/analysis`.

## Two things had to be thrown away first

**Sub-60 Hz is the room, not the fire.** `lowend.py` correlates each
recording's 20-60 Hz envelope against its 1-4 kHz envelope. In 24 of the 25
references the correlation is under 0.2 and in half of them it is
indistinguishable from zero — while in eight of them that band carries **27 to
60 per cent of the total energy**. It is traffic, ventilation, wind and
handling noise. The plugin generates nothing below 60 Hz and every script here
high-passes there before measuring anything.

| | <60 Hz share | corr with 1-4 kHz |
|---|---|---|
| `big-fire-loop` | 60.4% | 0.09 |
| `fireplace_02` | 49.3% | 0.01 |
| `fire-crackle-and-flames` | 46.0% | 0.20 |
| `fireplace_01` | 42.9% | −0.00 |
| `ambiance_campfire_loop_stereo` | 39.0% | −0.05 |

**Eight of the 25 references are not usable fires.** `ambiance_campfire_loop_stereo`,
`big-fire-loop`, `sauna-fireplace-loop`, `fireplace_01`, `fireplace_02`,
`fireplace_03`, `fireplace_08` and `fire-crackle-and-flames` are a steep
low-frequency ramp that dies by 400-800 Hz, above which they carry a **flat
−45 dB plateau from 1 kHz to 12 kHz** — a dither floor, not content. Their
crest factors (11.8-16.5 dB) say the same thing: nothing impulsive is in them.
They are rumble beds with a fire somewhere underneath, and averaging them in
drags every centroid towards a low-pass no listener would call a fire. Seventeen
references are left, and every number below is from those.

## Fire is not water, and the crest factor says so first

| | CrackleBlaze library | RiverFlow's library |
|---|---|---|
| crest factor, median | **31.7 dB** | 19.6 dB |
| envelope CV at 4 ms | **0.85** | 0.26 |
| envelope CV at 50 ms | **0.56** | 0.12 |

Half the energy of a river is its bed. A fire is a quiet bed with very loud,
very short things happening on top of it, and almost every design decision
below follows from that ratio.

## What was measured, and what it decided

| Quantity | Measured across the library | What it set |
|---|---|---|
| Bed colour, 125 Hz-16 kHz, crackles gated out | five clusters (`shapes.py`) | the five `Fire` shapes, shipped as cluster centroids |
| Sub-60 Hz energy | up to **60%** of total, uncorrelated with the fire in 24 of 25 files | nothing below 60 Hz is generated; `Highpass` defaults there |
| Bed level at 125 Hz vs 2 kHz | median **−4.0** against **−14.9** dB | the roar is a low-frequency phenomenon; the shapes' tilt |
| What the crackles add to the spectrum | **+5.9 dB at 4 kHz**, +4.0 at 2 kHz, +3.8 at 8 kHz | the crackle layer's band |
| Bed envelope CV, 100 ms, per band | 0.39 … 1.28, hump at 2-4 kHz | `Flare` |
| **Neighbouring-band envelope correlation** | **0.54 … 0.91, median 0.85** | the bands flare *together*. A fire is one flame, not eight independent ones — the opposite of RiverFlow's 0.07-0.25 |
| Bed envelope spectrum, 0.1-10 Hz | **−5.7 dB/decade**, strongest frequency 0.12-2.54 Hz with no peak that survives between recordings | the flare is a filtered random walk, **not an LFO** |
| Crackle rate (1-12 kHz, 6 MAD) | 11.5 … 51.0/s, median **29** | `Crackle Rate` |
| Crackle prominence over the bed | 6.5 … 20.2 dB, median **13.2** | `Crackle Level` |
| Crackle amplitude spread | log-normal, σ = **0.30 decades (6 dB)** | `Spread` |
| Crackle decay to −10 dB | median **2.5 ms** (1.0-5.5) | `Crackle Decay` |
| Crackle spectrum, bed-subtracted | falls **~2 dB/octave** from 250 Hz, −18 dB by 16 kHz | the crackle's noise-burst filter |
| Crackle spectral **flatness** | median **0.67** (0.15-0.92) | **no resonator.** A crackle is a click, not a struck piece of wood |
| Sizzle share of the population | 0.03 … **0.57**, median 0.21 | `Sap` — the wet/dry axis, and the library's widest per-recording variable |
| Sizzle decay to −10 dB | median **19 ms** (11-30) | `Sizzle Decay` |
| Sizzle spectrum | **the same colour as a crackle** to within 2 dB per octave | the two layers differ in duration, not in tone |
| Settle rate (80-300 Hz, crackle-rejected) | 0 … 136/min, median **6/min** | `Settle Rate` |
| Settle prominence over the low bed | 13.9 … 25.7 dB, median **16.1** | `Settle Level` |
| Settle decay | 6 … 10 ms | `Settle Decay` |
| L/R correlation | −0.52 … 1.00, median 0.69 | `Width` |

## The finding the engine rests on

`bursts.py` asks how crackles arrive. Three measures, at three timescales, and
they do not agree — which is the point.

| scale | measure | Poisson | measured |
|---|---|---|---|
| 1 s | Fano factor of the count | 1.0 | **3.90** median, up to **20.1** |
| 50 ms | P(interval < 50 ms) / exponential | 1.0 | **1.03** |
| 10 ms | P(interval < 10 ms) / exponential | 1.0 | **1.97**, up to 4.0 |
| 50 ms | branching ratio | 0 | **0.02** |

Read together that is not one process:

- **at 50 ms the arrivals are exactly Poisson**, and the branching ratio of 0.02
  rules out a self-exciting cascade. A crackle does not make the next crackle
  more likely.
- **below 10 ms there are twice as many intervals as Poisson allows.** A crackle
  is not one impulse: it is a short train of one to three, inside about ten
  milliseconds.
- **at one second the variance of the count is four to twenty times the mean.**
  The *rate itself* wanders, on the same seconds-long timescale as the bed's
  flare — and once the rate is allowed to wander, the whole Fano excess is
  accounted for without anything else being bursty.

So the spawner has three levels and no cascade: **a slowly wandering rate → a
Poisson process at that instantaneous rate → each arrival is a 1-3 pulse
crack.** The wander is the same random walk that drives the bed, which is why
`Flare` moves the roar and the crackle rate together: in a real fire a flare-up
is both.

## The five bed shapes

Octave-band levels in dB relative to the bed's own total, crackles gated out by
`bed.py` (the quietest 75% of 4 ms frames), then clustered by `shapes.py`.

| | 125 | 250 | 500 | 1k | 2k | 4k | 8k | 16k | members |
|---|---|---|---|---|---|---|---|---|---|
| **Deep Blaze** | −4.6 | −5.9 | −8.7 | −12.0 | −14.7 | −14.8 | −15.1 | −19.7 | 5 |
| **Log Fire** | −2.4 | −8.0 | −12.8 | −15.0 | −16.0 | −14.4 | −11.3 | −15.9 | 5 |
| **Camp Fire** | −4.4 | −8.6 | −12.5 | −13.4 | −12.3 | −9.7 | −8.9 | −12.3 | 3 |
| **Open Flame** | −14.2 | −6.9 | −8.1 | −10.8 | −10.2 | −9.3 | −8.8 | −14.3 | 3 |
| **Stove Draught** | −9.0 | −14.9 | −20.4 | −23.0 | −22.9 | −18.3 | −11.5 | −1.3 | 1 |

A table of eight numbers is a formula, not a sample; see the suite's note on
what pure synthesis does and does not permit. Nothing here reproduces recorded
audio — the numbers say what colour to give a noise source the plugin generates
itself.

`Stove Draught` is one recording, and shipping a cluster of one is a deliberate
choice rather than an oversight: `fireplace-woodstove-with-the-lid-open` is 22
dB down at 1 kHz and peaks at 16 kHz, which no other reference comes near, and
it is the only example in the library of a fire heard through a draught rather
than in a room. Dropping it would have cost the plugin the whole stove end of
its range to make a table look tidier.

## The scripts

| Script | What it answers |
|---|---|
| `refs.py` | the library survey: colour, slope, crest, envelope, stereo |
| `lowend.py` | is a reference's low end fire, or is it the microphone? |
| `bed.py` | the roar under the crackles: its colour, its flare, its rhythm |
| `shapes.py` | which bed colours the library actually contains |
| `crackles.py` | rate, amplitude law, prominence, decay, clustering |
| `bursts.py` | Poisson, Hawkes or rate-modulated — the three timescales |
| `crackshape.py` | what one crackle is made of, and the tick/hiss split |
| `settle.py` | the rare low thump of a log collapsing |
| `grain.py`, `events.py`, `onsets.py` | shared machinery, inherited from RiverFlow |
