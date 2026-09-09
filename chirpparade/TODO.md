# ChirpParade TODO

Current as of 0.5.0: 88 references, 6991 segmented syllables, 5749 usable
(82 %), 72 archetypes (8 per species), 9 species, 19 presets.

## The corvids were removed

Recorded here because `src/dsp/chirp_engine.cpp` points at it, and because it is
the largest open question in the plugin rather than a closed one.

Crow and Raven were modelled in 0.2.0, given measured partial balance in 0.3.0,
and refitted in 0.4.0 against twelve new corvid recordings that took Raven from
6 measured syllables to 146. They still did not sound like corvids, so they are
gone. **The recordings stay in the reference library and stay in its census** —
`species.py` measures them, `contours.py` extracts no archetypes for them.

They failed on three things, each of which is its own entry below:

- the valve saturates near five harmonics and a corvid needs twelve;
- `Rasp` is random per-cycle jitter, and corvid roughness has *structure* —
  subharmonics and period doubling — which this engine renders as a rough tone;
- the roughness statistic cannot tell a rough tone from a noisy one anyway, so
  the fit reported success while the ear did not.

Fixing the first two is what would bring them back. Nothing else in the plugin
needs them, so this is not blocking — but the corvid recordings are already
fetched and measured, so the moment a second oscillator exists there is a real
test waiting for it.

## What 0.5.0's listening pass found

Two defects the measurements did not catch, both found by ear. Recorded because
the second one is a trap that will spring again.

- **Flat archetypes are beeps.** 9 of the 72 archetypes travel less than 0.5
  octaves; at 0.4.0 it was 5 of 64, and those sat at index 7 where
  `contour = 0.5` never reached them. At 0.5.0 they landed on Sparrow[4],
  Crane[4] and Warbler[3] -- exactly where the presets point. Fired in sequence
  a motionless contour is a machine gun, and fired fast at varying pitch it is a
  dialling phone; neither is a bird. **The fix applied was per-preset**, moving
  four presets off those indices. **The real fix is a `MIN_PATH_OCT` gate in
  `usable()`**, the twin of the `MAX_PATH_OCT_PER_SEC` already there: a syllable
  that does not move is not worth an archetype slot. It is not done because it
  regenerates the table and would disturb the fifteen presets that this release
  improved -- it wants its own listening pass.
