#include "params.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>

namespace thunderclap {

namespace {

const char *const kFilterNames[] = {"Lowpass", "Bandpass", "Highpass", "Notch"};
const char *const kModeNames[] = {"One Shot", "Gated", "Storm"};

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
// host's generic UI lands somewhere useful across its whole travel. Log
// defaults are the raw 0..1 position: log(def / lo) / log(hi / lo).
const ParamDesc kParams[kNumParams] = {
   LIN(kParamGain, "gain", "Output Gain", "Output", -60.0, 12.0, 0.0, "dB",
       "Final output level of the whole instrument."),

   LOG(kParamDistance, "distance", "Distance", "Strike", 0.5, 0.2, 20.0, "km",
       "How far away the lightning strikes. Near is a crack, far is a rumble. Level is compensated."),
   LOG(kParamHeight, "height", "Height", "Strike", 0.699, 1.0, 10.0, "km",
       "Height of the cloud base the channel comes down from. Taller channels rumble longer."),
   LIN(kParamCloudSpread, "cloud_spread", "Cloud Spread", "Strike", 0.0, 20.0, 4.0, "km",
       "Length of the channel that runs inside the cloud. This is what stretches a distant thunder out."),
   PCT(kParamTortuosity, "tortuosity", "Tortuosity", "Strike", 0.5,
       "How crooked the channel is. A straight channel claps once; a jagged one crackles and rolls."),
   PCT(kParamBranching, "branching", "Branching", "Strike", 0.4,
       "Side branches off the main channel, each a smaller thunder of its own."),
   STEP(kParamStrokes, "strokes", "Strokes", "Strike", 1.0, 8.0, 3.0, "",
        "Return strokes down the same channel. Each repeats the thunder a little quieter."),
   LOG(kParamStrokeGap, "stroke_gap", "Stroke Gap", "Strike", 0.54, 5.0, 500.0, "ms",
       "Time between return strokes. Short gaps thicken the clap; long ones stutter."),
   PCT(kParamVariation, "variation", "Variation", "Strike", 0.5,
       "How far each flash may wander from these settings: distance, height, strokes, direction."),

   PCT(kParamCrack, "crack", "Crack", "Sound", 0.5,
       "Sharpness of the shock fronts. All the high end of a close strike lives here."),
   PCT(kParamWeight, "weight", "Weight", "Sound", 0.5,
       "Length of each shock wave, and so how deep the thunder sits."),
   PCT(kParamSwell, "swell", "Swell", "Sound", 0.3,
       "Shadows the low part of the channel, so the thunder swells in from the sky instead of cracking."),
   PCT(kParamRumble, "rumble", "Rumble", "Sound", 0.5,
       "A rolling noise floor that follows the density of arriving shocks."),
   PCT(kParamRumbleTone, "rumble_tone", "Rumble Tone", "Sound", 0.5,
       "Lowpass corner of the rumble, 60 Hz to 1.5 kHz: a subsonic floor or a mid growl."),
   PCT(kParamAir, "air", "Air Absorption", "Sound", 0.5,
       "Humidity of the air: how much high end is lost per kilometre."),
   PCT(kParamScatter, "scatter", "Scatter", "Sound", 0.5,
       "Random spread of level and length from shock to shock."),
   PCT(kParamFocus, "focus", "Focus", "Sound", 0.7,
       "Directivity of each channel element. Focused, only the parts side-on to you are loud."),

   PCT(kParamWidth, "width", "Width", "Stereo", 0.8,
       "How far the channel's spread across the sky is mapped onto the stereo field."),
   BIPCT(kParamPan, "pan", "Pan", "Stereo", 0.0,
         "Direction of the strike, left to right."),
   PCT(kParamRumbleWidth, "rumble_width", "Rumble Width", "Stereo", 0.9,
       "Stereo spread of the rumble, independent of the shocks."),
   PCT(kParamDrift, "drift", "Drift", "Stereo", 0.3,
       "Slow wander of the rumble across the stereo field while a thunder plays."),

   LIN(kParamEchoLevel, "echo_level", "Echo Level", "Echoes", -60.0, 6.0, -12.0, "dB",
       "Level of the long echoes off hills, buildings and the cloud base."),
   STEP(kParamEchoCount, "echo_count", "Echo Count", "Echoes", 0.0, 8.0, 3.0, "",
        "How many distinct reflectors there are."),
   LOG(kParamEchoSpread, "echo_spread", "Echo Spread", "Echoes", 0.661, 0.1, 6.0, "s",
       "How far away the reflectors are, as the delay of the furthest one."),
   PCT(kParamEchoDamping, "echo_damping", "Echo Damping", "Echoes", 0.5,
       "How much high end each echo loses on the way."),

   PCT(kParamSpaceAmount, "space_amount", "Space Amount", "Space", 0.15,
       "Mix of the room you hear it from: its first reflections and its tail."),
   PCT(kParamSpaceSize, "space_size", "Space Size", "Space", 0.5,
       "Dimension of that room, 3 m to 90 m. Reflections and decay time follow from it."),
   PCT(kParamSpaceDamping, "space_damping", "Space Damping", "Space", 0.5,
       "Absorption of its surfaces: bright stone at the left, soft and absorbent at the right."),

   ENUM(kParamFilterType, "filter_type", "Filter Type", "Filter", 0.0, kFilterNames,
        "Response of the global output filter."),
   LOG(kParamHighpass, "highpass", "Highpass", "Filter", 0.0, 20.0, 2000.0, "Hz",
       "Rolls off the bottom at 12 dB/oct. Off at the far left."),
   LOG(kParamFilterCutoff, "filter_cutoff", "Filter Cutoff", "Filter", 1.0, 20.0, 20000.0, "Hz",
       "Corner frequency of the output filter. Fully open at 20 kHz."),
   PCT(kParamFilterReso, "filter_reso", "Filter Resonance", "Filter", 0.1,
       "Emphasis at the cutoff frequency."),
   PCT(kParamFilterKeyTrack, "filter_key_track", "Filter Key Track", "Filter", 0.0,
       "How far the played note moves the cutoff. Follows the most recent note."),

   ENUM(kParamMode, "mode", "Mode", "Envelope", 0.0, kModeNames,
        "One Shot plays a flash out. Gated fades it when you let go. Storm keeps flashing while held."),
   LOG(kParamAttack, "attack", "Attack", "Envelope", 0.0, 1.0, 15000.0, "ms",
       "Fade-in applied to the shocks as they arrive. Softens the first crack."),
   LOG(kParamRelease, "release", "Release", "Envelope", 0.72, 1.0, 20000.0, "ms",
       "Fade-out of what is still to arrive after the note is let go, in Gated and Storm modes."),
   LOG(kParamStormRate, "storm_rate", "Storm Rate", "Envelope", 0.438, 1.0, 60.0, "/min",
       "Average flashes per minute in Storm mode. They arrive at random, never on a grid."),
   PCT(kParamVelToLevel, "vel_to_level", "Velocity to Level", "Envelope", 0.5,
       "How much note velocity controls loudness."),
   PCT(kParamVelToDistance, "vel_to_distance", "Velocity to Distance", "Envelope", 0.5,
       "Soft notes strike further away. At full depth a gentle note is eight times as far."),

   STEP(kParamMaxShocks, "max_shocks", "Max Shocks", "System", 128.0, 4096.0, 2048.0, "",
        "Number of channel elements per flash. Trade CPU against detail."),
   STEP(kParamSeed, "seed", "Random Seed", "System", 0.0, 999.0, 0.0, "",
        "Starting point of the random sequence. Zero never repeats a thunder."),

   // Appended after the original 41, so the ids above keep their meaning.
   PCT(kParamCompress, "compress", "Compress", "Dynamics", 0.0,
       "Lifts the rumble and the far claps towards the crack. Threshold and ratio move together; level is made up."),
   LOG(kParamCompAttack, "comp_attack", "Comp Attack", "Dynamics", 0.5, 0.1, 100.0, "ms",
       "How fast the compressor grabs a crack. Slow lets the first snap through."),
   LOG(kParamCompRelease, "comp_release", "Comp Release", "Dynamics", 0.565, 10.0, 3000.0, "ms",
       "How fast it lets go afterwards. Slow is a smooth swell; fast pumps with the claps."),

   PCT(kParamImpact, "impact", "Impact", "Impact", 0.0,
       "The blast the near channel throws off when the stroke fires: a low, hard slam under the crack."),
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
   // millisecond field, which is how values over a second are displayed. A
   // kilometre field is already in k, so a "k" there is its own unit.
   while (*end == ' ')
      ++end;
   const bool kilometres = std::strcmp(desc.unit, "km") == 0;
   if ((*end == 'k' || *end == 'K') && !kilometres)
      v *= 1000.0;
   else if ((*end == 'm' || *end == 'M') && kilometres && end[1] != 'i')
      v *= 0.001; // "800 m"
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

} // namespace thunderclap
