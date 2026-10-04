// Minimal CLAP host used to verify VerdaliScene outside a DAW: it walks the
// preset discovery factory exactly as a host would, loads scenes through the
// preset-load extension, and renders the result to a WAV file. Unlike the
// other plugins' hosts it also answers request_callback, because a scene
// builds its layers' engines on the main thread and asks for it to do so.
//
//   verdaliscene-render --list
//   verdaliscene-render --preset forest_river --out scene.wav --seconds 20
//   verdaliscene-render --all --outdir /tmp/scenes
//   verdaliscene-render --selftest
//   verdaliscene-render --preset forest_river --layers
//
// --layers renders the scene once whole and then each layer alone, the others
// held at -60 dB, and prints every one's level: what balancing a scene needs.
//
// --param matches a parameter's name; a name that only ends one -- "random
// seed" against "Rain 1 Random Seed" -- sets every layer's, each one a step
// further on, so a fixed seed gives a repeatable scene without two layers of
// one kind falling into lockstep.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#if defined(_WIN32)
#   include <windows.h>
#else
#   include <dlfcn.h>
#endif
#include <sys/stat.h>
#include <unistd.h>

#include <clap/clap.h>

// The host loads the plugin the way a DAW does. That is the one thing in this
// file that differs between platforms.
#if defined(_WIN32)
namespace {
void *dlopenCompat(const char *path) {
   return reinterpret_cast<void *>(LoadLibraryA(path));
}
void *dlsymCompat(void *h, const char *name) {
   return reinterpret_cast<void *>(GetProcAddress(reinterpret_cast<HMODULE>(h), name));
}
void dlcloseCompat(void *h) { FreeLibrary(reinterpret_cast<HMODULE>(h)); }
const char *dlerrorCompat() { return "see GetLastError()"; }
} // namespace
#   define RD_DLOPEN(p) dlopenCompat(p)
#   define RD_DLSYM(h, n) dlsymCompat(h, n)
#   define RD_DLCLOSE(h) dlcloseCompat(h)
#   define RD_DLERROR() dlerrorCompat()
#else
#   define RD_DLOPEN(p) dlopen(p, RTLD_NOW | RTLD_LOCAL)
#   define RD_DLSYM(h, n) dlsym(h, n)
#   define RD_DLCLOSE(h) dlclose(h)
#   define RD_DLERROR() dlerror()
#endif

#include "params.h"
#include "presets_generated.h"
#include "scene_preset.h"
#include "verdaliscene.h"
#include "verdalis/preset_library.h"

#include <filesystem>

#include "selftest_scene.h"

#include "verdalis/testing/preset_library_check.h"
#include <fstream>

// Setting an environment variable is spelled differently on each platform, and
// the self-test needs it to point the preset directory somewhere disposable.
namespace {
void setEnvVar(const char *name, const char *value) {
#if defined(_WIN32)
   _putenv_s(name, value ? value : "");
#else
   if (value)
      setenv(name, value, 1);
   else
      unsetenv(name);
#endif
}
} // namespace

namespace {

// ------------------------------------------------------------------- WAV out

bool writeWav(const std::string &path, const std::vector<float> &interleaved, uint32_t channels,
              uint32_t sampleRate) {
   FILE *f = std::fopen(path.c_str(), "wb");
   if (!f) {
      std::fprintf(stderr, "cannot write %s\n", path.c_str());
      return false;
   }
   const uint32_t frames = static_cast<uint32_t>(interleaved.size() / channels);
   const uint32_t dataBytes = frames * channels * 2;
   const uint32_t fmtChunk = 16;
   const uint32_t riffSize = 4 + (8 + fmtChunk) + (8 + dataBytes);

   auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
   auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };

   std::fwrite("RIFF", 1, 4, f);
   u32(riffSize);
   std::fwrite("WAVE", 1, 4, f);
   std::fwrite("fmt ", 1, 4, f);
   u32(fmtChunk);
   u16(1); // PCM
   u16(static_cast<uint16_t>(channels));
   u32(sampleRate);
   u32(sampleRate * channels * 2);
   u16(static_cast<uint16_t>(channels * 2));
   u16(16);
   std::fwrite("data", 1, 4, f);
   u32(dataBytes);

   for (float s : interleaved) {
      if (s > 1.0f)
         s = 1.0f;
      if (s < -1.0f)
         s = -1.0f;
      const int16_t v = static_cast<int16_t>(std::lrintf(s * 32767.0f));
      u16(static_cast<uint16_t>(v));
   }
   std::fclose(f);
   return true;
}

// ---------------------------------------------------------------- host object

bool gCallbackRequested = false;
uint32_t gLogWarnings = 0;

const clap_host_log_t kHostLog = {
   [](const clap_host_t *, clap_log_severity sev, const char *msg) {
      if (sev >= CLAP_LOG_WARNING)
         ++gLogWarnings;
      std::fprintf(stderr, "[plugin log %d] %s\n", sev, msg);
   }};

uint32_t gRescanCount = 0;

const clap_host_params_t kHostParams = {
   [](const clap_host_t *, clap_param_rescan_flags) { ++gRescanCount; },
   [](const clap_host_t *, clap_id, clap_param_clear_flags) {},
   [](const clap_host_t *) {},
};

uint32_t gPresetLoadedCount = 0;
uint32_t gPresetErrorCount = 0;

const clap_host_preset_load_t kHostPresetLoad = {
   [](const clap_host_t *, uint32_t, const char *loc, const char *key, int32_t,
      const char *msg) {
      ++gPresetErrorCount;
      std::fprintf(stderr, "[preset error] %s (%s / %s)\n", msg ? msg : "?", loc ? loc : "-",
                   key ? key : "-");
   },
   [](const clap_host_t *, uint32_t, const char *, const char *) { ++gPresetLoadedCount; },
};

const clap_host_thread_check_t kHostThreadCheck = {
   [](const clap_host_t *) { return true; },
   [](const clap_host_t *) { return true; },
};

clap_host_t gHost = {
   CLAP_VERSION_INIT,
   nullptr,
   "verdaliscene-render",
   "VerdaliScene",
   "https://example.invalid",
   "1.0.0",
   [](const clap_host_t *, const char *id) -> const void * {
      if (std::strcmp(id, CLAP_EXT_LOG) == 0)
         return &kHostLog;
      if (std::strcmp(id, CLAP_EXT_PARAMS) == 0)
         return &kHostParams;
      if (std::strcmp(id, CLAP_EXT_PRESET_LOAD) == 0)
         return &kHostPresetLoad;
      if (std::strcmp(id, CLAP_EXT_THREAD_CHECK) == 0)
         return &kHostThreadCheck;
      return nullptr;
   },
   [](const clap_host_t *) {},
   [](const clap_host_t *) {},
   // request_callback: answered between blocks, on this thread, which is the
   // main thread as far as the plugin can tell.
   [](const clap_host_t *) { gCallbackRequested = true; },
};

// ------------------------------------------------------- preset discovery side

struct PresetEntry {
   std::string name;
   std::string loadKey;
   std::string location;
   uint32_t locationKind = CLAP_PRESET_DISCOVERY_LOCATION_FILE;
   std::string description;
   std::string creator;
   std::vector<std::string> features;
};

struct Indexer {
   clap_preset_discovery_indexer_t iface{};
   std::vector<std::string> extensions;
   std::vector<std::pair<uint32_t, std::string>> locations;
};

struct Receiver {
   clap_preset_discovery_metadata_receiver_t iface{};
   std::vector<PresetEntry> *out = nullptr;
   uint32_t locationKind = 0;
   std::string location;
   bool failed = false;
};

Indexer gIndexer;

void setupIndexer() {
   gIndexer.iface.clap_version = CLAP_VERSION_INIT;
   gIndexer.iface.name = "verdaliscene-render";
   gIndexer.iface.vendor = "Verdalis";
   gIndexer.iface.url = "https://example.invalid";
   gIndexer.iface.version = "1.0.0";
   gIndexer.iface.indexer_data = &gIndexer;
   gIndexer.iface.declare_filetype = [](const clap_preset_discovery_indexer_t *ix,
                                        const clap_preset_discovery_filetype_t *ft) {
      auto *self = static_cast<Indexer *>(ix->indexer_data);
      self->extensions.push_back(ft->file_extension ? ft->file_extension : "");
      std::printf("  filetype: %s (.%s)\n", ft->name ? ft->name : "?",
                  ft->file_extension ? ft->file_extension : "");
      return true;
   };
   gIndexer.iface.declare_location = [](const clap_preset_discovery_indexer_t *ix,
                                        const clap_preset_discovery_location_t *loc) {
      auto *self = static_cast<Indexer *>(ix->indexer_data);
      self->locations.emplace_back(loc->kind, loc->location ? loc->location : "");
      std::printf("  location: %s kind=%u path=%s\n", loc->name ? loc->name : "?", loc->kind,
                  loc->location ? loc->location : "(plugin container)");
      return true;
   };
   gIndexer.iface.declare_soundpack = [](const clap_preset_discovery_indexer_t *,
                                         const clap_preset_discovery_soundpack_t *) {
      return true;
   };
   gIndexer.iface.get_extension = [](const clap_preset_discovery_indexer_t *,
                                     const char *) -> const void * { return nullptr; };
}

