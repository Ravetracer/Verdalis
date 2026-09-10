# RainyDay — open items


## The tack, and three explanations that did not survive

*Tack* was added because RiverFlow's *Trickle* layer -- single drops on wet
stone -- was said to have the sound RainyDay has always missed for larger drops
on concrete. The layer is real and useful either way: RainyDay modelled the drop
and not the surface, and now it models both. But the *reason* it sounds
different is not established, and three explanations were measured and
discarded:

- [x] ~~RainyDay's impacts are tuned where the reference's are broad (Q 18
      against 5.9).~~ **The Q estimator was unstable.** The same file measured
      Q 18 at one analysis window and Q 7.5 at another; the half-power width
      was being found by walking raw FFT bins whose scatter is as large as the
      peak. `events.py` now smooths before measuring a width and refuses to
      report one that is not resolved.
- [x] ~~RiverFlow's events are broadband where RainyDay's are peaky.~~ Measured
      by spectral flatness, which *is* stable across analysis windows, it is
      the other way round: RiverFlow 0.11, real drops on stone 0.34, RainyDay
      0.41. RiverFlow's trickle is the most tonal of the three.
- [x] ~~RainyDay's drops are incoherent -- each tuned to its own random pitch,
      where a real surface gives every drop the same colour.~~ The real
      recording has the *widest* per-event pitch spread of all: 1.99 octaves
      against RainyDay's 1.15 and RiverFlow's 0.96.

- [ ] **So: settle it by ear.** A/B renders are in `!dev/tack-ab/`, including
      the RiverFlow trickle layer isolated. Find the setting that sounds right,
      then work backwards to what measurement would have predicted it -- that
      measurement is the one worth adding to the analysis tools.
- [ ] Until then the factory presets are untouched and carry the default Tack of
      0.45. Once the right setting is known they should be set per preset, and
      the concrete and metal presets refitted against their own references.

## 1. Realism: what two more sources say we are still missing

