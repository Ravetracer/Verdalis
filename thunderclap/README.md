# ThunderClap

> Part of the **[Verdalis Plugin Suite](../README.md)** — synthesised weather,
> no samples. Built and released from the suite root.

A native Linux **CLAP** instrument that generates thunder — entirely by
synthesis. There are no samples anywhere in this project: every flash grows its
own lightning channel, and every shock wave you hear is computed from that
channel's geometry at run time. No two thunders are ever alike.

Play a note and it thunders. Hold a note in Storm mode and a storm passes.

ThunderClap is the sibling of [RainyDay](../rainyday): same build system, same
preset format, the same window. RainyDay makes rain and no thunder; ThunderClap
makes thunder and no rain. Put them on two tracks.

- 44 parameters covering the lightning channel, how its shocks are heard, the
  landscape's echoes, the room, the stereo field, a filter, the trigger
  envelope and a compressor
- 17 factory presets from a strike two hundred metres away to heat lightning on
  the horizon, measured against 38 recordings of real thunder
- Three trigger modes: one shot, gated, and a storm that keeps flashing while
  the note is held
- Exposed through CLAP preset discovery, so presets appear in the host's own
  browser
- Sample-accurate note and parameter handling, host modulation support,
  bounded CPU cost
- A plugin window drawn with X11 and Cairo: every parameter, a preset browser,
  a shock-activity meter and a lightning bolt every time a flash fires

## Build and install

Requires a C++17 compiler, CMake ≥ 3.16 and the CLAP headers.

```sh
./install.sh
```

That configures, builds, runs the self-test and installs to
`~/.clap/ThunderClap/`. Override the destination with
`THUNDERCLAP_PREFIX=/some/where ./install.sh`.

