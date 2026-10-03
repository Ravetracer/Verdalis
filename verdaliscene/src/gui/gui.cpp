// VerdaliScene's window.
//
// It is the suite's window -- the same knobs, chips, typed entry, preset
// browser and save dialog, from shared/ -- with two things added on top: a row
// of tabs, one for the scene and one for each layer, and a scene page whose
// centre is a mixer with a strip per layer.
//
// A layer's page is that plugin's own panel layout, from the plugin's own
// gui.cpp, with every parameter id moved to the layer's slot and the window's
// accent switched to the plugin's. Nothing here describes RainyDay's panels:
// when RainyDay's window changes, the rain page changes with it.
//
// Two preset bars share the shared browser and save dialog. The one at the
// bottom is the scene's; the one at the top of a layer page is that layer's,
// and its library is the plugin's own. A small proxy between the window and
// the plugin says which library a preset call is about.

#include "gui/gui.h"

#include "verdalis/gui/plugin_window.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "layers.h"
#include "params.h"
#include "verdaliscene.h"

namespace verdaliscene {

namespace {

using verdalis::Align;
using verdalis::Cell;
using verdalis::GuiDelegate;
using verdalis::HeaderOrnament;
using verdalis::OrnamentContext;
using verdalis::PanelSpec;
using verdalis::PluginWindow;
using verdalis::Rect;
using verdalis::Rgb;
using verdalis::Theme;
using verdalis::WindowSpec;
using verdalis::drawText;
using verdalis::drawTriangle;
using verdalis::kBarH;
using verdalis::kCellH;
using verdalis::kCellW;
using verdalis::kGap;
using verdalis::kHeaderH;
using verdalis::kHelpH;
using verdalis::kMargin;
using verdalis::kMenuPad;
using verdalis::kMenuRowH;
using verdalis::kPanelPad;
using verdalis::kPanelTitleH;
using verdalis::normalised;
using verdalis::roundedRect;
using verdalis::setColor;
using verdalis::textWidth;
using verdalis::upperCase;

// -------------------------------------------------------------------- theme
//
// Meadow. Every other accent in the suite belongs to one phenomenon; this one
// is the ground they all happen on, a yellow-green at hue 84 that sits between
// InsectSwarm's acid yellow (56) and RiverFlow's river green (138) without
// being mistakable for either. The greys lean towards it only slightly,
// because a layer's page swaps the accent for its own plugin's and the
// chassis has to carry rain blue and ember orange equally well.
constexpr Theme kTheme = {
   /* bgTop     */ {0.086, 0.100, 0.090},
   /* bgBottom  */ {0.052, 0.062, 0.055},
   /* panelFill */ {0.118, 0.137, 0.122},
   /* panelEdge */ {0.173, 0.200, 0.178},
   /* knobFace  */ {0.063, 0.075, 0.066},
   /* track     */ {0.192, 0.224, 0.198},
   /* accent    */ {0.651, 0.827, 0.353},
   /* text      */ {0.906, 0.925, 0.902},
   /* textDim   */ {0.518, 0.557, 0.522},
   /* textMute  */ {0.369, 0.404, 0.373},
   /* highlight */ {0.906, 0.925, 0.902},
};

// ------------------------------------------------------------------ geometry

constexpr int kTabH = 28;
constexpr int kTabY = kHeaderH + kGap;
constexpr int kPageTop = kTabY + kTabH + kGap;
// A layer page opens with that layer's own bar: its preset and its place in
// the scene.
constexpr int kLayerPanelsTop = kPageTop + kBarH + kGap;
// The scene page's lower row: two-row panels, as tall as one of a plugin's.
constexpr int kSceneRowH = kPanelTitleH + 2 * kCellH + kPanelPad;
// The mixer is never shorter than this, whatever the plugins' layouts allow.
constexpr int kMinMixerH = 420;

constexpr double kStripMaxW = 84.0;
constexpr double kStripMinW = 44.0;
constexpr double kMasterW = 96.0;
constexpr double kAddW = 64.0;
constexpr double kMixPad = 12.0;
constexpr double kStripButtonH = 18.0;

// What every page has to fit in: as wide as the widest plugin window and as
// tall as the tallest, plus the tabs and the layer bar above them.
struct Geometry {
   int contentW = 1100;
   int pageBottom = 0;
   int windowH = 0;
};

int rowsHeight(const WindowSpec &s) {
   int h = 0;
   for (int row = 0; row < s.rowCount; ++row) {
      int rowH = 0;
      for (int i = 0; i < s.rowLength[row]; ++i)
         rowH = std::max(rowH, verdalis::panelHeight(s.panels[s.rowStart[row] + i]));
      h += rowH + kGap;
   }
   return h;
}

const Geometry &geometry() {
   static const Geometry g = [] {
      Geometry out;
      out.pageBottom = kPageTop + kMinMixerH + kGap + kSceneRowH + kGap;
      for (int t = 0; t < kNumLayerTypes; ++t) {
         const WindowSpec s = layerType(t).windowSpec();
         out.contentW = std::max(out.contentW, s.contentW);
         out.pageBottom = std::max(out.pageBottom, kLayerPanelsTop + rowsHeight(s));
      }
      out.windowH = out.pageBottom + 2 + kBarH + 4 + kHelpH + 8;
      return out;
   }();
   return g;
}

// The scene page's own panels, under the mixer. Width and Output Gain are on
// the mixer's master strip too: the strip is where they are balanced against
// the layers, the panel is where they sit with the rest of the scene's own
// controls.
constexpr uint32_t kEnvParams[] = {kParamGate, kParamAttack, kParamDecay, kParamSustain,
                                   kParamRelease};
constexpr uint32_t kFilterParams[] = {kParamFilterType, kParamHighpass, kParamFilterCutoff,
                                      kParamFilterReso};
constexpr uint32_t kOutParams[] = {kParamWidth, kParamGain};

#define PANEL(title, cols, rows, arr)                                                              \
   { title, cols, rows, arr, static_cast<int>(sizeof(arr) / sizeof(arr[0])) }
const PanelSpec kScenePanels[] = {
   PANEL("ENVELOPE", 3, 2, kEnvParams),
   PANEL("FILTER", 2, 2, kFilterParams),
   PANEL("OUTPUT", 1, 2, kOutParams),
};
#undef PANEL
constexpr int kNumScenePanels = static_cast<int>(sizeof(kScenePanels) / sizeof(kScenePanels[0]));
static_assert(kNumSceneParams == 5 + 4 + 2, "every scene parameter must be on a scene panel");

Rgb accentOf(int type) {
   const LayerType &t = layerType(type);
   return {t.accent[0], t.accent[1], t.accent[2]};
}

// ------------------------------------------------------------------ the proxy
//
// The shared window talks to one GuiDelegate with one preset library. This is
// that delegate: parameters and meters go to the plugin as they are, and the
// preset calls go to whichever library `target` names.
class SceneProxy final : public GuiDelegate {
public:
   explicit SceneProxy(SceneDelegate &scene) : mScene(&scene) {}

   int target = kSceneTarget; // whose library the preset calls are about
   int page = kSceneTarget;   // whose meters the activity panel shows

   double guiParamValue(uint32_t id) const override { return mScene->paramValue(id); }
   void guiBeginEdit(uint32_t id) override { mScene->beginEdit(id); }
   void guiSetParam(uint32_t id, double value) override { mScene->setParam(id, value); }
   void guiEndEdit(uint32_t id) override { mScene->endEdit(id); }

   void guiOutputPeaks(float &left, float &right) const override {
      if (page < 0)
         mScene->outputPeaks(left, right);
      else
         mScene->layerPeaks(page, left, right);
   }

   // The scene page counts every layer's voices against every layer's pool,
   // and its event count is how many layers there are.
   uint32_t guiVoiceCount() const override {
      if (page >= 0)
         return mScene->layerVoices(page);
      uint32_t n = 0;
      for (int s = 0; s < kNumSlots; ++s)
         if (mScene->layerActive(s))
            n += mScene->layerVoices(s);
      return n;
   }

   uint32_t guiVoiceLimit() const override {
      if (page >= 0)
         return mScene->layerVoiceLimit(page);
      uint32_t n = 0;
      for (int s = 0; s < kNumSlots; ++s)
         if (mScene->layerActive(s))
            n += mScene->layerVoiceLimit(s);
      return std::max<uint32_t>(n, 1);
   }

   uint32_t guiEventCounter() const override {
      if (page >= 0)
         return mScene->layerEvents(page);
      uint32_t n = 0;
      for (int s = 0; s < kNumSlots; ++s)
         n += mScene->layerActive(s) ? 1 : 0;
      return n;
   }

