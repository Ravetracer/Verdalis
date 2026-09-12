# ThunderClap — current status

Written 2026-09-04. See `README.md` for the design and parameter reference, and
`TODO.md` for what is still open.

## State: working and playable

A complete, native Linux CLAP instrument that synthesises thunder. Builds clean
with GCC 13 (`-Wall -Wextra`, no warnings), passes 44 self-test checks, and is
installed to `~/.clap/ThunderClap/`. A Windows build cross-compiles with MinGW
and the same Cairo-backed window.

## What is implemented

**Synthesis** (`src/dsp/`) — no samples, everything computed:

- Every flash grows a lightning channel: a random walk with direction memory
  from the strike point up to the cloud base, a horizontal run inside the
  cloud, and branches below and inside the cloud. The channel is cut into up
  to 4096 elements, dealt out in proportion to length over distance.
- Every element becomes one arrival: time from its distance, level from 1/r
  and a sin^(2·Focus) directivity, N-wave length from a small power of the
  distance, air absorption as three one-poles at the 6, 12 and 24 dB points of
  the f^1.6 law, pan from its azimuth, and a ground shadow that grows with
  distance and with Swell. Arrivals are sorted and the flash is normalised on
  the energy of its loudest tenth of a second.
- Return strokes replay the channel with per-element jitter; only the first
  stroke lights the branches.
- Each arrival is played as an N-wave with Crack-controlled fronts, through its
  own absorption filter, with a burst at the front for the fine roughness of
  the channel -- a train of steps sampled and held at the time one wrinkle
  takes to pass, not noise, so its spectrum falls at 6 dB/oct above its corner
  the way the front's does. Crack moves the roughness from 55 cm to 12 cm.
- A rumble noise layer follows the arriving shock energy, as a band whose
  lowpass tracks Rumble Tone or the air, with width and slow drift.
- In-cloud elements radiate waves twice as long, with fading crackle and half
  again the energy (Kappus and Vernon: intracloud thunder peaks near 10 Hz
  against 50 for the ground stroke), which is the deep late swell.
- Impact carries the two things about a close strike that are not linear
  acoustics: the blast the near channel throws off (Friedlander's waveform, a
  cluster of pulses across the onset) and the bend a finite-amplitude wave puts
  in its own crests, applied to the shock sum and not to the rumble. Every
  preset sets it from its own Distance.
- Up to eight landscape echoes with a fixed loop gain so the tail decays the
  same at any echo level; RainyDay's room model after them, its loop highpass
  moved down to 16 Hz; then a soft-knee compressor with automatic make-up
  before the safety clipper.
- Three trigger modes: One Shot, Gated, Storm (Poisson flashes while held).
  Variation scatters every flash's distance, height, cloud spread, tortuosity,
  branching, weight, crack, stroke count, stroke gaps and azimuth.
- Two random generators, one for events and one for continuous noise, so a
  fixed Seed reproduces regardless of idle time.

**Plugin side** (`src/plugin.cpp`):

- 46 parameters, grouped, with real-unit display and text entry both ways
  (`800 m` and `2 km` both parse for a kilometre field).
- Extensions: `params`, `audio-ports`, `note-ports`, `state`, `tail`,
  `voice-info`, `preset-load`, `gui`, `timer-support`.
- Sample-accurate event handling, with parameters synced before the events of
  each sub-block so a note in the same block as a preset load sees the preset.
- Versioned, per-parameter-id state.

**Presets**: 18, embedded in the binary and installed beside it, gains set by
`tools/analysis/loudness.py` over three seeds each.

**Window** (`src/gui/gui.cpp`): RainyDay's window in a storm palette, nine
panels, a shock-activity meter with output meter, and a lightning bolt in the
header on every flash.

## Measured

Against the 38 recordings in `!dev/reference`, a close strike renders within
about 5 dB of the close recordings in every octave band up to 2.5 kHz and
within 10 dB at 5 kHz; City Thunder, set against the user's own phone
recording of a strike, is within 2 to 4 dB from 40 Hz to 2.5 kHz and shows the
late low swell, though about 10 dB under the recording's; a 5 km render shows the several-claps-per-thunder
structure Kappus and Vernon describe; the distant renders sit in the bottom two
octaves as the far recordings do, and swell in over seconds. Details and the
method are in `tools/analysis/README.md`.

## The ground, 1.4.0

The same user said the notch trick still beat the plugin, and the reason it
does is that a static notch at a few hundred hertz with the bottom and the top
left alone is the shape of a ground reflection, which the engine did not have.
The listener stood in free air. A listener stands on ground, so every shock
arrives twice, and for a strike a few hundred metres off with the ear 1.6 m up
the first cancellation runs from 60 Hz for the channel high in the cloud to
1.6 kHz for the part near the ground, clustered through 100 to 800.

