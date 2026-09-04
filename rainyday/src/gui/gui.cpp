// RainyDay's plugin window.
//
// Raw X11 for the window, Cairo for everything drawn inside it. No toolkit, so
// the plugin stays one .clap file and pulls in nothing a Linux audio machine
// does not already have.
//
// The whole layout is generated from the parameter table in params.cpp: the
// panels are the modules, the cells are the parameters, and the help line is
// the parameter's own tip. Adding a parameter there puts it on screen here.

#include "gui/gui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <cairo/cairo-xlib.h>
#include <cairo/cairo.h>

#include "params.h"

namespace rainyday {

namespace {

// ------------------------------------------------------------------- geometry
//
// All sizes are in design pixels; a display scale is applied by cairo when
// painting, so nothing below has to know about it.

constexpr int kCellW = 84;
constexpr int kCellH = 96;
constexpr int kPanelPad = 8;
constexpr int kPanelTitleH = 24;
constexpr int kMargin = 16;
constexpr int kGap = 8;
constexpr int kContentW = 880; // 10 cells plus the panel padding of each row
constexpr int kHeaderH = 66;
constexpr int kBarH = 32;
constexpr int kHelpH = 24;

constexpr int kWindowW = kContentW + 2 * kMargin;
constexpr int kWindowH = 648;
constexpr double kMenuRowH = 20.0;
constexpr double kMenuPad = 4.0;

constexpr double kKnobR = 21.0;
constexpr double kArcStart = 0.75 * M_PI;
constexpr double kArcSweep = 1.5 * M_PI;

// A cell either holds a knob or, for the two enum parameters, a chip that is
// clicked on its left or right half to step through the choices.
struct Cell {
   uint32_t param;
   int col;
   int row;
   int span;
};

struct PanelSpec {
   const char *title;
   int cols;
   int rows;
   const uint32_t *params;
   int count;
};

// Seven per line, which is one row of the panel.
const uint32_t kRainParams[] = {
   kParamSurface,  kParamDensity, kParamClumping, kParamDropPitch,   kParamPitchSpread,
   kParamDropDecay, kParamDecaySpread,
   kParamTonality, kParamBubble,  kParamImpact,   kParamSplash,      kParamLevelSpread,
   kParamChirp,     kParamNoteTracking,
};
const uint32_t kSpaceParams[] = {kParamWidth,       kParamDistance,  kParamAir,
                                 kParamSpaceAmount, kParamSpaceSize, kParamSpaceDamping};
const uint32_t kEnvParams[] = {kParamAttack,     kParamDecay,       kParamSustain,
                               kParamRelease,    kParamVelToLevel,  kParamVelToDensity};
const uint32_t kFilterParams[] = {kParamFilterType, kParamFilterCutoff, kParamFilterReso,
                                  kParamFilterKeyTrack};
const uint32_t kBedParams[] = {kParamBedLevel, kParamBedTone, kParamBedBody, kParamBedDrift};
const uint32_t kOutParams[] = {kParamGain, kParamMaxDroplets, kParamSeed};

#define PANEL(title, cols, rows, arr)                                                              \
   { title, cols, rows, arr, static_cast<int>(sizeof(arr) / sizeof(arr[0])) }

const PanelSpec kPanelSpecs[] = {
   PANEL("RAIN", 7, 2, kRainParams),    PANEL("SPACE", 3, 2, kSpaceParams),
   PANEL("ENVELOPE", 6, 1, kEnvParams), PANEL("FILTER", 4, 1, kFilterParams),
   PANEL("BED", 4, 1, kBedParams),      PANEL("OUTPUT", 3, 1, kOutParams),
};
#undef PANEL

constexpr int kNumPanels = static_cast<int>(sizeof(kPanelSpecs) / sizeof(kPanelSpecs[0]));

// Which panels share a row, in order. The activity meter fills what is left of
// the last row.
const int kRowStart[] = {0, 2, 4};
const int kRowCount[] = {2, 2, 2};
constexpr int kNumRows = 3;

// ---------------------------------------------------------------------- paint

struct Rgb {
   double r, g, b;
};

constexpr Rgb kBgTop = {0.086, 0.106, 0.137};
constexpr Rgb kBgBottom = {0.055, 0.067, 0.086};
constexpr Rgb kPanelFill = {0.118, 0.145, 0.184};
constexpr Rgb kPanelEdge = {0.169, 0.208, 0.259};
constexpr Rgb kKnobFace = {0.063, 0.078, 0.102};
constexpr Rgb kTrack = {0.188, 0.231, 0.286};
constexpr Rgb kAccent = {0.345, 0.714, 0.910};
constexpr Rgb kText = {0.894, 0.918, 0.941};
constexpr Rgb kTextDim = {0.494, 0.541, 0.600};
constexpr Rgb kTextMute = {0.353, 0.400, 0.455};

inline void setColor(cairo_t *cr, const Rgb &c, double a = 1.0) {
   cairo_set_source_rgba(cr, c.r, c.g, c.b, a);
}

void roundedRect(cairo_t *cr, double x, double y, double w, double h, double r) {
   cairo_new_sub_path(cr);
   cairo_arc(cr, x + w - r, y + r, r, -0.5 * M_PI, 0.0);
   cairo_arc(cr, x + w - r, y + h - r, r, 0.0, 0.5 * M_PI);
   cairo_arc(cr, x + r, y + h - r, r, 0.5 * M_PI, M_PI);
   cairo_arc(cr, x + r, y + r, r, M_PI, 1.5 * M_PI);
   cairo_close_path(cr);
}

enum class Align { Left, Center, Right };

// Asking an embedded window for the keyboard focus can fail for reasons that
// are none of the plugin's business. A failure must not reach Xlib's default
// handler, which exits the host.
int ignoreXError(Display *, XErrorEvent *) { return 0; }

// Drawn rather than typed: a glyph like U+25C0 is not in every sans font, and
// a missing-glyph box in the preset bar would look like a bug.
void drawTriangle(cairo_t *cr, double cx, double cy, double size, int dir) {
   const double h = size * 0.5;
   cairo_new_path(cr);
   if (dir < 0) {
      cairo_move_to(cr, cx + h * 0.7, cy - h);
      cairo_line_to(cr, cx + h * 0.7, cy + h);
      cairo_line_to(cr, cx - h * 0.7, cy);
   } else if (dir > 0) {
      cairo_move_to(cr, cx - h * 0.7, cy - h);
      cairo_line_to(cr, cx - h * 0.7, cy + h);
      cairo_line_to(cr, cx + h * 0.7, cy);
   } else {
      cairo_move_to(cr, cx - h, cy - h * 0.7);
      cairo_line_to(cr, cx + h, cy - h * 0.7);
      cairo_line_to(cr, cx, cy + h * 0.7);
   }
   cairo_close_path(cr);
   cairo_fill(cr);
}

void drawText(cairo_t *cr, double x, double baseline, const char *text, double size, bool bold,
              Align align) {
   cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL,
                          bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
   cairo_set_font_size(cr, size);
   cairo_text_extents_t ext;
   cairo_text_extents(cr, text, &ext);
   double tx = x;
   if (align == Align::Center)
      tx = x - (ext.width * 0.5 + ext.x_bearing);
   else if (align == Align::Right)
      tx = x - (ext.width + ext.x_bearing);
   cairo_move_to(cr, tx, baseline);
   cairo_show_text(cr, text);
}

double textWidth(cairo_t *cr, const char *text, double size, bool bold) {
   cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL,
                          bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
   cairo_set_font_size(cr, size);
   cairo_text_extents_t ext;
   cairo_text_extents(cr, text, &ext);
   return ext.width;
}

// Upper-cases into a small buffer for the panel and parameter labels, which are
// drawn in caps regardless of how they are written in the table.
void upperCase(const char *in, char *out, size_t outSize) {
   size_t i = 0;
   for (; in[i] && i + 1 < outSize; ++i)
      out[i] = static_cast<char>(in[i] >= 'a' && in[i] <= 'z' ? in[i] - 32 : in[i]);
   out[i] = 0;
}

// The parameter's position on its knob, 0..1.
double normalised(const ParamDesc &d, double raw) {
   const double span = d.max - d.min;
   if (span <= 0.0)
      return 0.0;
   return std::min(1.0, std::max(0.0, (raw - d.min) / span));
}

bool isChip(const ParamDesc &d) { return d.kind == ParamKind::Enum; }

// Every cell is one column wide. Surface used to take two so that its seven
// names had room, but Filter Type already prints "Bandpass" in a single cell,
// and giving the fourteenth Rain parameter its place is worth more than the
// extra width. The RAIN panel still comes out exactly full.
int cellSpan(uint32_t) { return 1; }
bool isStepped(const ParamDesc &d) {
   return d.kind == ParamKind::Enum || d.kind == ParamKind::Stepped;
}
bool isBipolar(const ParamDesc &d) { return d.min < -0.0001; }

struct Rect {
   double x = 0, y = 0, w = 0, h = 0;
   bool contains(double px, double py) const {
      return px >= x && px < x + w && py >= y && py < y + h;
   }
};

// --------------------------------------------------------------- the window

class X11Gui final : public Gui {
public:
   explicit X11Gui(GuiDelegate &delegate) : mDelegate(delegate) {
      buildLayout();
      for (uint32_t i = 0; i < kNumParams; ++i)
         mShown[i] = -1.0e9;
   }