   const std::vector<GuiPreset> &guiPresets() const override { return mScene->presets(target); }
   int guiCurrentPreset() const override { return mScene->currentPreset(target); }
   bool guiPresetEdited() const override { return mScene->presetEdited(target); }
   void guiLoadPreset(int index) override { mScene->loadPreset(target, index); }
   std::string guiSuggestedPresetName() const override {
      return mScene->suggestedPresetName(target);
   }
   bool guiSavePreset(const std::string &name, std::string &error) override {
      return mScene->savePreset(target, name, error);
   }
   bool guiPresetFoldersSupported() const override { return true; }
   std::vector<std::string> guiPresetPacks() const override { return mScene->presetPacks(target); }
   std::string guiPackPathFor(const std::string &folder) const override {
      return mScene->packPathFor(target, folder);
   }
   bool guiExportPack(const std::string &folder, const std::string &path,
                      std::string &error) override {
      return mScene->exportPack(target, folder, path, error);
   }
   bool guiImportPack(const std::string &path, std::string &folder, std::string &error) override {
      return mScene->importPack(target, path, folder, error);
   }

private:
   SceneDelegate *mScene;
};

// Holds the proxy so it is built before the window that refers to it: a base
// class is constructed before any member, so the proxy has to be a base too.
struct ProxyHolder {
   explicit ProxyHolder(SceneDelegate &scene) : mProxy(scene) {}
   SceneProxy mProxy;
};

// --------------------------------------------------------------- the ornament
//
// The scene page's header: one ribbon per layer, in that layer's colour,
// swaying as hard as the layer is loud. On a layer's own page the header is
// that plugin's own ornament instead.
class Ribbons final : public HeaderOrnament {
public:
   explicit Ribbons(SceneDelegate &scene) : mScene(scene) {}

   bool animating(uint32_t layers) const override { return layers > 0; }

   void draw(cairo_t *cr, const OrnamentContext &ctx) override {
      mPhase += 0.035;
      int count = 0;
      for (int s = 0; s < kNumSlots; ++s)
         count += mScene.layerActive(s) ? 1 : 0;
      if (count == 0)
         return;
      int k = 0;
      for (int s = 0; s < kNumSlots; ++s) {
         if (!mScene.layerActive(s))
            continue;
         float l = 0.0f, r = 0.0f;
         mScene.layerPeaks(s, l, r);
         const double peak = std::max(l, r);
         const double level =
            peak > 1.0e-5 ? std::min(1.0, std::max(0.0, (20.0 * std::log10(peak) + 60.0) / 60.0))
                          : 0.0;
         const double y0 = ctx.headerH * (0.2 + 0.6 * (k + 0.5) / count);
         const double amp = 2.0 + 10.0 * level;
         const double freq = 0.006 + 0.002 * (s % 5);
         const double speed = 0.6 + 0.15 * (s % 7);
         setColor(cr, accentOf(slotType(s)), 0.10 + 0.22 * level);
         cairo_set_line_width(cr, 1.2);
         cairo_new_path(cr);
         for (double x = 0.0; x <= ctx.windowW + 8.0; x += 8.0) {
            const double y = y0 + amp * std::sin(x * freq + mPhase * speed + s * 1.7) +
                             0.4 * amp * std::sin(x * freq * 2.3 - mPhase * speed * 0.7);
            if (x == 0.0)
               cairo_move_to(cr, x, y);
            else
               cairo_line_to(cr, x, y);
         }
         cairo_stroke(cr);
         ++k;
      }
   }

private:
   SceneDelegate &mScene;
   double mPhase = 0.0;
};

// The parameter table as the window shows it. The host's names carry the
// layer they belong to -- "River 1 Water" -- because a host lists two thousand
// of them in one flat list; on a layer's own page that prefix is noise, so the
// window draws each with its plugin's own name. Ids, ranges and tips are the
// host's table's, untouched.
const ParamDesc *displayTable() {
   static const std::vector<ParamDesc> table = [] {
      std::vector<ParamDesc> out(fullTable(), fullTable() + kTableSize);
      for (int slot = 0; slot < kNumSlots; ++slot) {
         const LayerType &t = layerType(slotType(slot));
         for (uint32_t i = 0; i < t.paramCount; ++i)
            out[slotParamId(slot, i)].name = t.paramTable()[i].name;
         for (uint32_t m = 0; m < kNumSlotParams; ++m)
            out[slotMixId(slot, m)].name = slotParamTable()[m].name;
      }
      return out;
   }();
   return table.data();
}

// --------------------------------------------------------------- the window

WindowSpec sceneSpec() {
   WindowSpec spec{};
   spec.wordmarkFirst = "Verdali";
   spec.wordmarkSecond = "Scene";
   spec.subtitle = "NATURE SCENE INSTRUMENT";
   spec.version = kPluginVersion;
   spec.voiceNoun = "voices";
   spec.eventNoun = "layers";
   spec.theme = kTheme;
   // The base lays these out once in its constructor, before this window has
   // a say; buildLayout() below replaces it at once.
   spec.panels = kScenePanels;
   spec.panelCount = 0;
   spec.rowStart = nullptr;
   spec.rowLength = nullptr;
   spec.rowCount = 0;
   spec.contentW = geometry().contentW;
   spec.windowH = geometry().windowH;
   spec.params = displayTable();
   spec.paramCount = kTableSize;
   spec.mixer = nullptr;
   spec.mixerCount = 0;
   spec.ornament = nullptr;
   return spec;
}

class SceneWindow final : private ProxyHolder, public PluginWindow {
public:
   explicit SceneWindow(SceneDelegate &scene)
      : ProxyHolder(scene), PluginWindow(mProxy, sceneSpec()), mScene(scene) {
      for (int t = 0; t < kNumLayerTypes; ++t) {
         mTypeSpecs[t] = layerType(t).windowSpec();
         mTypeOrnaments[t] = nullptr;
      }
      mSceneOrnament = new Ribbons(scene);
      syncLayers(true);
      showPage(kSceneTarget);
   }

   // The ornaments are this window's, not the base's: the base deletes
   // whatever mSpec.ornament points at, so it is cleared first.
   ~SceneWindow() override {
      mSpec.ornament = nullptr;
      delete mSceneOrnament;
      for (HeaderOrnament *o : mTypeOrnaments)
         delete o;
   }

private:
   // What a click or a hover on this window's own widgets lands on. The base
   // window's widgets -- cells, the bottom bar, the overlays -- are its own.
   enum HitKind {
      kHitNothing,
      kHitTab,
      kHitTabClose,
      kHitAddTab,
      kHitStripLabel,
      kHitStripFader,
      kHitStripPan,
      kHitStripStereo,
      kHitStripMute,
      kHitStripSolo,
      kHitMasterFader,
      kHitMasterWidth,
      kHitAddStrip,
      kHitLayerPrev,
      kHitLayerName,
      kHitLayerNext,
      kHitLayerSave,
      kHitLayerLevel,
      kHitLayerPan,
      kHitLayerStereo,
      kHitLayerShot,
      kHitLayerMute,
      kHitLayerSolo,
      kHitLayerRemove,
   };

   struct Hit {
      HitKind kind = kHitNothing;
      int slot = -1;
      bool operator==(const Hit &o) const { return kind == o.kind && slot == o.slot; }
      bool operator!=(const Hit &o) const { return !(*this == o); }
   };

   struct Tab {
      int slot;
      Rect r;
      Rect close;
   };

   struct Strip {
      int slot;
      Rect r;
      Rect label;
      Rect fader;
      Rect pan;
      Rect stereo;
      Rect mute;
      Rect solo;
   };

   // ------------------------------------------------------------- layers

   // Re-reads which layers exist. Anything that changes the set -- this window,
   // a preset, the host restoring a project -- shows up here on the next tick.
   void syncLayers(bool force) {
      uint64_t mask = 0;
      for (int s = 0; s < kNumSlots; ++s)
         if (mScene.layerActive(s))
            mask |= uint64_t{1} << s;
      if (!force && mask == mActiveMask)
         return;
      mActiveMask = mask;
      mLayers.clear();
      for (int s = 0; s < kNumSlots; ++s)
         if (mask & (uint64_t{1} << s))
            mLayers.push_back(s);
      if (mPage >= 0 && !(mask & (uint64_t{1} << mPage)))
         showPage(kSceneTarget);
      else
         buildLayout();
      mDirty = true;
   }

