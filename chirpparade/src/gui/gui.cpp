// ChirpParade's window: its panels, its colours and the sonogram across its
// header.
//
// The window itself -- the layout engine, the widgets, the preset browser, the
// typed value entry -- is shared by the suite and lives in
// shared/src/gui/window.cpp. What is here is only what makes this window
// ChirpParade's.

#include <algorithm>
#include <cmath>

#include "verdalis/gui/window.h"

#include "chirpparade.h"
#include "params.h"

namespace chirpparade {

namespace {

// ------------------------------------------------------------------- geometry
//
// Cell and panel sizes are the suite's (verdalis/gui/window.h); a knob is the
// same size in every plugin. Only the overall width, which follows from the
// busiest row, is ours: SYLLABLE and FLOCK are five cells wide because nine
// things about one syllable, and nine about a flock, are worth naming.
//
// The width is set by the second row rather than by the widest panel, because
// the activity meter fills what is left of the *last* row -- so the last row
// has to be the short one. Filling all three to the margin put the meter on top
// of the OUTPUT panel.
constexpr int kContentW = 1348;
constexpr int kWindowH = 740;

// ------------------------------------------------------------------- panels

// The syllable first, because it is the unit; the syrinx that makes it next;
// then the structures built out of it, and the place they happen in.
constexpr uint32_t kSyllableParams[] = {
   kParamPitch,  kParamSweep, kParamContour,   kParamDetail, kParamLength,
   kParamSkew,   kParamJitter, kParamPulseRate, kParamPulseDepth,
};
constexpr uint32_t kTimbreParams[] = {kParamSpecies,  kParamVoice, kParamPartials,
                                      kParamBreath,   kParamRasp,  kParamTract,
                                      kParamBeak,     kParamFormant, kParamRadiate};
constexpr uint32_t kPhraseParams[] = {kParamSyllables, kParamSyllableRate, kParamRateDrift,
                                      kParamLegato,    kParamMotif,        kParamVariation,
                                      kParamPhraseGap, kParamRepeats};
constexpr uint32_t kFlockParams[] = {
   kParamShotLevel,   kParamFlockLevel, kParamFlockRate, kParamBirds, kParamPitchSpread,
   kParamVoiceSpread, kParamAnswer,     kParamRestless,  kParamMaxVoices,
};
constexpr uint32_t kDrumParams[] = {kParamDrumLevel, kParamDrumRate,  kParamStrikes,
                                    kParamStrikeRate, kParamDrumAccel, kParamKnock,
                                    kParamRing};
constexpr uint32_t kPlaceParams[] = {kParamDistance,    kParamDistanceSpread, kParamAir,
                                     kParamWidth,       kParamSpaceAmount,
                                     kParamSpaceSize,   kParamSpaceDamping};
constexpr uint32_t kFilterParams[] = {kParamFilterType, kParamHighpass, kParamFilterCutoff,
                                      kParamFilterReso, kParamFilterKeyTrack};
constexpr uint32_t kEnvParams[] = {kParamAttack,  kParamDecay,      kParamSustain,
                                   kParamRelease, kParamVelToLevel, kParamVelToPitch};
constexpr uint32_t kOutParams[] = {kParamGain, kParamSeed};

#define PANEL(title, cols, rows, arr)                                                              \
   { title, cols, rows, arr, static_cast<int>(sizeof(arr) / sizeof(arr[0])) }

constexpr PanelSpec kPanelSpecs[] = {
   PANEL("SYLLABLE", 5, 2, kSyllableParams), PANEL("TIMBRE", 5, 2, kTimbreParams),
   PANEL("PHRASE", 4, 2, kPhraseParams),     PANEL("FLOCK", 5, 2, kFlockParams),
   PANEL("DRUM", 4, 2, kDrumParams),         PANEL("PLACE", 4, 2, kPlaceParams),
   PANEL("OUTPUT", 2, 1, kOutParams),        PANEL("FILTER", 5, 1, kFilterParams),
   PANEL("ENVELOPE", 6, 1, kEnvParams),
};
#undef PANEL

constexpr int kNumPanels = static_cast<int>(sizeof(kPanelSpecs) / sizeof(kPanelSpecs[0]));

// Which panels share a row, in order. The activity meter fills what is left of
// the last row.
constexpr int kRowStart[] = {0, 3, 7};
constexpr int kRowCount[] = {3, 4, 2};
constexpr int kNumRows = 3;

// The layout is a table, and a table is easy to break by adding a parameter to
// a panel that has no room for it, or by forgetting to put it on a panel at
// all. None of that should need a running window to notice, so it is checked
// here instead. panelWidth() and panelHeight() come from verdalis/gui/window.h.
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
// Finch gold. The suite's other four are rain blue, lightning violet, sea green
// and dust coral, so the warm end is half taken already -- but coral is a dry
// red-brown and this is a yellow one, morning light rather than dust. The
// chassis has an olive cast, which is bark and leaf rather than sand, and the
// highlight is the bleached white of a sky behind a canopy.
constexpr Theme kTheme = {
   /* bgTop     */ {0.078, 0.075, 0.051},
   /* bgBottom  */ {0.043, 0.041, 0.027},
   /* panelFill */ {0.106, 0.100, 0.063},
   /* panelEdge */ {0.176, 0.161, 0.090},
   /* knobFace  */ {0.059, 0.055, 0.035},
   /* track     */ {0.212, 0.192, 0.106},
   /* accent    */ {0.949, 0.780, 0.267},
   /* text      */ {0.953, 0.937, 0.871},
   /* textDim   */ {0.620, 0.596, 0.478},
   /* textMute  */ {0.416, 0.400, 0.310},
   /* highlight */ {1.000, 0.976, 0.882},
};

// ----------------------------------------------------------------- ornament

// A sonogram, scrolling right to left behind the wordmark.
//
// Every other plugin in the suite animates the thing it makes -- rain streaks,
// a lightning channel, a surf line, a flow field. What ChirpParade makes is a
// sonogram: birds are read off one, and the shape of a stroke on it *is* the
// syllable. So the header draws the trace the plugin would leave on one.
//
// One stroke per syllable, entering at the right edge and scrolling off the
// left. Its height, its slope and its length are deterministic in the syllable
// number, in the way ThunderClap's bolts are deterministic in the flash number:
// the window cannot see inside the engine, and the same syllable should always
// draw the same stroke.
class Sonogram final : public HeaderOrnament {
public:
   bool animating(uint32_t eventCounter) const override {
      return eventCounter != mLastEvent || mCount > 0;
   }

