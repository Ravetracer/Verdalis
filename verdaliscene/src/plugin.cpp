#include <algorithm>
#include <atomic>
#include <bitset>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <clap/clap.h>

#include "verdalis/dsp/adsr.h"
#include "verdalis/dsp/denormals.h"
#include "verdalis/dsp/fastmath.h"
#include "verdalis/dsp/filters.h"
#include "verdalis/dsp/rng.h"
#include "verdalis/preset_library.h"

#include "entry.h"
#include "factories.h"
#include "fx/chain.h"
#include "params.h"
#include "presets_generated.h"
#include "scene_delegate.h"
#include "scene_preset.h"
#include "verdaliscene.h"

#ifdef VERDALISCENE_WITH_GUI
#include <chrono>
#include <thread>

#include "gui/gui.h"
#endif

namespace verdaliscene {

namespace {

const char *const kFeatures[] = {CLAP_PLUGIN_FEATURE_INSTRUMENT,
                                 CLAP_PLUGIN_FEATURE_SYNTHESIZER,
                                 CLAP_PLUGIN_FEATURE_STEREO,
                                 "ambient",
                                 "texture",
                                 "noise",
                                 nullptr};

const clap_plugin_descriptor_t kDescriptor = {
   CLAP_VERSION_INIT,  kPluginId,          kPluginName, kPluginVendor,
   kPluginUrl,         kPluginUrl,         kPluginUrl,  kPluginVersion,
   kPluginDescription, kFeatures,
};

// State chunk header. Values are stored per parameter id so that adding
// parameters later cannot break older saved state.
constexpr uint32_t kStateMagic = 0x4E435356u; // 'VSCN' little-endian
constexpr uint32_t kStateVersion = 1;

// Every layer is played by one held note at this velocity. The plugins do not
// agree on a neutral one -- RainyDay, ThunderClap, ChirpParade, InsectSwarm and
// NightLife reference their velocity controls to 1.0, ShoreBreak, SkyHowl,
// RiverFlow and CrackleBlaze to 0.5 -- but they do agree on how their presets
// were made: every fit and every demo in the suite was rendered through its
// plugin's -render tool, which plays 0.9. At 0.9 a layer sounds like its
// preset does in its own plugin's demos.
constexpr double kDroneVelocity = 0.9;

// How long a removed layer is given to fade out on its own release before it
// is cut. Long enough for any envelope the plugins offer.
constexpr double kMaxReleaseSeconds = 30.0;

constexpr double StereoDelayMax = fx::StereoDelay::kMaxSec;

// The mixer's controls are smoothed over about this long, so a fader drag or
// a mute is a fade rather than a click.
constexpr double kMixSmoothSeconds = 0.015;

verdalis::PresetLibrarySpec sceneLibrary() {
   return {presetContext(), kBuiltinPresets, kNumBuiltinPresets};
}

std::string readFile(const std::string &path) {
   std::ifstream in(path, std::ios::binary);
   if (!in)
      return {};
   std::ostringstream s;
   s << in.rdbuf();
   return s.str();
}

} // namespace

class VerdaliScenePlugin final : public SceneDelegate {
public:
   explicit VerdaliScenePlugin(const clap_host_t *host)
      : mHost(host), mValues(new std::atomic<double>[kTableSize]),
        mMods(new std::atomic<double>[kTableSize]) {
      const ParamDesc *table = fullTable();
      for (uint32_t i = 0; i < kTableSize; ++i) {
         mValues[i].store(table[i].def, std::memory_order_relaxed);
         mMods[i].store(0.0, std::memory_order_relaxed);
      }
      for (int s = 0; s < kNumSlots; ++s) {
         // Sized here, on the main thread, so the audio thread never allocates.
         const LayerType &t = layerType(slotType(s));
         mSlots[s].real.assign(t.paramCount, 0.0);
         // Every plugin in the suite calls its seed "seed".
         for (uint32_t i = 0; i < t.paramCount; ++i)
            if (std::strcmp(t.paramTable()[i].key, "seed") == 0)
               mSlots[s].seedParam = i;
         mSlots[s].rng.reseed(0x9E3779B9u * static_cast<uint32_t>(s + 1) +
                              static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this)));
         mSlotDirty[s].store(true, std::memory_order_relaxed);
         mPending[s].store(nullptr, std::memory_order_relaxed);
         mDead[s].store(nullptr, std::memory_order_relaxed);
         mLive[s].store(false, std::memory_order_relaxed);
         mMute[s].store(false, std::memory_order_relaxed);
         mSolo[s].store(false, std::memory_order_relaxed);
         mSlotPeakL[s].store(0.0f, std::memory_order_relaxed);
         mSlotPeakR[s].store(0.0f, std::memory_order_relaxed);
         mSlotVoices[s].store(0, std::memory_order_relaxed);
         mSlotEvents[s].store(0, std::memory_order_relaxed);
      }
      for (auto &d : mFxDirty)
         d.store(true, std::memory_order_relaxed);

      mPlugin.desc = &kDescriptor;
      mPlugin.plugin_data = this;
      mPlugin.init = [](const clap_plugin_t *p) { return self(p)->init(); };
      mPlugin.destroy = [](const clap_plugin_t *p) { delete self(p); };
      mPlugin.activate = [](const clap_plugin_t *p, double sr, uint32_t minF, uint32_t maxF) {
         return self(p)->activate(sr, minF, maxF);
      };
      mPlugin.deactivate = [](const clap_plugin_t *p) { self(p)->deactivate(); };
      mPlugin.start_processing = [](const clap_plugin_t *) { return true; };
      mPlugin.stop_processing = [](const clap_plugin_t *) {};
      mPlugin.reset = [](const clap_plugin_t *p) { self(p)->resetAudio(); };
      mPlugin.process = [](const clap_plugin_t *p, const clap_process_t *pr) {
         return self(p)->process(pr);
      };
      mPlugin.get_extension = [](const clap_plugin_t *p, const char *id) {
         return self(p)->getExtension(id);
      };
      mPlugin.on_main_thread = [](const clap_plugin_t *p) { self(p)->onMainThread(); };
   }

   ~VerdaliScenePlugin() override {
#ifdef VERDALISCENE_WITH_GUI
      // Hosts normally call gui.destroy() first, but a plugin must not depend
      // on that to avoid taking its window down with it.
      stopGuiClock();
      delete mGui;
#endif
      deleteEngines();
   }

   const clap_plugin_t *clapPlugin() const { return &mPlugin; }