   void showPage(int page) {
      if (page != mPage) {
         closeEntry();
         closeMenu();
         mBrowserOpen = false;
         if (mSaveOpen)
            closeSaveDialog();
         mAddOpen = false;
         mDrag = -1;
      }
      mPage = page;
      mProxy.page = page;
      mOverlayTarget = kSceneTarget;
      mProxy.target = kSceneTarget;

      if (page < 0) {
         mSpec.theme = kTheme;
         mSpec.subtitle = "NATURE SCENE INSTRUMENT";
         mSpec.voiceNoun = "voices";
         mSpec.eventNoun = "layers";
         mSpec.ornament = mSceneOrnament;
      } else {
         const int type = slotType(page);
         const LayerType &t = layerType(type);
         const WindowSpec &ts = mTypeSpecs[type];
         mSpec.theme = kTheme;
         mSpec.theme.accent = ts.theme.accent;
         mSpec.theme.highlight = ts.theme.highlight;
         std::snprintf(mSubtitle, sizeof(mSubtitle), "%s %d  /  %s", t.label,
                       slotInstance(page) + 1, t.pluginName);
         upperCase(mSubtitle, mSubtitle, sizeof(mSubtitle));
         mSpec.subtitle = mSubtitle;
         mSpec.voiceNoun = ts.voiceNoun;
         mSpec.eventNoun = ts.eventNoun;
         if (!mTypeOrnaments[type] && t.createOrnament)
            mTypeOrnaments[type] = t.createOrnament();
         mSpec.ornament = mTypeOrnaments[type];
      }
      // The activity history belongs to whatever was on the page before.
      for (float &v : mHistory)
         v = 0.0f;
      mMeterFill = 0.0;
      mDecayFrames = 0;
      buildLayout();
      mDirty = true;
   }

   // --------------------------------------------------------------- layout

   void buildLayout() override {
      const Geometry &g = geometry();
      const double W = g.contentW;
      mPanels.clear();
      mCells.clear();
      mCellRects.clear();
      mTabs.clear();
      mStrips.clear();

      // ---- tabs
      double x = kMargin;
      mSceneTab = {x, static_cast<double>(kTabY), 84.0, static_cast<double>(kTabH)};
      x += mSceneTab.w + 4.0;
      const double avail = W - (x - kMargin) - 40.0;
      const double tabW =
         mLayers.empty() ? 0.0
                         : std::min(110.0, std::max(56.0, avail / mLayers.size() - 4.0));
      for (const int slot : mLayers) {
         Tab tab;
         tab.slot = slot;
         tab.r = {x, static_cast<double>(kTabY), tabW, static_cast<double>(kTabH)};
         tab.close = {x + tabW - 20.0, kTabY + (kTabH - 16.0) * 0.5, 16.0, 16.0};
         mTabs.push_back(tab);
         x += tabW + 4.0;
      }
      mAddTab = {x, static_cast<double>(kTabY), 32.0, static_cast<double>(kTabH)};

      if (mPage < 0)
         layoutScenePage(W, g);
      else
         layoutLayerPage(W);

      // ---- the scene's preset bar, where every Verdalis window has it
      const double barY = g.pageBottom + 2;
      mPrevRect = {static_cast<double>(kMargin) + 62, barY, 26, kBarH};
      mNameRect = {mPrevRect.x + mPrevRect.w + 4, barY, 300, kBarH};
      mNextRect = {mNameRect.x + mNameRect.w + 4, barY, 26, kBarH};
      mSaveRect = {mNextRect.x + mNextRect.w + 14, barY, 58, kBarH};
      mMixerRect = {mSaveRect.x + mSaveRect.w + 8, barY, 62, kBarH};
      mHoldRect = {mMixerRect.x + mMixerRect.w + 10, barY, 104, kBarH};
      mVersionRect = {kMargin + W - 56.0, 33.0, 56.0, 14.0};
      mBarY = static_cast<int>(barY);
      mHelpY = static_cast<int>(barY) + kBarH + 4;
   }

   void addPanelRow(const PanelSpec *specs, const int *indices, int count, double x0, double y,
                    double rightEdge, bool stretchLast, double &rowH) {
      double x = x0;
      rowH = 0.0;
      for (int i = 0; i < count; ++i) {
         const PanelSpec &spec = specs[indices[i]];
         Panel p;
         p.spec = &spec;
         p.rect.x = x;
         p.rect.y = y;
         p.rect.w = spec.cols * kCellW + 2 * kPanelPad;
         p.rect.h = kPanelTitleH + spec.rows * kCellH + kPanelPad;
         if (stretchLast && i == count - 1)
            p.rect.w = rightEdge - p.rect.x;
         rowH = std::max(rowH, p.rect.h);
         for (const Cell &cell : flowCells(spec)) {
            mCells.push_back(cell);
            mCellRects.push_back(cellRect(p, cell));
         }
         mPanels.push_back(p);
         x += p.rect.w + kGap;
      }
      mRowEndX = x;
   }

   void layoutScenePage(double W, const Geometry &g) {
      const double rowY = g.pageBottom - kGap - kSceneRowH;
      mMixerPanel = {static_cast<double>(kMargin), static_cast<double>(kPageTop), W,
                     rowY - kGap - kPageTop};

      const int indices[kNumScenePanels] = {0, 1, 2};
      double rowH = 0.0;
      addPanelRow(kScenePanels, indices, kNumScenePanels, kMargin, rowY, kMargin + W, false, rowH);
      mMeter = {mRowEndX, rowY, kMargin + W - mRowEndX, rowH};

      // ---- the mixer's strips, left to right in the tabs' order
      const Rect &p = mMixerPanel;
      const double top = p.y + kPanelTitleH + 6.0;
      const double h = p.h - kPanelTitleH - 6.0 - kMixPad;
      const double master = p.x + p.w - kMixPad - kMasterW;
      const double room = master - kMixPad - (p.x + kMixPad) - kAddW - 8.0;
      mStripW = mLayers.empty()
                   ? kStripMaxW
                   : std::max(kStripMinW, std::min(kStripMaxW, room / mLayers.size()));
      double x = p.x + kMixPad;
      for (const int slot : mLayers) {
         mStrips.push_back(stripAt(slot, x, top, mStripW, h));
         x += mStripW;
      }
      mAddStrip = {x + 4.0, top, kAddW, h};
      mMaster = stripAt(-1, master, top, kMasterW, h);
   }

   // One strip's controls, top to bottom: the name, the fader, its value, pan,
   // stereo or mono, and mute and solo.
   static Strip stripAt(int slot, double x, double top, double w, double h) {
      Strip s;
      s.slot = slot;
      s.r = {x, top, w, h};
      s.label = {x + 2.0, top, w - 4.0, 34.0};
      const double buttonsY = top + h - kStripButtonH;
      const double stereoY = buttonsY - 8.0 - kStripButtonH;
      const double panY = stereoY - 14.0 - 10.0;
      const double faderBottom = panY - 34.0;
      s.fader = {x + w * 0.5 - 6.0 - 4.0, top + 42.0, 12.0, faderBottom - (top + 42.0)};
      s.pan = {x + 8.0, panY, w - 16.0, 10.0};
      s.stereo = {x + 6.0, stereoY, w - 12.0, kStripButtonH};
      const double bw = std::min(26.0, (w - 14.0) * 0.5);
      s.mute = {x + w * 0.5 - 2.0 - bw, buttonsY, bw, kStripButtonH};
      s.solo = {x + w * 0.5 + 2.0, buttonsY, bw, kStripButtonH};
      return s;
   }

   void layoutLayerPage(double W) {
      const int slot = mPage;
      const int type = slotType(slot);
      const LayerType &t = layerType(type);
      const WindowSpec &ts = mTypeSpecs[type];

      // The plugin's panels with every id moved to this slot, leaving out
      // what the scene pins. Sized first: the specs point into these.
      mPageIds.assign(static_cast<size_t>(ts.panelCount), {});
      mPageSpecs.clear();
      for (int i = 0; i < ts.panelCount; ++i) {
         const PanelSpec &src = ts.panels[i];
         for (int c = 0; c < src.count; ++c)
            if (!pinnedParam(t, src.params[c]))
               mPageIds[static_cast<size_t>(i)].push_back(slotParamId(slot, src.params[c]));
      }
      for (int i = 0; i < ts.panelCount; ++i) {
         const PanelSpec &src = ts.panels[i];
         const auto &ids = mPageIds[static_cast<size_t>(i)];
         mPageSpecs.push_back({src.title, src.cols, src.rows, ids.data(),
                               static_cast<int>(ids.size())});
      }

      // The plugin's own layout, centred, at the width it was designed for.
      const double x0 = kMargin + std::floor((W - ts.contentW) * 0.5);
      double y = kLayerPanelsTop;
      for (int row = 0; row < ts.rowCount; ++row) {
         std::vector<int> indices;
         for (int i = 0; i < ts.rowLength[row]; ++i)
            indices.push_back(ts.rowStart[row] + i);
         const bool last = row == ts.rowCount - 1;
         double rowH = 0.0;
         addPanelRow(mPageSpecs.data(), indices.data(), static_cast<int>(indices.size()), x0, y,
                     x0 + ts.contentW, !last, rowH);
         if (last)
            mMeter = {mRowEndX, y, x0 + ts.contentW - mRowEndX, rowH};
         y += rowH + kGap;
      }

      // ---- the layer's bar: its preset on the left, its place on the right
      const double barY = kPageTop;
      const double labelW = std::max(60.0, std::strlen(t.pluginName) * 7.6 + 12.0);
      mLayerPrev = {kMargin + labelW, barY, 26, kBarH};
      mLayerName = {mLayerPrev.x + 30, barY, 260, kBarH};
      mLayerNext = {mLayerName.x + mLayerName.w + 4, barY, 26, kBarH};
      mLayerSave = {mLayerNext.x + mLayerNext.w + 14, barY, 58, kBarH};

      double right = kMargin + W;
      mLayerRemove = {right - 78.0, barY, 78.0, kBarH};
      right = mLayerRemove.x - 14.0;
      mLayerSolo = {right - 26.0, barY + 7.0, 26.0, kStripButtonH};
      mLayerMute = {mLayerSolo.x - 4.0 - 26.0, barY + 7.0, 26.0, kStripButtonH};
      right = mLayerMute.x - 16.0;
      const bool shot = t.shotLevelParam != kNoLayerParam;
      if (shot) {
         mLayerShot = {right - 110.0, barY + 11.0, 110.0, 10.0};
         right = mLayerShot.x - 46.0;
      } else {
         mLayerShot = {};
      }
      mLayerStereo = {right - 54.0, barY + 7.0, 54.0, kStripButtonH};
      right = mLayerStereo.x - 14.0;
      mLayerPan = {right - 96.0, barY + 11.0, 96.0, 10.0};
      right = mLayerPan.x - 34.0;
      mLayerLevel = {right - 120.0, barY + 11.0, 120.0, 10.0};
   }

