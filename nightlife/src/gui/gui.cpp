// NightLife's window: its panels, its colours and the night sky across its
// header.
//
// The window itself -- the layout engine, the widgets, the preset browser, the
// typed value entry -- is shared by the suite and lives in
// shared/src/gui/window.cpp. What is here is only what makes this window
// NightLife's.

#include <algorithm>
#include <cmath>

#include "verdalis/gui/window.h"

#include "nightlife.h"
#include "params.h"

namespace nightlife {

namespace {

// ------------------------------------------------------------------ geometry
//
// Cell and panel sizes are the suite's (verdalis/gui/window.h); a knob is the
// same size in every plugin. Only the overall width, which follows from the
// busiest row, is ours -- and this is the widest window in the suite, because
// it is the first plugin with four sounding layers rather than three.
constexpr int kContentW = 1516;
constexpr int kWindowH = 740;

// ------------------------------------------------------------------- panels

// The call first, because it is the unit; the larynx that makes it next; then
// the structures built out of it, then the three layers that are not calls at
// all, and the place all of it happens in.
constexpr uint32_t kCallParams[] = {
   kParamPitch,  kParamContour, kParamDetail,  kParamSweep,   kParamLength,
   kParamSkew,   kParamJitter,  kParamVibrato, kParamVibratoRate,
};
constexpr uint32_t kVoiceParams[] = {kParamCaller,  kParamVoice,   kParamPartials,
                                     kParamBreath,  kParamRasp,    kParamThroat,
                                     kParamMuzzle,  kParamFormant, kParamRadiate};
constexpr uint32_t kPhraseParams[] = {kParamCalls,     kParamCallRate,  kParamRateDrift,
                                      kParamLegato,    kParamMotif,     kParamVariation,
                                      kParamPhraseGap, kParamRepeats};
constexpr uint32_t kInsectParams[] = {kParamInsectLevel, kParamInsectPitch, kParamInsectWidth,
                                      kParamTrillRate,   kParamTrillDepth,  kParamShimmer};
constexpr uint32_t kPackParams[] = {
   kParamShotLevel,   kParamPackLevel, kParamPackRate,  kParamAnimals, kParamPitchSpread,
   kParamVoiceSpread, kParamAnswer,    kParamRestless,  kParamMaxVoices,
};
constexpr uint32_t kChorusParams[] = {
   kParamChorusLevel, kParamCroak,     kParamCroakPitch,   kParamPulseRate,
   kParamPulses,      kParamCroakLength, kParamFrogs,      kParamCroakRate,
   kParamRegularity,  kParamChorusSpread, kParamChorusWidth,
};
constexpr uint32_t kPlaceParams[] = {kParamDistance,  kParamDistanceSpread, kParamAir,
                                     kParamWidth,     kParamSpaceAmount,
                                     kParamSpaceSize, kParamSpaceDamping};
constexpr uint32_t kOutParams[] = {kParamGain, kParamSeed};
constexpr uint32_t kBedParams[] = {kParamBedLevel, kParamBedTilt, kParamBedMotion};
constexpr uint32_t kFilterParams[] = {kParamFilterType, kParamHighpass, kParamFilterCutoff,
                                      kParamFilterReso, kParamFilterKeyTrack};
constexpr uint32_t kEnvParams[] = {kParamAttack,  kParamDecay,      kParamSustain,
                                   kParamRelease, kParamVelToLevel, kParamVelToPitch};

#define PANEL(title, cols, rows, arr)                                                              \
   { title, cols, rows, arr, static_cast<int>(sizeof(arr) / sizeof(arr[0])) }

constexpr PanelSpec kPanelSpecs[] = {
   PANEL("CALL", 5, 2, kCallParams),     PANEL("VOICE", 5, 2, kVoiceParams),
   PANEL("PHRASE", 4, 2, kPhraseParams), PANEL("INSECTS", 3, 2, kInsectParams),
   PANEL("PACK", 5, 2, kPackParams),     PANEL("CHORUS", 6, 2, kChorusParams),
   PANEL("PLACE", 4, 2, kPlaceParams),   PANEL("OUTPUT", 2, 1, kOutParams),
   PANEL("BED", 3, 1, kBedParams),       PANEL("FILTER", 5, 1, kFilterParams),
   PANEL("ENVELOPE", 6, 1, kEnvParams),
};
#undef PANEL

constexpr int kNumPanels = static_cast<int>(sizeof(kPanelSpecs) / sizeof(kPanelSpecs[0]));

// Which panels share a row, in order. The activity meter fills what is left of
// the last row.
constexpr int kRowStart[] = {0, 4, 8};
constexpr int kRowCount[] = {4, 4, 3};
constexpr int kNumRows = 3;

// The layout is a table, and a table is easy to break by adding a parameter to
// a panel that has no room for it, or by forgetting to put it on a panel at
// all. None of that should need a running window to notice, so it is checked
// here instead.
constexpr int rowWidth(int row) {
   int w = 0;
   for (int i = 0; i < kRowCount[row]; ++i)
      w += panelWidth(kPanelSpecs[kRowStart[row] + i]) + (i ? kGap : 0);
   return w;
}

constexpr int contentBottom() {
   int y = kHeaderH + kGap;
   for (int row = 0; row < kNumRows; ++row) {
      int h = 0;
      for (int i = 0; i < kRowCount[row]; ++i) {
         const int ph = panelHeight(kPanelSpecs[kRowStart[row] + i]);
         h = ph > h ? ph : h;
      }
      y += h + kGap;
   }
   return y;
}

constexpr bool everyPanelHoldsItsParams() {
   for (int i = 0; i < kNumPanels; ++i)
      if (kPanelSpecs[i].cols * kPanelSpecs[i].rows < kPanelSpecs[i].count)
         return false;
   return true;
}

constexpr int placedParams() {
   int n = 0;
   for (int i = 0; i < kNumPanels; ++i)
      n += kPanelSpecs[i].count;
   return n;
}

constexpr bool everyPanelIsOnARow() {
   int n = 0;
   for (int row = 0; row < kNumRows; ++row)
      n += kRowCount[row];
   return n == kNumPanels;
}

static_assert(everyPanelIsOnARow(), "kRowStart / kRowCount do not cover every panel");
static_assert(everyPanelHoldsItsParams(), "a panel has more parameters than it has cells");
static_assert(placedParams() == static_cast<int>(kNumParams),
              "every parameter must appear on exactly one panel");
static_assert(rowWidth(0) <= kContentW, "row 0 is wider than the window");
static_assert(rowWidth(1) <= kContentW, "row 1 is wider than the window");
static_assert(rowWidth(2) <= kContentW, "row 2 is wider than the window");
static_assert(contentBottom() + 2 + kBarH + 24 <= kWindowH,
              "the window is too short for its panels");

// -------------------------------------------------------------------- theme
//
// Moonlight. Every other accent in the suite is a saturated colour -- rain
// blue, lightning violet, sea green, dust coral, finch gold, wasp yellow, river
// green, ember orange, impact magenta -- and this one deliberately is not: a
// pale, desaturated indigo at hue 221, which is what a light source you cannot
// look at directly does to everything it falls on. It sits between RainyDay's
// saturated cyan and ThunderClap's saturated violet and is mistakable for
// neither, because it is washed out where both of those are vivid.
//
// The chassis is the darkest blue-black in the suite, and the highlight is the
// moon itself.
constexpr Theme kTheme = {
   /* bgTop     */ {0.043, 0.047, 0.071},
   /* bgBottom  */ {0.023, 0.027, 0.043},
   /* panelFill */ {0.063, 0.069, 0.098},
   /* panelEdge */ {0.110, 0.122, 0.180},
   /* knobFace  */ {0.035, 0.039, 0.059},
   /* track     */ {0.133, 0.149, 0.216},
   /* accent    */ {0.624, 0.706, 0.910},
   /* text      */ {0.906, 0.918, 0.965},
   /* textDim   */ {0.545, 0.576, 0.667},
   /* textMute  */ {0.365, 0.392, 0.478},
   /* highlight */ {0.965, 0.973, 1.000},
};

// -------------------------------------------------------------------- mixer
//
// Four sounding layers and the output, which is one more layer than anything
// else in the suite has. Each level also sits on its own panel, which is where
// it belongs while that layer is being edited -- but building a night is mostly
// balancing them against one another, and for that they have to be side by
// side.
//
// In sounding order from the back: the bed, the insects in it, the chorus over
// that, the pack over everything, and the phrase a note fires on top.
//
// The pack, the insects and the bed cannot be placed as layers: the animals
// carry their own positions inside the engine, so Width in PLACE spreads the
// pack as a whole rather than any one layer. The chorus has a width of its own
// because a pond is a place and the pack is not.
constexpr MixerStrip kMixerStrips[] = {
   /* label       level                pan       width                master */
   {"BED", kParamBedLevel, kNoParam, kNoParam, false},
   {"INSECTS", kParamInsectLevel, kNoParam, kNoParam, false},
   {"CHORUS", kParamChorusLevel, kNoParam, kParamChorusWidth, false},
   {"PACK", kParamPackLevel, kNoParam, kNoParam, false},
   {"SHOT", kParamShotLevel, kNoParam, kNoParam, false},
   {"OUTPUT", kParamGain, kNoParam, kNoParam, true},
};

constexpr int kNumStrips = static_cast<int>(sizeof(kMixerStrips) / sizeof(kMixerStrips[0]));

// ----------------------------------------------------------------- ornament
//
// A night sky over a treeline, and a ring for every call.
//
// Every plugin in the suite animates the thing it makes: rain streaks, a
// lightning channel, a surf line, a flow field, a sonogram. What NightLife
// makes is a sound crossing a valley in the dark -- you hear a wolf long before
// you could see one, and what reaches you is a wavefront. So the header draws
// exactly that: stars and a treeline, and each call sends a ring out from a
// point on the horizon.
//
// The ring's position and size are deterministic in the call number, in the way
// ThunderClap's bolts are deterministic in the flash number. The window cannot
// see inside the engine, and the same call should always draw the same ring.
class NightSky final : public HeaderOrnament {
public:
   bool animating(uint32_t eventCounter) const override {
      return eventCounter != mLastEvent || mCount > 0;
   }