   void draw(cairo_t *cr, const OrnamentContext &ctx) override {
      // The scroll below divides by a width, and a zero-width header is one
      // call away from any host that asks for a silly size.
      if (!(ctx.windowW > 1.0) || !(ctx.headerH > 1.0))
         return;

      // The frequency grid a sonogram is read against. Drawn whether or not
      // anything is sounding, so an idle header is a waiting instrument rather
      // than an empty box.
      setColor(cr, ctx.theme->accent, 0.16);
      cairo_set_line_width(cr, 1.0);
      for (int i = 1; i < 4; ++i) {
         const double y = std::floor(ctx.headerH * i / 4.0) + 0.5;
         cairo_move_to(cr, 0.0, y);
         cairo_line_to(cr, ctx.windowW, y);
      }
      cairo_stroke(cr);

      // An idle header still scrolls, dimly. A host shows the window before the
      // first note and the suite's screenshots are taken there, and a sonogram
      // with nothing on it reads as a broken graph rather than a quiet one. The
      // first frame lays a trace across the whole width, and a new stroke is
      // added whenever the trace thins out -- at a quarter of the weight a real
      // syllable gets, so playing something is unmistakable.
      if (!mPrimed) {
         mPrimed = true;
         for (int i = 0; i < 9; ++i) {
            push(ctx, 0x51ED27u + static_cast<uint32_t>(i),
                 ctx.windowW * (0.02 + 0.115 * i));
            mMarks[mCount - 1].weight = kIdleWeight;
         }
      }
      if (ctx.eventCounter == mLastEvent && mCount < 7 && (++mIdleTick & 31u) == 0u) {
         push(ctx, 0x51ED27u + mIdleSeed++, ctx.windowW + 4.0);
         mMarks[mCount - 1].weight = kIdleWeight;
      }

      // New syllables. Capped per frame: a dawn chorus can put dozens of them
      // between two repaints, and drawing all of them would be a smear rather
      // than a sonogram.
      if (ctx.eventCounter != mLastEvent) {
         const uint32_t fresh = ctx.eventCounter - mLastEvent;
         const uint32_t take = fresh > 4u ? 4u : fresh;
         for (uint32_t i = 0; i < take; ++i)
            push(ctx, mLastEvent + fresh - take + i + 1u,
                 ctx.windowW + 6.0 * static_cast<double>(take - 1u - i));
         mLastEvent = ctx.eventCounter;
      }

      const double speed = ctx.windowW / 110.0;
      int kept = 0;
      cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
      for (int i = 0; i < mCount; ++i) {
         Mark m = mMarks[i];
         m.x -= speed;
         if (m.x + m.len < -2.0)
            continue;
         mMarks[kept++] = m;

         // Fading in from the right and out to the left, so nothing appears or
         // vanishes at an edge.
         const double edge = std::min(1.0, std::min(m.x + m.len, ctx.windowW - m.x) / 40.0);
         const double alpha = 0.72 * m.weight * std::max(0.0, edge);
         if (alpha <= 0.005)
            continue;
         cairo_set_line_width(cr, m.width);
         setColor(cr, ctx.theme->accent, alpha);
         cairo_move_to(cr, m.x, m.y0);
         // Two segments rather than one, so a stroke can arch or dip the way a
         // real syllable does instead of only sloping.
         cairo_line_to(cr, m.x + m.len * 0.5, m.ymid);
         cairo_line_to(cr, m.x + m.len, m.y1);
         cairo_stroke(cr);

         // The newest few catch the light, which is what makes the header read
         // as something happening now.
         if (m.weight > 0.85) {
            setColor(cr, ctx.theme->highlight, (m.weight - 0.85) * 4.0 * 0.5);
            cairo_set_line_width(cr, 1.2);
            cairo_move_to(cr, m.x + m.len * 0.5, m.ymid);
            cairo_line_to(cr, m.x + m.len, m.y1);
            cairo_stroke(cr);
         }
      }
      mCount = kept;

      // Everything already on screen ages by the same amount per frame, so a
      // stroke is bright for about a second. The idle strokes are held at their
      // own weight rather than fading, or the header would empty out again.
      for (int i = 0; i < mCount; ++i)
         if (mMarks[i].weight > kIdleWeight)
            mMarks[i].weight = std::max(kIdleWeight, mMarks[i].weight * 0.985);
   }

private:
   struct Mark {
      double x, y0, ymid, y1, len, width, weight;
   };