   // ---------------------------------------------------------------- paint

   void drawFrame(cairo_t *cr) override {
      settleOverlayTarget();
      drawBackground(cr);
      drawHeader(cr);
      drawTabs(cr);
      for (const Panel &p : mPanels)
         drawPanel(cr, p);
      drawMeter(cr);
      if (mPage < 0)
         drawSceneMixer(cr);
      else
         drawLayerBar(cr);

      // The bottom bar is always the scene's, whatever the overlays are about.
      const int keep = mProxy.target;
      mProxy.target = kSceneTarget;
      drawPresetBar(cr);
      mProxy.target = mPage;
      const uint32_t sliding = mDrag >= 0 && !isCellParam(static_cast<uint32_t>(mDrag))
                                  ? static_cast<uint32_t>(mDrag)
                                  : hitParam(mHover2);
      if (sliding != verdalis::kNoParam)
         drawSliderHelp(cr, sliding);
      else if (!mHelp.empty())
         drawOwnHelp(cr);
      else
         drawHelpLine(cr);
      mProxy.target = keep;

      if (mBrowserOpen)
         drawBrowser(cr);
      if (mMenuParam >= 0)
         drawMenu(cr);
      if (mAddOpen)
         drawAddMenu(cr);
      if (mSaveOpen)
         drawSaveDialog(cr);
   }

   void drawOwnHelp(cairo_t *cr) {
      setColor(cr, mSpec.theme.textMute);
      drawText(cr, kMargin, mHelpY + kHelpH - 8, mHelp.c_str(), 10, false, Align::Left);
   }

   // A slider of this window's own has no value printed beside it, so the help
   // line says what it is set to while it is under the pointer or in the hand.
   void drawSliderHelp(cairo_t *cr, uint32_t id) {
      const ParamDesc &d = mSpec.params[id];
      char value[64];
      if (!paramValueToText(d, mDelegate.guiParamValue(id), value, sizeof(value)))
         std::snprintf(value, sizeof(value), "--");
      char head[160];
      std::snprintf(head, sizeof(head), "%s  %s", d.name, value);
      setColor(cr, mSpec.theme.accent);
      drawText(cr, kMargin, mHelpY + kHelpH - 8, head, 10, true, Align::Left);
      const double w = textWidth(cr, head, 10, true);
      setColor(cr, mSpec.theme.textMute);
      drawText(cr, kMargin + w + 14, mHelpY + kHelpH - 8, d.tip, 10, false, Align::Left);
   }

   const char *saveTitle() const override {
      if (mOverlayTarget < 0)
         return "SAVE SCENE";
      std::snprintf(mTitle, sizeof(mTitle), "SAVE %s PRESET",
                    layerType(slotType(mOverlayTarget)).pluginName);
      upperCase(mTitle, mTitle, sizeof(mTitle));
      return mTitle;
   }

   const char *browserTitle() const override {
      if (mOverlayTarget < 0)
         return "SCENES";
      std::snprintf(mTitle, sizeof(mTitle), "%s PRESETS",
                    layerType(slotType(mOverlayTarget)).pluginName);
      upperCase(mTitle, mTitle, sizeof(mTitle));
      return mTitle;
   }

   const char *presetBarLabel() const override { return "SCENE"; }

   void drawTabs(cairo_t *cr) {
      const Theme &th = mSpec.theme;
      auto tab = [&](const Rect &r, const char *label, const Rgb &accent, bool selected,
                     bool hot, bool dot) {
         setColor(cr, selected ? th.panelFill : th.knobFace);
         roundedRect(cr, r.x, r.y, r.w, r.h, 4);
         cairo_fill_preserve(cr);
         setColor(cr, selected || hot ? accent : th.panelEdge, selected ? 0.9 : (hot ? 0.6 : 1.0));
         cairo_set_line_width(cr, 1.0);
         cairo_stroke(cr);
         if (selected) {
            setColor(cr, accent);
            cairo_rectangle(cr, r.x + 4, r.y + r.h - 3, r.w - 8, 2);
            cairo_fill(cr);
         }
         double tx = r.x + 10;
         if (dot) {
            setColor(cr, accent, 0.9);
            cairo_arc(cr, r.x + 11, r.y + r.h * 0.5, 3.5, 0, 2 * M_PI);
            cairo_fill(cr);
            tx = r.x + 20;
         }
         setColor(cr, selected ? th.text : (hot ? th.text : th.textDim));
         cairo_save(cr);
         cairo_rectangle(cr, r.x, r.y, r.w - (dot ? 18 : 6), r.h);
         cairo_clip(cr);
         if (dot)
            drawText(cr, tx, r.y + 18, label, 9.5, true, Align::Left);
         else
            drawText(cr, r.x + r.w * 0.5, r.y + 18, label, 9.5, true, Align::Center);
         cairo_restore(cr);
      };

      tab(mSceneTab, "SCENE", kTheme.accent, mPage < 0,
          mHover2.kind == kHitTab && mHover2.slot < 0, false);
      for (const Tab &t : mTabs) {
         char label[32];
         std::snprintf(label, sizeof(label), "%s %d", layerType(slotType(t.slot)).label,
                       slotInstance(t.slot) + 1);
         upperCase(label, label, sizeof(label));
         const bool hot = (mHover2.kind == kHitTab || mHover2.kind == kHitTabClose) &&
                          mHover2.slot == t.slot;
         tab(t.r, label, accentOf(slotType(t.slot)), mPage == t.slot, hot, true);
         if (hot && mPage == t.slot && t.r.w >= 70.0) {
            const bool closeHot = mHover2.kind == kHitTabClose;
            setColor(cr, closeHot ? kMuteRed : th.textMute);
            const double cx = t.close.x + t.close.w * 0.5;
            const double cy = t.close.y + t.close.h * 0.5;
            cairo_set_line_width(cr, 1.5);
            cairo_move_to(cr, cx - 3.5, cy - 3.5);
            cairo_line_to(cr, cx + 3.5, cy + 3.5);
            cairo_move_to(cr, cx + 3.5, cy - 3.5);
            cairo_line_to(cr, cx - 3.5, cy + 3.5);
            cairo_stroke(cr);
         }
      }
      const bool addHot = mHover2.kind == kHitAddTab || (mAddOpen && mAddFromTab);
      setColor(cr, th.knobFace);
      roundedRect(cr, mAddTab.x, mAddTab.y, mAddTab.w, mAddTab.h, 4);
      cairo_fill_preserve(cr);
      setColor(cr, addHot ? kTheme.accent : th.panelEdge, addHot ? 0.8 : 1.0);
      cairo_set_line_width(cr, 1.0);
      cairo_stroke(cr);
      drawPlus(cr, mAddTab.x + mAddTab.w * 0.5, mAddTab.y + mAddTab.h * 0.5, 5.0,
               addHot ? kTheme.accent : th.textDim);
   }

   static void drawPlus(cairo_t *cr, double cx, double cy, double r, const Rgb &c) {
      setColor(cr, c);
      cairo_set_line_width(cr, 1.6);
      cairo_move_to(cr, cx - r, cy);
      cairo_line_to(cr, cx + r, cy);
      cairo_move_to(cr, cx, cy - r);
      cairo_line_to(cr, cx, cy + r);
      cairo_stroke(cr);
   }

