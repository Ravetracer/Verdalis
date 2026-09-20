#include "params.h"

#include "verdalis/param_macros.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>

namespace insectswarm {

namespace {

const char *const kFilterNames[] = {"Lowpass", "Bandpass", "Highpass", "Notch"};
// In the order of the generated species table: rising wingbeat rate, which is
// also roughly falling body size.
const char *const kSpeciesNames[] = {"Hornet",   "Bumblebee", "Wasp",     "Housefly",
                                     "Honeybee", "Mosquito",  "Dragonfly"};

// Every default below is a measurement, not a preference. The numbers and how
// they were taken are in tools/analysis/README.md; the short version is 67
// field recordings of insects, 61 minutes of them, pitched frame by frame with
// a normalised autocorrelation and grouped by species. What separates the
// species is the wingbeat rate, the harmonic stack above it and how harmonic
// the sound is at all -- 86 to 464 Hz, stacks differing by 30 dB, and a
// harmonic-to-noise ratio running from 1 to 13 dB. What does not separate them
// is how far the rate wanders, so that is one knob here rather than a column of
// the species table.
const ParamDesc kParams[kNumParams] = {
   // ----------------------------------------------------------------- swarm
   ENUM(kParamSpecies, "species", "Species", "Swarm", 4.0, kSpeciesNames,
        "Which measured insect the swarm is made of. The choice sets four things at\n"
        "       once, all of them from the reference library: the wingbeat rate, the\n"
        "       tilt and the resonance fitted to that species' measured harmonic stack,\n"
        "       and how harmonic its buzz is to begin with.\n"
        "\n"
        "       That last one is the difference nobody expects. A honeybee's buzz\n"
        "       measures a harmonic-to-noise ratio of 4.3 dB and a mosquito's 12.8 --\n"
        "       so a bee is very nearly as much noise as tone, which is exactly why a\n"
        "       stack of oscillators never sounds like one, and a mosquito really is\n"
        "       the thin whine it seems to be.\n"
        "\n"
        "       Dragonfly is the odd row. Only 8 per cent of its frames are periodic at\n"
        "       all, against 56 to 94 for everything else: it is a clatter of wings and\n"
        "       not a buzz, and it is shipped that way rather than forced into a tone\n"
        "       the recordings do not contain."),
   LIN(kParamRateShift, "rate_shift", "Rate", "Swarm", -24.0, 24.0, 0.0, "st",
       "Shifts the wingbeat rate off the measured one, in semitones. At zero each\n"
       "       species beats at its measured median: 85.7 Hz for a hornet, 143.4 for a\n"
       "       bumblebee, 150.6 for a wasp, 193.1 for a housefly, 221.2 for a honeybee\n"
       "       and 463.7 for a mosquito.\n"
       "\n"
       "       It is an offset rather than an absolute rate on purpose. The species row\n"
       "       stays the thing that was measured, and this says how far from it you\n"
       "       have gone."),
   STEP(kParamCount, "count", "Count", "Swarm", 1.0, 64.0, 12.0, "",
        "How many individuals are flying. One is one insect; sixty-four is a hive.\n"
        "\n"
        "       The count is what turns a tone into a swarm, and the mechanism is\n"
        "       measured rather than assumed: a single close insect measures a\n"
        "       harmonic-to-noise ratio of 8 to 13 dB, while the library's hive and\n"
        "       swarm recordings measure 0 to 2 dB at the same clarity. Many\n"
        "       fundamentals at once *are* noise. Nothing else has to be added to make\n"
        "       a crowd sound like one."),
   LIN(kParamSwarmLevel, "swarm_level", "Swarm Level", "Swarm", -60.0, 6.0, -14.0, "dB",
       "Level of the swarm. The whole layer is normalised for the count first, so "
       "adding individuals thickens the sound without also making it louder."),
   LIN(kParamSpread, "spread", "Spread", "Swarm", 0.0, 400.0, 150.0, "ct",
       "How widely the individuals' wingbeat rates scatter, in cents.\n"
       "\n"
       "       This is the one default in the plugin that is calibrated rather than\n"
       "       measured, and the reason is worth stating plainly: the library cannot\n"
       "       measure it. A hive recording yields one dominant pitch frame by frame,\n"
       "       not sixty separate ones, so what comes out of it is how much *that* pitch\n"
       "       moved -- which is Wander, and is a different quantity.\n"
       "\n"
       "       What the library does measure is where the scatter ends up. A single\n"
       "       close insect comes out at 8 to 13 dB harmonic-to-noise and a hive or a\n"
       "       swarm at 0 to 2, because many fundamentals at once *are* noise. So this\n"
       "       is set to the value that lands there: at the default Count of twelve,\n"
       "       150 cents renders at 1.7 dB. At 70 it renders at 4.0 and still sounds\n"
       "       like a small number of insects, which is what it is.\n"
       "\n"
       "       It does nothing at all at a Count of one, and correctly so -- one insect\n"
       "       is one insect however wide the distribution it was drawn from."),
   LIN(kParamWander, "wander", "Wander", "Swarm", 0.0, 200.0, 18.0, "ct",
       "How far one individual's rate drifts while it flies, in cents per 85 ms\n"
       "       frame. Measured 10 to 24 across the species, median 15.\n"
       "\n"
       "       This is the number that buries the Doppler shift. A bee crossing at\n"
       "       3 m/s shifts its own pitch by 30 cents; its rate wanders by more than\n"
       "       that on its own, which is why a flyby reads as a level change and not as\n"
       "       a pitch bend. See Speed."),
   LOG(kParamWanderRate, "wander_rate", "Wander Rate", "Swarm", 0.6419, 0.1, 20.0, "Hz",
       "How fast that drifting is -- the corner of the filtered noise driving it. Low "
       "and the swarm breathes; high and every individual is unsteady on the note."),
   PCT(kParamRasp, "rasp", "Rasp", "Swarm", 0.5,
       "How much of one individual's buzz is turbulent air rather than the beat\n"
       "       itself. It is the single control over how tonal the layer is, and its\n"
       "       scale is the measurement: the species row carries that species' measured\n"
       "       harmonic-to-noise ratio, and this crossfades from twice that down to\n"
       "       nothing at all.\n"
       "\n"
       "       At the centre the layer reproduces the measurement. Turned down a\n"
       "       honeybee becomes the clean buzz it is usually drawn as and never is;\n"
       "       turned up it becomes the rasp it actually measures as."),

   // ------------------------------------------------------------------ wing
   BIPCT(kParamTilt, "tilt", "Tilt", "Wing", 0.0,
         "Tilts the fitted shelf brighter or darker. At zero each species has the tilt\n"
         "       fitted to its measured stack -- 3.5 dB for a honeybee, 36 for a\n"
         "       housefly, which is the difference between a buzz with a top end and one\n"
         "       that is all fundamental."),
   LIN(kParamFormant, "formant", "Formant", "Wing", -24.0, 24.0, 0.0, "st",
       "Moves the wing resonance off its fitted centre, in semitones. Every species'\n"
       "       measured stack has the same form -- a peak somewhere between 150 and\n"
       "       460 Hz and then a fall of 5 to 10 dB per octave -- and the fitted centres\n"
       "       are 159 Hz for a hornet, 227 for a wasp, 307 for a honeybee, 415 for a\n"
       "       housefly, 459 for a bumblebee and 1323 for a mosquito.\n"
       "\n"
       "       Because the resonance is absolute and the wingbeat rate is not, moving\n"
       "       Rate without moving this is what a real insect does when it flies harder:\n"
       "       the beat changes and the body it radiates from does not."),
   BIPCT(kParamResonance, "resonance", "Resonance", "Wing", 0.0,
         "How sharp that resonance is, against its fitted value. The fits run from 0.16 "
         "for a housefly -- a broad hump -- to 0.94 for a hornet, a honeybee and a wasp, "
         "which is a distinct formant you can hear as a pitch of its own."),
   PCT(kParamStroke, "stroke", "Stroke", "Wing", 0.0,
       "How unequal the two half-strokes are. A wing produces a pressure pulse on the\n"
       "       downstroke and another on the upstroke, and when the two are equal the\n"
       "       odd harmonics cancel and the buzz sits an octave up.\n"
       "\n"
       "       At zero the two are identical, which is what the species fits assume, so\n"
       "       zero is where the shipped table is correct. Turning it up restores the\n"
       "       odd harmonics and drops the apparent pitch by an octave -- the audible\n"
       "       difference between a bumblebee, whose second harmonic measures 11.7 dB\n"
       "       *above* its fundamental, and a mosquito, whose does not."),
   PCT(kParamBite, "bite", "Bite", "Wing", 0.45,
       "How sharp each stroke's pulse is. A narrow pulse has harmonics far up the "
       "spectrum and reads as a hard, papery buzz; a wide one is soft and rounded. It "
       "is the excitation's width, before the species' own shelf and resonance shape "
       "it."),
   PCT(kParamFlutter, "flutter", "Flutter", "Wing", 0.25,
       "Amplitude modulation at the wingbeat rate itself: the individual getting "
       "louder and quieter as its wings turn relative to the listener. Small amounts "
       "give the buzz its live, unsettled quality; large amounts make it a tremolo."),

   // ---------------------------------------------------------------- flyby
   LOG(kParamFlybyRate, "flyby_rate", "Flyby Rate", "Flyby", 0.4170, 0.02, 5.0, "/s",
       "How often one individual passes the listener. Independent of the swarm: a "
       "flyby is its own voice on its own trajectory, and it is what makes an insect "
       "read as moving rather than as getting louder."),
   LIN(kParamFlybyLevel, "flyby_level", "Flyby Level", "Flyby", -60.0, 6.0, -18.0, "dB",
       "Level of the flyby layer at its closest point."),
   LIN(kParamFlybyRise, "flyby_rise", "Rise", "Flyby", 0.0, 36.0, 13.3, "dB",
       "How far the pass rises above its own approach. Measured at a median of\n"
       "       13.3 dB across the ten recordings in the library that are a single clean\n"
       "       pass, from 9.5 to 44.2.\n"
       "\n"
       "       This is the flyby's real cue. The pitch barely moves; the level moves a\n"
       "       great deal."),
   LOG(kParamFlybyPass, "flyby_pass", "Pass Time", "Flyby", 0.5973, 0.1, 8.0, "s",
       "How long the pass takes, measured as the width of the level bump 6 dB down "
       "from its peak. The library's clean passes measure 0.26 to 6.8 s, median 1.37 -- "
       "and the spread is the pass distance, not the speed: a near miss is over in a "
       "quarter of a second whatever the insect is doing."),
   LIN(kParamFlybySpeed, "flyby_speed", "Speed", "Flyby", 0.2, 20.0, 3.2, "m/s",
       "Flight speed, which sets the Doppler shift through (c+v)/(c-v).\n"
       "\n"
       "       It is here because it is physically right, not because it is audible,\n"
       "       and the library is clear about which: at the measured median of 3.2 m/s\n"
       "       the shift across a pass is 30 cents, while the same insects' wingbeat\n"
       "       rates wander by 43 to 103 cents on their own. The Doppler is buried in\n"
       "       the wander, and the seven recordings here that appear to show a large\n"
       "       one are showing an insect changing gear. Turn it well past anything an\n"
       "       insect can do and it becomes an effect."),
   PCT(kParamFlybySweep, "flyby_sweep", "Sweep", "Flyby", 0.8,
       "How far across the stereo field a pass travels. At zero it arrives and leaves "
       "in the same place."),

   // ----------------------------------------------------------- stridulation
   LIN(kParamStridLevel, "strid_level", "Stridulate Level", "Stridulate", -60.0, 6.0, -60.0,
       "dB",
       "Level of the stridulation layer. Off by default: it is a second instrument "
       "sharing a window with the first, and most presets want one or the other."),
   LOG(kParamCarrier, "carrier", "Carrier", "Stridulate", 0.6465, 800.0, 16000.0, "Hz",
       "Where the resonant body rings. Measured as the spectral peak: 5549 Hz across "
       "the fourteen cicada references and 4518 across the nine cricket ones, with "
       "individual recordings from 1963 Hz to 14.7 kHz."),
   LOG(kParamCarrierQ, "carrier_q", "Carrier Q", "Stridulate", 0.4098, 2.0, 200.0, "",
       "How sharp that resonance is, measured as the peak frequency over its 6 dB\n"
       "       bandwidth. This is the one number that separates the two mechanisms\n"
       "       cleanly: 13.2 for a cicada, 25.8 for a cricket.\n"
       "\n"
       "       A cricket is nearly a sine wave. A cicada is a rattle with a colour."),
   LOG(kParamPulseRate, "pulse_rate", "Pulse Rate", "Stridulate", 0.8320, 5.0, 600.0, "Hz",
       "How fast the clicks arrive inside one chirp. Measured on the envelope of the "
       "carrier band alone, so it is the insect's rate and not whatever else was in "
       "the recording: 268 a second for a cicada, 36 for a cricket."),
   LOG(kParamEchemeRate, "echeme_rate", "Echeme Rate", "Stridulate", 0.7811, 0.2, 40.0, "Hz",
       "How often the chirps themselves come. An echeme is one burst of clicks; this "
       "is the slower rhythm they are grouped into. Measured 12.5 a second for a "
       "cicada and 10.5 for a cricket, with the library running from 0.8 to 19.9."),
   PCT(kParamDuty, "duty", "Duty", "Stridulate", 0.48,
       "What share of the time the chirping is sounding. Measured at 0.48 for a cicada "
       "and 0.33 for a cricket -- a cicada is very nearly continuous, a cricket leaves "
       "two thirds of its time silent, and that gap is most of what makes it a chirp."),
   STEP(kParamChorus, "chorus", "Chorus", "Stridulate", 1.0, 32.0, 6.0, "",
        "How many of them are calling. Each gets its own carrier, its own rate and its "
        "own phase, so a chorus smears the echeme rhythm out exactly as a real one "
        "does -- the library's chorus recordings measure an echeme periodicity half as "
        "clear as its solo ones."),
   PCT(kParamStridSpread, "strid_spread", "Scatter", "Stridulate", 0.5,
       "How unalike the callers are: how far their carriers, their click rates and\n"
       "       their chirp rates scatter around the settings above. At zero they are one\n"
       "       insect heard several times over, which is the thing a chorus never sounds\n"
       "       like.\n"
       "\n"
       "       Its range is set by the library rather than chosen. A chorus recording\n"
       "       measures the *composite* resonance, and the references put that at Q 13.2\n"
       "       for a cicada and 25.8 for a cricket -- which cannot happen if the callers\n"
       "       are spread much wider than a thirteenth of their carrier. So at the top\n"
       "       of this knob the carriers scatter by 3 per cent and no more; a rendered\n"
       "       chorus at 10 per cent measures half the Q the recordings do. The click\n"
       "       and chirp rates scatter further, because those are heard as a rhythm and\n"
       "       nothing in the library bounds them."),

   // ------------------------------------------------------------------- air
   PCT(kParamDistance, "distance", "Distance", "Air", 0.2,
       "How far away the insects are. Air absorption plus a downward tilt, and "
       "bypassed outright at zero, because no distance means no air to absorb."),
   PCT(kParamAir, "air", "Air", "Air", 0.5,
       "How much of the top end that distance costs. Humid air absorbs less than dry."),
   PCT(kParamWidth, "width", "Width", "Air", 0.75,
       "How widely the individuals are spread across the stereo field. The library's "
       "stereo references measure a side-to-mid ratio with a median of -7.8 dB for "
       "single insects, so even one insect is not a point source."),
   PCT(kParamSpaceAmount, "space_amount", "Space", "Air", 0.12,
       "How much reverberant field there is. Kept low by default: these are outdoor "
       "recordings, and a field has almost nothing close enough to reflect off."),
   PCT(kParamSpaceSize, "space_size", "Size", "Air", 0.55, "How big that space is."),
   PCT(kParamSpaceDamping, "space_damping", "Damping", "Air", 0.5,
       "How quickly the space eats the top end. Foliage eats a great deal."),

   // ------------------------------------------------------------------- bed
   LIN(kParamBedLevel, "bed_level", "Bed Level", "Bed", -60.0, 6.0, -60.0, "dB",
       "Level of the field the insects are in: a quiet broadband bed under everything "
       "else. Off by default, because what it models is the recordist's afternoon "
       "rather than the insect, and a preset that wants it should say so."),
   LOG(kParamBedTone, "bed_tone", "Bed Tone", "Bed", 0.5190, 100.0, 12000.0, "Hz",
       "Where the bed's energy sits."),
   BIPCT(kParamBedTilt, "bed_tilt", "Bed Tilt", "Bed", -0.2,
         "Tilts the bed brighter or darker about its tone."),

   // ---------------------------------------------------------------- filter
   ENUM(kParamFilterType, "filter_type", "Filter Type", "Filter", 0.0, kFilterNames,
        "Shape of the output filter."),
   LOG(kParamHighpass, "highpass", "Highpass", "Filter", 0.3010, 20.0, 2000.0, "Hz",
       "Permanent highpass. The default of 80 Hz sits just under the lowest wingbeat "
       "in the library -- a hornet at 85.7 Hz -- so nothing the engine generates is cut "
       "and nothing below it is generated."),
   LOG(kParamFilterCutoff, "filter_cutoff", "Filter Cutoff", "Filter", 1.0, 30.0, 20000.0, "Hz",
       "Corner of the output filter."),
   PCT(kParamFilterReso, "filter_reso", "Filter Resonance", "Filter", 0.1,
       "Resonance of the output filter."),
   PCT(kParamFilterKeyTrack, "filter_keytrack", "Filter Key Track", "Filter", 0.0,
       "How far the filter follows the played note."),

   // -------------------------------------------------------------- envelope
   LOG(kParamAttack, "attack", "Attack", "Envelope", 0.5759, 1.0, 20000.0, "ms",
       "Fade-in of the whole swarm when a note starts."),
   LOG(kParamDecay, "decay", "Decay", "Envelope", 0.5283, 5.0, 20000.0, "ms",
       "Fall from the initial level to the sustain."),
   PCT(kParamSustain, "sustain", "Sustain", "Envelope", 1.0,
       "Level held while the note is down."),
   LOG(kParamRelease, "release", "Release", "Envelope", 0.5969, 5.0, 30000.0, "ms",
       "Fade-out after the note is let go."),
   PCT(kParamVelToLevel, "vel_to_level", "Velocity To Level", "Envelope", 0.4,
       "How much velocity sets the level."),
   PCT(kParamVelToSwarm, "vel_to_swarm", "Velocity To Swarm", "Envelope", 0.3,
       "How much velocity sets how many individuals are flying and how hard they beat "
       "-- which is what a disturbed swarm actually does."),

   // ---------------------------------------------------------------- output
   LIN(kParamGain, "gain", "Output Gain", "Output", -60.0, 12.0, -3.0, "dB",
       "Output level."),
   STEP(kParamMaxVoices, "max_voices", "Max Individuals", "Output", 8.0, 256.0, 128.0, "",
        "Ceiling on individuals sounding at once across every held note, so the cost "
        "is bounded whatever Count and the note count ask for."),
   STEP(kParamSeed, "seed", "Random Seed", "Output", 0.0, 999.0, 0.0, "",
        "Non-zero gives the same swarm every time. Zero is always different."),

   // ----------------------------------------------------------- swarm, cont.
   // Appended because the ids are persisted; they belong to the Swarm module
   // and the window and the manual both group them there.
   LIN(kParamRoam, "roam", "Roam", "Swarm", 0.0, 12.0, 4.0, "dB",
       "How far one individual's level drifts while it flies, in decibels.\n"
       "\n"
       "       An insect is never a fixed distance from the microphone. It closes on\n"
       "       it and backs off again, and at close range that is most of what makes a\n"
       "       single fly or mosquito sound alive rather than like a held tone -- the\n"
       "       inverse square law is steep enough that a few centimetres are several\n"
       "       decibels. This is the amplitude counterpart of Wander, which does the\n"
       "       same thing to the rate.\n"
       "\n"
       "       It applies to each individual separately, so it scales itself: at a\n"
       "       Count of one it is the whole sound moving, and in a swarm of sixty the\n"
       "       independent drifts largely cancel and it reads as the crowd breathing.\n"
       "       That is why it is turned up on the single-insect presets and left low\n"
       "       on the hives.\n"
       "\n"
       "       The excursion is bounded and the compensation is set so that the\n"
       "       loudest moments stay where the Swarm Level put them: turning this up\n"
       "       adds the quieter moments rather than louder ones, so it can never\n"
       "       overload a patch that was in range without it. A solo insect loses\n"
       "       about 2 dB of average level at 8 dB of Roam, which is the insect\n"
       "       spending time further away and is the point.\n"
       "\n"
       "       It does not touch the Flyby layer, which already has a trajectory and\n"
       "       gets its level from the geometry."),
   LOG(kParamRoamRate, "roam_rate", "Roam Rate", "Swarm", 0.5654, 0.02, 4.0, "Hz",
       "How fast that drifting is -- the corner of the filtered noise driving it. An "
       "insect crosses its own body length many times a second but changes where it "
       "is in the room over seconds, so this sits well below Wander Rate."),

   // ---------------------------------------------------- stridulate, appended
   PCT(kParamScrape, "scrape", "Scrape", "Stridulate", 0.45,
       "How much of each pulse period the insect is actually driving its\n"
       "       resonator -- one wing stroke dragging a scraper across a file, or one\n"
       "       tymbal contraction buckling its ribs.\n"
       "\n"
       "       At the bottom it is a single click ringing the body, which is what\n"
       "       0.2.0 always did and what its own calibration comment recorded as\n"
       "       \"6 per cent of it is sounding\". Measured inside a chirp, the\n"
       "       references are on for a median 0.27 of the time for a cricket and 0.79\n"
       "       for a cicada -- so a click is right for neither, and the two mechanisms\n"
       "       do not agree with each other either. This is the control that separates\n"
       "       them."),
};

#undef LIN
#undef PCT
#undef BIPCT
#undef LOG
#undef STEP
#undef ENUM

} // namespace

const ParamDesc *paramTable() { return kParams; }

const ParamDesc *paramById(uint32_t id) { return paramByIdIn(kParams, kNumParams, id); }

const ParamDesc *paramByKey(const char *key) { return paramByKeyIn(kParams, kNumParams, key); }

} // namespace insectswarm
