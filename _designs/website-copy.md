# Verdalis — website copy

Three alternative texts per plugin for the plugin pages on the website, 2–3
paragraphs each, written in a plain product register: what the plugin aims to
be, then what it features.

Pick one per plugin, or mix paragraphs between them. Every factual claim is
taken from the plugin's README, STATUS and parameter table — re-check them if a
version changes the numbers.

---

## RainyDay

### Variant 1

**RainyDay aims to be rain you can place anywhere.** A field recording of rain
arrives with its distance, its surface and its room already baked in, and no
amount of EQ moves it somewhere else. RainyDay generates the rain instead of
replaying it: every droplet is computed at the moment it lands, so the sound
never repeats, has no loop point, and can be moved, re-surfaced and re-roomed
after the fact.

It features 42 parameters covering droplet statistics, the impact surface, the
stereo field, distance, space, a global filter and a full ADSR. Density runs to
five thousand drops a second and drop pitch from 40 Hz to 9 kHz. Eight surfaces —
water, puddle, leaves, wood, metal, glass, concrete and fabric — bias decay,
resonance, click and the secondary splash. Behind the drops you can still
resolve sits a separate far-field wash for the ones you cannot, and behind that a
room from three to ninety metres, with its own damping.

17 factory presets ship with it, from a single drip in a cave to a tropical
monsoon, each fitted against a real recording of the thing it imitates. It is a
native CLAP instrument for Linux and Windows with sample-accurate note and
parameter handling, host modulation, a fixable random seed for repeatable takes,
and its own resizable window with a preset browser and typed value entry. No
samples are included, and none ever will be.

### Variant 2

**RainyDay is a rain synthesiser: no samples, nothing recorded, everything
computed at run time.** The aim is a rain source that behaves like an instrument
rather than a file — play a note and it rains for as long as you hold it, let go
and the release fades it out, and two instances on two tracks give you two
different showers rather than the same one twice.

The model follows the physics. A droplet's size sets the pitch and the ring time
it lands with; the surface scales both and decides whether you hear a plink, a
splat or a rattle. Drops landing on a hard wet surface throw secondary droplets
a few milliseconds later, and that cascade is what separates water from hiss.
Distance, air absorption and room are independent controls, so the same rain can
be at the window, across the yard, or at the far end of a hall.

Features at a glance: 42 parameters, 17 fitted factory presets, eight impact
surfaces, four filter types, a full ADSR with velocity routing, CLAP preset
discovery so the presets appear in the host's own browser, bounded CPU cost, and
a hand-drawn plugin window — X11 and Cairo on Linux, Cairo on Windows — with a
droplet-activity meter. One self-contained `.clap` file, tested in Bitwig Studio
and Reaper.

### Variant 3

**RainyDay aims to solve the loop-point problem.** Rain is the most-used ambience
in existence and the easiest to catch out: however long the recording, the ear
finds where it repeats. RainyDay never repeats, because there is nothing to
repeat — the whole texture, from the individual drops to the wash behind them, is
synthesised from noise, oscillators and filters while it plays.

It features a droplet engine you can steer statistically rather than by hand:
arrival rate, clumping into surges and lulls, the size distribution that spreads
pitch and decay, per-droplet chirp, impact weight, splash length and the slosh
of secondary drops. On top of that sit a far-field bed, a stereo field, distance
with air absorption, a room, a filter and an ADSR — 42 parameters in all, every
one of them host-modulatable and sample accurate.

17 presets give you the range: first drops, light drizzle, a gutter trickle, a
window pane, a tin roof, rain on leaves, inside a car, a storm front, a tropical
monsoon. Each was fitted against a real recording; none of those recordings
ships with the plugin.

---

## ThunderClap

### Variant 1

**ThunderClap aims to model thunder from the lightning bolt outwards.** Thunder
sounds the way it does because a lightning channel is kilometres long and every
part of it is a different distance from the listener — the near parts arrive
first and sharp, the far parts late and dull, the parts inside the cloud deeper
than anything on the ground. Recording that gives you one thunder; modelling it
gives you all of them.

So every flash grows its own channel: a random walk from the strike point up to
the cloud base, a horizontal run inside the cloud, and branches off both. The
channel is cut into up to 4096 elements and each becomes one arrival, with its
time from its distance, its level from spreading and directivity, an N-wave
front, air absorption applied as three filters at the 6, 12 and 24 dB points of
the absorption law, a pan from its azimuth and a ground shadow that grows with
range. Return strokes replay the channel with fresh jitter, in-cloud elements
radiate longer and deeper waves, a rumble layer follows the arriving shock
energy, and up to eight landscape echoes send it back before the room, the
compressor and the output.

It features 44 parameters, 17 presets measured against 38 recordings of real
thunder, and three trigger modes: one shot, gated, and a storm that keeps
flashing while the note is held. The plugin window draws the bolt that made the
sound, from the same numbers. Native CLAP for Linux and Windows, no samples
anywhere.

### Variant 2

**ThunderClap is a thunder synthesiser whose main control is distance.** The aim
is that one parameter should do what it does in reality: move the strike, and the
sharpness of the crack, the loss of high end, the smearing of thousands of
arrivals into a roll and the sub-bass that survives longest all follow by
themselves, because they are consequences of the model rather than separate
settings.

