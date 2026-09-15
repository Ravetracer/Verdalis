// WhooshPact's window: its panels, its colours and its streaks.
//
// The window itself -- the layout engine, the widgets, the preset browser, the
// typed value entry -- is shared by the suite and lives in
// shared/src/gui/window.cpp. What is here is only what makes this window
// WhooshPact's.

#include <cmath>

#include "verdalis/gui/window.h"

#include "params.h"
#include "whooshpact.h"

namespace whooshpact {

namespace {

// ------------------------------------------------------------------- geometry
//
// Cell and panel sizes are the suite's (verdalis/gui/window.h); a knob is the
// same size in every plugin. Only the overall width, which follows from the
// busiest row, is ours: the gesture, the noise layer and the two pitched
// things need four, four, three and three cells across two rows, and that
// fixes the window.
constexpr int kContentW = 1264;
constexpr int kWindowH = 740;

// ------------------------------------------------------------------- panels

// The gesture first, because everything else is driven by where it has got to;
// then the four layers in the order they stack; then what is done to the sum.
constexpr uint32_t kGestureParams[] = {
   kParamType, kParamBlend, kParamSpan, kParamPeak,
   kParamHold, kParamRise,  kParamFall, kParamVariation,
};
constexpr uint32_t kAirParams[] = {
   kParamNoise,   kParamAirLevel, kParamAirCutoff, kParamAirSweep,
   kParamAirReso, kParamAirCurve, kParamAirTilt,   kParamAirWidth,
};
constexpr uint32_t kToneParams[] = {kParamWave,      kParamToneLevel,  kParamTonePitch,
                                    kParamToneGlide, kParamToneDetune, kParamToneWidth};
constexpr uint32_t kMotionParams[] = {kParamPanStart,   kParamPanEnd,      kParamSpaceAmount,
                                      kParamSpaceSize,  kParamSpaceDamping, kParamSpaceWidth};
constexpr uint32_t kSubParams[] = {kParamSubLevel, kParamSubPitch, kParamSubDrop,
                                   kParamSubDecay, kParamSubDrive, kParamSubClick};
constexpr uint32_t kHitParams[] = {kParamHitLevel, kParamHitTone, kParamHitDecay,
                                   kParamHitNoise, kParamHitBody, kParamHitTime};
constexpr uint32_t kFlutterParams[] = {kParamFlutterDepth,  kParamFlutterStart,
                                       kParamFlutterEnd,    kParamFlutterShape,
                                       kParamFlutterTarget, kParamFlutterSmooth};
constexpr uint32_t kEqParams[] = {
   kParamHighpass,  kParamLowpass,   kParamEqLowFreq,  kParamEqLowGain,
   kParamEqMidFreq, kParamEqMidGain, kParamEqHighFreq, kParamEqHighGain,
};
constexpr uint32_t kEnvParams[] = {kParamAttack,  kParamDecay,      kParamSustain,
                                   kParamRelease, kParamVelToLevel, kParamVelToTone};
constexpr uint32_t kOutParams[] = {kParamGain, kParamDrive, kParamMaxVoices, kParamSeed};

#define PANEL(title, cols, rows, arr)                                                              \
   { title, cols, rows, arr, static_cast<int>(sizeof(arr) / sizeof(arr[0])) }

constexpr PanelSpec kPanelSpecs[] = {
   PANEL("GESTURE", 4, 2, kGestureParams), PANEL("AIR", 4, 2, kAirParams),
   PANEL("TONE", 3, 2, kToneParams),       PANEL("MOTION", 3, 2, kMotionParams),
   PANEL("SUB", 3, 2, kSubParams),         PANEL("HIT", 3, 2, kHitParams),
   PANEL("FLUTTER", 3, 2, kFlutterParams), PANEL("EQ", 4, 2, kEqParams),
   PANEL("ENVELOPE", 6, 1, kEnvParams),    PANEL("OUTPUT", 4, 1, kOutParams),
};
#undef PANEL

constexpr int kNumPanels = static_cast<int>(sizeof(kPanelSpecs) / sizeof(kPanelSpecs[0]));

// Which panels share a row, in order. The activity meter fills what is left of
// the last row.
constexpr int kRowStart[] = {0, 4, 8};
constexpr int kRowCount[] = {4, 4, 2};
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
// Impact magenta. Every other plugin in the suite takes its accent from the
// thing it models -- rain blue, ember orange, river green -- and this one has
// no thing: it models production sounds rather than the world. So the colour
// is the one the others cannot be, a saturated magenta at hue 330, well clear
// of ThunderClap's violet on one side and CrackleBlaze's ember on the other,
// and obviously synthetic rather than obviously natural.
//
// The chassis is the same idea: near-black with a cool blue-violet cast, which
// is the one direction no other plugin's greys are tinted. `highlight` is a
// bright cyan -- the complement of the accent -- and it is what the impact
// flash in the header is drawn in, because a flash the same colour as
// everything else is not a flash.
constexpr Theme kTheme = {
   /* bgTop     */ {0.078, 0.063, 0.098},
   /* bgBottom  */ {0.047, 0.039, 0.067},
   /* panelFill */ {0.110, 0.090, 0.137},
   /* panelEdge */ {0.176, 0.141, 0.216},
   /* knobFace  */ {0.063, 0.051, 0.082},
   /* track     */ {0.204, 0.161, 0.247},
   /* accent    */ {1.000, 0.235, 0.592},
   /* text      */ {0.949, 0.925, 0.957},
   /* textDim   */ {0.588, 0.518, 0.612},
   /* textMute  */ {0.420, 0.365, 0.451},
   /* highlight */ {0.549, 0.937, 1.000},
};

// -------------------------------------------------------------------- mixer
//
// The four layers a gesture is balanced from, in the order they stack: the
// noise that is the whoosh, the pitched stack over it, and the two struck
// layers underneath -- the sub that carries a boom and the transient that
// makes an impact.
//
// Each fader is the level knob that already sits on that layer's panel, so the
// mixer adds no parameters of its own. Only Air and Tone carry a width, and
// neither carries a pan: where the gesture sits is Pan Start and Pan End on the
// Motion panel, and that is a property of the whole gesture rather than of one
// layer -- a whoosh whose noise crossed the field while its sub stayed put
// would not read as one sound moving.
constexpr MixerStrip kMixerStrips[] = {
   /* label     level              pan       width             master */
   {"AIR", kParamAirLevel, kNoParam, kParamAirWidth, false},
   {"TONE", kParamToneLevel, kNoParam, kParamToneWidth, false},
   {"SUB", kParamSubLevel, kNoParam, kNoParam, false},
   {"HIT", kParamHitLevel, kNoParam, kNoParam, false},
   {"OUTPUT", kParamGain, kNoParam, kNoParam, true},
};

constexpr int kNumStrips = static_cast<int>(sizeof(kMixerStrips) / sizeof(kMixerStrips[0]));

// ----------------------------------------------------------------- ornament

// One streak per gesture, crossing the header and breaking at the far end.
//
// The plugin makes sounds that travel, and that is what the header draws: every
// note fires a comet that crosses the window, thinning as it goes, and lands in
// a flash of the highlight colour. Its lane, its speed, its direction and its
// thickness all come from the gesture number, so the same gesture always draws
// the same streak and a pinned Seed gives a repeatable picture as well as a
// repeatable sound.
//
// When nothing is playing a row of dim lane marks stays on screen. A header
// that goes completely empty reads as a broken graph rather than a quiet one --
// the same reasoning that keeps a few strokes on ChirpParade's sonogram.
class Streaks final : public HeaderOrnament {
public:
   bool animating(uint32_t eventCounter) const override {
      if (eventCounter != mLastCounter)
         return true;
      for (const auto &s : mStreaks)
         if (s.life > 0.0)
            return true;
      return false;
   }

