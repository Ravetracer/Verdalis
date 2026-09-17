// InsectSwarm's window: its panels, its colours and its swarm.
//
// The window itself -- the layout engine, the widgets, the preset browser, the
// typed value entry -- is shared by the suite and lives in
// shared/src/gui/window.cpp. What is here is only what makes this window
// InsectSwarm's.

#include <cmath>

#include "verdalis/gui/window.h"

#include "params.h"
#include "insectswarm.h"

namespace insectswarm {

namespace {

// ------------------------------------------------------------------- geometry
//
// Cell and panel sizes are the suite's (verdalis/gui/window.h); a knob is the
// same size in every plugin. Only the overall width, which follows from the
// busiest row, is ours: the swarm, the wing and the flyby need four, three and
// three cells across two rows, and that fixes the window.
constexpr int kContentW = 988;
constexpr int kWindowH = 840;

// ------------------------------------------------------------------- panels

// The insects first -- what they are, what one of them sounds like, and what
// happens when one flies past -- then the other mechanism, then where the
// listener is standing.
constexpr uint32_t kSwarmParams[] = {
   kParamSpecies, kParamRateShift, kParamCount,      kParamSwarmLevel, kParamRasp,
   kParamSpread,  kParamWander,    kParamWanderRate, kParamRoam,       kParamRoamRate,
};
constexpr uint32_t kWingParams[] = {kParamTilt,   kParamFormant, kParamResonance,
                                    kParamStroke, kParamBite,    kParamFlutter};
constexpr uint32_t kFlybyParams[] = {kParamFlybyRate, kParamFlybyLevel, kParamFlybyRise,
                                     kParamFlybyPass, kParamFlybySpeed, kParamFlybySweep};
constexpr uint32_t kStridParams[] = {
   kParamStridLevel, kParamCarrier,    kParamCarrierQ,   kParamPulseRate,
   kParamEchemeRate, kParamDuty,       kParamChorus,     kParamStridSpread,
};
constexpr uint32_t kAirParams[] = {kParamDistance,    kParamAir,       kParamWidth,
                                   kParamSpaceAmount, kParamSpaceSize, kParamSpaceDamping};
constexpr uint32_t kBedParams[] = {kParamBedLevel, kParamBedTone, kParamBedTilt};
constexpr uint32_t kFilterParams[] = {kParamFilterType, kParamHighpass, kParamFilterCutoff,
                                      kParamFilterReso, kParamFilterKeyTrack};
constexpr uint32_t kEnvParams[] = {kParamAttack,  kParamDecay,      kParamSustain,
                                   kParamRelease, kParamVelToLevel, kParamVelToSwarm};
constexpr uint32_t kOutParams[] = {kParamGain, kParamMaxVoices, kParamSeed};

#define PANEL(title, cols, rows, arr)                                                              \
   { title, cols, rows, arr, static_cast<int>(sizeof(arr) / sizeof(arr[0])) }

constexpr PanelSpec kPanelSpecs[] = {
   PANEL("SWARM", 5, 2, kSwarmParams),      PANEL("WING", 3, 2, kWingParams),
   PANEL("FLYBY", 3, 2, kFlybyParams),      PANEL("STRIDULATE", 4, 2, kStridParams),
   PANEL("AIR", 3, 2, kAirParams),          PANEL("BED", 3, 1, kBedParams),
   PANEL("FILTER", 3, 2, kFilterParams),    PANEL("ENVELOPE", 3, 2, kEnvParams),
   PANEL("OUTPUT", 3, 1, kOutParams),
};
#undef PANEL

constexpr int kNumPanels = static_cast<int>(sizeof(kPanelSpecs) / sizeof(kPanelSpecs[0]));

// Which panels share a row, in order. The activity meter fills what is left of
// the last row.
constexpr int kRowStart[] = {0, 3, 6};
constexpr int kRowCount[] = {3, 3, 3};
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
// Wasp yellow: a sharp sulphur, up at hue 56 degrees, where ChirpParade's finch
// gold sits at 43. The two are the suite's only yellows and they have to be
// told apart at a glance -- ChirpParade's is warm and burnished, a bird in
// morning light; this one is acid, and it is a warning colour because on the
// insects it comes from that is exactly what it is for.
//
// The chassis is chitin: near-black with a green cast, so the greys lean the
// other way from ChirpParade's olive and the yellow stands off them rather than
// blending into them. `highlight` is the pale bloom on a wing, which is what
// the header's flybys are drawn in.
constexpr Theme kTheme = {
   /* bgTop     */ {0.055, 0.067, 0.043},
   /* bgBottom  */ {0.031, 0.039, 0.024},
   /* panelFill */ {0.078, 0.094, 0.059},
   /* panelEdge */ {0.129, 0.153, 0.086},
   /* knobFace  */ {0.043, 0.051, 0.033},
   /* track     */ {0.169, 0.192, 0.098},
   /* accent    */ {0.902, 0.851, 0.231},
   /* text      */ {0.945, 0.953, 0.898},
   /* textDim   */ {0.573, 0.604, 0.478},
   /* textMute  */ {0.384, 0.412, 0.318},
   /* highlight */ {1.000, 0.992, 0.855},
};

// -------------------------------------------------------------------- mixer
//
// The four layers, in the order they stack: the swarm, the passes that cross
// in front of it, the stridulation that is a second instrument sharing the
// window, and the field underneath all of it. Each fader is the level knob
// that already sits on that layer's panel, so the mixer adds no parameters of
// its own.
//
// Only the flyby carries a placement, and its pan is `Sweep` -- how far across
// the field a pass travels rather than where it sits, because a pass does not
// sit anywhere. Width belongs to no single strip: it spreads the swarm and the
// chorus at once.
constexpr MixerStrip kMixerStrips[] = {
   /* label        level              pan       width     master */
   {"SWARM", kParamSwarmLevel, kNoParam, kParamWidth, false},
   {"FLYBY", kParamFlybyLevel, kParamFlybySweep, kNoParam, false},
   {"STRIDUL", kParamStridLevel, kNoParam, kNoParam, false},
   {"BED", kParamBedLevel, kNoParam, kNoParam, false},
   {"OUTPUT", kParamGain, kNoParam, kNoParam, true},
};

constexpr int kNumStrips = static_cast<int>(sizeof(kMixerStrips) / sizeof(kMixerStrips[0]));

// ----------------------------------------------------------------- ornament

// The swarm across the header: a cloud of individuals milling about, and a
// streak drawn right across whenever one of them passes the listener.
//
// The cloud is the plugin's activity. Each mote drifts on two slow circles of
// its own, so the cloud churns without any of them repeating against another,
// and how many of them are drawn follows voiceLoad -- a hive preset swarms,
// a single bee does not.
//
// The streaks are the plugin's own flybys: one per pass, with its height, its
// direction and its speed derived from the flyby number, so the same pass
// always draws the same streak and a fixed Seed gives a repeatable picture as
// well as a repeatable sound.
//
// It animates always. Insects that have stopped moving are not quiet insects,
// they are a broken graph -- the same reasoning that keeps a few strokes on
// ChirpParade's sonogram when nothing is playing.
class Swarm final : public HeaderOrnament {
public:
   bool animating(uint32_t) const override { return true; }

