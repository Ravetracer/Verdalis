#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <clap/clap.h>

#include "verdalis/dsp/denormals.h"
#include "verdalis/dsp/fastmath.h"
#include "dsp/rain_engine.h"
#include "factories.h"
#include "params.h"
#include "presets_generated.h"
#include "rainyday.h"

#ifdef RAINYDAY_WITH_GUI
#include <chrono>
#include <thread>

#include <filesystem>

#include "gui/gui.h"
#endif

namespace rainyday {

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
constexpr uint32_t kStateMagic = 0x44594E52u; // 'RNYD' little-endian
constexpr uint32_t kStateVersion = 1;

} // namespace

#ifdef RAINYDAY_WITH_GUI
class RainyDayPlugin final : public GuiDelegate {
#else
class RainyDayPlugin {
#endif
public:
   explicit RainyDayPlugin(const clap_host_t *host) : mHost(host) {
      for (uint32_t i = 0; i < kNumParams; ++i) {
         mValues[i].store(paramTable()[i].def, std::memory_order_relaxed);
         mMods[i].store(0.0, std::memory_order_relaxed);
      }
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
      mPlugin.reset = [](const clap_plugin_t *p) { self(p)->mEngine.reset(); };
      mPlugin.process = [](const clap_plugin_t *p, const clap_process_t *pr) {
         return self(p)->process(pr);
      };
      mPlugin.get_extension = [](const clap_plugin_t *p, const char *id) {
         return self(p)->getExtension(id);
      };
      mPlugin.on_main_thread = [](const clap_plugin_t *) {};
   }

#ifdef RAINYDAY_WITH_GUI
   ~RainyDayPlugin() override {
      // Hosts normally call gui.destroy() first, but a plugin must not depend
      // on that to avoid taking its window down with it.
      stopGuiClock();
      delete mGui;
   }
#endif

   const clap_plugin_t *clapPlugin() const { return &mPlugin; }

private:
   static RainyDayPlugin *self(const clap_plugin_t *p) {
      return static_cast<RainyDayPlugin *>(p->plugin_data);
   }

   // ---------------------------------------------------------------- lifecycle

   bool init() { return true; }

   bool activate(double sampleRate, uint32_t /*minFrames*/, uint32_t maxFrames) {
      mSampleRate = sampleRate;
      mEngine.prepare(sampleRate, maxFrames);
      mParamsDirty.store(true, std::memory_order_release);
      return true;
   }

   void deactivate() {}

   // ------------------------------------------------------------------- params

   double effective(uint32_t id) const {
      const ParamDesc &d = paramTable()[id];
      const double v = mValues[id].load(std::memory_order_relaxed) +
                       mMods[id].load(std::memory_order_relaxed);
      return v < d.min ? d.min : (v > d.max ? d.max : v);
   }

   double realValue(uint32_t id) const { return paramToReal(paramTable()[id], effective(id)); }