private:
   static VerdaliScenePlugin *self(const clap_plugin_t *p) {
      return static_cast<VerdaliScenePlugin *>(p->plugin_data);
   }

   // ---------------------------------------------------------------- lifecycle

   bool init() { return true; }

   bool activate(double sampleRate, uint32_t /*minFrames*/, uint32_t maxFrames) {
      mSampleRate = sampleRate;
      mMaxFrames = std::max<uint32_t>(maxFrames, 1);
      mScratchL.assign(mMaxFrames, 0.0f);
      mScratchR.assign(mMaxFrames, 0.0f);
      mEnvBuf.assign(mMaxFrames, 0.0f);
      // The audio thread is not running yet, so every layer the scene holds
      // can be built straight into its slot.
      deleteEngines();
      for (int s = 0; s < kNumSlots; ++s) {
         SlotAudio &a = mSlots[s];
         a.resetPlayState();
         if (!slotWanted(s))
            continue;
         a.engine = newEngine(s);
         mLive[s].store(true, std::memory_order_release);
      }
      for (int c = 0; c < kNumFxChannels; ++c) {
         mFx[c].prepare(static_cast<float>(sampleRate), mMaxFrames);
         mFxDirty[c].store(true, std::memory_order_release);
      }
      mActive = true;
      ensureFx();
      for (int s = 0; s < kNumSlots; ++s)
         mSlotDirty[s].store(true, std::memory_order_release);
      mSceneDirty.store(true, std::memory_order_release);
      mEnv.reset();
      mGateOpen = false;
      mHpL.reset();
      mHpR.reset();
      mFilterL.reset();
      mFilterR.reset();
      mGainSmoothed = -1.0f;
      mActive = true;
      return true;
   }

   void deactivate() {
      mActive = false;
      deleteEngines();
      for (auto &c : mFx)
         c.releaseAll();
   }

   // [main-thread, not processing] Every engine, wherever it is in its life.
   void deleteEngines() {
      for (int s = 0; s < kNumSlots; ++s) {
         delete mSlots[s].engine;
         mSlots[s].engine = nullptr;
         delete mPending[s].exchange(nullptr, std::memory_order_acq_rel);
         delete mDead[s].exchange(nullptr, std::memory_order_acq_rel);
         mLive[s].store(false, std::memory_order_release);
      }
   }

   LayerEngine *newEngine(int slot) const {
      LayerEngine *e = layerType(slotType(slot)).create();
      e->prepare(mSampleRate, mMaxFrames);
      return e;
   }

   // [audio-thread] reset(): every engine starts over, which drops its note, so
   // the drone is struck again on the next block.
   void resetAudio() {
      for (auto &a : mSlots) {
         if (a.engine)
            a.engine->reset();
         a.resetPlayState();
      }
      for (auto &c : mFx)
         c.clear();
      mEnv.reset();
      mGateOpen = false;
      mHpL.reset();
      mHpR.reset();
      mFilterL.reset();
      mFilterR.reset();
   }

   // --------------------------------------------------------- engine handoff
   //
   // An engine is built and prepared on the main thread, because both allocate,
   // and handed to the audio thread through mPending. When a layer is removed
   // the audio thread lets it finish its release, then hands it back through
   // mDead for the main thread to delete. Neither side ever touches an engine
   // the other one owns, and an exchange decides every handover.

   bool slotWanted(int slot) const {
      return mValues[slotMixId(slot, kSlotActive)].load(std::memory_order_relaxed) >= 0.5;
   }

   // [main-thread] Builds an engine for every layer that wants one and has none.
   void ensureEngines() {
      if (!mActive)
         return;
      for (int s = 0; s < kNumSlots; ++s) {
         if (!slotWanted(s)) {
            // Asked for and then removed before the audio thread took it.
            delete mPending[s].exchange(nullptr, std::memory_order_acq_rel);
            continue;
         }
         if (mLive[s].load(std::memory_order_acquire) ||
             mPending[s].load(std::memory_order_acquire))
            continue;
         // An exchange rather than a store: in a host with no timer the window
         // runs on a thread of its own, and two callers building one slot's
         // engine at once must not leak the loser's.
         LayerEngine *built = newEngine(s);
         LayerEngine *none = nullptr;
         if (!mPending[s].compare_exchange_strong(none, built, std::memory_order_acq_rel))
            delete built;
      }
   }

   // [main-thread] Makes the buffers of every effect that is switched on and
   // has none yet: on the scene, and on every layer that exists. An effect's
   // buffers are kept once made, until the plugin is deactivated.
   void ensureFx() {
      if (!mActive)
         return;
      for (int c = 0; c < kNumFxChannels; ++c) {
         if (c != kMasterFx && !slotWanted(c))
            continue;
         for (int k = 0; k < kNumFxKinds; ++k) {
            const uint32_t on = fxParamId(c, fxKind(k).first);
            if (mValues[on].load(std::memory_order_relaxed) >= 0.5 && !mFx[c].ready(k))
               mFx[c].allocate(k);
         }
      }
   }

   void collectDead() {
      for (int s = 0; s < kNumSlots; ++s)
         delete mDead[s].exchange(nullptr, std::memory_order_acq_rel);
   }

   void onMainThread() {
      mCallbackPending.store(false, std::memory_order_release);
      collectDead();
      ensureEngines();
      ensureFx();
   }

   // [audio-thread]
   void requestMainThread() {
      if (!mHost || mCallbackPending.exchange(true, std::memory_order_acq_rel))
         return;
      mHost->request_callback(mHost);
   }

   // [audio-thread] Takes whatever the main thread has built, and asks for
   // whatever is still missing.
   void adoptEngines() {
      bool missing = false;
      for (int s = 0; s < kNumSlots; ++s) {
         SlotAudio &a = mSlots[s];
         if (LayerEngine *p = mPending[s].exchange(nullptr, std::memory_order_acq_rel)) {
            if (!a.engine) {
               a.engine = p;
               a.resetPlayState();
               mSlotDirty[s].store(true, std::memory_order_release);
               mLive[s].store(true, std::memory_order_release);
            } else {
               // A second one, built in the moment between this thread taking
               // the first and saying so. Hand it straight back.
               LayerEngine *none = nullptr;
               if (!mDead[s].compare_exchange_strong(none, p, std::memory_order_acq_rel))
                  mPending[s].store(p, std::memory_order_release);
               requestMainThread();
            }
         }
         if (!a.engine && slotWanted(s) && !mPending[s].load(std::memory_order_acquire))
            missing = true;
      }
      if (missing)
         requestMainThread();
   }

   // [audio-thread] Gives a finished engine back to the main thread. If the
   // last one has not been collected yet this waits a block.
   void retire(int slot) {
      SlotAudio &a = mSlots[slot];
      LayerEngine *none = nullptr;
      if (!mDead[slot].compare_exchange_strong(none, a.engine, std::memory_order_acq_rel)) {
         requestMainThread();
         return;
      }
      a.engine = nullptr;
      a.resetPlayState();
      mFx[slot].clear();
      mLive[slot].store(false, std::memory_order_release);
      mSlotPeakL[slot].store(0.0f, std::memory_order_relaxed);
      mSlotPeakR[slot].store(0.0f, std::memory_order_relaxed);
      mSlotVoices[slot].store(0, std::memory_order_relaxed);
      requestMainThread();
   }

   // ------------------------------------------------------------------- params

   double effective(uint32_t id) const {
      const ParamDesc &d = fullTable()[id];
      const double v = mValues[id].load(std::memory_order_relaxed) +
                       mMods[id].load(std::memory_order_relaxed);
      return v < d.min ? d.min : (v > d.max ? d.max : v);
   }

   double realValue(uint32_t id) const { return paramToReal(fullTable()[id], effective(id)); }

   void markDirty(uint32_t id) {
      const int fxChannel = fxChannelOf(id);
      if (fxChannel >= 0) {
         mFxDirty[fxChannel].store(true, std::memory_order_release);
         return;
      }
      const int slot = slotOf(id);
      if (slot < 0)
         mSceneDirty.store(true, std::memory_order_release);
      else
         mSlotDirty[slot].store(true, std::memory_order_release);
   }

   // [audio-thread] One channel's effects.
   void syncFx(int channel) {
      double real[kNumFxParams];
      for (uint32_t p = 0; p < kNumFxParams; ++p)
         real[p] = realValue(fxParamId(channel, p));
      mFx[channel].setParams(real, mTempo);
      if (mFx[channel].missing())
         requestMainThread();
   }

   // [audio-thread] The scene's envelope, filter and output.
   void syncScene() {
      const float sr = static_cast<float>(mSampleRate);
      mSustain = static_cast<float>(realValue(kParamSustain));
      mEnv.setParams(static_cast<float>(realValue(kParamAttack)) * 0.001f,
                     static_cast<float>(realValue(kParamDecay)) * 0.001f, mSustain,
                     static_cast<float>(realValue(kParamRelease)) * 0.001f, sr,
                     static_cast<float>(realValue(kParamAttackCurve)),
                     static_cast<float>(realValue(kParamDecayCurve)),
                     static_cast<float>(realValue(kParamReleaseCurve)));
      mGateMode = static_cast<int>(realValue(kParamGate));
      mTailsMode = static_cast<int>(realValue(kParamFxTails));

      const float hp = static_cast<float>(realValue(kParamHighpass));
      mHighpassBypass = hp <= 20.5f;
      mHpL.setCutoff(hp, sr);
      mHpR.setCutoff(hp, sr);

      const int type = static_cast<int>(realValue(kParamFilterType));
      const float cutoff = clampv(static_cast<float>(realValue(kParamFilterCutoff)), 20.0f, 0.49f * sr);
      const float reso = static_cast<float>(realValue(kParamFilterReso));
      // The same filter, and the same rule for leaving it out, as every
      // plugin's own output filter.
      mFilterBypass = (type == kFilterLowpass && cutoff >= 0.45f * sr) ||
                      (type == kFilterHighpass && cutoff <= 21.0f);
      mFilterL.setCutoff(cutoff, reso, sr);
      mFilterR.setCutoff(cutoff, reso, sr);
      mWLp = mWBp = mWHp = 0.0f;
      switch (type) {
      case kFilterLowpass:
         mWLp = 1.0f;
         break;
      case kFilterBandpass:
         mWBp = mFilterL.k();
         break;
      case kFilterHighpass:
         mWHp = 1.0f;
         break;
      case kFilterNotch:
      default:
         mWLp = 1.0f;
         mWHp = 1.0f;
         break;
      }
      mWidth = static_cast<float>(realValue(kParamWidth));
      mGainTarget = dbToGain(static_cast<float>(realValue(kParamGain)));
   }

   // [audio-thread] One layer's engine parameters and its place in the mix.
   void syncSlot(int slot) {
      SlotAudio &a = mSlots[slot];
      const LayerType &t = layerType(slotType(slot));
      const ParamDesc *table = fullTable();
      for (uint32_t i = 0; i < t.paramCount; ++i) {
         const uint32_t id = slotParamId(slot, i);
         const PinnedParam *pin = pinnedParam(t, i);
         a.real[i] = pin ? paramToReal(table[id], pin->raw) : realValue(id);
      }
      EnvelopeCurves curves;
      curves.attack = static_cast<float>(realValue(slotMixId(slot, kSlotAttackCurve)));
      curves.decay = static_cast<float>(realValue(slotMixId(slot, kSlotDecayCurve)));
      curves.release = static_cast<float>(realValue(slotMixId(slot, kSlotReleaseCurve)));
      if (a.engine)
         a.engine->setParams(a.real.data(), curves);

      if (t.shotLevelParam != kNoLayerParam) {
         a.shotLevelDb = static_cast<float>(a.real[t.shotLevelParam]);
         a.shotRatePerMin = static_cast<float>(realValue(slotMixId(slot, kSlotShotRate)));
      }
      if (t.firstEventRateParam != kNoLayerParam)
         a.firstRatePerMin = static_cast<float>(a.real[t.firstEventRateParam]);
      a.seed = a.seedParam != kNoLayerParam ? static_cast<int>(a.real[a.seedParam]) : 0;

      // The placement as a 2x2 matrix, so that switching between a stereo
      // balance and a mono point is one smoothed fade like any other move.
      // The fader comes after the layer's effects, so it is kept apart.
      a.gain = dbToGain(static_cast<float>(realValue(slotMixId(slot, kSlotLevel))));
      const float gain = 1.0f;
      const float pan = static_cast<float>(realValue(slotMixId(slot, kSlotPan)));
      const bool mono = static_cast<int>(realValue(slotMixId(slot, kSlotStereo))) == kStereoMono;
      if (mono) {
         // Equal power across the field, and equal power to the stereo image
         // it replaces: at the centre each side gets (L + R) / sqrt(2) * cos.
         const float theta = (pan + 1.0f) * 0.25f * 3.14159265358979f;
         const float l = std::cos(theta) * gain;
         const float r = std::sin(theta) * gain;
         a.target[0] = l;
         a.target[1] = l;
         a.target[2] = r;
         a.target[3] = r;
      } else {
         // A balance: the far side comes down, the near side stays where it
         // was, so a stereo layer keeps its own image.
         a.target[0] = gain * std::min(1.0f, 1.0f - pan);
         a.target[1] = 0.0f;
         a.target[2] = 0.0f;
         a.target[3] = gain * std::min(1.0f, 1.0f + pan);
      }
   }

   static uint32_t paramsCount(const clap_plugin_t *) {
      return static_cast<uint32_t>(hostParamIds().size());
   }

   static bool paramsGetInfo(const clap_plugin_t *, uint32_t index, clap_param_info_t *info) {
      const auto &ids = hostParamIds();
      if (index >= ids.size())
         return false;
      const ParamDesc &d = fullTable()[ids[index]];
      std::memset(info, 0, sizeof(*info));
      info->id = d.id;
      const int slot = slotOf(d.id);
      const bool active = slot >= 0 && localOf(d.id) == kSlotMixBase + kSlotActive;
      if (active) {
         // Whether a layer exists is the window's to say: adding one builds an
         // engine, and that is not something to do from an automation lane.
         info->flags = CLAP_PARAM_IS_STEPPED | CLAP_PARAM_IS_HIDDEN;
      } else {
         info->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_MODULATABLE;
         if (d.kind == ParamKind::Stepped || d.kind == ParamKind::Enum)
            info->flags |= CLAP_PARAM_IS_STEPPED;
         if (d.kind == ParamKind::Enum)
            info->flags |= CLAP_PARAM_IS_ENUM;
      }
      info->min_value = d.min;
      info->max_value = d.max;
      info->default_value = d.def;
      info->cookie = nullptr;
      std::snprintf(info->name, sizeof(info->name), "%s", d.name);
      std::snprintf(info->module, sizeof(info->module), "%s", d.module);
      return true;
   }

   static bool paramsGetValue(const clap_plugin_t *p, clap_id id, double *out) {
      if (!isRealParam(id))
         return false;
      *out = self(p)->mValues[id].load(std::memory_order_relaxed);
      return true;
   }

   static bool paramsValueToText(const clap_plugin_t *, clap_id id, double value, char *out,
                                 uint32_t size) {
      if (!isRealParam(id))
         return false;
      return paramValueToText(fullTable()[id], value, out, size);
   }

   static bool paramsTextToValue(const clap_plugin_t *, clap_id id, const char *text,
                                 double *out) {
      if (!isRealParam(id))
         return false;
      return paramTextToValue(fullTable()[id], text, out);
   }

   static void paramsFlush(const clap_plugin_t *p, const clap_input_events_t *in,
                           const clap_output_events_t *out) {
      VerdaliScenePlugin *plug = self(p);
      const uint32_t n = in ? in->size(in) : 0;
      for (uint32_t i = 0; i < n; ++i)
         plug->handleEvent(in->get(in, i));
      plug->drainGuiEdits(out, 0);
   }

   // ------------------------------------------------------------- gui edits
   //
   // The window runs on the main thread and must not write parameters behind
   // the host's back, or automation recording would never see a knob move. So
   // it posts into this single-producer queue and the audio side turns each
   // entry into a real CLAP event on its next process() or flush().

   enum class EditKind : uint8_t { GestureBegin, Value, GestureEnd };

   struct ParamEdit {
      uint32_t id;
      double value;
      EditKind kind;
   };

   static constexpr uint32_t kEditQueueSize = 1024; // power of two

   void pushGuiEdit(uint32_t id, double value, EditKind kind) {
      const uint32_t write = mEditWrite.load(std::memory_order_relaxed);
      const uint32_t read = mEditRead.load(std::memory_order_acquire);
      if (write - read >= kEditQueueSize)
         return; // full: the host is not calling us, dropping is the safe move
      mEditQueue[write & (kEditQueueSize - 1)] = {id, value, kind};
      mEditWrite.store(write + 1, std::memory_order_release);
      requestFlush();
   }

   void requestFlush() {
      if (!mHost)
         return;
      auto *hostParams =
         static_cast<const clap_host_params_t *>(mHost->get_extension(mHost, CLAP_EXT_PARAMS));
      if (hostParams && hostParams->request_flush)
         hostParams->request_flush(mHost);
   }

   void drainGuiEdits(const clap_output_events_t *out, uint32_t time) {
      const uint32_t write = mEditWrite.load(std::memory_order_acquire);
      uint32_t read = mEditRead.load(std::memory_order_relaxed);
      for (; read != write; ++read) {
         const ParamEdit &edit = mEditQueue[read & (kEditQueueSize - 1)];
         if (!out)
            continue;
         if (edit.kind == EditKind::Value) {
            clap_event_param_value_t ev;
            std::memset(&ev, 0, sizeof(ev));
            ev.header.size = sizeof(ev);
            ev.header.time = time;
            ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
            ev.header.type = CLAP_EVENT_PARAM_VALUE;
            ev.param_id = edit.id;
            ev.note_id = -1;
            ev.port_index = -1;
            ev.channel = -1;
            ev.key = -1;
            ev.value = edit.value;
            out->try_push(out, &ev.header);
         } else {
            clap_event_param_gesture_t ev;
            std::memset(&ev, 0, sizeof(ev));
            ev.header.size = sizeof(ev);
            ev.header.time = time;
            ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
            ev.header.type = edit.kind == EditKind::GestureBegin ? CLAP_EVENT_PARAM_GESTURE_BEGIN
                                                                 : CLAP_EVENT_PARAM_GESTURE_END;
            ev.param_id = edit.id;
            out->try_push(out, &ev.header);
         }
      }
      mEditRead.store(read, std::memory_order_release);
   }

   // Any path that moves parameters behind the host's back -- a preset, a
   // layer added or removed, saved state -- has to ask the host to re-read
   // them, and tell it the project has changed. [main-thread]
   void notifyParamValuesChanged() {
      if (!mHost)
         return;
      auto *hostParams =
         static_cast<const clap_host_params_t *>(mHost->get_extension(mHost, CLAP_EXT_PARAMS));
      if (hostParams && hostParams->rescan)
         hostParams->rescan(mHost, CLAP_PARAM_RESCAN_VALUES | CLAP_PARAM_RESCAN_TEXT);
   }

   void markStateDirty() {
      if (!mHost)
         return;
      auto *hostState =
         static_cast<const clap_host_state_t *>(mHost->get_extension(mHost, CLAP_EXT_STATE));
      if (hostState && hostState->mark_dirty)
         hostState->mark_dirty(mHost);
   }

   // [main-thread] After any change to which layers exist or what they hold.
   void structureChanged() {
      ensureEngines();
      ensureFx();
      notifyParamValuesChanged();
      markStateDirty();
   }

   // -------------------------------------------------------------- the scene

   // [main-thread] The scene as it stands, in the form a preset is written in.
   Scene currentScene() const {
      Scene scene = emptyScene();
      for (uint32_t i = 0; i < kNumSceneParams; ++i)
         scene.values[i] = mValues[i].load(std::memory_order_relaxed);
      for (uint32_t p = 0; p < kNumFxParams; ++p)
         scene.fx[p] = mValues[fxParamId(kMasterFx, p)].load(std::memory_order_relaxed);
      for (int s = 0; s < kNumSlots; ++s) {
         if (!slotWanted(s))
            continue;
         scene.layers.push_back(slotLayer(s));
      }
      return scene;
   }

   SceneLayer slotLayer(int slot) const {
      SceneLayer layer;
      layer.type = slotType(slot);
      const LayerType &t = layerType(layer.type);
      layer.values.resize(t.paramCount);
      for (uint32_t i = 0; i < t.paramCount; ++i)
         layer.values[i] = mValues[slotParamId(slot, i)].load(std::memory_order_relaxed);
      for (uint32_t p = 0; p < kNumSlotParams; ++p)
         layer.mix[p] = mValues[slotMixId(slot, p)].load(std::memory_order_relaxed);
      for (uint32_t p = 0; p < kNumFxParams; ++p)
         layer.fx[p] = mValues[fxParamId(slot, p)].load(std::memory_order_relaxed);
      layer.presetName = mSlotPreset[slot].name;
      return layer;
   }

   // [main-thread] Writes a layer into a slot and makes it exist.
   void writeLayer(int slot, const SceneLayer &layer, bool withMix) {
      const LayerType &t = layerType(slotType(slot));
      for (uint32_t i = 0; i < t.paramCount && i < layer.values.size(); ++i)
         mValues[slotParamId(slot, i)].store(layer.values[i], std::memory_order_relaxed);
      // The layer's place in the scene and its effects belong to the scene: a
      // plugin preset loaded into the layer leaves them alone.
      if (withMix) {
         for (uint32_t p = kSlotLevel; p < kNumSlotParams; ++p)
            mValues[slotMixId(slot, p)].store(layer.mix[p], std::memory_order_relaxed);
         for (uint32_t p = 0; p < kNumFxParams; ++p)
            mValues[fxParamId(slot, p)].store(layer.fx[p], std::memory_order_relaxed);
         mFxDirty[slot].store(true, std::memory_order_release);
      }
      mValues[slotMixId(slot, kSlotActive)].store(1.0, std::memory_order_relaxed);
      mSlotDirty[slot].store(true, std::memory_order_release);
   }

   // [main-thread] Replaces everything with `scene`: its parameters, and its
   // layers in the first free slots of their kind, in the order it lists them.
   void applyScene(const Scene &scene) {
      for (uint32_t i = 0; i < kNumSceneParams; ++i)
         mValues[i].store(scene.values[i], std::memory_order_relaxed);
      for (uint32_t p = 0; p < kNumFxParams; ++p)
         mValues[fxParamId(kMasterFx, p)].store(scene.fx[p], std::memory_order_relaxed);
      mFxDirty[kMasterFx].store(true, std::memory_order_release);
      for (int s = 0; s < kNumSlots; ++s) {
         mValues[slotMixId(s, kSlotActive)].store(0.0, std::memory_order_relaxed);
         mSlotPreset[s] = SlotPreset{};
         mSlotDirty[s].store(true, std::memory_order_release);
      }
      int used[kNumLayerTypes] = {};
      for (const SceneLayer &layer : scene.layers) {
         if (used[layer.type] >= kInstancesPerType)
            continue; // more of one kind than a scene holds
         const int slot = slotIndex(layer.type, used[layer.type]++);
         writeLayer(slot, layer, true);
         mSlotPreset[slot].name = layer.presetName;
      }
      // A preset is a starting point for the mixer too: nothing it brings in
      // should arrive muted by a button pressed for the scene before it.
      for (int s = 0; s < kNumSlots; ++s) {
         mMute[s].store(false, std::memory_order_relaxed);
         mSolo[s].store(false, std::memory_order_relaxed);
      }
      mSceneDirty.store(true, std::memory_order_release);
      structureChanged();
   }

   // -------------------------------------------------------------------- state

   static void putString(std::string &blob, const std::string &s) {
      const uint32_t n = static_cast<uint32_t>(std::min<size_t>(s.size(), 4096));
      blob.append(reinterpret_cast<const char *>(&n), sizeof(n));
      blob.append(s.data(), n);
   }

   static bool stateSave(const clap_plugin_t *p, const clap_ostream_t *stream) {
      VerdaliScenePlugin *plug = self(p);
      const auto &ids = hostParamIds();
      std::string blob;
      const uint32_t header[3] = {kStateMagic, kStateVersion, static_cast<uint32_t>(ids.size())};
      blob.append(reinterpret_cast<const char *>(header), sizeof(header));
      for (const uint32_t id : ids) {
         const double v = plug->mValues[id].load(std::memory_order_relaxed);
         blob.append(reinterpret_cast<const char *>(&id), sizeof(id));
         blob.append(reinterpret_cast<const char *>(&v), sizeof(v));
      }
      // What the preset bars say: not parameters, but the project should open
      // showing the same names it was saved with.
      putString(blob, plug->mScene.name);
      blob.push_back(plug->mScene.edited ? 1 : 0);
      for (int s = 0; s < kNumSlots; ++s) {
         putString(blob, plug->mSlotPreset[s].name);
         blob.push_back(plug->mSlotPreset[s].edited ? 1 : 0);
      }

      size_t written = 0;
      while (written < blob.size()) {
         const int64_t n = stream->write(stream, blob.data() + written, blob.size() - written);
         if (n <= 0)
            return false;
         written += static_cast<size_t>(n);
      }
      return true;
   }

   static bool readExactly(const clap_istream_t *stream, void *dst, size_t bytes) {
      size_t got = 0;
      char *out = static_cast<char *>(dst);
      while (got < bytes) {
         const int64_t n = stream->read(stream, out + got, bytes - got);
         if (n <= 0)
            return false;
         got += static_cast<size_t>(n);
      }
      return true;
   }

   static bool readString(const clap_istream_t *stream, std::string &out) {
      uint32_t n = 0;
      if (!readExactly(stream, &n, sizeof(n)) || n > 4096)
         return false;
      out.assign(n, '\0');
      return n == 0 || readExactly(stream, out.data(), n);
   }

   static bool stateLoad(const clap_plugin_t *p, const clap_istream_t *stream) {
      VerdaliScenePlugin *plug = self(p);
      uint32_t header[3] = {0, 0, 0};
      if (!readExactly(stream, header, sizeof(header)))
         return false;
      if (header[0] != kStateMagic || header[1] > kStateVersion)
         return false;
      const uint32_t count = header[2];
      if (count > kTableSize)
         return false;

      // Everything the state does not mention goes back to its default, so a
      // parameter added after the project was saved does not keep whatever
      // the last scene left in it.
      const ParamDesc *table = fullTable();
      for (uint32_t i = 0; i < kTableSize; ++i)
         plug->mValues[i].store(table[i].def, std::memory_order_relaxed);
      for (uint32_t i = 0; i < count; ++i) {
         uint32_t id = 0;
         double value = 0.0;
         if (!readExactly(stream, &id, sizeof(id)) || !readExactly(stream, &value, sizeof(value)))
            return false;
         if (!isRealParam(id))
            continue; // from a newer version: ignore
         plug->mValues[id].store(clampv(value, table[id].min, table[id].max),
                                 std::memory_order_relaxed);
      }

      // The names are optional: a state without them is still a whole state.
      std::string name;
      char edited = 0;
      if (readString(stream, name) && readExactly(stream, &edited, 1)) {
         plug->mScene.name = name;
         plug->mScene.edited = edited != 0;
         for (int s = 0; s < kNumSlots; ++s) {
            if (!readString(stream, name) || !readExactly(stream, &edited, 1))
               break;
            plug->mSlotPreset[s].name = name;
            plug->mSlotPreset[s].edited = edited != 0;
         }
      }
      plug->mScene.key.clear();
      for (int s = 0; s < kNumSlots; ++s)
         plug->mSlotDirty[s].store(true, std::memory_order_release);
      for (auto &d : plug->mFxDirty)
         d.store(true, std::memory_order_release);
      plug->mSceneDirty.store(true, std::memory_order_release);
      plug->ensureEngines();
      plug->ensureFx();
      plug->notifyParamValuesChanged();
      return true;
   }

   // ------------------------------------------------------------- scene presets

   void reportPresetError(uint32_t locationKind, const char *location, const char *loadKey,
                          const std::string &msg) {
      if (!mHost)
         return;
      auto *hostPreset = static_cast<const clap_host_preset_load_t *>(
         mHost->get_extension(mHost, CLAP_EXT_PRESET_LOAD));
      if (!hostPreset)
         hostPreset = static_cast<const clap_host_preset_load_t *>(
            mHost->get_extension(mHost, CLAP_EXT_PRESET_LOAD_COMPAT));
      if (hostPreset && hostPreset->on_error)
         hostPreset->on_error(mHost, locationKind, location, loadKey, 0, msg.c_str());
      auto *log = static_cast<const clap_host_log_t *>(mHost->get_extension(mHost, CLAP_EXT_LOG));
      if (log && log->log)
         log->log(mHost, CLAP_LOG_WARNING, msg.c_str());
   }

   static bool presetLoadFromLocation(const clap_plugin_t *p, uint32_t locationKind,
                                      const char *location, const char *loadKey) {
      VerdaliScenePlugin *plug = self(p);
      std::string text;
      if (locationKind == CLAP_PRESET_DISCOVERY_LOCATION_FILE) {
         if (!location || !location[0]) {
            plug->reportPresetError(locationKind, location, loadKey, "missing preset path");
            return false;
         }
         text = readFile(location);
         if (text.empty()) {
            plug->reportPresetError(locationKind, location, loadKey,
                                    std::string("cannot read ") + location);
            return false;
         }
      } else if (locationKind == CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN) {
         const BuiltinPreset *found = nullptr;
         for (unsigned i = 0; loadKey && i < kNumBuiltinPresets; ++i)
            if (std::strcmp(kBuiltinPresets[i].loadKey, loadKey) == 0)
               found = &kBuiltinPresets[i];
         if (!found) {
            plug->reportPresetError(locationKind, location, loadKey,
                                    std::string("unknown built-in preset '") +
                                       (loadKey ? loadKey : "") + "'");
            return false;
         }
         text = found->text;
      } else {
         plug->reportPresetError(locationKind, location, loadKey, "unsupported location kind");
         return false;
      }

      Scene scene;
      std::string error;
      if (!parseScene(text.c_str(), text.size(), scene, error)) {
         plug->reportPresetError(locationKind, location, loadKey, error);
         return false;
      }
      plug->applyScene(scene);
      plug->mScene.name = scene.name;
      plug->mScene.key = locationKind == CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN
                            ? std::string(loadKey)
                            : std::string(location);
      plug->mScene.edited = false;

      if (plug->mHost) {
         auto *hostPreset = static_cast<const clap_host_preset_load_t *>(
            plug->mHost->get_extension(plug->mHost, CLAP_EXT_PRESET_LOAD));
         if (!hostPreset)
            hostPreset = static_cast<const clap_host_preset_load_t *>(
               plug->mHost->get_extension(plug->mHost, CLAP_EXT_PRESET_LOAD_COMPAT));
         if (hostPreset && hostPreset->loaded)
            hostPreset->loaded(plug->mHost, locationKind, location, loadKey);
      }
      return true;
   }

   // --------------------------------------------------------------------- ports

   static uint32_t audioPortsCount(const clap_plugin_t *, bool isInput) {
      return isInput ? 0 : 1;
   }

   static bool audioPortsGet(const clap_plugin_t *, uint32_t index, bool isInput,
                             clap_audio_port_info_t *info) {
      if (isInput || index != 0)
         return false;
      std::memset(info, 0, sizeof(*info));
      info->id = 0;
      std::snprintf(info->name, sizeof(info->name), "Scene Out");
      info->flags = CLAP_AUDIO_PORT_IS_MAIN;
      info->channel_count = 2;
      info->port_type = CLAP_PORT_STEREO;
      info->in_place_pair = CLAP_INVALID_ID;
      return true;
   }

   // Notes open and close the scene when Gate is set to Notes. They never
   // reach a layer.
   static uint32_t notePortsCount(const clap_plugin_t *, bool isInput) {
      return isInput ? 1 : 0;
   }

   static bool notePortsGet(const clap_plugin_t *, uint32_t index, bool isInput,
                            clap_note_port_info_t *info) {
      if (!isInput || index != 0)
         return false;
      std::memset(info, 0, sizeof(*info));
      info->id = 0;
      info->supported_dialects = CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI;
      info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
      std::snprintf(info->name, sizeof(info->name), "Gate In");
      return true;
   }

   // How long one channel's effects ring on after their input has stopped, in
   // seconds; infinite for a frozen reverb or a delay that does not decay.
   double fxTailSeconds(int channel) const {
      auto fx = [&](uint32_t p) { return realValue(fxParamId(channel, p)); };
      double seconds = 0.0;
      if (fx(kFxReverbOn) >= 0.5) {
         if (fx(kFxReverbFreeze) >= 0.5)
            return HUGE_VAL;
         seconds += 1.2 * fx(kFxReverbDecay) * 0.001 + fx(kFxReverbPredelay) * 0.001;
      }
      if (fx(kFxDelayOn) >= 0.5) {
         const double fb = fx(kFxDelayFeedback);
         if (fb >= 0.98)
            return HUGE_VAL;
         // Down by 90 dB, at the longest time the delay can be set to.
         seconds += StereoDelayMax * std::ceil(-4.5 / std::log10(std::max(fb, 0.01)));
      }
      return seconds;
   }

   // A scene that is always open never ends by itself. One a key or the
   // transport closes ends when the longest release it can have has run out:
   // a layer's own, cut off at kMaxReleaseSeconds, or the scene's, whose
   // analogue shape takes about 1.4 times its Release to settle -- and, with
   // FX Tails on Ring Out, when the effects have rung out after that: the
   // longest of the layers' chains, then the scene's, which they feed.
   static uint32_t tailGet(const clap_plugin_t *p) {
      VerdaliScenePlugin *plug = self(p);
      bool layers = false;
      for (int s = 0; s < kNumSlots; ++s)
         layers = layers || plug->slotWanted(s);
      if (layers && static_cast<int>(plug->realValue(kParamGate)) == kGateAlways)
         return UINT32_MAX;
      double seconds =
         std::max(kMaxReleaseSeconds, 1.4 * plug->realValue(kParamRelease) * 0.001 + 1.0);
      if (static_cast<int>(plug->realValue(kParamFxTails)) == kTailsRingOut) {
         double layerTail = 0.0;
         for (int s = 0; s < kNumSlots; ++s)
            if (plug->slotWanted(s))
               layerTail = std::max(layerTail, plug->fxTailSeconds(s));
         seconds += layerTail + plug->fxTailSeconds(kMasterFx);
      }
      if (!std::isfinite(seconds))
         return UINT32_MAX;
      return static_cast<uint32_t>(std::min(plug->mSampleRate * seconds, 4.0e9));
   }

   // ------------------------------------------------------------------ process

   void noteGate(int16_t channel, int16_t key, bool on) {
      if (channel < 0 || key < 0) {
         if (!on)
            mHeldNotes.reset();
         return;
      }
      const size_t bit = static_cast<size_t>(channel & 15) * 128 + static_cast<size_t>(key & 127);
      mHeldNotes.set(bit, on);
   }

   void handleEvent(const clap_event_header_t *hdr) {
      if (!hdr || hdr->space_id != CLAP_CORE_EVENT_SPACE_ID)
         return;

      switch (hdr->type) {
      case CLAP_EVENT_NOTE_ON: {
         const auto *ev = reinterpret_cast<const clap_event_note_t *>(hdr);
         noteGate(ev->channel, ev->key, true);
         break;
      }
      case CLAP_EVENT_NOTE_OFF:
      case CLAP_EVENT_NOTE_CHOKE: {
         const auto *ev = reinterpret_cast<const clap_event_note_t *>(hdr);
         noteGate(ev->channel, ev->key, false);
         break;
      }
      case CLAP_EVENT_PARAM_VALUE: {
         const auto *ev = reinterpret_cast<const clap_event_param_value_t *>(hdr);
         if (!isRealParam(ev->param_id))
            break;
         const ParamDesc &d = fullTable()[ev->param_id];
         mValues[ev->param_id].store(clampv(ev->value, d.min, d.max), std::memory_order_relaxed);
         markDirty(ev->param_id);
         // Somebody other than the window switched a layer on or off. Its
         // engine is the main thread's to build or delete.
         if (slotOf(ev->param_id) >= 0 &&
             localOf(ev->param_id) == kSlotMixBase + kSlotActive)
            requestMainThread();
         break;
      }
      case CLAP_EVENT_PARAM_MOD: {
         const auto *ev = reinterpret_cast<const clap_event_param_mod_t *>(hdr);
         if (ev->note_id >= 0 || !isRealParam(ev->param_id))
            break;
         mMods[ev->param_id].store(ev->amount, std::memory_order_relaxed);
         markDirty(ev->param_id);
         break;
      }
      case CLAP_EVENT_MIDI: {
         const auto *ev = reinterpret_cast<const clap_event_midi_t *>(hdr);
         const uint8_t status = ev->data[0] & 0xF0;
         const int16_t channel = static_cast<int16_t>(ev->data[0] & 0x0F);
         const int16_t key = static_cast<int16_t>(ev->data[1] & 0x7F);
         const uint8_t vel = ev->data[2] & 0x7F;
         if (status == 0x90 && vel > 0)
            noteGate(channel, key, true);
         else if (status == 0x80 || (status == 0x90 && vel == 0))
            noteGate(channel, key, false);
         else if (status == 0xB0 && (ev->data[1] == 120 || ev->data[1] == 123))
            mHeldNotes.reset();
         break;
      }
      default:
         break;
      }
   }

   clap_process_status process(const clap_process_t *pr) {
      if (!pr || pr->audio_outputs_count < 1 || pr->audio_outputs[0].channel_count < 2)
         return CLAP_PROCESS_ERROR;

      // Contained to this call; the host's FPU mode is restored on the way out.
      const ScopedNoDenormals noDenormals;

      float *outL = pr->audio_outputs[0].data32[0];
      float *outR = pr->audio_outputs[0].data32[1];
      if (!outL || !outR)
         return CLAP_PROCESS_ERROR;

      const uint32_t numFrames = pr->frames_count;
      const clap_input_events_t *in = pr->in_events;
      const uint32_t numEvents = in ? in->size(in) : 0;

      // No transport means nobody is saying it is stopped.
      mTransportPlaying =
         !pr->transport || (pr->transport->flags & CLAP_TRANSPORT_IS_PLAYING) != 0;
      // A synced delay follows the host's tempo; 120 BPM when there is none.
      const double tempo = pr->transport && (pr->transport->flags & CLAP_TRANSPORT_HAS_TEMPO)
                              ? pr->transport->tempo
                              : 120.0;
      if (tempo > 0.0 && tempo != mTempo) {
         mTempo = tempo;
         for (auto &d : mFxDirty)
            d.store(true, std::memory_order_release);
      }

      drainGuiEdits(pr->out_events, 0);
      adoptEngines();

      uint32_t eventIndex = 0;
      uint32_t frame = 0;
      while (frame < numFrames) {
         // Consume every event scheduled at or before the current frame, so
         // parameter changes land sample-accurately.
         while (eventIndex < numEvents) {
            const clap_event_header_t *hdr = in->get(in, eventIndex);
            if (!hdr || hdr->time > frame)
               break;
            handleEvent(hdr);
            ++eventIndex;
         }
         uint32_t next = numFrames;
         if (eventIndex < numEvents) {
            const clap_event_header_t *hdr = in->get(in, eventIndex);
            if (hdr && hdr->time > frame && hdr->time < numFrames)
               next = hdr->time;
         }
         // The scratch buffers are as long as the host promised a block
         // would be; a host that breaks that promise still gets every frame.
         next = std::min(next, frame + mMaxFrames);

         if (mSceneDirty.exchange(false, std::memory_order_acq_rel))
            syncScene();
         for (int s = 0; s < kNumSlots; ++s)
            if (mSlots[s].engine && mSlotDirty[s].exchange(false, std::memory_order_acq_rel))
               syncSlot(s);
         for (int c = 0; c < kNumFxChannels; ++c)
            if (mFxDirty[c].exchange(false, std::memory_order_acq_rel))
               syncFx(c);

         renderBlock(outL + frame, outR + frame, next - frame);
         frame = next;
      }

      publishMeters(outL, outR, numFrames);
      return CLAP_PROCESS_CONTINUE;
   }

   bool gateWanted() const {
      switch (mGateMode) {
      case kGateTransport:
         return mTransportPlaying;
      case kGateNotes:
         return mHeldNotes.any();
      case kGateAlways:
      default:
         return true;
      }
   }

   void renderBlock(float *outL, float *outR, uint32_t n) {
      std::memset(outL, 0, n * sizeof(float));
      std::memset(outR, 0, n * sizeof(float));

      const bool gate = gateWanted();
      if (gate != mGateOpen) {
         mGateOpen = gate;
         if (gate)
            mEnv.gateOn();
         else
            mEnv.gateOff();
      } else if (gate && mEnv.isIdle() && mSustain > 0.0f) {
         // A sustain of zero lets the scene die away with the gate still open,
         // which is what an envelope does. Bringing the sustain back up has to
         // bring the scene back: with Gate on Always nothing else ever would.
         mEnv.gateOn();
      }

      mGateShown.store(mGateOpen, std::memory_order_relaxed);

      // A closed gate whose release has run out is silence, and the layers
      // are stopped rather than run unheard (see pauseSlot) -- all but their
      // effects' tails, with FX Tails on Ring Out.
      const bool paused = !mGateOpen && mEnv.isIdle();
      const bool ringOut = mTailsMode == kTailsRingOut;

      // The scene's envelope, once per sample. Where it is applied is what FX
      // Tails decides: on Ring Out it fades each layer on its way into its
      // effects, and the effects' output is left alone; on Release it fades
      // the scene on its way out, after every effect.
      float *env = mEnvBuf.data();
      if (paused) {
         std::fill(env, env + n, 0.0f);
      } else {
         for (uint32_t i = 0; i < n; ++i)
            env[i] = mEnv.tick();
      }

      bool anySolo = false;
      for (int s = 0; s < kNumSlots; ++s)
         anySolo = anySolo || (mSolo[s].load(std::memory_order_relaxed) && mSlots[s].engine);
      bool tails = false;
      for (int s = 0; s < kNumSlots; ++s) {
         if (!mSlots[s].engine)
            continue;
         if (!paused)
            renderSlot(s, outL, outR, n, anySolo, ringOut ? env : nullptr);
         else if (ringOut)
            tails = ringOutSlot(s, outL, outR, n, anySolo) || tails;
         else
            pauseSlot(s);
      }

      fx::Chain &master = mFx[kMasterFx];
      if (paused && !ringOut) {
         // On Release the scene's effects are inside its envelope, so nothing
         // of them is left once it has run out -- and nothing of this tail may
         // come back when the gate next opens.
         if (!mMasterCleared) {
            master.clear();
            mMasterCleared = true;
         }
         return;
      }
      mMasterCleared = false;
      // On Ring Out the scene's effects run while the scene does and for as
      // long as they, or the layers' tails feeding them, still sound.
      if (paused && !tails && master.quiet())
         return;

      const float sr = static_cast<float>(mSampleRate);
      const float smooth = 1.0f - std::exp(-1.0f / static_cast<float>(kMixSmoothSeconds * sr));
      if (mGainSmoothed < 0.0f)
         mGainSmoothed = mGainTarget;
      if (!paused || tails) {
         for (uint32_t i = 0; i < n; ++i) {
            float l = outL[i];
            float r = outR[i];
            if (!mHighpassBypass) {
               l = mHpL.tick(l);
               r = mHpR.tick(r);
            }
            if (!mFilterBypass) {
               float lp, bp, hp;
               mFilterL.tick(l, lp, bp, hp);
               l = mWLp * lp + mWBp * bp + mWHp * hp;
               mFilterR.tick(r, lp, bp, hp);
               r = mWLp * lp + mWBp * bp + mWHp * hp;
            }
            outL[i] = l;
            outR[i] = r;
         }
      }
      if (ringOut) {
         master.process(outL, outR, n);
      } else {
         master.process(outL, outR, n);
         for (uint32_t i = 0; i < n; ++i) {
            outL[i] *= env[i];
            outR[i] *= env[i];
         }
      }
      for (uint32_t i = 0; i < n; ++i) {
         const float mid = 0.5f * (outL[i] + outR[i]);
         const float side = 0.5f * (outL[i] - outR[i]) * mWidth;
         mGainSmoothed += smooth * (mGainTarget - mGainSmoothed);
         outL[i] = softClip((mid + side) * mGainSmoothed);
         outR[i] = softClip((mid - side) * mGainSmoothed);
      }
   }

   // [audio-thread] A layer while the scene is silent. Whatever is left of its
   // own release cannot be heard any more, so it is cut, and the next opening
   // starts the layer from nothing. A layer removed in the meantime has
   // nothing left to fade out, so it goes now.
   void pauseSlot(int slot) {
      SlotAudio &a = mSlots[slot];
      if (!slotWanted(slot)) {
         retire(slot);
         return;
      }
      if (a.noteHeld || a.releasing) {
         a.engine->allSoundOff();
         mFx[slot].clear();
         a.noteHeld = false;
         a.releasing = false;
      }
      a.firstDelay = -1.0;
      a.shotTimer = -1.0;
   }

   // [audio-thread] The same, with FX Tails on Ring Out: the layer stops as it
   // does there, but whatever its effects still hold rings on through its
   // fader until it has died away. Returns whether anything did.
   bool ringOutSlot(int slot, float *outL, float *outR, uint32_t n, bool anySolo) {
      SlotAudio &a = mSlots[slot];
      if (!slotWanted(slot)) {
         retire(slot);
         return false;
      }
      if (a.noteHeld || a.releasing) {
         a.engine->allSoundOff();
         a.noteHeld = false;
         a.releasing = false;
      }
      a.firstDelay = -1.0;
      a.shotTimer = -1.0;
      if (mFx[slot].quiet())
         return false;
      float *sl = mScratchL.data();
      float *sr2 = mScratchR.data();
      std::memset(sl, 0, n * sizeof(float));
      std::memset(sr2, 0, n * sizeof(float));
      mFx[slot].process(sl, sr2, n);
      mixSlot(slot, sl, sr2, outL, outR, n, anySolo);
      return true;
   }

   // [audio-thread] A layer's fader, into the scene's bus.
   void mixSlot(int slot, const float *sl, const float *sr2, float *outL, float *outR, uint32_t n,
                bool anySolo) {
      SlotAudio &a = mSlots[slot];
      const float smooth =
         1.0f - std::exp(-1.0f / static_cast<float>(kMixSmoothSeconds * mSampleRate));
      const bool audible = anySolo ? mSolo[slot].load(std::memory_order_relaxed)
                                   : !mMute[slot].load(std::memory_order_relaxed);
      const float gainTarget = audible ? a.gain : 0.0f;
      float peakL = 0.0f;
      float peakR = 0.0f;
      for (uint32_t i = 0; i < n; ++i) {
         a.g += smooth * (gainTarget - a.g);
         const float l = a.g * sl[i];
         const float r = a.g * sr2[i];
         outL[i] += l;
         outR[i] += r;
         peakL = std::max(peakL, std::fabs(l));
         peakR = std::max(peakR, std::fabs(r));
      }
      a.blockPeakL = std::max(a.blockPeakL, peakL);
      a.blockPeakR = std::max(a.blockPeakR, peakR);
   }

   // [audio-thread] One layer for one block. The gate is the layer's drone
   // note: it is struck when the gate opens and let go when it closes, so the
   // layer's own envelope -- the plugin's Attack and Release, bent by the
   // layer's curves -- plays inside the scene's.
   //
   // `env` is the scene's envelope for the block when FX Tails is on Ring Out,
   // applied between the layer's placement and its effects; null on Release,
   // where the scene applies it after everything.
   void renderSlot(int slot, float *outL, float *outR, uint32_t n, bool anySolo,
                   const float *env) {
      SlotAudio &a = mSlots[slot];
      LayerEngine *e = a.engine;
      const LayerType &t = layerType(slotType(slot));
      const double sr = mSampleRate;
      const bool wanted = slotWanted(slot);

      if (wanted && mGateOpen) {
         if (!a.noteHeld) {
            // A storm's first flash lands at a random moment within its first
            // average interval: not the instant the layer is added, which would
            // be a flash on cue, and not after the full Poisson wait either,
            // which with the sound's travel time on top left a storm silent
            // for most of a minute and read as a layer that does not work.
            if (a.firstDelay < 0.0) {
               // The scene's own randomness -- when the first flash comes,
               // when a bird sings -- follows the layer's seed as the
               // engine's does, so a fixed seed is a repeatable scene.
               if (a.seed > 0)
                  a.rng.reseed(static_cast<uint32_t>(a.seed) * 7919u +
                               static_cast<uint32_t>(slot) * 104729u);
               a.firstDelay = 0.0;
               if (t.firstEventRateParam != kNoLayerParam && a.firstRatePerMin > 0.0f)
                  a.firstDelay = a.rng.uniform() * (60.0f / a.firstRatePerMin) * sr;
            }
            a.firstDelay -= n;
            if (a.firstDelay <= 0.0) {
               e->noteOn(kDroneVelocity);
               a.noteHeld = true;
               a.releasing = false;
               a.firstDelay = -1.0;
               a.shotTimer = -1.0;
            }
         }
         if (a.noteHeld && t.shotLevelParam != kNoLayerParam && a.shotRatePerMin > 0.0f) {
            const float rate = a.shotRatePerMin / 60.0f;
            if (a.shotTimer < 0.0)
               a.shotTimer = a.rng.exponential(rate) * sr;
            a.shotTimer -= n;
            if (a.shotTimer <= 0.0) {
               if (a.shotLevelDb > -59.9f)
                  e->fireShot();
               a.shotTimer = a.rng.exponential(rate) * sr;
            }
         }
      } else {
         if (a.noteHeld) {
            e->noteOff();
            a.noteHeld = false;
            a.releasing = true;
            a.releaseSec = 0.0;
         }
         // The next opening draws its own first flash.
         a.firstDelay = -1.0;
         if (!wanted && !a.releasing) {
            // Never struck, or already faded: removed before its first note,
            // a storm still waiting for its first flash, or a layer removed
            // after the gate had closed and its release had run out.
            retire(slot);
            return;
         }
      }

      float *sl = mScratchL.data();
      float *sr2 = mScratchR.data();
      std::memset(sl, 0, n * sizeof(float));
      std::memset(sr2, 0, n * sizeof(float));
      e->process(sl, sr2, n);

      // Placed first, then the layer's effects, then its fader: a bird
      // placed to the left goes into its reverb from the left, and the
      // reverb spreads it into the room the way a real one would.
      const float smooth =
         1.0f - std::exp(-1.0f / static_cast<float>(kMixSmoothSeconds * mSampleRate));
      for (uint32_t i = 0; i < n; ++i) {
         for (int k = 0; k < 4; ++k)
            a.m[k] += smooth * (a.target[k] - a.m[k]);
         const float l = a.m[0] * sl[i] + a.m[1] * sr2[i];
         const float r = a.m[2] * sl[i] + a.m[3] * sr2[i];
         sl[i] = l;
         sr2[i] = r;
      }
      if (env) {
         for (uint32_t i = 0; i < n; ++i) {
            sl[i] *= env[i];
            sr2[i] *= env[i];
         }
      }
      mFx[slot].process(sl, sr2, n);
      mixSlot(slot, sl, sr2, outL, outR, n, anySolo);

      if (a.releasing) {
         a.releaseSec += n / sr;
         // Released when the engine is silent and its effects' tails have
         // died away too.
         if ((e->isSilent() && mFx[slot].quiet()) || a.releaseSec > kMaxReleaseSeconds) {
            a.releasing = false;
            if (!wanted)
               retire(slot);
         }
      }
   }

   // A peak meter that fell as fast as the signal would be unreadable, so the
   // published value decays towards the true peak over about 350 ms instead.
   void publishMeters(const float *l, const float *r, uint32_t frames) {
      if (frames == 0)
         return;
      const double seconds = static_cast<double>(frames) / (mSampleRate > 0 ? mSampleRate : 48000.0);
      const float fall = static_cast<float>(std::exp(-seconds / 0.35));
      float peakL = 0.0f;
      float peakR = 0.0f;
      for (uint32_t i = 0; i < frames; ++i) {
         peakL = std::max(peakL, std::fabs(l[i]));
         peakR = std::max(peakR, std::fabs(r[i]));
      }
      mFallingPeakL = std::max(peakL, mFallingPeakL * fall);
      mFallingPeakR = std::max(peakR, mFallingPeakR * fall);
      mPeakL.store(mFallingPeakL, std::memory_order_relaxed);
      mPeakR.store(mFallingPeakR, std::memory_order_relaxed);

      for (int s = 0; s < kNumSlots; ++s) {
         SlotAudio &a = mSlots[s];
         if (!a.engine)
            continue;
         a.fallL = std::max(a.blockPeakL, a.fallL * fall);
         a.fallR = std::max(a.blockPeakR, a.fallR * fall);
         a.blockPeakL = a.blockPeakR = 0.0f;
         mSlotPeakL[s].store(a.fallL, std::memory_order_relaxed);
         mSlotPeakR[s].store(a.fallR, std::memory_order_relaxed);
         mSlotVoices[s].store(a.engine->voiceCount(), std::memory_order_relaxed);
         mSlotEvents[s].store(a.engine->eventCount(), std::memory_order_relaxed);
      }
   }

   // ------------------------------------------------------------ SceneDelegate