Added 2026-09-04 from `!dev/RealtimeSoundSimulationOfRain.pdf` (Miklavcic, Zita
& Arvidsson, DAFx'04) and the "How it works" of `github.com/gtnoble/drip`. Both
agree with what the engine already does -- Marshall-Palmer sizing, the entrained
bubble as the tonal layer, only some impacts entraining one, air absorption, a
statistical far field -- so what follows is only the difference. Ranked by how
much realism each is likely to buy.

Every one of these changes the sound of every preset and so costs a full re-fit.
1a and 1b are done, 1d is half done, and 1c is rejected -- each with the
measurements that settled it, so none of them gets tried again on the strength
of the same argument.

### 1a. Amplitude should follow impact energy, not drop mass -- DONE

The engine sets droplet amplitude proportional to volume, i.e. to mass. drip
uses `E_acoustic = 0.001 x (1/2 m v^2)`: about a tenth of a per cent of the
kinetic energy becomes sound. Amplitude is the square root of energy, so the law
should be `A ~ sqrt(m) * v_term`, not `A ~ m`.

Terminal velocity is itself a function of size (roughly 2 m/s at 0.5 mm rising
to 9 m/s at 5 mm), so this is not a constant factor. Over a ten-to-one range of
radius the present law spreads amplitude over 1000:1 where the physical one
gives about 143:1 (the 95:1 first written here was optimistic; it needs a range
where the velocity has saturated at both ends). If that is right, RainyDay is exaggerating its big drops
by an order of magnitude, which would read as isolated loud plonks over a bed
rather than as rain. **This is the most suspicious single thing in the engine**
and it is cheap to test: it is one expression in `spawnDroplet`.

Built 2026-09-04. Measured rather than assumed: over the range the engine draws,
the old law spread loudest against quietest by 130:1 and the energy law gives
45:1; over 0.5 to 5 mm it is 1000:1 against 143:1. So the order of magnitude was
right. Mean loudness still holds within half a decibel across Level Spread.

### 1b. The impact frequency is drawn at random -- DONE

`kImpactMinHz`..`kImpactMaxHz` drew the impact blip uniformly between 1 and
16 kHz for every droplet on every surface. That came from Liu, Cheng & Tong
(2019); drip derives it for a rigid surface as `v / 2R`, the drop's speed over
its own diameter.

Built 2026-09-04. Both sources are honoured and the surface chooses: liquid
surfaces keep the statistical draw, rigid ones derive it from the drop, and
`impactFromDrop` blends between them in the log domain. For the sizes the engine
draws, `v / 2R` lands between about 3 and 4 kHz, against four octaves of noise.

One correction to drip on the way. A single deterministic frequency per drop
measures as markedly more tonal than the recordings -- every rigid-surface
preset fell below its reference on per-frame flatness -- which is right, because
`v / 2R` is a characteristic contact time and the real one varies with the angle
a drop arrives at and how far it flattens. It sets the centre of a spread
instead. The width is a modelling choice: sweeping it from half an octave to two
moved the flatness by under 0.01, so the measurements have no opinion.

### 1c. Anchor the bubble pitch to Minnaert -- REJECTED, the numbers refute it

Minnaert gives `f = 3.26 / R`, so an audible plink at 1 kHz needs a bubble of
3.26 mm radius: a 6.5 mm bubble, larger than most raindrops entirely. Working
the other way, a bubble entrained by a 1.5 mm drop at the 0.1 to 0.3 of drop
radius the literature reports rings somewhere between 14 and 44 kHz, and even a
5 mm drop only reaches 4 to 13 kHz. RainyDay's fitted `Drop Pitch` values run
from 529 Hz to 8.3 kHz with a median of 2.1 kHz, which is where the recordings
plainly have their tonal energy.

So anchoring the tonal layer to Minnaert would push nearly all of it ultrasonic
and delete the character every preset is built on. The audible pitched content
in real rain is not a Minnaert bubble from a single raindrop; it is more likely
cavity and surface resonance, or larger coalesced bubbles.

It also would not buy what it promised. Minnaert relates pitch to *bubble*
radius, and nothing fixes the bubble radius given the drop, so the freedom moves
from "Drop Pitch in Hz" to "entrainment ratio" and the first is far more use to
whoever is turning the knob. The `1/R` relationship the engine already has is
the part of Minnaert that survives contact with the measurements, and the 2019
paper confirmed it. Physically implausible values are kept out by the per-preset
bounds in `fit.py`, which is where that belongs.

### 1d. Distance is a gain and a filter, not a distance -- PARTLY DONE

The propagation delay is built: a droplet's distance now sets when it is heard,
`r / 343 m/s`, so near droplets are loud, bright and early together instead of
the first two being asserted without the third. `startOffset` counts across
blocks to allow it.

It is worth almost nothing audibly, which is worth writing down. Delaying a
Poisson process by independent random amounts leaves a Poisson process of the
same rate, so a steady rain texture cannot tell the difference; measured, it
costs about 1 dB in the first 100 ms after a note and nothing after 500 ms. It
is kept because it is correct and free, not because it is a depth cue. The claim
that it was "the cheapest depth cue there is" was wrong.

The other two parts are **rejected**:

- Inverse-square spreading. True `1/r^2` across a 30 m field with any sane near
  limit is a 50-plus dB range, which makes Distance unusable as a control. The
  existing `1 / (1 + 3d)` is a deliberate regularisation, not an oversight.
- drip's air absorption, `alpha(f) = 0.02 (f/1000)^1.5 dB/100 m`. That is
  0.19 dB at 10 kHz over the whole field, and roughly sixteen times less than
  standard atmospheric absorption figures at every frequency checked between
  1 and 10 kHz. Adopting it would be a large regression against the exponential
  lowpass already there, which is more aggressive than physical absorption but
  is an artistic control the whole library is fitted around.

### 1e. Not applicable, recorded so it is not rediscovered

The DAFx paper's own conclusion is that bare rain rarely sounds like rain: what
listeners recognise is rain plus rooftops resonating, water flowing, leaves,
wind. That is a real finding, and it is also exactly the scope this project has
decided against. Worth remembering when a preset sounds thin for no reason the
measurements can explain.

Added 2026-09-06: `!dev/Numerical_Calculation_of_Slosh_Dissipation.pdf` (Malan,
Pilloton, Colagrossi & Malan, Appl. Sci. 2022) is not about this kind of slosh
and does not need reading again. It simulates a partly filled aircraft fuel tank
on springs, oscillating at 6.51 Hz, whose liquid slams into the tank roof; the
subject is the energy dissipation budget, computed by SPH against a
finite-volume VOF method. There is no acoustics in it at all -- the only speed of
sound quoted is the artificial one of the weakly compressible SPH model, a
numerical device -- and the length and time scales are metres and hertz against
the millimetres and kilohertz a droplet works in.

The one point of contact is structural rather than quantitative: the paper
splits dissipation into a continuous viscous term and a discrete impact-loss
term and finds the impact losses dominate, and it notes that greater
fragmentation dissipates more energy. That agrees with the cascade model in the
engine -- discrete sub-bursts rather than a smooth wash -- but it constrains no
number here.

What would actually move the slosh, in order: a close recording of rain on wet
wood or wet glass, which is what the two reasoned surface columns are waiting
for; then the drop-impact-on-a-thin-liquid-film literature (crown splash, the
splashing threshold, secondary droplet count and size against film thickness and
Weber number), which is the cascade itself; then impact acoustics of drops on
wetted rigid surfaces.

### 1f. The bubble's pitch bend was measured on the wrong part of the drop -- DONE

Closed 2026-09-05, and worth keeping because the mistake is easy to repeat.

The bend had been fitted at 0.01 to 0.09 octaves by tracking isolated drops
across the 80 ms they are loud. That measurement is right; the conclusion was
not. Energy weighting only ever sees the plateau, and the plateau is the one
part of a drop that does not move -- the reference stays within 0.07 octaves of
750 Hz for as long as it is within 2 dB of peak. The rise happens afterwards,
from 846 Hz to 2 kHz between 30 and 110 ms, while the drop falls from -9 to
-36 dB. +1.47 octaves, essentially all of it below -3 dB.

A 20x pitch-preserving time stretch of the reference is what made this
tractable: twenty times the cycles to track, and zero-crossing timing is the
only method that survives the quiet tail. Fitting the accumulated bend against
`(e^(kf) - 1) / (e^k - 1)` over the reliable window gives k = 0.5.

Two anchoring traps, having been caught by both:

- Front-loading the bend into the attack makes any large span sound like a
  laser, which reads as evidence that the span must be small. It is not; it is
  evidence that the shape is wrong.
- Back-loading it and then spreading the sweep across the droplet's whole
  lifetime (1.6 ring times, about -96 dB) leaves under a fifth of the bend
  before the drop is inaudible, which sounds like no bend at all. The sweep has
  to finish at 0.6 ring times -- the -36 dB point -- and hold.

Large spans belong only to surfaces that trap a bubble: Water and Puddle carry
1.30 and 1.45 octaves, every rigid surface keeps the hundredth of an octave it
had. The rain textures that happen to sit on Water are held still with
`chirp = 0.02` in the preset, verified at 0.00 dB per band against the previous
engine, so only Cave Drips, Puddle Plinks and Dripping Faucet moved.

## 2. What the library's residual says the engine is missing

Measured 2026-09-04 across all sixteen fitted presets, against nine different
reference recordings. The point of doing it this way: a preset that measures
badly may simply be pointed wrong, but a bias that survives sixteen independent
fits against different targets cannot be a preset's fault. That is the engine.

    band        mean    spread   presets leaning the same way
    200-400   -1.98 dB    2.57      13 of 16
    3.2-6.3k  +2.69 dB    2.89      13 of 16
    12-20k    -3.84 dB    6.46      12 of 16   (spread too wide to trust)

So the engine is consistently **thin between 200 and 400 Hz and heavy between
3 and 6 kHz**: a tilt, not a level error. Two things point at the impact layer
as the cause of the upper half. It is the widest-band part of a droplet and the
least constrained by anything physical; and 3 to 6 kHz is exactly where the
drop-derived impact frequency from 1b now sits, scaled by surface brightness.
Inside the Car regressed in that same band when 1b landed.

Both checked on 2026-09-04:

- **The 3 to 6 kHz excess does not come from 1b.** Measured at 2287146, the
  parent of the commit that introduced the drop-derived impact frequency, with
  that commit's own presets and tooling: on the thirteen presets whose fits had
  converged there, 200-400 Hz was already short by 1.9 dB (11 of 13 negative)
  and 3.2-6.3 kHz already heavy by 2.2 dB (10 of 13 positive). The same signs,
  the same counts, within half a decibel of the current engine. The apparent
  jump in the all-sixteen mean was three presets that were badly fitted at the
  old commit and happened to sit below their references in that band. So the
  tilt is the engine's and predates the impact model; revisiting the impact
  weighting per surface would not have fixed it.
- **The 200 to 400 Hz shortfall is where a struck surface rings.** Built as the
  fifth droplet layer: one low mode of the surface per impact, at a frequency,
  ring time and weight that are the surface's own (the `body` columns of
  `kSurfaces`), scattered per droplet, scaled by Impact, bypassing the droplet's
  radiation highpass because it is the surface radiating. Water and Puddle have
  none. Whether it also relieves the 3-6 kHz excess -- by letting the fit stop
  compensating for the missing low mids elsewhere -- is what the re-fit after it
  has to show.