   void draw(cairo_t *cr, const OrnamentContext &ctx) override {
      mPhase += 0.008;

      // New passes since the last frame become streaks. Capped: a preset at
      // five flybys a second would ask for five streaks a frame and none of
      // them would be legible anyway.
      if (ctx.eventCounter != mLastCounter) {
         const uint32_t fresh = ctx.eventCounter - mLastCounter;
         const uint32_t n = fresh > 2u ? 2u : fresh;
         for (uint32_t k = 0; k < n; ++k)
            addStreak(mLastCounter + fresh - n + k + 1u, ctx);
         mLastCounter = ctx.eventCounter;
      }

      // ------------------------------------------------------------- cloud
      //
      // Thickest in the middle of the header and thinning towards the edges,
      // so the wordmark sits in a swarm rather than on a field of dots.
      const int shown = 14 + static_cast<int>(46.0 * ctx.voiceLoad);
      for (int i = 0; i < shown; ++i) {
         // Two incommensurate circles per mote: a slow wide drift and a fast
         // narrow jitter, which is what a hovering insect actually does.
         const double a = 0.37 + 0.11 * i;
         const double b = 1.93 + 0.23 * i;
         const double cx = mCentre[i % kSeeds] * ctx.windowW;
         const double cy = ctx.headerH * (0.18 + 0.64 * mHeight[i % kSeeds]);
         const double wide = 14.0 + 22.0 * mSpread[i % kSeeds];
         const double x = cx + wide * std::sin(mPhase * a + i) + 3.0 * std::sin(mPhase * b);
         const double y = cy + wide * 0.35 * std::cos(mPhase * a * 1.31 + i * 2.1) +
                          2.0 * std::cos(mPhase * b * 1.17);
         // Motes near the edges fade out rather than being clipped off.
         const double u = x / ctx.windowW;
         const double edge = u < 0.0 || u > 1.0 ? 0.0 : std::sin(M_PI * u);
         const double a0 = 0.10 + 0.22 * ctx.voiceLoad;
         setColor(cr, ctx.theme->accent, a0 * edge * (0.45 + 0.55 * mSpread[i % kSeeds]));
         cairo_new_path(cr);
         cairo_arc(cr, x, y, 0.9 + 0.9 * mSpread[i % kSeeds], 0.0, 2.0 * M_PI);
         cairo_fill(cr);
      }

      // ----------------------------------------------------------- streaks
      for (auto &s : mStreaks) {
         if (s.life <= 0.0)
            continue;
         s.life -= s.fade;
         s.x += s.vx;
         const double y = s.y * ctx.headerH;
         const double len = 26.0 + 34.0 * s.len;
         const double tail = s.vx > 0.0 ? -len : len;
         cairo_new_path(cr);
         cairo_move_to(cr, s.x, y);
         cairo_line_to(cr, s.x + tail, y + s.drop * len * 0.18);
         cairo_set_line_width(cr, 1.0 + 0.8 * s.len);
         setColor(cr, ctx.theme->highlight, s.life * s.life * 0.55);
         cairo_stroke(cr);
         // The insect itself, at the head of its own streak.
         cairo_new_path(cr);
         cairo_arc(cr, s.x, y, 1.3 + 1.0 * s.len, 0.0, 2.0 * M_PI);
         setColor(cr, ctx.theme->accent, s.life * 0.8);
         cairo_fill(cr);
         if (s.x < -80.0 || s.x > ctx.windowW + 80.0)
            s.life = 0.0;
      }
   }

private:
   struct Streak {
      double x = 0.0, y = 0.0, vx = 0.0, len = 0.0, drop = 0.0, life = 0.0, fade = 0.0;
   };

