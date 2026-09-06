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
```

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
recordings in `!dev/reference`, and all of it is built into the engine.

**Close thunder peaks at 80 to 160 Hz and keeps its top end.** Measured
against their own loudest band, the recordings of strikes a few hundred metres
away sit 6 dB down at 320 Hz, 13 at 640, 23 at 1.3 kHz, 33 at 2.6 kHz and 42
at 5 kHz. A clean N-wave of the right length gets the bottom four of those
right and is 20 dB short above 2 kHz, which is why every shock front carries a
burst of noise for the fine roughness of the channel.

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
