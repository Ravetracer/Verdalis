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

#include "verdalis/dsp/adsr.h"
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
// The mixer is never shorter than this, whatever the plugins' layouts allow:
// tall enough for a channel strip's envelope and filter above a fader that is
// still worth dragging.
constexpr int kMinMixerH = 460;

// A channel strip carries four envelope knobs side by side, which sets how
// narrow it can get; more layers than fit scroll.
constexpr double kStripMaxW = 124.0;
constexpr double kStripMinW = 110.0;
constexpr double kMasterW = 124.0;
constexpr double kAddW = 64.0;
constexpr double kMixPad = 12.0;
constexpr double kStripButtonH = 18.0;
// The top of a strip, from its top edge: name, envelope graph, the envelope's
// knobs, the filter's type and its knobs, and then the fader.
constexpr double kStripEnvY = 40.0;
constexpr double kStripEnvH = 46.0;
constexpr double kStripEnvKnobsY = 92.0;
constexpr double kStripTypeY = 132.0;
constexpr double kStripFilterKnobsY = 152.0;
constexpr double kStripKnobH = 34.0;
// The effects: one lit letter per effect on the channel.
constexpr double kStripFxY = 196.0;
constexpr double kStripFxH = 16.0;
constexpr double kStripFaderY = 224.0;
constexpr double kMiniKnobR = 10.0;

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
// Each curve under the time it bends.
constexpr uint32_t kEnvParams[] = {kParamGate,    kParamAttack,      kParamDecay,
                                   kParamRelease, kParamSustain,     kParamAttackCurve,
                                   kParamDecayCurve, kParamReleaseCurve};
constexpr uint32_t kFilterParams[] = {kParamFilterType, kParamHighpass, kParamFilterCutoff,
                                      kParamFilterReso};
constexpr uint32_t kOutParams[] = {kParamWidth, kParamFxTails, kParamGain};

#define PANEL(title, cols, rows, arr)                                                              \
   { title, cols, rows, arr, static_cast<int>(sizeof(arr) / sizeof(arr[0])) }
const PanelSpec kScenePanels[] = {
   PANEL("ENVELOPE", 4, 2, kEnvParams),
   PANEL("FILTER", 2, 2, kFilterParams),
   PANEL("OUTPUT", 2, 2, kOutParams),
};
#undef PANEL
constexpr int kNumScenePanels = static_cast<int>(sizeof(kScenePanels) / sizeof(kScenePanels[0]));
static_assert(kNumSceneParams == 8 + 4 + 3, "every scene parameter must be on a scene panel");

// The effects' panel titles, by FxKind.
const char *const kFxTitles[kNumFxKinds] = {"REVERB",  "DELAY",   "CHORUS",  "FLANGER",
                                            "PHASER",  "WIDENER", "AUTO PAN"};
// The order the chain runs them, which is the order they are shown in
// wherever they are shown as a row.
constexpr int kRunOrder[kNumFxKinds] = {kFxPhaser, kFxChorus,  kFxFlanger, kFxDelay,
                                        kFxReverb, kFxWidener, kFxAutoPan};
constexpr const int *kRunOrderDisplay = kRunOrder;

Rgb accentOf(int type) {
   const LayerType &t = layerType(type);
   return {t.accent[0], t.accent[1], t.accent[2]};
}

// ----------------------------------------------------------- channel strips
//
// What a mixer strip carries of a layer -- or, on the master strip, of the
// scene: its envelope with the curves that bend it, and its filter. A layer's
// are the plugin's own parameters, the same ids its page shows, so a knob on
// the strip and the knob on the page are one control; the curves are the
// layer's own placement parameters. kNoParam where a layer has no such
// control: thunder's envelope has no decay and no sustain.
struct StripParams {
   uint32_t attack = verdalis::kNoParam;
   uint32_t decay = verdalis::kNoParam;
   uint32_t sustain = verdalis::kNoParam;
   uint32_t release = verdalis::kNoParam;
   uint32_t curve[3] = {verdalis::kNoParam, verdalis::kNoParam, verdalis::kNoParam};
   uint32_t filterType = verdalis::kNoParam;
   uint32_t highpass = verdalis::kNoParam;
   uint32_t cutoff = verdalis::kNoParam;
   uint32_t reso = verdalis::kNoParam;
};

StripParams stripParams(int slot) {
   StripParams sp;
   if (slot < 0) {
      sp.attack = kParamAttack;
      sp.decay = kParamDecay;
      sp.sustain = kParamSustain;
      sp.release = kParamRelease;
      sp.curve[0] = kParamAttackCurve;
      sp.curve[1] = kParamDecayCurve;
      sp.curve[2] = kParamReleaseCurve;
      sp.filterType = kParamFilterType;
      sp.highpass = kParamHighpass;
      sp.cutoff = kParamFilterCutoff;
      sp.reso = kParamFilterReso;
      return sp;
   }
   const LayerType &t = layerType(slotType(slot));
   auto own = [&](const char *key) {
      const uint32_t p = layerParamByKey(t, key);
      return p == kNoLayerParam || pinnedParam(t, p) ? verdalis::kNoParam : slotParamId(slot, p);
   };
   sp.attack = own("attack");
   sp.decay = own("decay");
   sp.sustain = own("sustain");
   sp.release = own("release");
   for (uint32_t i = 0; i < 3; ++i)
      if (slotParamApplies(t, kSlotAttackCurve + i))
         sp.curve[i] = slotMixId(slot, kSlotAttackCurve + i);
   sp.filterType = own("filter_type");
   sp.highpass = own("highpass");
   sp.cutoff = own("filter_cutoff");
   sp.reso = own("filter_reso");
   return sp;
}

// An envelope as its graph draws it. Each stage gets a share of the width that
// grows with the log of its length, so a 2 ms attack and a 20 s release are
// both there to see and to grab; within a stage the shape is the one the
// envelope plays (verdalis::Adsr), over the time it really takes.
struct EnvShape {
   enum Segment { kAttack = 0, kDecay, kSustain, kRelease, kNumSegments };
   double x[kNumSegments + 1] = {}; // where each segment starts, and the last one ends
   double sustain = 1.0;
   float k[3] = {0.0f, 0.0f, 0.0f}; // attack, decay, release
   double y0 = 0.0, yRange = 1.0;   // level 0 and full level, as y

   double yOf(double level) const { return y0 - level * yRange; }
   // The level along segment `seg` at its own normalised time p.
   double level(int seg, double p) const {
      using verdalis::Adsr;
      switch (seg) {
      case kAttack:
         return Adsr::shape(static_cast<float>(p), k[0]);
      case kDecay:
         return 1.0 + (sustain - 1.0) * Adsr::shape(static_cast<float>(p), k[1]);
      case kSustain:
         return sustain;
      case kRelease:
      default:
         return sustain * (1.0 - Adsr::shape(static_cast<float>(p), k[2]));
      }
   }
};