   void draw(cairo_t *cr, const OrnamentContext &ctx) override {
      if (!(ctx.windowW > 1.0) || !(ctx.headerH > 1.0))
         return;
      const double h = ctx.headerH;
      const double horizon = h * 0.78;

      // The stars. Fixed, because a star does not move in the four seconds
      // anybody looks at a plugin header, and a twinkling one would make the
      // window repaint for ever.
      for (int i = 0; i < 40; ++i) {
         const double x = ctx.windowW * unit(static_cast<uint32_t>(i) * 2654435761u + 5u);
         const double y = horizon * (0.08 + 0.85 * unit(static_cast<uint32_t>(i) * 40503u + 11u));
         const double a = 0.10 + 0.32 * unit(static_cast<uint32_t>(i) * 668265263u + 17u);
         setColor(cr, ctx.theme->highlight, a);
         cairo_arc(cr, x, y, 0.6 + 0.7 * unit(static_cast<uint32_t>(i) * 374761393u + 23u), 0.0,
                   6.2831853);
         cairo_fill(cr);
      }

      // New calls, capped per frame: a full pond over a pack can put dozens
      // between two repaints, and drawing all of them would be a smear.
      if (ctx.eventCounter != mLastEvent) {
         const uint32_t fresh = ctx.eventCounter - mLastEvent;
         const uint32_t take = fresh > 3u ? 3u : fresh;
         for (uint32_t i = 0; i < take; ++i)
            push(ctx, mLastEvent + fresh - take + i + 1u, 1.0);
         mLastEvent = ctx.eventCounter;
      }
      // An idle header still has something crossing it, dimly. A host shows the
      // window before the first note and the suite's screenshots are taken
      // there, and a still night with nothing in it reads as a broken graphic
      // rather than a quiet one.
      if (ctx.eventCounter == mLastEvent && mCount < 2 && (++mIdleTick & 63u) == 0u)
         push(ctx, 0x4F1B2Du + mIdleSeed++, kIdleWeight);

      // The rings, oldest first so the newest is drawn over them.
      cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
      int kept = 0;
      for (int i = 0; i < mCount; ++i) {
         Ring ring = mRings[i];
         ring.r += ring.speed * ctx.windowW;
         if (ring.r > ctx.windowW * 0.75) {
            continue;
         }
         mRings[kept++] = ring;
         // A wavefront loses amplitude as it spreads, which is also what makes
         // the ring fade rather than simply being drawn for a while.
         const double alpha = ring.weight * 0.55 / (1.0 + 7.0 * ring.r / ctx.windowW);
         if (alpha <= 0.004)
            continue;
         setColor(cr, ctx.theme->accent, alpha);
         cairo_set_line_width(cr, 1.0 + 0.9 * ring.weight);
         // Half a ring: the ground is in the way of the rest of it.
         cairo_new_path(cr);
         cairo_arc(cr, ring.x, ring.y, ring.r, 3.14159265, 6.2831853);
         cairo_stroke(cr);
      }
      mCount = kept;

      // The treeline, drawn over the rings so that they rise out of it. One
      // fixed silhouette: it is the horizon, and a horizon that changed shape
      // between repaints would be a different place every frame.
      cairo_new_path(cr);
      cairo_move_to(cr, 0.0, h);
      cairo_line_to(cr, 0.0, horizon);
      const int steps = 48;
      for (int i = 0; i <= steps; ++i) {
         const double t = static_cast<double>(i) / steps;
         const double x = ctx.windowW * t;
         // Three slow waves plus a per-tree jitter: a treeline is not a hill.
         double y = horizon - h * 0.05 * std::sin(t * 7.3 + 0.7) -
                    h * 0.035 * std::sin(t * 17.1 + 2.2);
         y -= h * 0.06 * unit(static_cast<uint32_t>(i) * 2246822519u + 29u);
         cairo_line_to(cr, x, y);
      }
      cairo_line_to(cr, ctx.windowW, h);
      cairo_close_path(cr);
      // Filled darker than the sky above it rather than in a panel colour: the
      // first version used bgBottom, which is what the header fades to anyway,
      // and the horizon was invisible.
      cairo_set_source_rgba(cr, 0.008, 0.010, 0.020, 0.95);
      cairo_fill_preserve(cr);
      // A rim, so the treeline is a shape and not an absence. Moonlight catches
      // the top of a canopy; nothing else in this header is lit at all.
      setColor(cr, ctx.theme->accent, 0.18);
      cairo_set_line_width(cr, 1.0);
      cairo_stroke(cr);

      // Everything on screen ages by the same amount per frame, so a ring is
      // bright for about a second.
      for (int i = 0; i < mCount; ++i)
         if (mRings[i].weight > kIdleWeight)
            mRings[i].weight = std::max(kIdleWeight, mRings[i].weight * 0.97);
   }

private:
   struct Ring {
      double x, y, r, speed, weight;
   };

