# NightLife TODO

Current as of 0.1.0: 45 references, 225 segmented calls, 208 usable contours,
48 archetypes, 6 callers, 627 croaks, 8 croak types, 16 presets.

## The thing that has not been done

- [ ] **Listen to it against the references.** Every figure in `STATUS.md` is a
      measurement and the suite's own rule is that a number agreeing with a
      number proves nothing about the sound. ChirpParade 0.1.0 was 11,000 lines
      validated entirely against statistics its own analysis code produced, and
      it was wrong. Render A/B pairs — a preset beside the recording its numbers
      came from — and sit through them before anything here is called finished.
      `tools/analysis/listen.py` writes the fifteen pairs into `!dev/listen-v1/`,
      loudness-matched and at one sample rate, so the only difference left is
      the one that matters.

## Callers

- **The pitch anchor and the census disagree by 11 %, uniformly.** The table is
  anchored at each call's loudest moment; the census median is over every loud
  frame. Both are defensible and they are not the same quantity. Either the
  census should report the pitch at the level peak — which is what the engine's
  own anchoring already does — or `Pitch` should mean the median. Until one of
  them moves, `fit.py` will keep printing +11 % and it will keep being correct.
- **The segmenter measures every rendered call 10–15 % short.** Same cause as
  ChirpParade's 11–18 %: it measures the voiced portion, not the stretched
  contour. The estimator should change, not the engine.
- **The screech owl's whinny is under-resolved.** Its contours turn direction up
  to 143 times in one call; 48 terms over three seconds draws 68 of them. The
  term count is per second and that is right for the other five, but a trill is
  the case where a fixed rate per second is still the wrong budget. Choosing the
  count from the contour's own turn count rather than its duration would fix it.
- **The loon is one recording.** Eleven usable contours behind eight archetypes,
  which is less material than any other caller has by a factor of four. Two more
  loon recordings would make it a caller rather than an impression of one.
- **The performed werewolf recordings are measured and unused.** They are in the
  census as `(performed)` — 3 calls, 3.4 s median, the longest in the library —
  and give no archetypes, because the suite fits to the world. If a "creature"
  caller is ever wanted they are already measured.
- **Nothing morphs between archetypes.** `Contour` steps from one measured curve
  to the next, as in ChirpParade, and for the same reason: interpolating two
  curves that move out of phase gives a smooth glide, which is the failure this
  whole approach exists to avoid. It needs alignment first.
- **The tract is one resonance.** A wolf's vocal tract has a series of them at
  odd multiples of `c/4L`, and on the low callers the third and fifth would
  probably be audible. Two more filters.

## Chorus

- **The Fano comparison is limited by the segmenter on both sides.** A dense
  chorus merges into runs — eight frogs at the default rate produce ~968 croaks
  in a minute and the segmenter finds 40 to 83 of them. The rendered and the
  measured numbers are biased the same way, which is why they are comparable at
  all, but the agreement is weaker than the table in `STATUS.md` looks. A
  sparser test (three frogs, 25 croaks a minute) resolves the events and is what
  the `Regularity` default was actually swept on.
- **The croak table is eight recordings, not eight clusters.** Each row is the
  median of one close recording, because eight files cannot be clustered into
  archetypes that mean anything. More close-miked single frogs would let the
  croak table be built the way the contour table is.
- **There is no bullfrog.** The library's lowest measured resonance is 1529 Hz
  and its slowest pulse rate 9.8 Hz. `Bullfrog Bank` pulls Croak Pitch below
  anything measured, which is the one preset in the factory set that does.
- **The croak envelope is a one-pole attack and an exponential release.** The
  rise and fall times are measured per row, but the *shape* between them is not —
  a real croak's envelope was never fitted the way a call's level contour was.

## Insects

- **This is the weakest measurement in the plugin.** The library has no recording
  of insects on their own. What is measured is the cricket band *behind* a scops
  owl and behind two frog choruses, isolated by being narrow, high and steady
  where everything else in those files is none of the three. Three or four clean
  recordings of a summer night would replace all of it.
- **InsectSwarm 0.3.0 learned from this layer, and it can learn back.** That
  plugin's crickets were rebuilt after this one's were judged better by ear, and
  measuring both against its own nine cricket references turned up something
  this layer does not do at all: a real cricket *chirps* -- it has an echeme
  structure, on for a fraction of a second and off again, measured at 0.5 to
  10.5 Hz. NightLife's insects are a continuous trill with no chirp above it,
  which is why they measure a chirp-peak prominence of 3.0 against those
  references' 7.6 to 69.9. It sounds right as a distant bed and would not
  survive being brought close.
- **The two trill rates disagree by an octave.** 33 Hz and 49 Hz, which is
  plausibly a katydid and a cricket rather than an error — but with two
  measurements there is no way to tell.
- **The rendered band measures Q 12–13 whatever the resonator is set to.** The
  estimator smooths over 50 Hz to bridge the comb of a pulsed band, which floors
  the width it can report at about 210 Hz at 3 kHz. The references' 20.7 comes
  from 24 kHz files where the same smoother is half as many bins wide. The
  estimator needs to scale its smoother with the sample rate before this
  comparison means anything.

## Bed

- **3.9 dB rms is close to the floor for eight octave bands.** The residual sits
  where the measured curve has third-octave features — a peak at 315–400 Hz and
  a dip at 500–630 — that an octave-wide bank cannot draw. A shelving tilt
  carrying the curve's average slope, with the bank solving only the residual,
  is already on the suite's list of likely shared extractions; this is another
  plugin that wants it.
- **The correction loop is manual.** `bed.py --calibrate <render>` prints the
  corrected gains and they are pasted into the generated header, which is the
  one place in the plugin where a generated file is edited by hand. It should
  run the renderer itself and iterate to a tolerance.

## Suite

- **The Fano factor is now measured in three plugins and means three different
  things.** CrackleBlaze 3.90 (clustered), NightLife 0.30 (regular), RainyDay
  and ShoreBreak untested and spawning Poisson. It is the one statistic that
  says whether a spawner is right, and it belongs in `shared/tools/analysis`
  beside `wavio.py` rather than being written a third time.
- **The contour pipeline is now in two plugins and diverging.** `contours.py`
  here is ChirpParade's with a different window, different gates and a different
  term budget. The differences are all justified and all measured, which is
  exactly the argument for extracting the pipeline with its parameters exposed
  rather than copying it a third time.
- **The phrase scheduler with per-individual identities is in two plugins now.**
  ChirpParade's TODO predicted NightLife would want it, and it did: `Animal`,
  `Phrase` and the answering behaviour are its code with the names changed.
- **`src/plugin.cpp` is duplicated nine ways.** ~1150 lines of which ~130 differ,
  and the differences are the engine type and `syncEngineParams`.

## VST3

- [ ] **Pin the VST3 class id** with `CLAP_VST3_TUID_STRING` before anything
      reorganises the plugin ids. The wrapper hashes it from the CLAP id, which
      is stable but becomes unchangeable the moment this ships.
- [ ] **Factory presets do not reach a VST3 host's own browser.** The plugin's
      browser lists all sixteen; the host's does not. Upstream has work on this
      on its `next` branch.
