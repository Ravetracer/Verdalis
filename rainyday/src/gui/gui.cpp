// RainyDay's plugin window.
//
// Cairo for everything drawn inside it, and the platform's own windowing under
// that: X11 on Linux, Win32 on Windows. No toolkit, so the plugin stays one
// .clap file and pulls in nothing a machine that can run a DAW does not already
// have. Everything between the window and the drawing -- the layout, the hit
// testing, the overlays -- is shared, and the two platforms differ only in how
// a window is made, how events arrive and where the finished frame is blitted.
//
// The whole layout is generated from the parameter table in params.cpp: the
// panels are the modules, the cells are the parameters, and the help line is
// the parameter's own tip. Adding a parameter there puts it on screen here.

#include "gui/gui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include <cairo/cairo.h>

#if defined(_WIN32)
#   include <cairo-win32.h>
#   include <windows.h>
#   include <windowsx.h> // GET_X_LPARAM
// MinGW's <cmath> hides M_PI unless this is asked for, and the knob arcs need it.
#   ifndef M_PI
#      define M_PI 3.14159265358979323846
#   endif
#else
#   include <X11/Xlib.h>
#   include <X11/Xutil.h>
#   include <X11/keysym.h>
#   include <cairo/cairo-xlib.h>
#endif

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
// Ten cells wide, plus the padding and gaps of the busiest row, which is the
// four-panel one. Rows with fewer panels stretch their last panel to match.
// Widened by one cell when Slosh joined the RAIN panel, which needs eight
// columns rather than seven.
constexpr int kContentW = 1012;
constexpr int kHeaderH = 66;
constexpr int kBarH = 32;
constexpr int kHelpH = 24;

constexpr int kWindowW = kContentW + 2 * kMargin;
constexpr int kWindowH = 740;
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

// Seven per line, which is one row of the panel. Slosh sits next to Splash:
// it lengthens what Splash starts.
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

#define PANEL(title, cols, rows, arr)                                                              \
   { title, cols, rows, arr, static_cast<int>(sizeof(arr) / sizeof(arr[0])) }

constexpr PanelSpec kPanelSpecs[] = {
   PANEL("RAIN", 8, 2, kRainParams),     PANEL("DISTANT", 3, 2, kDistantParams),
   PANEL("ENVELOPE", 3, 2, kEnvParams),  PANEL("FILTER", 3, 2, kFilterParams),
   PANEL("SPACE", 3, 2, kSpaceParams),   PANEL("CLOSE", 1, 2, kCloseParams),
   PANEL("OUTPUT", 3, 1, kOutParams),
};
#undef PANEL

constexpr int kNumPanels = static_cast<int>(sizeof(kPanelSpecs) / sizeof(kPanelSpecs[0]));

// Which panels share a row, in order. DISTANT and CLOSE sit on the right-hand
// edge of their rows so the two layer panels read as a pair. The activity meter
// fills what is left of the last row.
constexpr int kRowStart[] = {0, 2, 6};
constexpr int kRowCount[] = {2, 4, 1};
constexpr int kNumRows = 3;

// The layout is a table, and a table is easy to break by adding a parameter to
// a panel that has no room for it, or by forgetting to put it on a panel at
// all. None of that should need a running window to notice, so it is checked
// here instead.
constexpr int panelWidth(const PanelSpec &s) { return s.cols * kCellW + 2 * kPanelPad; }
constexpr int panelHeight(const PanelSpec &s) { return kPanelTitleH + s.rows * kCellH + kPanelPad; }

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

// Button numbers follow X11's, which is what the shared code was written
// against; the Win32 side translates into them.
constexpr unsigned kButtonLeft = 1;
constexpr unsigned kButtonRight = 3;
constexpr unsigned kWheelUp = 4;
constexpr unsigned kWheelDown = 5;

enum class Align { Left, Center, Right };

#if !defined(_WIN32)
// Asking an embedded window for the keyboard focus can fail for reasons that
// are none of the plugin's business. A failure must not reach Xlib's default
// handler, which exits the host.
int ignoreXError(Display *, XErrorEvent *) { return 0; }
#endif

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

