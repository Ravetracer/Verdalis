# WhooshPact TODO

Ordered roughly by how much difference it would make.

## Listen to the sub pass

The sub layer is no longer on in every preset. Its base is -60 dB, as Tone and
Hit already were, so a preset opts into it; six presets now have none, three had
theirs trimmed, and *Dry Snap* was added as an accent with no sub and no
transient, the counterpart to *Simple Whoosh*.

**Every one of those calls was made by measurement, not by ear.** Each candidate
was rendered with the layer and without, and the low fraction of both held
against the family median -- the table is in `tools/analysis/README.md`. The
renders are in `!dev/audition-sub/`, paired as `__A_with-sub` and `__B_no-sub`.

What still wants a listen, in the order the measurement left them closest:

- **Big Horn and Brass Wall** kept their sub on a 0.01 margin against the family
  median. Either could go the other way.
- **Deep Whoosh** measures 0.78 below 100 Hz against the transitions' 0.62 even
  with its sub trimmed 6 dB, because the brown noise and the 900 Hz corner are
  what put it there. It may want no sub at all.
- **Annihilation and Rising Dread** were trimmed 3 dB rather than emptied, which
  lands them at 0.76 and 0.75 against 0.74. That is the measurement satisfied;
  whether they lost anything is not something it can say.

The same question still applies to Air, which is on in all thirty-one. It is
more defensible -- it is the layer everything else is built over -- but a boom
that is nothing but a sub and a room is a legitimate preset and there is not
one.

## Validate by ear against the references

The suite's rule is ear first, then measurement, and this plugin currently has
the second without the first. ChirpParade 0.1.0 was 11,000 lines that measured
correctly on twenty quantities and sounded nothing like a bird.

What is needed: render each family's presets next to the references they were
fitted to and listen to the pairs. The measurements say the shapes and the
balances are right; they cannot say whether a braam sounds like a braam.

Specific suspicions to check first:

- **The Hit layer.** Three inharmonic modes plus filtered noise is thin next to
  the references' impacts, which have debris in them — many small events rather
  than one shaped burst.
- **The Tone layer's braams.** A detuned saw stack is the right family of sound
  but the references have a slow formant movement in them that a static filter
  does not give.
- **Green noise.** The name has no standard definition; a 500 Hz band with Q 1.1
  is one reading of it and may not be what anyone expects.

## Make the Hit layer ring rather than be enveloped

The three modes are excited by enveloped noise and their decay is that envelope,
not their Q. That is why a long `Hit Decay` reads as noise held open instead of
as metal ringing down.

The reason it is built that way: a `Svf` at Q 50 and 900 Hz rings for 18 ms, and
`Hit Decay` goes to 3 s. Getting a real 3 s ring at that frequency needs a
proper modal resonator with its own decay rate per mode — which RiverFlow's
`Pocket` is nearly, and which would then belong in `shared/`.

## Debris

The measured impacts are not one event. `Impact - Bits & Pieces`,
`Impact - Complex Wreck` and `Impact - Complex Dissolve` are scatters of small
events over a second or more, and the plugin has no way to make one.

This is the same stochastic spawner RainyDay, ShoreBreak and CrackleBlaze
already have three copies of, which the suite `CLAUDE.md` already lists as worth
extracting. A fourth caller is a good reason to do it, and a `Debris` parameter
on the Hit panel is what it would drive.

## Tempo sync

`Span` in beats as well as seconds, and the flutter rate in note values. A
transition is almost always cut to the bar, and every one of these has to be
dialled in by hand at the moment.

It needs the host transport, which no plugin in the suite reads yet.

## Reverse

A reversed whoosh is a staple and is *not* the same as `Peak` near 1.0: what
makes a reverse sound reversed is the exponential rise, which the `Rise` curve
can only approximate. A proper reverse would run the gesture's own envelope
backwards over a reversed-decay layer.

## Extract the EQ into a shared voicing block

`Biquad` and `Tilt` are already in `shared/include/verdalis/dsp/biquad.h`, but
the three-band EQ built on them is WhooshPact's own. The suite `CLAUDE.md`
already anticipates the other half of this: ShoreBreak, SkyHowl, RiverFlow and
CrackleBlaze each carry a measured-bed filterbank with a solved gain vector, and
all four would be better as a shelving tilt carrying the average slope with the
bank solving only the residual. `Tilt` is that filter. Nothing has been changed
in those four yet.

## Per-note modulation

`Variation` is per trigger and global. Per-note CLAP modulation of individual
parameters would let one note be deliberately different rather than randomly so,
which is what an arrangement actually wants at a section boundary.

## Crest factor

The downshifter references measure 7.7 dB; the presets render at 11–13. Those
are limited masters and the output stage here is a soft clipper, so the gap is
expected — but a proper limiter on the output, or a compressor with the gesture
envelope as its sidechain, would close it and is what the material is asking
for.

## Smaller things

- `Air Curve` only warps the sweep forwards (`t^curve`). An S-curve would let the
  movement happen in the middle rather than only early or late.
- The Hit layer has no stereo placement of its own. Three modes spread across
  the field would give a collision some size.
- Space's `setEnclosure` is derived from `Space Size` rather than exposed. For a
  made sound rather than a field recording that may be the wrong call.
- Several parameter display names repeat across panels (`Decay`, `Tone`,
  `Width`, `Cutoff`). That is the suite's convention and hosts group by module,
  but it makes `render --param` ambiguous and the ids have to be used instead.