void setupReceiver(Receiver &rx) {
   rx.iface.receiver_data = &rx;
   rx.iface.on_error = [](const clap_preset_discovery_metadata_receiver_t *r, int32_t,
                          const char *msg) {
      auto *self = static_cast<Receiver *>(r->receiver_data);
      self->failed = true;
      std::fprintf(stderr, "  metadata error: %s\n", msg ? msg : "?");
   };
   rx.iface.begin_preset = [](const clap_preset_discovery_metadata_receiver_t *r,
                              const char *name, const char *loadKey) {
      auto *self = static_cast<Receiver *>(r->receiver_data);
      PresetEntry e;
      e.name = name ? name : "";
      e.loadKey = loadKey ? loadKey : "";
      e.location = self->location;
      e.locationKind = self->locationKind;
      self->out->push_back(e);
      return true;
   };
   rx.iface.add_plugin_id = [](const clap_preset_discovery_metadata_receiver_t *,
                               const clap_universal_plugin_id_t *) {};
   rx.iface.set_soundpack_id = [](const clap_preset_discovery_metadata_receiver_t *,
                                  const char *) {};
   rx.iface.set_flags = [](const clap_preset_discovery_metadata_receiver_t *, uint32_t) {};
   rx.iface.add_creator = [](const clap_preset_discovery_metadata_receiver_t *r,
                             const char *c) {
      auto *self = static_cast<Receiver *>(r->receiver_data);
      if (!self->out->empty() && c)
         self->out->back().creator = c;
   };
   rx.iface.set_description = [](const clap_preset_discovery_metadata_receiver_t *r,
                                 const char *d) {
      auto *self = static_cast<Receiver *>(r->receiver_data);
      if (!self->out->empty() && d)
         self->out->back().description = d;
   };
   rx.iface.set_timestamps = [](const clap_preset_discovery_metadata_receiver_t *, clap_timestamp,
                                clap_timestamp) {};
   rx.iface.add_feature = [](const clap_preset_discovery_metadata_receiver_t *r,
                             const char *f) {
      auto *self = static_cast<Receiver *>(r->receiver_data);
      if (!self->out->empty() && f)
         self->out->back().features.push_back(f);
   };
   rx.iface.add_extra_info = [](const clap_preset_discovery_metadata_receiver_t *, const char *,
                                const char *) {};
}

std::vector<PresetEntry> discoverPresets(const clap_plugin_entry_t *entry) {
   std::vector<PresetEntry> presets;
   const auto *factory = static_cast<const clap_preset_discovery_factory_t *>(
      entry->get_factory(CLAP_PRESET_DISCOVERY_FACTORY_ID));
   if (!factory) {
      std::fprintf(stderr, "plugin exposes no preset discovery factory\n");
      return presets;
   }

   const uint32_t providerCount = factory->count(factory);
   std::printf("preset providers: %u\n", providerCount);
   for (uint32_t i = 0; i < providerCount; ++i) {
      const auto *desc = factory->get_descriptor(factory, i);
      if (!desc)
         continue;
      std::printf("provider '%s' (%s)\n", desc->name, desc->id);
      const auto *provider = factory->create(factory, &gIndexer.iface, desc->id);
      if (!provider) {
         std::fprintf(stderr, "  create failed\n");
         continue;
      }
      gIndexer.locations.clear();
      gIndexer.extensions.clear();
      if (!provider->init(provider)) {
         std::fprintf(stderr, "  init failed\n");
         provider->destroy(provider);
         continue;
      }

      for (const auto &loc : gIndexer.locations) {
         Receiver rx;
         rx.out = &presets;
         setupReceiver(rx);

         if (loc.first == CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN) {
            rx.locationKind = CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN;
            rx.location.clear();
            provider->get_metadata(provider, CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN, nullptr,
                                   &rx.iface);
            continue;
         }

         // Crawl the directory the way a host indexer would: a declared location
         // is a directory to walk, not a single flat listing, and the user
         // library has one level of folders in it. A flat walk here would list
         // fewer presets than the plugin's own browser shows.
         std::error_code walkEc;
         std::filesystem::recursive_directory_iterator walk(
            loc.second, std::filesystem::directory_options::skip_permission_denied, walkEc);
         if (walkEc) {
            std::fprintf(stderr, "  cannot open location %s\n", loc.second.c_str());
            continue;
         }
         std::vector<std::string> files;
         for (const auto &entry : walk) {
            if (!entry.is_regular_file())
               continue;
            const std::string name = entry.path().filename().string();
            if (name.size() < 2 || name[0] == '.')
               continue;
            bool match = gIndexer.extensions.empty();
            for (const auto &ext : gIndexer.extensions) {
               if (ext.empty())
                  continue;
               if (name.size() > ext.size() + 1 &&
                   name.compare(name.size() - ext.size(), ext.size(), ext) == 0 &&
                   name[name.size() - ext.size() - 1] == '.')
                  match = true;
            }
            if (match)
               files.push_back(entry.path().string());
         }
         std::sort(files.begin(), files.end());
         for (const auto &f : files) {
            rx.locationKind = CLAP_PRESET_DISCOVERY_LOCATION_FILE;
            rx.location = f;
            provider->get_metadata(provider, CLAP_PRESET_DISCOVERY_LOCATION_FILE, f.c_str(),
                                   &rx.iface);
         }
      }
      provider->destroy(provider);
   }
   return presets;
}

// ------------------------------------------------------------------ rendering

// Holds the events for one process() call. Parameter events come first so a
// host-style "set the parameter, then play the note" ordering is preserved.
struct EventList {
   std::vector<clap_event_param_value_t> params;
   std::vector<clap_event_note_t> notes;
   clap_input_events_t in{};

   void build() {
      in.ctx = this;
      in.size = [](const clap_input_events_t *l) {
         auto *self = static_cast<EventList *>(l->ctx);
         return static_cast<uint32_t>(self->params.size() + self->notes.size());
      };
      in.get = [](const clap_input_events_t *l, uint32_t index) -> const clap_event_header_t * {
         auto *self = static_cast<EventList *>(l->ctx);
         if (index < self->params.size())
            return &self->params[index].header;
         index -= static_cast<uint32_t>(self->params.size());
         if (index >= self->notes.size())
            return nullptr;
         return &self->notes[index].header;
      };
   }
};

clap_event_param_value_t makeParamValue(clap_id id, double value) {
   clap_event_param_value_t ev{};
   ev.header.size = sizeof(ev);
   ev.header.time = 0;
   ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
   ev.header.type = CLAP_EVENT_PARAM_VALUE;
   ev.header.flags = 0;
   ev.param_id = id;
   ev.cookie = nullptr;
   ev.note_id = -1;
   ev.port_index = -1;
   ev.channel = -1;
   ev.key = -1;
   ev.value = value;
   return ev;
}

std::vector<std::pair<clap_id, double>> gParamOverrides;

clap_event_note_t makeNote(uint16_t type, uint32_t time, int16_t key, double velocity) {
   clap_event_note_t ev{};
   ev.header.size = sizeof(ev);
   ev.header.time = time;
   ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
   ev.header.type = type;
   ev.header.flags = 0;
   ev.note_id = 1;
   ev.port_index = 0;
   ev.channel = 0;
   ev.key = key;
   ev.velocity = velocity;
   return ev;
}

struct RenderResult {
   std::vector<float> interleaved;
   float peak = 0.0f;
   double rms = 0.0;
   bool sawNonFinite = false;
   uint32_t sleepAt = 0;
};