   void draw(cairo_t *cr, const OrnamentContext &ctx) override {
      if (ctx.eventCounter != mLastCounter) {
         // A run of notes faster than the frame rate would ask for more streaks
         // than are visible anyway, so only the last few are drawn.
         const uint32_t fresh = ctx.eventCounter - mLastCounter;
         const uint32_t n = fresh > 3u ? 3u : fresh;
         for (uint32_t k = 0; k < n; ++k)
            addStreak(mLastCounter + fresh - n + k + 1u, ctx);
         mLastCounter = ctx.eventCounter;
      }

      // ------------------------------------------------------- lane marks
      const int kLanes = 5;
      for (int l = 0; l < kLanes; ++l) {
         const double y = ctx.headerH * (0.22 + 0.56 * (l + 0.5) / kLanes);
         cairo_new_path(cr);
         cairo_move_to(cr, 0.0, y);
         cairo_line_to(cr, ctx.windowW, y);
         setColor(cr, ctx.theme->accent, 0.13 + 0.12 * ctx.voiceLoad);
         cairo_set_line_width(cr, 1.0);
         cairo_stroke(cr);
      }

      // ---------------------------------------------------------- streaks
      for (auto &s : mStreaks) {
         if (s.life <= 0.0)
            continue;
         s.life -= s.fade;
         s.x += s.speed;

         const double head = s.x;
         const double tail = s.x - s.length * s.dir;
         const double y = s.y * ctx.headerH;
         const double w = s.width * (0.35 + 0.65 * s.life);

         // The comet: a filled path that tapers from the head back to a point,
         // which is what a swept sound looks like on a spectrogram.
         cairo_new_path(cr);
         cairo_move_to(cr, head, y - w);
         cairo_line_to(cr, head, y + w);
         cairo_line_to(cr, tail, y);
         cairo_close_path(cr);
         setColor(cr, ctx.theme->accent, 0.10 + 0.45 * s.life * s.life);
         cairo_fill(cr);

         // The landing flash, once the head has crossed the window.
         const bool landed = s.dir > 0.0 ? head > ctx.windowW * 0.92
                                         : head < ctx.windowW * 0.08;
         if (landed && s.flash > 0.0) {
            s.flash -= 0.06;
            const double r = (1.0 - s.flash) * ctx.headerH * 0.55;
            cairo_new_path(cr);
            cairo_arc(cr, s.dir > 0.0 ? ctx.windowW : 0.0, y, r > 0.5 ? r : 0.5, 0.0, 2.0 * M_PI);
            setColor(cr, ctx.theme->highlight, 0.22 * s.flash * s.flash);
            cairo_fill(cr);
         }
      }
   }

private:
   struct Streak {
      double x = 0.0, y = 0.0, speed = 0.0, length = 0.0, width = 0.0;
      double dir = 1.0, life = 0.0, fade = 0.0, flash = 1.0;
   };