   // Deterministic in the flyby number: the same pass always draws the same
   // streak, so a pinned Seed gives a repeatable picture too.
   void addStreak(uint32_t n, const OrnamentContext &ctx) {
      uint32_t h = n * 2654435761u;
      h ^= h >> 15;
      h *= 0x85EBCA6Bu;
      h ^= h >> 13;
      const double fx = static_cast<double>(h & 0xFFu) / 255.0;
      const double fy = static_cast<double>((h >> 8) & 0xFFu) / 255.0;
      const double fz = static_cast<double>((h >> 16) & 0xFFu) / 255.0;
      const bool toRight = (h >> 24) & 1u;
      Streak &s = mStreaks[mNext];
      mNext = (mNext + 1) % kMaxStreaks;
      s.x = toRight ? -40.0 : ctx.windowW + 40.0;
      s.vx = (toRight ? 1.0 : -1.0) * (3.2 + 5.5 * fx);
      s.y = 0.16 + 0.68 * fy;
      s.len = fz;
      s.drop = fy - 0.5;
      s.life = 1.0;
      s.fade = 0.004 + 0.004 * fz;
   }

   static constexpr int kMaxStreaks = 8;
   static constexpr int kSeeds = 17;
   // Fixed scatter for the cloud: a prime number of positions, so the motes do
   // not fall into columns as more of them are drawn.
   static constexpr double mCentre[kSeeds] = {0.06, 0.53, 0.19, 0.78, 0.35, 0.91, 0.11,
                                              0.64, 0.27, 0.83, 0.44, 0.02, 0.71, 0.15,
                                              0.58, 0.97, 0.39};
   static constexpr double mHeight[kSeeds] = {0.32, 0.71, 0.14, 0.88, 0.47, 0.25, 0.63,
                                              0.06, 0.79, 0.41, 0.95, 0.19, 0.55, 0.84,
                                              0.11, 0.68, 0.37};
   static constexpr double mSpread[kSeeds] = {0.42, 0.87, 0.21, 0.64, 0.09, 0.95, 0.53,
                                              0.31, 0.76, 0.17, 0.60, 0.88, 0.05, 0.49,
                                              0.72, 0.26, 0.93};
   Streak mStreaks[kMaxStreaks];
   int mNext = 0;
   uint32_t mLastCounter = 0;
   double mPhase = 0.0;
};

Swarm gSwarm;

const WindowSpec kSpec = {
   /* wordmarkFirst  */ "Insect",
   /* wordmarkSecond */ "Swarm",
   /* subtitle       */ "SYNTHETIC INSECT INSTRUMENT",
   /* version        */ kPluginVersion,
   /* voiceNoun      */ "insects",
   /* eventNoun      */ "flybys",
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
   /* ornament       */ &gSwarm,
};

} // namespace

Gui *createGui(GuiDelegate &delegate) {
   static WindowSpec spec = kSpec;
   spec.params = paramTable();
   return createWindow(delegate, spec);
}

} // namespace insectswarm