   ~X11Gui() override { closeWindow(); }

   bool open() override {
      if (mWindow)
         return true;
      XInitThreads();
      mDisplay = XOpenDisplay(nullptr);
      if (!mDisplay)
         return false;

      const int screen = DefaultScreen(mDisplay);
      mVisual = DefaultVisual(mDisplay, screen);

      XSetWindowAttributes attrs;
      std::memset(&attrs, 0, sizeof(attrs));
      attrs.background_pixel = BlackPixel(mDisplay, screen);
      attrs.border_pixel = 0;
      attrs.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                         LeaveWindowMask | KeyPressMask | StructureNotifyMask;

      mWindow = XCreateWindow(mDisplay, RootWindow(mDisplay, screen), 0, 0, pixelW(), pixelH(), 0,
                              DefaultDepth(mDisplay, screen), InputOutput, mVisual,
                              CWBackPixel | CWBorderPixel | CWEventMask, &attrs);
      if (!mWindow) {
         XCloseDisplay(mDisplay);
         mDisplay = nullptr;
         return false;
      }

      // Politely handle the close button when the window is floating.
      mDeleteAtom = XInternAtom(mDisplay, "WM_DELETE_WINDOW", False);
      XSetWMProtocols(mDisplay, mWindow, &mDeleteAtom, 1);

      mTarget = cairo_xlib_surface_create(mDisplay, mWindow, mVisual, pixelW(), pixelH());
      mTargetCr = cairo_create(mTarget);
      allocateBuffer();
      XFlush(mDisplay);
      return true;
   }

   bool embed(unsigned long parentWindow) override {
      if (!mWindow)
         return false;
      XReparentWindow(mDisplay, mWindow, static_cast<Window>(parentWindow), 0, 0);
      XFlush(mDisplay);
      return true;
   }

   bool setTransientFor(unsigned long parentWindow) override {
      if (!mWindow)
         return false;
      XSetTransientForHint(mDisplay, mWindow, static_cast<Window>(parentWindow));
      XFlush(mDisplay);
      return true;
   }

   void setTitle(const char *title) override {
      if (mWindow && title)
         XStoreName(mDisplay, mWindow, title);
   }

   void setScale(double scale) override {
      if (scale < 0.5 || scale > 4.0 || std::fabs(scale - mScale) < 0.001)
         return;
      mScale = scale;
      if (!mWindow)
         return;
      XResizeWindow(mDisplay, mWindow, pixelW(), pixelH());
      cairo_xlib_surface_set_size(mTarget, pixelW(), pixelH());
      allocateBuffer();
      mDirty = true;
   }

   void size(uint32_t *width, uint32_t *height) const override {
      *width = pixelW();
      *height = pixelH();
   }

   void show() override {
      if (!mWindow)
         return;
      XMapWindow(mDisplay, mWindow);
      XFlush(mDisplay);
      mDirty = true;
   }

   void hide() override {
      if (!mWindow)
         return;
      XUnmapWindow(mDisplay, mWindow);
      XFlush(mDisplay);
   }

   void tick() override {
      if (!mDisplay)
         return;
      pumpEvents();
      if (!mWindow)
         return;
      if (needsRepaint())
         paint();
   }

private:
   uint32_t pixelW() const { return static_cast<uint32_t>(kWindowW * mScale + 0.5); }
   uint32_t pixelH() const { return static_cast<uint32_t>(kWindowH * mScale + 0.5); }

   void allocateBuffer() {
      if (mBufferCr) {
         cairo_destroy(mBufferCr);
         mBufferCr = nullptr;
      }
      if (mBuffer) {
         cairo_surface_destroy(mBuffer);
         mBuffer = nullptr;
      }
      mBuffer = cairo_image_surface_create(CAIRO_FORMAT_RGB24, static_cast<int>(pixelW()),
                                           static_cast<int>(pixelH()));
      mBufferCr = cairo_create(mBuffer);
   }

   void closeWindow() {
      if (mBufferCr)
         cairo_destroy(mBufferCr);
      if (mBuffer)
         cairo_surface_destroy(mBuffer);
      if (mTargetCr)
         cairo_destroy(mTargetCr);
      if (mTarget)
         cairo_surface_destroy(mTarget);
      mBufferCr = nullptr;
      mBuffer = nullptr;
      mTargetCr = nullptr;
      mTarget = nullptr;
      if (mDisplay) {
         if (mWindow)
            XDestroyWindow(mDisplay, mWindow);
         XCloseDisplay(mDisplay);
      }
      mWindow = 0;
      mDisplay = nullptr;
   }

