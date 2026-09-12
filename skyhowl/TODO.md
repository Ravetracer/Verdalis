# SkyHowl TODO

## The VST3

SkyHowl builds and ships a VST3 for both platforms. What is left is suite-wide
rather than this plugin's:

- [ ] **Pin the VST3 class id.** The wrapper hashes the TUID from the CLAP id.
      It is stable, but it is now released, so it can never change -- set it
      explicitly with `CLAP_VST3_TUID_STRING` before anything touches plugin ids.
- [ ] **Factory presets do not reach a VST3 host's own browser.** The plugin's
      browser lists them all; the host's does not. clap-wrapper's `next` branch
      has *preset discovery: let wrapped formats browse a CLAP's presets*.
      Revisit when that releases.
- [ ] **The VST3 3.8 build break is not reported upstream.** clap-wrapper 0.16.0
      does not compile against VST 3.8, which renamed the `DEFINE_INTERFACES`
      macro parameter to `_iid` while adding `FObject::iid`. Six lines, carried
      in `shared/patches/clap-wrapper-vst3-sdk-3.8.patch`.


## Fit

- [ ] **Make the howl swoop as far as the references.** The tonal presets swoop
      a median 0.28 of an octave against the references' 0.72. The shedding
      frequency follows a wind speed smoothed with a fixed 0.65 s time constant
      — physically right in principle, since an obstacle sees the flow averaged
      over the eddies that envelop it, but the constant is a guess and is
      probably too long. It should be derived from the obstacle size and the
      turbulence length scale, which are both already in the engine.
- [ ] **`Far Away` is 23 dB over its reference at 250 Hz.** That reference has a
      26 dB notch between two adjacent third-octaves — 125 Hz at −12, 250 Hz at
      −38 — with a separate quiet broadband plateau above it. The model produces
      a monotone bed and cannot make the notch. Worth deciding whether the notch
      is a property of distant wind or of that one recording before chasing it.
- [ ] **`Between Houses` is 14 dB short at 4 kHz.** A funnelled street wind is
      the one place where the space model may need to do more of the work than
      the bed; the reference is much flatter across the top than the synthesis.
- [ ] Fit the terrain table's *spectral* tilts. The gustiness factors are
      derived from roughness lengths and are defensible; the `tiltAdd`, `buffet`
      and `hiss` multipliers beside them were chosen by ear. They should be
      fitted against references that actually name their terrain.
- [ ] Fit the foliage table the same way. The band and the resolvable rate are
      measured, but the spread between the eight types is voiced from the two
      ends of the library that are unambiguous — dry leaves at a flux variation
      of 0.80 and a merged conifer-like hiss at 0.20.

## Model

- [ ] **The library's broadband centroid does not rise with level and the model
      says it should.** Median correlation −0.10, positive in only 27 of 63
      recordings. The bed follows U³ and the buffet only U², so a gust should
      brighten. The recordings that darken are the ones with the most energy
      below 50 Hz, which points at microphone pseudo-sound and branch drag
      rather than radiated wind — but that is a hypothesis, not a measurement.
      Either confirm it on the full-bandwidth `.wav` material alone, or the
      buffet needs a steeper law than U².
- [ ] **Let the played key set the howl pitch.** The obvious musical move for an
      instrument whose whole point is a pitched tone that tracks a control
      signal, and the one thing the parameter set does not currently offer:
      `Filter Key Track` follows the suite convention, but nothing ties the note
      to the shedding frequency. It wants a parameter, not silent behaviour, and
      the HOWL panel has a spare cell.
- [ ] **Correlate the rustle with the gust that caused it.** A leaf is struck by
      the gust that reaches it, and the engine already pans gusts, but leaves
      are panned independently. On a wide image the rustle should move with the
      gust across the field.
- [ ] **Two-stage gusts.** A real gust front has a sharp leading edge and a long
      wake, and `Gust Shape` can produce either but not both at once. The
      references' median rise/fall ratio of 1.01 says the *envelope* is
      symmetric, which is why the default is symmetric, but the spectrum may not
      be: the leading edge could be brighter than the tail.
- [ ] **Vortex shedding locks.** At a critical wind speed a shedding frequency
      captures a nearby structural resonance and holds it over a range of speeds
      instead of sliding through — which is exactly the sound of a wire howling
      at one note through a changing wind. Modelling the lock-in would give
      `Howl Track` a non-linear middle instead of a straight line.
- [ ] Reynolds number. St ≈ 0.2 holds over 300 < Re < 2×10⁵ and departs outside
      it, which for a sub-millimetre obstacle in a light wind is inside the
      plugin's range. The pitch of the smallest obstacles is therefore slightly
      wrong at low wind speeds.

## Suite

- [ ] `plugin.cpp` is now duplicated four ways (~1140 lines, ~130 differing).
      With a fourth copy in hand the shape of what is common has not changed:
      the CLAP lifecycle is identical and only the engine type differs. Extract
      it, templated on the engine.
- [ ] `tools/render.cpp` and `tools/guihost.cpp` are also four copies. The
      render host's self-test is almost entirely plugin-agnostic; the one part
      that is not — the enum key it checks survives a preset round trip — should
      come from the plugin rather than being edited into each copy. It was the
      only self-test failure when this plugin was first built.
- [ ] The aeolian resonator bank is the third variation on "a pool of tuned
      resonators excited by noise" in the suite, after RainyDay's droplets and
      ShoreBreak's bubbles. Unlike those two it is *not* a bubble, so it does
      not belong in the shared bubble resonator the suite roadmap anticipates —
      but the pooling, the pan spread and the size-spread-in-octaves are the
      same code three times.
