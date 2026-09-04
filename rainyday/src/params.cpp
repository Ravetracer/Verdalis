#include "params.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>

namespace rainyday {

namespace {

const char *const kSurfaceNames[] = {"Water", "Puddle", "Leaves",  "Wood",
                                     "Metal", "Glass",  "Concrete", "Fabric"};
const char *const kFilterNames[] = {"Lowpass", "Bandpass", "Highpass", "Notch"};

#define LIN(id, key, name, mod, lo, hi, def, unit, tip)                                            \
   { id, key, name, mod, lo, hi, def, ParamKind::Linear, 0, 0, unit, nullptr, 0, tip }
#define PCT(id, key, name, mod, def, tip)                                                          \
   { id, key, name, mod, 0.0, 1.0, def, ParamKind::Percent, 0, 0, "%", nullptr, 0, tip }
#define BIPCT(id, key, name, mod, def, tip)                                                        \
   { id, key, name, mod, -1.0, 1.0, def, ParamKind::Percent, 0, 0, "%", nullptr, 0, tip }
#define LOG(id, key, name, mod, def, dlo, dhi, unit, tip)                                          \
   { id, key, name, mod, 0.0, 1.0, def, ParamKind::Log, dlo, dhi, unit, nullptr, 0, tip }
#define STEP(id, key, name, mod, lo, hi, def, unit, tip)                                           \
   { id, key, name, mod, lo, hi, def, ParamKind::Stepped, 0, 0, unit, nullptr, 0, tip }
#define ENUM(id, key, name, mod, def, names, tip)                                                  \
   {                                                                                               \
      id, key, name, mod, 0.0, static_cast<double>(sizeof(names) / sizeof(names[0]) - 1), def,     \
         ParamKind::Enum, 0, 0, "", names, sizeof(names) / sizeof(names[0]), tip                   \
   }

// The full control surface. Ranges chosen so that a plain linear fader in the
// host's generic UI lands somewhere musically useful across its whole travel.
const ParamDesc kParams[kNumParams] = {
   LIN(kParamGain, "gain", "Output Gain", "Output", -60.0, 12.0, 0.0, "dB",
       "Final output level of the whole instrument."),

   LOG(kParamDensity, "density", "Density", "Rain", 0.62, 0.2, 5000.0, "drops/s",
       "Average droplet arrival rate. Loudness is compensated, so this is texture, not volume."),
   PCT(kParamClumping, "clumping", "Clumping", "Rain", 0.25,
       "How much the arrival rate fluctuates -- surges and lulls instead of an even patter."),
   LOG(kParamDropPitch, "drop_pitch", "Drop Pitch", "Rain", 0.5, 40.0, 9000.0, "Hz",
       "Base resonant frequency of a droplet. Big drops land below it, fine ones above."),
   LIN(kParamPitchSpread, "pitch_spread", "Pitch Spread", "Rain", 0.0, 5.0, 1.6, "oct",
       "Random octave spread on top of the pitch that drop size already implies."),
   LOG(kParamDropDecay, "drop_decay", "Drop Decay", "Rain", 0.54, 1.0, 1200.0, "ms",
       "Base ring time of a droplet. The surface scales it further."),
   PCT(kParamDecaySpread, "decay_spread", "Decay Spread", "Rain", 0.5,
       "Randomises ring time from drop to drop."),
   PCT(kParamTonality, "tonality", "Tonality", "Rain", 0.35,
       "Noisy splat at the left, pitched plink at the right."),
   PCT(kParamImpact, "impact", "Impact", "Rain", 0.5,
       "Weight of the broadband click at the moment of impact."),
   PCT(kParamSplash, "splash", "Splash", "Rain", 0.35,
       "Length and weight of the wet noise burst after the impact."),
   PCT(kParamLevelSpread, "level_spread", "Level Spread", "Rain", 0.6,
       "Skew of the drop-size distribution, which also spreads pitch and decay."),
   BIPCT(kParamChirp, "chirp", "Chirp", "Rain", 0.35,
         "Per-droplet pitch bend. Positive rises, the way a bubble in water does."),
   ENUM(kParamSurface, "surface", "Surface", "Rain", 0.0, kSurfaceNames,
        "What the rain is falling on. Biases decay, resonance, click and chirp."),
   PCT(kParamNoteTracking, "note_tracking", "Note Tracking", "Rain", 0.5,
       "How far the played MIDI note transposes droplet pitch."),

   LIN(kParamBedLevel, "bed_level", "Bed Level", "Distant", -60.0, 6.0, -12.0, "dB",
       "Level of the far-field wash -- the drops too distant to hear individually."),
   PCT(kParamBedTone, "bed_tone", "Bed Tone", "Distant", 0.5,
       "Lowpass corner of the bed, dark to bright. Level compensated."),
   PCT(kParamBedBody, "bed_body", "Bed Body", "Distant", 0.2,
       "Resonance at the bed's corner frequency."),
   PCT(kParamBedDrift, "bed_drift", "Bed Drift", "Distant", 0.3,
       "Slow intensity drift shared by the bed and the droplet rate."),

   PCT(kParamWidth, "width", "Drop Width", "Close", 0.85,
       "How wide droplets are panned and how decorrelated the bed is."),
   PCT(kParamDistance, "distance", "Distance", "Space", 0.3,
       "Pushes the whole rain field away: quieter, duller, further back."),
   PCT(kParamAir, "air", "Air Absorption", "Space", 0.5,
       "How much high end distance costs."),
   PCT(kParamSpaceAmount, "space_amount", "Space Amount", "Space", 0.2,
       "Mix of the feedback delay network that puts the rain in a room."),
   PCT(kParamSpaceSize, "space_size", "Space Size", "Space", 0.5,
       "Size of that room -- delay lengths and decay time."),
   PCT(kParamSpaceDamping, "space_damping", "Space Damping", "Space", 0.5,
       "Bright stone at the left, soft absorbent surfaces at the right."),

   ENUM(kParamFilterType, "filter_type", "Filter Type", "Filter", 0.0, kFilterNames,
        "Response of the global output filter."),
   LOG(kParamFilterCutoff, "filter_cutoff", "Filter Cutoff", "Filter", 1.0, 20.0, 20000.0, "Hz",
       "Corner frequency of the output filter. Fully open at 20 kHz."),
   PCT(kParamFilterReso, "filter_reso", "Filter Resonance", "Filter", 0.1,
       "Emphasis at the cutoff frequency."),
   PCT(kParamFilterKeyTrack, "filter_key_track", "Filter Key Track", "Filter", 0.0,
       "How far the played note moves the cutoff. Follows the most recent note."),

   LOG(kParamAttack, "attack", "Attack", "Envelope", 0.551, 1.0, 15000.0, "ms",
       "Fade-in of the rain after a note starts."),
   LOG(kParamDecay, "decay", "Decay", "Envelope", 0.665, 1.0, 15000.0, "ms",
       "Fall from full level down to the sustain level."),
   PCT(kParamSustain, "sustain", "Sustain", "Envelope", 1.0,
       "Level the rain holds at while the note is held."),
   LOG(kParamRelease, "release", "Release", "Envelope", 0.716, 1.0, 20000.0, "ms",
       "Fade-out after the note is let go."),
   PCT(kParamVelToLevel, "vel_to_level", "Velocity to Level", "Envelope", 0.5,
       "How much note velocity controls loudness."),
   PCT(kParamVelToDensity, "vel_to_density", "Velocity to Density", "Envelope", 0.3,
       "How much note velocity controls the droplet rate."),

   STEP(kParamMaxDroplets, "max_droplets", "Max Droplets", "System", 32.0, 2048.0, 512.0, "",
        "Ceiling on simultaneously sounding droplets. Trade CPU against detail."),
   STEP(kParamSeed, "seed", "Random Seed", "System", 0.0, 999.0, 0.0, "",
        "Starting point of the random sequence, for a repeatable rain."),

   // Appended after the original 36, so the ids above keep their meaning.
   PCT(kParamBubble, "bubble", "Bubble Chance", "Rain", 1.0,
       "Fraction of droplets that ring at all. The rest are only splash and click."),

   // The near droplets and the far-field bed are two layers of the same rain and
   // used to share one width control between them. Each now has its own.
   BIPCT(kParamDropPan, "drop_pan", "Drop Pan", "Close", 0.0,
         "Slides the close droplets left or right without narrowing them."),
   PCT(kParamBedWidth, "bed_width", "Bed Width", "Distant", 0.85,
       "Stereo spread of the far-field bed, independent of the droplets."),
   BIPCT(kParamBedPan, "bed_pan", "Bed Pan", "Distant", 0.0,
         "Slides the far-field bed left or right without narrowing it."),
   LOG(kParamHighpass, "highpass", "Highpass", "Filter", 0.0, 20.0, 2000.0, "Hz",
       "Rolls off the bottom at 12 dB/oct. Off at the far left."),
};

#undef LIN
#undef PCT
#undef BIPCT
#undef LOG
#undef STEP
#undef ENUM

} // namespace

const ParamDesc *paramTable() { return kParams; }

const ParamDesc *paramById(uint32_t id) {
   if (id >= kNumParams)
      return nullptr;
   // The table is laid out in id order; assert that invariant cheaply.
   const ParamDesc *d = &kParams[id];
   return d->id == id ? d : nullptr;
}

const ParamDesc *paramByKey(const char *key) {
   if (!key)
      return nullptr;
   for (uint32_t i = 0; i < kNumParams; ++i)
      if (std::strcmp(kParams[i].key, key) == 0)
         return &kParams[i];
   return nullptr;
}

double paramToReal(const ParamDesc &desc, double raw) {
   if (raw < desc.min)
      raw = desc.min;
   if (raw > desc.max)
      raw = desc.max;
   switch (desc.kind) {
   case ParamKind::Log:
      return desc.dispMin * std::pow(desc.dispMax / desc.dispMin, raw);
   case ParamKind::Stepped:
   case ParamKind::Enum:
      return std::floor(raw + 0.5);
   case ParamKind::Percent:
   case ParamKind::Linear:
   default:
      return raw;
   }
}

double realToParam(const ParamDesc &desc, double real) {
   double raw = real;
   if (desc.kind == ParamKind::Log) {
      if (real <= 0.0)
         raw = 0.0;
      else
         raw = std::log(real / desc.dispMin) / std::log(desc.dispMax / desc.dispMin);
   }
   if (raw < desc.min)
      raw = desc.min;
   if (raw > desc.max)
      raw = desc.max;
   return raw;
}

namespace {

struct LogDisplay {
   double value;
   int decimals;
   bool scaled; // shown in k, or in seconds for a millisecond parameter
};

// Rounding can push a value across the very boundary that chose its precision:
// 99.96 shows as "100.0" with one decimal, and reading that back shows "100"
// with none, so a host that round-trips the text sees the value drift. Decide
// the format from the number as it will actually be printed, which takes at
// most a couple of passes to settle.
LogDisplay chooseLogDisplay(double real) {
   LogDisplay d{real, 2, false};
   for (int pass = 0; pass < 4; ++pass) {
      d.scaled = real >= 1000.0;
      d.value = d.scaled ? real * 0.001 : real;
      d.decimals = d.scaled ? 2 : (d.value >= 100.0 ? 0 : (d.value >= 10.0 ? 1 : 2));
      const double scale = std::pow(10.0, d.decimals);
      d.value = std::round(d.value * scale) / scale;
      const double shown = d.scaled ? d.value * 1000.0 : d.value;
      if (shown == real)
         break;
      real = shown;
   }
   return d;
}

} // namespace

bool paramValueToText(const ParamDesc &desc, double raw, char *out, uint32_t outSize) {
   if (!out || outSize == 0)
      return false;
   const double real = paramToReal(desc, raw);
   int n = 0;
   switch (desc.kind) {
   case ParamKind::Enum: {
      const uint32_t idx = static_cast<uint32_t>(real < 0 ? 0 : real);
      const char *name = idx < desc.enumCount ? desc.enumNames[idx] : "?";
      n = std::snprintf(out, outSize, "%s", name);
      break;
   }
   case ParamKind::Stepped:
      n = std::snprintf(out, outSize, "%d%s%s", static_cast<int>(real), desc.unit[0] ? " " : "",
                        desc.unit);
      break;
   case ParamKind::Percent:
      n = std::snprintf(out, outSize, "%.1f %%", real * 100.0);
      break;
   case ParamKind::Log: {
      // Milliseconds roll over into seconds; everything else takes a k prefix.
      // "1.20 kms" is not a unit anybody uses.
      const LogDisplay d = chooseLogDisplay(real);
      const char *unit = desc.unit;
      char scaled[16];
      if (d.scaled) {
         if (std::strcmp(desc.unit, "ms") == 0) {
            unit = "s";
         } else {
            std::snprintf(scaled, sizeof(scaled), "k%s", desc.unit);
            unit = scaled;
         }
      }
      n = std::snprintf(out, outSize, "%.*f %s", d.decimals, d.value, unit);
      break;
   }
   case ParamKind::Linear:
   default:
      if (real <= -59.95 && std::strcmp(desc.unit, "dB") == 0)
         n = std::snprintf(out, outSize, "-inf dB");
      else
         n = std::snprintf(out, outSize, "%.2f %s", real, desc.unit);
      break;
   }
   return n > 0 && static_cast<uint32_t>(n) < outSize;
}

bool paramTextToValue(const ParamDesc &desc, const char *text, double *outRaw) {
   if (!text || !outRaw)
      return false;

   if (desc.kind == ParamKind::Enum) {
      for (uint32_t i = 0; i < desc.enumCount; ++i) {
         if (strcasecmp(text, desc.enumNames[i]) == 0) {
            *outRaw = static_cast<double>(i);
            return true;
         }
      }
      // Fall through to numeric parsing so "2" also works.
   }

   char *end = nullptr;
   double v = std::strtod(text, &end);
   if (end == text)
      return false;

   // Accept a k/K multiplier for Hz and ms fields, and a bare "s" on a
   // millisecond field, which is how values over a second are displayed.
   while (*end == ' ')
      ++end;
   if (*end == 'k' || *end == 'K')
      v *= 1000.0;
   else if ((*end == 's' || *end == 'S') && std::strcmp(desc.unit, "ms") == 0)
      v *= 1000.0;

   switch (desc.kind) {
   case ParamKind::Percent:
      *outRaw = v * 0.01;
      break;
   case ParamKind::Log:
      *outRaw = realToParam(desc, v);
      break;
   default:
      *outRaw = v;
      break;
   }
   if (*outRaw < desc.min)
      *outRaw = desc.min;
   if (*outRaw > desc.max)
      *outRaw = desc.max;
   return true;
}

} // namespace rainyday