   // ------------------------------------------------------------------ layout

   struct Panel {
      const PanelSpec *spec;
      Rect rect;
   };

   void buildLayout() {
      int y = kHeaderH + kGap;
      for (int row = 0; row < kNumRows; ++row) {
         int x = kMargin;
         int rowH = 0;
         for (int i = 0; i < kRowCount[row]; ++i) {
            const PanelSpec &spec = kPanelSpecs[kRowStart[row] + i];
            Panel p;
            p.spec = &spec;
            p.rect.x = x;
            p.rect.y = y;
            p.rect.w = spec.cols * kCellW + 2 * kPanelPad;
            p.rect.h = kPanelTitleH + spec.rows * kCellH + kPanelPad;
            rowH = std::max(rowH, static_cast<int>(p.rect.h));

            for (const Cell &cell : flowCells(spec)) {
               mCells.push_back(cell);
               mCellRects.push_back(cellRect(p, cell));
            }
            mPanels.push_back(p);
            x += static_cast<int>(p.rect.w) + kGap;
         }
         // The activity meter takes whatever is left of the bottom row.
         if (row == kNumRows - 1) {
            mMeter.x = x;
            mMeter.y = y;
            mMeter.w = kMargin + kContentW - x;
            mMeter.h = rowH;
         }
         y += rowH + kGap;
      }

      const int barY = y + 2;
      mPrevRect = {static_cast<double>(kMargin) + 62, static_cast<double>(barY), 26, kBarH};
      mNameRect = {mPrevRect.x + mPrevRect.w + 4, static_cast<double>(barY), 300, kBarH};
      mNextRect = {mNameRect.x + mNameRect.w + 4, static_cast<double>(barY), 26, kBarH};
      mSaveRect = {mNextRect.x + mNextRect.w + 14, static_cast<double>(barY), 58, kBarH};
      mBarY = barY;
      mHelpY = barY + kBarH + 4;
   }

   Rect cellRect(const Panel &p, const Cell &cell) const {
      Rect r;
      r.x = p.rect.x + kPanelPad + cell.col * kCellW;
      r.y = p.rect.y + kPanelTitleH + cell.row * kCellH;
      r.w = kCellW * cell.span;
      r.h = kCellH;
      return r;
   }

   // Lays a panel's parameters out left to right, wrapping to the next row when
   // a cell no longer fits. Kept in one place so drawing and hit testing can
   // never disagree about where a control is.
   static std::vector<Cell> flowCells(const PanelSpec &spec) {
      std::vector<Cell> cells;
      int col = 0, row = 0;
      for (int c = 0; c < spec.count; ++c) {
         const uint32_t id = spec.params[c];
         const int span = cellSpan(id);
         if (col + span > spec.cols) {
            col = 0;
            ++row;
         }
         cells.push_back({id, col, row, span});
         col += span;
         if (col >= spec.cols) {
            col = 0;
            ++row;
         }
      }
      return cells;
   }

   // ------------------------------------------------------------------- paint

   bool needsRepaint() {
      if (mDirty)
         return true;
      for (uint32_t i = 0; i < kNumParams; ++i) {
         const double v = mDelegate.guiParamValue(i);
         if (std::fabs(v - mShown[i]) > 1.0e-9)
            return true;
      }
      const uint32_t drops = mDelegate.guiDropletCount();
      return drops != mLastDropCount || mMeterFill > 0.001 || mDecayFrames > 0;
   }

   void paint() {
      cairo_t *cr = mBufferCr;
      if (!cr)
         return;
      mDirty = false;
      for (uint32_t i = 0; i < kNumParams; ++i)
         mShown[i] = mDelegate.guiParamValue(i);

      cairo_save(cr);
      cairo_scale(cr, mScale, mScale);
      cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

      drawBackground(cr);
      drawHeader(cr);
      for (const Panel &p : mPanels)
         drawPanel(cr, p);
      drawMeter(cr);
      drawPresetBar(cr);
      drawHelpLine(cr);
      if (mBrowserOpen)
         drawBrowser(cr);
      if (mMenuParam >= 0)
         drawMenu(cr);
      if (mSaveOpen)
         drawSaveDialog(cr);

      cairo_restore(cr);

      cairo_set_source_surface(mTargetCr, mBuffer, 0, 0);
      cairo_paint(mTargetCr);
      cairo_surface_flush(mTarget);
      XFlush(mDisplay);
   }

   void drawBackground(cairo_t *cr) {
      cairo_pattern_t *grad = cairo_pattern_create_linear(0, 0, 0, kWindowH);
      cairo_pattern_add_color_stop_rgb(grad, 0.0, kBgTop.r, kBgTop.g, kBgTop.b);
      cairo_pattern_add_color_stop_rgb(grad, 1.0, kBgBottom.r, kBgBottom.g, kBgBottom.b);
      cairo_set_source(cr, grad);
      cairo_rectangle(cr, 0, 0, kWindowW, kWindowH);
      cairo_fill(cr);
      cairo_pattern_destroy(grad);
   }

   // The header carries the wordmark and a few falling streaks whose density
   // follows the engine, so the window shows what it is doing even at a glance.
   void drawHeader(cairo_t *cr) {
      const uint32_t drops = mDelegate.guiDropletCount();
      const uint32_t limit = std::max<uint32_t>(1, mDelegate.guiDropletLimit());
      const double load = std::min(1.0, static_cast<double>(drops) / static_cast<double>(limit));

      cairo_save(cr);
      cairo_rectangle(cr, 0, 0, kWindowW, kHeaderH);
      cairo_clip(cr);
      mStreakPhase += 1.0;
      const int streaks = static_cast<int>(load * 90.0);
      for (int i = 0; i < streaks; ++i) {
         // A cheap deterministic hash keeps each streak in its own lane.
         const double h = std::fmod(std::sin(i * 12.9898) * 43758.5453, 1.0);
         const double x = std::fabs(h) * kWindowW;
         const double speed = 2.0 + std::fabs(std::fmod(h * 7.0, 1.0)) * 4.0;
         const double y = std::fmod(mStreakPhase * speed + std::fabs(h) * 400.0, kHeaderH + 24.0);
         setColor(cr, kAccent, 0.10 + 0.10 * load);
         cairo_set_line_width(cr, 1.0);
         cairo_move_to(cr, x, y - 14.0);
         cairo_line_to(cr, x, y);
         cairo_stroke(cr);
      }
      cairo_restore(cr);

      const double baseline = 46;
      cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
      cairo_set_font_size(cr, 30);
      setColor(cr, kText);
      cairo_move_to(cr, kMargin, baseline);
      cairo_show_text(cr, "Rainy");
      double advance = textWidth(cr, "Rainy", 30, true);
      setColor(cr, kAccent);
      cairo_move_to(cr, kMargin + advance + 2, baseline);
      cairo_show_text(cr, "Day");

      setColor(cr, kTextMute);
      drawText(cr, kMargin + kContentW, 30, "SYNTHETIC RAIN INSTRUMENT", 9, true, Align::Right);
      char ver[64];
      std::snprintf(ver, sizeof(ver), "v%s", "1.0.0");
      drawText(cr, kMargin + kContentW, 44, ver, 9, false, Align::Right);

      setColor(cr, kPanelEdge);
      cairo_set_line_width(cr, 1.0);
      cairo_move_to(cr, kMargin, kHeaderH - 0.5);
      cairo_line_to(cr, kMargin + kContentW, kHeaderH - 0.5);
      cairo_stroke(cr);
   }

