#include "params.h"

#include "verdalis/param_macros.h"

#include <cstring>

namespace verdaliscene {

namespace {

const char *const kGateNames[] = {"Always", "Transport", "Notes"};
const char *const kTailsNames[] = {"Ring Out", "Release"};
const char *const kFilterNames[] = {"Lowpass", "Bandpass", "Highpass", "Notch"};
const char *const kStereoNames[] = {"Stereo", "Mono"};
const char *const kOnNames[] = {"Off", "On"};
const char *const kDelayModeNames[] = {"Mono", "Stereo", "Ping-Pong"};
const char *const kGlideNames[] = {"Tape", "Fade"};
const char *const kStageNames[] = {"4", "6", "8"};
const char *const kPanShapeNames[] = {"Sine", "Triangle", "Drift"};
// Note values for a synced delay, shortest first, and their lengths in
// quarter notes. T is a triplet (2/3), D dotted (3/2).
const char *const kNoteNames[] = {"1/64",   "1/32 T", "1/32",  "1/16 T", "1/32 D",
                                  "1/16",   "1/8 T",  "1/16 D", "1/8",   "1/4 T",
                                  "1/8 D",  "1/4",    "1/2 T", "1/4 D",  "1/2",
                                  "1/1 T",  "1/2 D",  "1/1",   "1/1 D",  "2/1"};
const double kNoteBeats[] = {0.0625, 1.0 / 12.0, 0.125, 1.0 / 6.0, 0.1875, 0.25, 1.0 / 3.0,
                             0.375,  0.5,        2.0 / 3.0, 0.75,   1.0,  4.0 / 3.0, 1.5,
                             2.0,    8.0 / 3.0,  3.0,   4.0,    6.0,   8.0};
static_assert(sizeof(kNoteNames) / sizeof(kNoteNames[0]) == sizeof(kNoteBeats) / sizeof(kNoteBeats[0]),
              "every note value needs its length");

// Log defaults are the raw 0..1 position: log(def / lo) / log(hi / lo).
const ParamDesc kScene[kNumSceneParams] = {
   ENUM(kParamGate, "gate", "Gate", "Envelope", 2.0, kGateNames,
        "What starts and ends the scene. Notes plays while any key is held, Transport "
        "follows the host's play and stop, Always plays from the moment the plugin runs. "
        "Opening the gate starts every layer, closing it lets every layer and the scene fade "
        "out on their own releases. A key never plays a single event in a layer."),
   LOG(kParamAttack, "attack", "Attack", "Envelope", 0.7094, 1.0, 30000.0, "ms",
       "How long the whole scene takes to fade in when the gate opens."),
   LOG(kParamDecay, "decay", "Decay", "Envelope", 0.6701, 1.0, 30000.0, "ms",
       "Fall from full level to the sustain level after the fade-in."),
   PCT(kParamSustain, "sustain", "Sustain", "Envelope", 1.0,
       "Level the scene holds while the gate is open."),
   LOG(kParamRelease, "release", "Release", "Envelope", 0.7766, 1.0, 30000.0, "ms",
       "How long the scene takes to fade out when the gate closes."),

   ENUM(kParamFilterType, "filter_type", "Filter Type", "Filter", 0.0, kFilterNames,
        "Response of the filter over the whole scene."),
   LOG(kParamHighpass, "highpass", "Highpass", "Filter", 0.0, 20.0, 2000.0, "Hz",
       "Rolls off the bottom of the whole scene at 12 dB/oct. Off at the far left."),
   LOG(kParamFilterCutoff, "filter_cutoff", "Filter Cutoff", "Filter", 1.0, 20.0, 20000.0, "Hz",
       "Corner of the scene filter. A lowpass here is the scene heard through a wall."),
   PCT(kParamFilterReso, "filter_reso", "Filter Resonance", "Filter", 0.1,
       "Emphasis at the cutoff frequency."),

   {kParamWidth, "width", "Width", "Output", 0.0, 2.0, 1.0, ParamKind::Percent, 0, 0, "%",
    nullptr, 0,
    "Stereo width of the whole scene: mono at the left, as recorded at 100 %, wider "
    "beyond."},
   LIN(kParamGain, "gain", "Output Gain", "Output", -60.0, 12.0, 0.0, "dB",
       "Final level of the whole scene, after the envelope and the filter."),

   BIPCT(kParamAttackCurve, "attack_curve", "Attack Curve", "Envelope", 0.0,
         "Bends the scene's fade-in. At 0 % it has its natural shape, rising quickly and "
         "easing into full level; lower values straighten it and then hold it back so the "
         "scene swells in late, higher values bring it in at once. The length stays the same."),
   BIPCT(kParamDecayCurve, "decay_curve", "Decay Curve", "Envelope", 0.0,
         "Bends the fall from full level to Sustain. At 0 % it drops quickly and eases out; "
         "lower values straighten it and then hold the level up before it falls, higher values "
         "drop it at once. The length stays the same."),
   BIPCT(kParamReleaseCurve, "release_curve", "Release Curve", "Envelope", 0.0,
         "Bends the scene's fade-out when the gate closes. At 0 % it falls quickly and trails "
         "off; lower values straighten it and then hold the scene up before it goes, higher "
         "values let it go at once. The length stays the same."),
   ENUM(kParamFxTails, "tails", "FX Tails", "Output", 0.0, kTailsNames,
        "What the scene's release does to the effects. Ring Out fades what goes into every "
        "layer's effects and the scene's own, so a reverb or a delay rings on by itself after "
        "the gate closes, for as long as it lasts. Release fades what comes out of them, so "
        "the tails fade with the rest of the scene and are gone when its release has run "
        "out."),
};

// A layer's place in the scene. The keys are the ones a scene preset uses in
// a layer's section.
const ParamDesc kSlot[kNumSlotParams] = {
   STEP(kSlotActive, "layer_active", "Active", "Layer", 0.0, 1.0, 0.0, "",
        "Whether the layer exists. Added and removed in the window, not automated."),
   LIN(kSlotLevel, "layer_level", "Level", "Layer", -60.0, 12.0, 0.0, "dB",
       "The layer's fader in the scene mixer, on top of the layer's own output gain."),
   BIPCT(kSlotPan, "layer_pan", "Pan", "Layer", 0.0,
         "Where the layer sits. A stereo layer is balanced, a mono one is placed."),
   ENUM(kSlotStereo, "layer_stereo", "Stereo", "Layer", 0.0, kStereoNames,
        "Stereo keeps the layer's own image. Mono folds it to one point that Pan then "
        "places, for a source that should come from one direction."),
   LIN(kSlotShotRate, "layer_shot_rate", "Shot Rate", "Layer", 0.0, 30.0, 2.0, "/min",
       "Bird and night layers only. How often, at random, the phrase a played note "
       "would fire sings on its own, at the layer's Shot Level. Nothing fires while "
       "Shot Level is off, and zero stops it."),
   BIPCT(kSlotAttackCurve, "layer_attack_curve", "Attack Curve", "Layer", 0.0,
         "Bends the layer's own fade-in, the one its Attack sets. At 0 % it has the plugin's "
         "natural shape; lower values straighten it and then make the layer swell in late, "
         "higher values bring it in at once. The length stays the same."),
   BIPCT(kSlotDecayCurve, "layer_decay_curve", "Decay Curve", "Layer", 0.0,
         "Bends the layer's own fall from full level to its Sustain. At 0 % it has the "
         "plugin's natural shape; lower values hold the level up before it falls, higher "
         "values drop it at once. Not on thunder, whose envelope has no decay."),
   BIPCT(kSlotReleaseCurve, "layer_release_curve", "Release Curve", "Layer", 0.0,
         "Bends the layer's own fade-out when the gate closes, the one its Release sets. At "
         "0 % it has the plugin's natural shape; lower values hold the layer up before it "
         "goes, higher values let it go at once. The length stays the same."),
};

// One channel's effects. The same table serves the scene's chain and every
// layer's; names are short because the window shows them inside a panel that
// already says which effect they belong to, and the host's names add it.
const ParamDesc kFx[kNumFxParams] = {
   ENUM(kFxReverbOn, "fx_reverb_on", "On", "Reverb", 0.0, kOnNames,
        "Switches the reverb in. Its buffers are made the first time it is."),
   PCT(kFxReverbMix, "fx_reverb_mix", "Mix", "Reverb", 0.3,
       "Dry against wet, equal power: 0 % is untouched, 100 % is the reverb alone."),
   LOG(kFxReverbPredelay, "fx_reverb_predelay", "Pre-Delay", "Reverb", 0.4853, 0.5, 1000.0, "ms",
       "Time before the tail begins: the distance to the nearest walls."),
   PCT(kFxReverbSize, "fx_reverb_size", "Size", "Reverb", 0.55,
       "The space's dimension, from a small room to a vast hall. Sets the spacing of the "
       "echoes, not how long they last."),
   LOG(kFxReverbDecay, "fx_reverb_decay", "Decay", "Reverb", 0.5032, 200.0, 120000.0, "ms",
       "How long the tail takes to fall by 60 dB, up to two minutes. Exact at every Size."),
   PCT(kFxReverbDiffusion, "fx_reverb_diffusion", "Diffusion", "Reverb", 0.75,
       "How quickly the first echoes melt into a smooth wash. Low keeps discrete reflections."),
   PCT(kFxReverbDamping, "fx_reverb_damping", "Damping", "Reverb", 0.45,
       "How much faster the top end dies than the rest: soft surfaces, air over a long "
       "distance. 0 % keeps the highs as long as everything else."),
   LOG(kFxReverbDampFreq, "fx_reverb_damp_freq", "Damp Freq", "Reverb", 0.6, 500.0, 16000.0, "Hz",
       "Where Damping starts to act."),
   PCT(kFxReverbModDepth, "fx_reverb_mod_depth", "Mod Depth", "Reverb", 0.35,
       "Slow drift inside the tail, which keeps a long decay from ringing metallic. Higher "
       "is lusher and, at the top, gently chorused."),
   LOG(kFxReverbModRate, "fx_reverb_mod_rate", "Mod Rate", "Reverb", 0.5, 0.05, 5.0, "Hz",
       "How fast the tail drifts."),
   LOG(kFxReverbLowCut, "fx_reverb_low_cut", "Low Cut", "Reverb", 0.0, 20.0, 2000.0, "Hz",
       "Takes the bottom out of the reverb only, 12 dB/oct. Off at the far left."),
   LOG(kFxReverbHighCut, "fx_reverb_high_cut", "High Cut", "Reverb", 1.0, 500.0, 20000.0, "Hz",
       "Takes the top out of the reverb only, 12 dB/oct. Off at the far right."),
   PCT(kFxReverbWidth, "fx_reverb_width", "Width", "Reverb", 1.0,
       "Stereo width of the tail, from mono to fully decorrelated."),
   ENUM(kFxReverbFreeze, "fx_reverb_freeze", "Freeze", "Reverb", 0.0, kOnNames,
        "Holds the tail as it is, indefinitely, and stops anything new entering it."),

   ENUM(kFxDelayOn, "fx_delay_on", "On", "Delay", 0.0, kOnNames,
        "Switches the delay in. Its buffers are made the first time it is."),
   ENUM(kFxDelayMode, "fx_delay_mode", "Mode", "Delay", 1.0, kDelayModeNames,
        "Mono: one line, heard in both ears. Stereo: a line per side. Ping-Pong: the "
        "repeats start on the left and bounce from side to side."),
   ENUM(kFxDelaySync, "fx_delay_sync", "Sync", "Delay", 0.0, kOnNames,
        "Off sets the time in milliseconds, On as a note value at the host's tempo "
        "(120 BPM when the host gives none)."),
   LOG(kFxDelayTime, "fx_delay_time", "Time", "Delay", 0.6595, 1.0, 8000.0, "ms",
       "Time between repeats, up to eight seconds, while Sync is off."),
   ENUM(kFxDelayNote, "fx_delay_note", "Note", "Delay", 10.0, kNoteNames,
        "Time between repeats as a note value, while Sync is on. T is a triplet, D dotted."),
   BIPCT(kFxDelayOffset, "fx_delay_offset", "Offset", "Delay", 0.0,
         "The right side's time against the left's, up to half again longer or shorter. "
         "Unequal sides make a ping-pong skip and a stereo delay spread."),
   {kFxDelayFeedback, "fx_delay_feedback", "Feedback", "Delay", 0.0, 1.5, 0.45,
    ParamKind::Percent, 0, 0, "%", nullptr, 0,
    "How much of each repeat comes round again. Past 100 % the repeats build into a "
    "held, saturating wall instead of dying away."},
   LOG(kFxDelayLowCut, "fx_delay_low_cut", "Low Cut", "Delay", 0.301, 20.0, 2000.0, "Hz",
       "Thins every repeat a little more, 12 dB/oct in the loop. Off at the far left."),
   LOG(kFxDelayHighCut, "fx_delay_high_cut", "High Cut", "Delay", 0.7516, 500.0, 20000.0, "Hz",
       "Darkens every repeat a little more, 12 dB/oct in the loop. Off at the far right."),
   PCT(kFxDelaySaturation, "fx_delay_saturation", "Saturation", "Delay", 0.2,
       "Tape-like drive in the loop: rounds the repeats off and sets how loud a run-away "
       "feedback settles."),
   PCT(kFxDelayWow, "fx_delay_wow", "Wow", "Delay", 0.1,
       "Wavers the time like a tape echo, by a slow swing and a slower random drift."),
   LOG(kFxDelayWowRate, "fx_delay_wow_rate", "Wow Rate", "Delay", 0.4896, 0.05, 8.0, "Hz",
       "How fast the time wavers."),
   PCT(kFxDelayDiffusion, "fx_delay_diffusion", "Diffusion", "Delay", 0.0,
       "Smears each repeat further than the last, so the echoes dissolve into a cloud. "
       "The time between them stays the same."),
   PCT(kFxDelayDucking, "fx_delay_ducking", "Ducking", "Delay", 0.0,
       "Pulls the repeats down while the layer is loud and lets them up as it falls quiet."),
   ENUM(kFxDelayGlide, "fx_delay_glide", "Glide", "Delay", 0.0, kGlideNames,
        "How a time change arrives. Tape slides to it and bends the pitch of the repeats on "
        "the way, Fade crossfades to it without a pitch sweep."),
   PCT(kFxDelayWidth, "fx_delay_width", "Width", "Delay", 1.0,
       "Stereo width of the repeats, from mono to as wide as the mode makes them."),
   PCT(kFxDelayMix, "fx_delay_mix", "Mix", "Delay", 0.3,
       "Dry against wet, equal power: 0 % is untouched, 100 % is the repeats alone."),

   ENUM(kFxChorusOn, "fx_chorus_on", "On", "Chorus", 0.0, kOnNames, "Switches the chorus in."),
   LOG(kFxChorusRate, "fx_chorus_rate", "Rate", "Chorus", 0.5426, 0.02, 5.0, "Hz",
       "How fast the voices drift. Each voice runs a little faster than the one before."),
   PCT(kFxChorusDepth, "fx_chorus_depth", "Depth", "Chorus", 0.5,
       "How far the voices drift around their delay."),
   LOG(kFxChorusDelay, "fx_chorus_delay", "Delay", "Chorus", 0.5981, 2.0, 40.0, "ms",
       "How far behind the original the voices sit. Short is a shimmer, long a doubling."),
   STEP(kFxChorusVoices, "fx_chorus_voices", "Voices", "Chorus", 1.0, 4.0, 2.0, "",
        "How many drifting copies, spread evenly around the cycle."),
   PCT(kFxChorusWidth, "fx_chorus_width", "Width", "Chorus", 1.0,
       "Moves the right side's voices a quarter cycle on, for a wide chorus."),
   PCT(kFxChorusMix, "fx_chorus_mix", "Mix", "Chorus", 0.5, "Dry against the voices."),

   ENUM(kFxFlangerOn, "fx_flanger_on", "On", "Flanger", 0.0, kOnNames, "Switches the flanger in."),
   LOG(kFxFlangerRate, "fx_flanger_rate", "Rate", "Flanger", 0.3649, 0.02, 5.0, "Hz",
       "How fast the sweep goes up and down."),
   PCT(kFxFlangerDepth, "fx_flanger_depth", "Depth", "Flanger", 0.6,
       "How far the sweep goes, up to three octaves."),
   LOG(kFxFlangerManual, "fx_flanger_manual", "Manual", "Flanger", 0.588, 0.1, 10.0, "ms",
       "Where the sweep starts. Short is a high, airy jet, long a low, hollow one."),
   BIPCT(kFxFlangerFeedback, "fx_flanger_feedback", "Feedback", "Flanger", 0.5,
         "Sharpens the notches into resonances; negative gives the hollow, odd-harmonic sound."),
   PCT(kFxFlangerWidth, "fx_flanger_width", "Width", "Flanger", 0.5,
       "Offsets the right side's sweep by up to a quarter cycle."),
   PCT(kFxFlangerMix, "fx_flanger_mix", "Mix", "Flanger", 0.5,
       "Dry against swept. 50 % gives the deepest notches."),

   ENUM(kFxPhaserOn, "fx_phaser_on", "On", "Phaser", 0.0, kOnNames, "Switches the phaser in."),
   LOG(kFxPhaserRate, "fx_phaser_rate", "Rate", "Phaser", 0.417, 0.02, 5.0, "Hz",
       "How fast the notches sweep."),
   PCT(kFxPhaserDepth, "fx_phaser_depth", "Depth", "Phaser", 0.7,
       "How far they sweep, up to two octaves either side of Centre."),
   LOG(kFxPhaserCentre, "fx_phaser_centre", "Centre", "Phaser", 0.5637, 100.0, 4000.0, "Hz",
       "The middle of the sweep."),
   PCT(kFxPhaserFeedback, "fx_phaser_feedback", "Feedback", "Phaser", 0.5,
       "Deepens the notches into a vocal resonance."),
   ENUM(kFxPhaserStages, "fx_phaser_stages", "Stages", "Phaser", 1.0, kStageNames,
        "Allpass stages: two notches for four, three for six, four for eight."),
   PCT(kFxPhaserWidth, "fx_phaser_width", "Width", "Phaser", 0.5,
       "Offsets the right side's sweep by up to a quarter cycle."),
   PCT(kFxPhaserMix, "fx_phaser_mix", "Mix", "Phaser", 0.5,
       "Dry against phased. 50 % gives the deepest notches."),

   ENUM(kFxWidenerOn, "fx_widener_on", "On", "Widener", 0.0, kOnNames,
        "Switches the stereo widener in."),
   {kFxWidenerWidth, "fx_widener_width", "Width", "Widener", 0.0, 2.0, 1.3, ParamKind::Percent, 0, 0,
    "%", nullptr, 0,
    "The side signal: 0 % folds to mono, 100 % leaves it, 200 % doubles it."},
   PCT(kFxWidenerDecorrelation, "fx_widener_decorrelation", "Spread", "Widener", 0.3,
       "Spreads a narrow sound across the field by sending different frequencies to "
       "different places. Sums back to the original in mono."),
   LOG(kFxWidenerBassMono, "fx_widener_bass_mono", "Bass Mono", "Widener", 0.0, 20.0, 500.0, "Hz",
       "Everything below this is centred. Off at the far left."),

   ENUM(kFxAutoPanOn, "fx_autopan_on", "On", "Auto Pan", 0.0, kOnNames,
        "Switches the auto-pan in."),
   LOG(kFxAutoPanRate, "fx_autopan_rate", "Rate", "Auto Pan", 0.3843, 0.01, 4.0, "Hz",
       "How fast the layer moves. 0.1 Hz is once across and back in ten seconds."),
   PCT(kFxAutoPanDepth, "fx_autopan_depth", "Depth", "Auto Pan", 0.5,
       "How far it moves, up to hard left and right."),
   ENUM(kFxAutoPanShape, "fx_autopan_shape", "Shape", "Auto Pan", 2.0, kPanShapeNames,
        "Sine and Triangle swing evenly; Drift wanders to a new random place each cycle, "
        "like something moving about."),
};

const FxKindInfo kFxKinds[kNumFxKinds] = {
   {"Reverb", "R", kFxReverbOn, kFxDelayOn - kFxReverbOn},
   {"Delay", "D", kFxDelayOn, kFxChorusOn - kFxDelayOn},
   {"Chorus", "C", kFxChorusOn, kFxFlangerOn - kFxChorusOn},
   {"Flanger", "F", kFxFlangerOn, kFxPhaserOn - kFxFlangerOn},
   {"Phaser", "P", kFxPhaserOn, kFxWidenerOn - kFxPhaserOn},
   {"Widener", "W", kFxWidenerOn, kFxAutoPanOn - kFxWidenerOn},
   {"Auto Pan", "A", kFxAutoPanOn, kNumFxParams - kFxAutoPanOn},
};

#undef LIN
#undef PCT
#undef BIPCT
#undef LOG
#undef STEP
#undef ENUM

// The manual's table: the scene's, then the placement ones without Active,
// which a preset expresses by having the section at all.
struct DocTable {
   ParamDesc entries[kNumParams];
   DocTable() {
      uint32_t n = 0;
      for (uint32_t i = 0; i < kNumSceneParams; ++i)
         entries[n++] = kScene[i];
      for (uint32_t i = kSlotLevel; i < kNumSlotParams; ++i)
         entries[n++] = kSlot[i];
      for (uint32_t i = 0; i < kNumFxParams; ++i)
         entries[n++] = kFx[i];
   }
};

} // namespace

const ParamDesc *sceneParamTable() { return kScene; }
const ParamDesc *slotParamTable() { return kSlot; }

const ParamDesc *paramTable() {
   static const DocTable t;
   return t.entries;
}

const ParamDesc *sceneParamByKey(const char *key) {
   return paramByKeyIn(kScene, kNumSceneParams, key);
}

const ParamDesc *slotParamByKey(const char *key) {
   return paramByKeyIn(kSlot, kNumSlotParams, key);
}

const ParamDesc *fxParamTable() { return kFx; }

const ParamDesc *fxParamByKey(const char *key) { return paramByKeyIn(kFx, kNumFxParams, key); }

const FxKindInfo &fxKind(int kind) { return kFxKinds[kind < 0 || kind >= kNumFxKinds ? 0 : kind]; }

int fxKindOf(uint32_t fxParam) {
   for (int k = kNumFxKinds - 1; k >= 0; --k)
      if (fxParam >= kFxKinds[k].first)
         return k;
   return 0;
}

double fxNoteBeats(int note) {
   const int n = fxNumNotes();
   return kNoteBeats[note < 0 ? 0 : (note >= n ? n - 1 : note)];
}

int fxNumNotes() { return static_cast<int>(sizeof(kNoteBeats) / sizeof(kNoteBeats[0])); }

} // namespace verdaliscene