Manually, if you prefer:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCLAP_INCLUDE_DIR=/path/to/clap/include
cmake --build build
cmake --install build            # defaults to ~/.clap
```

`CLAP_INCLUDE_DIR` is auto-detected from a few common locations, including
`../CLAP/clap/include` — which is where the Verdalis suite keeps the SDK, at
the suite root beside this plugin folder. Pass it explicitly if the configure
step cannot find `clap/clap.h`.

The installed layout matters: the plugin locates its factory presets by looking
for a `presets` directory **next to its own binary**, so keep them together.

```
~/.clap/ThunderClap/ThunderClap.clap
~/.clap/ThunderClap/presets/*.thunderclap
```

### Bitwig Studio

`~/.clap` is scanned by default. After installing, restart Bitwig or rescan
under *Settings → Locations → Plug-in Locations*. ThunderClap then shows up as
an instrument (`Ravetracer`), and the factory presets are indexed through
CLAP's preset-discovery mechanism.

Drop it on an instrument track and hit a key.

### Windows

A Windows build is a nice-to-have and is cross-compiled from Linux with
MinGW. The window needs a Cairo built for Windows first, which
`cmake/build-windows-cairo.sh` does into a prefix of your choosing (it needs
`meson`; a venv works if the system Python refuses to install it):

```sh
./cmake/build-windows-cairo.sh /some/prefix
cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake \
      -DTHUNDERCLAP_WIN_CAIRO=/some/prefix
cmake --build build-win
```

The result is `build-win/ThunderClap.clap`, an x86-64 DLL with the runtime
linked in, so nothing else has to be copied beside it. Without the Cairo prefix
it builds without a window and falls back to the host's generic parameter view.

## The plugin window

ThunderClap draws its own window with raw **X11** and **Cairo** — no toolkit,
so the plugin stays one self-contained `.clap` file and needs nothing a Linux
audio machine does not already have. It is embedded in the host's window
through `CLAP_EXT_GUI` (X11 API, non-floating) and repainted from the host's
timer, at a fixed 960×740.

The layout is generated from the parameter table in `src/params.cpp`: panels
are the modules, cells are the parameters, and the help line at the bottom is
the parameter's own `tip`. Adding a parameter there puts it on screen here with
no GUI change.

| Gesture | Effect |
|---|---|
| Drag a knob up/down | Change the value |
| Shift-drag | Fine control (a fifth of the travel) |
| Double-click a knob, or right-click anything | Back to the default |
| Scroll wheel | Step the value |
| Click a selector's name | Open its list and pick a value |
| Click a selector's `◀` / `▶` arrow | Step one choice |
| Click the preset name | Open the browser |
| `◀` / `▶` next to the name | Previous / next preset |
| `SAVE` | Save the current settings as your own preset |

`SAVE` writes the current parameter values to
`~/.config/ThunderClap/presets`, creating the directory if it is not there, and
the browser picks the new preset up immediately. The name becomes the filename
with anything awkward replaced, so `My Thunder / 2` is saved as
`My_Thunder_2.thunderclap`; the file is the same plain text format as the
factory library and can be edited by hand afterwards. While the field is open
the plugin takes a keyboard grab, because an embedded plugin window is not given
the input focus by every host and CLAP has no way to ask for it — Bitwig, for
one, does not hand it over, and without the grab the field never sees a
keystroke. The grab lasts only as long as the dialog, so the host's own
shortcuts are unavailable until you press Enter or Escape or click outside.

The browser lists the factory library followed by anything in
`~/.config/ThunderClap/presets`, marked `USER`. Loading goes through the same
`clap.preset-load` path a host uses, so there is one code path for it either
way. A `*` after the preset name means a parameter has been touched since it
was loaded.

Knob moves leave the window as real CLAP events — a gesture-begin, the values,
a gesture-end — so host automation recording sees them exactly as it would a
move made in the host's own panel. The window never writes the plugin's
parameters behind the host's back.

The **ACTIVITY** panel plots how many shock waves are sounding, published by
the audio thread rather than read out of the engine, with an output meter in
dB beside it. Every flash lights a bolt in the header, grown from the flash's
own number so no two of those look alike either.

Build it out with `-DTHUNDERCLAP_BUILD_GUI=OFF`, or let CMake drop it
automatically if X11 and Cairo are missing; the plugin then falls back to the
host's generic parameter view.


## How it is synthesised

Thunder is the sound of a lightning channel: several kilometres of it, and
every metre a separate source. The channel heats to twenty thousand degrees
and expands in a few microseconds, which launches a shock wave from all of it
at once, and the listener then hears each piece when its own shock arrives —
the near pieces first and loud, the far pieces late, quiet and dull, because
the air soaks up the high end over distance. A crooked channel has stretches
side-on to the listener whose shocks arrive together and pile into a *clap*,
and stretches end-on whose shocks arrive spread out and quiet: that is the
*rumble*. Return strokes re-light the same channel a few dozen milliseconds
apart. All of it is geometry, and ThunderClap computes the geometry.

This is the model of Few (1969) and Ribner and Roy (1982), who synthesised
thunder this way and found it convincing. What follows is how the engine does
it and where the recordings corrected it.

### 1. The channel

Every flash grows a new channel. From the strike point — `Distance` away, at
an azimuth `Variation` scatters — a random walk climbs to the cloud base at
`Height`, each step remembering its direction so the channel wanders rather
than zigzags; `Tortuosity` sets how far it wanders. At the top it turns and
runs `Cloud Spread` kilometres inside the cloud, which is the part of a real
channel that is longest and the reason a distant thunder lasts twenty seconds
rather than two: the listener hears the difference in path length, and a
horizontal channel has a great deal of it. `Branching` adds side branches,
downward from the lower channel and sideways from the cloud part, each a
smaller channel of its own. The in-cloud branches are where the extra claps
come from: Kappus and Vernon counted one to five claps per thunder in a storm
5 km off, and only half the time was the first one the loudest.

The channel is then cut into elements — up to `Max Shocks` of them — dealt out
in proportion to length over distance, so the near channel is cut finely where
it is heard as separate shocks and the far channel coarsely where it is heard
as a wash. An element longer than the channel's coherence length stands for
several sources adding incoherently, so its amplitude goes as the square root
of its length and the power per metre of channel does not depend on how finely
it was cut.

### 2. What each element sends

For every element the engine works out, once, when and how its shock arrives:

- **When**: its distance over the speed of sound. The clock starts at the first
  shock anyone will hear.
- **How loud**: 1/r for spherical spreading, times a directivity — a line
  element radiates broadside and not off its ends, so its level goes as the
  sine of the angle between the element and the line of sight, raised to a
  power `Focus` sets. Focused, only the parts side-on to you are loud, and the
  claps stand out from the rumble.
- **How long**: the N-wave a shock has become by the time it arrives lengthens
  with a small power of the distance travelled, so far shocks are deeper.
  `Weight` scales it. And the channel inside the cloud radiates longer waves
  than the stroke below it: Kappus and Vernon, and Holmes before them, put
  intracloud thunder's peak near 10 Hz against 50 Hz for a cloud-to-ground
  stroke. An element's wave lengthens with its height, to twice the length at
  the cloud base, and its crackle fades by the same factor. That is the deep
  swell that arrives ten seconds after the crack and shakes the floor, and in
  the recording City Thunder is set against it is the loudest thing in the
  file, so the cloud also radiates half again as hard — the one number in the
  model that is a fit and not a law.
- **How dull**: atmospheric absorption grows as f^1.3 in decibels, about 5 dB
  per kilometre at 1 kHz in the damp air under a storm (ISO 9613 gives 4.7,
  10 and 24 dB/km at 1, 2 and 4 kHz at 20 °C and 70 % humidity), `Air
  Absorption` scaling that. No fixed filter slope fits both a strike at 300 m,
  which keeps its 4 kHz, and one at 10 km, which has lost 40 dB there, so each
  shock gets three one-poles at staggered corners — where the loss reaches 6,
  12 and 24 dB — which sit on the real curve.
- **From where**: its azimuth, mapped onto the stereo field by `Width`. A
  channel a kilometre away spans the sky and the stereo field; one fifteen
  kilometres away is a point.
- **The ground shadow**: in a normal temperature lapse sound bends upward, so
  the foot of a distant channel is never heard and a far thunder swells in
  from the cloud instead of cracking from the strike. `Swell` sets how much of
  the low channel is shadowed; the shadow grows with distance on its own.

`Scatter` spreads level and length from element to element, and the whole
flash is normalised so that its loudest tenth of a second carries a fixed
energy. `Distance` therefore changes what a thunder sounds like without
deciding whether it is audible; a mild loss per kilometre on top keeps far
ones further back.

### 3. Return strokes

`Strokes` re-light the channel, `Stroke Gap` apart, each usually quieter and a
little late. Dart leaders do not branch, so only the first stroke lights the
branches. Kappus and Vernon note that strokes 30 ms apart would put a peak at
33 Hz in the spectrum if they were exact repeats; they are not exact here — a
per-element, per-stroke jitter sees to that — and they do put the 20 ms
fluctuation into the first second of a close strike that the recordings show.

### 4. The shock wave

Each arrival is played as an **N-wave**: a pressure jump, a straight fall
through zero to the mirror value, and a jump back. Both fronts are eased over a
rise time `Crack` sets, so a soft setting is a thud and a hard one a snap. A
clean N-wave's spectrum falls at 6 dB/oct above its peak, and measured against
the close recordings that is 20 dB short above 2 kHz: the tearing of a strike a
few hundred metres off carries as much energy above a kilohertz as the wave
carries below it. So every front also carries a burst of noise, standing for
the fine roughness of the channel radiating a spray of tiny shocks. It goes
through the same air as the wave, which is why a close strike has it and a
distant one has lost it.

### 5. The rumble

Thousands of N-waves overlapping are a rumble, but with a finite element budget
they are also a crackle, so a noise layer fills in underneath. Its level
follows the energy of the shocks arriving, integrated over a fifth of a second,
and its lowpass follows either `Rumble Tone` or the air, whichever has taken
more off. It is a band, not a lowpass: a close thunder has less under 40 Hz
than at 100. `Rumble Width` spreads it and `Drift` lets it wander across the
field while the thunder plays.

### 6. Echoes and space

`Echoes` are the landscape: up to eight reflectors at distances `Echo Spread`
apart, each duller and quieter the further it stands, panned where it stands. A
little of what comes back goes round again, because a hill throws the echo of
an echo too, and the loop gain is a property of the landscape rather than of
`Echo Level`, so the tail decays the same however loud the echoes are mixed.
This is what lets a thunder trail off instead of stop.

`Space` is RainyDay's room model, unchanged but for its lowest highpass moved
down an octave so it does not take the bottom off the thunder: eight early
reflections and an 8-line feedback delay network from one physical size,
Sabine's law for the decay. It is the room you hear the storm from — a porch,
a stairwell, a hall — not the storm itself.

### 7. Dynamics

`Compress` is a stereo-linked compressor at the very end, before the safety
clipper, with a soft knee and automatic make-up: threshold and ratio move
together, from nothing to −24 dB at 4:1, and a full-scale peak still comes out
at full scale. What it does to a thunder is lift everything under the crack —
the rumble, the far claps, the echo tail — towards it, which is how a thunder
heard through a phone that was already limiting on the crack sounds: dense,
and the ground keeps shaking after the sky has stopped. `Comp Attack` slow lets
the first snap through; `Comp Release` fast pumps with the claps. The ratio is
capped at 4:1 on purpose, because a hard ratio flattens the decay of the tail
and a thunder that never quite ends is worse than one never compressed.

### 8. Triggering

`Mode` decides what a note means. **One Shot** plays the flash out whatever
the note does. **Gated** fades what has not yet arrived when the note is let
go, over `Release`. **Storm** keeps lighting flashes at `Storm Rate` while the
note is held, as a Poisson process — never on a grid — and each flash draws
its own distance, height, direction and stroke count from `Variation`, so a
held note is a storm and not a loop. `Attack` fades the shocks in as they
arrive and softens a first crack. `Velocity to Distance` puts soft notes
further away, up to eight times as far, and `Velocity to Level` does the
obvious.

### 9. Variation

Every flash grows a new channel, and `Variation` scatters the settings it
grows from: distance, height, cloud spread, tortuosity, branching, weight,
crack, the number of strokes, their gaps, and the azimuth the strike stands at.
At 0 % every flash is a fresh channel with the same statistics; at 100 % the
same preset gives you a strike overhead and a rumble over the hills on
successive keys. `Random Seed` at 0 never repeats; any other value renders the
same thunder for the same notes every time, whatever the plugin was doing
before them — the noise and the geometry come from separate generators so
idling cannot move the sequence along.

### 10. Measured against recordings

The engine's constants were set against 38 recordings of real thunder and
three papers, with `tools/analysis/measure.py` computing the same figures for
a recording and a render; its README lists what came out of it. In short: a
close strike peaks at 80 to 160 Hz and is 42 dB down at 5 kHz; a distant one
is nothing but the bottom two octaves; the attack is 20 ms, the clap a second,
the rumble ten; and distant thunder takes seconds to swell in. The factory
presets have their `Output Gain` set by `tools/analysis/loudness.py`, which
renders each over several seeds and lands the loudest peak at -4 dBFS, backing
off a third of a decibel per kilometre of distance, in a few steps because the
compressor makes the level a non-linear function of the gain.

## Parameters

### Strike

| Parameter | Range | What it does |
|---|---|---|
| Distance | 0.2 – 20 km | How far away the lightning strikes; level is compensated |
| Height | 1 – 10 km | Height of the cloud base the channel comes down from |
| Cloud Spread | 0 – 20 km | Length of channel inside the cloud; stretches the thunder out |
| Tortuosity | 0 – 100 % | How crooked the channel is |
| Branching | 0 – 100 % | Side branches, below the cloud and inside it |
| Strokes | 1 – 8 | Return strokes down the same channel |
| Stroke Gap | 5 – 500 ms | Time between return strokes |
| Variation | 0 – 100 % | How far each flash may wander from these settings |

### Sound

| Parameter | Range | What it does |
|---|---|---|
| Crack | 0 – 100 % | Sharpness of the shock fronts and their noise burst |
| Weight | 0 – 100 % | Length of each shock wave; how deep the thunder sits |
| Swell | 0 – 100 % | How much of the low channel is in the ground shadow |
| Rumble | 0 – 100 % | Level of the noise floor that follows the arrivals |
| Rumble Tone | 0 – 100 % | Its lowpass corner, 60 Hz to 1.5 kHz |
| Air Absorption | 0 – 100 % | Humidity: high-end loss per kilometre |
| Scatter | 0 – 100 % | Random spread of level and length per shock |
| Focus | 0 – 100 % | Directivity of the channel elements |

### Impact

| Parameter | Range | What it does |
|---|---|---|
| Impact | 0 – 100 % | The near channel's blast: a low, hard slam under the crack |

Every element of the channel radiates its own N-wave, but the near section of
a return stroke also expands as one body, and what that sends out is a blast
rather than an N-wave: a near-instant jump to peak overpressure, a decay back
through zero, and a longer, shallower negative phase — Friedlander's waveform,
whose spectrum peaks at 1/(2πT). It is not one pulse but a short cluster of
them, taken from the loudest arrival in each slice of the first 350 ms, so the
energy lands across the onset instead of on a single sample; one pulse alone
buys a tall peak and almost nothing in the band that is supposed to hit.

This is the part of a close thunder that arrives as a slam rather than a tear,
and it fills the 40 to 150 Hz that the reference recordings put at the top of
the spectrum through the first 300 ms — a band the elements' own N-waves, each
one short and each arriving at its own time, cannot fill between them however
many there are. It defaults to 0, which is the model without it.

Two presets use it. Impact carries a lot of level, so both give back output
gain to keep a hard flash off the clipper: the loudest of twelve seeds peaks
at -0.5 dBFS on City Thunder and -0.4 dBFS on Heavy Impact. Turning Impact up
on a preset that was matched without it will want the same treatment.

### Stereo

| Parameter | Range | What it does |
|---|---|---|
| Width | 0 – 100 % | How the channel's spread across the sky maps onto stereo |
| Pan | −100 – +100 % | Direction of the strike |
| Rumble Width | 0 – 100 % | Stereo spread of the rumble |
| Drift | 0 – 100 % | Slow wander of the rumble while a thunder plays |

### Echoes and Space

| Parameter | Range | What it does |
|---|---|---|
| Echo Level | −inf – +6 dB | Level of the landscape's echoes |
| Echo Count | 0 – 8 | How many reflectors |
| Echo Spread | 0.1 – 6 s | Delay of the furthest reflector |
| Echo Damping | 0 – 100 % | High-end loss per echo |
| Space Amount | 0 – 100 % | Mix of the room you hear it from |
| Space Size | 0 – 100 % | Room dimension, 3 m to 90 m |
| Space Damping | 0 – 100 % | Absorption of its surfaces |

### Filter

State-variable filter across the whole output: `Filter Type`
(Lowpass/Bandpass/Highpass/Notch), `Filter Cutoff` (20 Hz – 20 kHz),
`Filter Resonance` and `Filter Key Track`. A wide-open lowpass is bypassed
outright. Alongside it, `Highpass` (20 Hz – 2 kHz) is a permanent 12 dB/oct
rolloff, bypassed at the far left — thunder has a great deal under 40 Hz, and
not every mix wants it.

### Envelope

`Mode` (One Shot / Gated / Storm), `Attack`, `Release`, `Storm Rate`
(1 – 60 flashes per minute), `Velocity to Level`, `Velocity to Distance`.

### Dynamics

| Parameter | Range | What it does |
|---|---|---|
| Compress | 0 – 100 % | Threshold 0 to −24 dB and ratio 1:1 to 4:1 together, level made up |
| Comp Attack | 0.1 – 100 ms | How fast a crack is caught |
| Comp Release | 10 – 3000 ms | How fast it lets go |

### System

- `Output Gain` — final level, and how the factory presets are loudness matched
- `Max Shocks` — channel elements per flash (128 – 4096). This is the CPU
  dial and the detail dial: a flash is generated on the audio thread when it
  fires, and each element is one shock to play.
- `Random Seed` — `0` never repeats; any other value renders identically every
  time.

## Presets

| Preset | Character |
|---|---|
| Overhead Crack | A strike two hundred metres away: a short, violent tear |
| Close Strike | A few hundred metres off: crack, then a long rolling decay |
| Heavy Impact | Close enough to arrive as one blow: the blast lands first, dark, and it is over in seconds |
| Dry Crack | One stroke, small channel, hardly any rumble or echo |
| Single Bolt | One clean bolt a kilometre off, one clap, an honest decay |
| City Thunder | A phone's thunder: the blast hits, claps wander the roofs, a deep late swell |
| Rolling Thunder | Crooked, branching, several claps, twenty seconds of roll |
| Mountain Echoes | A strike in a valley; the echoes outlast the thunder |
| Cloud Crawler | Intracloud lightning; nothing reaches the ground, the sky rolls |
| Deep Sub | Long shocks, heavy rumble, nothing above 400 Hz |
| Gated Rumble | Gated: the thunder lasts as long as the key does |
| Distant Rumble | Ten kilometres off: no crack, a slow deep roll |
| Far Horizon | Twenty kilometres: very deep, very slow, more felt than heard |
| Summer Storm | Storm mode: flashes every ten seconds or so, none alike |
| Storm Front | Storm mode: every few seconds, close and violent |
| Heat Lightning | Storm mode: the odd murmur from a storm too far to see |
| Under The Rain | Storm mode, made to sit under RainyDay's Storm Front |

### Preset format

Presets are plain text, in real-world units, and are read both from disk and
from copies embedded in the plugin binary:

```ini
name = Rolling Thunder
description = A crooked, branching channel a few kilometres away.
features = ambient, sfx, evolving

distance = 2.5         # km
cloud_spread = 8       # km
strokes = 4
stroke_gap = 60        # ms
crack = 0.5            # 0..1 ratios
mode = One Shot        # by name
```

Unknown keys are ignored and missing keys keep their current value, so
hand-editing is safe. Drop your own `.thunderclap` files into
`~/.config/ThunderClap/presets/` and they are indexed as user content — the
directory is only declared to the host if it already exists, so create it
yourself first, or press `SAVE` once.

## Verification

The repo ships a small CLAP host used to test the plugin without a DAW. It
drives the real preset-discovery factory the way a host does.

```sh
./build/thunderclap-render --selftest                   # 44 host-contract checks
./build/thunderclap-render --list                       # walk preset discovery
./build/thunderclap-render --preset close_strike --out /tmp/t.wav --tail 15
./build/thunderclap-render --all --outdir /tmp/thunder  # render the whole library
./build/thunderclap-render --preset storm_front --seconds 30 --tail 10 \
                           --param "Random Seed=7" --out /tmp/storm.wav
```

A thunder plays itself out after the note, so `--tail` is where it lives; the
default is a one-second note and a fourteen-second tail. `--param` accepts
parameter names or numeric ids, with values in display units
(`"Distance=800 m"`, `"Filter Cutoff=2.5k"`), routed through the plugin's own
`text_to_value`.

The self-test covers parameter metadata, text round-tripping, state
save/load/restore equality, garbage-state rejection, parameter clamping, all
parameters at their extremes, odd block sizes, silence before the first note,
seed reproducibility after reset, preset write/read agreement, and
activate/deactivate cycles.

`./build/thunderclap-guihost build/ThunderClap.clap presets/close_strike.thunderclap 20 --note`
opens the window in a plain X11 frame and plays one note, for photographing it.

## Layout

```
src/plugin.cpp              CLAP entry, extensions, parameters, state, events
src/preset_provider.cpp     CLAP preset-discovery factory
src/preset.cpp              preset text parser, path resolution
src/params.cpp              the parameter table, its tips and unit conversions
src/gui/gui.cpp             the plugin window: X11/Win32, Cairo, layout, interaction
src/dsp/thunder_engine.*    the channel, its arrivals, the shocks, rumble, echoes
src/dsp/dynamics.h          the output compressor
src/dsp/filters.h           state-variable and one-pole filters
src/dsp/reverb.h            delay line, allpass, the room (early reflections + 8-line tank)
src/dsp/rng.h               xoshiro128+, uniform/Gaussian/exponential draws
src/dsp/adsr.h              the envelope
src/dsp/fastmath.h          fast sine, decay coefficients
presets/*.thunderclap       the factory library, also embedded at build time
tools/render.cpp            the offline verification host
tools/guihost.cpp           opens the window without a DAW
tools/fithost.cpp           renders presets back to back for a fitting loop
tools/analysis/             measures recordings and renders; sets preset gains
```

`!dev/` is not tracked. It holds local reference material used while working on
the plugin — thunder recordings and the papers — none of which is ours to
redistribute.

## References

- Few, A. A., *Power spectrum of thunder*, J. Geophys. Res. 74, 1969.
- Ribner, H. S. and Roy, D., *Acoustics of thunder: a quasilinear model for
  tortuous lightning*, J. Acoust. Soc. Am. 72, 1982.
- Kappus, M. E. and Vernon, F. L., *Acoustic signature of thunder from seismic
  records*, J. Geophys. Res. 96, 1991.
- Guo et al., *Study and analysis of the thunder source location error based on
  acoustic ray-tracing*, Remote Sensing 16, 2024.
- Rusz et al., *Locating thunder source using a large-aperture micro-barometer
  array*, Front. Earth Sci. 9, 2021.

## Notes and limits

- Linux/x86-64, tested with GCC 13. State is stored little-endian.
- 8 voices, 12 flashes and 4096 shocks in shared pools, so held-note count
  cannot multiply the CPU cost without bound. A flash's channel is grown on the
  audio thread when it fires: a few hundred microseconds at 2048 elements.
- Host parameter modulation (`CLAP_EVENT_PARAM_MOD`) is supported globally.
  Per-note modulation and note expressions are ignored.
- Parameters read at the moment a flash fires decide that flash; changing
  Distance while a thunder rolls changes the next one.
- Deliberately built without `-ffast-math`: on x86 GCC that links
  `crtfastmath.o`, which would flip FTZ/DAZ for the entire host process.
- No rain, no wind — this is thunder only. RainyDay does the rain.

## License

MIT. See `LICENSE`.

ThunderClap contains no samples and no third-party code. The only external
dependency is the CLAP headers, which are MIT licensed, plus X11 and Cairo for
the plugin window.
