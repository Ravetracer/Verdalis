// CrackleBlaze's window: its panels, its colours and its fire.
//
// The window itself -- the layout engine, the widgets, the preset browser, the
// typed value entry -- is shared by the suite and lives in
// shared/src/gui/window.cpp. What is here is only what makes this window
// CrackleBlaze's.

#include <cmath>

#include "verdalis/gui/window.h"

#include "params.h"
#include "crackleblaze.h"

namespace crackleblaze {

namespace {

// ------------------------------------------------------------------- geometry
//
// Cell and panel sizes are the suite's (verdalis/gui/window.h); a knob is the
// same size in every plugin. Only the overall width, which follows from the
// busiest row, is ours: the roar and the two event layers that sit on it need
// four, four and three cells across two rows, and that fixes the window.
constexpr int kContentW = 988;
constexpr int kWindowH = 740;

// ------------------------------------------------------------------- panels

// The fire first, then the things that happen in it, then where the listener
// is standing.
constexpr uint32_t kBlazeParams[] = {
   kParamFireType, kParamFireBlend, kParamRoarLevel, kParamRoarTilt,
   kParamRoarBody, kParamFlare,     kParamFlareRate, kParamDraught,
};
constexpr uint32_t kCrackleParams[] = {
   kParamCrackleRate,   kParamCrackleLevel, kParamCrackleDecay, kParamCrackleTone,
   kParamCrackleSpread, kParamBurst,        kParamSnap,         kParamCrackleBody,
};
constexpr uint32_t kSizzleParams[] = {kParamSap, kParamSizzleLevel, kParamSizzleDecay,
                                      kParamSizzleTone, kParamSteam};
constexpr uint32_t kSettleParams[] = {kParamSettleRate, kParamSettleLevel, kParamSettleTone,
                                      kParamSettleDecay};
constexpr uint32_t kHearthParams[] = {
   kParamHearthType, kParamDistance,     kParamAir,       kParamWidth,
   kParamRoarWidth,  kParamSpaceAmount,  kParamSpaceSize, kParamSpaceDamping,
};
constexpr uint32_t kFilterParams[] = {kParamFilterType, kParamHighpass, kParamFilterCutoff,
                                      kParamFilterReso, kParamFilterKeyTrack};
constexpr uint32_t kEnvParams[] = {kParamAttack,  kParamDecay,      kParamSustain,
                                   kParamRelease, kParamVelToLevel, kParamVelToFire};
constexpr uint32_t kOutParams[] = {kParamGain, kParamMaxEvents, kParamSeed};

#define PANEL(title, cols, rows, arr)                                                              \
   { title, cols, rows, arr, static_cast<int>(sizeof(arr) / sizeof(arr[0])) }

constexpr PanelSpec kPanelSpecs[] = {
   PANEL("BLAZE", 4, 2, kBlazeParams),   PANEL("CRACKLE", 4, 2, kCrackleParams),
   PANEL("SIZZLE", 3, 2, kSizzleParams), PANEL("SETTLE", 2, 2, kSettleParams),
   PANEL("HEARTH", 4, 2, kHearthParams), PANEL("FILTER", 3, 2, kFilterParams),
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
// Ember orange: the colour of burning wood rather than of flame, which is why
// it sits on the red side of SkyHowl's dust coral rather than beside it. The
// two are the suite's only warm accents and they have to be told apart at a
// glance -- SkyHowl's is desaturated and dusty, this one is saturated and hot.
//
// The chassis is a hearth at night: near-black with a deep red-brown cast, the
// darkest in the suite, so that the accent reads as something glowing in it.
// `highlight` is the pale gold of a flame's hottest part, which is what the
// embers in the header are drawn in.
constexpr Theme kTheme = {
   /* bgTop     */ {0.110, 0.063, 0.047},
   /* bgBottom  */ {0.070, 0.039, 0.031},
   /* panelFill */ {0.145, 0.082, 0.059},
   /* panelEdge */ {0.216, 0.126, 0.090},
   /* knobFace  */ {0.082, 0.047, 0.035},
   /* track     */ {0.239, 0.141, 0.098},
   /* accent    */ {1.000, 0.353, 0.173},
   /* text      */ {0.961, 0.918, 0.898},
   /* textDim   */ {0.596, 0.494, 0.451},
   /* textMute  */ {0.439, 0.353, 0.318},
   /* highlight */ {1.000, 0.847, 0.616},
};

// -------------------------------------------------------------------- mixer
//
// The four layers a fire is balanced from, in the order they stack: the roar
// underneath everything, then the two populations of events that sit on it --
// the crackles standing a measured 13.2 dB above the roar and the sizzles
// under them -- and the settles, which are rare enough to be easy to lose and
// easy to overdo. Each fader is the level knob that already sits on that
// layer's panel, so the mixer adds no parameters of its own.
//
// Only the roar carries a placement: Roar Width is its own, where Width is the
// stereo spread of every event population at once and belongs to no single
// strip. Sap is not a strip either -- it moves events from one population to
// the other rather than changing how loud either is.
constexpr MixerStrip kMixerStrips[] = {
   /* label      level                pan       width              master */
   {"ROAR", kParamRoarLevel, kNoParam, kParamRoarWidth, false},
   {"CRACKLE", kParamCrackleLevel, kNoParam, kNoParam, false},
   {"SIZZLE", kParamSizzleLevel, kNoParam, kNoParam, false},
   {"SETTLE", kParamSettleLevel, kNoParam, kNoParam, false},
   {"OUTPUT", kParamGain, kNoParam, kNoParam, true},
};

constexpr int kNumStrips = static_cast<int>(sizeof(kMixerStrips) / sizeof(kMixerStrips[0]));

// ----------------------------------------------------------------- ornament

// The fire across the header: tongues of flame along the bottom, with an ember
// thrown up wherever a crackle happens.
//
// The tongues are drawn as filled paths that taper to a point, each with its
// own sway and its own period so they never line up -- a row of flames beating
// in step reads as a graphic, not as a fire. Their height follows voiceLoad, so
// a busy preset burns taller.
//
// The embers are the plugin's own events: one per crackle, with its path and
// its lifetime derived from the crackle number, so the same crackle always
// throws the same ember and a fixed Seed gives a repeatable picture as well as
// a repeatable sound.
//
// It animates always. A fire with still flames is not a quiet fire, it is a
// broken graph -- the same reasoning that keeps a few strokes on ChirpParade's
// sonogram when nothing is playing.
class Blaze final : public HeaderOrnament {
public:
   bool animating(uint32_t) const override { return true; }

   void draw(cairo_t *cr, const OrnamentContext &ctx) override {
      mPhase += 0.011;

      // New crackles since the last frame become embers. Capped, because a
      // preset at fifty crackles a second would otherwise ask for fifty embers
      // a frame and none of them would be visible anyway.
      if (ctx.eventCounter != mLastCounter) {
         const uint32_t fresh = ctx.eventCounter - mLastCounter;
         const uint32_t n = fresh > 3u ? 3u : fresh;
         for (uint32_t k = 0; k < n; ++k)
            addEmber(mLastCounter + fresh - n + k + 1u, ctx);
         mLastCounter = ctx.eventCounter;
      }

      // ------------------------------------------------------------ flames
      const int kTongues = 11;
      const double base = ctx.headerH * 1.04; // rooted just below the header
      for (int t = 0; t < kTongues; ++t) {
         const double u = (t + 0.5) / kTongues;
         const double x0 = u * ctx.windowW;
         // Tallest in the middle of the fire, shorter at its edges, and taller
         // again the busier the plugin is.
         const double bell = 0.35 + 0.65 * std::sin(M_PI * u);
         const double h = ctx.headerH * bell * (0.62 + 0.55 * ctx.voiceLoad) *
                          (0.74 + 0.26 * std::sin(mPhase * (1.7 + 0.41 * t) + t));
         const double w = ctx.windowW / kTongues * 0.46;
         // The tip leans, and each tongue leans on its own clock.
         const double lean = w * 0.55 * std::sin(mPhase * (1.1 + 0.27 * t) + t * 2.1);

         cairo_new_path(cr);
         cairo_move_to(cr, x0 - w * 0.5, base);
         cairo_curve_to(cr, x0 - w * 0.45, base - h * 0.45, x0 - w * 0.22 + lean * 0.5,
                        base - h * 0.75, x0 + lean, base - h);
         cairo_curve_to(cr, x0 + w * 0.22 + lean * 0.5, base - h * 0.75, x0 + w * 0.45,
                        base - h * 0.45, x0 + w * 0.5, base);
         cairo_close_path(cr);
         setColor(cr, ctx.theme->accent, 0.08 + 0.13 * bell + 0.07 * ctx.voiceLoad);
         cairo_fill(cr);

         // The hotter core, a third of the width and two thirds of the height.
         cairo_new_path(cr);
         cairo_move_to(cr, x0 - w * 0.17, base);
         cairo_curve_to(cr, x0 - w * 0.15, base - h * 0.32, x0 - w * 0.07 + lean * 0.35,
                        base - h * 0.52, x0 + lean * 0.7, base - h * 0.66);
         cairo_curve_to(cr, x0 + w * 0.07 + lean * 0.35, base - h * 0.52, x0 + w * 0.15,
                        base - h * 0.32, x0 + w * 0.17, base);
         cairo_close_path(cr);
         setColor(cr, ctx.theme->highlight, 0.04 + 0.06 * bell);
         cairo_fill(cr);
      }

      // ------------------------------------------------------------ embers
      for (auto &e : mEmbers) {
         if (e.life <= 0.0)
            continue;
         e.life -= e.fade;
         e.y -= e.rise;
         e.x += e.drift;
         // Embers cool as they climb: from the pale core colour towards the
         // accent, and out.
         const double a = e.life * e.life * 0.85;
         const double hot = e.life;
         const Rgb &c = ctx.theme->accent;
         const Rgb &hi = ctx.theme->highlight;
         const Rgb mix = {c.r + (hi.r - c.r) * hot, c.g + (hi.g - c.g) * hot,
                          c.b + (hi.b - c.b) * hot};
         cairo_new_path(cr);
         cairo_arc(cr, e.x, e.y, e.size, 0.0, 2.0 * M_PI);
         setColor(cr, mix, a);
         cairo_fill(cr);
      }
   }

private:
   struct Ember {
      double x = 0.0, y = 0.0, rise = 0.0, drift = 0.0, size = 0.0, life = 0.0, fade = 0.0;
   };

   // Deterministic in the crackle number: the same crackle always throws the
   // same ember, so a pinned Seed gives a repeatable picture too.
   void addEmber(uint32_t n, const OrnamentContext &ctx) {
      uint32_t h = n * 2654435761u;
      h ^= h >> 15;
      h *= 0x85EBCA6Bu;
      h ^= h >> 13;
      const double fx = static_cast<double>(h & 0xFFFFu) / 65535.0;
      const double fy = static_cast<double>((h >> 16) & 0xFFu) / 255.0;
      const double fz = static_cast<double>((h >> 24) & 0xFFu) / 255.0;
      Ember &e = mEmbers[mNext];
      mNext = (mNext + 1) % kMaxEmbers;
      e.x = 0.04 * ctx.windowW + fx * 0.92 * ctx.windowW;
      e.y = ctx.headerH * (0.72 + 0.22 * fy);
      e.rise = 0.35 + 0.75 * fy;
      e.drift = (fz - 0.5) * 0.5;
      e.size = 0.8 + 1.1 * fz;
      e.life = 1.0;
      e.fade = 0.013 + 0.014 * fz;
   }

   static constexpr int kMaxEmbers = 24;
   Ember mEmbers[kMaxEmbers];
   int mNext = 0;
   uint32_t mLastCounter = 0;
   double mPhase = 0.0;
};

const WindowSpec kSpec = {
   /* wordmarkFirst  */ "Crackle",
   /* wordmarkSecond */ "Blaze",
   /* subtitle       */ "SYNTHETIC FIRE INSTRUMENT",
   /* version        */ kPluginVersion,
   /* voiceNoun      */ "embers",
   /* eventNoun      */ "crackles",
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
   spec.ornament = new Blaze();
   return createWindow(delegate, spec);
}

} // namespace crackleblaze
