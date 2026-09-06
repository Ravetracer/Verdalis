// ThunderClap's window: its panels, its colours and its lightning.
//
// The window itself -- the layout engine, the widgets, the preset browser, the
// typed value entry -- is shared by the suite and lives in
// shared/src/gui/window.cpp. What is here is only what makes this window
// ThunderClap's.

#include <cstdint>

#include "verdalis/gui/window.h"

#include "params.h"
#include "thunderclap.h"

namespace thunderclap {

namespace {

// ------------------------------------------------------------------- geometry
//
// Cell and panel sizes are the suite's (verdalis/gui/window.h); only the
// overall width, which follows from the busiest row, is ours.
constexpr int kContentW = 928;
constexpr int kWindowH = 740;

// ------------------------------------------------------------------- panels

constexpr uint32_t kStrikeParams[] = {
   kParamDistance, kParamHeight,  kParamCloudSpread, kParamTortuosity,
   kParamBranching, kParamStrokes, kParamStrokeGap,   kParamVariation,
};
constexpr uint32_t kSoundParams[] = {
   kParamCrack,  kParamWeight,     kParamSwell, kParamRumble,
   kParamRumbleTone, kParamAir, kParamScatter, kParamFocus,
};
constexpr uint32_t kStereoParams[] = {kParamWidth, kParamPan, kParamRumbleWidth, kParamDrift};
constexpr uint32_t kEnvParams[] = {kParamMode,      kParamAttack,     kParamRelease,
                                   kParamStormRate, kParamVelToLevel, kParamVelToDistance};
constexpr uint32_t kFilterParams[] = {kParamFilterType, kParamHighpass,   kParamFilterCutoff,
                                      kParamFilterReso, kParamFilterKeyTrack};
constexpr uint32_t kEchoParams[] = {kParamEchoLevel, kParamEchoCount, kParamEchoSpread,
                                    kParamEchoDamping};
constexpr uint32_t kSpaceParams[] = {kParamSpaceAmount, kParamSpaceSize, kParamSpaceDamping};
constexpr uint32_t kOutParams[] = {kParamGain, kParamMaxShocks, kParamSeed};
constexpr uint32_t kDynParams[] = {kParamCompress, kParamCompAttack, kParamCompRelease};
constexpr uint32_t kImpactParams[] = {kParamImpact};

#define PANEL(title, cols, rows, arr)                                                              \
   { title, cols, rows, arr, static_cast<int>(sizeof(arr) / sizeof(arr[0])) }

constexpr PanelSpec kPanelSpecs[] = {
   PANEL("STRIKE", 4, 2, kStrikeParams),  PANEL("SOUND", 4, 2, kSoundParams),
   PANEL("STEREO", 2, 2, kStereoParams),  PANEL("ENVELOPE", 3, 2, kEnvParams),
   PANEL("FILTER", 3, 2, kFilterParams),  PANEL("ECHOES", 2, 2, kEchoParams),
   PANEL("SPACE", 2, 2, kSpaceParams),    PANEL("OUTPUT", 3, 1, kOutParams),
   PANEL("DYNAMICS", 3, 1, kDynParams),   PANEL("IMPACT", 1, 1, kImpactParams),
};
#undef PANEL

constexpr int kNumPanels = static_cast<int>(sizeof(kPanelSpecs) / sizeof(kPanelSpecs[0]));

// Which panels share a row, in order. The channel and its sound sit together on
// the first row, the ways of shaping it on the second, and the activity meter
// fills what is left of the last row.
constexpr int kRowStart[] = {0, 3, 7};
constexpr int kRowCount[] = {3, 4, 3};
constexpr int kNumRows = 3;

// The layout is a table, and a table is easy to break by adding a parameter to
// a panel that has no room for it, or by forgetting to put it on a panel at
// all. None of that should need a running window to notice, so it is checked
// here instead.
// panelWidth() and panelHeight() come from verdalis/gui/window.h.

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
static_assert(contentBottom() + 2 + kBarH + 24 <= kWindowH, "the window is too short for its panels");

// -------------------------------------------------------------------- theme
//
// RainyDay's greys, an octave darker, with the sky's own colour for the accent:
// the violet of a lightning channel rather than the blue of rain. The highlight
// is the white-hot core of the channel itself.
constexpr Theme kTheme = {
   /* bgTop     */ {0.094, 0.090, 0.125},
   /* bgBottom  */ {0.055, 0.051, 0.078},
   /* panelFill */ {0.129, 0.125, 0.173},
   /* panelEdge */ {0.184, 0.180, 0.247},
   /* knobFace  */ {0.067, 0.063, 0.094},
   /* track     */ {0.208, 0.204, 0.278},
   /* accent    */ {0.702, 0.588, 0.980},
   /* text      */ {0.918, 0.906, 0.945},
   /* textDim   */ {0.541, 0.518, 0.620},
   /* textMute  */ {0.400, 0.380, 0.475},
   /* highlight */ {0.980, 0.965, 0.900},
};

// ----------------------------------------------------------------- ornament

// A lightning bolt across the header, grown from the flash's own number, so no
// two flashes look alike and the same flash always draws the same bolt.
class Lightning final : public HeaderOrnament {
public:
   bool animating(uint32_t eventCounter) const override {
      return mFrames > 0 || eventCounter != mLastCounter;
   }