   // A stroke's shape, deterministic in the syllable number.
   static uint32_t hash32(uint32_t x) {
      x ^= x >> 16;
      x *= 0x7FEB352Du;
      x ^= x >> 15;
      x *= 0x846CA68Bu;
      x ^= x >> 16;
      return x;
   }
   static double unit(uint32_t x) { return (hash32(x) >> 8) * (1.0 / 16777216.0); }

   void push(const OrnamentContext &ctx, uint32_t counter, double x) {
      if (mCount >= kMax) {
         // Drop the oldest, which is the leftmost.
         for (int i = 1; i < mCount; ++i)
            mMarks[i - 1] = mMarks[i];
         --mCount;
      }
      const double h = ctx.headerH;
      const double base = 0.10 + 0.78 * unit(counter * 2654435761u + 1u);
      const double sweep = (unit(counter * 40503u + 7u) - 0.5) * 0.5;
      const double bend = (unit(counter * 2246822519u + 13u) - 0.5) * 0.45;
      Mark m;
      m.x = x;
      // A sonogram's frequency axis runs upwards, so a rising syllable has to
      // draw upwards: y falls as the pitch rises.
      m.y0 = h * (1.0 - base);
      m.y1 = h * (1.0 - (base + sweep));
      m.ymid = h * (1.0 - (base + sweep * 0.5 + bend));
      m.y0 = clamp(m.y0, 3.0, h - 3.0);
      m.y1 = clamp(m.y1, 3.0, h - 3.0);
      m.ymid = clamp(m.ymid, 3.0, h - 3.0);
      m.len = ctx.windowW * (0.012 + 0.045 * unit(counter * 374761393u + 19u));
      m.width = 1.4 + 1.4 * unit(counter * 668265263u + 23u);
      m.weight = 1.0;
      mMarks[mCount++] = m;
   }

   static double clamp(double v, double lo, double hi) {
      return v < lo ? lo : (v > hi ? hi : v);
   }

   static constexpr int kMax = 64;
   static constexpr double kIdleWeight = 0.22;
   Mark mMarks[kMax];
   int mCount = 0;
   uint32_t mLastEvent = 0;
   bool mPrimed = false;
   uint32_t mIdleTick = 0;
   uint32_t mIdleSeed = 100;
};

Sonogram gSonogram;

const WindowSpec kSpec = {
   /* wordmarkFirst  */ "Chirp",
   /* wordmarkSecond */ "Parade",
   /* subtitle       */ "SYNTHETIC BIRD INSTRUMENT",
   /* version        */ kPluginVersion,
   /* voiceNoun      */ "voices",
   /* eventNoun      */ "sung",
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
   /* ornament       */ &gSonogram,
};

} // namespace

Gui *createGui(GuiDelegate &delegate) {
   static WindowSpec spec = kSpec;
   spec.params = paramTable();
   return createWindow(delegate, spec);
}

} // namespace chirpparade