   void drawPanel(cairo_t *cr, const Panel &p) {
      setColor(cr, kPanelFill);
      roundedRect(cr, p.rect.x, p.rect.y, p.rect.w, p.rect.h, 5);
      cairo_fill_preserve(cr);
      setColor(cr, kPanelEdge);
      cairo_set_line_width(cr, 1.0);
      cairo_stroke(cr);

      setColor(cr, kAccent, 0.85);
      drawText(cr, p.rect.x + kPanelPad + 2, p.rect.y + 16, p.spec->title, 10, true, Align::Left);

      for (const Cell &cell : flowCells(*p.spec))
         drawCell(cr, cellRect(p, cell), cell.param);
   }

   // Fits a caps label into one cell: full size if it fits, a little smaller if
   // that is enough, otherwise broken across two lines at its middle space.
   void drawLabel(cairo_t *cr, const Rect &r, const char *label) {
      const double avail = r.w - 6;
      const double cx = r.x + r.w * 0.5;
      if (textWidth(cr, label, 8.5, true) <= avail) {
         drawText(cr, cx, r.y + 14, label, 8.5, true, Align::Center);
         return;
      }
      const char *space = nullptr;
      const size_t len = std::strlen(label);
      for (size_t i = 0; i < len; ++i)
         if (label[i] == ' ' &&
             (!space || std::abs(static_cast<int>(i) - static_cast<int>(len / 2)) <
                           std::abs(static_cast<int>(space - label) - static_cast<int>(len / 2))))
            space = label + i;
      if (!space) {
         drawText(cr, cx, r.y + 14, label, 7.0, true, Align::Center);
         return;
      }
      (void)avail;
      char first[64];
      const size_t firstLen = std::min(sizeof(first) - 1, static_cast<size_t>(space - label));
      std::memcpy(first, label, firstLen);
      first[firstLen] = 0;
      drawText(cr, cx, r.y + 10, first, 8.0, true, Align::Center);
      drawText(cr, cx, r.y + 19, space + 1, 8.0, true, Align::Center);
   }

   void drawCell(cairo_t *cr, const Rect &r, uint32_t id) {
      const ParamDesc &d = paramTable()[id];
      const double raw = mDelegate.guiParamValue(id);
      const bool hot = (mHover == static_cast<int>(id)) || (mDrag == static_cast<int>(id));

      char label[64];
      upperCase(d.name, label, sizeof(label));
      setColor(cr, hot ? kText : kTextDim);
      drawLabel(cr, r, label);

      char text[128];
      if (!paramValueToText(d, raw, text, sizeof(text)))
         std::snprintf(text, sizeof(text), "--");

      if (isChip(d)) {
         const double cw = r.w - 10;
         const double ch = 22;
         const double cx = r.x + 5;
         const double cy = r.y + 34;
         setColor(cr, kKnobFace);
         roundedRect(cr, cx, cy, cw, ch, 3);
         cairo_fill_preserve(cr);
         setColor(cr, hot ? kAccent : kPanelEdge, hot ? 0.7 : 1.0);
         cairo_set_line_width(cr, 1.0);
         cairo_stroke(cr);

         setColor(cr, kTextMute);
         drawTriangle(cr, cx + 9, cy + ch * 0.5, 7, -1);
         drawTriangle(cr, cx + cw - 9, cy + ch * 0.5, 7, 1);
         setColor(cr, kText);
         drawText(cr, cx + cw * 0.5, cy + 15, text, 10, false, Align::Center);
         return;
      }

      const double cx = r.x + r.w * 0.5;
      const double cy = r.y + 46;
      const double t = normalised(d, raw);

      setColor(cr, kTrack);
      cairo_set_line_width(cr, 3.0);
      cairo_arc(cr, cx, cy, kKnobR, kArcStart, kArcStart + kArcSweep);
      cairo_stroke(cr);

      const double from = isBipolar(d) ? kArcStart + kArcSweep * 0.5 : kArcStart;
      const double to = kArcStart + kArcSweep * t;
      setColor(cr, kAccent);
      cairo_set_line_width(cr, 3.0);
      if (to >= from)
         cairo_arc(cr, cx, cy, kKnobR, from, to);
      else
         cairo_arc_negative(cr, cx, cy, kKnobR, from, to);
      cairo_stroke(cr);

      setColor(cr, kKnobFace);
      cairo_arc(cr, cx, cy, kKnobR - 5, 0, 2 * M_PI);
      cairo_fill_preserve(cr);
      setColor(cr, hot ? kAccent : kPanelEdge, hot ? 0.6 : 1.0);
      cairo_set_line_width(cr, 1.0);
      cairo_stroke(cr);

      const double ang = kArcStart + kArcSweep * t;
      setColor(cr, kText);
      cairo_set_line_width(cr, 2.0);
      cairo_move_to(cr, cx + std::cos(ang) * (kKnobR - 15), cy + std::sin(ang) * (kKnobR - 15));
      cairo_line_to(cr, cx + std::cos(ang) * (kKnobR - 7), cy + std::sin(ang) * (kKnobR - 7));
      cairo_stroke(cr);

      setColor(cr, hot ? kAccent : kText, hot ? 1.0 : 0.85);
      drawText(cr, cx, r.y + 86, text, 9.5, false, Align::Center);
   }

