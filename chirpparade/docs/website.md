# ChirpParade

**Synthesised birds. A CLAP instrument for Linux and Windows.**

ChirpParade generates birdsong. It does not play any back. Every syllable is
computed while the plugin runs, from a measured frequency contour driving an
oscillator through a modelled valve and a modelled windpipe. There are no
samples in it, and no two calls it makes are ever identical.

Play a short note and one bird sings one phrase. Hold the note and a flock
carries on around it, calling and answering, for as long as you hold it.

---

## A bird syllable *is* its frequency contour

This is the whole idea, and it was arrived at the hard way.

A syllable of birdsong is not a note with a shape on it. It is a scribble: a
frequency that travels three to seven times further than the distance between
its highest and lowest point, changing direction up to forty times, at slew
rates reaching hundreds of octaves per second. Analysed through the 21 ms
window that most spectrograms use, none of that is visible — a syllable looks
like a gentle glide, and a model built to reproduce that glide sounds nothing
like a bird.

So ChirpParade does not describe a syllable. It carries measured ones.

Real recordings were segmented into individual syllables, each one tracked at a
0.33 ms hop, fitted as a cosine series, clustered by shape, and the medoid of
each cluster kept — one real syllable, not an average of several, because two
contours that zig-zag out of phase average into exactly the smooth glide the
whole approach exists to avoid.

What ships is the coefficients. They are a formula, not a recording: a few
thousand numbers, tens of kilobytes, from which the sound is computed afresh
every time. Refitting a real syllable and measuring it back lands within a few
tens of cents of the original pitch and a fraction of a decibel of its level.

## Timbre is a valve, not a filter

Air passes through a bird's syrinx only while the two membranes are apart.
**Voice** is the fraction of each cycle they are shut.

At zero the valve never closes and a pure sine comes out — which is what 59 %
of the syllables in the reference library actually are. Close it and the
airflow becomes a one-sided pulse with a full harmonic stack, evens as well as
odds.

That one-sidedness is the point. A symmetric oscillator — including the syrinx
equation in the physics literature — has energy at f, 3f, 5f and nothing
between, and no amount of drive will give it the even harmonics a low, rough
call has. Rectifying the flow is the same step that makes a human glottal pulse
rich rather than sinusoidal.

Above the valve sits the trachea, modelled as a tube closed at one end and
resonating at `c/4L`. **Beak** opens the far end: the resonance rises and
broadens and begins to track the pitch across the syllable, the way a
songbird's gape does.

## Partials: reaching for the recording itself

Each archetype also carries the measured balance between its first six
harmonics *across* the syllable — six amplitude curves, phase-locked to the
same oscillator.

This exists because that balance moves by around 4.4 dB over a single syllable
in the references, and a fixed valve through a fixed tract cannot do that at
all. **Partials** crossfades the synthetic valve into the measured balance, and
it is scaled by how much of that syllable's energy the harmonic comb actually
accounted for — so an archetype that happens to contain a second bird does not
pretend to know something it doesn't.

## Two instruments in one window

**A shot.** A note fires a deliberate phrase that always completes, however
short the note. Play a melody and each note is a bird.

**A drone.** While a note is held, a flock of individuals calls unprompted as a
Poisson process. Each has its own pitch, position, distance and voice, and
**Answer** makes one bird's phrase provoke a reply from another.

Either half turns fully off, so the same plugin is a playable bird and an
ambience generator.

**Woodpecker drumming** is a third, separate layer, because drumming is
sonation rather than voice — a bill against wood, not air through a syrinx. Two
broad wooden modes excited by a contact, in a roll that accelerates by the
measured amount.

## Eight species

Choosing a species moves the pitch register, the syllable length, the harmonic
richness, the roughness, the rate *and* the whole set of measured contours at
once — because in the recordings all of those move together.

**Whistler · Sparrow · Warbler · Budgie · Woodpecker · Crane · Goose · Screech**

Every number in that table is a median measured from recordings of that bird.
Pitch at its default is the library-wide median, so each species at its default
sings in its own register without being transposed there by hand.

Screech is the exception and is labelled as one: it has no reference of its
own, takes the most extreme contours in the library, and is kept as an effect
rather than as a bird.

## What it does not do

**There is no crow and no raven.** Both were built, measured, fitted and
removed. A corvid's call is rough in a way this engine reproduces as a rough
*tone* rather than as a croak, and no amount of further fitting closed that
gap — the reference recordings are still in the library and still part of its
census, but they no longer become an instrument. Shipping a raven that isn't a
raven would have been worse than shipping no raven.

**The valve saturates around five harmonics.** The noisiest references measure
twelve. Closing the valve further aliases rather than helping. The likely
answer is a second syringeal side — a real bird has two, driven independently,
which is how some species sing two notes at once.

**Phrases are simpler than real song.** Real song has figures; ChirpParade's
phrase-level pitch movement is a fixed interval per syllable. The syllables are
measured; how a species' *phrase* moves is not, yet.

**The tract is one resonance.** A real trachea has a series at odd multiples of
`c/4L`.

These are written up in full in the manual and in `STATUS.md`, with the numbers.

## Technical

- Native **CLAP**, Linux (x86-64) and Windows (x86-64)
- 61 parameters — syllable, voice, tract, phrase, flock, drumming, distance,
  full ADSR
- Sample-accurate note and parameter handling, host modulation, bounded voice
  pools
- Preset browser and preset discovery built in; presets are plain text
- No GUI toolkit dependency: the window is X11 or Win32 plus Cairo, drawn by
  hand, with a sonogram scrolling across its header
- Renders a twelve-bird dawn chorus many times faster than realtime
- Reproducible: with the seed pinned, renders are bit-identical across runs and
  sample rates

## Licence and provenance

MIT. Part of the **Verdalis Plugin Suite** — a set of instruments that model
natural sound sources from first principles, where everything is synthesised
and nothing is sampled.

The reference recordings used to fit the contours are not distributed with the
plugin and are not part of it. What ships is measurement: tables of
coefficients from which the audio is computed.
