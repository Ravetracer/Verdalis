# InsectSwarm — planned work

## Measurement

- **Find a second hornet reference.** The row rests on one recording of 4.2
  seconds. Everything about it — the 85.7 Hz rate, the resonance sitting between
  h1 and h2, the octave-high perceived pitch — follows from that single file.
- **Find more wasp and dragonfly references.** Two and two usable respectively.
  The dragonfly row in particular claims something strong (that it is not a buzz
  at all) on thin evidence, even though the crest factor and the voiced fraction
  both agree with it.
- **Measure a swarm's inter-individual rate spread properly.** It cannot be got
  from a mixed recording, which is why `Spread` is calibrated rather than
  measured. A multi-microphone or a close-range recording of a known number of
  individuals would settle it.
- **Beetles.** The reference README asks for beetle-in-flight recordings and the
  library has none. A beetle is the one common flyer whose wing loading is
  different enough to be a separate row rather than a `Rate` offset.

## Engine

- **Measured contours instead of a parametric flyby.** The suite widened its
  "pure synthesis" rule after ChirpParade: a measured curve stored as
  coefficients is a formula, not a sample. The flyby's level trajectory is
  currently the 1/r law, which is right for a point source in free field and is
  not what the ten clean passes in the library actually trace. Extracting those
  ten envelopes and shipping their mean as coefficients is the obvious next step,
  and the same argument applies to the wingbeat's own amplitude modulation
  through a stroke.
- **`Roam` should move the pan and the tone as well as the level.** An insect
  that has moved away is quieter, duller and somewhere else, and only the first
  of those is modelled. The tone is the cheaper of the two to add — the flyby
  path already has a distance lowpass that generalises — and the pan is the one
  that would make a single close insect read as circling rather than as
  breathing.
- **Measure `Roam` against something.** It is currently the only control in the
  plugin whose size is set by ear. A recording of one insect at a known distance,
  or a stereo pair close enough to triangulate, would give the level drift a
  reference the way `Wander` has one.
- **The individual pool is per note.** Four notes each with up to 64 individuals
  is 256, and `Max Individuals` caps the total — but by dropping new individuals
  rather than by thinning existing ones, so a fifth note arrives quieter than the
  first four. A shared pool with proper stealing would be better.
- **Intrinsic click jitter for the stridulators.** The click train is exactly
  periodic, which no tymbal is. It would make the layer sound less mechanical and
  would also make `pulse.py` able to read it back.

## Shared

Three things here belong in `shared/` when a second plugin needs them, and the
suite's own notes already anticipate two of them:

- **The two-pulse stroke excitation.** A wing pushes air on the downstroke and
  again on the upstroke, and when the two are equal the odd harmonics cancel.
  Anything modelling a reciprocating source wants it, and the trap it avoids is
  the one ChirpParade's notes describe from the other direction.
- **The measure-don't-derive level normalisation.** `calibrate()` runs the real
  excitation through a real copy of the real shaper and normalises on what comes
  out. Every plugin in the suite has a level knob whose meaning drifts when a
  timbre knob moves; this is the general answer to that, and it is 40 lines.
- **The geometric flyby.** Distance, level, pan, air absorption and Doppler all
  from one r(t). SkyHowl and ThunderClap both model distance and neither moves a
  source through the field.

## Overlap with NightLife

The roadmap gives NightLife "night insects, crickets". This plugin has nine
cricket references and ships a cricket preset, and the decision taken here is
that **InsectSwarm owns the stridulation mechanism** — the tymbal, the file and
scraper, the carrier and the click rate — because that is what the references in
`!dev` measure and because it is the same model as the cicada's.

What is left for NightLife is the part this plugin has no model for: the
*arrangement* of a night chorus. Which species call at which hour, how a chorus
starts and stops, how one caller triggers its neighbours. That is a scheduler,
not a resonator, and it is much closer to ChirpParade's phrase scheduler with
per-individual identities than to anything here.