   void drawMeter(cairo_t *cr) {
      setColor(cr, kPanelFill);
      roundedRect(cr, mMeter.x, mMeter.y, mMeter.w, mMeter.h, 5);
      cairo_fill_preserve(cr);
      setColor(cr, kPanelEdge);
      cairo_set_line_width(cr, 1.0);
      cairo_stroke(cr);
      setColor(cr, kAccent, 0.85);
      drawText(cr, mMeter.x + kPanelPad + 2, mMeter.y + 16, "ACTIVITY", 10, true, Align::Left);

      const uint32_t drops = mDelegate.guiDropletCount();
      const uint32_t limit = std::max<uint32_t>(1, mDelegate.guiDropletLimit());
      mLastDropCount = drops;
      // Compressed rather than linear: a texture that uses 3% of the droplet
      // ceiling is perfectly normal rain, and a linear meter would show nothing
      // at all for it.
      const double load = std::min(1.0, static_cast<double>(drops) / static_cast<double>(limit));
      mHistory[mHistoryHead] = static_cast<float>(std::pow(load, 0.45));
      mHistoryHead = (mHistoryHead + 1) % kHistory;
      mMeterFill = load;
      if (load > 0.0001)
         mDecayFrames = kHistory;
      else if (mDecayFrames > 0)
         --mDecayFrames;

      const double gx = mMeter.x + kPanelPad;
      const double gy = mMeter.y + kPanelTitleH + 4;
      const double gw = mMeter.w - 2 * kPanelPad;
      const double gh = mMeter.h - kPanelTitleH - 30;

      setColor(cr, kKnobFace);
      roundedRect(cr, gx, gy, gw, gh, 3);
      cairo_fill(cr);

      setColor(cr, kTrack, 0.5);
      cairo_set_line_width(cr, 1.0);
      for (int i = 1; i < 4; ++i) {
         const double ly = std::floor(gy + gh * (i / 4.0)) + 0.5;
         cairo_move_to(cr, gx + 3, ly);
         cairo_line_to(cr, gx + gw - 3, ly);
      }
      cairo_stroke(cr);

      const double barW = gw / kHistory;
      for (int i = 0; i < kHistory; ++i) {
         const int idx = (mHistoryHead + i) % kHistory;
         const double v = mHistory[idx];
         if (v <= 0.0)
            continue;
         const double h = std::max(1.0, v * (gh - 4));
         setColor(cr, kAccent, 0.30 + 0.55 * (static_cast<double>(i) / kHistory));
         cairo_rectangle(cr, gx + i * barW, gy + gh - 2 - h, std::max(1.0, barW - 0.5), h);
         cairo_fill(cr);
      }

      char info[64];
      std::snprintf(info, sizeof(info), "%u droplets", drops);
      setColor(cr, kTextDim);
      drawText(cr, gx, mMeter.y + mMeter.h - 10, info, 9, false, Align::Left);
      std::snprintf(info, sizeof(info), "max %u", limit);
      drawText(cr, gx + gw, mMeter.y + mMeter.h - 10, info, 9, false, Align::Right);
   }

   const char *presetLabel(char *buf, size_t size) const {
      const auto &list = mDelegate.guiPresets();
      const int cur = mDelegate.guiCurrentPreset();
      if (cur < 0 || cur >= static_cast<int>(list.size())) {
         std::snprintf(buf, size, "Init");
         return buf;
      }
      std::snprintf(buf, size, "%s%s", list[static_cast<size_t>(cur)].name.c_str(),
                    mDelegate.guiPresetEdited() ? " *" : "");
      return buf;
   }

   void drawPresetBar(cairo_t *cr) {
      setColor(cr, kTextMute);
      drawText(cr, kMargin, mBarY + 21, "PRESET", 9, true, Align::Left);

      auto button = [&](const Rect &r, int dir, bool hot) {
         setColor(cr, kPanelFill);
         roundedRect(cr, r.x, r.y, r.w, r.h, 4);
         cairo_fill_preserve(cr);
         setColor(cr, hot ? kAccent : kPanelEdge, hot ? 0.7 : 1.0);
         cairo_set_line_width(cr, 1.0);
         cairo_stroke(cr);
         setColor(cr, hot ? kAccent : kTextDim);
         drawTriangle(cr, r.x + r.w * 0.5, r.y + r.h * 0.5, 9, dir);
      };

      button(mPrevRect, -1, mHoverWidget == Widget::Prev);
      button(mNextRect, 1, mHoverWidget == Widget::Next);

      const bool hot = mHoverWidget == Widget::Name || mBrowserOpen;
      setColor(cr, kPanelFill);
      roundedRect(cr, mNameRect.x, mNameRect.y, mNameRect.w, mNameRect.h, 4);
      cairo_fill_preserve(cr);
      setColor(cr, hot ? kAccent : kPanelEdge, hot ? 0.7 : 1.0);
      cairo_set_line_width(cr, 1.0);
      cairo_stroke(cr);

      char name[160];
      presetLabel(name, sizeof(name));
      setColor(cr, kAccent);
      drawText(cr, mNameRect.x + mNameRect.w * 0.5, mNameRect.y + 21, name, 13, true,
               Align::Center);
      setColor(cr, kTextMute);
      drawTriangle(cr, mNameRect.x + mNameRect.w - 12, mNameRect.y + mNameRect.h * 0.5, 8, 0);

      const bool saveHot = mHoverWidget == Widget::Save || mSaveOpen;
      setColor(cr, kPanelFill);
      roundedRect(cr, mSaveRect.x, mSaveRect.y, mSaveRect.w, mSaveRect.h, 4);
      cairo_fill_preserve(cr);
      setColor(cr, saveHot ? kAccent : kPanelEdge, saveHot ? 0.7 : 1.0);
      cairo_set_line_width(cr, 1.0);
      cairo_stroke(cr);
      setColor(cr, saveHot ? kAccent : kTextDim);
      drawText(cr, mSaveRect.x + mSaveRect.w * 0.5, mSaveRect.y + 20, "SAVE", 10, true,
               Align::Center);
   }

   // ------------------------------------------------------------ save dialog

   Rect savePanel() const {
      Rect r;
      r.w = 420;
      r.h = 156;
      r.x = (kWindowW - r.w) * 0.5;
      r.y = kHeaderH + 130;
      return r;
   }

   Rect saveFieldRect() const {
      const Rect p = savePanel();
      return {p.x + 20, p.y + 54, p.w - 40, 30};
   }

   Rect saveOkRect() const {
      const Rect p = savePanel();
      return {p.x + p.w - 20 - 92, p.y + p.h - 20 - 28, 92, 28};
   }

   Rect saveCancelRect() const {
      const Rect p = savePanel();
      return {p.x + p.w - 20 - 92 - 10 - 92, p.y + p.h - 20 - 28, 92, 28};
   }

   void openSaveDialog() {
      mBrowserOpen = false;
      closeMenu();
      mSaveOpen = true;
      mSaveName = mDelegate.guiSuggestedPresetName();
      mSaveStatus.clear();
      mSaveFailed = false;
      // An embedded plugin window only sees key events if the host routes them
      // here. Asking for the focus is worth a try, and a failure to get it is
      // not worth an X error, so the handler is muted around the request.
      if (mDisplay && mWindow) {
         XErrorHandler previous = XSetErrorHandler(&ignoreXError);
         XSetInputFocus(mDisplay, mWindow, RevertToParent, CurrentTime);
         XSync(mDisplay, False);
         XSetErrorHandler(previous);
      }
      mDirty = true;
   }

   void commitSave() {
      std::string name = mSaveName;
      while (!name.empty() && name.front() == ' ')
         name.erase(name.begin());
      while (!name.empty() && name.back() == ' ')
         name.pop_back();
      if (name.empty()) {
         mSaveStatus = "Give the preset a name.";
         mSaveFailed = true;
         mDirty = true;
         return;
      }
      std::string error;
      if (!mDelegate.guiSavePreset(name, error)) {
         mSaveStatus = error.empty() ? std::string("Could not save the preset.") : error;
         mSaveFailed = true;
         mDirty = true;
         return;
      }
      mSaveOpen = false;
      mDirty = true;
   }

