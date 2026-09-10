// RainyDay's window: its panels, its colours and its rain.
//
// The window itself -- the layout engine, the widgets, the preset browser, the
// typed value entry -- is shared by the suite and lives in
// shared/src/gui/window.cpp. What is here is only what makes this window
// RainyDay's.

#include <cmath>

#include "verdalis/gui/window.h"

#include "params.h"
#include "rainyday.h"

namespace rainyday {

namespace {

// ------------------------------------------------------------------- geometry
//
// Cell and panel sizes are the suite's (verdalis/gui/window.h); a knob is the
// same size in every plugin. Only the overall width, which follows from the
// busiest row, is ours.
//
// Ten cells wide, plus the padding and gaps of the busiest row, which is the
// four-panel one. Rows with fewer panels stretch their last panel to match.
// Widened by one cell when Slosh joined the RAIN panel, which needs eight
// columns rather than seven.
constexpr int kContentW = 1060;
constexpr int kWindowH = 844;

// ------------------------------------------------------------------- panels

// Nine per line, which is one row of the panel. Slosh sits next to Splash,
// because it lengthens what Splash starts, and the two Tack controls sit at the
// end beside Note Tracking: they are the surface being struck rather than the
// drop, so they belong after everything the drop itself does.
constexpr uint32_t kRainParams[] = {
   kParamSurface,  kParamDensity, kParamClumping, kParamDropPitch,   kParamPitchSpread,
   kParamDropDecay, kParamDecaySpread,
   kParamTonality, kParamBubble,  kParamImpact,   kParamSplash,      kParamSlosh,
   kParamLevelSpread, kParamChirp,
   kParamNoteTracking,
};
constexpr uint32_t kSpaceParams[] = {kParamDistance,  kParamAir,       kParamSpaceAmount,
                                 kParamSpaceSize, kParamSpaceDamping};
constexpr uint32_t kEnvParams[] = {kParamAttack,     kParamDecay,       kParamSustain,
                               kParamRelease,    kParamVelToLevel,  kParamVelToDensity};
constexpr uint32_t kFilterParams[] = {kParamFilterType, kParamHighpass,   kParamFilterCutoff,
                                  kParamFilterReso, kParamFilterKeyTrack};
// The two layers of the same rain, each with its own placement.
constexpr uint32_t kDistantParams[] = {kParamBedLevel, kParamBedTone,  kParamBedBody,
                                   kParamBedDrift, kParamBedWidth, kParamBedPan};
constexpr uint32_t kCloseParams[] = {kParamWidth, kParamDropPan};
constexpr uint32_t kOutParams[] = {kParamGain, kParamMaxDroplets, kParamSeed};
// Drops landing on a hard surface, as its own layer. Eight controls, and the
// same eight RiverFlow's Trickle panel carries -- it is the same generator.
constexpr uint32_t kTrickleParams[] = {
   kParamTrickleLevel,  kParamTrickleRate,   kParamTrickleSize, kParamTrickleSpread,
   kParamTrickleDecay,  kParamTrickleImpact, kParamStoneTone,   kParamTrickleSplash,
};

#define PANEL(title, cols, rows, arr)                                                              \
   { title, cols, rows, arr, static_cast<int>(sizeof(arr) / sizeof(arr[0])) }

constexpr PanelSpec kPanelSpecs[] = {
   PANEL("RAIN", 8, 2, kRainParams),     PANEL("DISTANT", 3, 2, kDistantParams),
   PANEL("ENVELOPE", 3, 2, kEnvParams),  PANEL("FILTER", 3, 2, kFilterParams),
   PANEL("SPACE", 3, 2, kSpaceParams),   PANEL("CLOSE", 1, 2, kCloseParams),
   PANEL("TRICKLE", 4, 2, kTrickleParams),
   PANEL("OUTPUT", 3, 1, kOutParams),
};
#undef PANEL

constexpr int kNumPanels = static_cast<int>(sizeof(kPanelSpecs) / sizeof(kPanelSpecs[0]));

// Which panels share a row, in order. DISTANT and CLOSE sit on the right-hand
// edge of their rows so the two layer panels read as a pair. The activity meter
// fills what is left of the last row.
constexpr int kRowStart[] = {0, 2, 6};
constexpr int kRowCount[] = {2, 4, 2};
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
// Rain blue. The greys are tinted towards it, so the whole window reads as one
// instrument rather than a grey chassis with a coloured knob.
constexpr Theme kTheme = {
   /* bgTop     */ {0.086, 0.106, 0.137},
   /* bgBottom  */ {0.055, 0.067, 0.086},
   /* panelFill */ {0.118, 0.145, 0.184},
   /* panelEdge */ {0.169, 0.208, 0.259},
   /* knobFace  */ {0.063, 0.078, 0.102},
   /* track     */ {0.188, 0.231, 0.286},
   /* accent    */ {0.345, 0.714, 0.910},
   /* text      */ {0.894, 0.918, 0.941},
   /* textDim   */ {0.494, 0.541, 0.600},
   /* textMute  */ {0.353, 0.400, 0.455},
   /* highlight */ {0.894, 0.918, 0.941},
};

// -------------------------------------------------------------------- mixer
//
// The layers a preset is actually balanced between, far to near: the
// far-field wash, then the drops on the surface, then the instrument's
// output. Each strip's fader is the level knob that already sits on that
// layer's panel, so the mixer adds no parameters of its own.
//
// The close droplets are deliberately not a strip. They have no level: their
// Density is loudness compensated, so the droplets are the body of the output
// and the output gain is their fader. Giving them a second one would only
// duplicate the master.
constexpr MixerStrip kMixerStrips[] = {
   /* label      level               pan            width            master */
   {"DISTANT", kParamBedLevel, kParamBedPan, kParamBedWidth, false},
   {"TRICKLE", kParamTrickleLevel, kNoParam, kNoParam, false},
   {"OUTPUT", kParamGain, kNoParam, kNoParam, true},
};

constexpr int kNumStrips = static_cast<int>(sizeof(kMixerStrips) / sizeof(kMixerStrips[0]));

// ----------------------------------------------------------------- ornament

// Rain falling through the header, as heavy as the engine is busy. Nothing is
// stored per streak: each one's lane and speed come from a hash of its index,
// so the whole thing is one phase counter.
class RainStreaks final : public HeaderOrnament {
public:
   void draw(cairo_t *cr, const OrnamentContext &ctx) override {
      mPhase += 1.0;
      const int streaks = static_cast<int>(ctx.voiceLoad * 90.0);
      for (int i = 0; i < streaks; ++i) {
         // A cheap deterministic hash keeps each streak in its own lane.
         const double h = std::fmod(std::sin(i * 12.9898) * 43758.5453, 1.0);
         const double x = std::fabs(h) * ctx.windowW;
         const double speed = 2.0 + std::fabs(std::fmod(h * 7.0, 1.0)) * 4.0;
         const double y = std::fmod(mPhase * speed + std::fabs(h) * 400.0, ctx.headerH + 24.0);
         setColor(cr, ctx.theme->accent, 0.10 + 0.10 * ctx.voiceLoad);
         cairo_set_line_width(cr, 1.0);
         cairo_move_to(cr, x, y - 14.0);
         cairo_line_to(cr, x, y);
         cairo_stroke(cr);
      }
   }

private:
   double mPhase = 0.0;
};

RainStreaks gStreaks;

const WindowSpec kSpec = {
   /* wordmarkFirst  */ "Rainy",
   /* wordmarkSecond */ "Day",
   /* subtitle       */ "SYNTHETIC RAIN INSTRUMENT",
   /* version        */ kPluginVersion,
   /* voiceNoun      */ "droplets",
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
   /* mixer          */ kMixerStrips,
   /* mixerCount     */ kNumStrips,
   /* ornament       */ &gStreaks,
};

} // namespace

Gui *createGui(GuiDelegate &delegate) {
   static WindowSpec spec = kSpec;
   spec.params = paramTable();
   return createWindow(delegate, spec);
}

} // namespace rainyday
