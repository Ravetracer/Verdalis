// SkyHowl's window: its panels, its colours and the flow across its header.
//
// The window itself -- the layout engine, the widgets, the preset browser, the
// typed value entry -- is shared by the suite and lives in
// shared/src/gui/window.cpp. What is here is only what makes this window
// SkyHowl's.

#include <cmath>

#include "verdalis/gui/window.h"

#include "params.h"
#include "skyhowl.h"

namespace skyhowl {

namespace {

// ------------------------------------------------------------------- geometry
//
// Cell and panel sizes are the suite's (verdalis/gui/window.h); a knob is the
// same size in every plugin. Only the overall width, which follows from the
// busiest row, is ours: the HOWL panel is five cells wide because an aeolian
// tone has nine things worth naming.
constexpr int kContentW = 1156;
constexpr int kWindowH = 740;

// ------------------------------------------------------------------- panels

// The flow first, because everything else is driven by it, and the sources it
// drives after. Speed Law sits at the end of AIRFLOW because it colours
// everything before it.
constexpr uint32_t kWindParams[] = {
   kParamWindSpeed, kParamTurbulence, kParamGustRate, kParamGustDepth,
   kParamGustLength, kParamGustShape,  kParamSquall,   kParamSquallRate,
};
constexpr uint32_t kAirflowParams[] = {kParamFlowLevel, kParamFlowTone,   kParamFlowTilt,
                                       kParamBuffet,    kParamBuffetTone, kParamHiss,
                                       kParamSpeedLaw};
constexpr uint32_t kHowlParams[] = {
   kParamHowlAmount, kParamObstacle,  kParamHowlSize,      kParamHowlSpread, kParamHowlVoices,
   kParamHowlReso,   kParamHowlTrack, kParamHowlThreshold, kParamWarble,
};
constexpr uint32_t kRustleParams[] = {
   kParamRustleAmount, kParamFoliage,       kParamRustleSize, kParamRustleDensity,
   kParamRustleSpread, kParamRustleDecay,   kParamRustleThreshold, kParamClatter,
};
constexpr uint32_t kPlaceParams[] = {kParamTerrain,     kParamDistance, kParamAir,
                                     kParamWidth,       kParamSpaceAmount,
                                     kParamSpaceSize,   kParamSpaceDamping};
constexpr uint32_t kFilterParams[] = {kParamFilterType, kParamHighpass, kParamFilterCutoff,
                                      kParamFilterReso, kParamFilterKeyTrack};
constexpr uint32_t kEnvParams[] = {kParamAttack,  kParamDecay,      kParamSustain,
                                   kParamRelease, kParamVelToLevel, kParamVelToSpeed};
constexpr uint32_t kOutParams[] = {kParamGain, kParamMaxGusts, kParamSeed};

#define PANEL(title, cols, rows, arr)                                                              \
   { title, cols, rows, arr, static_cast<int>(sizeof(arr) / sizeof(arr[0])) }

constexpr PanelSpec kPanelSpecs[] = {
   PANEL("WIND", 4, 2, kWindParams),     PANEL("AIRFLOW", 4, 2, kAirflowParams),
   PANEL("HOWL", 5, 2, kHowlParams),     PANEL("RUSTLE", 4, 2, kRustleParams),
   PANEL("PLACE", 4, 2, kPlaceParams),   PANEL("FILTER", 3, 2, kFilterParams),
   PANEL("ENVELOPE", 6, 1, kEnvParams),  PANEL("OUTPUT", 3, 1, kOutParams),
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
// Dust coral. The suite's other three plugins are all cold -- rain blue,
// lightning violet, sea green -- so wind, which everyone expects to be grey,
// is the one that gets to be warm: dry grass and dust carried across a plain,
// against a chassis with a red-brown cast rather than a neutral one. The
// highlight is the bleached white of a sky at the edge of a squall.
constexpr Theme kTheme = {
   /* bgTop     */ {0.086, 0.063, 0.055},
   /* bgBottom  */ {0.051, 0.035, 0.031},
   /* panelFill */ {0.114, 0.082, 0.071},
   /* panelEdge */ {0.188, 0.129, 0.098},
   /* knobFace  */ {0.063, 0.043, 0.035},
   /* track     */ {0.220, 0.153, 0.118},
   /* accent    */ {0.941, 0.518, 0.361},
   /* text      */ {0.949, 0.890, 0.855},
   /* textDim   */ {0.627, 0.518, 0.467},
   /* textMute  */ {0.431, 0.341, 0.298},
   /* highlight */ {1.000, 0.922, 0.863},
};

// -------------------------------------------------------------------- mixer
//
// The three layers a preset is balanced between, in the order the wind reaches
// the listener: the broadband bed out in the flow, the tone the obstacle sheds
// off it, and the foliage the whole thing is heard through. Each fader is the
// level that already sits on that layer's own panel, so the mixer adds no
// parameters.
//
// Buffet and Hiss are not strips. Both have their own level, but both are the
// bed's low and high ends rather than layers of their own -- they sit on
// AIRFLOW beside Flow Level because moving them changes what the bed sounds
// like, not how much of it there is against everything else.
constexpr MixerStrip kMixerStrips[] = {
   {"FLOW", kParamFlowLevel, kNoParam, kNoParam, false},
   {"HOWL", kParamHowlAmount, kNoParam, kNoParam, false},
   {"RUSTLE", kParamRustleAmount, kNoParam, kNoParam, false},
   {"OUTPUT", kParamGain, kNoParam, kNoParam, true},
};

constexpr int kNumStrips = static_cast<int>(sizeof(kMixerStrips) / sizeof(kMixerStrips[0]));

// ----------------------------------------------------------------- ornament

// The flow across the header: streaklines, the way a wind tunnel shows a flow
// it cannot otherwise photograph.
//
// Each streak runs left to right at its own speed. Two things move them: the
// activity meter, which is how many gusts are in flight, and the gust counter
// itself -- every new gust puts a shove into the field that decays away, so a
// gust is visible as a surge of the whole flow rather than as one more line.
// The streaks are fixed rather than respawned, so the header reads as one
// flow field being blown harder rather than as a particle effect.
class Streaklines final : public HeaderOrnament {
public:
   bool animating(uint32_t) const override { return true; }

   void draw(cairo_t *cr, const OrnamentContext &ctx) override {
      // The wrap below divides by a width, and fmod by zero is NaN -- which
      // cairo would then swallow into an error state rather than draw. A
      // zero-width header is not expected, but it is one call away.
      if (!(ctx.windowW > 1.0) || !(ctx.headerH > 1.0))
         return;

      // A new gust shoves the whole field. Detected here rather than in
      // animating(), which must not change state.
      if (ctx.eventCounter != mLastEvent) {
         mLastEvent = ctx.eventCounter;
         mShove = 1.0;
      }
      mShove *= 0.982;

      const double speed = 0.0022 + 0.0075 * ctx.voiceLoad + 0.010 * mShove;
      mPhase += speed;
      if (mPhase > 1.0)
         mPhase -= std::floor(mPhase);

      const double load = ctx.voiceLoad + 0.5 * mShove;
      cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

      for (int i = 0; i < kStreaks; ++i) {
         // Deterministic in the streak's index: the field is the same field
         // every time the window opens, and only the wind through it changes.
         const double lane = (i * 7 % kStreaks) / static_cast<double>(kStreaks);
         const double y = ctx.headerH * (0.10 + 0.80 * lane);
         const double rate = 0.55 + 1.35 * ((i * 13 % 17) / 16.0);
         const double len = ctx.windowW * (0.05 + 0.13 * ((i * 11 % 19) / 18.0)) *
                            (0.6 + 0.8 * load);

         // Wrapped over one and a half window widths, so a streak leaves the
         // right edge before its head comes back in on the left.
         const double span = ctx.windowW * 1.5;
         double x = std::fmod(mPhase * rate * span + i * span / kStreaks, span) - len;

         // The base is what the header looks like with nothing playing, which
         // is how a host shows the window before the first note and how the
         // suite's screenshots are taken. ShoreBreak's surf line sits at 0.10
         // idle; anything much under that reads as an empty header.
         const double alpha = 0.10 + 0.26 * load * (0.4 + 0.6 * ((i * 5 % 7) / 6.0));
         cairo_set_line_width(cr, 0.8 + 0.9 * ((i * 3 % 5) / 4.0));
         setColor(cr, ctx.theme->accent, alpha);
         cairo_move_to(cr, x, y);
         cairo_line_to(cr, x + len, y);
         cairo_stroke(cr);

         // The head of the fastest streaks catches the light.
         if (rate > 1.45) {
            setColor(cr, ctx.theme->highlight, alpha * 1.6);
            cairo_set_line_width(cr, 1.4);
            cairo_move_to(cr, x + len - 6.0, y);
            cairo_line_to(cr, x + len, y);
            cairo_stroke(cr);
         }
      }
   }

private:
   static constexpr int kStreaks = 26;
   double mPhase = 0.0;
   double mShove = 0.0;
   uint32_t mLastEvent = 0;
};

Streaklines gStreaklines;

const WindowSpec kSpec = {
   /* wordmarkFirst  */ "Sky",
   /* wordmarkSecond */ "Howl",
   /* subtitle       */ "SYNTHETIC WIND INSTRUMENT",
   /* version        */ kPluginVersion,
   /* voiceNoun      */ "gusts",
   /* eventNoun      */ "passed",
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
   /* ornament       */ &gStreaklines,
};

} // namespace

Gui *createGui(GuiDelegate &delegate) {
   static WindowSpec spec = kSpec;
   spec.params = paramTable();
   return createWindow(delegate, spec);
}

} // namespace skyhowl