EnvShape envShape(const ParamDesc *table, const GuiDelegate &d, const StripParams &sp,
                  const Rect &r) {
   using verdalis::Adsr;
   using verdalis::kNoParam;
   auto real = [&](uint32_t id, double none) {
      return id == kNoParam ? none : paramToReal(table[id], d.guiParamValue(id));
   };
   EnvShape e;
   const double a = real(sp.attack, 0.0) * 0.001;
   const double dec = real(sp.decay, 0.0) * 0.001;
   const double rel = real(sp.release, 0.0) * 0.001;
   e.sustain = sp.sustain == kNoParam ? 1.0 : std::min(1.0, std::max(0.0, real(sp.sustain, 1.0)));
   const float s = static_cast<float>(e.sustain);

   double len[3] = {0.0, 0.0, 0.0};
   float k0 = 0.0f;
   if (sp.attack != kNoParam && Adsr::naturalBend(0.0f, Adsr::kAttackTarget, 1.0f, k0)) {
      len[0] = Adsr::kAttackTau * a * k0;
      e.k[0] = Adsr::bend(k0, static_cast<float>(real(sp.curve[0], 0.0)));
   }
   if (sp.decay != kNoParam && Adsr::naturalBend(1.0f, s, s + Adsr::kSettle, k0)) {
      len[1] = Adsr::kDecayTau * dec * k0;
      e.k[1] = Adsr::bend(k0, static_cast<float>(real(sp.curve[1], 0.0)));
   }
   // With nothing to sustain the envelope has ended before a release.
   if (sp.release != kNoParam && s > 0.0f &&
       Adsr::naturalBend(s, Adsr::kReleaseTarget, Adsr::kSettle, k0)) {
      len[2] = Adsr::kReleaseTau * rel * k0;
      e.k[2] = Adsr::bend(k0, static_cast<float>(real(sp.curve[2], 0.0)));
   }

   const double inner = r.w - 8.0;
   const double longest = std::log1p(60.0 / 0.005);
   double w[3];
   double used = 0.0;
   for (int i = 0; i < 3; ++i) {
      const bool there = (i == 0 && sp.attack != kNoParam) || (i == 1 && len[1] > 0.0) ||
                         (i == 2 && len[2] > 0.0);
      w[i] = there ? std::max(9.0, 0.27 * inner * std::log1p(len[i] / 0.005) / longest) : 0.0;
      used += w[i];
   }
   e.x[0] = r.x + 4.0;
   e.x[1] = e.x[0] + w[0];
   e.x[2] = e.x[1] + w[1];
   e.x[3] = e.x[2] + std::max(0.0, inner - used);
   e.x[4] = e.x[3] + w[2];
   e.y0 = r.y + r.h - 5.0;
   e.yRange = r.h - 11.0;
   return e;
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
   bool guiSavePreset(const std::string &folder, const std::string &name,
                      const std::string &description, std::string &error) override {
      return mScene->savePreset(target, folder, name, description, error);
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
   std::vector<std::string> guiPresetCollections() const override {
      return mScene->presetCollections(target);
   }
   bool guiCreateCollection(const std::string &name, std::string &folder,
                            std::string &error) override {
      return mScene->createCollection(target, name, folder, error);
   }
   bool guiRenameCollection(const std::string &folder, const std::string &name,
                            std::string &renamed, std::string &error) override {
      return mScene->renameCollection(target, folder, name, renamed, error);
   }
   bool guiDeleteCollection(const std::string &folder, std::string &error) override {
      return mScene->deleteCollection(target, folder, error);
   }
   bool guiMovePreset(int index, const std::string &folder, std::string &error) override {
      return mScene->movePreset(target, index, folder, error);
   }
   bool guiEditPreset(int index, const std::string &name, const std::string &description,
                      std::string &error) override {
      return mScene->editPreset(target, index, name, description, error);
   }
   bool guiDeletePreset(int index, std::string &error) override {
      return mScene->deletePreset(target, index, error);
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
      // An effect's knob sits in that effect's panel: "Decay", not
      // "Rain 1 Reverb Decay".
      for (int ch = 0; ch < kNumFxChannels; ++ch)
         for (uint32_t p = 0; p < kNumFxParams; ++p)
            out[fxParamId(ch, p)].name = fxParamTable()[p].name;
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
      // A channel strip's own controls, the master strip's included, and the
      // envelope graph on a layer's bar. `param` says which.
      kHitKnob,
      kHitEnvelope,
      kHitFilterType,
      kHitScrollLeft,
      kHitScrollRight,
      // The effects: a strip's row of them or a layer bar's button, which
      // open the effects view; and in that view, an effect's switch and the
      // way back.
      kHitStripFx,
      kHitLayerFx,
      kHitFxPower,
      kHitFxBack,
   };

   struct Hit {
      HitKind kind = kHitNothing;
      int slot = -1;
      uint32_t param = verdalis::kNoParam;
      int segment = -1; // an envelope graph's EnvShape::Segment
      bool operator==(const Hit &o) const {
         return kind == o.kind && slot == o.slot && param == o.param && segment == o.segment;
      }
      bool operator!=(const Hit &o) const { return !(*this == o); }
   };

   struct Tab {
      int slot;
      Rect r;
      Rect close;
   };

   // A knob on a strip: its cell, label on top and the knob under it.
   struct MiniKnob {
      Rect r;
      uint32_t id = verdalis::kNoParam;
      const char *label = "";
   };

   enum { kKnobAttack = 0, kKnobDecay, kKnobSustain, kKnobRelease, kKnobHighpass, kKnobCutoff,
          kKnobReso, kNumStripKnobs };

   struct Strip {
      int slot; // the layer, or kSceneTarget for the master strip
      Rect r;
      Rect label;
      Rect env;
      Rect filterType;
      Rect fx;
      MiniKnob knobs[kNumStripKnobs];
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
         mTypeMenuParam = -1;
         mDrag = -1;
         mFxChannel = -1;
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

      if (mFxChannel >= 0)
         layoutFxView(W);
      else if (mPage < 0)
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

      // ---- the mixer's strips, left to right in the tabs' order. As many as
      // fit at a strip's narrowest; the rest are scrolled to.
      const Rect &p = mMixerPanel;
      const double top = p.y + kPanelTitleH + 6.0;
      const double h = p.h - kPanelTitleH - 6.0 - kMixPad;
      const double master = p.x + p.w - kMixPad - kMasterW;
      const double room = master - kMixPad - (p.x + kMixPad) - kAddW - 8.0;
      const int count = static_cast<int>(mLayers.size());
      const int fit = std::max(1, static_cast<int>(std::floor(room / kStripMinW)));
      mStripsShown = std::min(count, fit);
      mStripFirst = std::max(0, std::min(mStripFirst, count - mStripsShown));
      mStripW = count == 0 ? kStripMaxW
                           : std::max(kStripMinW, std::min(kStripMaxW, room / mStripsShown));
      double x = p.x + kMixPad;
      for (int k = mStripFirst; k < mStripFirst + mStripsShown; ++k) {
         mStrips.push_back(stripAt(mLayers[static_cast<size_t>(k)], x, top, mStripW, h));
         x += mStripW;
      }
      mAddStrip = {x + 4.0, top, kAddW, h};
      mMaster = stripAt(kSceneTarget, master, top, kMasterW, h);
      if (count > mStripsShown) {
         mScrollLeft = {p.x + 70.0, p.y + 4.0, 22.0, 18.0};
         mScrollRight = {mScrollLeft.x + mScrollLeft.w + 74.0, p.y + 4.0, 22.0, 18.0};
      } else {
         mScrollLeft = mScrollRight = {};
      }
   }

   // One strip's controls, top to bottom: the name, the envelope graph and
   // its knobs, the filter's type and knobs, the fader and its value, pan,
   // stereo or mono, and mute and solo.
   static Strip stripAt(int slot, double x, double top, double w, double h) {
      Strip s;
      s.slot = slot;
      s.r = {x, top, w, h};
      s.label = {x + 2.0, top, w - 4.0, 34.0};
      s.env = {x + 7.0, top + kStripEnvY, w - 14.0, kStripEnvH};

      const StripParams sp = stripParams(slot);
      const uint32_t ids[kNumStripKnobs] = {sp.attack,   sp.decay,  sp.sustain, sp.release,
                                            sp.highpass, sp.cutoff, sp.reso};
      static const char *const labels[kNumStripKnobs] = {"ATK", "DEC", "SUS", "REL",
                                                         "HP",  "CUT", "RES"};
      const double envW = (w - 12.0) / 4.0;
      const double filterW = (w - 12.0) / 3.0;
      for (int i = 0; i < kNumStripKnobs; ++i) {
         MiniKnob &k = s.knobs[i];
         k.id = ids[i];
         k.label = labels[i];
         k.r = i < kKnobHighpass
                  ? Rect{x + 6.0 + i * envW, top + kStripEnvKnobsY, envW, kStripKnobH}
                  : Rect{x + 6.0 + (i - kKnobHighpass) * filterW, top + kStripFilterKnobsY,
                         filterW, kStripKnobH};
      }
      s.filterType = {x + 12.0, top + kStripTypeY, w - 24.0, 16.0};
      s.fx = {x + 7.0, top + kStripFxY, w - 14.0, kStripFxH};

      const double buttonsY = top + h - kStripButtonH;
      const double stereoY = buttonsY - 8.0 - kStripButtonH;
      const double panY = stereoY - 14.0 - 10.0;
      const double faderBottom = panY - 34.0;
      const double faderTop = top + kStripFaderY;
      s.fader = {x + w * 0.5 - 6.0 - 4.0, faderTop, 12.0, faderBottom - faderTop};
      s.pan = {x + 8.0, panY, w - 16.0, 10.0};
      s.stereo = {x + 6.0, stereoY, w - 12.0, kStripButtonH};
      const double bw = std::min(26.0, (w - 14.0) * 0.5);
      s.mute = {x + w * 0.5 - 2.0 - bw, buttonsY, bw, kStripButtonH};
      s.solo = {x + w * 0.5 + 2.0, buttonsY, bw, kStripButtonH};
      return s;
   }

   // ------------------------------------------------------- the effects view
   //
   // A channel's effects take the page below the tabs: a bar with the chain
   // and the way back, then a panel per effect -- the suite's own panels, so
   // every knob drags, types and resets the way every other knob does. The
   // first row is the two that matter most, the reverb and the delay, with
   // the channel's activity beside them; the second is the rest, in the order
   // the chain runs them.

   void openFx(int channel) {
      closeEntry();
      closeMenu();
      mAddOpen = false;
      mTypeMenuParam = -1;
      mFxChannel = channel;
      mSpec.theme = kTheme;
      if (channel != kMasterFx) {
         const WindowSpec &ts = mTypeSpecs[slotType(channel)];
         mSpec.theme.accent = ts.theme.accent;
         mSpec.theme.highlight = ts.theme.highlight;
      }
      mHover2 = {};
      mHelp.clear();
      buildLayout();
      mDirty = true;
   }

   void closeFx() {
      if (mFxChannel < 0)
         return;
      mFxChannel = -1;
      closeEntry();
      closeMenu();
      // Back to the page's own theme and layout.
      const int page = mPage;
      mPage = -2;
      showPage(page);
   }

   std::string channelName(int channel) const {
      if (channel == kMasterFx)
         return "SCENE";
      char label[48];
      std::snprintf(label, sizeof(label), "%s %d", layerType(slotType(channel)).label,
                    slotInstance(channel) + 1);
      upperCase(label, label, sizeof(label));
      return label;
   }

   void layoutFxView(double W) {
      const int ch = mFxChannel;
      mFxIds.assign(kNumFxKinds, {});
      mFxSpecs.clear();
      mFxPanelKind.clear();
      for (int k = 0; k < kNumFxKinds; ++k) {
         const FxKindInfo &info = fxKind(k);
         // The On switch is in the panel's title, not a knob.
         for (uint32_t p = info.first + 1; p < info.first + info.count; ++p)
            mFxIds[static_cast<size_t>(k)].push_back(fxParamId(ch, p));
      }
      auto spec = [&](int k, int cols) {
         const auto &ids = mFxIds[static_cast<size_t>(k)];
         mFxSpecs.push_back({kFxTitles[k], cols, 2, ids.data(), static_cast<int>(ids.size())});
      };
      // Sized first, so the panels below can point into the vector.
      spec(kFxReverb, 7);
      spec(kFxDelay, 8);
      spec(kFxPhaser, 4);
      spec(kFxChorus, 3);
      spec(kFxFlanger, 3);
      spec(kFxWidener, 2);
      spec(kFxAutoPan, 2);
      static const int kKinds[] = {kFxReverb, kFxDelay,   kFxPhaser, kFxChorus,
                                   kFxFlanger, kFxWidener, kFxAutoPan};

      // The bar: what this is, the chain as switches, and the way back.
      const double barY = kPageTop;
      mFxBack = {kMargin + W - 78.0, barY, 78.0, kBarH};
      double cx = kMargin + 260.0;
      for (const int k : kRunOrder) {
         const double w = 22.0 + 7.2 * std::strlen(fxKind(k).name);
         mFxChain[k] = {cx, barY + 5.0, w, kBarH - 10.0};
         cx += w + 18.0;
      }

      double y = kLayerPanelsTop;
      double rowH = 0.0;
      const int first[2] = {0, 1};
      addPanelRow(mFxSpecs.data(), first, 2, kMargin, y, kMargin + W, false, rowH);
      mMeter = {mRowEndX, y, kMargin + W - mRowEndX, rowH};
      for (int i = 0; i < 2; ++i)
         mFxPanelKind.push_back(kKinds[i]);
      y += rowH + kGap;
      const int second[5] = {2, 3, 4, 5, 6};
      addPanelRow(mFxSpecs.data(), second, 5, kMargin, y, kMargin + W, true, rowH);
      for (int i = 2; i < 7; ++i)
         mFxPanelKind.push_back(kKinds[i]);
      for (size_t i = 0; i < mPanels.size(); ++i) {
         const Rect &r = mPanels[i].rect;
         mFxPower[mFxPanelKind[i]] = {r.x + r.w - kPanelPad - 44.0, r.y + 5.0, 44.0, 16.0};
      }
   }

   bool fxOn(int channel, int kind) const {
      return mDelegate.guiParamValue(fxParamId(channel, fxKind(kind).first)) >= 0.5;
   }

   void toggleFx(int channel, int kind) {
      const uint32_t id = fxParamId(channel, fxKind(kind).first);
      setParamNow(id, fxOn(channel, kind) ? 0.0 : 1.0);
   }

   void drawFxView(cairo_t *cr) {
      const Theme &th = mSpec.theme;
      const int ch = mFxChannel;
      // The bar.
      setColor(cr, th.textMute);
      drawText(cr, kMargin, kPageTop + 21, "EFFECTS", 9, true, Align::Left);
      setColor(cr, th.accent);
      drawText(cr, kMargin + 66, kPageTop + 21, channelName(ch).c_str(), 12, true, Align::Left);
      setColor(cr, th.textMute);
      drawText(cr, kMargin + 250, kPageTop + 21, "CHAIN", 8.5, true, Align::Right);
      bool firstBox = true;
      for (const int k : kRunOrder) {
         const Rect &r = mFxChain[k];
         if (!firstBox) {
            setColor(cr, th.textMute);
            drawTriangle(cr, r.x - 9.0, r.y + r.h * 0.5, 6, 1);
         }
         firstBox = false;
         const bool on = fxOn(ch, k);
         const bool hot = mHover2.kind == kHitFxPower && mHover2.segment == k;
         char label[32];
         upperCase(fxKind(k).name, label, sizeof(label));
         drawToggle(cr, r, label, on, hot, th.accent);
      }
      const bool backHot = mHover2.kind == kHitFxBack;
      drawBarButton(cr, mFxBack, backHot);
      setColor(cr, backHot ? th.accent : th.textDim);
      drawText(cr, mFxBack.x + mFxBack.w * 0.5, mFxBack.y + 20, "BACK", 10, true, Align::Center);

      // Each effect's switch, in its panel's title.
      for (size_t i = 0; i < mPanels.size(); ++i) {
         const int k = mFxPanelKind[i];
         const bool on = fxOn(ch, k);
         const bool hot = mHover2.kind == kHitFxPower && mHover2.segment == k;
         drawToggle(cr, mFxPower[k], on ? "ON" : "OFF", on, hot, th.accent);
      }
   }

   // A switched-off effect's panel is drawn faded: its settings are kept and
   // can be changed, but nothing of it is heard.
   void drawFxPanels(cairo_t *cr) {
      for (size_t i = 0; i < mPanels.size(); ++i) {
         const bool on = fxOn(mFxChannel, mFxPanelKind[i]);
         if (on) {
            drawPanel(cr, mPanels[i]);
            continue;
         }
         cairo_push_group(cr);
         drawPanel(cr, mPanels[i]);
         cairo_pop_group_to_source(cr);
         cairo_paint_with_alpha(cr, 0.45);
      }
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
      // The layer's envelope, bent where it is grabbed, as on its strip.
      mLayerEnv = {mLayerSave.x + mLayerSave.w + 74, barY + 1, 156, kBarH - 2.0};
      mLayerFx = {mLayerEnv.x + mLayerEnv.w + 14, barY, 54, kBarH};

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
      if (mFxChannel >= 0) {
         drawFxPanels(cr);
         drawMeter(cr);
         drawFxView(cr);
      } else {
         for (const Panel &p : mPanels)
            drawPanel(cr, p);
         drawMeter(cr);
         if (mPage < 0)
            drawSceneMixer(cr);
         else
            drawLayerBar(cr);
      }

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
      if (mTypeMenuParam >= 0)
         drawTypeMenu(cr);
      if (mSaveOpen)
         drawSaveDialog(cr);
   }

   void drawOwnHelp(cairo_t *cr) {
      setColor(cr, mSpec.theme.textMute);
      drawText(cr, kMargin, mHelpY + kHelpH - 8, mHelp.c_str(), 10, false, Align::Left);
   }

   // A slider or knob of this window's own has no value printed beside it, so
   // the help line says what it is set to while it is under the pointer or in
   // the hand. A segment of an envelope graph says how to bend it instead of
   // repeating its parameter's tip.
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
      const char *tip = mHover2.kind == kHitEnvelope && !mHelp.empty() ? mHelp.c_str() : d.tip;
      drawText(cr, kMargin + w + 14, mHelpY + kHelpH - 8, tip, 10, false, Align::Left);
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

   // ------------------------------------------------------- channel strips

   static bool isStripKind(HitKind k) {
      return (k >= kHitStripLabel && k <= kHitStripSolo) || k == kHitKnob || k == kHitEnvelope ||
             k == kHitFilterType;
   }

   // The envelope, filled under its curve. The segment under the pointer, or in
   // the hand, is drawn brighter with a handle at its middle: that is the one a
   // drag bends.
   void drawEnvelope(cairo_t *cr, const Rect &r, int slot, bool live) {
      const Theme &th = mSpec.theme;
      setColor(cr, th.knobFace);
      roundedRect(cr, r.x, r.y, r.w, r.h, 3);
      cairo_fill(cr);

      const StripParams sp = stripParams(slot);
      const EnvShape e = envShape(mSpec.params, mDelegate, sp, r);
      setColor(cr, th.track, 0.7);
      cairo_set_line_width(cr, 1.0);
      cairo_move_to(cr, r.x + 3.0, std::floor(e.y0) + 0.5);
      cairo_line_to(cr, r.x + r.w - 3.0, std::floor(e.y0) + 0.5);
      cairo_stroke(cr);

      auto traceSegment = [&](int seg) {
         const int steps = seg == EnvShape::kSustain ? 1 : 28;
         for (int j = 1; j <= steps; ++j) {
            const double p = static_cast<double>(j) / steps;
            cairo_line_to(cr, e.x[seg] + (e.x[seg + 1] - e.x[seg]) * p, e.yOf(e.level(seg, p)));
         }
      };
      auto trace = [&]() {
         cairo_new_path(cr);
         cairo_move_to(cr, e.x[0], e.yOf(0.0));
         for (int seg = 0; seg < EnvShape::kNumSegments; ++seg)
            traceSegment(seg);
      };
      const Rgb &c = live ? th.accent : th.textMute;
      trace();
      cairo_line_to(cr, e.x[EnvShape::kNumSegments], e.y0);
      cairo_close_path(cr);
      setColor(cr, c, 0.16);
      cairo_fill(cr);
      trace();
      setColor(cr, c, live ? 0.9 : 0.5);
      cairo_set_line_width(cr, 1.4);
      cairo_stroke(cr);

      if (mHover2.kind != kHitEnvelope || mHover2.slot != slot || mHover2.segment < 0)
         return;
      const int seg = mHover2.segment;
      cairo_new_path(cr);
      cairo_move_to(cr, e.x[seg], e.yOf(e.level(seg, 0.0)));
      traceSegment(seg);
      setColor(cr, th.text, 0.95);
      cairo_set_line_width(cr, 2.2);
      cairo_stroke(cr);
      const double mx = 0.5 * (e.x[seg] + e.x[seg + 1]);
      const double my = e.yOf(e.level(seg, 0.5));
      setColor(cr, th.knobFace);
      cairo_arc(cr, mx, my, 3.6, 0, 2 * M_PI);
      cairo_fill_preserve(cr);
      setColor(cr, c);
      cairo_set_line_width(cr, 1.4);
      cairo_stroke(cr);
   }

   // A strip's knob: smaller than a panel's and without a value under it --
   // the help line says what it is set to while it is under the pointer.
   void drawMiniKnob(cairo_t *cr, const MiniKnob &k, bool hot, bool live) {
      const Theme &th = mSpec.theme;
      const double cx = k.r.x + k.r.w * 0.5;
      const double cy = k.r.y + 22.0;
      const bool absent = k.id == verdalis::kNoParam;
      setColor(cr, hot ? th.text : th.textDim, absent ? 0.35 : (live ? 1.0 : 0.6));
      drawText(cr, cx, k.r.y + 8.0, k.label, 7.0, true, Align::Center);
      if (absent) {
         setColor(cr, th.panelEdge);
         cairo_set_line_width(cr, 1.0);
         cairo_new_path(cr);
         cairo_arc(cr, cx, cy, kMiniKnobR - 3.5, 0, 2 * M_PI);
         cairo_stroke(cr);
         return;
      }
      const ParamDesc &d = mSpec.params[k.id];
      const double t = normalised(d, mDelegate.guiParamValue(k.id));
      setColor(cr, th.track);
      cairo_set_line_width(cr, 2.4);
      cairo_new_path(cr);
      cairo_arc(cr, cx, cy, kMiniKnobR, verdalis::kArcStart, verdalis::kArcStart + verdalis::kArcSweep);
      cairo_stroke(cr);
      const double from = verdalis::isBipolar(d) ? verdalis::kArcStart + verdalis::kArcSweep * 0.5
                                                 : verdalis::kArcStart;
      const double to = verdalis::kArcStart + verdalis::kArcSweep * t;
      setColor(cr, live ? th.accent : th.textMute);
      cairo_new_path(cr);
      if (to >= from)
         cairo_arc(cr, cx, cy, kMiniKnobR, from, to);
      else
         cairo_arc_negative(cr, cx, cy, kMiniKnobR, from, to);
      cairo_stroke(cr);
      setColor(cr, th.knobFace);
      cairo_new_path(cr);
      cairo_arc(cr, cx, cy, kMiniKnobR - 3.5, 0, 2 * M_PI);
      cairo_fill_preserve(cr);
      setColor(cr, hot ? th.accent : th.panelEdge, hot ? 0.7 : 1.0);
      cairo_set_line_width(cr, 1.0);
      cairo_stroke(cr);
      setColor(cr, th.text);
      cairo_set_line_width(cr, 1.6);
      cairo_move_to(cr, cx + std::cos(to) * 1.5, cy + std::sin(to) * 1.5);
      cairo_line_to(cr, cx + std::cos(to) * (kMiniKnobR - 4.5), cy + std::sin(to) * (kMiniKnobR - 4.5));
      cairo_stroke(cr);
   }

   void drawTypeChip(cairo_t *cr, const Rect &r, uint32_t id, bool hot, bool live) {
      if (id == verdalis::kNoParam)
         return;
      const Theme &th = mSpec.theme;
      setColor(cr, th.knobFace);
      roundedRect(cr, r.x, r.y, r.w, r.h, 3);
      cairo_fill_preserve(cr);
      setColor(cr, hot ? th.accent : th.panelEdge, hot ? 0.7 : 1.0);
      cairo_set_line_width(cr, 1.0);
      cairo_stroke(cr);
      char text[64];
      if (!paramValueToText(mSpec.params[id], mDelegate.guiParamValue(id), text, sizeof(text)))
         std::snprintf(text, sizeof(text), "--");
      setColor(cr, th.text, live ? 0.9 : 0.55);
      drawText(cr, r.x + (r.w - 10.0) * 0.5, r.y + 12.0, text, 8.5, false, Align::Center);
      setColor(cr, th.textMute);
      drawTriangle(cr, r.x + r.w - 8.0, r.y + r.h * 0.5, 6, 0);
   }

   // Everything above the fader, the same on every strip: the envelope and
   // its knobs, the filter's type and its knobs.
   void drawChannelTop(cairo_t *cr, const Strip &s, bool live) {
      const Theme &th = mSpec.theme;
      drawEnvelope(cr, s.env, s.slot, live);
      for (const MiniKnob &k : s.knobs) {
         const bool hot = k.id != verdalis::kNoParam &&
                          ((mHover2.kind == kHitKnob && mHover2.param == k.id) ||
                           mDrag == static_cast<int>(k.id));
         drawMiniKnob(cr, k, hot, live);
      }
      const uint32_t type = stripParams(s.slot).filterType;
      const bool typeHot = type != verdalis::kNoParam &&
                           ((mHover2.kind == kHitFilterType && mHover2.param == type) ||
                            mTypeMenuParam == static_cast<int>(type));
      setColor(cr, th.panelEdge);
      cairo_set_line_width(cr, 1.0);
      cairo_move_to(cr, s.r.x + 12.0, std::floor(s.filterType.y - 6.0) + 0.5);
      cairo_line_to(cr, s.r.x + s.r.w - 12.0, std::floor(s.filterType.y - 6.0) + 0.5);
      cairo_move_to(cr, s.r.x + 12.0, std::floor(s.fader.y - 8.0) + 0.5);
      cairo_line_to(cr, s.r.x + s.r.w - 12.0, std::floor(s.fader.y - 8.0) + 0.5);
      cairo_stroke(cr);
      drawTypeChip(cr, s.filterType, type, typeHot, live);
      drawFxRow(cr, s, live);
   }

   // One letter per effect, lit when it is on, in the order of the effects
   // view. The whole row opens that view.
   void drawFxRow(cairo_t *cr, const Strip &s, bool live) {
      const Theme &th = mSpec.theme;
      const int ch = s.slot < 0 ? kMasterFx : s.slot;
      const bool hot = mHover2.kind == kHitStripFx && mHover2.slot == s.slot;
      setColor(cr, th.knobFace);
      roundedRect(cr, s.fx.x, s.fx.y, s.fx.w, s.fx.h, 3);
      cairo_fill_preserve(cr);
      setColor(cr, hot ? th.accent : th.panelEdge, hot ? 0.8 : 1.0);
      cairo_set_line_width(cr, 1.0);
      cairo_stroke(cr);
      const double cell = s.fx.w / kNumFxKinds;
      for (int i = 0; i < kNumFxKinds; ++i) {
         const int k = kRunOrderDisplay[i];
         const bool on = fxOn(ch, k);
         const double cx = s.fx.x + cell * (i + 0.5);
         if (on) {
            setColor(cr, th.accent, live ? 0.85 : 0.4);
            roundedRect(cr, cx - cell * 0.5 + 1.5, s.fx.y + 2.0, cell - 3.0, s.fx.h - 4.0, 2);
            cairo_fill(cr);
            setColor(cr, th.bgBottom);
         } else {
            setColor(cr, hot ? th.textDim : th.textMute, 0.8);
         }
         drawText(cr, cx, s.fx.y + 11.5, fxKind(k).letter, 7.5, true, Align::Center);
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

      if (mScrollLeft.w > 0.0) {
         const bool canLeft = mStripFirst > 0;
         const bool canRight = mStripFirst + mStripsShown < static_cast<int>(mLayers.size());
         drawBarButton(cr, mScrollLeft, canLeft && mHover2.kind == kHitScrollLeft);
         setColor(cr, canLeft ? (mHover2.kind == kHitScrollLeft ? th.accent : th.textDim)
                              : th.panelEdge);
         drawTriangle(cr, mScrollLeft.x + mScrollLeft.w * 0.5, mScrollLeft.y + mScrollLeft.h * 0.5,
                      7, -1);
         drawBarButton(cr, mScrollRight, canRight && mHover2.kind == kHitScrollRight);
         setColor(cr, canRight ? (mHover2.kind == kHitScrollRight ? th.accent : th.textDim)
                               : th.panelEdge);
         drawTriangle(cr, mScrollRight.x + mScrollRight.w * 0.5,
                      mScrollRight.y + mScrollRight.h * 0.5, 7, 1);
         char range[48];
         std::snprintf(range, sizeof(range), "%d-%d of %d", mStripFirst + 1,
                       mStripFirst + mStripsShown, static_cast<int>(mLayers.size()));
         setColor(cr, th.textDim);
         drawText(cr, 0.5 * (mScrollLeft.x + mScrollLeft.w + mScrollRight.x), p.y + 17, range, 9,
                  false, Align::Center);
      }

      bool holds = false;
      for (const int s : mLayers)
         holds = holds || mScene.layerMuted(s) || mScene.layerSoloed(s);
      const int gate = static_cast<int>(std::floor(mDelegate.guiParamValue(kParamGate) + 0.5));
      const char *note = holds ? "M and S are not saved with the scene"
                               : "One strip per layer. Click a name to open the layer.";
      bool waiting = false;
      if (!holds && !mLayers.empty() && !mScene.sceneGateOpen()) {
         if (gate == kGateNotes) {
            note = "Hold a key to play the scene.";
            waiting = true;
         } else if (gate == kGateTransport) {
            note = "Start the host's transport to play the scene.";
            waiting = true;
         }
      }
      setColor(cr, waiting ? kTheme.accent : th.textMute);
      drawText(cr, p.x + p.w - kPanelPad - 2, p.y + 16, note, 9, waiting, Align::Right);

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
      const bool hovered = mHover2.slot == s.slot && isStripKind(mHover2.kind);
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

      drawChannelTop(cr, s, live);

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

   // The scene's own strip, the same shape as a layer's: its envelope and
   // filter, its output gain and width, and a light for its gate.
   void drawMasterStrip(cairo_t *cr) {
      const Theme &th = mSpec.theme;
      const Strip &s = mMaster;
      setColor(cr, th.panelEdge);
      cairo_set_line_width(cr, 1.0);
      cairo_move_to(cr, s.r.x - 8.5, s.r.y);
      cairo_line_to(cr, s.r.x - 8.5, s.r.y + s.r.h);
      cairo_stroke(cr);

      const bool hovered = mHover2.slot == kSceneTarget &&
                           (isStripKind(mHover2.kind) || mHover2.kind == kHitMasterFader ||
                            mHover2.kind == kHitMasterWidth);
      setColor(cr, kTheme.accent, 0.9);
      cairo_rectangle(cr, s.r.x + 6, s.r.y, s.r.w - 12, 2.5);
      cairo_fill(cr);
      setColor(cr, hovered ? th.text : th.textDim);
      drawText(cr, s.r.x + s.r.w * 0.5, s.r.y + 17, "SCENE", 9.0, true, Align::Center);
      char gateText[48];
      if (!paramValueToText(mSpec.params[kParamGate], mDelegate.guiParamValue(kParamGate),
                            gateText, sizeof(gateText)))
         std::snprintf(gateText, sizeof(gateText), "--");
      char sub[64];
      std::snprintf(sub, sizeof(sub), "gate: %s", gateText);
      setColor(cr, th.textMute);
      drawText(cr, s.r.x + s.r.w * 0.5, s.r.y + 30, sub, 8.0, false, Align::Center);

      const Rgb keep = mSpec.theme.accent;
      mSpec.theme.accent = kTheme.accent;
      drawChannelTop(cr, s, true);

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

      // Lit while the gate is open: with the gate on Notes, an unlit light and
      // silence mean no key is down, not that something is broken.
      const bool open = mScene.sceneGateOpen();
      const Rect led = {s.mute.x, s.mute.y, s.solo.x + s.solo.w - s.mute.x, s.mute.h};
      drawMixerButton(cr, led, "GATE", open, false, kTheme.accent);
      mSpec.theme.accent = keep;
   }

   // A filter type's list, opened from a strip's chip.
   Rect typeMenuPanel() const {
      const ParamDesc &d = mSpec.params[mTypeMenuParam];
      Rect r;
      r.w = std::max(mTypeMenuAnchor.w, 96.0);
      r.h = d.enumCount * kMenuRowH + 2 * kMenuPad;
      r.x = mTypeMenuAnchor.x + (mTypeMenuAnchor.w - r.w) * 0.5;
      r.y = mTypeMenuAnchor.y + mTypeMenuAnchor.h + 3.0;
      if (r.y + r.h > geometry().windowH - 8)
         r.y = mTypeMenuAnchor.y - 3.0 - r.h;
      r.x = std::min(r.x, kMargin + geometry().contentW - r.w);
      return r;
   }

   Rect typeMenuRow(int i) const {
      const Rect p = typeMenuPanel();
      return {p.x + kMenuPad, p.y + kMenuPad + i * kMenuRowH, p.w - 2 * kMenuPad, kMenuRowH};
   }

   int typeMenuRowAt(double x, double y) const {
      if (mTypeMenuParam < 0)
         return -1;
      for (uint32_t i = 0; i < mSpec.params[mTypeMenuParam].enumCount; ++i)
         if (typeMenuRow(static_cast<int>(i)).contains(x, y))
            return static_cast<int>(i);
      return -1;
   }

   void drawTypeMenu(cairo_t *cr) {
      const Theme &th = mSpec.theme;
      const ParamDesc &d = mSpec.params[mTypeMenuParam];
      const Rect p = typeMenuPanel();
      setColor(cr, th.panelFill);
      roundedRect(cr, p.x, p.y, p.w, p.h, 5);
      cairo_fill_preserve(cr);
      setColor(cr, mTypeMenuAccent, 0.5);
      cairo_set_line_width(cr, 1.0);
      cairo_stroke(cr);
      const int cur = static_cast<int>(
         std::floor(mDelegate.guiParamValue(static_cast<uint32_t>(mTypeMenuParam)) + 0.5));
      for (int i = 0; i < static_cast<int>(d.enumCount); ++i) {
         const Rect r = typeMenuRow(i);
         const bool hot = mTypeMenuHover == i;
         const bool sel = cur == i;
         if (hot || sel) {
            setColor(cr, mTypeMenuAccent, hot ? 0.20 : 0.10);
            roundedRect(cr, r.x, r.y, r.w, r.h, 3);
            cairo_fill(cr);
         }
         setColor(cr, sel ? mTypeMenuAccent : th.text, hot ? 1.0 : 0.85);
         drawText(cr, r.x + 8, r.y + r.h * 0.5 + 4, d.enumNames[i], 10, sel, Align::Left);
      }
   }

   void openTypeMenu(uint32_t id, const Rect &anchor, const Rgb &accent) {
      closeEntry();
      closeMenu();
      mAddOpen = false;
      mTypeMenuParam = static_cast<int>(id);
      mTypeMenuAnchor = anchor;
      mTypeMenuAccent = accent;
      mTypeMenuHover = -1;
      mDirty = true;
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

      sliderLabel(mLayerEnv, "SHAPE");
      drawEnvelope(cr, mLayerEnv, slot, live);
      {
         const bool fxHot = mHover2.kind == kHitLayerFx;
         int count = 0;
         for (int k = 0; k < kNumFxKinds; ++k)
            count += fxOn(slot, k) ? 1 : 0;
         drawBarButton(cr, mLayerFx, fxHot);
         char fxText[16];
         std::snprintf(fxText, sizeof(fxText), count ? "FX %d" : "FX", count);
         setColor(cr, fxHot || count ? th.accent : th.textDim);
         drawText(cr, mLayerFx.x + mLayerFx.w * 0.5, mLayerFx.y + 20, fxText, 10, true,
                  Align::Center);
      }

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
      mTypeMenuParam = -1;
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

      if (mFxChannel >= 0) {
         if (mFxBack.contains(x, y))
            return {kHitFxBack, -1};
         for (int k = 0; k < kNumFxKinds; ++k)
            if (mFxChain[k].contains(x, y) || mFxPower[k].contains(x, y))
               return {kHitFxPower, -1, fxParamId(mFxChannel, fxKind(k).first), k};
         return {};
      }

      if (mPage < 0) {
         if (mScrollLeft.w > 0.0 && mScrollLeft.contains(x, y))
            return {kHitScrollLeft, -1};
         if (mScrollRight.w > 0.0 && mScrollRight.contains(x, y))
            return {kHitScrollRight, -1};
         for (const Strip &s : mStrips) {
            if (!s.r.contains(x, y))
               continue;
            const Hit own = channelHit(s, x, y);
            if (own.kind != kHitNothing)
               return own;
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
         if (mMaster.r.contains(x, y)) {
            const Hit own = channelHit(mMaster, x, y);
            if (own.kind != kHitNothing)
               return own;
         }
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
      if (mLayerEnv.contains(x, y))
         return envelopeHit(mLayerEnv, slot, x);
      if (mLayerFx.contains(x, y))
         return {kHitLayerFx, slot};
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

   // What a click on a strip's envelope, knobs or filter chip lands on.
   Hit channelHit(const Strip &s, double x, double y) const {
      if (s.env.contains(x, y))
         return envelopeHit(s.env, s.slot, x);
      if (s.fx.contains(x, y))
         return {kHitStripFx, s.slot};
      for (const MiniKnob &k : s.knobs)
         if (k.id != verdalis::kNoParam && k.r.contains(x, y))
            return {kHitKnob, s.slot, k.id};
      const uint32_t type = stripParams(s.slot).filterType;
      if (type != verdalis::kNoParam && s.filterType.contains(x, y))
         return {kHitFilterType, s.slot, type};
      return {};
   }

   // The segment of an envelope graph under x: a stage's curve, or on the
   // plateau the sustain level.
   Hit envelopeHit(const Rect &r, int slot, double x) const {
      const StripParams sp = stripParams(slot);
      const EnvShape e = envShape(mSpec.params, mDelegate, sp, r);
      int seg = EnvShape::kSustain;
      for (int i = 0; i < EnvShape::kNumSegments; ++i) {
         if (e.x[i + 1] > e.x[i] && x <= e.x[i + 1]) {
            seg = i;
            break;
         }
         if (i == EnvShape::kRelease && e.x[i + 1] > e.x[i])
            seg = i; // past the end: the release is the last thing there
      }
      const uint32_t ids[EnvShape::kNumSegments] = {sp.curve[0], sp.curve[1], sp.sustain,
                                                    sp.curve[2]};
      if (ids[seg] == verdalis::kNoParam)
         return {};
      return {kHitEnvelope, slot, ids[seg], seg};
   }

   // Which way a drag on a graph segment moves its parameter, so the segment
   // follows the hand: up bows a rising attack up by bending it towards +, but
   // bows a falling decay or release up by bending it towards -.
   static int envelopeSign(int segment) {
      return segment == EnvShape::kDecay || segment == EnvShape::kRelease ? -1 : 1;
   }

   // The parameter a hit drags or nudges, or kNoParam.
   static uint32_t hitParam(const Hit &h) {
      switch (h.kind) {
      case kHitKnob:
      case kHitEnvelope:
      case kHitFilterType:
         return h.param;
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
      case kHitEnvelope:
         return h.segment == EnvShape::kSustain
                   ? "Drag up or down to set the level held while the gate is open."
                   : "Drag up or down to bend this stage. Double-click puts back its natural "
                     "shape.";
      case kHitStripFx:
      case kHitLayerFx: {
         const int ch = h.slot < 0 ? kMasterFx : h.slot;
         std::string on;
         for (const int k : kRunOrder)
            if (fxOn(ch, k))
               on += std::string(on.empty() ? "" : ", ") + fxKind(k).name;
         const std::string who = ch == kMasterFx ? "the whole scene" : "this layer";
         return on.empty() ? "No effects on " + who + ". Click to open them."
                           : "Effects on " + who + ": " + on + ". Click to open them.";
      }
      case kHitFxPower:
         return std::string("Switch the ") + fxKind(h.segment).name +
                " on or off. Its settings are kept either way.";
      case kHitFxBack:
         return mPage < 0 ? "Back to the mixer." : "Back to the layer's own controls.";
      case kHitScrollLeft:
      case kHitScrollRight:
         return "More layers than fit: scroll the mixer. The wheel over a name does too.";
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
      if (mAddOpen || mTypeMenuParam >= 0) {
         mAddOpen = false;
         mTypeMenuParam = -1;
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
      // Whatever is pressed now is not a graph segment until it says so.
      mEnvSign = 0;

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

      if (mTypeMenuParam >= 0) {
         const uint32_t id = static_cast<uint32_t>(mTypeMenuParam);
         if (button == verdalis::kWheelUp || button == verdalis::kWheelDown) {
            nudge(id, button == verdalis::kWheelUp ? -1 : 1, false);
            return;
         }
         if (button == verdalis::kButtonLeft) {
            const int row = typeMenuRowAt(x, y);
            if (row >= 0)
               setParamNow(id, static_cast<double>(row));
            if (row >= 0 || !typeMenuPanel().contains(x, y))
               mTypeMenuParam = -1;
         }
         mDirty = true;
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
      const bool wheel = button == verdalis::kWheelUp || button == verdalis::kWheelDown;
      if (h.kind == kHitScrollLeft || h.kind == kHitScrollRight ||
          (wheel && h.kind == kHitStripLabel)) {
         const bool left = h.kind == kHitScrollLeft ||
                           (h.kind == kHitStripLabel && button == verdalis::kWheelUp);
         if (button == verdalis::kButtonLeft || wheel) {
            mStripFirst += left ? -1 : 1;
            buildLayout();
         }
         return;
      }
      if (h.kind == kHitFilterType) {
         if (wheel) {
            nudge(h.param, button == verdalis::kWheelUp ? 1 : -1, false);
         } else if (button == verdalis::kButtonRight) {
            setParamNow(h.param, mSpec.params[h.param].def);
         } else if (button == verdalis::kButtonLeft) {
            const Strip *st = h.slot == kSceneTarget ? &mMaster : stripFor(h.slot);
            if (st)
               openTypeMenu(h.param, st->filterType,
                            h.slot == kSceneTarget ? kTheme.accent : accentOf(slotType(h.slot)));
         }
         return;
      }
      const uint32_t id = hitParam(h);
      if (id != verdalis::kNoParam) {
         if (wheel) {
            const int sign = h.kind == kHitEnvelope ? envelopeSign(h.segment) : 1;
            nudge(id, (button == verdalis::kWheelUp ? 1 : -1) * sign, shift);
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
         mDragStartX = x;
         mDragStartY = y;
         mDragStartValue = mDelegate.guiParamValue(id);
         if (track.w > 0.0 || track.h > 0.0) {
            mDragHoriz = !isFader(h.kind);
            const double travel = mDragHoriz ? track.w : track.h - kHandleH;
            mDragUnitsPerPx = (d.max - d.min) / std::max(1.0, travel);
         } else {
            // A knob: dragged up and down at the panels' knobs' own rate.
            mDragHoriz = false;
            mDragUnitsPerPx = 0.0;
         }
         mEnvSign = h.kind == kHitEnvelope ? envelopeSign(h.segment) : 0;
         mDelegate.guiBeginEdit(id);
         return;
      }
      if (button != verdalis::kButtonLeft)
         return;

      switch (h.kind) {
      case kHitTab:
      case kHitStripLabel:
         mFxChannel = -1;
         mPage = h.slot == mPage ? -2 : mPage;
         showPage(h.slot);
         break;
      case kHitStripFx:
         openFx(h.slot < 0 ? kMasterFx : h.slot);
         break;
      case kHitLayerFx:
         openFx(h.slot);
         break;
      case kHitFxBack:
         closeFx();
         break;
      case kHitFxPower:
         toggleFx(mFxChannel, h.segment);
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

   // A preset in the browser loads when the button comes up, and that can
   // close the browser, after which the overlays are about the scene again.
   void onButtonRelease() override {
      PluginWindow::onButtonRelease();
      settleOverlayTarget();
   }

   void onMotion(double x, double y, bool shift) override {
      if (mDrag >= 0 && mEnvSign != 0) {
         // A graph segment follows the hand, which for a falling stage is the
         // opposite way to its parameter.
         const uint32_t id = static_cast<uint32_t>(mDrag);
         const ParamDesc &d = mSpec.params[id];
         const double fine = shift ? 0.2 : 1.0;
         double v = mDragStartValue + mEnvSign * (mDragStartY - y) * (d.max - d.min) / 140.0 * fine;
         v = std::min(d.max, std::max(d.min, v));
         mDelegate.guiSetParam(id, v);
         mDirty = true;
         return;
      }
      if (mDrag >= 0 || baseOverlayOpen()) {
         PluginWindow::onMotion(x, y, shift);
         return;
      }
      if (mTypeMenuParam >= 0) {
         const int hover = typeMenuRowAt(x, y);
         if (hover != mTypeMenuHover) {
            mTypeMenuHover = hover;
            mDirty = true;
         }
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
      const bool gate = mScene.sceneGateOpen();
      if (gate != mGateShown) {
         mGateShown = gate;
         mDirty = true;
      }
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

   int mStripFirst = 0;  // the first strip on screen, when they do not all fit
   int mStripsShown = 0;
   Rect mScrollLeft, mScrollRight;
   // Which way a drag on an envelope graph moves its parameter; 0 when the
   // drag is anything else.
   int mEnvSign = 0;
   int mTypeMenuParam = -1; // a filter type whose list is open, or -1
   Rect mTypeMenuAnchor;
   Rgb mTypeMenuAccent = kTheme.accent;
   int mTypeMenuHover = -1;
   bool mGateShown = false;

   Rect mLayerPrev, mLayerName, mLayerNext, mLayerSave, mLayerEnv, mLayerFx;

   // The effects view: whose effects it shows (a layer's slot, or kMasterFx),
   // or -1 when it is closed.
   int mFxChannel = -1;
   std::vector<std::vector<uint32_t>> mFxIds;
   std::vector<PanelSpec> mFxSpecs;
   std::vector<int> mFxPanelKind; // which effect each panel is
   Rect mFxBack;
   Rect mFxChain[kNumFxKinds];
   Rect mFxPower[kNumFxKinds];
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