   // Deterministic in the gesture number, so a pinned Seed draws the same
   // picture every time it plays the same sound.
   void addStreak(uint32_t n, const OrnamentContext &ctx) {
      uint32_t h = n * 2654435761u;
      h ^= h >> 15;
      h *= 0x85EBCA6Bu;
      h ^= h >> 13;
      const double fa = static_cast<double>(h & 0xFFu) / 255.0;
      const double fb = static_cast<double>((h >> 8) & 0xFFu) / 255.0;
      const double fc = static_cast<double>((h >> 16) & 0xFFu) / 255.0;
      const double fd = static_cast<double>((h >> 24) & 0xFFu) / 255.0;

      Streak &s = mStreaks[mNext];
      mNext = (mNext + 1) % kMaxStreaks;
      s.dir = fd < 0.5 ? 1.0 : -1.0;
      s.x = s.dir > 0.0 ? -ctx.windowW * 0.15 : ctx.windowW * 1.15;
      s.y = 0.24 + 0.52 * fa;
      s.speed = s.dir * ctx.windowW * (0.008 + 0.016 * fb);
      s.length = ctx.windowW * (0.18 + 0.30 * fc);
      s.width = ctx.headerH * (0.045 + 0.075 * fb);
      s.life = 1.0;
      s.fade = 0.010 + 0.012 * fc;
      s.flash = 1.0;
   }

   static constexpr int kMaxStreaks = 12;
   Streak mStreaks[kMaxStreaks];
   int mNext = 0;
   uint32_t mLastCounter = 0;
};

Streaks gStreaks;

const WindowSpec kSpec = {
   /* wordmarkFirst  */ "Whoosh",
   /* wordmarkSecond */ "Pact",
   /* subtitle       */ "SYNTHETIC TRANSITION INSTRUMENT",
   /* version        */ kPluginVersion,
   /* voiceNoun      */ "gestures",
   /* eventNoun      */ "triggers",
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

} // namespace whooshpact