public:
   double paramValue(uint32_t id) const override {
      return id < kTableSize ? mValues[id].load(std::memory_order_relaxed) : 0.0;
   }

   void beginEdit(uint32_t id) override {
      if (isRealParam(id))
         pushGuiEdit(id, 0.0, EditKind::GestureBegin);
   }

   void setParam(uint32_t id, double value) override {
      if (!isRealParam(id))
         return;
      const ParamDesc &d = fullTable()[id];
      const double clamped = clampv(value, d.min, d.max);
      mValues[id].store(clamped, std::memory_order_relaxed);
      markDirty(id);
      mScene.edited = true;
      const int slot = slotOf(id);
      if (slot >= 0 && localOf(id) < kSlotMixBase)
         mSlotPreset[slot].edited = true;
      pushGuiEdit(id, clamped, EditKind::Value);
   }

   void endEdit(uint32_t id) override {
      if (isRealParam(id))
         pushGuiEdit(id, 0.0, EditKind::GestureEnd);
   }

   bool layerActive(int slot) const override {
      return slot >= 0 && slot < kNumSlots && slotWanted(slot);
   }

   int addLayer(int type) override {
      if (type < 0 || type >= kNumLayerTypes)
         return -1;
      for (int i = 0; i < kInstancesPerType; ++i) {
         const int slot = slotIndex(type, i);
         if (slotWanted(slot))
            continue;
         const SceneLayer layer = freshLayer(type);
         writeLayer(slot, layer, true);
         mSlotPreset[slot] = SlotPreset{};
         mSlotPreset[slot].name = layer.presetName;
         mMute[slot].store(false, std::memory_order_relaxed);
         mSolo[slot].store(false, std::memory_order_relaxed);
         mScene.edited = true;
         structureChanged();
         return slot;
      }
      return -1;
   }

   void removeLayer(int slot) override {
      if (slot < 0 || slot >= kNumSlots || !slotWanted(slot))
         return;
      mValues[slotMixId(slot, kSlotActive)].store(0.0, std::memory_order_relaxed);
      mMute[slot].store(false, std::memory_order_relaxed);
      mSolo[slot].store(false, std::memory_order_relaxed);
      mScene.edited = true;
      structureChanged();
   }

   bool layerMuted(int slot) const override {
      return slot >= 0 && slot < kNumSlots && mMute[slot].load(std::memory_order_relaxed);
   }
   bool layerSoloed(int slot) const override {
      return slot >= 0 && slot < kNumSlots && mSolo[slot].load(std::memory_order_relaxed);
   }
   void setLayerMuted(int slot, bool on) override {
      if (slot >= 0 && slot < kNumSlots)
         mMute[slot].store(on, std::memory_order_relaxed);
   }
   void setLayerSoloed(int slot, bool on) override {
      if (slot >= 0 && slot < kNumSlots)
         mSolo[slot].store(on, std::memory_order_relaxed);
   }

   bool sceneGateOpen() const override { return mGateShown.load(std::memory_order_relaxed); }

   void outputPeaks(float &left, float &right) const override {
      left = mPeakL.load(std::memory_order_relaxed);
      right = mPeakR.load(std::memory_order_relaxed);
   }

   void layerPeaks(int slot, float &left, float &right) const override {
      left = right = 0.0f;
      if (slot < 0 || slot >= kNumSlots || !mLive[slot].load(std::memory_order_relaxed))
         return;
      left = mSlotPeakL[slot].load(std::memory_order_relaxed);
      right = mSlotPeakR[slot].load(std::memory_order_relaxed);
   }

   uint32_t layerVoices(int slot) const override {
      if (slot < 0 || slot >= kNumSlots || !mLive[slot].load(std::memory_order_relaxed))
         return 0;
      return mSlotVoices[slot].load(std::memory_order_relaxed);
   }

   uint32_t layerVoiceLimit(int slot) const override {
      if (slot < 0 || slot >= kNumSlots)
         return 1;
      const LayerType &t = layerType(slotType(slot));
      if (t.voiceLimitParam != kNoLayerParam)
         return std::max<uint32_t>(
            1, static_cast<uint32_t>(realValue(slotParamId(slot, t.voiceLimitParam))));
      return std::max<uint32_t>(1, t.voiceLimitFixed);
   }

   uint32_t layerEvents(int slot) const override {
      if (slot < 0 || slot >= kNumSlots)
         return 0;
      return mSlotEvents[slot].load(std::memory_order_relaxed);
   }

   // ------------------------------------------------------- preset libraries
   //
   // The scene's own library, and one per layer type -- which is that
   // plugin's own: its factory presets from the binary and the user's from
   // the same folder the plugin itself saves into. A layer preset saved here
   // shows up in the plugin, and one saved in the plugin shows up here.

   verdalis::PresetLibrarySpec librarySpec(int target) const {
      if (target < 0 || target >= kNumSlots)
         return sceneLibrary();
      return layerType(slotType(target)).library();
   }

   std::vector<GuiPreset> &libraryList(int target) {
      if (target < 0 || target >= kNumSlots) {
         if (!mScenePresetsScanned) {
            mScenePresets = verdalis::scanPresetLibrary(sceneLibrary());
            mScenePresetsScanned = true;
         }
         return mScenePresets;
      }
      const int type = slotType(target);
      if (!mLayerPresetsScanned[type]) {
         mLayerPresets[type] = verdalis::scanPresetLibrary(layerType(type).library());
         mLayerPresetsScanned[type] = true;
      }
      return mLayerPresets[type];
   }

   void rescanLibrary(int target) {
      if (target < 0 || target >= kNumSlots)
         mScenePresetsScanned = false;
      else
         mLayerPresetsScanned[slotType(target)] = false;
      libraryList(target);
   }

   const std::vector<GuiPreset> &presets(int target) override { return libraryList(target); }

   int currentPreset(int target) override {
      const std::vector<GuiPreset> &list = libraryList(target);
      const bool scene = target < 0 || target >= kNumSlots;
      const std::string &key = scene ? mScene.key : mSlotPreset[target].key;
      const std::string &name = scene ? mScene.name : mSlotPreset[target].name;
      if (!scene && !layerActive(target))
         return -1;
      if (!key.empty())
         for (size_t i = 0; i < list.size(); ++i)
            if (list[i].loadKey == key || list[i].path == key)
               return static_cast<int>(i);
      // Restored from a project or a scene: known by name only. The factory
      // shelf comes first in the list, so a factory preset wins a tie.
      if (!name.empty())
         for (size_t i = 0; i < list.size(); ++i)
            if (list[i].name == name)
               return static_cast<int>(i);
      return -1;
   }

   bool presetEdited(int target) const override {
      if (target < 0 || target >= kNumSlots)
         return mScene.edited;
      return mSlotPreset[target].edited;
   }

   static std::string presetText(const GuiPreset &entry, const verdalis::PresetLibrarySpec &lib) {
      if (!entry.loadKey.empty()) {
         for (unsigned i = 0; i < lib.builtinCount; ++i)
            if (entry.loadKey == lib.builtins[i].loadKey)
               return lib.builtins[i].text;
         return {};
      }
      return readFile(entry.path);
   }

   void loadPreset(int target, int index) override {
      std::vector<GuiPreset> &list = libraryList(target);
      if (index < 0 || index >= static_cast<int>(list.size()))
         return;
      const GuiPreset entry = list[static_cast<size_t>(index)];
      if (target < 0 || target >= kNumSlots) {
         if (!entry.loadKey.empty())
            presetLoadFromLocation(&mPlugin, CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN, nullptr,
                                   entry.loadKey.c_str());
         else
            presetLoadFromLocation(&mPlugin, CLAP_PRESET_DISCOVERY_LOCATION_FILE,
                                   entry.path.c_str(), nullptr);
         return;
      }
      if (!layerActive(target))
         return;
      const std::string text = presetText(entry, librarySpec(target));
      SceneLayer layer = slotLayer(target);
      std::string error;
      if (text.empty() || !applyLayerPreset(slotType(target), text, layer, error))
         return;
      // The plugin's own parameters only: where the layer sits in the scene
      // is the scene's, not the preset's.
      writeLayer(target, layer, false);
      mSlotPreset[target].name = entry.name;
      mSlotPreset[target].key = entry.loadKey.empty() ? entry.path : entry.loadKey;
      mSlotPreset[target].edited = false;
      mScene.edited = true;
      structureChanged();
   }

   std::string suggestedPresetName(int target) override {
      if (target < 0 || target >= kNumSlots)
         return mScene.name.empty() ? std::string("My Scene") : mScene.name;
      if (!mSlotPreset[target].name.empty())
         return mSlotPreset[target].name;
      return std::string("My ") + layerType(slotType(target)).label;
   }

   bool savePreset(int target, const std::string &input, std::string &error) override {
      std::string folder, name;
      verdalis::splitPresetFolder(input, folder, name);
      if (name.empty()) {
         error = "Give the preset a name after the folder.";
         return false;
      }
      const verdalis::PresetLibrarySpec lib = librarySpec(target);
      const std::string path = verdalis::userPresetPathIn(lib.ctx, folder, name);
      if (path.empty()) {
         error = "No user preset directory: neither XDG_CONFIG_HOME nor HOME is set.";
         return false;
      }
      const bool scene = target < 0 || target >= kNumSlots;
      std::string text;
      if (scene) {
         Scene s = currentScene();
         s.name = name;
         text = formatScene(s);
      } else {
         if (!layerActive(target)) {
            error = "That layer is no longer in the scene.";
            return false;
         }
         text = formatLayerPreset(slotLayer(target), name);
      }
      if (!writePresetFile(path, text, error))
         return false;
      rescanLibrary(target);
      if (scene) {
         mScene.name = name;
         mScene.key = path;
         mScene.edited = false;
      } else {
         mSlotPreset[target].name = name;
         mSlotPreset[target].key = path;
         mSlotPreset[target].edited = false;
      }
      return true;
   }

   std::vector<std::string> presetPacks(int target) override {
      return verdalis::presetPackFiles(librarySpec(target));
   }

   std::string packPathFor(int target, const std::string &folder) override {
      return verdalis::presetPackPathFor(librarySpec(target), folder);
   }

   bool exportPack(int target, const std::string &folder, const std::string &path,
                   std::string &error) override {
      return verdalis::exportPresetPack(librarySpec(target), libraryList(target), folder, path,
                                        error);
   }

   bool importPack(int target, const std::string &path, std::string &folder,
                   std::string &error) override {
      if (!verdalis::importPresetPack(librarySpec(target), path, folder, error))
         return false;
      rescanLibrary(target);
      return true;
   }

