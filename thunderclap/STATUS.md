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
- Each arrival is played as an N-wave with Crack-controlled fronts and a noise
  burst at the front for the fine roughness of the channel, through its own
  absorption filter.
- A rumble noise layer follows the arriving shock energy, as a band whose
  lowpass tracks Rumble Tone or the air, with width and slow drift.
- In-cloud elements radiate waves twice as long, with fading crackle and half
  again the energy (Kappus and Vernon: intracloud thunder peaks near 10 Hz
  against 50 for the ground stroke), which is the deep late swell.
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

- 44 parameters, grouped, with real-unit display and text entry both ways
  (`800 m` and `2 km` both parse for a kilometre field).
- Extensions: `params`, `audio-ports`, `note-ports`, `state`, `tail`,
  `voice-info`, `preset-load`, `gui`, `timer-support`.
- Sample-accurate event handling, with parameters synced before the events of
  each sub-block so a note in the same block as a preset load sees the preset.
- Versioned, per-parameter-id state.

**Presets**: 16, embedded in the binary and installed beside it, gains set by
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

## Not done

See `TODO.md`. The headline items: no numerical fit of presets against
individual recordings yet (the engine constants were set against the whole set
and the presets by hand and by the loudness pass), and a Windows build that has
not been run on Windows.