RenderResult renderPlugin(const clap_plugin_t *plugin, double sampleRate, uint32_t blockSize,
                          double holdSeconds, double tailSeconds, int16_t key, double velocity) {
   RenderResult res;
   // A negative hold time means: never send a note at all.
   const bool silentRun = holdSeconds < 0.0;
   const uint32_t holdFrames =
      silentRun ? 0 : static_cast<uint32_t>(holdSeconds * sampleRate);
   const uint32_t totalFrames = holdFrames + static_cast<uint32_t>(tailSeconds * sampleRate);

   std::vector<float> left(blockSize), right(blockSize);
   float *channels[2] = {left.data(), right.data()};
   clap_audio_buffer_t outBuf{};
   outBuf.data32 = channels;
   outBuf.data64 = nullptr;
   outBuf.channel_count = 2;
   outBuf.latency = 0;
   outBuf.constant_mask = 0;

   clap_output_events_t outEvents{};
   outEvents.ctx = nullptr;
   outEvents.try_push = [](const clap_output_events_t *, const clap_event_header_t *) {
      return true;
   };

   plugin->start_processing(plugin);

   res.interleaved.reserve(totalFrames * 2);
   uint32_t frame = 0;
   bool noteOnSent = silentRun, noteOffSent = silentRun;

   while (frame < totalFrames) {
      const uint32_t n = std::min<uint32_t>(blockSize, totalFrames - frame);

      EventList events;
      if (!noteOnSent) {
         for (const auto &ov : gParamOverrides)
            events.params.push_back(makeParamValue(ov.first, ov.second));
         events.notes.push_back(makeNote(CLAP_EVENT_NOTE_ON, 0, key, velocity));
         noteOnSent = true;
      }
      if (!noteOffSent && frame + n > holdFrames && holdFrames >= frame) {
         events.notes.push_back(
            makeNote(CLAP_EVENT_NOTE_OFF, holdFrames - frame, key, velocity));
         noteOffSent = true;
      }
      events.build();

      clap_process_t pr{};
      pr.steady_time = frame;
      pr.frames_count = n;
      pr.transport = nullptr;
      pr.audio_inputs = nullptr;
      pr.audio_inputs_count = 0;
      pr.audio_outputs = &outBuf;
      pr.audio_outputs_count = 1;
      pr.in_events = &events.in;
      pr.out_events = &outEvents;

      const clap_process_status st = plugin->process(plugin, &pr);
      if (gCallbackRequested) {
         gCallbackRequested = false;
         plugin->on_main_thread(plugin);
      }
      if (st == CLAP_PROCESS_ERROR) {
         std::fprintf(stderr, "process() returned ERROR\n");
         break;
      }
      if (st == CLAP_PROCESS_SLEEP && res.sleepAt == 0 && noteOffSent)
         res.sleepAt = frame;

      for (uint32_t i = 0; i < n; ++i) {
         const float l = left[i], r = right[i];
         if (!std::isfinite(l) || !std::isfinite(r))
            res.sawNonFinite = true;
         res.peak = std::max(res.peak, std::max(std::fabs(l), std::fabs(r)));
         res.rms += static_cast<double>(l) * l + static_cast<double>(r) * r;
         res.interleaved.push_back(l);
         res.interleaved.push_back(r);
      }
      frame += n;
   }

   plugin->stop_processing(plugin);
   if (!res.interleaved.empty())
      res.rms = std::sqrt(res.rms / res.interleaved.size());
   return res;
}

// --------------------------------------------------------------------- driver

const clap_plugin_t *createPlugin(const clap_plugin_entry_t *entry) {
   const auto *factory =
      static_cast<const clap_plugin_factory_t *>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
   if (!factory || factory->get_plugin_count(factory) == 0) {
      std::fprintf(stderr, "no plugin factory\n");
      return nullptr;
   }
   const clap_plugin_descriptor_t *desc = factory->get_plugin_descriptor(factory, 0);
   const clap_plugin_t *plugin = factory->create_plugin(factory, &gHost, desc->id);
   if (!plugin || !plugin->init(plugin)) {
      std::fprintf(stderr, "plugin creation failed\n");
      return nullptr;
   }
   return plugin;
}

// Resolves "<name or id>=<value>" against the plugin's own parameter list,
// using text_to_value so the value can be given in real units ("2200 Hz").
bool resolveParamOverrides(const clap_plugin_t *plugin,
                           const std::vector<std::string> &specs) {
   gParamOverrides.clear();
   if (specs.empty())
      return true;
   const auto *params = static_cast<const clap_plugin_params_t *>(
      plugin->get_extension(plugin, CLAP_EXT_PARAMS));
   if (!params)
      return false;

   auto squash = [](const std::string &in) {
      std::string out;
      for (char c : in)
         if (!std::isspace(static_cast<unsigned char>(c)))
            out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      return out;
   };

   const uint32_t count = params->count(plugin);
   for (const auto &spec : specs) {
      const size_t eq = spec.find('=');
      if (eq == std::string::npos) {
         std::fprintf(stderr, "bad --param '%s', expected key=value\n", spec.c_str());
         return false;
      }
      const std::string key = squash(spec.substr(0, eq));
      const std::string valueText = spec.substr(eq + 1);

      bool found = false;
      for (uint32_t i = 0; i < count && !found; ++i) {
         clap_param_info_t info{};
         if (!params->get_info(plugin, i, &info))
            continue;
         if (squash(info.name) != key && std::to_string(info.id) != key)
            continue;
         double raw = 0.0;
         if (!params->text_to_value(plugin, info.id, valueText.c_str(), &raw)) {
            std::fprintf(stderr, "cannot parse value '%s' for '%s'\n", valueText.c_str(),
                         info.name);
            return false;
         }
         gParamOverrides.emplace_back(info.id, raw);
         found = true;
      }
      // Every layer's copy of a parameter: "random seed" sets them all, a step
      // apart so that two layers of one kind do not run in lockstep.
      int step = 0;
      for (uint32_t i = 0; i < count && !found; ++i) {
         clap_param_info_t info{};
         if (!params->get_info(plugin, i, &info))
            continue;
         const std::string name = squash(info.name);
         if (name.size() <= key.size() || name.compare(name.size() - key.size(), key.size(), key))
            continue;
         double raw = 0.0;
         if (!params->text_to_value(plugin, info.id, valueText.c_str(), &raw))
            continue;
         if (raw > 0.0 && info.flags & CLAP_PARAM_IS_STEPPED)
            raw = std::min(info.max_value, raw + step);
         ++step;
         gParamOverrides.emplace_back(info.id, raw);
      }
      found = found || step > 0;
      if (!found) {
         std::fprintf(stderr, "no such parameter: '%s'\n", spec.substr(0, eq).c_str());
         return false;
      }
   }
   return true;
}

bool loadPreset(const clap_plugin_t *plugin, const PresetEntry &preset) {
   const auto *ext = static_cast<const clap_plugin_preset_load_t *>(
      plugin->get_extension(plugin, CLAP_EXT_PRESET_LOAD));
   if (!ext) {
      std::fprintf(stderr, "plugin has no preset-load extension\n");
      return false;
   }
   const char *location =
      preset.locationKind == CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN ? nullptr
                                                                   : preset.location.c_str();
   return ext->from_location(plugin, preset.locationKind, location, preset.loadKey.c_str());
}

std::string sanitise(const std::string &s) {
   std::string out;
   for (char c : s)
      out += (std::isalnum(static_cast<unsigned char>(c)) ? c : '_');
   return out;
}

// Case-insensitive key used to match a preset by name, load key or file stem.
std::string matchKey(const std::string &s) {
   std::string out;
   for (char c : s) {
      const unsigned char u = static_cast<unsigned char>(c);
      if (std::isalnum(u))
         out += static_cast<char>(std::tolower(u));
   }
   return out;
}


// Reads every parameter's current value into a vector, in id order.
std::vector<double> snapshotParams(const clap_plugin_t *plugin) {
   std::vector<double> out;
   const auto *params = static_cast<const clap_plugin_params_t *>(
      plugin->get_extension(plugin, CLAP_EXT_PARAMS));
   if (!params)
      return out;
   const uint32_t count = params->count(plugin);
   for (uint32_t i = 0; i < count; ++i) {
      clap_param_info_t info{};
      if (!params->get_info(plugin, i, &info))
         continue;
      double v = 0.0;
      params->get_value(plugin, info.id, &v);
      out.push_back(v);
   }
   return out;
}

// Pushes one parameter value per parameter through a real process() call, the
// way a host would, and returns what the plugin reports back afterwards.
enum class Extreme { Min, Max, Mid };

std::vector<double> driveAllParams(const clap_plugin_t *plugin, double sampleRate,
                                   Extreme which, RenderResult *renderOut) {
   const auto *params = static_cast<const clap_plugin_params_t *>(
      plugin->get_extension(plugin, CLAP_EXT_PARAMS));
   if (!params)
      return {};

   gParamOverrides.clear();
   const uint32_t count = params->count(plugin);
   for (uint32_t i = 0; i < count; ++i) {
      clap_param_info_t info{};
      if (!params->get_info(plugin, i, &info))
         continue;
      double v = info.default_value;
      if (which == Extreme::Min)
         v = info.min_value;
      else if (which == Extreme::Max)
         v = info.max_value;
      else
         v = 0.5 * (info.min_value + info.max_value);
      gParamOverrides.emplace_back(info.id, v);
   }

   const RenderResult res = renderPlugin(plugin, sampleRate, 512, 0.6, 0.6, 60, 1.0);
   if (renderOut)
      *renderOut = res;
   gParamOverrides.clear();
   return snapshotParams(plugin);
}

int runSelfTest(const clap_plugin_entry_t *entry, double sampleRate);

double toDb(double v) { return 20.0 * std::log10(std::max(v, 1e-9)); }