   // A layer's level as a dB fraction of the meter, the way the output meter
   // shows it.
   static double meterLevel(float peak) {
      if (peak <= 1.0e-5f)
         return 0.0;
      const double db = 20.0 * std::log10(static_cast<double>(peak));
      return std::min(1.0, std::max(0.0, (db + 60.0) / 60.0));
   }

   void drawPeakBars(cairo_t *cr, const Rect &fader, float l, float r, const Rgb &accent) {
      const double x = fader.x + fader.w + 8.0;
      const double vals[2] = {meterLevel(l), meterLevel(r)};
      for (int ch = 0; ch < 2; ++ch) {
         const double bx = x + ch * 5.0;
         setColor(cr, mSpec.theme.knobFace);
         cairo_rectangle(cr, bx, fader.y, 3.0, fader.h);
         cairo_fill(cr);
         const double fh = vals[ch] * fader.h;
         if (fh > 0.5) {
            const bool hot = (ch ? r : l) > 0.708f;
            setColor(cr, hot ? mSpec.theme.text : accent, 0.85);
            cairo_rectangle(cr, bx, fader.y + fader.h - fh, 3.0, fh);
            cairo_fill(cr);
         }
      }
   }

   void drawSceneMixer(cairo_t *cr) {
      const Theme &th = mSpec.theme;
      const Rect &p = mMixerPanel;
      setColor(cr, th.panelFill);
      roundedRect(cr, p.x, p.y, p.w, p.h, 5);
      cairo_fill_preserve(cr);
      setColor(cr, th.panelEdge);
      cairo_set_line_width(cr, 1.0);
      cairo_stroke(cr);
      setColor(cr, th.accent, 0.85);
      drawText(cr, p.x + kPanelPad + 2, p.y + 16, "MIXER", 10, true, Align::Left);

      bool holds = false;
      for (const int s : mLayers)
         holds = holds || mScene.layerMuted(s) || mScene.layerSoloed(s);
      setColor(cr, th.textMute);
      drawText(cr, p.x + p.w - kPanelPad - 2, p.y + 16,
               holds ? "M and S are not saved with the scene"
                     : "One strip per layer. Click a name to open the layer.",
               9, false, Align::Right);

      for (const Strip &s : mStrips)
         drawStrip(cr, s);
      drawAddStrip(cr);
      drawMasterStrip(cr);

      if (mLayers.empty()) {
         setColor(cr, th.textDim);
         // Centred in the empty floor between the ADD strip and the master.
         const double cx = (mAddStrip.x + mAddStrip.w + mMaster.r.x) * 0.5;
         drawText(cr, cx, p.y + p.h * 0.45, "An empty scene.", 14, true, Align::Center);
         setColor(cr, th.textMute);
         drawText(cr, cx, p.y + p.h * 0.45 + 22,
                  "Add rain, thunder, waves, wind, birds, a river, fire, insects or the night",
                  10, false, Align::Center);
         drawText(cr, cx, p.y + p.h * 0.45 + 38,
                  "with + -- or pick a scene from the bar at the bottom.", 10, false,
                  Align::Center);
      }
   }

   void drawStrip(cairo_t *cr, const Strip &s) {
      const Theme &th = mSpec.theme;
      const int type = slotType(s.slot);
      const Rgb accent = accentOf(type);
      const bool hovered = mHover2.slot == s.slot && mHover2.kind >= kHitStripLabel &&
                           mHover2.kind <= kHitStripSolo;
      const bool muted = mScene.layerMuted(s.slot);
      bool anySolo = false;
      for (const int sl : mLayers)
         anySolo = anySolo || mScene.layerSoloed(sl);
      const bool live = anySolo ? mScene.layerSoloed(s.slot) : !muted;

      // Each strip in its own layer's colour: the base's fader and slider draw
      // in the window's accent, which is borrowed for the length of the strip.
      const Rgb keep = mSpec.theme.accent;
      mSpec.theme.accent = accent;

      setColor(cr, accent, live ? 0.9 : 0.35);
      cairo_rectangle(cr, s.r.x + 6, s.r.y, s.r.w - 12, 2.5);
      cairo_fill(cr);

      char label[32];
      std::snprintf(label, sizeof(label), "%s %d", layerType(type).label,
                    slotInstance(s.slot) + 1);
      upperCase(label, label, sizeof(label));
      const bool labelHot = mHover2.kind == kHitStripLabel && mHover2.slot == s.slot;
      setColor(cr, labelHot ? accent : (hovered ? th.text : th.textDim));
      cairo_save(cr);
      cairo_rectangle(cr, s.r.x + 1, s.r.y, s.r.w - 2, 40);
      cairo_clip(cr);
      drawText(cr, s.r.x + s.r.w * 0.5, s.r.y + 17, label, 9.0, true, Align::Center);
      // What the layer is playing, under its name.
      const std::vector<GuiPreset> &list = mScene.presets(s.slot);
      const int cur = mScene.currentPreset(s.slot);
      if (cur >= 0 && cur < static_cast<int>(list.size())) {
         setColor(cr, th.textMute);
         drawText(cr, s.r.x + s.r.w * 0.5, s.r.y + 30, list[static_cast<size_t>(cur)].name.c_str(),
                  8.0, false, Align::Center);
      }
      cairo_restore(cr);

      const uint32_t level = slotMixId(s.slot, kSlotLevel);
      const ParamDesc &ld = mSpec.params[level];
      const double raw = mDelegate.guiParamValue(level);
      drawFader(cr, s.fader, normalised(ld, raw), mHover2.kind == kHitStripFader && hovered, live);
      float pl = 0.0f, pr = 0.0f;
      mScene.layerPeaks(s.slot, pl, pr);
      drawPeakBars(cr, s.fader, pl, pr, accent);

      char text[64];
      if (!paramValueToText(ld, raw, text, sizeof(text)))
         std::snprintf(text, sizeof(text), "--");
      setColor(cr, th.text, live ? 0.9 : 0.5);
      drawText(cr, s.r.x + s.r.w * 0.5, s.fader.y + s.fader.h + 16, text, 9.0, false,
               Align::Center);

      const uint32_t pan = slotMixId(s.slot, kSlotPan);
      drawSlider(cr, s.pan, "", mSpec.params[pan], mDelegate.guiParamValue(pan),
                 mHover2.kind == kHitStripPan && hovered, live);

      const uint32_t stereo = slotMixId(s.slot, kSlotStereo);
      const bool mono = mDelegate.guiParamValue(stereo) >= 0.5;
      drawToggle(cr, s.stereo, mono ? "MONO" : "STEREO", mono,
                 mHover2.kind == kHitStripStereo && hovered, accent);

      drawMixerButton(cr, s.mute, "M", muted, mHover2.kind == kHitStripMute && hovered, kMuteRed);
      drawMixerButton(cr, s.solo, "S", mScene.layerSoloed(s.slot),
                      mHover2.kind == kHitStripSolo && hovered, accent);
      mSpec.theme.accent = keep;
   }

   // A two-state button: lit when it is on, the word saying which state.
   void drawToggle(cairo_t *cr, const Rect &r, const char *label, bool on, bool hot,
                   const Rgb &accent) {
      const Theme &th = mSpec.theme;
      setColor(cr, on ? accent : th.knobFace, on ? 0.22 : 1.0);
      roundedRect(cr, r.x, r.y, r.w, r.h, 3);
      cairo_fill_preserve(cr);
      setColor(cr, hot || on ? accent : th.panelEdge, hot ? 0.9 : (on ? 0.7 : 1.0));
      cairo_set_line_width(cr, 1.0);
      cairo_stroke(cr);
      setColor(cr, on || hot ? accent : th.textDim);
      drawText(cr, r.x + r.w * 0.5, r.y + r.h - 5.0, label, 8.5, true, Align::Center);
   }

   void drawAddStrip(cairo_t *cr) {
      const Theme &th = mSpec.theme;
      const Rect &r = mAddStrip;
      const bool hot = mHover2.kind == kHitAddStrip || (mAddOpen && !mAddFromTab);
      const double dash[] = {4.0, 3.0};
      cairo_set_dash(cr, dash, 2, 0.0);
      setColor(cr, hot ? kTheme.accent : th.panelEdge, hot ? 0.8 : 1.0);
      cairo_set_line_width(cr, 1.0);
      roundedRect(cr, r.x + 0.5, r.y + 0.5, r.w - 1, r.h - 1, 5);
      cairo_stroke(cr);
      cairo_set_dash(cr, nullptr, 0, 0.0);
      drawPlus(cr, r.x + r.w * 0.5, r.y + r.h * 0.5 - 10, 7.0, hot ? kTheme.accent : th.textDim);
      setColor(cr, hot ? kTheme.accent : th.textDim);
      drawText(cr, r.x + r.w * 0.5, r.y + r.h * 0.5 + 16, "ADD", 9, true, Align::Center);
   }