   void drawSaveDialog(cairo_t *cr) {
      setColor(cr, kBgBottom, 0.88);
      cairo_rectangle(cr, 0, 0, kWindowW, kWindowH);
      cairo_fill(cr);

      const Rect p = savePanel();
      setColor(cr, kPanelFill);
      roundedRect(cr, p.x, p.y, p.w, p.h, 6);
      cairo_fill_preserve(cr);
      setColor(cr, kAccent, 0.5);
      cairo_set_line_width(cr, 1.0);
      cairo_stroke(cr);

      setColor(cr, kAccent);
      drawText(cr, p.x + 20, p.y + 26, "SAVE PRESET", 11, true, Align::Left);
      setColor(cr, kTextMute);
      drawText(cr, p.x + p.w - 20, p.y + 26, "to your own preset folder", 9, false, Align::Right);

      const Rect f = saveFieldRect();
      setColor(cr, kKnobFace);
      roundedRect(cr, f.x, f.y, f.w, f.h, 3);
      cairo_fill_preserve(cr);
      setColor(cr, mSaveFailed ? kAccent : kPanelEdge, 1.0);
      cairo_set_line_width(cr, 1.0);
      cairo_stroke(cr);

      std::string shown = mSaveName;
      shown += "_"; // a plain caret; the window has no blinking anywhere else
      setColor(cr, kText);
      drawText(cr, f.x + 9, f.y + 20, shown.c_str(), 12, false, Align::Left);

      setColor(cr, kTextMute);
      drawText(cr, p.x + 20, f.y + f.h + 20,
               mSaveStatus.empty() ? "Type a name, then Enter. Esc cancels." : mSaveStatus.c_str(),
               9, false, Align::Left);

      auto dialogButton = [&](const Rect &r, const char *label, bool accent) {
         setColor(cr, kPanelFill);
         roundedRect(cr, r.x, r.y, r.w, r.h, 4);
         cairo_fill_preserve(cr);
         setColor(cr, accent ? kAccent : kPanelEdge, accent ? 0.7 : 1.0);
         cairo_set_line_width(cr, 1.0);
         cairo_stroke(cr);
         setColor(cr, accent ? kAccent : kTextDim);
         drawText(cr, r.x + r.w * 0.5, r.y + 18, label, 10, true, Align::Center);
      };
      dialogButton(saveCancelRect(), "CANCEL", false);
      dialogButton(saveOkRect(), "SAVE", true);
   }

   void onSaveKey(XKeyEvent &ke) {
      char buf[32];
      KeySym sym = 0;
      const int n = XLookupString(&ke, buf, sizeof(buf) - 1, &sym, nullptr);
      if (sym == XK_Escape) {
         mSaveOpen = false;
         mDirty = true;
         return;
      }
      if (sym == XK_Return || sym == XK_KP_Enter) {
         commitSave();
         return;
      }
      if (sym == XK_BackSpace) {
         if (!mSaveName.empty())
            mSaveName.pop_back();
         mSaveStatus.clear();
         mSaveFailed = false;
         mDirty = true;
         return;
      }
      for (int i = 0; i < n; ++i) {
         const unsigned char c = static_cast<unsigned char>(buf[i]);
         if (c >= 0x20 && c != 0x7F && mSaveName.size() < 48)
            mSaveName.push_back(static_cast<char>(c));
      }
      mSaveStatus.clear();
      mSaveFailed = false;
      mDirty = true;
   }

   void drawHelpLine(cairo_t *cr) {
      const char *msg = nullptr;
      if (mHover >= 0 && mHover < static_cast<int>(kNumParams))
         msg = paramTable()[mHover].tip;
      if (!msg) {
         const auto &list = mDelegate.guiPresets();
         const int cur = mDelegate.guiCurrentPreset();
         if (cur >= 0 && cur < static_cast<int>(list.size()) &&
             !list[static_cast<size_t>(cur)].description.empty())
            msg = list[static_cast<size_t>(cur)].description.c_str();
      }
      if (!msg)
         msg = "Drag a knob to edit, double-click to reset, shift-drag for fine "
               "control. Click a menu to pick from the list, its arrows to step.";

      setColor(cr, kTextMute);
      drawText(cr, kMargin, mHelpY + kHelpH - 8, msg, 10, false, Align::Left);
   }

   // ---------------------------------------------------------------- browser

   int browserRows() const {
      const int n = static_cast<int>(mDelegate.guiPresets().size());
      return (n + kBrowserCols - 1) / kBrowserCols;
   }

   Rect browserPanel() const {
      const double maxH = kWindowH - kHeaderH - 90;
      Rect r;
      r.x = kMargin + 40;
      r.w = kContentW - 80;
      r.h = std::min(maxH, 40.0 + browserRows() * kBrowserRowH + kBrowserPad);
      r.y = kHeaderH + (maxH - r.h) * 0.5 + 20;
      return r;
   }

   Rect browserItemRect(int index) const {
      const Rect p = browserPanel();
      const int rows = std::max(1, browserRows());
      const int col = index / rows;
      const int row = index % rows;
      Rect r;
      r.w = (p.w - 2 * kBrowserPad) / kBrowserCols;
      r.h = kBrowserRowH;
      r.x = p.x + kBrowserPad + col * r.w;
      r.y = p.y + 40 + row * r.h;
      return r;
   }

   // An enum chip opens a list anchored to itself. Stepping one value per click
   // never reaches the far end of a seven-way enum without a lot of clicking,
   // and the value the user wants is always visible this way.
   Rect menuPanel() const {
      const ParamDesc &d = paramTable()[mMenuParam];
      const Rect &c = cellRectFor(static_cast<uint32_t>(mMenuParam));
      Rect r;
      r.w = std::max(c.w - 10.0, 96.0);
      r.h = d.enumCount * kMenuRowH + 2 * kMenuPad;
      r.x = c.x + 5;
      r.y = c.y + 58; // just under the chip
      // Flip above the chip rather than run off the bottom of the window.
      if (r.y + r.h > kWindowH - 8)
         r.y = c.y + 32 - r.h;
      if (r.y < kHeaderH + 4)
         r.y = kHeaderH + 4;
      return r;
   }

   Rect menuItemRect(int index) const {
      const Rect p = menuPanel();
      Rect r;
      r.x = p.x + kMenuPad;
      r.w = p.w - 2 * kMenuPad;
      r.h = kMenuRowH;
      r.y = p.y + kMenuPad + index * kMenuRowH;
      return r;
   }

   int menuItemAt(double x, double y) const {
      if (mMenuParam < 0)
         return -1;
      const ParamDesc &d = paramTable()[mMenuParam];
      for (int i = 0; i < static_cast<int>(d.enumCount); ++i)
         if (menuItemRect(i).contains(x, y))
            return i;
      return -1;
   }