private:
#ifdef VERDALISCENE_WITH_GUI
   // Which windowing system this build's window speaks, and how the host's
   // parent handle reaches it. The window itself is in src/gui/gui.cpp.
#if defined(_WIN32)
   static constexpr const char *kWindowApi = CLAP_WINDOW_API_WIN32;
   static uintptr_t nativeHandle(const clap_window_t &w) {
      return reinterpret_cast<uintptr_t>(w.win32);
   }
#else
   static constexpr const char *kWindowApi = CLAP_WINDOW_API_X11;
   static uintptr_t nativeHandle(const clap_window_t &w) {
      return static_cast<uintptr_t>(w.x11);
   }
#endif

   // ----------------------------------------------------------- gui extension

   static bool guiIsApiSupported(const clap_plugin_t *, const char *api, bool isFloating) {
      return !isFloating && api && std::strcmp(api, kWindowApi) == 0;
   }

   static bool guiGetPreferredApi(const clap_plugin_t *, const char **api, bool *isFloating) {
      *api = kWindowApi;
      *isFloating = false;
      return true;
   }

   static bool guiCreate(const clap_plugin_t *p, const char *api, bool isFloating) {
      if (!guiIsApiSupported(p, api, isFloating))
         return false;
      VerdaliScenePlugin *plug = self(p);
      if (plug->mGui)
         return true;
      plug->mGui = verdaliscene::createGui(*plug);
      if (!plug->mGui)
         return false;
      plug->startGuiClock();
      return true;
   }

   static void guiDestroy(const clap_plugin_t *p) {
      VerdaliScenePlugin *plug = self(p);
      plug->stopGuiClock();
      delete plug->mGui;
      plug->mGui = nullptr;
   }

   static bool guiSetScale(const clap_plugin_t *p, double scale) {
      VerdaliScenePlugin *plug = self(p);
      if (!plug->mGui)
         return false;
      plug->mGui->setScale(scale);
      return true;
   }

   static bool guiGetSize(const clap_plugin_t *p, uint32_t *width, uint32_t *height) {
      VerdaliScenePlugin *plug = self(p);
      if (!plug->mGui)
         return false;
      plug->mGui->size(width, height);
      return true;
   }

   static bool guiCanResize(const clap_plugin_t *) { return true; }

   static bool guiGetResizeHints(const clap_plugin_t *p, clap_gui_resize_hints_t *hints) {
      VerdaliScenePlugin *plug = self(p);
      if (!plug->mGui || !hints)
         return false;
      uint32_t w = 0, h = 0;
      plug->mGui->designSize(&w, &h);
      hints->can_resize_horizontally = true;
      hints->can_resize_vertically = true;
      hints->preserve_aspect_ratio = true;
      hints->aspect_ratio_width = w;
      hints->aspect_ratio_height = h;
      return true;
   }

   static bool guiAdjustSize(const clap_plugin_t *p, uint32_t *width, uint32_t *height) {
      VerdaliScenePlugin *plug = self(p);
      if (!plug->mGui || !width || !height)
         return false;
      plug->mGui->fitSize(width, height);
      return true;
   }

   static bool guiSetSize(const clap_plugin_t *p, uint32_t width, uint32_t height) {
      VerdaliScenePlugin *plug = self(p);
      if (!plug->mGui)
         return false;
      return plug->mGui->resize(width, height);
   }

   static bool guiSetParent(const clap_plugin_t *p, const clap_window_t *window) {
      VerdaliScenePlugin *plug = self(p);
      if (!plug->mGui || !window || std::strcmp(window->api, kWindowApi) != 0)
         return false;
      return plug->mGui->embed(nativeHandle(*window));
   }

   static bool guiSetTransient(const clap_plugin_t *p, const clap_window_t *window) {
      VerdaliScenePlugin *plug = self(p);
      if (!plug->mGui || !window || std::strcmp(window->api, kWindowApi) != 0)
         return false;
      return plug->mGui->setTransientFor(nativeHandle(*window));
   }

   static void guiSuggestTitle(const clap_plugin_t *p, const char *title) {
      VerdaliScenePlugin *plug = self(p);
      if (plug->mGui)
         plug->mGui->setTitle(title);
   }

   static bool guiShow(const clap_plugin_t *p) {
      VerdaliScenePlugin *plug = self(p);
      if (!plug->mGui)
         return false;
      plug->mGui->show();
      return true;
   }

   static bool guiHide(const clap_plugin_t *p) {
      VerdaliScenePlugin *plug = self(p);
      if (!plug->mGui)
         return false;
      plug->mGui->hide();
      return true;
   }

   // ------------------------------------------------------------- gui clock
   //
   // The window is repainted from the host's timer, which is the main thread
   // and so the only place CLAP allows GUI work. Hosts are not required to
   // offer timers, though, and a window that never repaints is useless -- so
   // there is a fallback thread with its own X11 connection for those.

   static void guiOnTimer(const clap_plugin_t *p, clap_id timerId) {
      VerdaliScenePlugin *plug = self(p);
      if (plug->mGui && timerId == plug->mTimerId) {
         // The window is the most regular visitor the main thread gets, so
         // it is also where engines are built and collected when a host is
         // slow to answer request_callback -- or does not answer it at all,
         // which would otherwise leave a layer switched on by automation
         // without an engine for as long as the window is open.
         plug->collectDead();
         plug->ensureEngines();
         plug->ensureFx();
         plug->mGui->tick();
      }
   }

   void startGuiClock() {
      if (mHost) {
         auto *timer = static_cast<const clap_host_timer_support_t *>(
            mHost->get_extension(mHost, CLAP_EXT_TIMER_SUPPORT));
         if (timer && timer->register_timer && timer->register_timer(mHost, 33, &mTimerId))
            return;
      }
      mTimerId = CLAP_INVALID_ID;
      mGuiThreadRun.store(true, std::memory_order_release);
      mGuiThread = std::thread([this] {
         while (mGuiThreadRun.load(std::memory_order_acquire)) {
            if (mGui)
               mGui->tick();
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
         }
      });
   }

   void stopGuiClock() {
      if (mGuiThreadRun.exchange(false, std::memory_order_acq_rel)) {
         if (mGuiThread.joinable())
            mGuiThread.join();
         return;
      }
      if (mHost && mTimerId != CLAP_INVALID_ID) {
         auto *timer = static_cast<const clap_host_timer_support_t *>(
            mHost->get_extension(mHost, CLAP_EXT_TIMER_SUPPORT));
         if (timer && timer->unregister_timer)
            timer->unregister_timer(mHost, mTimerId);
      }
      mTimerId = CLAP_INVALID_ID;
   }