### After the 2026-09-05 re-fit

Measured over all sixteen presets, same method: 3.2-6.3 kHz went from +2.69 dB
(13 of 16 positive) to +1.24 dB (10 of 16); 200-400 Hz from -2.05 to -1.97 dB.
The low-mid number hides where it moved. The surfaces with a body are largely
fixed (Under an Umbrella -7.0 to -2.2, Inside the Car -5.1 to -2.4, Tin Roof
-2.8 to -0.9), and what remains is the presets that use the **Water** surface
while imitating something that is not water: Steady Rain and Tropical Monsoon
are fitted to a roof recording, Downpour and Storm Front to concrete, all with
`surface = Water`, which has no body by design. Surface is never fitted because
it is a preset's identity; here the identity looks wrong. Try Wood for the roof
presets and Concrete for the concrete ones, re-fit those four, and see whether
the shortfall goes.

### After the 2026-09-05 chirp re-fit

Re-fitted after the pitch-bend rework of 1f. Scored on seeds the fit never saw
(31/32/33), the library went from a mean distance of 144.0 to 138.1, with no
preset regressing.

That number is a filtered result, not the fit's own. **Seven of sixteen presets
were accepted and nine rejected**, on the rule that a preset is only taken if it
improved on the unseen seeds rather than the ones it was fitted against:

- taken: steady_rain, rain_on_leaves, concrete_alley, tin_roof, inside_the_car,
  cave_drips, window_pane
- refused because they got worse on unseen seeds: light_drizzle (34.0 ->
  361.7, a tenfold blow-up and the clearest case of section 3 there has been),
  puddle_plinks (124.7 -> 138.9), downpour (13.8 -> 15.4), first_drops
  (22.7 -> 23.0)
- refused although the number improved: **dripping_faucet**. The fit halved
  Tonality (0.578 -> 0.298), more than doubled Pitch Spread (0.914 -> 2.11) and
  nearly doubled Density, turning a slow tap with a consistent pitch into a
  fast scatter across two octaves -- measured, twice the drop events and the
  spectral centroid up from 3.7 to 5.1 kHz. It bought 2 % on a distance that
  stays above 800 either way, and it would have undone the drop character 1f
  exists to produce.
- the rest converged to no change

Two things worth doing before the next re-fit. `dripping_faucet` scores 768 on
one seed set and 1426 on another with identical parameters: its distance is
dominated by which drops happen to fall, so the pairing against
`multiple_water_drops_faucet` is measuring seed noise, not tone. And the accept
rule above is applied by hand at the moment; section 3 wants it in `fit.py`.

## 3. The fit overfits its own seeds -- FIXED 2026-09-06, with two limits left