#if defined(_WIN32)
   static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

   static const wchar_t *windowClassName() { return L"RainyDayWindow"; }

   bool open() override {
      if (mWindow)
         return true;
      static bool registered = false;
      if (!registered) {
         WNDCLASSEXW wc{};
         wc.cbSize = sizeof(wc);
         wc.style = CS_OWNDC;
         wc.lpfnWndProc = &X11Gui::wndProc;
         wc.hInstance = GetModuleHandleW(nullptr);
         wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
         wc.hbrBackground = nullptr; // every pixel is painted, so never erase
         wc.lpszClassName = windowClassName();
         if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return false;
         registered = true;
      }
      // Created as an unowned popup, not as a child: WS_CHILD demands a parent
      // at creation and CLAP does not supply one until set_parent. embed()
      // turns it into a child once the host says where it goes.
      mWindow = CreateWindowExW(0, windowClassName(), L"RainyDay", WS_POPUP | WS_CLIPCHILDREN,
                                0, 0, static_cast<int>(pixelW()), static_cast<int>(pixelH()),
                                nullptr, nullptr, GetModuleHandleW(nullptr), this);
      if (!mWindow)
         return false;
      // No target surface is made here. A cached DC belongs to the window as it
      // was when the DC was taken, and this window is reparented into the
      // host's afterwards; the surface to draw on is the one BeginPaint hands
      // over, which is correct by construction.
      allocateBuffer();
      return true;
   }

   bool embed(uintptr_t parentWindow) override {
      if (!mWindow)
         return false;
      SetParent(mWindow, reinterpret_cast<HWND>(parentWindow));
      SetWindowLongPtrW(mWindow, GWL_STYLE, WS_CHILD | WS_CLIPCHILDREN | WS_VISIBLE);
      SetWindowPos(mWindow, nullptr, 0, 0, static_cast<int>(pixelW()),
                   static_cast<int>(pixelH()), SWP_NOZORDER | SWP_FRAMECHANGED);
      return true;
   }

   bool setTransientFor(uintptr_t) override {
      // Windows has no separate transient hint for an embedded child.
      return mWindow != nullptr;
   }

   void setTitle(const char *title) override {
      if (mWindow && title)
         SetWindowTextA(mWindow, title);
   }