#else
   void stopGuiClock() {}
#endif // VERDALISCENE_WITH_GUI

   // ---------------------------------------------------------------- extensions

   const void *getExtension(const char *id) const {
      static const clap_plugin_params_t kParamsExt = {
         paramsCount, paramsGetInfo, paramsGetValue, paramsValueToText, paramsTextToValue,
         paramsFlush};
      static const clap_plugin_audio_ports_t kAudioPortsExt = {audioPortsCount, audioPortsGet};
      static const clap_plugin_note_ports_t kNotePortsExt = {notePortsCount, notePortsGet};
      static const clap_plugin_state_t kStateExt = {stateSave, stateLoad};
      static const clap_plugin_tail_t kTailExt = {tailGet};
      static const clap_plugin_preset_load_t kPresetLoadExt = {presetLoadFromLocation};
#ifdef VERDALISCENE_WITH_GUI
      static const clap_plugin_gui_t kGuiExt = {
         guiIsApiSupported, guiGetPreferredApi, guiCreate,      guiDestroy,
         guiSetScale,       guiGetSize,         guiCanResize,   guiGetResizeHints,
         guiAdjustSize,     guiSetSize,         guiSetParent,   guiSetTransient,
         guiSuggestTitle,   guiShow,            guiHide};
      static const clap_plugin_timer_support_t kTimerExt = {guiOnTimer};
#endif

      if (std::strcmp(id, CLAP_EXT_PARAMS) == 0)
         return &kParamsExt;
      if (std::strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0)
         return &kAudioPortsExt;
      if (std::strcmp(id, CLAP_EXT_NOTE_PORTS) == 0)
         return &kNotePortsExt;
      if (std::strcmp(id, CLAP_EXT_STATE) == 0)
         return &kStateExt;
      if (std::strcmp(id, CLAP_EXT_TAIL) == 0)
         return &kTailExt;
      if (std::strcmp(id, CLAP_EXT_PRESET_LOAD) == 0 ||
          std::strcmp(id, CLAP_EXT_PRESET_LOAD_COMPAT) == 0)
         return &kPresetLoadExt;
#ifdef VERDALISCENE_WITH_GUI
      if (std::strcmp(id, CLAP_EXT_GUI) == 0)
         return &kGuiExt;
      if (std::strcmp(id, CLAP_EXT_TIMER_SUPPORT) == 0)
         return &kTimerExt;
#endif
      return nullptr;
   }

   // ------------------------------------------------------------------ state

   // One layer slot as the audio thread sees it.
   struct SlotAudio {
      LayerEngine *engine = nullptr;
      bool noteHeld = false;
      bool releasing = false;
      double releaseSec = 0.0;
      double firstDelay = -1.0; // samples to the drone note; < 0 is not drawn yet
      double shotTimer = -1.0;  // samples to the next shot; < 0 is not drawn yet
      std::vector<double> real; // the plugin's parameters, real-world values
      uint32_t seedParam = kNoLayerParam;
      int seed = 0;
      float shotLevelDb = -60.0f;
      float shotRatePerMin = 0.0f;
      float firstRatePerMin = 0.0f;
      float target[4] = {0.0f, 0.0f, 0.0f, 0.0f}; // placement: LL, LR, RL, RR
      float m[4] = {0.0f, 0.0f, 0.0f, 0.0f};      // the same, smoothed
      float gain = 0.0f;                          // the fader, after the effects
      float g = 0.0f;                             // the same, smoothed, with mute and solo
      float blockPeakL = 0.0f, blockPeakR = 0.0f;
      float fallL = 0.0f, fallR = 0.0f;
      verdalis::Rng rng;

      void resetPlayState() {
         noteHeld = false;
         releasing = false;
         releaseSec = 0.0;
         firstDelay = -1.0;
         shotTimer = -1.0;
         for (float &v : m)
            v = 0.0f;
         g = 0.0f;
         blockPeakL = blockPeakR = fallL = fallR = 0.0f;
      }
   };

   // What a preset bar shows: the name of what was loaded, where it came
   // from when that is known, and whether it has been changed since.
   struct SlotPreset {
      std::string name;
      std::string key; // a factory preset's load key, or a file's path
      bool edited = false;
   };

   clap_plugin_t mPlugin{};
   const clap_host_t *mHost = nullptr;

   std::unique_ptr<std::atomic<double>[]> mValues;
   std::unique_ptr<std::atomic<double>[]> mMods;
   std::atomic<bool> mSceneDirty{true};
   std::atomic<bool> mSlotDirty[kNumSlots];

   SlotAudio mSlots[kNumSlots];
   // Every layer's effects and, last, the scene's (kMasterFx).
   fx::Chain mFx[kNumFxChannels];
   std::atomic<bool> mFxDirty[kNumFxChannels];
   double mTempo = 120.0; // audio thread
   std::atomic<LayerEngine *> mPending[kNumSlots];
   std::atomic<LayerEngine *> mDead[kNumSlots];
   std::atomic<bool> mLive[kNumSlots];
   std::atomic<bool> mMute[kNumSlots];
   std::atomic<bool> mSolo[kNumSlots];
   std::atomic<bool> mCallbackPending{false};
   bool mActive = false; // main thread

   std::atomic<float> mSlotPeakL[kNumSlots];
   std::atomic<float> mSlotPeakR[kNumSlots];
   std::atomic<uint32_t> mSlotVoices[kNumSlots];
   std::atomic<uint32_t> mSlotEvents[kNumSlots];
   std::atomic<float> mPeakL{0.0f};
   std::atomic<float> mPeakR{0.0f};
   float mFallingPeakL = 0.0f; // audio thread only
   float mFallingPeakR = 0.0f;

   // The scene's own processing, audio thread only.
   verdalis::Adsr mEnv;
   int mGateMode = kGateAlways;
   int mTailsMode = kTailsRingOut;
   bool mMasterCleared = false;
   float mSustain = 1.0f;
   bool mGateOpen = false;
   std::atomic<bool> mGateShown{false}; // mGateOpen, for the window
   bool mTransportPlaying = true;
   std::bitset<16 * 128> mHeldNotes;
   verdalis::Hp2 mHpL, mHpR;
   bool mHighpassBypass = true;
   verdalis::Svf mFilterL, mFilterR;
   bool mFilterBypass = true;
   float mWLp = 1.0f, mWBp = 0.0f, mWHp = 0.0f;
   float mWidth = 1.0f;
   float mGainTarget = 1.0f;
   float mGainSmoothed = -1.0f;
   std::vector<float> mScratchL, mScratchR;
   std::vector<float> mEnvBuf; // the scene's envelope over one block
   double mSampleRate = 48000.0;
   uint32_t mMaxFrames = 1;

   ParamEdit mEditQueue[kEditQueueSize];
   std::atomic<uint32_t> mEditWrite{0};
   std::atomic<uint32_t> mEditRead{0};

   // Presets, main thread only.
   SlotPreset mScene;
   SlotPreset mSlotPreset[kNumSlots];
   std::vector<GuiPreset> mScenePresets;
   bool mScenePresetsScanned = false;
   std::vector<GuiPreset> mLayerPresets[kNumLayerTypes];
   bool mLayerPresetsScanned[kNumLayerTypes] = {};

