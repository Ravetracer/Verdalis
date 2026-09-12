# ShoreBreak TODO

## The VST3

ShoreBreak builds and ships a VST3 for both platforms. What is left is suite-wide
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

- [ ] **Raise the crest factor by 3–9 dB.** The references are peakier than the
      synthesis: 34 dB for Sand and Foam against 23.7, 29 dB for Gentle Waves
      against 20.8. Suspects, in order: too many waves alive at once (the pool
      is generous and the foam decay is long, so waves overlap where real ones
      do not); the break envelope's decay being exponential where the references
      suggest something closer to two stages; and the swell bed filling gaps
      that should be quiet. Measure with `tools/analysis`, not by ear.
- [~] **Partly done: make the steady sources actually steady.** Distance now
      multiplies the events and smears their edges, taking Distant Roar from
      0.53 to 0.32 against a reference 0.10. Still not steady enough.
- [ ] **Old note: make the steady sources actually steady.** Distant Roar measures an
      envelope variation of 0.53 against the reference's 0.10. At distance the
      individual breaks should smear into the roar; they still punch through.
      A distance-dependent smoothing of the break envelope is the obvious idea,
      and is physically defensible: what arrives from a kilometre away has been
      through a kilometre of scattering.
- [x] ~~Fit the remaining ten presets.~~ All 17 are compared by
      tools/analysis/fit.py; 12 still have a single band 6-11 dB out.
- [ ] Harbour Lapping is 12 dB too bright at 12.5 kHz and its envelope is too
      eventful. A harbour is a boxy, close, low sound and the model is not quite
      getting the boxiness — the space may need to do more of the work.

## Model

- [ ] The sizzle peaks 0.5-0.9 s after the break in the presets, following the
      description of hearing it as a later event. The references' own high band
      peaks *with* the break, being broadband, and merely persists afterwards.
      Both readings are defensible and the difference is audible; settle it by
      ear rather than by moving the number again.
- [ ] Gentle Waves' sizzle measures a texture CV of 0.52 against the references'
      0.68-1.01 -- still the smoothest of the set.

- [ ] **Settle the preset graininess by ear.** The measurements got the
      character into range but several presets are now grainier than their
      references. The metric cannot tell the difference between convincing
      bubbling and too much of it.
- [x] ~~Couple the bubbles.~~ Three collective modes at Xue et al.'s measured
      ratios rather than one.
- [ ] **Old note: couple the bubbles.** Xue et al.'s point is that bubbles in a cloud
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