   void closeMenu() {
      mMenuParam = -1;
      mMenuHover = -1;
   }

   void drawMenu(cairo_t *cr) {
      const ParamDesc &d = paramTable()[mMenuParam];
      const Rect p = menuPanel();
      setColor(cr, kPanelFill);
      roundedRect(cr, p.x, p.y, p.w, p.h, 5);
      cairo_fill_preserve(cr);
      setColor(cr, kAccent, 0.5);
      cairo_set_line_width(cr, 1.0);
      cairo_stroke(cr);

      const int cur = static_cast<int>(
         std::floor(mDelegate.guiParamValue(static_cast<uint32_t>(mMenuParam)) + 0.5));
      for (int i = 0; i < static_cast<int>(d.enumCount); ++i) {
         const Rect r = menuItemRect(i);
         const bool hot = mMenuHover == i;
         const bool sel = cur == i;
         if (hot || sel) {
            setColor(cr, kAccent, hot ? 0.20 : 0.10);
            roundedRect(cr, r.x, r.y, r.w, r.h, 3);
            cairo_fill(cr);
         }
         setColor(cr, sel ? kAccent : kText, hot ? 1.0 : 0.85);
         drawText(cr, r.x + 8, r.y + r.h * 0.5 + 4, d.enumNames[i], 10, sel, Align::Left);
      }
   }

   void drawBrowser(cairo_t *cr) {
      setColor(cr, kBgBottom, 0.88);
      cairo_rectangle(cr, 0, 0, kWindowW, kWindowH);
      cairo_fill(cr);

      const Rect p = browserPanel();
      setColor(cr, kPanelFill);
      roundedRect(cr, p.x, p.y, p.w, p.h, 6);
      cairo_fill_preserve(cr);
      setColor(cr, kAccent, 0.5);
      cairo_set_line_width(cr, 1.0);
      cairo_stroke(cr);

      setColor(cr, kAccent);
      drawText(cr, p.x + kBrowserPad, p.y + 24, "PRESETS", 11, true, Align::Left);
      setColor(cr, kTextMute);
      drawText(cr, p.x + p.w - kBrowserPad, p.y + 24, "click to load, or click outside to close",
               9, false, Align::Right);

      const auto &list = mDelegate.guiPresets();
      const int cur = mDelegate.guiCurrentPreset();
      for (size_t i = 0; i < list.size(); ++i) {
         const Rect r = browserItemRect(static_cast<int>(i));
         if (r.y + r.h > p.y + p.h)
            continue;
         const bool hot = mBrowserHover == static_cast<int>(i);
         const bool sel = cur == static_cast<int>(i);
         if (hot || sel) {
            setColor(cr, kAccent, hot ? 0.20 : 0.10);
            roundedRect(cr, r.x + 2, r.y + 1, r.w - 4, r.h - 2, 3);
            cairo_fill(cr);
         }
         setColor(cr, sel ? kAccent : kText, hot ? 1.0 : 0.85);
         drawText(cr, r.x + 10, r.y + r.h * 0.5 + 4, list[i].name.c_str(), 11, sel, Align::Left);
         if (list[i].userContent) {
            setColor(cr, kTextMute);
            drawText(cr, r.x + r.w - 10, r.y + r.h * 0.5 + 4, "USER", 8, true, Align::Right);
         }
      }
   }

   // ----------------------------------------------------------------- events

   // `None` is taken: X11 defines it as a macro.
   enum class Widget { NoWidget, Prev, Next, Name, Save };

   void pumpEvents() {
      XEvent ev;
      while (XPending(mDisplay)) {
         XNextEvent(mDisplay, &ev);
         switch (ev.type) {
         case Expose:
            mDirty = true;
            break;
         case ButtonPress:
            onButtonPress(ev.xbutton);
            break;
         case ButtonRelease:
            onButtonRelease();
            break;
         case MotionNotify:
            onMotion(ev.xmotion.x / mScale, ev.xmotion.y / mScale, ev.xmotion.state);
            break;
         case LeaveNotify:
            if (mDrag < 0) {
               mHover = -1;
               mHoverWidget = Widget::NoWidget;
               mDirty = true;
            }
            break;
         case KeyPress:
            if (mSaveOpen) {
               onSaveKey(ev.xkey);
               break;
            }
            if (mBrowserOpen || mMenuParam >= 0) {
               mBrowserOpen = false;
               closeMenu();
               mDirty = true;
            }
            break;
         case ClientMessage:
            if (static_cast<Atom>(ev.xclient.data.l[0]) == mDeleteAtom)
               hide();
            break;
         default:
            break;
         }
      }
   }

   int cellAt(double x, double y) const {
      for (size_t i = 0; i < mCellRects.size(); ++i)
         if (mCellRects[i].contains(x, y))
            return static_cast<int>(mCells[i].param);
      return -1;
   }

   void onButtonPress(const XButtonEvent &be) {
      const double x = be.x / mScale;
      const double y = be.y / mScale;

      if (mSaveOpen) {
         if (be.button == Button1) {
            if (saveOkRect().contains(x, y))
               commitSave();
            else if (saveCancelRect().contains(x, y) || !savePanel().contains(x, y))
               mSaveOpen = false;
         }
         mDirty = true;
         return;
      }

      if (mMenuParam >= 0) {
         if (be.button == Button1) {
            const int item = menuItemAt(x, y);
            const bool inside = menuPanel().contains(x, y);
            if (item >= 0) {
               const uint32_t id = static_cast<uint32_t>(mMenuParam);
               mDelegate.guiBeginEdit(id);
               mDelegate.guiSetParam(id, static_cast<double>(item));
               mDelegate.guiEndEdit(id);
            }
            if (item >= 0 || !inside)
               closeMenu();
         }
         mDirty = true;
         return;
      }

      if (mBrowserOpen) {
         if (be.button == Button1) {
            const int item = browserItemAt(x, y);
            if (item >= 0)
               mDelegate.guiLoadPreset(item);
            if (item >= 0 || !browserPanel().contains(x, y))
               mBrowserOpen = false;
         }
         mDirty = true;
         return;
      }

      if (be.button == Button4 || be.button == Button5) {
         const int id = cellAt(x, y);
         if (id >= 0)
            nudge(static_cast<uint32_t>(id), be.button == Button4 ? 1 : -1,
                  (be.state & ShiftMask) != 0);
         return;
      }

      if (mPrevRect.contains(x, y)) {
         stepPreset(-1);
         return;
      }
      if (mNextRect.contains(x, y)) {
         stepPreset(1);
         return;
      }
      if (mSaveRect.contains(x, y)) {
         openSaveDialog();
         return;
      }
      if (mNameRect.contains(x, y)) {
         mBrowserOpen = true;
         mBrowserHover = -1;
         closeMenu();
         mDirty = true;
         return;
      }

      const int id = cellAt(x, y);
      if (id < 0)
         return;
      const ParamDesc &d = paramTable()[static_cast<uint32_t>(id)];

      // Right-click and double-click both mean "put it back where it was".
      const bool doubleClick =
         be.button == Button1 && mLastClickParam == id && be.time - mLastClickTime < 400;
      mLastClickParam = id;
      mLastClickTime = be.time;

      if (be.button == Button3 || (doubleClick && !isChip(d))) {
         mDelegate.guiBeginEdit(static_cast<uint32_t>(id));
         mDelegate.guiSetParam(static_cast<uint32_t>(id), d.def);
         mDelegate.guiEndEdit(static_cast<uint32_t>(id));
         mLastClickParam = -1;
         mDirty = true;
         return;
      }
      if (be.button != Button1)
         return;

      if (isChip(d)) {
         // The two arrows still step by one; the name between them opens the
         // full list.
         const Rect &r = cellRectFor(static_cast<uint32_t>(id));
         const double chipX = r.x + 5;
         const double chipW = r.w - 10;
         if (x < chipX + 18.0) {
            nudge(static_cast<uint32_t>(id), -1, false);
         } else if (x > chipX + chipW - 18.0) {
            nudge(static_cast<uint32_t>(id), 1, false);
         } else {
            mMenuParam = id;
            mMenuHover = menuItemAt(x, y);
            mDirty = true;
         }
         return;
      }

      mDrag = id;
      mDragStartY = y;
      mDragStartValue = mDelegate.guiParamValue(static_cast<uint32_t>(id));
      mDelegate.guiBeginEdit(static_cast<uint32_t>(id));
      mDirty = true;
   }