It features shock-wave synthesis from a per-flash lightning channel — thousands
of radiating elements, each with its own delay, level, direction, front sharpness
and absorption filter — plus a rumble layer, up to eight landscape echoes with a
fixed loop gain so the tail decays the same at any echo level, a room behind
them, and a soft-knee compressor with automatic make-up in front of the safety
clipper, so a close strike stays a strike rather than a clip.

44 parameters, 17 factory presets from heat lightning on the horizon to a bolt
directly overhead, CLAP preset discovery, sample-accurate notes and parameters,
host modulation, bounded CPU cost and a resizable hand-drawn window with a
shock-activity meter. Put it on a track beside RainyDay and you have the whole
storm.

### Variant 3

**ThunderClap aims to be a storm you can conduct.** It is not a one-shot player:
it is an instrument in which each note is a flash, each flash is a fresh
lightning channel, and holding a note in Storm mode lets the weather pass on its
own while you play the distance, the orientation and the landscape around it.

Everything is computed — no samples. The channel geometry produces the arrival
pattern; the arrival pattern produces the sound. Crack controls how sharp the
shock fronts are, Focus how directional the channel is, Swell how much the
ground shadow leaves behind, and distance how much of the top end the air has
already taken. The in-cloud portion contributes the deep late swell, measured to
peak near 10 Hz against 50 Hz for a ground stroke, which is the part most thunder
libraries do not have.

It features 44 parameters, 17 presets fitted against 38 real recordings, three
trigger modes, a compressor and a filter on the output, a lightning ornament in
the window that is deterministic in the flash number, and a Linux and Windows
build of one self-contained CLAP plugin. Tested in Bitwig Studio and Reaper.

---

## ShoreBreak

### Variant 1

**ShoreBreak aims to synthesise a breaking wave the way a breaking wave actually
happens — as four overlapping sounds rather than one.** There is the crest
collapsing into a cloud of bubbles, the sheet of foam it leaves behind, the water
washing back down through whatever the shore is made of, and single bubbles
popping in the foam long after the wave itself has gone. All four are generated,
none are sampled, and an arriving wave bursts the foam the previous one left,
because a beach never has two sheets of foam hanging in the air at once.

It features 49 parameters over break, foam, wash, bubbles and the swell bed
beneath them, on six kinds of shore: sand, shingle, pebbles, rock, reef and
harbour wall. Galvin's four breaker types — spilling, plunging, collapsing,
surging — are a parameter, because they measurably differ in spectral slope
above 1.5 kHz. Bubble pitch follows bubble radius, decay follows breaker size at
the rates the literature gives, and the bubbling of a crest before it collapses
is its own control. Distance is the best-fitting part of the model: the distant
presets track their reference recordings within about a decibel from 800 Hz up.

17 factory presets run from an open-sea murmur to a shore in uproar, fitted
against 114 minutes of field recording. Native CLAP for Linux and Windows,
sample-accurate note and parameter handling, host modulation, CLAP preset
discovery, and the suite's window in ShoreBreak's own sea-green with a surf line
in the header.

### Variant 2

**ShoreBreak is an ocean-surf synthesiser built out of measurements.** The aim was
a surf source that is not a noise generator with an LFO on it: every layer, and
the shape of every layer, came from analysing real recordings and published work
on breaking waves, and then the whole thing is generated from noise, filters and
resonators so that no two tides are alike.

It features a four-layer wave model plus a swell bed. The break is noise through
a bandpass centred where a real bubble cloud resonates, sweeping downwards as the
cloud grows and rising over a quarter to a whole second rather than striking. The
foam is highpassed hard — the references measure −82 dB at 50 Hz — and outlives
its break by up to three times, which is why the quiet stretches between waves
measure brighter than the waves. The wash is a mid band with a slow random walk
whose coarseness follows the shore type. The bubbles are individual resonators,
and the sizzle behind them is thousands of sub-millimetre bubbles ringing
between 5 and 13 kHz, generated as a granular high band because at that rate
they merge anyway.

49 parameters, 17 presets, six shores, four breaker types, a filter and a full
ADSR, in one self-contained CLAP plugin for Linux and Windows. Hold a note and
the sea comes in for as long as you hold it.

### Variant 3

**ShoreBreak aims to give you surf with its beach detached.** Recorded surf comes
with everything fixed: the shore, the size of the breakers, how far up the sand
the microphone was and how much top end the distance had already eaten.
ShoreBreak makes each of those a control, because it builds the sea rather than
replaying a take of it.

It features five layers you can balance independently — break, foam, wash,
individual bubbles and the swell bed underneath — across 49 parameters, on six
shore types from sand to harbour wall, with Galvin's four breaker types, a
size-dependent decay rate, a bubbling precursor before the crest goes over, and
distance modelling with air absorption that measures within about a decibel of
its references from 800 Hz up. Waves interact rather than merely overlapping: a
break clears the foam still lying on the shore.

17 factory presets, fitted against 55 field recordings totalling 114 minutes, run
from a distant roar to big waves crashing. Native CLAP, Linux and Windows,
sample-accurate parameters, host modulation, bounded CPU cost, its own resizable
window with a preset browser, typed value entry and a wave-activity meter. Not a
single sample in it.