   void drawMasterStrip(cairo_t *cr) {
      const Theme &th = mSpec.theme;
      const Strip &s = mMaster;
      setColor(cr, th.panelEdge);
      cairo_set_line_width(cr, 1.0);
      cairo_move_to(cr, s.r.x - 8.5, s.r.y);
      cairo_line_to(cr, s.r.x - 8.5, s.r.y + s.r.h);
      cairo_stroke(cr);

      setColor(cr, kTheme.accent, 0.9);
      cairo_rectangle(cr, s.r.x + 6, s.r.y, s.r.w - 12, 2.5);
      cairo_fill(cr);
      setColor(cr, mHover2.kind == kHitMasterFader || mHover2.kind == kHitMasterWidth
                      ? th.text
                      : th.textDim);
      drawText(cr, s.r.x + s.r.w * 0.5, s.r.y + 17, "SCENE", 9.0, true, Align::Center);
      setColor(cr, th.textMute);
      drawText(cr, s.r.x + s.r.w * 0.5, s.r.y + 30, "output", 8.0, false, Align::Center);

      const Rgb keep = mSpec.theme.accent;
      mSpec.theme.accent = kTheme.accent;
      const ParamDesc &gd = mSpec.params[kParamGain];
      const double gain = mDelegate.guiParamValue(kParamGain);
      drawFader(cr, s.fader, normalised(gd, gain), mHover2.kind == kHitMasterFader, true);
      float pl = 0.0f, pr = 0.0f;
      mScene.outputPeaks(pl, pr);
      drawPeakBars(cr, s.fader, pl, pr, kTheme.accent);
      char text[64];
      if (!paramValueToText(gd, gain, text, sizeof(text)))
         std::snprintf(text, sizeof(text), "--");
      setColor(cr, th.text, 0.9);
      drawText(cr, s.r.x + s.r.w * 0.5, s.fader.y + s.fader.h + 16, text, 9.0, false,
               Align::Center);
      drawSlider(cr, s.pan, "", mSpec.params[kParamWidth], mDelegate.guiParamValue(kParamWidth),
                 mHover2.kind == kHitMasterWidth, true);
      setColor(cr, th.textMute);
      drawText(cr, s.r.x + s.r.w * 0.5, s.stereo.y + 13, "WIDTH", 8.0, true, Align::Center);
      mSpec.theme.accent = keep;
   }

   void drawBarButton(cairo_t *cr, const Rect &r, bool hot) {
      setColor(cr, mSpec.theme.panelFill);
      roundedRect(cr, r.x, r.y, r.w, r.h, 4);
      cairo_fill_preserve(cr);
      setColor(cr, hot ? mSpec.theme.accent : mSpec.theme.panelEdge, hot ? 0.7 : 1.0);
      cairo_set_line_width(cr, 1.0);
      cairo_stroke(cr);
   }

   void drawLayerBar(cairo_t *cr) {
      const Theme &th = mSpec.theme;
      const int slot = mPage;
      const LayerType &t = layerType(slotType(slot));
      char name[32];
      upperCase(t.pluginName, name, sizeof(name));
      setColor(cr, th.textMute);
      drawText(cr, kMargin, kPageTop + 21, name, 9, true, Align::Left);

      drawBarButton(cr, mLayerPrev, mHover2.kind == kHitLayerPrev);
      setColor(cr, mHover2.kind == kHitLayerPrev ? th.accent : th.textDim);
      drawTriangle(cr, mLayerPrev.x + mLayerPrev.w * 0.5, mLayerPrev.y + mLayerPrev.h * 0.5, 9, -1);
      drawBarButton(cr, mLayerNext, mHover2.kind == kHitLayerNext);
      setColor(cr, mHover2.kind == kHitLayerNext ? th.accent : th.textDim);
      drawTriangle(cr, mLayerNext.x + mLayerNext.w * 0.5, mLayerNext.y + mLayerNext.h * 0.5, 9, 1);

      const bool nameHot = mHover2.kind == kHitLayerName || (mBrowserOpen && mOverlayTarget >= 0);
      drawBarButton(cr, mLayerName, nameHot);
      const std::vector<GuiPreset> &list = mScene.presets(slot);
      const int cur = mScene.currentPreset(slot);
      char label[160];
      if (cur >= 0 && cur < static_cast<int>(list.size()))
         std::snprintf(label, sizeof(label), "%s%s", list[static_cast<size_t>(cur)].name.c_str(),
                       mScene.presetEdited(slot) ? " *" : "");
      else
         std::snprintf(label, sizeof(label), "%s", mScene.presetEdited(slot) ? "Init *" : "Init");
      setColor(cr, th.accent);
      cairo_save(cr);
      cairo_rectangle(cr, mLayerName.x + 4, mLayerName.y, mLayerName.w - 26, mLayerName.h);
      cairo_clip(cr);
      drawText(cr, mLayerName.x + (mLayerName.w - 14) * 0.5, mLayerName.y + 21, label, 12, true,
               Align::Center);
      cairo_restore(cr);
      setColor(cr, th.textMute);
      drawTriangle(cr, mLayerName.x + mLayerName.w - 12, mLayerName.y + mLayerName.h * 0.5, 8, 0);

      const bool saveHot = mHover2.kind == kHitLayerSave || (mSaveOpen && mOverlayTarget >= 0);
      drawBarButton(cr, mLayerSave, saveHot);
      setColor(cr, saveHot ? th.accent : th.textDim);
      drawText(cr, mLayerSave.x + mLayerSave.w * 0.5, mLayerSave.y + 20, "SAVE", 10, true,
               Align::Center);

      // Where the layer sits in the scene: the same controls as its strip.
      auto sliderLabel = [&](const Rect &r, const char *text) {
         setColor(cr, th.textMute);
         drawText(cr, r.x - 8, kPageTop + 20, text, 8.5, true, Align::Right);
      };
      const bool muted = mScene.layerMuted(slot);
      bool anySolo = false;
      for (const int s : mLayers)
         anySolo = anySolo || mScene.layerSoloed(s);
      const bool live = anySolo ? mScene.layerSoloed(slot) : !muted;

      const uint32_t level = slotMixId(slot, kSlotLevel);
      sliderLabel(mLayerLevel, "LEVEL");
      drawSlider(cr, mLayerLevel, "", mSpec.params[level], mDelegate.guiParamValue(level),
                 mHover2.kind == kHitLayerLevel, live);
      const uint32_t pan = slotMixId(slot, kSlotPan);
      sliderLabel(mLayerPan, "PAN");
      drawSlider(cr, mLayerPan, "", mSpec.params[pan], mDelegate.guiParamValue(pan),
                 mHover2.kind == kHitLayerPan, live);
      const uint32_t stereo = slotMixId(slot, kSlotStereo);
      const bool mono = mDelegate.guiParamValue(stereo) >= 0.5;
      drawToggle(cr, mLayerStereo, mono ? "MONO" : "STEREO", mono,
                 mHover2.kind == kHitLayerStereo, th.accent);
      if (t.shotLevelParam != kNoLayerParam) {
         const uint32_t shot = slotMixId(slot, kSlotShotRate);
         sliderLabel(mLayerShot, "SHOTS");
         drawSlider(cr, mLayerShot, "", mSpec.params[shot], mDelegate.guiParamValue(shot),
                    mHover2.kind == kHitLayerShot, true);
      }
      drawMixerButton(cr, mLayerMute, "M", muted, mHover2.kind == kHitLayerMute, kMuteRed);
      drawMixerButton(cr, mLayerSolo, "S", mScene.layerSoloed(slot), mHover2.kind == kHitLayerSolo,
                      th.accent);

      const bool removeHot = mHover2.kind == kHitLayerRemove;
      drawBarButton(cr, mLayerRemove, false);
      if (removeHot) {
         setColor(cr, kMuteRed, 0.8);
         roundedRect(cr, mLayerRemove.x, mLayerRemove.y, mLayerRemove.w, mLayerRemove.h, 4);
         cairo_stroke(cr);
      }
      setColor(cr, removeHot ? kMuteRed : th.textDim);
      drawText(cr, mLayerRemove.x + mLayerRemove.w * 0.5, mLayerRemove.y + 20, "REMOVE", 10, true,
               Align::Center);
   }

   // ------------------------------------------------------------ add menu

   Rect addMenuPanel() const {
      Rect r;
      r.w = 260.0;
      r.h = kNumLayerTypes * 24.0 + 2 * kMenuPad + 22.0;
      r.x = mAddAnchor.x;
      r.y = mAddAnchor.y + mAddAnchor.h + 4.0;
      if (r.x + r.w > kMargin + geometry().contentW)
         r.x = kMargin + geometry().contentW - r.w;
      if (r.y + r.h > geometry().windowH - 8)
         r.y = std::max(static_cast<double>(kHeaderH), mAddAnchor.y + 40.0);
      return r;
   }