`Ground`, the 46th parameter, is that, and it is the cheapest density there is
because every arrival becomes two. On Dry Crack, 0 to 100 % takes the crest
factor over the 50 ms around the peak from 11.1 dB to 8.1 and the share of the
first 200 ms within 6 dB of the peak from 67 % to 86 %, with the body 2.5 dB
closer to the peak. The shock pool is now twice the element ceiling to hold
it, which also settles the old note about three overlapping flashes dropping
shocks. Render cost for the whole library went from 1.22 s to 1.55 s.

**It does not reproduce the deep notch, and it was never going to.** Every
element has its own delay, so the comb smears into a broad 2 to 4 dB tilt
rather than a 17 dB hole. The hole is a sound-design choice and not something
any of the recordings measure, so it ships as a preset instead: **Hollow
Strike**, a close strike through a wide notch at 400 Hz, and the one preset in
the library that is honestly a sound rather than a thunder.

## The clap, 1.3.0

A user's experiment found the rest of it: City Thunder with the distance pulled
in, the output filter set to Notch, and a DAW envelope decaying fast on the
`Highpass` knob -- a strike that arrives thin and bright with the bottom
flooding in behind it. That envelope turns out to be in the recordings.
Measured over 40 ms windows hopped by 10, the close references come in with the
30 to 120 Hz band 15 to 32 dB down and bring it up over the first 20 to 40 ms,
while the spectral centroid falls from 225 to 430 Hz to 76 to 100. Every render
started at its final centroid, near 100 Hz, with nothing left to arrive.

`Bloom`, the 45th parameter, is that. The cause is coherence: long wavelengths
need many arrivals to add up and a clap does not have many at its first
millisecond. Two consequences, both modelled -- the body radiating while the
clap gathers is smaller, so each arrival's N-wave and each blast pulse's time
constant are scaled by how much had gathered when they landed; and the sum
itself is highpassed by a corner that walks down as they gather. The clock
restarts on every return stroke. Measured on Dry Crack, the centroid now runs
306 Hz down to 117 over 20 ms with the 50 to 150 Hz band coming up 12.8 dB.

The shock front also got its own range law. It was a fixed fraction of the
wave's length, and since the wave lengthens only as the cube-tenth root of the
range, a strike at 300 m and one at 3 km came out almost equally sharp.
Molecular relaxation thickens a front far faster than that, so the rise time is
now `0.18 ms x (r/1 km)^0.9`, softened by Crack. Close strikes are now markedly
the brightest thing the plugin does.

One thing was tried and dropped, and the reason is in the source: feeding the
crest the steepening eats back through a highpass, which is what a steepening
wave does with it. Against the recordings it bought under a decibel from 640 Hz
to 5 kHz and cost 1.2 dB of crest factor for every 0.3 of its gain.

## The clap, 1.2.0

The crack was measured against six clean close-strike recordings
(`!dev/reference_new`) with the new `measure.py clap` and `measure.py impact`,
and two things were wrong with it.

The crackle at each shock front was white noise, which is flat to Nyquist. The
recordings are 15 to 25 dB quieter than that in the 1.25 to 5 kHz band, and the
excess was heard as a crackle laid over the thunder instead of the thunder's
own edge. It is now a train of steps at the scale the channel is rough on, so
it falls at 6 dB/oct above its corner exactly as the front does; the close
presets now sit within about 4 dB of the recordings in every octave to 5 kHz.

The strike was also a spray of separate spikes where the recordings are a wall:
11 to 16 dB of crest factor over the 50 ms around the peak against their 5 to
13, and 5 to 60 % of the first 200 ms within 6 dB of the peak against their 14
to 62. Finite-amplitude propagation is what flattens a real one, and that is
now in `Impact` alongside the blast. The close presets now measure 6 to 13 dB
of crest and 30 to 75 % density.

Two bugs turned up on the way, both in the blast and both live in 1.1.0:
`f.blastTauSec` was computed from `f.distanceKm` twenty lines before it was
assigned, so the blast took its length from whatever flash last used that pool
slot; and `reset()` redrew the landscape's reflectors from the running
generator when Seed was 0, so a render was only reproducible from the second
take onward. Both are fixed, and the self-test's fixed-Seed check now covers
them because Impact is no longer zero by default.

## Not done

See `TODO.md`. The headline items: no numerical fit of presets against
individual recordings yet (the engine constants were set against the whole set
and the presets by hand and by the loudness pass), and a Windows build that has
not been run on Windows.
