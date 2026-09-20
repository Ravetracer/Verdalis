# InsectSwarm

**Synthesised insects — a CLAP and VST3 instrument for Linux and Windows.**

InsectSwarm generates flying and calling insects: a single bee, a hive, a wasp
crossing the microphone, a mosquito that will not leave, a cicada chorus at noon,
a field of crickets after dark. It contains no samples. Every wingbeat, every
pass and every tymbal click is computed while the plugin plays, so no two takes
are alike and there is no loop point to find.

Part of the [Verdalis](https://github.com/Ravetracer/Verdalis) suite.

## A bee is half noise

The design rests on one number, and it is the first thing the reference library
of 67 field recordings says.

| | harmonic-to-noise ratio |
|---|---|
| Honeybee | **6.6 dB** |
| Mosquito | **16.0 dB** |
| a hive or a swarm | 0 – 2 dB |

A mosquito really is the thin whine it sounds like. A honeybee is very nearly as
much turbulent air as tone — which is exactly why a stack of oscillators never
sounds like one, however carefully its harmonics are set. So the engine generates
turbulence alongside the wingbeat, and *Rasp* is the ratio between them in dB,
measured against that table.

## Seven measured species

The wingbeat rate is the tone, and everything above it is a harmonic of it.

| Species | wingbeat | what the library says about it |
|---|---|---|
| Hornet | 85.7 Hz | the lowest in the library; its second harmonic is 10.7 dB *above* its fundamental |
| Dragonfly | 118.8 Hz | only 8 % of its frames are periodic at all — a clatter, not a buzz |
| Bumblebee | 143.4 Hz | second harmonic 11.7 dB up, which is why it reads as heavy rather than merely low |
| Wasp | 150.6 Hz | |
| Housefly | 193.1 Hz | |
| Honeybee | 221.2 Hz | 21 references, more than any other species |
| Mosquito | 463.7 Hz | the most harmonic thing measured |

Choosing a species sets its rate, the shelf and resonance fitted to its measured
harmonic stack, and how harmonic its buzz is to begin with. Everything else on
the panel moves that measurement rather than replacing it.

## A crowd needs no separate mechanism

A single close insect measures 8 to 13 dB harmonic-to-noise; a hive measures 0 to
2. Enough fundamentals scattered widely enough **are** noise, and nothing has to
be added to make a crowd sound like one. *Count* and *Spread* are the whole of
it: a dozen individuals at 150 cents apart render at 1.7 dB, inside the measured
hive figure. At a *Count* of one, *Spread* correctly does nothing.

## An insect is never a fixed distance away

*Wander* drifts an individual's wingbeat rate, and every species in the library
does it: 10 to 24 cents from one 85 ms frame to the next. *Roam* is the same
thing done to its level, because an insect closes on the microphone and backs off
again, and close in the inverse square law turns a few centimetres into several
decibels. At a *Count* of one it is most of what separates a fly from a held
tone; in a swarm of sixty the individual drifts are independent and largely
cancel, so the same control reads as the crowd breathing. The factory presets
carry 6 to 10 dB of it on the single insects and half a decibel on the hives.

Its excursion is bounded and its compensation is set so that the loudest moments
stay where *Swarm Level* put them, so turning it up adds the quieter moments
rather than louder ones and cannot overload a patch that was in range without it.

## A flyby is geometry

*Flyby* puts one individual on a straight trajectory past the listener. Distance
sets the level, the air absorption, the pan and the Doppler together, from the
two numbers the library measures across its ten clean passes: a **13.3 dB rise**
over a **1.37 s** pass.

The Doppler is in there because it is physically right, not because it is
audible. At the measured 3.2 m/s a pass shifts the wingbeat by 30 cents, while
the same insects' rates wander by 43 to 103 cents on their own. It is buried.
Push *Speed* past anything an insect can do and it becomes an effect.

## Cicadas and crickets are a different instrument

Neither beats its wings. A tymbal buckles and a scraper crosses a file, and what
comes out is a resonant body driven in pulses — a carrier and a pulse rate, not a
fundamental and a stack. The *Stridulate* layer is that model, and the library
separates the two sharply:

| | carrier | Q | pulses/s | duty | within-chirp duty |
|---|---|---|---|---|---|
| Cicada | 5549 Hz | **13.2** | **268** | 0.48 | **0.79** |
| Cricket | 4518 Hz | **25.8** | **36** | 0.33 | **0.27** |

Twice as sharp and seven times slower is the whole distance between a dry rattle
and a pure whistling trill. Both are factory presets.

The last column is what 0.3.0 added and it is the difference between the two
mechanisms rather than between their numbers. A scraper dragged across a file
drives the harp for most of a wing stroke; a tymbal snaps rib by rib. Modelled as
a single click into a resonator — which is what 0.2.0 did for both — the insect
is sounding for 6 % of each pulse period, and neither of those is 6 %. *Scrape*
is that fraction, and at zero it is the old single click exactly.

## A chorus is not twelve of the same insect

The measurement that found this was not a spectrum. Every spectral statistic the
stridulation layer was fitted to — carrier, bandwidth, Q, comb depth, flatness —
was inside the references' spread in 0.2.0 and still is. What was wrong was in
time:

| | references (9 cricket) | 0.2.0 | 0.3.0 |
|---|---|---|---|
| steadiness (how deep the holes are) | 0.16 – 0.85, med 0.36 | **0.08** | 0.44 |
| chirp-peak width / centre | 0.07 – 0.73, med 0.55 | **0.04** | 0.11 |

Twelve callers within ±5 % of one chirp rate drift in and out of phase *together*
and leave the chorus with holes no field has. And widening the spread alone does
not fix it, because twelve exactly periodic trains are a picket fence however far
apart the pickets stand: a lone cricket's own chirp-rate peak is 0.73 of its
centre wide, so a real caller's clock wanders. Each caller here now has one.

## Presets

Sixteen, from *Single Bee* through *Hive Wall* and *Mosquito In The Ear* to
*Cicada Noon* and *Summer Meadow*. Every one of them carries the measured numbers
for what it claims to be.

## Building and installing

```sh
./install.sh                 # configure, build, self-test, install to ~/.clap
./install.sh --vst3          # also builds and installs the VST3
```

Or manually:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build        # defaults to ~/.clap/InsectSwarm
```

Needs a CLAP SDK checkout beside the plugin (`../CLAP/clap`), X11 and Cairo for
the window, and a C++17 compiler. The suite's `README.md` has the details.

## Offline tools

```sh
./build/insectswarm-render --list                    # exercises preset discovery
./build/insectswarm-render --selftest                # 38 checks, run by install.sh
./build/insectswarm-render --all --outdir /tmp/wav   # renders every factory preset
./build/insectswarm-guihost ./build/InsectSwarm.clap "" 8   # the real window, 8 seconds
```

## Where the numbers come from

`tools/analysis/README.md` is the measurement record: what was measured, what it
decided, what it ruled out, and the three things about the fit that were got
wrong first. The reference recordings themselves are not in this repository.

## Licence

MIT, with the rest of the suite.