   Rect addMenuRow(int type) const {
      const Rect p = addMenuPanel();
      return {p.x + kMenuPad, p.y + kMenuPad + 22.0 + type * 24.0, p.w - 2 * kMenuPad, 24.0};
   }

   int instancesOf(int type) const {
      int n = 0;
      for (int i = 0; i < kInstancesPerType; ++i)
         n += mScene.layerActive(slotIndex(type, i)) ? 1 : 0;
      return n;
   }

   void drawAddMenu(cairo_t *cr) {
      const Theme &th = mSpec.theme;
      const Rect p = addMenuPanel();
      setColor(cr, th.panelFill);
      roundedRect(cr, p.x, p.y, p.w, p.h, 5);
      cairo_fill_preserve(cr);
      setColor(cr, kTheme.accent, 0.5);
      cairo_set_line_width(cr, 1.0);
      cairo_stroke(cr);
      setColor(cr, th.textMute);
      drawText(cr, p.x + 10, p.y + 17, "ADD A LAYER", 9, true, Align::Left);

      for (int type = 0; type < kNumLayerTypes; ++type) {
         const Rect r = addMenuRow(type);
         const LayerType &t = layerType(type);
         const int used = instancesOf(type);
         const bool full = used >= kInstancesPerType;
         const bool hot = mAddHover == type && !full;
         if (hot) {
            setColor(cr, accentOf(type), 0.18);
            roundedRect(cr, r.x, r.y, r.w, r.h, 3);
            cairo_fill(cr);
         }
         setColor(cr, accentOf(type), full ? 0.3 : 0.95);
         cairo_arc(cr, r.x + 10, r.y + r.h * 0.5, 4.0, 0, 2 * M_PI);
         cairo_fill(cr);
         setColor(cr, th.text, full ? 0.35 : (hot ? 1.0 : 0.85));
         drawText(cr, r.x + 22, r.y + 16, t.label, 10.5, true, Align::Left);
         setColor(cr, th.textMute, full ? 0.6 : 1.0);
         drawText(cr, r.x + 92, r.y + 16, t.pluginName, 9.5, false, Align::Left);
         char count[16];
         std::snprintf(count, sizeof(count), "%d / %d", used, kInstancesPerType);
         drawText(cr, r.x + r.w - 8, r.y + 16, count, 9, false, Align::Right);
      }
   }

   void openAddMenu(const Rect &anchor, bool fromTab) {
      closeEntry();
      closeMenu();
      mAddOpen = true;
      mAddFromTab = fromTab;
      mAddAnchor = anchor;
      mAddHover = -1;
      mDirty = true;
   }

   void addLayer(int type) {
      mAddOpen = false;
      const int slot = mScene.addLayer(type);
      syncLayers(true);
      // From the tabs a new layer opens at once: that is where it is edited.
      // From the mixer it stays on the scene, where it is being balanced.
      if (slot >= 0 && mAddFromTab)
         showPage(slot);
      mDirty = true;
   }

   void removeLayer(int slot) {
      mScene.removeLayer(slot);
      syncLayers(true);
      mHover2 = {};
      mHelp.clear();
   }

   // ----------------------------------------------------------- hit tests

   Hit hitAt(double x, double y) const {
      if (mSceneTab.contains(x, y))
         return {kHitTab, kSceneTarget};
      for (const Tab &t : mTabs) {
         if (!t.r.contains(x, y))
            continue;
         // Only the open tab can be closed from its tab. Anywhere on another
         // tab opens it: a quick click near a tab's edge must never throw a
         // layer away, and the cross is not even drawn until it is hovered.
         if (t.slot == mPage && t.r.w >= 70.0 && t.close.contains(x, y))
            return {kHitTabClose, t.slot};
         return {kHitTab, t.slot};
      }
      if (mAddTab.contains(x, y))
         return {kHitAddTab, -1};

      if (mPage < 0) {
         for (const Strip &s : mStrips) {
            if (!s.r.contains(x, y))
               continue;
            Rect grab = s.fader;
            grab.x -= 6.0;
            grab.w += 12.0;
            if (grab.contains(x, y))
               return {kHitStripFader, s.slot};
            if (s.pan.contains(x, y))
               return {kHitStripPan, s.slot};
            if (s.stereo.contains(x, y))
               return {kHitStripStereo, s.slot};
            if (s.mute.contains(x, y))
               return {kHitStripMute, s.slot};
            if (s.solo.contains(x, y))
               return {kHitStripSolo, s.slot};
            if (s.label.contains(x, y))
               return {kHitStripLabel, s.slot};
            return {};
         }
         if (mAddStrip.contains(x, y))
            return {kHitAddStrip, -1};
         Rect grab = mMaster.fader;
         grab.x -= 6.0;
         grab.w += 12.0;
         if (grab.contains(x, y))
            return {kHitMasterFader, -1};
         if (mMaster.pan.contains(x, y))
            return {kHitMasterWidth, -1};
         return {};
      }

      const int slot = mPage;
      auto widen = [](Rect r) {
         r.y -= 6.0;
         r.h += 12.0;
         return r;
      };
      if (mLayerPrev.contains(x, y))
         return {kHitLayerPrev, slot};
      if (mLayerName.contains(x, y))
         return {kHitLayerName, slot};
      if (mLayerNext.contains(x, y))
         return {kHitLayerNext, slot};
      if (mLayerSave.contains(x, y))
         return {kHitLayerSave, slot};
      if (widen(mLayerLevel).contains(x, y))
         return {kHitLayerLevel, slot};
      if (widen(mLayerPan).contains(x, y))
         return {kHitLayerPan, slot};
      if (mLayerStereo.contains(x, y))
         return {kHitLayerStereo, slot};
      if (mLayerShot.w > 0.0 && widen(mLayerShot).contains(x, y))
         return {kHitLayerShot, slot};
      if (mLayerMute.contains(x, y))
         return {kHitLayerMute, slot};
      if (mLayerSolo.contains(x, y))
         return {kHitLayerSolo, slot};
      if (mLayerRemove.contains(x, y))
         return {kHitLayerRemove, slot};
      return {};
   }

   // The parameter a hit drags or nudges, or kNoParam.
   static uint32_t hitParam(const Hit &h) {
      switch (h.kind) {
      case kHitStripFader:
      case kHitLayerLevel:
         return slotMixId(h.slot, kSlotLevel);
      case kHitStripPan:
      case kHitLayerPan:
         return slotMixId(h.slot, kSlotPan);
      case kHitLayerShot:
         return slotMixId(h.slot, kSlotShotRate);
      case kHitMasterFader:
         return kParamGain;
      case kHitMasterWidth:
         return kParamWidth;
      default:
         return verdalis::kNoParam;
      }
   }

   static bool isFader(HitKind k) { return k == kHitStripFader || k == kHitMasterFader; }

   bool isCellParam(uint32_t id) const {
      for (const Cell &c : mCells)
         if (c.param == id)
            return true;
      return false;
   }

   const Strip *stripFor(int slot) const {
      for (const Strip &s : mStrips)
         if (s.slot == slot)
            return &s;
      return nullptr;
   }

   Rect hitTrack(const Hit &h) const {
      switch (h.kind) {
      case kHitStripFader:
         return stripFor(h.slot) ? stripFor(h.slot)->fader : Rect{};
      case kHitStripPan:
         return stripFor(h.slot) ? stripFor(h.slot)->pan : Rect{};
      case kHitMasterFader:
         return mMaster.fader;
      case kHitMasterWidth:
         return mMaster.pan;
      case kHitLayerLevel:
         return mLayerLevel;
      case kHitLayerPan:
         return mLayerPan;
      case kHitLayerShot:
         return mLayerShot;
      default:
         return {};
      }
   }

   std::string helpFor(const Hit &h) const {
      switch (h.kind) {
      case kHitTab:
         return h.slot < 0 ? "The scene: its mixer, its envelope and its filter."
                           : std::string("Open ") + layerType(slotType(h.slot)).pluginName +
                                "'s controls for this layer.";
      case kHitTabClose:
      case kHitLayerRemove:
         return "Remove this layer from the scene. It fades out on its own release.";
      case kHitAddTab:
      case kHitAddStrip:
         return "Add a layer: any of the nine instruments, up to four of each.";
      case kHitStripLabel:
         return "Open this layer.";
      case kHitStripStereo:
      case kHitLayerStereo:
         return mSpec.params[slotMixId(h.slot, kSlotStereo)].tip;
      case kHitStripMute:
      case kHitLayerMute:
         return "Mute this layer. Not saved: a mute is for listening, not for the scene.";
      case kHitStripSolo:
      case kHitLayerSolo:
         return "Hear this layer alone, or with the others soloed. Not saved either.";
      case kHitLayerPrev:
      case kHitLayerNext:
      case kHitLayerName:
         return std::string("Load any ") + layerType(slotType(h.slot)).pluginName +
                " preset into this layer -- the factory set and your own.";
      case kHitLayerSave:
         return std::string("Save this layer as a ") + layerType(slotType(h.slot)).pluginName +
                " preset. It appears in " + layerType(slotType(h.slot)).pluginName +
                " itself too.";
      default:
         return {};
      }
   }