#else
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

   bool embed(uintptr_t parentWindow) override {
      if (!mWindow)
         return false;
      XReparentWindow(mDisplay, mWindow, static_cast<Window>(parentWindow), 0, 0);
      XFlush(mDisplay);
      return true;
   }

   bool setTransientFor(uintptr_t parentWindow) override {
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
#endif

   // Resizing is zooming: every coordinate in this file is a design pixel and
   // cairo applies one scale to all of it, so a host that asks for a bigger
   // window gets the same layout drawn larger. Nothing reflows, and nothing has
   // to. The scale range is generous at the top for large displays and stops at
   // half size at the bottom, where the help line becomes unreadable.
   static constexpr double kMinScale = 0.5;
   static constexpr double kMaxScale = 4.0;

   static double scaleFor(uint32_t width, uint32_t height) {
      const double s = std::min(width / static_cast<double>(kWindowW),
                                height / static_cast<double>(kWindowH));
      return std::min(kMaxScale, std::max(kMinScale, s));
   }

   void designSize(uint32_t *width, uint32_t *height) const override {
      *width = kWindowW;
      *height = kWindowH;
   }

   void fitSize(uint32_t *width, uint32_t *height) const override {
      const double s = scaleFor(*width, *height);
      *width = static_cast<uint32_t>(kWindowW * s + 0.5);
      *height = static_cast<uint32_t>(kWindowH * s + 0.5);
   }

   bool resize(uint32_t width, uint32_t height) override {
      setScale(scaleFor(width, height));
      return true;
   }

   void setScale(double scale) override {
      if (scale < kMinScale || scale > kMaxScale || std::fabs(scale - mScale) < 0.001)
         return;
      mScale = scale;
      if (!mWindow)
         return;
#if defined(_WIN32)
      SetWindowPos(mWindow, nullptr, 0, 0, static_cast<int>(pixelW()),
                   static_cast<int>(pixelH()), SWP_NOMOVE | SWP_NOZORDER);
#else
      XResizeWindow(mDisplay, mWindow, pixelW(), pixelH());
      cairo_xlib_surface_set_size(mTarget, pixelW(), pixelH());
#endif
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
#if defined(_WIN32)
      ShowWindow(mWindow, SW_SHOWNA);
#else
      XMapWindow(mDisplay, mWindow);
      XFlush(mDisplay);
#endif
      mDirty = true;
   }

   void hide() override {
      if (!mWindow)
         return;
      if (mSaveOpen)
         closeSaveDialog();
      closeEntry();
#if defined(_WIN32)
      ShowWindow(mWindow, SW_HIDE);
#else
      XUnmapWindow(mDisplay, mWindow);
      XFlush(mDisplay);
#endif
   }

   void tick() override {
#if defined(_WIN32)
      if (!mWindow)
         return;
      pumpEvents();
      // Painting happens in WM_PAINT, not here. GDI belongs to the thread that
      // owns the window, and tick() is called from whatever clock the host
      // offers -- in a host with no timer that is a thread of the plugin's own,
      // which would be drawing on someone else's DC. Marking the window dirty
      // is safe from any thread and the owning thread does the work.
      if (needsRepaint())
         InvalidateRect(mWindow, nullptr, FALSE);
#else
      if (!mDisplay)
         return;
      pumpEvents();
      if (!mWindow)
         return;
      if (needsRepaint())
         paint();
#endif
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
#if defined(_WIN32)
      if (mWindow) {
         releaseKeyboard();
         DestroyWindow(mWindow);
      }
      mWindow = nullptr;
#else
      if (mDisplay) {
         // Closing the display would drop the grab anyway, but say so plainly
         // rather than depending on it: a keyboard nobody can type on is a very
         // expensive thing to leave behind.
         releaseKeyboard();
         if (mWindow)
            XDestroyWindow(mDisplay, mWindow);
         XCloseDisplay(mDisplay);
      }
      mWindow = 0;
      mDisplay = nullptr;
#endif
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
            // Rows hold different numbers of panels and so carry different
            // amounts of padding. The last panel of a row without the activity
            // meter takes up the difference, which keeps every row flush with
            // the right-hand edge instead of ending raggedly.
            if (row != kNumRows - 1 && i == kRowCount[row] - 1)
               p.rect.w = kMargin + kContentW - p.rect.x;
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

#if defined(_WIN32)
   // Lends paint() a target for the duration of one WM_PAINT. Everything the
   // frame is built from is the shared code; only where it lands differs.
   void paintToDC(HDC dc) {
      cairo_surface_t *surface = cairo_win32_surface_create(dc);
      cairo_t *cr = cairo_create(surface);
      cairo_surface_t *const keptTarget = mTarget;
      cairo_t *const keptCr = mTargetCr;
      mTarget = surface;
      mTargetCr = cr;
      paint();
      mTarget = keptTarget;
      mTargetCr = keptCr;
      cairo_destroy(cr);
      cairo_surface_destroy(surface);
   }
#endif

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
#if !defined(_WIN32)
      XFlush(mDisplay);
#endif
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

   // The value under a knob is also a text field. Click it and type: whatever
   // the display prints is accepted back, including units and multipliers
   // ("2.2k", "500 ms", "-12 dB"), because paramTextToValue is the same parser
   // the host's text entry goes through. Like the save field it grabs the
   // keyboard for as long as it is open, because an embedded window is not
   // given the focus by every host; it is a modal field and lives for one
   // value, which is the case where a grab is defensible.
   // Both window systems reduce a keystroke to the same few commands plus some
   // text, so the fields do not need to know which one they are on.
   enum class KeyCommand { NoCommand, Escape, Accept, Backspace, Up, Down };

   Rect valueRect(const Rect &cell) const { return {cell.x + 6, cell.y + 74, cell.w - 12, 22}; }

   void openEntry(uint32_t id) {
      const ParamDesc &d = paramTable()[id];
      if (isChip(d))
         return;
      mBrowserOpen = false;
      closeMenu();
      char text[128];
      if (!paramValueToText(d, mDelegate.guiParamValue(id), text, sizeof(text)))
         text[0] = 0;
      mEntryParam = static_cast<int>(id);
      mEntryText = text;
      mEntryFailed = false;
      mEntryFresh = true;
      grabKeyboard();
      mDirty = true;
   }

   // Every path that leaves the field comes through here, for the same reason
   // as closeSaveDialog: a grab that outlives its field is a dead keyboard.
   void closeEntry() {
      if (mEntryParam < 0)
         return;
      mEntryParam = -1;
      releaseKeyboard();
      mDirty = true;
   }

   void commitEntry() {
      if (mEntryParam < 0)
         return;
      const uint32_t id = static_cast<uint32_t>(mEntryParam);
      double raw = 0.0;
      if (!paramTextToValue(paramTable()[id], mEntryText.c_str(), &raw)) {
         mEntryFailed = true;
         mDirty = true;
         return;
      }
      mDelegate.guiBeginEdit(id);
      mDelegate.guiSetParam(id, raw);
      mDelegate.guiEndEdit(id);
      closeEntry();
   }

   void onEntryKey(KeyCommand cmd, const char *text, int textLen) {
      if (cmd == KeyCommand::Escape) {
         closeEntry();
         return;
      }
      if (cmd == KeyCommand::Accept) {
         commitEntry();
         return;
      }
      // The field opens showing the current value as selected text: the first
      // keystroke replaces it, and only then does typing append.
      if (cmd == KeyCommand::Backspace) {
         if (mEntryFresh)
            mEntryText.clear();
         else if (!mEntryText.empty())
            mEntryText.pop_back();
         mEntryFresh = false;
      } else {
         bool typed = false;
         for (int i = 0; i < textLen; ++i) {
            const unsigned char c = static_cast<unsigned char>(text[i]);
            if (c < 0x20 || c == 0x7F)
               continue;
            if (mEntryFresh) {
               mEntryText.clear();
               mEntryFresh = false;
            }
            if (mEntryText.size() < 24)
               mEntryText.push_back(static_cast<char>(c));
            typed = true;
         }
         if (!typed && cmd == KeyCommand::NoCommand)
            return; // a modifier or dead key: nothing to show
      }
      mEntryFailed = false;
      mDirty = true;
   }

   void drawEntryField(cairo_t *cr, const Rect &cell) {
      const Rect f = valueRect(cell);
      setColor(cr, kKnobFace);
      roundedRect(cr, f.x, f.y, f.w, f.h, 3);
      cairo_fill_preserve(cr);
      if (mEntryFailed)
         cairo_set_source_rgb(cr, 0.85, 0.30, 0.30);
      else
         setColor(cr, kAccent, 0.9);
      cairo_set_line_width(cr, 1.0);
      cairo_stroke(cr);
      if (mEntryFresh && !mEntryText.empty()) {
         // Selected: the text sits on an accent bar, as a text field shows a
         // selection, so it is clear that typing replaces it.
         setColor(cr, kAccent, 0.35);
         roundedRect(cr, f.x + 4, f.y + 4, f.w - 8, f.h - 8, 2);
         cairo_fill(cr);
      }
      setColor(cr, kText);
      const std::string shown = mEntryFresh ? mEntryText : mEntryText + "|";
      drawText(cr, f.x + f.w * 0.5, f.y + f.h * 0.5 + 4, shown.c_str(), 9.5, false, Align::Center);
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

      if (mEntryParam == static_cast<int>(id)) {
         drawEntryField(cr, r);
         return;
      }
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
      // The right-hand strip of the panel is the output meter; the droplet
      // history gets what is left.
      const double outW = 54;
      const double gw = mMeter.w - 2 * kPanelPad - outW;
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

      drawOutputMeter(cr, gx + gw + kPanelPad, gy, outW - kPanelPad, gh);
   }

   // Two vertical bars for the output peak. Scaled in dB, because a linear peak
   // meter spends nine tenths of its travel on the top 20 dB and tells you
   // nothing about a quiet drizzle.
   void drawOutputMeter(cairo_t *cr, double x, double y, double w, double h) {
      float peakL = 0.0f;
      float peakR = 0.0f;
      mDelegate.guiOutputPeaks(peakL, peakR);

      auto toBar = [](float peak) {
         if (peak <= 1.0e-5f)
            return 0.0;
         const double db = 20.0 * std::log10(static_cast<double>(peak));
         return std::min(1.0, std::max(0.0, (db + 60.0) / 60.0));
      };

      const double barW = (w - 4) * 0.5;
      const double values[2] = {toBar(peakL), toBar(peakR)};
      const float peaks[2] = {peakL, peakR};
      for (int ch = 0; ch < 2; ++ch) {
         const double bx = x + ch * (barW + 4);
         setColor(cr, kKnobFace);
         roundedRect(cr, bx, y, barW, h, 3);
         cairo_fill(cr);

         const double fh = values[ch] * (h - 4);
         if (fh > 0.5) {
            // Over -3 dBFS is worth seeing before the soft clipper is doing the
            // work for you.
            const bool hot = peaks[ch] > 0.708f;
            setColor(cr, hot ? kText : kAccent, hot ? 0.95 : 0.85);
            roundedRect(cr, bx + 2, y + h - 2 - fh, barW - 4, fh, 2);
            cairo_fill(cr);
         }
      }

      // -12 dBFS, the only gridline worth the ink at this size.
      setColor(cr, kTrack, 0.6);
      cairo_set_line_width(cr, 1.0);
      const double ly = std::floor(y + h - 2 - ((-12.0 + 60.0) / 60.0) * (h - 4)) + 0.5;
      cairo_move_to(cr, x, ly);
      cairo_line_to(cr, x + w, ly);
      cairo_stroke(cr);

      setColor(cr, kTextDim);
      const double loudest = std::max(values[0], values[1]);
      char label[32];
      if (loudest <= 0.0)
         std::snprintf(label, sizeof(label), "-inf");
      else
         std::snprintf(label, sizeof(label), "%.0f", 20.0 * std::log10(std::max(peakL, peakR)));
      drawText(cr, x + w * 0.5, y + h + 14, label, 9, false, Align::Center);
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
      grabKeyboard();
      if (!mKeyboardGrabbed)
         mSaveStatus = "Cannot reach the keyboard. SAVE stores it under this name.";
      mDirty = true;
   }

   // An embedded plugin window is not given the input focus by every host, and
   // there is no way in CLAP to ask for it. Bitwig does not hand it over, so
   // XSetInputFocus alone leaves the field unable to see a single keystroke.
   //
   // A grab does not depend on the host at all, and a modal dialog is the one
   // situation that genuinely warrants one: it lasts only while the field is
   // open, and taking the keyboard is exactly what being modal means. The focus
   // request is still made first, because where it does work it is the better
   // behaved of the two and leaves the host's own shortcuts alone.
   //
   // Failing to get the grab is not an error worth propagating: another client
   // may hold one. The dialog says so and saving under the offered name still
   // works with the mouse.
   void grabKeyboard() {
      if (!mWindow || mKeyboardGrabbed)
         return;
#if defined(_WIN32)
      // Windows routes keys to whichever window holds the focus, and a child
      // window is allowed to take it, so there is nothing here to grab. That is
      // why this is not the ugly compromise it has to be on X11: the host keeps
      // its shortcuts, and focus returns on its own when the dialog closes.
      mPrevFocus = SetFocus(mWindow);
      mKeyboardGrabbed = GetFocus() == mWindow;
#else
      if (!mDisplay)
         return;
      XErrorHandler previous = XSetErrorHandler(&ignoreXError);
      XSetInputFocus(mDisplay, mWindow, RevertToParent, CurrentTime);
      mKeyboardGrabbed = XGrabKeyboard(mDisplay, mWindow, True, GrabModeAsync, GrabModeAsync,
                                       CurrentTime) == GrabSuccess;
      XSync(mDisplay, False);
      XSetErrorHandler(previous);
#endif
   }

   void releaseKeyboard() {
      if (!mKeyboardGrabbed)
         return;
#if defined(_WIN32)
      if (mPrevFocus && IsWindow(mPrevFocus))
         SetFocus(mPrevFocus);
      mPrevFocus = nullptr;
#else
      if (mDisplay) {
         XErrorHandler previous = XSetErrorHandler(&ignoreXError);
         XUngrabKeyboard(mDisplay, CurrentTime);
         XSync(mDisplay, False);
         XSetErrorHandler(previous);
      }
#endif
      mKeyboardGrabbed = false;
   }

   // Every path that leaves the dialog has to come through here. A keyboard
   // grab that outlives its dialog would leave the whole desktop unable to type
   // until the plugin is unloaded.
   void closeSaveDialog() {
      mSaveOpen = false;
      releaseKeyboard();
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
      closeSaveDialog();
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

   // `None` is taken: X11 defines it as a macro, the same trap as Widget.

   void onSaveKey(KeyCommand cmd, const char *text, int textLen) {
      if (cmd == KeyCommand::Escape) {
         closeSaveDialog();
         return;
      }
      if (cmd == KeyCommand::Accept) {
         commitSave();
         return;
      }
      if (cmd == KeyCommand::Backspace) {
         if (!mSaveName.empty())
            mSaveName.pop_back();
         mSaveStatus.clear();
         mSaveFailed = false;
         mDirty = true;
         return;
      }
      for (int i = 0; i < textLen; ++i) {
         const unsigned char c = static_cast<unsigned char>(text[i]);
         if (c >= 0x20 && c != 0x7F && mSaveName.size() < 48)
            mSaveName.push_back(static_cast<char>(c));
      }
      mSaveStatus.clear();
      mSaveFailed = false;
      mDirty = true;
   }

   // A keystroke while a list is open. Up and down move through a selector
   // list or scroll the browser, Return and Escape close them, and anything
   // else dismisses whatever is open, which is what a key did here before the
   // lists learnt to navigate. Whether keys arrive at all is up to the host:
   // an embedded window is not given the focus by every one of them.
   void onOverlayKey(KeyCommand cmd) {
      if (mMenuParam >= 0) {
         if (cmd == KeyCommand::Up || cmd == KeyCommand::Down) {
            stepMenu(cmd == KeyCommand::Up ? -1 : 1);
            return;
         }
         closeMenu();
         mDirty = true;
         return;
      }
      if (mBrowserOpen) {
         if (cmd == KeyCommand::Up || cmd == KeyCommand::Down) {
            scrollBrowser(cmd == KeyCommand::Up ? -1 : 1);
            return;
         }
         mBrowserOpen = false;
         mDirty = true;
      }
   }

   // Moves the open selector list's value by one entry, clamped at the ends.
   void stepMenu(int direction) {
      if (mMenuParam < 0)
         return;
      const uint32_t id = static_cast<uint32_t>(mMenuParam);
      const ParamDesc &d = paramTable()[id];
      const int cur = static_cast<int>(std::floor(mDelegate.guiParamValue(id) + 0.5));
      const int next = std::min(static_cast<int>(d.enumCount) - 1, std::max(0, cur + direction));
      if (next == cur)
         return;
      mDelegate.guiBeginEdit(id);
      mDelegate.guiSetParam(id, static_cast<double>(next));
      mDelegate.guiEndEdit(id);
      mMenuHover = next;
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
               "control. Click a menu to pick from the list, its arrows to step. Click a value to type one.";

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

   // Rows that fit in the panel. A library larger than that scrolls, one row
   // per wheel step; the factory set fits without scrolling.
   int browserVisibleRows() const {
      const Rect p = browserPanel();
      return std::max(1, static_cast<int>((p.h - 40 - kBrowserPad) / kBrowserRowH));
   }

   int browserMaxScroll() const { return std::max(0, browserRows() - browserVisibleRows()); }

   void scrollBrowser(int rows) {
      const int next = std::min(browserMaxScroll(), std::max(0, mBrowserScroll + rows));
      if (next != mBrowserScroll) {
         mBrowserScroll = next;
         mDirty = true;
      }
   }

   void openBrowser() {
      mBrowserOpen = true;
      mBrowserHover = -1;
      // Open with the current preset in view.
      const int cur = mDelegate.guiCurrentPreset();
      const int row = cur >= 0 ? cur / kBrowserCols : 0;
      mBrowserScroll = std::min(browserMaxScroll(),
                                std::max(0, row - browserVisibleRows() / 2));
      closeMenu();
      mDirty = true;
   }

   // Row-major, so scrolling by rows keeps every column moving together. The
   // row is relative to the scroll position; a negative or too-large row is a
   // preset that is currently out of view and browserItemVisible() says so.
   Rect browserItemRect(int index) const {
      const Rect p = browserPanel();
      const int col = index % kBrowserCols;
      const int row = index / kBrowserCols - mBrowserScroll;
      Rect r;
      r.w = (p.w - 2 * kBrowserPad - kBrowserScrollW) / kBrowserCols;
      r.h = kBrowserRowH;
      r.x = p.x + kBrowserPad + col * r.w;
      r.y = p.y + 40 + row * r.h;
      return r;
   }

   bool browserItemVisible(int index) const {
      const int row = index / kBrowserCols - mBrowserScroll;
      return row >= 0 && row < browserVisibleRows();
   }

   Rect browserScrollbar() const {
      const Rect p = browserPanel();
      Rect r;
      r.w = kBrowserScrollW - 6;
      r.x = p.x + p.w - kBrowserPad - r.w;
      r.y = p.y + 40;
      r.h = browserVisibleRows() * kBrowserRowH;
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
      drawText(cr, p.x + p.w - kBrowserPad, p.y + 24,
               browserMaxScroll() > 0 ? "click to load, wheel to scroll, click outside to close"
                                      : "click to load, or click outside to close",
               9, false, Align::Right);

      const auto &list = mDelegate.guiPresets();
      const int cur = mDelegate.guiCurrentPreset();
      for (size_t i = 0; i < list.size(); ++i) {
         if (!browserItemVisible(static_cast<int>(i)))
            continue;
         const Rect r = browserItemRect(static_cast<int>(i));
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

      // A scrollbar only when there is something to scroll: a track with a
      // thumb whose length is the visible fraction of the list.
      if (browserMaxScroll() > 0) {
         const Rect sb = browserScrollbar();
         setColor(cr, kText, 0.08);
         roundedRect(cr, sb.x, sb.y, sb.w, sb.h, 3);
         cairo_fill(cr);
         const double frac = browserVisibleRows() / static_cast<double>(browserRows());
         const double thumbH = std::max(18.0, sb.h * frac);
         const double thumbY =
            sb.y + (sb.h - thumbH) * (mBrowserScroll / static_cast<double>(browserMaxScroll()));
         setColor(cr, kAccent, 0.55);
         roundedRect(cr, sb.x, thumbY, sb.w, thumbH, 3);
         cairo_fill(cr);
      }
   }

   // ----------------------------------------------------------------- events

   // `None` is taken: X11 defines it as a macro.
   enum class Widget { NoWidget, Prev, Next, Name, Save };

#if defined(_WIN32)
   void pumpEvents() {
      MSG msg;
      while (mWindow && PeekMessageW(&msg, mWindow, 0, 0, PM_REMOVE)) {
         TranslateMessage(&msg);
         DispatchMessageW(&msg);
      }
   }

   // Called from wndProc, which is where Windows delivers what X11 hands over
   // through pumpEvents. Everything below this line is shared again.
   LRESULT handleMessage(UINT msg, WPARAM wp, LPARAM lp) {
      const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
      switch (msg) {
      case WM_PAINT: {
         PAINTSTRUCT ps;
         const HDC dc = BeginPaint(mWindow, &ps);
         mDirty = true; // a paint request is not conditional on anything
         paintToDC(dc);
         EndPaint(mWindow, &ps);
         return 0;
      }
      case WM_ERASEBKGND:
         return 1; // every pixel is painted; erasing only causes a flicker
      case WM_LBUTTONDOWN:
      case WM_RBUTTONDOWN: {
         SetCapture(mWindow);
         const unsigned button = msg == WM_LBUTTONDOWN ? 1u : 3u;
         onPointerDown(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), button, GetMessageTime(), shift);
         return 0;
      }
      case WM_LBUTTONUP:
      case WM_RBUTTONUP:
         ReleaseCapture();
         onButtonRelease();
         return 0;
      case WM_MOUSEWHEEL: {
         // Wheel coordinates arrive in screen space, unlike every other message.
         POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
         ScreenToClient(mWindow, &pt);
         const unsigned button = GET_WHEEL_DELTA_WPARAM(wp) > 0 ? 4u : 5u;
         onPointerDown(pt.x, pt.y, button, GetMessageTime(), shift);
         return 0;
      }
      case WM_MOUSEMOVE: {
         if (!mTrackingLeave) {
            TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, mWindow, 0};
            TrackMouseEvent(&tme);
            mTrackingLeave = true;
         }
         onMotion(GET_X_LPARAM(lp) / mScale, GET_Y_LPARAM(lp) / mScale, shift);
         return 0;
      }
      case WM_MOUSELEAVE:
         mTrackingLeave = false;
         if (mDrag < 0) {
            mHover = -1;
            mHoverWidget = Widget::NoWidget;
            mDirty = true;
         }
         return 0;
      case WM_KEYDOWN: {
         KeyCommand cmd = KeyCommand::NoCommand;
         if (wp == VK_ESCAPE)
            cmd = KeyCommand::Escape;
         else if (wp == VK_RETURN)
            cmd = KeyCommand::Accept;
         else if (wp == VK_BACK)
            cmd = KeyCommand::Backspace;
         else if (wp == VK_UP)
            cmd = KeyCommand::Up;
         else if (wp == VK_DOWN)
            cmd = KeyCommand::Down;
         if (!mSaveOpen && mEntryParam < 0) {
            onOverlayKey(cmd);
            return 0;
         }
         if (cmd != KeyCommand::NoCommand) {
            if (mSaveOpen)
               onSaveKey(cmd, nullptr, 0);
            else
               onEntryKey(cmd, nullptr, 0);
         }
         return 0; // the text itself arrives as WM_CHAR
      }
      case WM_CHAR: {
         if (!mSaveOpen && mEntryParam < 0)
            return 0;
         const char c = static_cast<char>(wp);
         if (static_cast<unsigned char>(c) >= 0x20 && c != 0x7F) {
            if (mSaveOpen)
               onSaveKey(KeyCommand::NoCommand, &c, 1);
            else
               onEntryKey(KeyCommand::NoCommand, &c, 1);
         }
         return 0;
      }
      default:
         break;
      }
      return DefWindowProcW(mWindow, msg, wp, lp);
   }
#else
   void pumpEvents() {
      XEvent ev;
      while (XPending(mDisplay)) {
         XNextEvent(mDisplay, &ev);
         switch (ev.type) {
         case Expose:
            mDirty = true;
            break;
         case ButtonPress:
            onPointerDown(ev.xbutton.x, ev.xbutton.y, ev.xbutton.button, ev.xbutton.time,
                          (ev.xbutton.state & ShiftMask) != 0);
            break;
         case ButtonRelease:
            onButtonRelease();
            break;
         case MotionNotify:
            onMotion(ev.xmotion.x / mScale, ev.xmotion.y / mScale,
                     (ev.xmotion.state & ShiftMask) != 0);
            break;
         case LeaveNotify:
            if (mDrag < 0) {
               mHover = -1;
               mHoverWidget = Widget::NoWidget;
               mDirty = true;
            }
            break;
         case KeyPress: {
            char buf[32];
            KeySym sym = 0;
            const int n = XLookupString(&ev.xkey, buf, sizeof(buf) - 1, &sym, nullptr);
            KeyCommand cmd = KeyCommand::NoCommand;
            if (sym == XK_Escape)
               cmd = KeyCommand::Escape;
            else if (sym == XK_Return || sym == XK_KP_Enter)
               cmd = KeyCommand::Accept;
            else if (sym == XK_BackSpace)
               cmd = KeyCommand::Backspace;
            else if (sym == XK_Up || sym == XK_KP_Up)
               cmd = KeyCommand::Up;
            else if (sym == XK_Down || sym == XK_KP_Down)
               cmd = KeyCommand::Down;
            if (mSaveOpen)
               onSaveKey(cmd, buf, n);
            else if (mEntryParam >= 0)
               onEntryKey(cmd, buf, n);
            else
               onOverlayKey(cmd);
            break;
         }
         case ClientMessage:
            if (static_cast<Atom>(ev.xclient.data.l[0]) == mDeleteAtom)
               hide();
            break;
         default:
            break;
         }
      }
   }
#endif

   int cellAt(double x, double y) const {
      for (size_t i = 0; i < mCellRects.size(); ++i)
         if (mCellRects[i].contains(x, y))
            return static_cast<int>(mCells[i].param);
      return -1;
   }

   // Buttons: 1 left, 2 middle, 3 right, 4/5 wheel up/down, matching X11's
   // numbering because that is what the shared code below was written against.
   void onPointerDown(double px, double py, unsigned button, unsigned long timeMs,
                      bool shift) {
      const double x = px / mScale;
      const double y = py / mScale;
      const struct {
         unsigned button;
         unsigned long time;
      } be{button, timeMs};

      if (mSaveOpen) {
         if (be.button == kButtonLeft) {
            if (saveOkRect().contains(x, y))
               commitSave();
            else if (saveCancelRect().contains(x, y) || !savePanel().contains(x, y))
               closeSaveDialog();
         }
         mDirty = true;
         return;
      }

      if (mEntryParam >= 0) {
         const bool inside = valueRect(cellRectFor(static_cast<uint32_t>(mEntryParam))).contains(x, y);
         const bool second = be.button == kButtonLeft && mLastClickParam == mEntryParam &&
                             be.time - mLastClickTime < 400;
         if (inside && !second)
            return; // a click in the field it is typing into
         // Anywhere else cancels the entry, and the click then does what it
         // would have done; a double-click on the value falls through to the
         // reset it has always been.
         closeEntry();
      }

      if (mMenuParam >= 0) {
         if (be.button == kWheelUp || be.button == kWheelDown) {
            stepMenu(be.button == kWheelUp ? -1 : 1);
            return;
         }
         if (be.button == kButtonLeft) {
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
         if (be.button == kWheelUp || be.button == kWheelDown) {
            scrollBrowser(be.button == kWheelUp ? -1 : 1);
            return;
         }
         if (be.button == kButtonLeft) {
            const int item = browserItemAt(x, y);
            if (item >= 0)
               mDelegate.guiLoadPreset(item);
            if (item >= 0 || !browserPanel().contains(x, y))
               mBrowserOpen = false;
         }
         mDirty = true;
         return;
      }

      if (be.button == kWheelUp || be.button == kWheelDown) {
         const int id = cellAt(x, y);
         if (id >= 0)
            nudge(static_cast<uint32_t>(id), be.button == kWheelUp ? 1 : -1,
                  shift);
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
         openBrowser();
         return;
      }

      const int id = cellAt(x, y);
      if (id < 0)
         return;
      const ParamDesc &d = paramTable()[static_cast<uint32_t>(id)];

      // Right-click and double-click both mean "put it back where it was".
      const bool doubleClick =
         be.button == kButtonLeft && mLastClickParam == id && be.time - mLastClickTime < 400;
      mLastClickParam = id;
      mLastClickTime = be.time;

      if (be.button == kButtonRight || (doubleClick && !isChip(d))) {
         mDelegate.guiBeginEdit(static_cast<uint32_t>(id));
         mDelegate.guiSetParam(static_cast<uint32_t>(id), d.def);
         mDelegate.guiEndEdit(static_cast<uint32_t>(id));
         mLastClickParam = -1;
         mDirty = true;
         return;
      }
      if (be.button != kButtonLeft)
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

      if (be.button == kButtonLeft &&
          valueRect(cellRectFor(static_cast<uint32_t>(id))).contains(x, y)) {
         openEntry(static_cast<uint32_t>(id));
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

   void onMotion(double x, double y, bool shift) {
      if (mDrag >= 0) {
         const uint32_t id = static_cast<uint32_t>(mDrag);
         const ParamDesc &d = paramTable()[id];
         const double span = d.max - d.min;
         const double fine = shift ? 0.2 : 1.0;
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
      for (size_t i = 0; i < list.size(); ++i) {
         if (!browserItemVisible(static_cast<int>(i)))
            continue;
         if (browserItemRect(static_cast<int>(i)).contains(x, y))
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
   static constexpr int kBrowserScrollW = 14;

   GuiDelegate &mDelegate;

#if defined(_WIN32)
   HWND mWindow = nullptr;
   HWND mPrevFocus = nullptr;
   bool mTrackingLeave = false;
#else
   Display *mDisplay = nullptr;
   Window mWindow = 0;
   Visual *mVisual = nullptr;
   Atom mDeleteAtom = 0;
#endif
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
   unsigned long mLastClickTime = 0; // X11 Time and Win32 GetMessageTime alike

   bool mBrowserOpen = false;
   int mBrowserHover = -1;
   int mBrowserScroll = 0; // first visible row
   bool mSaveOpen = false;
   int mEntryParam = -1; // knob whose value is being typed, or -1
   std::string mEntryText;
   bool mEntryFailed = false;
   bool mEntryFresh = false; // the current value is shown selected; typing replaces it
   bool mKeyboardGrabbed = false;
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

#if defined(_WIN32)
// Windows hands the object back through the window's user data; the pointer is
// planted when CreateWindowEx delivers WM_NCCREATE.
LRESULT CALLBACK X11Gui::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
   if (msg == WM_NCCREATE) {
      auto *cs = reinterpret_cast<CREATESTRUCTW *>(lp);
      SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                        reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
      auto *self = static_cast<X11Gui *>(cs->lpCreateParams);
      if (self)
         self->mWindow = hwnd;
      return DefWindowProcW(hwnd, msg, wp, lp);
   }
   auto *self = reinterpret_cast<X11Gui *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
   if (!self || !self->mWindow)
      return DefWindowProcW(hwnd, msg, wp, lp);
   return self->handleMessage(msg, wp, lp);
}
#endif

Gui *createGui(GuiDelegate &delegate) {
   auto *gui = new X11Gui(delegate);
   if (!gui->open()) {
      delete gui;
      return nullptr;
   }
   return gui;
}

} // namespace rainyday