   static uint32_t hash32(uint32_t x) {
      x ^= x >> 16;
      x *= 0x7FEB352Du;
      x ^= x >> 15;
      x *= 0x846CA68Bu;
      x ^= x >> 16;
      return x;
   }
   static double unit(uint32_t x) { return (hash32(x) >> 8) * (1.0 / 16777216.0); }

   void push(const OrnamentContext &ctx, uint32_t counter, double weight) {
      if (mCount >= kMax) {
         for (int i = 1; i < mCount; ++i)
            mRings[i - 1] = mRings[i];
         --mCount;
      }
      Ring ring;
      ring.x = ctx.windowW * (0.04 + 0.92 * unit(counter * 2654435761u + 3u));
      ring.y = ctx.headerH * (0.74 + 0.06 * unit(counter * 40503u + 9u));
      ring.r = 2.0;
      // A near call spreads fast and a far one slowly, which is the only cue a
      // static picture has for distance.
      ring.speed = 0.004 + 0.011 * unit(counter * 668265263u + 15u);
      ring.weight = weight;
      mRings[mCount++] = ring;
   }

   static constexpr int kMax = 24;
   static constexpr double kIdleWeight = 0.30;
   Ring mRings[kMax];
   int mCount = 0;
   uint32_t mLastEvent = 0;
   uint32_t mIdleTick = 0;
   uint32_t mIdleSeed = 100;
};

const WindowSpec kSpec = {
   /* wordmarkFirst  */ "Night",
   /* wordmarkSecond */ "Life",
   /* subtitle       */ "SYNTHETIC NIGHT INSTRUMENT",
   /* version        */ kPluginVersion,
   /* voiceNoun      */ "voices",
   /* eventNoun      */ "calls",
   /* theme          */ kTheme,
   /* panels         */ kPanelSpecs,
   /* panelCount     */ kNumPanels,
   /* rowStart       */ kRowStart,
   /* rowLength      */ kRowCount,
   /* rowCount       */ kNumRows,
   /* contentW       */ kContentW,
   /* windowH        */ kWindowH,
   /* params         */ nullptr, // filled in by createGui, paramTable() is a call
   /* paramCount     */ kNumParams,
   /* mixer          */ kMixerStrips,
   /* mixerCount     */ kNumStrips,
   /* ornament       */ nullptr, // per window, created in createGui()
};

} // namespace

Gui *createGui(GuiDelegate &delegate) {
   WindowSpec spec = kSpec;
   spec.params = paramTable();
   // One ornament per window: it carries animation state, and a single
   // shared instance would let two windows of this plugin drive each other.
   spec.ornament = new NightSky();
   return createWindow(delegate, spec);
}

} // namespace nightlife
