// RiverFlow's window: its panels, its colours and its run of water.
//
// The window itself -- the layout engine, the widgets, the preset browser, the
// typed value entry -- is shared by the suite and lives in
// shared/src/gui/window.cpp. What is here is only what makes this window
// RiverFlow's.

#include <cmath>

#include "verdalis/gui/window.h"

#include "params.h"
#include "riverflow.h"

namespace riverflow {

namespace {

// ------------------------------------------------------------------- geometry
//
// Cell and panel sizes are the suite's (verdalis/gui/window.h); a knob is the
// same size in every plugin. Only the overall width, which follows from the
// busiest row, is ours: the three layers of water each need four cells across
// two rows, and that fixes the window.
constexpr int kContentW = 1072;
constexpr int kWindowH = 740;

// ------------------------------------------------------------------- panels

// Water first, then the two populations of events that sit on it, then the
// pool, then where the listener is standing.
constexpr uint32_t kFlowParams[] = {
   kParamWaterType,  kParamWaterBlend, kParamFlowLevel, kParamFlowTilt,
   kParamFlowBody,   kParamTurbulence, kParamSurgeRate, kParamFlowGrain,
};
constexpr uint32_t kStoneParams[] = {
   kParamDabbleRate,    kParamDabbleLevel, kParamDabbleSize,    kParamDabbleSpread,
   kParamDabbleCluster, kParamDabbleSpill, kParamDabbleDamping, kParamDabbleGlug,
};
constexpr uint32_t kTrickleParams[] = {
   kParamTrickleRate,  kParamTrickleLevel,   kParamTrickleSize, kParamTrickleSpread,
   kParamTrickleDecay, kParamTrickleImpact,  kParamStoneTone,   kParamSplash,
};
constexpr uint32_t kPlungeParams[] = {kParamPlungeLevel, kParamPlungeTone, kParamPlungeDepth,
                                      kParamPlungeQ};
constexpr uint32_t kReachParams[] = {
   kParamBankType,    kParamDistance,  kParamAir,           kParamWidth,
   kParamFlowWidth,   kParamSpaceAmount, kParamSpaceSize,   kParamSpaceDamping,
};
constexpr uint32_t kFilterParams[] = {kParamFilterType, kParamHighpass, kParamFilterCutoff,
                                      kParamFilterReso, kParamFilterKeyTrack};
constexpr uint32_t kEnvParams[] = {kParamAttack,  kParamDecay,      kParamSustain,
                                   kParamRelease, kParamVelToLevel, kParamVelToFlow};
constexpr uint32_t kOutParams[] = {kParamGain, kParamMaxEvents, kParamSeed};

#define PANEL(title, cols, rows, arr)                                                              \
   { title, cols, rows, arr, static_cast<int>(sizeof(arr) / sizeof(arr[0])) }

constexpr PanelSpec kPanelSpecs[] = {
   PANEL("FLOW", 4, 2, kFlowParams),      PANEL("STONES", 4, 2, kStoneParams),
   PANEL("TRICKLE", 4, 2, kTrickleParams), PANEL("PLUNGE", 2, 2, kPlungeParams),
   PANEL("REACH", 4, 2, kReachParams),    PANEL("FILTER", 3, 2, kFilterParams),
   PANEL("ENVELOPE", 6, 1, kEnvParams),   PANEL("OUTPUT", 3, 1, kOutParams),
};
#undef PANEL

constexpr int kNumPanels = static_cast<int>(sizeof(kPanelSpecs) / sizeof(kPanelSpecs[0]));

// Which panels share a row, in order. The activity meter fills what is left of
// the last row.
constexpr int kRowStart[] = {0, 3, 6};
constexpr int kRowCount[] = {3, 3, 2};
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
// River green: moss on wet stone, and meltwater over it. Deliberately the
// green side of the suite's water plugins rather than the blue -- RainyDay's
// rain blue and ShoreBreak's sea green are both taken, and a forest river is
// neither the sky nor the sea. The greys carry the same green so the chassis
// reads as wet stone; the highlight is the white of sun on moving water.
constexpr Theme kTheme = {
   /* bgTop     */ {0.078, 0.110, 0.086},
   /* bgBottom  */ {0.047, 0.070, 0.055},
   /* panelFill */ {0.102, 0.145, 0.114},
   /* panelEdge */ {0.153, 0.212, 0.165},
   /* knobFace  */ {0.058, 0.082, 0.063},
   /* track     */ {0.173, 0.235, 0.184},
   /* accent    */ {0.341, 0.780, 0.478},
   /* text      */ {0.898, 0.929, 0.898},
   /* textDim   */ {0.478, 0.565, 0.494},
   /* textMute  */ {0.345, 0.416, 0.357},
   /* highlight */ {0.949, 0.988, 0.945},
};

// ----------------------------------------------------------------- ornament

// The run across the header: streamlines drifting downstream, with a ring
// spreading wherever a dabble happens.
//
// The streamlines are what a river looks like from the bank -- bands of water
// moving at slightly different speeds, so they shear past each other and never
// line up. The rings are the plugin's own events: one per dabble, placed and
// sized deterministically in the dabble number, so the same dabble always
// draws the same ring and a fixed Seed gives a repeatable picture as well as a
// repeatable sound.
//
// It animates always. A river with a still surface is not a quiet river, it is
// a broken graph -- the same reasoning that keeps a few strokes on
// ChirpParade's sonogram when nothing is playing.
class Run final : public HeaderOrnament {
public:
   bool animating(uint32_t) const override { return true; }