// The scene, then each of its layers alone. A layer is found by its hidden
// Active switch, and silenced by its Level, so nothing else about the scene
// moves between takes.
void layerReport(const clap_plugin_t *plugin, double sampleRate, uint32_t block, double seconds) {
   const auto *params = static_cast<const clap_plugin_params_t *>(
      plugin->get_extension(plugin, CLAP_EXT_PARAMS));
   struct Layer {
      std::string name;
      clap_id level;
   };
   std::vector<Layer> layers;
   const uint32_t count = params->count(plugin);
   for (uint32_t i = 0; i < count; ++i) {
      clap_param_info_t info{};
      if (!params->get_info(plugin, i, &info))
         continue;
      const std::string name = info.name;
      if (name.size() < 7 || name.compare(name.size() - 7, 7, " Active") != 0)
         continue;
      double v = 0.0;
      params->get_value(plugin, info.id, &v);
      if (v < 0.5)
         continue;
      const std::string prefix = name.substr(0, name.size() - 7);
      for (uint32_t j = 0; j < count; ++j) {
         clap_param_info_t li{};
         if (params->get_info(plugin, j, &li) && prefix + " Level" == li.name)
            layers.push_back({prefix, li.id});
      }
   }
   std::vector<double> levels;
   for (const Layer &l : layers) {
      double v = 0.0;
      params->get_value(plugin, l.level, &v);
      levels.push_back(v);
   }
   auto take = [&](int solo) {
      gParamOverrides.clear();
      for (size_t k = 0; k < layers.size(); ++k)
         gParamOverrides.emplace_back(layers[k].level, solo < 0 || static_cast<int>(k) == solo
                                                          ? levels[k]
                                                          : -60.0);
      plugin->reset(plugin);
      // The overrides go in with the first block, which a negative hold skips.
      const RenderResult r = renderPlugin(plugin, sampleRate, block, seconds, 0.0, 60, 1.0);
      gParamOverrides.clear();
      return r;
   };
   const RenderResult all = take(-1);
   std::printf("  %-12s peak %+6.1f dBFS  rms %+6.1f dBFS\n", "(scene)", toDb(all.peak),
               toDb(all.rms));
   for (size_t k = 0; k < layers.size(); ++k) {
      const RenderResult r = take(static_cast<int>(k));
      std::printf("  %-12s peak %+6.1f dBFS  rms %+6.1f dBFS  (fader %+5.1f dB)\n",
                  layers[k].name.c_str(), toDb(r.peak), toDb(r.rms), levels[k]);
   }
}

} // namespace

int main(int argc, char **argv) {
   std::string pluginPath = "./VerdaliScene.clap";
   std::string presetSel, outPath = "scene.wav", outDir = ".";
   double seconds = 6.0, tail = 3.0;
   int16_t key = 60;
   double velocity = 0.9;
   double sampleRate = 48000.0;
   uint32_t blockSize = 512;
   bool doList = false, doAll = false, doSelfTest = false, doLayers = false;
   std::vector<std::string> paramSpecs;

   for (int i = 1; i < argc; ++i) {
      const std::string a = argv[i];
      auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
      if (a == "--plugin")
         pluginPath = next();
      else if (a == "--preset")
         presetSel = next();
      else if (a == "--out")
         outPath = next();
      else if (a == "--outdir")
         outDir = next();
      else if (a == "--seconds")
         seconds = std::atof(next().c_str());
      else if (a == "--tail")
         tail = std::atof(next().c_str());
      else if (a == "--key")
         key = static_cast<int16_t>(std::atoi(next().c_str()));
      else if (a == "--velocity")
         velocity = std::atof(next().c_str());
      else if (a == "--rate")
         sampleRate = std::atof(next().c_str());
      else if (a == "--block")
         blockSize = static_cast<uint32_t>(std::atoi(next().c_str()));
      else if (a == "--param")
         paramSpecs.push_back(next());
      else if (a == "--list")
         doList = true;
      else if (a == "--all")
         doAll = true;
      else if (a == "--selftest")
         doSelfTest = true;
      else if (a == "--layers")
         doLayers = true;
      else {
         std::fprintf(stderr, "unknown argument: %s\n", a.c_str());
         return 2;
      }
   }

   void *dso = RD_DLOPEN(pluginPath.c_str());
   if (!dso) {
      std::fprintf(stderr, "dlopen failed: %s\n", RD_DLERROR());
      return 1;
   }
   auto *entry = static_cast<const clap_plugin_entry_t *>(RD_DLSYM(dso, "clap_entry"));
   if (!entry) {
      std::fprintf(stderr, "no clap_entry symbol: %s\n", RD_DLERROR());
      return 1;
   }
   if (!clap_version_is_compatible(entry->clap_version)) {
      std::fprintf(stderr, "incompatible CLAP version\n");
      return 1;
   }
   if (!entry->init(pluginPath.c_str())) {
      std::fprintf(stderr, "entry init failed\n");
      return 1;
   }

   setupIndexer();

   int rc = 0;
   if (doSelfTest) {
      rc = runSelfTest(entry, sampleRate);
      entry->deinit();
      RD_DLCLOSE(dso);
      return rc;
   }

   std::vector<PresetEntry> presets = discoverPresets(entry);
   std::printf("discovered %zu presets\n", presets.size());

   if (doList) {
      for (const auto &p : presets) {
         std::printf("  %-22s kind=%u key=%-20s %s\n", p.name.c_str(), p.locationKind,
                     p.loadKey.empty() ? "-" : p.loadKey.c_str(), p.description.c_str());
      }
      entry->deinit();
      RD_DLCLOSE(dso);
      return 0;
   }

   std::vector<PresetEntry> todo;
   if (doAll) {
      // --outdir is written into once per preset; create it here so a missing
      // directory is one error before any work rather than seventeen after it.
      std::error_code ec;
      std::filesystem::create_directories(outDir, ec);
      if (!std::filesystem::is_directory(outDir, ec)) {
         std::fprintf(stderr, "could not create the output directory %s\n", outDir.c_str());
         return 1;
      }
      todo = presets;
   } else if (!presetSel.empty() && std::filesystem::is_regular_file(presetSel)) {
      // A scene file anywhere on disk, loaded the way a host loads a file it
      // found itself: a scene in the making, before it is a factory one.
      PresetEntry file;
      file.name = std::filesystem::path(presetSel).stem().string();
      file.location = presetSel;
      file.locationKind = CLAP_PRESET_DISCOVERY_LOCATION_FILE;
      todo.push_back(file);
   } else if (!presetSel.empty()) {
      const std::string want = matchKey(presetSel);
      for (const auto &p : presets) {
         std::string stem = p.location;
         const size_t slash = stem.rfind('/');
         if (slash != std::string::npos)
            stem = stem.substr(slash + 1);
         const size_t dot = stem.rfind('.');
         if (dot != std::string::npos)
            stem = stem.substr(0, dot);
         if (matchKey(p.name) == want || matchKey(p.loadKey) == want || matchKey(stem) == want)
            todo.push_back(p);
      }
      if (todo.empty()) {
         std::fprintf(stderr, "preset '%s' not found\n", presetSel.c_str());
         entry->deinit();
         RD_DLCLOSE(dso);
         return 1;
      }
      todo.resize(1);
   } else {
      todo.push_back(PresetEntry{}); // defaults, no preset load
   }

   for (const auto &preset : todo) {
      const clap_plugin_t *plugin = createPlugin(entry);
      if (!plugin) {
         rc = 1;
         break;
      }
      if (!preset.name.empty() && !loadPreset(plugin, preset))
         std::fprintf(stderr, "warning: failed to load preset '%s'\n", preset.name.c_str());

      if (!resolveParamOverrides(plugin, paramSpecs)) {
         plugin->destroy(plugin);
         rc = 1;
         break;
      }

      if (!plugin->activate(plugin, sampleRate, 1, blockSize)) {
         std::fprintf(stderr, "activate failed\n");
         plugin->destroy(plugin);
         rc = 1;
         break;
      }

      if (doLayers) {
         layerReport(plugin, sampleRate, blockSize, seconds);
         plugin->deactivate(plugin);
         plugin->destroy(plugin);
         continue;
      }

      const RenderResult res =
         renderPlugin(plugin, sampleRate, blockSize, seconds, tail, key, velocity);

      const std::string file =
         doAll ? outDir + "/" + sanitise(preset.name.empty() ? "default" : preset.name) + ".wav"
               : outPath;
      if (!writeWav(file, res.interleaved, 2, static_cast<uint32_t>(sampleRate))) {
         std::fprintf(stderr, "could not write %s\n", file.c_str());
         return 1;
      }

      std::printf("%-24s peak %6.3f (%+6.1f dBFS)  rms %7.5f (%+6.1f dBFS)%s -> %s\n",
                  preset.name.empty() ? "(defaults)" : preset.name.c_str(), res.peak,
                  20.0 * std::log10(std::max(res.peak, 1e-9f)), res.rms,
                  20.0 * std::log10(std::max(res.rms, 1e-9)),
                  res.sawNonFinite ? "  !! NON-FINITE OUTPUT" : "", file.c_str());
      if (res.sawNonFinite)
         rc = 1;

      plugin->deactivate(plugin);
      plugin->destroy(plugin);
   }

   entry->deinit();
   RD_DLCLOSE(dso);
   return rc;
}