The seed count was already fixed in 447ceb3 (twelve seeds a candidate, not two);
what remained was everything around it. Measured over 32 fresh seeds per preset
before changing anything:

    preset             mean      sd  sd/mean   se@12   se@3
    cave_drips        475.2   469.7    98.8%   135.6  271.2
    dripping_faucet  1019.8   898.4    88.1%   259.3  518.7
    storm_front        86.2    68.2    79.1%    19.7   39.4
    light_drizzle     720.0   462.1    64.2%   133.4  266.8
    window_pane         8.1     4.6    57.0%     1.3    2.7
    gutter_trickle     36.5     0.8     2.3%     0.2    0.5

Four things were wrong, and all four are fixed.

**The held-out check was noisier than the fit it judged.** The fit averaged
twelve seeds and the accept/reject decision was made on three, which for
Dripping Faucet is a standard error of 519 on a mean of 1020. `VERIFY_SEEDS` is
eight now.

**A paired test was tried and refuted.** Scoring a candidate and its parent on
the same seed does not give them the same droplets: the seed drives a Poisson
process whose realisation depends on its rate, so changing Density -- or
anything that shifts how many draws a droplet consumes -- produces different
rain entirely. Measured over 32 seeds on eight presets, the spread of the
per-seed difference was 0.3 to 1.7 times the spread of the distance itself. The
idea is recorded here so it is not tried again.

**The mean was ranking accidents.** Several presets are heavy-tailed rather than
merely wide: Cave Drips has a median of 391 against a maximum of 3016, Dripping
Faucet 788 against 4234. `score_many` aggregates by median now. It costs
nothing -- the same renders are scored either way.

**The objective paid a preset for breaking a measurement.** `feat.decay_ms`
returns NaN when no isolated event decays 20 dB inside its 0.4 s window, and
`distance` silently skipped the term when it did. Over 24 seeds Light Drizzle
measured on 18 of them and scored 1026, and on the other 6 the term vanished and
it scored 43, so a fit could improve the number by making the sound
unmeasurable. That is exactly the 34.0 -> 361.7 blow-up of the previous run.
Fixing it took Light Drizzle's spread from 64 % to 0.8 %.

**And W_DECAY was pointing the wrong way.** Measuring the references shows most
of them saturate the estimator's 1200 ms ceiling -- leaves 1175, metal 1186, car
1195, window 1196, umbrella 1178 and the cave itself 1159 -- while roof, concrete
and sewer return NaN. Only `rain_soft` (38.7 ms) and
`multiple_water_drops_faucet` (20.9 ms) carry a real measurement. So the term
contributed about 0.1 to Cave Drips, the one preset whose ring time it was
written for, and a large constant to the two that have a usable reference. The
reference now decides whether the term is asked at all, so no candidate can
switch it on or off.

**The accept rule is no longer a person reading two numbers.** `fit_preset`
requires the held-out median to improve by more than one standard error of
itself, and writes a refused preset back unchanged so the output directory is
always a complete, installable library.

### Two limits this exposed, both still open

**Some presets cannot be adjudicated at all.** Cave Drips carries a held-out
noise of 283 because `direct_late` and `late_rt` hold 98 % of its variance
(`direct_late` has a standard deviation of 353 about a mean of 81); Storm Front
carries 43 because `tflat` holds 99 % of its. No improvement either preset can
make will clear its own measurement error, so both are effectively unfittable
until those estimators are stabilised. Loosening the margin would not fix this,
it would only stop the noise being visible.

**Two presets are paired with the wrong recording**, which is what the decay work
uncovered rather than something it caused:

- `light_drizzle` is fitted to `rain_soft`, which reports 0.82 events a second,
  while the preset renders 1256 drops a second. The decay term is therefore a
  constant penalty of about 1000 that no parameter can move -- verified by
  sweeping Drop Decay over a factor of 40 and Density over a factor of 20, which
  changed the measured decay from 1198 ms to 1197 and 1091 respectively.
- `dripping_faucet` was already known to measure seed noise rather than tone.

### The 2026-09-06 re-fit

Run with all of the above in place. **Five of sixteen presets improved on unseen
seeds**: steady_rain (+1.9 against a noise of 0.7), rain_on_leaves (+2.9/0.6),
concrete_alley (+159.8/7.2), light_drizzle (+9.5/2.5), gutter_trickle (+1.1/0.6).
The other eleven were refused automatically and written back unchanged.

Two of the five accepted fits were then refused **by hand**, on the same grounds
as dripping_faucet in the previous run -- the number improved and the preset got
worse:

- **concrete_alley**, and this is the one to remember. Its 92 % improvement came
  from cutting Splash 1 -> 0.635 and pushing Distance 0.547 -> 0.687 and Air
  0.152 -> 0.245. The fit cannot touch Slosh, so it starved the layer from every
  other side instead. Measured over three seeds: energy above 8 kHz fell from
  46.5 % to 14.6 % and crest from 24.8 to 17.1 dB, which are the two signatures
  of the splat. That is correct against `rain_on_concrete`, which has 4 % above
  8 kHz, and wrong against the close recording the layer was built from, which
  has 25.8 % and a crest of 31.6. **The concrete presets need repointing before
  they are fitted again.**
- **light_drizzle** went from 1256 drops a second to 3014 with clumping 0.712 ->
  0.992, for a gain of 9.5 on a distance of 1014 that is more than 98 % constant
  decay penalty. It bought noise and cost the preset its name.

So three were installed: gutter_trickle, rain_on_leaves and steady_rain, with
Output Gain re-matched afterwards.

## 4. CPU cost

Measured 2026-09-04 with a benchmark that drives `RainEngine` directly with
flush-to-zero set the way `process()` sets it: Downpour rendered at 130 % of
realtime on one core, Tin Roof at 67 %, Storm Front at 35 %. Far too heavy for
a plugin. Two exact changes -- each layer of a droplet is skipped once it has
decayed below -140 dBFS, and the sine comes from a table -- took those to 52 %,
16 % and 22 %, with the output identical to below -100 dB, so nothing needed
re-fitting. The remaining cost is the chain of filters every droplet runs every
sample (resonator, radiation highpass, air), which is bound by its own serial
dependency and not by arithmetic: the table sine bought almost nothing.

What would buy the next factor:

- **A random generator per droplet.** Every droplet draws from the shared
  generator every sample for its splash, whether the splash is still audible
  or not, because skipping a draw would shift every later random number and
  change the whole rain. Seeding a small generator per droplet at spawn would
  let a droplet stop the moment all its layers are silent instead of running
  to 1.6x its longest decay (about a third of its life is spent below -60 dB),
  and let the draw itself be skipped. It changes the rain a fixed Seed produces,
  once; the statistics do not change, so the fitted library stays valid, but it
  cannot be verified by comparing renders, only by re-measuring.
- **Processing droplets in lockstep.** Four or eight droplets per loop
  iteration, so the filter chains' latencies overlap. A restructuring of
  `Droplet` into arrays; likely another 2-3x.
- ~~Check whether Downpour drops droplets.~~ Counted: over six seconds of
  Downpour, 24733 spawns, none refused; 2430 (10 %) took over a slot whose
  droplet had already decayed below -60 dB, which is the intended path and
  inaudible. Distant Rain Wall reuses 4.6 %, every other preset none. The pool
  sizes are right.

## 5. GUI follow-ups

- **Text entry on a knob -- DONE 2026-09-04.** Click the value under a knob
  and it becomes a field with the current text selected; typing replaces it,
  Return applies it through `paramTextToValue()` (so `2.2k`, `500 ms`, `-12 dB`
  all work), Escape cancels, and a value the parser rejects turns the field red
  and leaves it open. A double-click on the value still resets. It takes the
  keyboard the way the save field does, for the life of the field, which is the
  modal case where a grab is defensible; the XEmbed item below is still the
  principled fix for both.