   void draw(cairo_t *cr, const OrnamentContext &ctx) override {
      mPhase += 0.0022;

      // New dabbles since the last frame become rings. Capped, because a
      // preset at forty dabbles a second would otherwise ask for forty rings a
      // frame and none of them would be visible anyway.
      if (ctx.eventCounter != mLastCounter) {
         const uint32_t fresh = ctx.eventCounter - mLastCounter;
         const uint32_t n = fresh > 3u ? 3u : fresh;
         for (uint32_t k = 0; k < n; ++k)
            addRing(mLastCounter + fresh - n + k + 1u, ctx);
         mLastCounter = ctx.eventCounter;
      }

      // ------------------------------------------------------- streamlines
      const int kLines = 7;
      const int steps = 88;
      for (int l = 0; l < kLines; ++l) {
         const double u = (l + 0.5) / kLines;
         const double y0 = ctx.headerH * (0.20 + 0.72 * u);
         // Slower near the banks, faster mid-stream: the shear is what makes
         // it read as flow rather than as a set of wavy lines.
         const double speed = 0.6 + 1.5 * std::sin(M_PI * u);
         const double amp = ctx.headerH * 0.045 * (0.5 + u);
         cairo_new_path(cr);
         for (int i = 0; i <= steps; ++i) {
            const double t = static_cast<double>(i) / steps;
            const double x = t * ctx.windowW;
            const double y = y0 +
                             amp * std::sin(2.0 * M_PI * (t * (2.1 + 0.7 * l) + mPhase * speed)) +
                             amp * 0.5 * std::sin(2.0 * M_PI * (t * (4.7 - 0.3 * l) -
                                                                mPhase * speed * 1.7));
            if (i == 0)
               cairo_move_to(cr, x, y);
            else
               cairo_line_to(cr, x, y);
         }
         cairo_set_line_width(cr, 1.0);
         setColor(cr, ctx.theme->accent, 0.07 + 0.13 * ctx.voiceLoad + 0.05 * std::sin(M_PI * u));
         cairo_stroke(cr);
      }

      // -------------------------------------------------------------- rings
      for (auto &r : mRings) {
         if (r.life <= 0.0)
            continue;
         r.life -= 0.012;
         r.radius += r.growth;
         const double a = r.life * r.life * 0.55;
         // Flattened, because the surface is being looked across rather than
         // down at.
         cairo_save(cr);
         cairo_translate(cr, r.x, r.y);
         cairo_scale(cr, 1.0, 0.34);
         cairo_new_path(cr);
         cairo_arc(cr, 0.0, 0.0, r.radius, 0.0, 2.0 * M_PI);
         cairo_restore(cr);
         cairo_set_line_width(cr, 1.1);
         setColor(cr, ctx.theme->highlight, a);
         cairo_stroke(cr);
      }
   }

private:
   struct Ring {
      double x = 0.0, y = 0.0, radius = 0.0, growth = 0.0, life = 0.0;
   };

   // Deterministic in the dabble number: the same dabble always lands in the
   // same place, so a pinned Seed gives a repeatable picture too.
   void addRing(uint32_t n, const OrnamentContext &ctx) {
      uint32_t h = n * 2654435761u;
      h ^= h >> 15;
      h *= 0x85EBCA6Bu;
      h ^= h >> 13;
      const double fx = static_cast<double>(h & 0xFFFFu) / 65535.0;
      const double fy = static_cast<double>((h >> 16) & 0xFFu) / 255.0;
      Ring &r = mRings[mNext];
      mNext = (mNext + 1) % kMaxRings;
      r.x = 0.05 * ctx.windowW + fx * 0.90 * ctx.windowW;
      r.y = ctx.headerH * (0.24 + 0.66 * fy);
      r.radius = 1.5;
      r.growth = 0.55 + 0.9 * fy;
      r.life = 1.0;
   }

   static constexpr int kMaxRings = 12;
   Ring mRings[kMaxRings];
   int mNext = 0;
   uint32_t mLastCounter = 0;
   double mPhase = 0.0;
};

Run gRun;

const WindowSpec kSpec = {
   /* wordmarkFirst  */ "River",
   /* wordmarkSecond */ "Flow",
   /* subtitle       */ "SYNTHETIC WATER INSTRUMENT",
   /* version        */ kPluginVersion,
   /* voiceNoun      */ "bubbles",
   /* eventNoun      */ "dabbles",
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
   /* ornament       */ &gRun,
};

} // namespace

Gui *createGui(GuiDelegate &delegate) {
   static WindowSpec spec = kSpec;
   spec.params = paramTable();
   return createWindow(delegate, spec);
}

} // namespace riverflow