   void syncEngineParams() {
      EngineParams p;
      p.gain = dbToGain(static_cast<float>(realValue(kParamGain)));
      p.densityHz = static_cast<float>(realValue(kParamDensity));
      p.clumping = static_cast<float>(realValue(kParamClumping));
      p.dropPitchHz = static_cast<float>(realValue(kParamDropPitch));
      p.pitchSpreadOct = static_cast<float>(realValue(kParamPitchSpread));
      p.dropDecaySec = static_cast<float>(realValue(kParamDropDecay)) * 0.001f;
      p.decaySpread = static_cast<float>(realValue(kParamDecaySpread));
      p.tonality = static_cast<float>(realValue(kParamTonality));
      p.impact = static_cast<float>(realValue(kParamImpact));
      p.splash = static_cast<float>(realValue(kParamSplash));
      p.slosh = static_cast<float>(realValue(kParamSlosh));
      p.levelSpread = static_cast<float>(realValue(kParamLevelSpread));
      p.chirp = static_cast<float>(realValue(kParamChirp));
      p.bubbleChance = static_cast<float>(realValue(kParamBubble));
      p.surface = static_cast<int>(realValue(kParamSurface));
      p.trickleGain = dbToGain(static_cast<float>(realValue(kParamTrickleLevel)));
      p.trickleRateHz = static_cast<float>(realValue(kParamTrickleRate));
      p.trickleSizeMm = static_cast<float>(realValue(kParamTrickleSize));
      p.trickleSpreadOct = static_cast<float>(realValue(kParamTrickleSpread));
      p.trickleDecaySec = static_cast<float>(realValue(kParamTrickleDecay)) * 0.001f;
      p.trickleImpact = static_cast<float>(realValue(kParamTrickleImpact));
      p.stoneToneHz = static_cast<float>(realValue(kParamStoneTone));
      p.trickleSplash = static_cast<float>(realValue(kParamTrickleSplash));
      p.noteTracking = static_cast<float>(realValue(kParamNoteTracking));

      p.bedGain = dbToGain(static_cast<float>(realValue(kParamBedLevel)));
      p.bedTone = static_cast<float>(realValue(kParamBedTone));
      p.bedBody = static_cast<float>(realValue(kParamBedBody));
      p.bedDrift = static_cast<float>(realValue(kParamBedDrift));
      p.bedWidth = static_cast<float>(realValue(kParamBedWidth));
      p.bedPan = static_cast<float>(realValue(kParamBedPan));

      p.width = static_cast<float>(realValue(kParamWidth));
      p.dropPan = static_cast<float>(realValue(kParamDropPan));
      p.distance = static_cast<float>(realValue(kParamDistance));
      p.air = static_cast<float>(realValue(kParamAir));
      p.spaceAmount = static_cast<float>(realValue(kParamSpaceAmount));
      p.spaceSize = static_cast<float>(realValue(kParamSpaceSize));
      p.spaceDamping = static_cast<float>(realValue(kParamSpaceDamping));

      p.filterType = static_cast<int>(realValue(kParamFilterType));
      p.highpassHz = static_cast<float>(realValue(kParamHighpass));
      p.filterCutoffHz = static_cast<float>(realValue(kParamFilterCutoff));
      p.filterReso = static_cast<float>(realValue(kParamFilterReso));
      p.filterKeyTrack = static_cast<float>(realValue(kParamFilterKeyTrack));

      p.attackSec = static_cast<float>(realValue(kParamAttack)) * 0.001f;
      p.decaySec = static_cast<float>(realValue(kParamDecay)) * 0.001f;
      p.sustain = static_cast<float>(realValue(kParamSustain));
      p.releaseSec = static_cast<float>(realValue(kParamRelease)) * 0.001f;
      p.velToLevel = static_cast<float>(realValue(kParamVelToLevel));
      p.velToDensity = static_cast<float>(realValue(kParamVelToDensity));

      p.maxDroplets = static_cast<int>(realValue(kParamMaxDroplets));
      p.seed = static_cast<int>(realValue(kParamSeed));

      mEngine.setParams(p);
   }

   static uint32_t paramsCount(const clap_plugin_t *) { return kNumParams; }

   static bool paramsGetInfo(const clap_plugin_t *, uint32_t index, clap_param_info_t *info) {
      if (index >= kNumParams)
         return false;
      const ParamDesc &d = paramTable()[index];
      std::memset(info, 0, sizeof(*info));
      info->id = d.id;
      info->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_MODULATABLE;
      if (d.kind == ParamKind::Stepped || d.kind == ParamKind::Enum)
         info->flags |= CLAP_PARAM_IS_STEPPED;
      if (d.kind == ParamKind::Enum)
         info->flags |= CLAP_PARAM_IS_ENUM;
      info->min_value = d.min;
      info->max_value = d.max;
      info->default_value = d.def;
      info->cookie = nullptr;
      std::snprintf(info->name, sizeof(info->name), "%s", d.name);
      std::snprintf(info->module, sizeof(info->module), "%s", d.module);
      return true;
   }

   static bool paramsGetValue(const clap_plugin_t *p, clap_id id, double *out) {
      if (id >= kNumParams)
         return false;
      *out = self(p)->mValues[id].load(std::memory_order_relaxed);
      return true;
   }

