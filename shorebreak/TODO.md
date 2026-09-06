# ShoreBreak TODO

## Fit

- [ ] **Raise the crest factor by 3–9 dB.** The references are peakier than the
      synthesis: 34 dB for Sand and Foam against 23.7, 29 dB for Gentle Waves
      against 20.8. Suspects, in order: too many waves alive at once (the pool
      is generous and the foam decay is long, so waves overlap where real ones
      do not); the break envelope's decay being exponential where the references
      suggest something closer to two stages; and the swell bed filling gaps
      that should be quiet. Measure with `tools/analysis`, not by ear.
- [ ] **Make the steady sources actually steady.** Distant Roar measures an
      envelope variation of 0.53 against the reference's 0.10. At distance the
      individual breaks should smear into the roar; they still punch through.
      A distance-dependent smoothing of the break envelope is the obvious idea,
      and is physically defensible: what arrives from a kilometre away has been
      through a kilometre of scattering.
- [ ] Fit the remaining ten presets against their references properly. Only
      seven have been compared numerically so far.
- [ ] Harbour Lapping is 12 dB too bright at 12.5 kHz and its envelope is too
      eventful. A harbour is a boxy, close, low sound and the model is not quite
      getting the boxiness — the space may need to do more of the work.

## Model

- [ ] **The bubbles are pure sinusoids and probably still too salient.** A real
      bubble is buried in moving water; a clean decaying sine at the same
      measured graininess stands out far more. Giving each one a noisier body --
      a short filtered-noise component at its own frequency rather than only the
      pinch-off click -- would let the level come back up without them poking
      out.
- [ ] Take the hint from `AZ Low-FI Shore` further: it runs 100% wet with a big
      room, and a large space is doing a lot of the work of making it cohere.
      ShoreBreak's presets are still comparatively dry.

- [ ] **Settle the preset graininess by ear.** The measurements got the
      character into range but several presets are now grainier than their
      references. The metric cannot tell the difference between convincing
      bubbling and too much of it.
- [ ] **Couple the bubbles.** Xue et al.'s point is that bubbles in a cloud
      force each other, and that this is what produces the low modes. The
      collective mode is currently a single resonator at f0/cbrt(N); the real
      thing is a spectrum of modes (their Figure 3 shows 386, 589, 732, 1121,
      1579 Hz for one pour). A handful of resonators rather than one would be
      closer, and is still cheap.

- [ ] **Bubble pitch as a radius.** Minnaert gives f₀ ≈ 3.26 / r, so the
      parameter could read in millimetres, which is what it physically is. That
      would match the way RainyDay names its droplet sizes.
- [ ] **A second break stage.** The references' decay looks like a fast initial
      fall and a slower tail rather than one exponential. The literature's
      −7 dB/s is the tail; the first half second is steeper.
- [ ] Correlate the wash with the break that spawned it more strongly — on a
      steep shore the wash is the break, on a flat one it is a separate event.
- [ ] Shore types are a table of six tilt factors chosen by ear. They should be
      fitted against references that actually name their shore.

## Suite

- [ ] `plugin.cpp` is now duplicated three ways (~1140 lines, ~126 differing).
      With a third copy in hand the shape of what is common is finally clear:
      the CLAP lifecycle is identical and only the engine type differs. Extract
      it, templated on the engine.
- [ ] `tools/render.cpp` and `tools/guihost.cpp` are also three copies now. The
      render host's self-test is mostly plugin-agnostic; the parts that are not
      (the enum key it checks) should come from the plugin.