namespace {

// Exercises the parts a DAW leans on but a plain render does not -- parameter
// metadata, text conversion, state, the sleep contract -- and then the parts
// that are VerdaliScene's own: that layers come and go safely, that every
// factory scene and every one of the nine plugins' presets loads, and that a
// layer saved as a preset of its plugin reads back as that plugin's preset.
int runSelfTest(const clap_plugin_entry_t *entry, double sampleRate) {
   using namespace verdaliscene;
   int failures = 0;
   auto check = [&](bool ok, const std::string &what) {
      std::printf("  [%s] %s\n", ok ? "ok" : "FAIL", what.c_str());
      if (!ok)
         ++failures;
   };

   const clap_plugin_t *plugin = createPlugin(entry);
   if (!plugin)
      return 1;

   // --- parameters
   const auto *params = static_cast<const clap_plugin_params_t *>(
      plugin->get_extension(plugin, CLAP_EXT_PARAMS));
   check(params != nullptr, "params extension present");
   uint32_t count = params ? params->count(plugin) : 0;
   check(count > 0, "parameter count > 0");
   std::printf("  %u parameters\n", count);

   bool infoOk = true, textOk = true, roundTripOk = true, idsUnique = true;
   std::vector<clap_id> seen;
   for (uint32_t i = 0; i < count; ++i) {
      clap_param_info_t info{};
      if (!params->get_info(plugin, i, &info)) {
         infoOk = false;
         continue;
      }
      seen.push_back(info.id);
      if (!(info.min_value <= info.default_value && info.default_value <= info.max_value))
         infoOk = false;
      if (info.name[0] == '\0')
         infoOk = false;
      char buf[CLAP_NAME_SIZE];
      if (!params->value_to_text(plugin, info.id, info.default_value, buf, sizeof(buf))) {
         textOk = false;
         continue;
      }
      double back = 0.0;
      if (!params->text_to_value(plugin, info.id, buf, &back)) {
         roundTripOk = false;
         continue;
      }
      const double span = info.max_value - info.min_value;
      if (std::fabs(back - info.default_value) > std::max(0.02 * span, 1e-6)) {
         std::fprintf(stderr, "    text round-trip drift on '%s': %f -> '%s' -> %f\n", info.name,
                      info.default_value, buf, back);
         roundTripOk = false;
      }
   }
   std::sort(seen.begin(), seen.end());
   idsUnique = std::adjacent_find(seen.begin(), seen.end()) == seen.end();
   check(infoOk, "every get_info() is well formed");
   check(idsUnique, "parameter ids are unique");
   check(textOk, "value_to_text() works for all defaults");
   check(roundTripOk, "value_to_text -> text_to_value round-trips");

   // Every layer's copy of every one of its plugin's parameters is there, at
   // the id the layout promises, except what a layer pins.
   {
      bool layout = true;
      for (int slot = 0; slot < kNumSlots && layout; ++slot) {
         const LayerType &t = layerType(slotType(slot));
         for (uint32_t i = 0; i < t.paramCount; ++i) {
            const uint32_t id = slotParamId(slot, i);
            const bool listed = std::binary_search(seen.begin(), seen.end(), id);
            if (listed == (pinnedParam(t, i) != nullptr)) {
               std::fprintf(stderr, "    slot %d parameter %u: listed %d\n", slot, i, listed);
               layout = false;
               break;
            }
            if (std::strcmp(fullTable()[id].tip, t.paramTable()[i].tip) != 0)
               layout = false;
         }
      }
      check(layout, "every layer carries its plugin's parameters at the promised ids");
   }

   const auto *state =
      static_cast<const clap_plugin_state_t *>(plugin->get_extension(plugin, CLAP_EXT_STATE));
   check(state != nullptr, "state extension present");
   auto saveState = [&](const clap_plugin_t *pl) {
      std::string blob;
      clap_ostream_t os{};
      os.ctx = &blob;
      os.write = [](const clap_ostream_t *s, const void *buf, uint64_t size) -> int64_t {
         static_cast<std::string *>(s->ctx)->append(static_cast<const char *>(buf), size);
         return static_cast<int64_t>(size);
      };
      state->save(pl, &os);
      return blob;
   };
   struct ReadCtx {
      const std::string *data;
      size_t pos;
   };
   auto loadState = [&](const clap_plugin_t *pl, const std::string &blob) {
      ReadCtx rc{&blob, 0};
      clap_istream_t is{};
      is.ctx = &rc;
      is.read = [](const clap_istream_t *s, void *buf, uint64_t size) -> int64_t {
         auto *c = static_cast<ReadCtx *>(s->ctx);
         const size_t n = std::min<size_t>(size, c->data->size() - c->pos);
         std::memcpy(buf, c->data->data() + c->pos, n);
         c->pos += n;
         return static_cast<int64_t>(n);
      };
      return state->load(pl, &is);
   };
   if (state) {
      const std::string blob = saveState(plugin);
      check(!blob.empty(), "state blob is not empty");
      check(loadState(plugin, blob), "state load");
      check(!loadState(plugin, std::string("not a scene at all, not even close")),
            "state load rejects garbage");
   }

   // --- an empty scene is silence, not noise
   check(plugin->activate(plugin, sampleRate, 1, 512), "activate");
   {
      const RenderResult r = renderPlugin(plugin, sampleRate, 512, -1.0, 1.0, 60, 0.0);
      check(!r.sawNonFinite && r.peak == 0.0f, "an empty scene renders exact silence");
   }

   // --- every factory scene: loads cleanly, is silent until a key opens it,
   // plays on the key, stays bounded
   const std::vector<PresetEntry> scenes = discoverPresets(entry);
   // The scene the envelope, effects and seed checks run on: fixed in
   // selftest_scene.h, loaded from a file the way a host loads one.
   PresetEntry fixture;
   fixture.name = "Self-Test Scene";
   fixture.locationKind = CLAP_PRESET_DISCOVERY_LOCATION_FILE;
   fixture.location = (std::filesystem::temp_directory_path() /
                       ("verdaliscene-selftest-" + std::to_string(static_cast<long>(getpid())) +
                        ".verdaliscene"))
                         .string();
   {
      std::ofstream out(fixture.location, std::ios::binary);
      out << kSelfTestScene;
   }
   check(!scenes.empty(), "factory scenes are discovered");
   for (const PresetEntry &p : scenes) {
      const clap_plugin_t *pl = createPlugin(entry);
      if (!pl)
         continue;
      const uint32_t warnings = gLogWarnings;
      const uint32_t errors = gPresetErrorCount;
      const bool loaded = loadPreset(pl, p);
      pl->activate(pl, sampleRate, 1, 512);
      const RenderResult quiet = renderPlugin(pl, sampleRate, 512, -1.0, 1.0, 60, 0.0);
      const RenderResult r = renderPlugin(pl, sampleRate, 512, 4.0, 0.0, 60, 0.9);
      check(loaded && gPresetErrorCount == errors && gLogWarnings == warnings &&
               quiet.peak == 0.0f && !r.sawNonFinite && r.peak > 0.001f && r.peak <= 1.001f,
            "scene \"" + p.name + "\" loads, waits for a key and plays (peak " +
               std::to_string(20.0 * std::log10(std::max(r.peak, 1e-9f))).substr(0, 5) +
               " dBFS)");
      pl->deactivate(pl);
      pl->destroy(pl);
   }

   // --- a key starts the scene and letting go ends it: the scene's release and
   // every layer's own run out into exact silence, and the next key starts it
   // all again
   if (!scenes.empty()) {
      const clap_plugin_t *pl = createPlugin(entry);
      loadPreset(pl, fixture);
      pl->activate(pl, sampleRate, 1, 512);
      const RenderResult held = renderPlugin(pl, sampleRate, 512, 3.0, 0.0, 60, 0.9);
      // The note-off goes out with the first block of the next render.
      const RenderResult after = renderPlugin(pl, sampleRate, 512, 0.0, 40.0, 60, 0.9);
      float lastSecond = 0.0f;
      const size_t tailStart = after.interleaved.size() - static_cast<size_t>(2 * sampleRate);
      for (size_t i = tailStart; i < after.interleaved.size(); ++i)
         lastSecond = std::max(lastSecond, std::fabs(after.interleaved[i]));
      check(held.peak > 0.001f && after.peak > 0.0f && lastSecond == 0.0f,
            "letting go of the key fades the scene out into silence");
      const RenderResult again = renderPlugin(pl, sampleRate, 512, 2.0, 0.0, 60, 0.9);
      check(again.peak > 0.001f, "the next key starts the scene again");
      pl->deactivate(pl);
      pl->destroy(pl);
   }

   // --- the curves reach the sound: a release bent to hold the level up
   // leaves more of the scene after the key than one bent to let go at once,
   // for the scene's own envelope and for a layer's
   if (!scenes.empty()) {
      auto releaseEnergy = [&](const std::vector<std::pair<clap_id, double>> &curves) {
         const clap_plugin_t *pl = createPlugin(entry);
         loadPreset(pl, fixture);
         pl->activate(pl, sampleRate, 1, 512);
         resolveParamOverrides(pl, {"random seed=7"});
         for (const auto &c : curves)
            gParamOverrides.push_back(c);
         renderPlugin(pl, sampleRate, 512, 5.0, 0.0, 60, 0.9);
         gParamOverrides.clear();
         const RenderResult tail = renderPlugin(pl, sampleRate, 512, 0.0, 1.5, 60, 0.9);
         pl->deactivate(pl);
         pl->destroy(pl);
         return tail.rms;
      };
      const double heldScene = releaseEnergy({{kParamReleaseCurve, -1.0}});
      const double goneScene = releaseEnergy({{kParamReleaseCurve, 1.0}});
      check(heldScene > 1.5 * goneScene,
            "the scene's release curve shapes its fade-out (" +
               std::to_string(toDb(heldScene)).substr(0, 5) + " vs " +
               std::to_string(toDb(goneScene)).substr(0, 5) + " dBFS)");
      Scene first;
      std::string err;
      int slot = -1;
      // The scene the energy was measured on.
      const std::string text = kSelfTestScene;
      if (parseScene(text.c_str(), text.size(), first, err) && !first.layers.empty())
         slot = slotIndex(first.layers.front().type, 0);
      bool ok = false;
      std::string what = "no layer";
      if (slot >= 0) {
         // The scene's own release long and straight, so what is left after
         // the key is the layer's.
         const double heldLayer =
            releaseEnergy({{kParamReleaseCurve, -1.0}, {slotMixId(slot, kSlotReleaseCurve), -1.0}});
         const double goneLayer =
            releaseEnergy({{kParamReleaseCurve, -1.0}, {slotMixId(slot, kSlotReleaseCurve), 1.0}});
         ok = heldLayer > goneLayer * 1.02;
         what = std::to_string(toDb(heldLayer)).substr(0, 5) + " vs " +
                std::to_string(toDb(goneLayer)).substr(0, 5) + " dBFS";
      }
      check(ok, "a layer's release curve shapes the layer's own fade-out (" + what + ")");
   }

   // --- effects: each one, switched on for a layer through the host's own
   // parameter events, changes the sound and stays finite -- which also means
   // its buffers were made on the main thread when it was asked for -- and the
   // scene's own reverb rings on after the scene has faded
   if (!scenes.empty()) {
      Scene first;
      std::string err;
      const std::string text = kSelfTestScene;
      const int slot = parseScene(text.c_str(), text.size(), first, err) && !first.layers.empty()
                          ? slotIndex(first.layers.front().type, 0)
                          : 0;
      auto take = [&](const std::vector<std::pair<clap_id, double>> &extra, double hold,
                      double tail) {
         const clap_plugin_t *pl = createPlugin(entry);
         loadPreset(pl, fixture);
         pl->activate(pl, sampleRate, 1, 512);
         resolveParamOverrides(pl, {"random seed=7"});
         for (const auto &e : extra)
            gParamOverrides.push_back(e);
         RenderResult r = renderPlugin(pl, sampleRate, 512, hold, 0.0, 60, 0.9);
         gParamOverrides.clear();
         if (tail > 0.0) {
            const RenderResult t = renderPlugin(pl, sampleRate, 512, 0.0, tail, 60, 0.9);
            r.interleaved.insert(r.interleaved.end(), t.interleaved.begin(), t.interleaved.end());
            r.sawNonFinite = r.sawNonFinite || t.sawNonFinite;
            r.peak = std::max(r.peak, t.peak);
         }
         pl->deactivate(pl);
         pl->destroy(pl);
         return r;
      };
      const RenderResult plain = take({}, 2.0, 0.0);
      bool allChange = true, allFinite = true;
      std::string quietOnes;
      for (int k = 0; k < kNumFxKinds; ++k) {
         std::vector<std::pair<clap_id, double>> on = {{fxParamId(slot, fxKind(k).first), 1.0}};
         // Make each one plainly audible.
         if (k == kFxReverb)
            on.push_back({fxParamId(slot, kFxReverbMix), 0.6});
         if (k == kFxDelay)
            on.push_back({fxParamId(slot, kFxDelayMix), 0.6});
         if (k == kFxWidener)
            on.push_back({fxParamId(slot, kFxWidenerWidth), 0.0});
         if (k == kFxAutoPan)
            on.push_back({fxParamId(slot, kFxAutoPanDepth), 1.0});
         const RenderResult r = take(on, 2.0, 0.0);
         double diff = 0.0;
         for (size_t i = 0; i < r.interleaved.size() && i < plain.interleaved.size(); ++i)
            diff += std::fabs(r.interleaved[i] - plain.interleaved[i]);
         diff /= std::max<size_t>(1, r.interleaved.size());
         allFinite = allFinite && !r.sawNonFinite && r.peak <= 1.001f;
         if (!(diff > 1.0e-4)) {
            allChange = false;
            quietOnes += std::string(" ") + fxKind(k).name;
         }
      }
      check(allChange, "every effect on a layer changes its sound" +
                          (quietOnes.empty() ? std::string() : " (not:" + quietOnes + ")"));
      check(allFinite, "every effect on a layer stays finite and bounded");

      // The scene's own reverb, after the envelope: what is left a few seconds
      // after the scene's release has run out.
      auto lateRms = [](const RenderResult &r, double from, double to, double rate) {
         const size_t a = static_cast<size_t>(from * rate) * 2;
         const size_t b = std::min(r.interleaved.size(), static_cast<size_t>(to * rate) * 2);
         double q = 0.0;
         for (size_t i = a; i < b; ++i)
            q += static_cast<double>(r.interleaved[i]) * r.interleaved[i];
         return b > a ? std::sqrt(q / static_cast<double>(b - a)) : 0.0;
      };
      const double release = first.values[kParamRelease];
      (void)release;
      const RenderResult dryTail = take({}, 2.0, 14.0);
      const RenderResult wetTail = take({{fxParamId(kMasterFx, kFxReverbOn), 1.0},
                                         {fxParamId(kMasterFx, kFxReverbMix), 0.5},
                                         {fxParamId(kMasterFx, kFxReverbDecay),
                                          realToParam(fxParamTable()[kFxReverbDecay], 12000.0)}},
                                        2.0, 14.0);
      const double dryLate = lateRms(dryTail, 13.0, 16.0, sampleRate);
      const double wetLate = lateRms(wetTail, 13.0, 16.0, sampleRate);
      check(dryLate == 0.0 && wetLate > 1.0e-5,
            "the scene's reverb rings on after the scene has faded (" +
               std::to_string(toDb(wetLate)).substr(0, 5) + " dBFS 11 s after the key)");

      // FX Tails: on Ring Out a layer's reverb rings on after the scene has
      // faded, as the scene's does; on Release both are gone with the scene's
      // release -- and the scene's reverb still sounds while the key is held.
      const double ringOut = static_cast<double>(kTailsRingOut);
      const double fadeOut = static_cast<double>(kTailsRelease);
      const std::vector<std::pair<clap_id, double>> layerVerb = {
         {fxParamId(slot, kFxReverbOn), 1.0},
         {fxParamId(slot, kFxReverbMix), 0.5},
         {fxParamId(slot, kFxReverbDecay), realToParam(fxParamTable()[kFxReverbDecay], 12000.0)}};
      auto with = [](std::vector<std::pair<clap_id, double>> v, clap_id id, double value) {
         v.push_back({id, value});
         return v;
      };
      const double layerRing =
         lateRms(take(with(layerVerb, kParamFxTails, ringOut), 2.0, 14.0), 13.0, 16.0, sampleRate);
      const double layerGone =
         lateRms(take(with(layerVerb, kParamFxTails, fadeOut), 2.0, 14.0), 13.0, 16.0, sampleRate);
      const std::vector<std::pair<clap_id, double>> sceneVerb = {
         {fxParamId(kMasterFx, kFxReverbOn), 1.0},
         {fxParamId(kMasterFx, kFxReverbMix), 0.5},
         {fxParamId(kMasterFx, kFxReverbDecay),
          realToParam(fxParamTable()[kFxReverbDecay], 12000.0)}};
      const RenderResult sceneFade = take(with(sceneVerb, kParamFxTails, fadeOut), 2.0, 14.0);
      const double sceneGone = lateRms(sceneFade, 13.0, 16.0, sampleRate);
      double heldDiff = 0.0;
      const size_t held = static_cast<size_t>(2.0 * sampleRate) * 2;
      for (size_t i = 0; i < held && i < sceneFade.interleaved.size() &&
                         i < dryTail.interleaved.size();
           ++i)
         heldDiff += std::fabs(sceneFade.interleaved[i] - dryTail.interleaved[i]);
      check(layerRing > 1.0e-5 && layerGone == 0.0,
            "FX Tails: a layer's reverb rings on after the scene on Ring Out (" +
               std::to_string(toDb(layerRing)).substr(0, 5) +
               " dBFS 11 s after the key) and is gone with it on Release");
      check(sceneGone == 0.0 && heldDiff > 1.0e-3,
            "FX Tails: on Release the scene's reverb sounds while the key is held and is "
            "gone with the scene's release");
      // And fades *with* the release, rather than holding up until it ends:
      // 3 to 4.5 s after the key the faded tail is well under the ringing one.
      const double fading = lateRms(sceneFade, 5.0, 6.5, sampleRate);
      const double ringing = lateRms(wetTail, 5.0, 6.5, sampleRate);
      check(fading < 0.5 * ringing,
            "FX Tails: on Release the scene's reverb fades with the release (" +
               std::to_string(toDb(fading)).substr(0, 5) + " against " +
               std::to_string(toDb(ringing)).substr(0, 5) + " dBFS on Ring Out, 3 s after the key)");
   }

   // --- with every layer's seed fixed, a scene is the same scene every time
   //     -- effects included: a scene that carries a reverb and a delay on a
   //     layer plays them the same in every take, the first one too
   if (!scenes.empty()) {
      Scene fixed;
      std::string ferr;
      PresetEntry wet = fixture;
      wet.location += ".wet.verdaliscene";
      if (parseScene(kSelfTestScene, std::strlen(kSelfTestScene), fixed, ferr) &&
          !fixed.layers.empty()) {
         fixed.layers.front().fx[kFxReverbOn] = 1.0;
         fixed.layers.front().fx[kFxDelayOn] = 1.0;
         std::ofstream out(wet.location, std::ios::binary);
         out << formatScene(fixed);
      }
      loadPreset(plugin, wet);
      resolveParamOverrides(plugin, {"random seed=7"});
      const auto overrides = gParamOverrides;
      plugin->reset(plugin);
      const RenderResult first = renderPlugin(plugin, sampleRate, 512, 0.5, 0.5, 60, 1.0);
      gParamOverrides.clear();
      renderPlugin(plugin, sampleRate, 512, 0.4, 0.4, 48, 1.0);
      plugin->reset(plugin);
      gParamOverrides = overrides;
      const RenderResult second = renderPlugin(plugin, sampleRate, 512, 0.5, 0.5, 60, 1.0);
      gParamOverrides.clear();
      check(!overrides.empty() && first.interleaved == second.interleaved,
            "fixed layer seeds render identically after reset");
   }

   // --- every parameter at once, the layer switches included: all 36 layers
   // built, run and torn down again through the main-thread handover
   {
      RenderResult r;
      const std::vector<double> atMax = driveAllParams(plugin, sampleRate, Extreme::Max, &r);
      check(!r.sawNonFinite, "all parameters at maximum (every layer on): output stays finite");
      check(r.peak <= 1.001f, "all parameters at maximum: output stays bounded");
      bool reflected = true;
      const uint32_t count2 = params->count(plugin);
      for (uint32_t i = 0; i < count2 && i < atMax.size(); ++i) {
         clap_param_info_t info{};
         if (params->get_info(plugin, i, &info) && std::fabs(atMax[i] - info.max_value) > 1e-9)
            reflected = false;
      }
      check(reflected, "get_value() reflects incoming parameter events");
      driveAllParams(plugin, sampleRate, Extreme::Min, &r);
      check(!r.sawNonFinite, "all parameters at minimum (every layer off): output stays finite");
      driveAllParams(plugin, sampleRate, Extreme::Mid, &r);
      check(!r.sawNonFinite, "all parameters mid-range: output stays finite");
      check(r.peak > 0.0f, "all parameters mid-range: still audible");
   }

   // --- out of range values from the host must be clamped, not trusted
   {
      const uint32_t count2 = params->count(plugin);
      gParamOverrides.clear();
      for (uint32_t i = 0; i < count2; ++i) {
         clap_param_info_t info{};
         if (params->get_info(plugin, i, &info))
            gParamOverrides.emplace_back(info.id, info.max_value + 1000.0);
      }
      const RenderResult r = renderPlugin(plugin, sampleRate, 512, 0.3, 0.3, 60, 1.0);
      gParamOverrides.clear();
      bool clamped = true;
      for (uint32_t i = 0; i < count2; ++i) {
         clap_param_info_t info{};
         if (!params->get_info(plugin, i, &info))
            continue;
         double v = 0.0;
         params->get_value(plugin, info.id, &v);
         if (v > info.max_value + 1e-9)
            clamped = false;
      }
      check(clamped, "out-of-range parameter values are clamped");
      check(!r.sawNonFinite, "out-of-range parameters do not break the DSP");
   }

   // --- a real state round trip: a scene, then everything changed, then back
   if (state && !scenes.empty()) {
      loadPreset(plugin, scenes.back());
      renderPlugin(plugin, sampleRate, 512, 0.2, 0.2, 60, 1.0);
      const std::vector<double> original = snapshotParams(plugin);
      const std::string saved = saveState(plugin);
      RenderResult r;
      const std::vector<double> changed = driveAllParams(plugin, sampleRate, Extreme::Min, &r);
      check(changed != original, "parameters actually changed before restoring");
      loadState(plugin, saved);
      check(snapshotParams(plugin) == original, "state load restores every parameter exactly");
      const RenderResult after = renderPlugin(plugin, sampleRate, 512, 0.2, 1.0, 60, 1.0);
      check(!after.sawNonFinite && after.peak > 0.0f, "a restored scene plays again");
   }

   // --- deactivate/activate cycles must be safe, with layers in place
   plugin->deactivate(plugin);
   check(plugin->activate(plugin, sampleRate, 1, 256), "re-activate after deactivate");
   {
      const RenderResult r = renderPlugin(plugin, sampleRate, 256, 0.3, 0.3, 60, 1.0);
      check(!r.sawNonFinite && r.peak > 0.0f, "plays after re-activation");
   }
   const auto *tailExt =
      static_cast<const clap_plugin_tail_t *>(plugin->get_extension(plugin, CLAP_EXT_TAIL));
   check(tailExt != nullptr, "tail extension present");
   check(tailExt && tailExt->get(plugin) > 0, "tail is a positive number of samples");

   // --- the scene format: what is written is what is read
   {
      Scene original = emptyScene();
      original.name = "Round Trip";
      original.author = "selftest";
      original.description = "Written by the self-test.";
      const ParamDesc *scene = sceneParamTable();
      for (uint32_t i = 0; i < kNumSceneParams; ++i) {
         const ParamDesc &d = scene[i];
         double v = d.min + 0.37 * (d.max - d.min);
         if (d.kind == ParamKind::Enum || d.kind == ParamKind::Stepped)
            v = std::floor(v + 0.5);
         original.values[i] = v;
      }
      for (uint32_t f = 0; f < kNumFxParams; ++f) {
         const ParamDesc &d = fxParamTable()[f];
         double v = d.min + 0.61 * (d.max - d.min);
         if (d.kind == ParamKind::Enum || d.kind == ParamKind::Stepped)
            v = std::floor(v + 0.5);
         original.fx[f] = v;
      }
      for (int type = 0; type < kNumLayerTypes; ++type) {
         for (int copy = 0; copy < 2; ++copy) {
            SceneLayer layer = defaultLayer(type);
            const LayerType &t = layerType(type);
            const ParamDesc *table = t.paramTable();
            for (uint32_t i = 0; i < t.paramCount; ++i) {
               const ParamDesc &d = table[i];
               double v = d.min + (0.21 + 0.3 * copy) * (d.max - d.min);
               if (d.kind == ParamKind::Enum || d.kind == ParamKind::Stepped)
                  v = std::floor(v + 0.5);
               layer.values[i] = v;
            }
            layer.mix[kSlotLevel] = -7.5;
            layer.mix[kSlotPan] = copy ? 0.4 : -0.25;
            layer.mix[kSlotStereo] = copy;
            layer.mix[kSlotShotRate] = t.shotLevelParam != kNoLayerParam ? 4.5 : 2.0;
            layer.mix[kSlotAttackCurve] = copy ? -0.35 : 0.6;
            layer.mix[kSlotDecayCurve] = slotParamApplies(t, kSlotDecayCurve) ? -0.8 : 0.0;
            layer.mix[kSlotReleaseCurve] = copy ? 0.25 : -1.0;
            for (uint32_t f = 0; f < kNumFxParams; ++f) {
               const ParamDesc &d = fxParamTable()[f];
               double v = d.min + (0.17 + 0.29 * copy + 0.013 * type) * (d.max - d.min);
               if (d.kind == ParamKind::Enum || d.kind == ParamKind::Stepped)
                  v = std::floor(v + 0.5);
               layer.fx[f] = v;
            }
            layer.presetName = "Layer " + std::to_string(copy);
            original.layers.push_back(layer);
         }
      }
      const std::string text = formatScene(original);
      Scene back;
      std::string err;
      check(parseScene(text.c_str(), text.size(), back, err) && back.warnings.empty(),
            "a written scene parses back in");
      check(back.name == original.name && back.description == original.description &&
               back.layers.size() == original.layers.size(),
            "a written scene keeps its name and every layer");
      double worst = 0.0;
      std::string where;
      auto compare = [&](const ParamDesc &d, double want, double got, const std::string &what) {
         const double span = d.max - d.min;
         const double e = span > 0.0 ? std::fabs(got - want) / span : 0.0;
         if (e > worst) {
            worst = e;
            where = what + " " + d.key;
         }
      };
      for (uint32_t i = 0; i < kNumSceneParams; ++i)
         compare(scene[i], original.values[i], back.values[i], "scene");
      for (uint32_t f = 0; f < kNumFxParams; ++f)
         compare(fxParamTable()[f], original.fx[f], back.fx[f], "scene effects");
      for (size_t k = 0; k < back.layers.size() && k < original.layers.size(); ++k) {
         const SceneLayer &a = original.layers[k];
         const SceneLayer &b = back.layers[k];
         const LayerType &t = layerType(a.type);
         if (a.type != b.type || a.presetName != b.presetName)
            worst = 1.0;
         for (uint32_t i = 0; i < t.paramCount; ++i)
            compare(t.paramTable()[i], a.values[i], b.values[i], t.pluginName);
         for (uint32_t m = kSlotLevel; m < kNumSlotParams; ++m)
            if (slotParamApplies(t, m))
               compare(slotParamTable()[m], a.mix[m], b.mix[m], "mixer");
         for (uint32_t f = 0; f < kNumFxParams; ++f)
            compare(fxParamTable()[f], a.fx[f], b.fx[f], "effects");
      }
      if (worst > 0.005)
         std::printf("       worst drift %.4f on %s\n", worst, where.c_str());
      check(worst <= 0.005, "a written scene reads back with the same values");
   }

   // --- every factory scene's layers resolve: no layer_from naming nothing
   {
      bool clean = true;
      for (unsigned i = 0; i < kNumBuiltinPresets; ++i) {
         Scene s;
         std::string err;
         const char *text = kBuiltinPresets[i].text;
         if (!parseScene(text, std::strlen(text), s, err) || !s.warnings.empty() ||
             s.layers.empty()) {
            clean = false;
            std::printf("       %s: %s\n", kBuiltinPresets[i].loadKey,
                        !err.empty() ? err.c_str()
                        : s.warnings.empty() ? "no layers"
                                             : s.warnings.front().c_str());
         }
      }
      check(clean, "every factory scene names only layers and presets that exist");
   }

   // --- compatibility: every preset of every plugin loads into a layer, and a
   // layer writes back out as that plugin's own preset, value for value
   {
      int loaded = 0;
      bool allLoad = true;
      double worst = 0.0;
      std::string where;
      for (int type = 0; type < kNumLayerTypes; ++type) {
         const LayerType &t = layerType(type);
         const verdalis::PresetLibrarySpec lib = t.library();
         for (unsigned i = 0; i < lib.builtinCount; ++i) {
            SceneLayer layer = defaultLayer(type);
            std::string err;
            if (!applyLayerPreset(type, lib.builtins[i].text, layer, err)) {
               allLoad = false;
               std::printf("       %s %s: %s\n", t.pluginName, lib.builtins[i].loadKey,
                           err.c_str());
               continue;
            }
            ++loaded;
            const std::string out = formatLayerPreset(layer, layer.presetName);
            PresetData a, b;
            verdalis::parsePreset(lib.ctx, lib.builtins[i].text,
                                  std::strlen(lib.builtins[i].text), a, err);
            verdalis::parsePreset(lib.ctx, out.c_str(), out.size(), b, err);
            for (const auto &want : a.values) {
               for (const auto &got : b.values) {
                  if (got.first != want.first)
                     continue;
                  const ParamDesc &d = t.paramTable()[want.first];
                  const double span = d.max - d.min;
                  const double e = span > 0.0 ? std::fabs(got.second - want.second) / span : 0.0;
                  if (e > worst) {
                     worst = e;
                     where = std::string(t.pluginName) + " " + lib.builtins[i].loadKey + " " +
                             d.key;
                  }
               }
            }
         }
      }
      std::printf("  %d plugin presets loaded into layers\n", loaded);
      check(allLoad && loaded > 100, "every preset of every plugin loads into a layer");
      if (worst > 0.005)
         std::printf("       worst drift %.4f on %s\n", worst, where.c_str());
      check(worst <= 0.005, "a layer writes back as its plugin's preset, value for value");
   }

   // --- a layer saved as a preset lands in that plugin's own library, on a
   // real filesystem
   {
      const std::string tmpdir =
         (std::filesystem::temp_directory_path() /
          ("verdaliscene-selftest-" + std::to_string(
#if defined(_WIN32)
              static_cast<unsigned long>(GetCurrentProcessId())
#else
              static_cast<unsigned long>(getpid())
#endif
              ))).string();
      std::error_code mkec;
      if (std::filesystem::create_directories(tmpdir, mkec) || !mkec) {
         setEnvVar("XDG_CONFIG_HOME", tmpdir.c_str());
         setEnvVar("APPDATA", tmpdir.c_str());
         const LayerType &t = layerType(kLayerRain);
         SceneLayer layer = freshLayer(kLayerRain);
         const std::string path =
            verdalis::userPresetPathIn(t.library().ctx, "From A Scene", "Saved By Selftest");
         std::string err;
         check(!path.empty() && writePresetFile(path, formatLayerPreset(layer, "Saved By Selftest"),
                                                err),
               "a layer preset writes into its plugin's user library");
         bool found = false;
         for (const auto &p : verdalis::scanPresetLibrary(t.library()))
            found = found || (p.name == "Saved By Selftest" &&
                              p.folder == verdalis::presetFileStem("From A Scene"));
         check(found, "the plugin's own library lists it, in its folder");
         check(!findLayerPresetText(kLayerRain, "Saved By Selftest").empty(),
               "a scene can name it with layer_from");

         // A scene mixed by ear, saved into a folder and handed over as a
         // pack, comes back as the same file: every layer's every value, the
         // layers' curves and the scene's own.
         Scene mixed;
         std::string perr;
         const char *factory = kBuiltinPresets[0].text;
         parseScene(factory, std::strlen(factory), mixed, perr);
         mixed.name = "Mixed By Ear";
         mixed.values[kParamGate] = kGateNotes;
         mixed.values[kParamReleaseCurve] = -0.4;
         mixed.fx[kFxReverbOn] = 1.0;
         mixed.fx[kFxReverbDecay] = 0.8;
         for (SceneLayer &layer : mixed.layers) {
            layer.mix[kSlotLevel] -= 1.5;
            layer.mix[kSlotAttackCurve] = 0.3;
            layer.mix[kSlotReleaseCurve] = -0.65;
            layer.values[0] = layer.values[0] * 0.5;
            layer.fx[kFxDelayOn] = 1.0;
            layer.fx[kFxDelayFeedback] = 1.2;
         }
         const verdalis::PresetLibrarySpec sceneLib{presetContext(), kBuiltinPresets,
                                                    kNumBuiltinPresets};
         const std::string written = formatScene(mixed);
         const std::string scenePath =
            verdalis::userPresetPathIn(sceneLib.ctx, "Handover", "Mixed By Ear");
         const std::string packPath = tmpdir + "/handover.verdaliscenepack";
         bool same = !scenePath.empty() && writePresetFile(scenePath, written, perr) &&
                     verdalis::exportPresetPack(sceneLib, verdalis::scanPresetLibrary(sceneLib),
                                                verdalis::presetFileStem("Handover"), packPath,
                                                perr);
         std::error_code dirEc;
         std::filesystem::remove_all(std::filesystem::path(scenePath).parent_path(), dirEc);
         std::string folder;
         same = same && verdalis::importPresetPack(sceneLib, packPath, folder, perr);
         std::string imported;
         for (const auto &p : verdalis::scanPresetLibrary(sceneLib)) {
            if (p.name != "Mixed By Ear" || p.folder != folder)
               continue;
            std::ifstream in(p.path, std::ios::binary);
            imported.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
         }
         Scene back;
         same = same && imported == written &&
                parseScene(imported.c_str(), imported.size(), back, perr) &&
                formatScene(back) == written;
         if (!same)
            std::printf("       %s\n", perr.c_str());
         check(same, "a scene exported as a pack and imported again is the same scene");

         // A scene renamed and described in the browser keeps every layer:
         // only its name and description lines change.
         {
            const std::string editPath =
               verdalis::userPresetPathIn(sceneLib.ctx, "Edits", "Mixed By Ear");
            writePresetFile(editPath, written, perr);
            int at = -1;
            const std::vector<GuiPreset> lib = verdalis::scanPresetLibrary(sceneLib);
            for (size_t i = 0; i < lib.size(); ++i)
               if (lib[i].path == editPath)
                  at = static_cast<int>(i);
            std::string edited;
            Scene renamed;
            std::string text;
            bool kept = at >= 0 && verdalis::editPreset(sceneLib, lib[static_cast<size_t>(at)],
                                                        "Evening Mix", "Mixed for the evening.",
                                                        edited, perr);
            if (kept) {
               std::ifstream in(edited, std::ios::binary);
               text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
               kept = parseScene(text.c_str(), text.size(), renamed, perr);
            }
            if (kept) {
               renamed.name = mixed.name;
               renamed.description = mixed.description;
               kept = renamed.layers.size() == mixed.layers.size() &&
                      formatScene(renamed) == written && text.find("Evening Mix") != std::string::npos &&
                      text.find("Mixed for the evening.") != std::string::npos;
            }
            check(kept, "a scene renamed and described keeps every layer and value");
            std::filesystem::remove_all(std::filesystem::path(editPath).parent_path(), dirEc);
         }
         // Collections, moving, renaming, describing and deleting: on the
         // scene library, with its real factory scenes, and on a layer's,
         // which is its plugin's own.
         auto checkFn = [&](bool ok, const std::string &what) {
            check(ok, ("scenes: " + what).c_str());
         };
         verdalis::testing::checkPresetLibrary(sceneLib, checkFn);
         verdalis::testing::checkPresetLibrary(
            layerType(kLayerBirds).library(),
            [&](bool ok, const std::string &what) { check(ok, ("birds layer: " + what).c_str()); });
         std::error_code rmec;
         std::filesystem::remove_all(tmpdir, rmec);
         setEnvVar("XDG_CONFIG_HOME", nullptr);
         setEnvVar("APPDATA", nullptr);
      }
   }

   plugin->deactivate(plugin);
   plugin->destroy(plugin);

   std::error_code fixtureEc;
   std::filesystem::remove(fixture.location, fixtureEc);
   std::filesystem::remove(fixture.location + ".wet.verdaliscene", fixtureEc);

   std::printf("\nselftest: %d failure(s)\n", failures);
   return failures == 0 ? 0 : 1;
}

} // namespace