   void onButtonRelease() {
      if (mDrag >= 0) {
         mDelegate.guiEndEdit(static_cast<uint32_t>(mDrag));
         mDrag = -1;
         mDirty = true;
      }
   }

   void onMotion(double x, double y, unsigned int state) {
      if (mDrag >= 0) {
         const uint32_t id = static_cast<uint32_t>(mDrag);
         const ParamDesc &d = paramTable()[id];
         const double span = d.max - d.min;
         const double fine = (state & ShiftMask) ? 0.2 : 1.0;
         double v = mDragStartValue + (mDragStartY - y) * (span / 200.0) * fine;
         if (isStepped(d))
            v = std::floor(v + 0.5);
         v = std::min(d.max, std::max(d.min, v));
         mDelegate.guiSetParam(id, v);
         mDirty = true;
         return;
      }

      if (mSaveOpen)
         return;

      if (mMenuParam >= 0) {
         const int item = menuItemAt(x, y);
         if (item != mMenuHover) {
            mMenuHover = item;
            mDirty = true;
         }
         return;
      }

      if (mBrowserOpen) {
         const int item = browserItemAt(x, y);
         if (item != mBrowserHover) {
            mBrowserHover = item;
            mDirty = true;
         }
         return;
      }

      const int id = cellAt(x, y);
      Widget w = Widget::NoWidget;
      if (mPrevRect.contains(x, y))
         w = Widget::Prev;
      else if (mNextRect.contains(x, y))
         w = Widget::Next;
      else if (mNameRect.contains(x, y))
         w = Widget::Name;
      else if (mSaveRect.contains(x, y))
         w = Widget::Save;

      if (id != mHover || w != mHoverWidget) {
         mHover = id;
         mHoverWidget = w;
         mDirty = true;
      }
   }

   int browserItemAt(double x, double y) const {
      const auto &list = mDelegate.guiPresets();
      const Rect p = browserPanel();
      for (size_t i = 0; i < list.size(); ++i) {
         const Rect r = browserItemRect(static_cast<int>(i));
         if (r.y + r.h > p.y + p.h)
            continue;
         if (r.contains(x, y))
            return static_cast<int>(i);
      }
      return -1;
   }

   const Rect &cellRectFor(uint32_t id) const {
      for (size_t i = 0; i < mCells.size(); ++i)
         if (mCells[i].param == id)
            return mCellRects[i];
      return mCellRects[0];
   }

   void nudge(uint32_t id, int direction, bool fine) {
      const ParamDesc &d = paramTable()[id];
      const double span = d.max - d.min;
      double v = mDelegate.guiParamValue(id);
      if (isStepped(d))
         v = std::floor(v + 0.5) + direction;
      else
         v += direction * span * (fine ? 0.005 : 0.02);
      v = std::min(d.max, std::max(d.min, v));
      mDelegate.guiBeginEdit(id);
      mDelegate.guiSetParam(id, v);
      mDelegate.guiEndEdit(id);
      mDirty = true;
   }

   void stepPreset(int direction) {
      const int n = static_cast<int>(mDelegate.guiPresets().size());
      if (n <= 0)
         return;
      int cur = mDelegate.guiCurrentPreset();
      if (cur < 0)
         cur = direction > 0 ? -1 : 0;
      int next = (cur + direction) % n;
      if (next < 0)
         next += n;
      mDelegate.guiLoadPreset(next);
      mDirty = true;
   }

   // ------------------------------------------------------------------ state

   static constexpr int kHistory = 96;
   static constexpr int kBrowserCols = 3;
   static constexpr int kBrowserPad = 14;
   static constexpr int kBrowserRowH = 26;

   GuiDelegate &mDelegate;

   Display *mDisplay = nullptr;
   Window mWindow = 0;
   Visual *mVisual = nullptr;
   Atom mDeleteAtom = 0;
   cairo_surface_t *mTarget = nullptr;
   cairo_t *mTargetCr = nullptr;
   cairo_surface_t *mBuffer = nullptr;
   cairo_t *mBufferCr = nullptr;
   double mScale = 1.0;

   std::vector<Panel> mPanels;
   std::vector<Cell> mCells;
   std::vector<Rect> mCellRects;
   Rect mMeter;
   Rect mPrevRect, mNameRect, mNextRect, mSaveRect;
   int mBarY = 0, mHelpY = 0;

   bool mDirty = true;
   int mHover = -1;
   Widget mHoverWidget = Widget::NoWidget;
   int mDrag = -1;
   double mDragStartY = 0.0, mDragStartValue = 0.0;
   int mLastClickParam = -1;
   Time mLastClickTime = 0;

   bool mBrowserOpen = false;
   int mBrowserHover = -1;
   bool mSaveOpen = false;
   bool mSaveFailed = false;
   std::string mSaveName;
   std::string mSaveStatus;
   int mMenuParam = -1; // enum parameter whose dropdown is open, or -1
   int mMenuHover = -1;

   double mShown[kNumParams];
   uint32_t mLastDropCount = 0;
   double mMeterFill = 0.0;
   int mDecayFrames = 0;
   double mStreakPhase = 0.0;
   float mHistory[kHistory] = {0.0f};
   int mHistoryHead = 0;
};

} // namespace

Gui *createGui(GuiDelegate &delegate) {
   auto *gui = new X11Gui(delegate);
   if (!gui->open()) {
      delete gui;
      return nullptr;
   }
   return gui;
}

} // namespace rainyday
