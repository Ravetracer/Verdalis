// ShoreBreak's window: its panels, its colours and its surf line.
//
// The window itself -- the layout engine, the widgets, the preset browser, the
// typed value entry -- is shared by the suite and lives in
// shared/src/gui/window.cpp. What is here is only what makes this window
// ShoreBreak's.

#include <cmath>

#include "verdalis/gui/window.h"

#include "params.h"
#include "shorebreak.h"

namespace shorebreak {

namespace {

// ------------------------------------------------------------------- geometry
//
// Cell and panel sizes are the suite's (verdalis/gui/window.h); a knob is the
// same size in every plugin. Only the overall width, which follows from the
// busiest row, is ours: the SURF panel is six cells wide because a breaking
// wave has eleven things worth naming.
constexpr int kContentW = 1072;
constexpr int kWindowH = 740;

// ------------------------------------------------------------------- panels

// Six per line, which is one row of the panel. Breaker and Precursor sit at
// the end because they colour everything before them.
constexpr uint32_t kSurfParams[] = {
   kParamWavePeriod, kParamSetVariation, kParamWaveSize,  kParamSizeVariation,
   kParamBreakAttack, kParamBreakDecay,
   kParamBreakTone,  kParamBreakBody,    kParamCrestSweep, kParamBreakerType,
   kParamPrecursor,   kParamBubbleMix,
};
constexpr uint32_t kFoamParams[] = {kParamFoamLevel, kParamFoamDecay, kParamFoamTone,
                                    kParamFoamDelay, kParamFizz};
constexpr uint32_t kSwellParams[] = {kParamSwellLevel, kParamSwellTone, kParamSwellDepth,
                                     kParamSwellRate,  kParamSwellWidth};
constexpr uint32_t kBubbleParams[] = {kParamBubbleRate, kParamBubblePitch, kParamBubbleSpread,
                                      kParamBubbleDecay};
constexpr uint32_t kWashParams[] = {kParamWashLevel, kParamWashDecay, kParamWashTone, kParamSand};
constexpr uint32_t kShoreParams[] = {kParamShoreType,   kParamDistance,  kParamAir,
                                     kParamWidth,       kParamSpaceAmount, kParamSpaceSize};
constexpr uint32_t kFilterParams[] = {kParamFilterType, kParamHighpass, kParamFilterCutoff,
                                      kParamFilterReso, kParamFilterKeyTrack,
                                      kParamSpaceDamping};
constexpr uint32_t kEnvParams[] = {kParamAttack,     kParamDecay,      kParamSustain,
                                   kParamRelease,    kParamVelToLevel, kParamVelToSize};
constexpr uint32_t kOutParams[] = {kParamGain, kParamMaxWaves, kParamSeed};

#define PANEL(title, cols, rows, arr)                                                              \
   { title, cols, rows, arr, static_cast<int>(sizeof(arr) / sizeof(arr[0])) }

constexpr PanelSpec kPanelSpecs[] = {
   PANEL("SURF", 6, 2, kSurfParams),      PANEL("FOAM", 3, 2, kFoamParams),
   PANEL("SWELL", 3, 2, kSwellParams),    PANEL("BUBBLES", 2, 2, kBubbleParams),
   PANEL("WASH", 2, 2, kWashParams),      PANEL("SHORE", 3, 2, kShoreParams),
   PANEL("FILTER", 3, 2, kFilterParams),  PANEL("ENVELOPE", 6, 1, kEnvParams),
   PANEL("OUTPUT", 3, 1, kOutParams),
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
// Sea green: the colour of shallow water over sand, against a chassis a shade
// warmer and greener than RainyDay's. The highlight is the white of foam.
constexpr Theme kTheme = {
   /* bgTop     */ {0.075, 0.114, 0.118},
   /* bgBottom  */ {0.043, 0.071, 0.075},
   /* panelFill */ {0.102, 0.153, 0.157},
   /* panelEdge */ {0.149, 0.220, 0.220},
   /* knobFace  */ {0.055, 0.086, 0.090},
   /* track     */ {0.169, 0.243, 0.243},
   /* accent    */ {0.310, 0.816, 0.729},
   /* text      */ {0.886, 0.929, 0.918},
   /* textDim   */ {0.463, 0.573, 0.557},
   /* textMute  */ {0.333, 0.427, 0.416},
   /* highlight */ {0.949, 0.980, 0.969},
};

// ----------------------------------------------------------------- ornament

// The surf line across the header: a slow swell with the foam of whatever is
// breaking riding on it. Two sine components at incommensurate rates, so it
// never repeats, and a foam crest whose brightness follows the voice load.
class SurfLine final : public HeaderOrnament {
public:
   bool animating(uint32_t) const override { return true; }

   void draw(cairo_t *cr, const OrnamentContext &ctx) override {
      mPhase += 0.0016;
      const double base = ctx.headerH * 0.72;
      const double amp = ctx.headerH * (0.06 + 0.10 * ctx.voiceLoad);

      // The water: one filled sweep, drawn as a path along the swell.
      cairo_new_path(cr);
      cairo_move_to(cr, 0.0, ctx.headerH);
      const int steps = 96;
      for (int i = 0; i <= steps; ++i) {
         const double u = static_cast<double>(i) / steps;
         const double x = u * ctx.windowW;
         const double y = base + amp * (std::sin(2.0 * M_PI * (u * 1.7 + mPhase)) +
                                        0.45 * std::sin(2.0 * M_PI * (u * 3.1 - mPhase * 1.6)));
         cairo_line_to(cr, x, y);
      }
      cairo_line_to(cr, ctx.windowW, ctx.headerH);
      cairo_close_path(cr);
      setColor(cr, ctx.theme->accent, 0.10 + 0.10 * ctx.voiceLoad);
      cairo_fill(cr);

      // The foam on top of it, brighter the busier the beach is.
      cairo_new_path(cr);
      for (int i = 0; i <= steps; ++i) {
         const double u = static_cast<double>(i) / steps;
         const double x = u * ctx.windowW;
         const double y = base + amp * (std::sin(2.0 * M_PI * (u * 1.7 + mPhase)) +
                                        0.45 * std::sin(2.0 * M_PI * (u * 3.1 - mPhase * 1.6)));
         if (i == 0)
            cairo_move_to(cr, x, y);
         else
            cairo_line_to(cr, x, y);
      }
      cairo_set_line_width(cr, 1.2);
      setColor(cr, ctx.theme->highlight, 0.10 + 0.35 * ctx.voiceLoad);
      cairo_stroke(cr);
   }

private:
   double mPhase = 0.0;
};

SurfLine gSurfLine;

const WindowSpec kSpec = {
   /* wordmarkFirst  */ "Shore",
   /* wordmarkSecond */ "Break",
   /* subtitle       */ "SYNTHETIC SURF INSTRUMENT",
   /* version        */ kPluginVersion,
   /* voiceNoun      */ "waves",
   /* eventNoun      */ nullptr,
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
   /* ornament       */ &gSurfLine,
};

} // namespace

Gui *createGui(GuiDelegate &delegate) {
   static WindowSpec spec = kSpec;
   spec.params = paramTable();
   return createWindow(delegate, spec);
}

} // namespace shorebreak