   // -------------------------------------------------------------- events

   // The overlays a layer bar opens are about that layer's library until they
   // close; everything else is about the scene's.
   void settleOverlayTarget() {
      if (!mBrowserOpen && !mSaveOpen)
         mOverlayTarget = kSceneTarget;
      mProxy.target = mOverlayTarget;
   }

   bool baseOverlayOpen() const {
      return mSaveOpen || mMenuParam >= 0 || mBrowserOpen;
   }

   void onOverlayKey(KeyCommand cmd) override {
      if (mAddOpen) {
         mAddOpen = false;
         mDirty = true;
         return;
      }
      settleOverlayTarget();
      PluginWindow::onOverlayKey(cmd);
      settleOverlayTarget();
   }

   void onPointerDown(double px, double py, unsigned button, unsigned long timeMs,
                      bool shift) override {
      const double x = px / mScale;
      const double y = py / mScale;
      settleOverlayTarget();

      if (mEntryParam >= 0) {
         const bool inside =
            valueRect(cellRectFor(static_cast<uint32_t>(mEntryParam))).contains(x, y);
         if (inside) {
            PluginWindow::onPointerDown(px, py, button, timeMs, shift);
            return;
         }
         closeEntry();
      }
      if (baseOverlayOpen()) {
         PluginWindow::onPointerDown(px, py, button, timeMs, shift);
         settleOverlayTarget();
         return;
      }

      if (mAddOpen) {
         if (button == verdalis::kButtonLeft) {
            for (int type = 0; type < kNumLayerTypes; ++type) {
               if (addMenuRow(type).contains(x, y)) {
                  if (instancesOf(type) < kInstancesPerType)
                     addLayer(type);
                  return;
               }
            }
            if (!addMenuPanel().contains(x, y))
               mAddOpen = false;
         }
         mDirty = true;
         return;
      }

      const Hit h = hitAt(x, y);
      if (h.kind == kHitNothing) {
         PluginWindow::onPointerDown(px, py, button, timeMs, shift);
         settleOverlayTarget();
         return;
      }
      onOwnDown(h, x, y, button, timeMs, shift);
      mDirty = true;
   }

   void onOwnDown(const Hit &h, double x, double y, unsigned button, unsigned long timeMs,
                  bool shift) {
      const uint32_t id = hitParam(h);
      if (id != verdalis::kNoParam) {
         if (button == verdalis::kWheelUp || button == verdalis::kWheelDown) {
            nudge(id, button == verdalis::kWheelUp ? 1 : -1, shift);
            return;
         }
         const bool doubleClick = button == verdalis::kButtonLeft &&
                                  mLastClickParam == static_cast<int>(id) &&
                                  timeMs - mLastClickTime < 400;
         mLastClickParam = static_cast<int>(id);
         mLastClickTime = timeMs;
         if (button == verdalis::kButtonRight || doubleClick) {
            setParamNow(id, mSpec.params[id].def);
            mLastClickParam = -1;
            return;
         }
         if (button != verdalis::kButtonLeft)
            return;
         const Rect track = hitTrack(h);
         const ParamDesc &d = mSpec.params[id];
         mDrag = static_cast<int>(id);
         mDragHoriz = !isFader(h.kind);
         mDragStartX = x;
         mDragStartY = y;
         mDragStartValue = mDelegate.guiParamValue(id);
         const double travel = mDragHoriz ? track.w : track.h - kHandleH;
         mDragUnitsPerPx = (d.max - d.min) / std::max(1.0, travel);
         mDelegate.guiBeginEdit(id);
         return;
      }
      if (button != verdalis::kButtonLeft)
         return;

      switch (h.kind) {
      case kHitTab:
      case kHitStripLabel:
         showPage(h.slot);
         break;
      case kHitTabClose:
      case kHitLayerRemove:
         removeLayer(h.slot);
         break;
      case kHitAddTab:
         openAddMenu(mAddTab, true);
         break;
      case kHitAddStrip:
         openAddMenu({mAddStrip.x, mAddStrip.y + mAddStrip.h * 0.5 - 30.0, mAddStrip.w, 0.0},
                     false);
         break;
      case kHitStripStereo:
      case kHitLayerStereo: {
         const uint32_t sid = slotMixId(h.slot, kSlotStereo);
         setParamNow(sid, mDelegate.guiParamValue(sid) >= 0.5 ? 0.0 : 1.0);
         break;
      }
      case kHitStripMute:
      case kHitLayerMute:
         mScene.setLayerMuted(h.slot, !mScene.layerMuted(h.slot));
         break;
      case kHitStripSolo:
      case kHitLayerSolo:
         mScene.setLayerSoloed(h.slot, !mScene.layerSoloed(h.slot));
         break;
      case kHitLayerPrev:
      case kHitLayerNext:
         mProxy.target = h.slot;
         stepPreset(h.kind == kHitLayerPrev ? -1 : 1);
         mProxy.target = kSceneTarget;
         break;
      case kHitLayerName:
         mOverlayTarget = h.slot;
         mProxy.target = h.slot;
         openBrowser();
         break;
      case kHitLayerSave:
         mOverlayTarget = h.slot;
         mProxy.target = h.slot;
         openSaveDialog();
         break;
      default:
         break;
      }
   }

   void onMotion(double x, double y, bool shift) override {
      if (mDrag >= 0 || baseOverlayOpen()) {
         PluginWindow::onMotion(x, y, shift);
         return;
      }
      if (mAddOpen) {
         int hover = -1;
         for (int type = 0; type < kNumLayerTypes; ++type)
            if (addMenuRow(type).contains(x, y))
               hover = type;
         if (hover != mAddHover) {
            mAddHover = hover;
            mDirty = true;
         }
         return;
      }
      const Hit h = hitAt(x, y);
      if (h != mHover2) {
         mHover2 = h;
         mHelp = helpFor(h);
         mDirty = true;
      }
      if (h.kind != kHitNothing) {
         // A control of this window's own: the help line names its parameter
         // the way it names a knob's.
         const uint32_t id = hitParam(h);
         const int hover = id != verdalis::kNoParam ? static_cast<int>(id) : -1;
         if (hover != mHover || mHoverWidget != Widget::NoWidget) {
            mHover = hover;
            mHoverWidget = Widget::NoWidget;
            mDirty = true;
         }
         return;
      }
      PluginWindow::onMotion(x, y, shift);
   }

   bool needsRepaint() override {
      syncLayers(false);
      if (PluginWindow::needsRepaint())
         return true;
      float l = 0.0f, r = 0.0f;
      mScene.outputPeaks(l, r);
      if (l > 1.0e-5f || r > 1.0e-5f)
         return true;
      for (const int s : mLayers) {
         mScene.layerPeaks(s, l, r);
         if (l > 1.0e-5f || r > 1.0e-5f)
            return true;
      }
      return false;
   }

   // ---------------------------------------------------------------- state

   SceneDelegate &mScene;
   int mPage = -2; // forces the first showPage() to set everything
   int mOverlayTarget = kSceneTarget;
   uint64_t mActiveMask = 0;
   std::vector<int> mLayers;

   WindowSpec mTypeSpecs[kNumLayerTypes];
   HeaderOrnament *mTypeOrnaments[kNumLayerTypes];
   HeaderOrnament *mSceneOrnament = nullptr;
   char mSubtitle[96] = {};
   mutable char mTitle[96] = {};

   // A layer page's panels, with ids moved to the layer's slot.
   std::vector<std::vector<uint32_t>> mPageIds;
   std::vector<PanelSpec> mPageSpecs;
   double mRowEndX = 0.0;

   Rect mSceneTab, mAddTab;
   std::vector<Tab> mTabs;
   Rect mMixerPanel;
   std::vector<Strip> mStrips;
   Strip mMaster{};
   Rect mAddStrip;
   double mStripW = kStripMaxW;

   Rect mLayerPrev, mLayerName, mLayerNext, mLayerSave;
   Rect mLayerLevel, mLayerPan, mLayerStereo, mLayerShot, mLayerMute, mLayerSolo, mLayerRemove;

   bool mAddOpen = false;
   bool mAddFromTab = false;
   Rect mAddAnchor;
   int mAddHover = -1;

   Hit mHover2;
   std::string mHelp;
};

} // namespace

Gui *createGui(SceneDelegate &delegate) {
   auto *gui = new SceneWindow(delegate);
   if (!gui->open()) {
      delete gui;
      return nullptr;
   }
   return gui;
}

} // namespace verdaliscene