- **Fixed at 0.5.0: the archetype's own duration now drives the syllable.**
  `Contour::durationSec` was measured and stored for every archetype from the
  start and never read; the length came from the species median alone, so every
  curve was stretched to the same target. `chirp_engine.cpp` now builds the
  length from `durationSec / kLibraryLengthSec` scaled by `Length`, and the
  phrase advances by the slot the syllable actually took, so a curve longer than
  the nominal interval pushes the next syllable out instead of being truncated.
  At a species' median archetype this is identical to the old multiplier, so the
  change is entirely in the spread around it. Nightingale Song's duration spread
  went from 5.1 to 10.2 (p90/p10 against the reference's 4.9).

  It also removed `mSpeciesLengthMul` entirely, which is what silently rescaled
  every preset's `Length` whenever a species' measured median moved -- the
  second of the two defects the 0.5.0 listening pass found. `SpeciesTraits::
  lengthSec` is now documentation and an analysis target only.

  **Still open from it**: `fit.py --species` measures every species' length
  11-18 % short. The bias is uniform across all nine, so it is the segmenter
  measuring the voiced portion rather than the full stretched contour, not the
  engine -- but the targets in `fit.py` are archetype durations and the two are
  not the same quantity. Either the target or the estimator should change.

- **`--param` takes the *displayed* value, not the raw one.** `Contour` is a
  percentage, so `--param contour=0.5` means half a percent and selects
  archetype 0. It cost two wrong diagnoses in one session, once in a hand render
  and once inside `fit.py --species`, where it silently rendered the same
  archetype eight times and produced a table that looked like an engine
  regression. The same shape of trap as `randomseed` against `seed`, already in
  the suite's CLAUDE.md.

## Contours

- **The tonality gate sweep needs re-running.** The table in `contours.py` that
  justifies `MIN_TONALITY_DB = 0.0` was swept against the *58-recording*
  library, and two of its four columns are Crow counts for a species that no
  longer exists. The finding it records may well still hold — the gate selects
  for tonality, so contours keep improving below 0 dB while the harmonic comb
  share falls away, which is why separate gates for the contour and for the
  partial measurement are probably the right answer — but none of the numbers
  are current. Re-sweep against the 98 before acting on it.
- **Choosing the archetype by duration is still worth doing.** Taking
  `lengthSec` from the archetype medians rather than from all measured syllables
  fixed the symptom at 0.5.0 -- every species now renders within 5 % of its
  stated length -- but the underlying mismatch is only hidden. A user who turns
  `Length` well away from a species' default still stretches a short curve into
  separate notes. Picking the archetype partly by how close its own duration is
  to `Length` would fix it properly.
- **`Partials` trades timbre evolution against harmonic count.** Turning it up
  makes the balance drift across a syllable approach the references' measured
  4.4 dB — a sparrow goes from 1.0 to 3.2 dB — while dropping the harmonic count,
  because the archetype's spectrum is what that one syllable actually had and
  the gate selected the cleanest ones. A wider gate is the real fix; see the
  first entry.
- **One preset ships below the 0.6 default and nobody has re-checked why.**
  `jungle_screech` at 0.30. (`hollow_tree` was the other; it was removed at
  0.5.0 -- a drumming woodpecker layered under a songbird did not read as one
  bird, or as any bird.)
- **Nothing morphs between archetypes.** `Contour` steps from one measured curve
  to the next. Interpolating the coefficients would give a continuous control —
  but naively, since two contours that zig-zag out of phase interpolate to a
  smooth glide, which is the exact failure this whole rewrite was about. It
  needs alignment first (dynamic time warping on the curves, then interpolate),
  which is real work.
- **Cleaning the references beat every other lever, and there is more of it to
  do.** Isolating each recording to a single bird by hand took the gate's pass
  rate from 45 % to 82 % and the usable contours from 3567 to 5749 -- out of
  *fewer* files. No fitting change has ever come close. The remaining 18 % is
  still mostly files where the tracker follows two birds at once.
- **Level and pitch are fitted independently.** They are not independent in a
  bird: the pitch tends to peak where the level does. Fitting them jointly, or
  storing their correlation, would let `Variation` perturb them in a way that
  stays plausible instead of drifting apart.
- **Six partials is where the measurement stops, not the bird.** Above the sixth
  the engine has only the valve, so a syllable with real energy at the eighth or
  tenth harmonic gets a synthetic version of it. Storing more is cheap; whether
  the measurement is trustworthy that high is the question.
- **`Detail` is a one-pole.** It smooths the contour as it is read, which is
  cheap and does the right thing perceptually, but it also delays it. A
  zero-phase smoother over the table at spawn would be more honest and costs
  nothing at audio rate.

## Reference library

- **17 files are grouped nowhere and contribute nothing.** They were measured at
  0.5.0 and left out deliberately, for three different reasons.
- **Three species are one recording short of existing.** Each has a genuinely
  distinct measured character and too little of it to cluster eight archetypes
  from one individual, which would give a species whose `Contour` knob does
  nothing:
  | candidate | usable contours | files | character |
  |---|---|---|---|
  | Skylark | 386 | 1 | 4.2 kHz, 37 ms, 836 syllables/min -- nothing else is that short or that fast |
  | Cuckoo | 29 | 1 | 643 Hz, 264 ms, one harmonic, HNR 16.7 -- low *and* pure, which no species is |
  | Lapwing | 13 | 1 | 3.9 kHz with 4 harmonics and −25 dB roughness -- high, rich and rough at once |
  Two or three more recordings of any of them and it is worth an enum slot.
  Appending is safe; see the note in `contours.py`'s `SPECIES_ORDER`.
- **The fowl scatter and stay out.** Three roosters, three turkeys and the
  pheasant total 37 usable contours between them, with a harmonic-comb share of
  0.43 to 0.66, and they land nearest three different existing species. Only the
  pheasant was placed. A Fowl species needs real material, not these.
- **`eurasian-wren-close-clean.wav` is 178 good contours with no home.** 98 %
  pass rate, but 5.4 kHz at −19.7 dB roughness sits 2.1 from its nearest
  neighbour -- high and rough, where the high species are all clean. It is the
  best unused file in the library.
- **Four files measure something other than the bird**, and were left as they
  are rather than fixed: `little_owl.wav` reads a 9494 Hz median where a little
  owl calls near 700, `woodpecker.wav` is drumming rather than voice and belongs
  in the `(drumming)` group, `egyptian-and-grayleg-goose.wav` has two species in
  it (HNR −0.5, comb share 0.48), and `eurasian-wren-alarm-call.wav` passes only
  21 %.
- **Goose is now the thinnest species in the table.** 5 files, 123 syllables,
  63 usable -- a 51 % pass rate, the worst of any group, against a library
  average of 82 %. Its eight archetypes come from less material than any other
  species'. More waterfowl is worth more here than anywhere else.

## Voice

- **The valve saturates around five harmonics.** The noisiest references measure
  twelve. Closing further is not the answer — it aliases. What is probably
  needed is a second oscillator: a bird has two syringeal sides, controlled
  independently, which is how some species produce two notes at once and the
  most likely route back to the corvids.
- **Roughness may be a limit of the statistic rather than of the engine.** A
  harmonic stack is spectrally flat whether or not any noise is present, and
  spectral flatness cannot tell the two apart — which is how the corvids passed
  their fit and failed the ear. Deciding it needs a better measure, probably a
  harmonic-to-noise ratio on a resolved partial rather than band flatness. Until
  there is one, no roughness figure in this plugin means much.
- **`Rasp` is per-cycle closure jitter only.** Corvid roughness has structure —
  subharmonics and period doubling — that random jitter does not reproduce.
- **The tract is one resonance.** A trachea has a series at odd multiples of
  `c/4L`. The third and fifth would cost two filters and are probably audible on
  the low, harmonic-rich voices.

## Fit

- **Screech measures 51 % low on pitch.** Its archetypes swing so far that a
  fundamental estimate over the whole syllable is not a meaningful quantity.
  Estimating the pitch where the syllable is loudest, as the engine's own
  anchoring already does, would fix the measurement. (Budgie had the same
  reading at 65 % low and no longer does: the cause there was a mismatched
  `lengthSec`, not the width of its contours.)
- **Screech does not fit and cannot.** It has no reference of its own and
  borrows the eight highest-path contours in the library by construction. It is
  kept as an effect, not a bird.
- **Spectral flatness is a poor fit target for this engine.** These contours
  sweep at hundreds of octaves a second, and a pure sine doing that smears
  across a 21 ms analysis window and reads as rough. The `rough` tolerance in
  `fit.py` is 6 dB for that reason, and the figure carries the contour's motion
  as well as the breath. `single_chirp` is the one preset that misses its own
  stated targets, and this is why.
- **No preset was fitted band-by-band against a specific recording.** The fit is
  per quantity rather than spectral, unlike SkyHowl's third-octave comparison.

## Structure

- **`Motif` is a fixed interval per syllable.** Real song has figures, not ramps.
  A short pitch sequence per phrase — even four steps — would be a large gain.
- **Phrase-level contours are not measured.** The syllable contours are; how a
  species' *phrase* moves is not, and it could be extracted the same way.
- **The drum has no body size.** `Knock` sets the first mode and the second is a
  fixed 2.3× above it. Whether a branch is solid or hollow changes that ratio,
  and it is what separates a live tree from a dead one.
- **Distance is inverse-distance plus air absorption, nothing else.** No ground
  reflection, and birds are usually above the listener.

## Rejected, with the reason recorded

- **Mel spectrograms with Griffin-Lim** (SoundPlot). 92.9 ms analysis window at
  librosa's defaults, Griffin-Lim's worst case is exactly this material, and its
  own metrics report a negative SNR. Also the wrong side of the suite's line: a
  mel spectrogram at usable resolution is the recording with the phase removed.
- **pYIN for the pitch track.** Swept over three frame lengths and three
  confidence gates; it discards 80 % of the contour path and cuts the peak slew
  from 424 to 16 oct/s. Its Viterbi smoothing assumes slowly-varying pitch.
  `setup-venv.sh` still installs librosa, because its spectral features may yet
  be useful for choosing archetypes that differ in timbre as well as in shape.

## Verification

- **Listen, properly, against the references.** The numbers are in place; the
  ear is the gate, and 0.5.0 is the release where that matters most -- every
  measured quantity improved at once, which is exactly what 0.1.0's statistics
  did before it turned out not to sound like a bird. `!dev/listen-v6/` holds
  old-and-new pairs of all 19 presets at a pinned seed, plus a `Contour` sweep
  on Piper. Use `contours.py --wav` for reference/resynthesis pairs.
- **Run `clap-validator`**, which needs building in `CLAP/` first.
- **Look for aliasing at 96 kHz and at high `Voice`.** The hinge width is
  derived, not measured.

## Documentation drift

`STATUS.md` was brought current at 0.5.0. Three places still carry figures from
before the 0.4.0 widening, all of them two library revisions stale:

- `tools/analysis/README.md`: 58 recordings, 4268 syllables, 1641 usable, "71
  archetypes, 9656 floats, 38 KB", 40 terms. The table ships **72 archetypes,
  13824 floats, 54 KB**, at 96 pitch terms. Its tonality-gate sweep is the same
  pre-widening table as the one in `contours.py`.
- `docs/manual.md`: "1641 syllables passed the quality gate; 67 became
  archetypes." It also predates `Piper`.
- `docs/website.md`: lists the eight species without `Piper`.
- `presets/jungle_screech.chirpparade`'s description still claims Screech spans
  "7.5 to 10.3 octaves of path". It spans 16.7 to 24.1.

## Suite

- **The contour extractor belongs in `shared/tools`.** The suite has just widened
  its rule to allow measured tables, and RainyDay's droplet resonances,
  ShoreBreak's breaker envelopes and ThunderClap's N-wave shapes are all
  parametric where a measured curve would be truer. The pipeline here —
  segment, track, fit a cosine series, cluster, keep medoids, emit a header — is
  general.
- **The one-sided valve belongs in `shared/dsp`.** Any plugin modelling a valve
  needs it, and the odd-harmonic trap it avoids is easy to fall into twice.
- **The phrase scheduler with per-individual identities is reusable.** RainyDay,
  ShoreBreak and CrackleBlaze spawn events at a rate; this is the first that
  models individuals, and NightLife will want exactly that.
- **`src/plugin.cpp` is now duplicated five ways.** ~1140 lines of which ~128
  differ, and the differences are the engine type and `syncEngineParams`.