   void draw(cairo_t *cr, const OrnamentContext &ctx) override {
      if (ctx.eventCounter != mLastCounter) {
         mLastCounter = ctx.eventCounter;
         mFrames = kFrames;
         mSeed = ctx.eventCounter;
      }
      if (mFrames <= 0)
         return;
      --mFrames;
      drawBolt(cr, ctx, static_cast<double>(mFrames) / kFrames);
   }

private:
   static constexpr int kFrames = 24;

   // Deterministic in the flash number, so the same flash draws the same bolt
   // however often it is repainted.
   void drawBolt(cairo_t *cr, const OrnamentContext &ctx, double fade) {
      uint32_t h = mSeed * 2654435761u + 0x9E3779B9u;
      auto next = [&h]() {
         h ^= h << 13;
         h ^= h >> 17;
         h ^= h << 5;
         return (h >> 8) * (1.0 / 16777216.0);
      };
      const double x0 = kMargin + (ctx.windowW - 2 * kMargin) * (0.45 + 0.45 * next());
      const double glowA = 0.22 * fade;
      const double coreA = 0.55 + 0.45 * fade;

      auto strand = [&](double x, double y, double yEnd, double drift, double width, double alpha) {
         cairo_new_path(cr);
         cairo_move_to(cr, x, y);
         while (y < yEnd) {
            y += 4.0 + 7.0 * next();
            x += drift + (next() - 0.5) * 16.0;
            cairo_line_to(cr, x, y);
         }
         cairo_set_line_width(cr, width * 3.0);
         setColor(cr, ctx.theme->accent, alpha * glowA);
         cairo_stroke_preserve(cr);
         cairo_set_line_width(cr, width);
         setColor(cr, ctx.theme->highlight, alpha * coreA);
         cairo_stroke(cr);
      };

      cairo_set_line_join(cr, CAIRO_LINE_JOIN_MITER);
      cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
      strand(x0, -4.0, ctx.headerH + 4.0, (next() - 0.5) * 3.0, 1.6, 1.0);
      const int branches = 1 + static_cast<int>(next() * 2.0);
      for (int b = 0; b < branches; ++b) {
         const double by = 8.0 + next() * (ctx.headerH * 0.5);
         const double bx = x0 + (next() - 0.5) * 30.0;
         strand(bx, by, by + 14.0 + next() * 26.0, (next() < 0.5 ? -1.0 : 1.0) * (2.0 + 3.0 * next()),
                0.9, 0.6);
      }
      cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
   }

   uint32_t mLastCounter = 0;
   uint32_t mSeed = 0;
   int mFrames = 0;
};

Lightning gLightning;

const WindowSpec kSpec = {
   /* wordmarkFirst  */ "Thunder",
   /* wordmarkSecond */ "Clap",
   /* subtitle       */ "SYNTHETIC THUNDER INSTRUMENT",
   /* version        */ kPluginVersion,
   /* voiceNoun      */ "shocks",
   /* eventNoun      */ "flashes",
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
   /* ornament       */ &gLightning,
};

} // namespace

Gui *createGui(GuiDelegate &delegate) {
   static WindowSpec spec = kSpec;
   spec.params = paramTable();
   return createWindow(delegate, spec);
}

} // namespace thunderclap