- **Keyboard focus for the embedded window.** Confirmed in Bitwig: asking for
  the input focus does not work, so the save dialog takes a keyboard grab
  instead. That is defensible for a modal field but it does not generalise --
  text entry on a knob cannot grab the keyboard every time a value is clicked.
  The principled fix is the XEmbed protocol: set `_XEMBED_INFO` on the window,
  handle the `_XEMBED` client messages, and ask the embedder for focus with
  `XEMBED_REQUEST_FOCUS`. Worth doing before any further text entry is added,
  and it needs a real host to test against.
- **Resizable window -- DONE 2026-09-04.** `can_resize` is true with
  preserve-aspect-ratio hints; `adjust_size` snaps a request to the largest
  scale that fits (0.5x to 4x) and `set_size` applies it as the one cairo scale
  over the unchanged layout. Verified at 1440x1110 and 600x463 through
  `rainyday-guihost`, which now forwards its window's size changes to the plugin
  the way a DAW does.
- **Wayland.** Only the X11 window API is offered. Under a Wayland host this
  falls back to the generic parameter view.
- **A scrollbar in the browser -- DONE 2026-09-04.** The grid is row-major and
  scrolls by rows on the wheel or the arrow keys, with a scrollbar drawn only
  when the library overflows; it opens with the current preset in view. Verified
  with seventy presets.
- **The selector list -- DONE 2026-09-04.** The wheel and the arrow keys step an
  open list's value, Return and Escape close it. Whether keys arrive at all is
  still the host's decision (see the XEmbed item above); in the test host they
  do.

`tools/guihost.cpp` opens the editor outside a DAW, which is how any of this
gets checked. It opens a window on the current display, so it is not something
to run unannounced.

## 6. Later / nice to have

- **Wind and thunder are not coming here.** Decided 2026-09-05: they will be
  separate plugins. RainyDay stays rain, and nothing in it should be shaped to
  leave room for them.
- **Per-note modulation.** `CLAP_EVENT_PARAM_MOD` is handled globally;
  per-note-id modulation is currently ignored.
- **Per-voice filter.** `Filter Key Track` follows the most recent note
  because the state-variable filter is a single global stage.
- **Fit the new layer controls.** `Bed Width` and `Drop Width` were split out of
  the old single `Stereo Width`, and every preset simply kept its old value for
  both, which is why the library measures exactly as it did before. Nothing has
  yet asked whether a preset wants its bed wider than its droplets, and the
  reference recordings are mono, so this needs either stereo references or a
  decision made by ear.
- **More surfaces.** The surface model is a small table in
  `src/dsp/rain_engine.cpp` (`kSurfaces`); adding e.g. canvas, water butt or
  car roof is a one-line change plus a parameter enum entry.
- **More reference recordings.** The fit in `tools/analysis/` is only as good as
  what it is fitted to. Distant Rain Wall has no usable reference at all and is
  derived from the fitted Downpour by hand; Gutter Trickle and Window Pane are
  matched to the nearest neighbour rather than to themselves. A true far-field
  recording, a downspout, rain on a tent and rain on a water surface would each
  earn their keep.
- **Teach the fit to tell a droplet from a room -- DONE 2026-09-04.** The
  objective now measures, on references sparse enough to have isolated events,
  the event rate, the late RT60 (Schroeder integration from 50 ms after each
  isolated event, floor subtracted) and the direct-to-late energy ratio, and
  compares per-frame flatness as a log ratio there as well, since 0.001 against
  0.010 is nothing squared and everything heard. Sparse presets render for 30 s
  per candidate instead of 6. Cave Drips is the first preset fitted with its
  space and filter controls (`DROP` mode in `pairs.py`), because its reference
  has a measured room. Still open: the same treatment for Dripping Faucet and
  Puddle Plinks once their references are judged good enough to fit a room to.