   static bool paramsValueToText(const clap_plugin_t *, clap_id id, double value, char *out,
                                 uint32_t size) {
      const ParamDesc *d = paramById(id);
      if (!d)
         return false;
      return paramValueToText(*d, value, out, size);
   }

   static bool paramsTextToValue(const clap_plugin_t *, clap_id id, const char *text,
                                 double *out) {
      const ParamDesc *d = paramById(id);
      if (!d)
         return false;
      return paramTextToValue(*d, text, out);
   }

   static void paramsFlush(const clap_plugin_t *p, const clap_input_events_t *in,
                           const clap_output_events_t *out) {
      RainyDayPlugin *plug = self(p);
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

   static constexpr uint32_t kEditQueueSize = 512; // power of two

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

   // -------------------------------------------------------------------- state

   static bool stateSave(const clap_plugin_t *p, const clap_ostream_t *stream) {
      RainyDayPlugin *plug = self(p);
      std::string blob;
      const uint32_t header[3] = {kStateMagic, kStateVersion, kNumParams};
      blob.append(reinterpret_cast<const char *>(header), sizeof(header));
      for (uint32_t i = 0; i < kNumParams; ++i) {
         const uint32_t id = paramTable()[i].id;
         const double v = plug->mValues[i].load(std::memory_order_relaxed);
         blob.append(reinterpret_cast<const char *>(&id), sizeof(id));
         blob.append(reinterpret_cast<const char *>(&v), sizeof(v));
      }

      size_t written = 0;
      while (written < blob.size()) {
         const int64_t n =
            stream->write(stream, blob.data() + written, blob.size() - written);
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

   static bool stateLoad(const clap_plugin_t *p, const clap_istream_t *stream) {
      RainyDayPlugin *plug = self(p);
      uint32_t header[3] = {0, 0, 0};
      if (!readExactly(stream, header, sizeof(header)))
         return false;
      if (header[0] != kStateMagic || header[1] > kStateVersion)
         return false;

      const uint32_t count = header[2];
      // Guard against a corrupt count claiming a huge payload.
      if (count > 4096)
         return false;

      for (uint32_t i = 0; i < count; ++i) {
         uint32_t id = 0;
         double value = 0.0;
         if (!readExactly(stream, &id, sizeof(id)) ||
             !readExactly(stream, &value, sizeof(value)))
            return false;
         const ParamDesc *d = paramById(id);
         if (!d)
            continue; // unknown id from a newer version: ignore
         plug->mValues[id].store(clampv(value, d->min, d->max), std::memory_order_relaxed);
      }
      plug->mParamsDirty.store(true, std::memory_order_release);
      plug->notifyParamValuesChanged();
      return true;
   }

   // -------------------------------------------------------------- preset load

   void applyPreset(const PresetData &preset) {
      for (const auto &kv : preset.values) {
         const ParamDesc *d = paramById(kv.first);
         if (!d)
            continue;
         mValues[kv.first].store(clampv(kv.second, d->min, d->max), std::memory_order_relaxed);
      }
      mParamsDirty.store(true, std::memory_order_release);
      notifyParamValuesChanged();
   }

   // Any path that moves parameters behind the host's back -- loading a preset,
   // loading saved state -- has to ask the host to re-read them, or it goes on
   // showing and automating the values it last knew about. [main-thread]
   void notifyParamValuesChanged() {
      if (!mHost)
         return;
      auto *hostParams =
         static_cast<const clap_host_params_t *>(mHost->get_extension(mHost, CLAP_EXT_PARAMS));
      if (hostParams && hostParams->rescan)
         hostParams->rescan(mHost, CLAP_PARAM_RESCAN_VALUES | CLAP_PARAM_RESCAN_TEXT);
   }

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
      RainyDayPlugin *plug = self(p);
      PresetData preset;
      std::string error;

      if (locationKind == CLAP_PRESET_DISCOVERY_LOCATION_FILE) {
         if (!location || !location[0]) {
            plug->reportPresetError(locationKind, location, loadKey, "missing preset path");
            return false;
         }
         if (!parsePresetFile(location, preset, error)) {
            plug->reportPresetError(locationKind, location, loadKey, error);
            return false;
         }
      } else if (locationKind == CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN) {
         if (!loadKey || !loadKey[0]) {
            plug->reportPresetError(locationKind, location, loadKey, "missing preset load key");
            return false;
         }
         const BuiltinPreset *found = nullptr;
         for (unsigned i = 0; i < kNumBuiltinPresets; ++i) {
            if (std::strcmp(kBuiltinPresets[i].loadKey, loadKey) == 0) {
               found = &kBuiltinPresets[i];
               break;
            }
         }
         if (!found) {
            plug->reportPresetError(locationKind, location, loadKey,
                                    std::string("unknown built-in preset '") + loadKey + "'");
            return false;
         }
         if (!parsePreset(found->text, std::strlen(found->text), preset, error)) {
            plug->reportPresetError(locationKind, location, loadKey, error);
            return false;
         }
      } else {
         plug->reportPresetError(locationKind, location, loadKey, "unsupported location kind");
         return false;
      }

      plug->applyPreset(preset);
      plug->notePresetLoaded(locationKind, loadKey, location);

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
      std::snprintf(info->name, sizeof(info->name), "Rain Out");
      info->flags = CLAP_AUDIO_PORT_IS_MAIN;
      info->channel_count = 2;
      info->port_type = CLAP_PORT_STEREO;
      info->in_place_pair = CLAP_INVALID_ID;
      return true;
   }

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
      std::snprintf(info->name, sizeof(info->name), "Note In");
      return true;
   }

   static uint32_t tailGet(const clap_plugin_t *p) {
      RainyDayPlugin *plug = self(p);
      const double tail = plug->mEngine.tailSeconds() * plug->mSampleRate;
      return static_cast<uint32_t>(clampv(tail, 0.0, 2.0e9));
   }

   static bool voiceInfoGet(const clap_plugin_t *, clap_voice_info_t *info) {
      info->voice_count = RainEngine::kMaxVoices;
      info->voice_capacity = RainEngine::kMaxVoices;
      info->flags = CLAP_VOICE_INFO_SUPPORTS_OVERLAPPING_NOTES;
      return true;
   }

   // ------------------------------------------------------------------ process

   void handleEvent(const clap_event_header_t *hdr) {
      if (!hdr || hdr->space_id != CLAP_CORE_EVENT_SPACE_ID)
         return;

      switch (hdr->type) {
      case CLAP_EVENT_NOTE_ON: {
         const auto *ev = reinterpret_cast<const clap_event_note_t *>(hdr);
         mEngine.noteOn(ev->port_index, ev->channel, ev->key, ev->note_id, ev->velocity);
         break;
      }
      case CLAP_EVENT_NOTE_OFF: {
         const auto *ev = reinterpret_cast<const clap_event_note_t *>(hdr);
         mEngine.noteOff(ev->port_index, ev->channel, ev->key, ev->note_id);
         break;
      }
      case CLAP_EVENT_NOTE_CHOKE: {
         const auto *ev = reinterpret_cast<const clap_event_note_t *>(hdr);
         mEngine.choke(ev->port_index, ev->channel, ev->key, ev->note_id);
         break;
      }
      case CLAP_EVENT_PARAM_VALUE: {
         const auto *ev = reinterpret_cast<const clap_event_param_value_t *>(hdr);
         const ParamDesc *d = paramById(ev->param_id);
         if (!d)
            break;
         mValues[ev->param_id].store(clampv(ev->value, d->min, d->max),
                                     std::memory_order_relaxed);
         mParamsDirty.store(true, std::memory_order_relaxed);
         break;
      }
      case CLAP_EVENT_PARAM_MOD: {
         const auto *ev = reinterpret_cast<const clap_event_param_mod_t *>(hdr);
         if (ev->note_id >= 0)
            break; // per-note modulation is not supported
         if (!paramById(ev->param_id))
            break;
         mMods[ev->param_id].store(ev->amount, std::memory_order_relaxed);
         mParamsDirty.store(true, std::memory_order_relaxed);
         break;
      }
      case CLAP_EVENT_MIDI: {
         const auto *ev = reinterpret_cast<const clap_event_midi_t *>(hdr);
         const uint8_t status = ev->data[0] & 0xF0;
         const int16_t channel = static_cast<int16_t>(ev->data[0] & 0x0F);
         const int16_t key = static_cast<int16_t>(ev->data[1] & 0x7F);
         const uint8_t vel = ev->data[2] & 0x7F;
         if (status == 0x90 && vel > 0)
            mEngine.noteOn(ev->port_index, channel, key, -1, vel / 127.0);
         else if (status == 0x80 || (status == 0x90 && vel == 0))
            mEngine.noteOff(ev->port_index, channel, key, -1);
         else if (status == 0xB0 && (ev->data[1] == 120 || ev->data[1] == 123))
            mEngine.allSoundOff();
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

      drainGuiEdits(pr->out_events, 0);

      uint32_t eventIndex = 0;
      uint32_t frame = 0;

      while (frame < numFrames) {
         // Consume every event scheduled at or before the current frame, so
         // parameter changes and notes land sample-accurately.
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

         if (mParamsDirty.exchange(false, std::memory_order_acq_rel))
            syncEngineParams();

         const uint32_t n = next - frame;
         std::memset(outL + frame, 0, n * sizeof(float));
         std::memset(outR + frame, 0, n * sizeof(float));
         mEngine.process(outL + frame, outR + frame, n);
         frame = next;
      }

      // Published for the window's activity meter; the GUI never reads engine
      // state directly.
      mDropletMeter.store(mEngine.activeDropletCount(), std::memory_order_relaxed);
      publishOutputPeaks(outL, outR, numFrames);

      return mEngine.isSilent() ? CLAP_PROCESS_SLEEP : CLAP_PROCESS_CONTINUE;
   }


   // ----------------------------------------------------------- preset list
   //
   // What the window's browser shows: the factory library, which is embedded in
   // the binary, followed by whatever the user has put in their own preset
   // directory. Loading goes through the same preset-load path a host uses, so
   // there is exactly one code path for it.

   void ensurePresetList() {
      if (mPresetsScanned)
         return;
      mPresetsScanned = true;
#ifdef RAINYDAY_WITH_GUI
      for (unsigned i = 0; i < kNumBuiltinPresets; ++i) {
         PresetData data;
         std::string error;
         if (!parsePreset(kBuiltinPresets[i].text, std::strlen(kBuiltinPresets[i].text), data,
                          error))
            continue;
         GuiPreset entry;
         entry.name = data.name.empty() ? kBuiltinPresets[i].loadKey : data.name;
         entry.description = data.description;
         entry.loadKey = kBuiltinPresets[i].loadKey;
         mPresets.push_back(entry);
      }

      const std::string dir = userPresetDir();
      std::error_code ec;
      if (dir.empty() || !std::filesystem::is_directory(dir, ec))
         return;
      std::vector<GuiPreset> user;
      const std::string suffix = std::string(".") + kPresetExtension;
      for (const auto &entry : std::filesystem::directory_iterator(dir, ec)) {
         const std::string name = entry.path().filename().string();
         if (name.size() <= suffix.size() ||
             name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0)
            continue;
         const std::string path = entry.path().string();
         PresetData data;
         std::string error;
         if (!parsePresetFile(path, data, error))
            continue;
         GuiPreset item;
         item.name = data.name.empty() ? name.substr(0, name.size() - suffix.size()) : data.name;
         item.description = data.description;
         item.path = path;
         item.userContent = true;
         user.push_back(item);
      }
      std::sort(user.begin(), user.end(),
                [](const GuiPreset &a, const GuiPreset &b) { return a.name < b.name; });
      mPresets.insert(mPresets.end(), user.begin(), user.end());
#endif
   }

   void notePresetLoaded(uint32_t locationKind, const char *loadKey, const char *location) {
#ifdef RAINYDAY_WITH_GUI
      ensurePresetList();
      mCurrentPreset = -1;
      for (size_t i = 0; i < mPresets.size(); ++i) {
         const bool match =
            locationKind == CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN
               ? (loadKey && mPresets[i].loadKey == loadKey)
               : (location && !mPresets[i].path.empty() && mPresets[i].path == location);
         if (match) {
            mCurrentPreset = static_cast<int>(i);
            break;
         }
      }
      mPresetEdited = false;
#else
      (void)locationKind;
      (void)loadKey;
      (void)location;
#endif
   }

   // A peak meter that fell as fast as the signal would be unreadable, so the
   // published value decays towards the true peak over about 350 ms instead.
   void publishOutputPeaks(const float *l, const float *r, uint32_t frames) {
      if (frames == 0)
         return;
      float peakL = 0.0f;
      float peakR = 0.0f;
      for (uint32_t i = 0; i < frames; ++i) {
         peakL = std::max(peakL, std::fabs(l[i]));
         peakR = std::max(peakR, std::fabs(r[i]));
      }
      const double seconds = static_cast<double>(frames) / (mSampleRate > 0 ? mSampleRate : 48000.0);
      const float fall = static_cast<float>(std::exp(-seconds / 0.35));
      mFallingPeakL = std::max(peakL, mFallingPeakL * fall);
      mFallingPeakR = std::max(peakR, mFallingPeakR * fall);
      mPeakL.store(mFallingPeakL, std::memory_order_relaxed);
      mPeakR.store(mFallingPeakR, std::memory_order_relaxed);
   }

#ifdef RAINYDAY_WITH_GUI
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

   // -------------------------------------------------------- GuiDelegate

   double guiParamValue(uint32_t id) const override {
      return id < kNumParams ? mValues[id].load(std::memory_order_relaxed) : 0.0;
   }

   void guiBeginEdit(uint32_t id) override { pushGuiEdit(id, 0.0, EditKind::GestureBegin); }

   void guiSetParam(uint32_t id, double value) override {
      const ParamDesc *d = paramById(id);
      if (!d)
         return;
      const double clamped = clampv(value, d->min, d->max);
      mValues[id].store(clamped, std::memory_order_relaxed);
      mParamsDirty.store(true, std::memory_order_release);
      mPresetEdited = true;
      pushGuiEdit(id, clamped, EditKind::Value);
   }

   void guiEndEdit(uint32_t id) override { pushGuiEdit(id, 0.0, EditKind::GestureEnd); }

   void guiOutputPeaks(float &left, float &right) const override {
      left = mPeakL.load(std::memory_order_relaxed);
      right = mPeakR.load(std::memory_order_relaxed);
   }

   uint32_t guiVoiceCount() const override {
      return mDropletMeter.load(std::memory_order_relaxed);
   }

   uint32_t guiVoiceLimit() const override {
      return static_cast<uint32_t>(realValue(kParamMaxDroplets));
   }

   std::string guiSuggestedPresetName() const override {
      if (mCurrentPreset >= 0 && mCurrentPreset < static_cast<int>(mPresets.size()))
         return mPresets[static_cast<size_t>(mCurrentPreset)].name;
      return "My Rain";
   }

   bool guiSavePreset(const std::string &name, std::string &error) override {
      const std::string path = userPresetPath(name);
      if (path.empty()) {
         error = "No user preset directory: neither XDG_CONFIG_HOME nor HOME is set.";
         return false;
      }

      // Everything the engine is currently using, written as the user's own
      // preset. Author and description are left out: they belong to whoever
      // wrote the preset this was derived from, not to this copy.
      PresetData data;
      data.name = name;
      for (uint32_t i = 0; i < kNumParams; ++i) {
         const uint32_t id = paramTable()[i].id;
         data.values.emplace_back(id, mValues[id].load(std::memory_order_relaxed));
      }
      if (!writePresetFile(path, formatPreset(data), error))
         return false;

      // Rescan so the browser shows it at once, and select what was just saved.
      mPresets.clear();
      mPresetsScanned = false;
      ensurePresetList();
      mCurrentPreset = -1;
      for (size_t i = 0; i < mPresets.size(); ++i) {
         if (mPresets[i].path == path) {
            mCurrentPreset = static_cast<int>(i);
            break;
         }
      }
      mPresetEdited = false;
      return true;
   }

   const std::vector<GuiPreset> &guiPresets() const override { return mPresets; }
   int guiCurrentPreset() const override { return mCurrentPreset; }
   bool guiPresetEdited() const override { return mPresetEdited; }

   void guiLoadPreset(int index) override {
      if (index < 0 || index >= static_cast<int>(mPresets.size()))
         return;
      const GuiPreset entry = mPresets[static_cast<size_t>(index)];
      if (!entry.loadKey.empty())
         presetLoadFromLocation(&mPlugin, CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN, nullptr,
                                entry.loadKey.c_str());
      else
         presetLoadFromLocation(&mPlugin, CLAP_PRESET_DISCOVERY_LOCATION_FILE, entry.path.c_str(),
                                nullptr);
   }

   // ----------------------------------------------------------- gui extension

   static bool guiIsApiSupported(const clap_plugin_t *, const char *api, bool isFloating) {
      // Embedded X11 only. A floating window would mean owning a top-level
      // window and its focus behaviour, which is the host's job here.
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
      RainyDayPlugin *plug = self(p);
      if (plug->mGui)
         return true;
      plug->ensurePresetList();
      plug->mGui = rainyday::createGui(*plug);
      if (!plug->mGui)
         return false;
      plug->startGuiClock();
      return true;
   }

   static void guiDestroy(const clap_plugin_t *p) {
      RainyDayPlugin *plug = self(p);
      plug->stopGuiClock();
      delete plug->mGui;
      plug->mGui = nullptr;
   }

   static bool guiSetScale(const clap_plugin_t *p, double scale) {
      RainyDayPlugin *plug = self(p);
      if (!plug->mGui)
         return false;
      plug->mGui->setScale(scale);
      return true;
   }

   static bool guiGetSize(const clap_plugin_t *p, uint32_t *width, uint32_t *height) {
      RainyDayPlugin *plug = self(p);
      if (!plug->mGui)
         return false;
      plug->mGui->size(width, height);
      return true;
   }

   // The window resizes by zooming: one layout, one cairo scale, so it keeps
   // its aspect ratio and the host is told so. adjust_size snaps whatever the
   // host proposes to the nearest size the window can take, and set_size then
   // takes it.
   static bool guiCanResize(const clap_plugin_t *) { return true; }

   static bool guiGetResizeHints(const clap_plugin_t *p, clap_gui_resize_hints_t *hints) {
      RainyDayPlugin *plug = self(p);
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
      RainyDayPlugin *plug = self(p);
      if (!plug->mGui || !width || !height)
         return false;
      plug->mGui->fitSize(width, height);
      return true;
   }

   static bool guiSetSize(const clap_plugin_t *p, uint32_t width, uint32_t height) {
      RainyDayPlugin *plug = self(p);
      if (!plug->mGui)
         return false;
      return plug->mGui->resize(width, height);
   }

   static bool guiSetParent(const clap_plugin_t *p, const clap_window_t *window) {
      RainyDayPlugin *plug = self(p);
      if (!plug->mGui || !window || std::strcmp(window->api, kWindowApi) != 0)
         return false;
      return plug->mGui->embed(nativeHandle(*window));
   }

   static bool guiSetTransient(const clap_plugin_t *p, const clap_window_t *window) {
      RainyDayPlugin *plug = self(p);
      if (!plug->mGui || !window || std::strcmp(window->api, kWindowApi) != 0)
         return false;
      return plug->mGui->setTransientFor(nativeHandle(*window));
   }

   static void guiSuggestTitle(const clap_plugin_t *p, const char *title) {
      RainyDayPlugin *plug = self(p);
      if (plug->mGui)
         plug->mGui->setTitle(title);
   }

   static bool guiShow(const clap_plugin_t *p) {
      RainyDayPlugin *plug = self(p);
      if (!plug->mGui)
         return false;
      plug->mGui->show();
      return true;
   }

   static bool guiHide(const clap_plugin_t *p) {
      RainyDayPlugin *plug = self(p);
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
      RainyDayPlugin *plug = self(p);
      if (plug->mGui && timerId == plug->mTimerId)
         plug->mGui->tick();
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
#endif // RAINYDAY_WITH_GUI

   // ---------------------------------------------------------------- extensions

   const void *getExtension(const char *id) const {
      static const clap_plugin_params_t kParamsExt = {
         paramsCount, paramsGetInfo, paramsGetValue, paramsValueToText, paramsTextToValue,
         paramsFlush};
      static const clap_plugin_audio_ports_t kAudioPortsExt = {audioPortsCount, audioPortsGet};
      static const clap_plugin_note_ports_t kNotePortsExt = {notePortsCount, notePortsGet};
      static const clap_plugin_state_t kStateExt = {stateSave, stateLoad};
      static const clap_plugin_tail_t kTailExt = {tailGet};
      static const clap_plugin_voice_info_t kVoiceInfoExt = {voiceInfoGet};
      static const clap_plugin_preset_load_t kPresetLoadExt = {presetLoadFromLocation};
#ifdef RAINYDAY_WITH_GUI
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
      if (std::strcmp(id, CLAP_EXT_VOICE_INFO) == 0)
         return &kVoiceInfoExt;
      if (std::strcmp(id, CLAP_EXT_PRESET_LOAD) == 0 ||
          std::strcmp(id, CLAP_EXT_PRESET_LOAD_COMPAT) == 0)
         return &kPresetLoadExt;
#ifdef RAINYDAY_WITH_GUI
      if (std::strcmp(id, CLAP_EXT_GUI) == 0)
         return &kGuiExt;
      if (std::strcmp(id, CLAP_EXT_TIMER_SUPPORT) == 0)
         return &kTimerExt;
#endif
      return nullptr;
   }

   clap_plugin_t mPlugin{};
   const clap_host_t *mHost = nullptr;

   std::atomic<double> mValues[kNumParams];
   std::atomic<double> mMods[kNumParams];
   std::atomic<bool> mParamsDirty{true};
   std::atomic<uint32_t> mDropletMeter{0};
   std::atomic<float> mPeakL{0.0f};
   std::atomic<float> mPeakR{0.0f};
   float mFallingPeakL = 0.0f; // audio thread only
   float mFallingPeakR = 0.0f;

   ParamEdit mEditQueue[kEditQueueSize];
   std::atomic<uint32_t> mEditWrite{0};
   std::atomic<uint32_t> mEditRead{0};

   bool mPresetsScanned = false;
   int mCurrentPreset = -1;
   bool mPresetEdited = false;
#ifdef RAINYDAY_WITH_GUI
   std::vector<GuiPreset> mPresets;
   Gui *mGui = nullptr;
   clap_id mTimerId = CLAP_INVALID_ID;
   std::thread mGuiThread;
   std::atomic<bool> mGuiThreadRun{false};
#endif

   RainEngine mEngine;
   double mSampleRate = 48000.0;
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
   auto *plug = new RainyDayPlugin(host);
   return plug->clapPlugin();
}

} // namespace

const clap_plugin_factory_t gPluginFactory = {factoryCount, factoryGetDescriptor, factoryCreate};

} // namespace rainyday

// --------------------------------------------------------------------- entry

extern "C" {

static bool entryInit(const char *) { return true; }
static void entryDeinit() {}

static const void *entryGetFactory(const char *factoryId) {
   if (!factoryId)
      return nullptr;
   if (std::strcmp(factoryId, CLAP_PLUGIN_FACTORY_ID) == 0)
      return &rainyday::gPluginFactory;
   if (std::strcmp(factoryId, CLAP_PRESET_DISCOVERY_FACTORY_ID) == 0 ||
       std::strcmp(factoryId, CLAP_PRESET_DISCOVERY_FACTORY_ID_COMPAT) == 0)
      return rainyday::presetDiscoveryFactory();
   return nullptr;
}

// clap/entry.h already declares this with CLAP_EXPORT, so the definition must
// not repeat the visibility attribute. The linker version script pins the
// export as well, making clap_entry the only symbol this DSO exposes.
const clap_plugin_entry_t clap_entry = {CLAP_VERSION_INIT, entryInit, entryDeinit,
                                        entryGetFactory};
}