#ifdef VERDALISCENE_WITH_GUI
   Gui *mGui = nullptr;
   clap_id mTimerId = CLAP_INVALID_ID;
   std::thread mGuiThread;
   std::atomic<bool> mGuiThreadRun{false};
#endif
};

// ------------------------------------------------------------- plugin factory

namespace {

uint32_t factoryCount(const clap_plugin_factory_t *) { return 1; }

const clap_plugin_descriptor_t *factoryGetDescriptor(const clap_plugin_factory_t *,
                                                     uint32_t index) {
   return index == 0 ? &kDescriptor : nullptr;
}

const clap_plugin_t *factoryCreate(const clap_plugin_factory_t *, const clap_host_t *host,
                                   const char *pluginId) {
   if (!host || !clap_version_is_compatible(host->clap_version))
      return nullptr;
   if (!pluginId || std::strcmp(pluginId, kPluginId) != 0)
      return nullptr;
   auto *plug = new VerdaliScenePlugin(host);
   return plug->clapPlugin();
}

} // namespace

const clap_plugin_factory_t gPluginFactory = {factoryCount, factoryGetDescriptor, factoryCreate};

} // namespace verdaliscene

// --------------------------------------------------------------------- entry
//
// The clap_entry structure itself is not here: it is assembled per plugin
// format from the three functions below, so that the .clap and the .vst3 can be
// built from one copy of the plugin. See entry.h.

extern "C" {

bool verdalisceneEntryInit(const char *) { return true; }
void verdalisceneEntryDeinit() {}

const void *verdalisceneEntryGetFactory(const char *factoryId) {
   if (!factoryId)
      return nullptr;
   if (std::strcmp(factoryId, CLAP_PLUGIN_FACTORY_ID) == 0)
      return &verdaliscene::gPluginFactory;
   if (std::strcmp(factoryId, CLAP_PRESET_DISCOVERY_FACTORY_ID) == 0 ||
       std::strcmp(factoryId, CLAP_PRESET_DISCOVERY_FACTORY_ID_COMPAT) == 0)
      return verdaliscene::presetDiscoveryFactory();
   return nullptr;
}
}
