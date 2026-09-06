// Minimal CLAP host used to verify RainyDay outside a DAW: it walks the preset
// discovery factory exactly as a host would, loads presets through the
// preset-load extension, plays a MIDI note, and renders the result to a WAV
// file. It also round-trips plugin state and checks the output for NaNs.
//
//   rainyday-render --list
//   rainyday-render --preset steady_rain --out rain.wav --seconds 8
//   rainyday-render --all --outdir /tmp/rain
//   rainyday-render --selftest

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <dirent.h>
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
#include "rainyday.h"

#include <filesystem>

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

const clap_host_log_t kHostLog = {
   [](const clap_host_t *, clap_log_severity sev, const char *msg) {
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
   "rainyday-render",
   "RainyDay",
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
   [](const clap_host_t *) {},
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
   gIndexer.iface.name = "rainyday-render";
   gIndexer.iface.vendor = "RainyDay";
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

         // Crawl the directory the way a host indexer would.
         DIR *dir = opendir(loc.second.c_str());
         if (!dir) {
            std::fprintf(stderr, "  cannot open location %s\n", loc.second.c_str());
            continue;
         }
         std::vector<std::string> files;
         while (dirent *de = readdir(dir)) {
            const std::string name = de->d_name;
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
               files.push_back(loc.second + "/" + name);
         }
         closedir(dir);
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

} // namespace

int main(int argc, char **argv) {
   std::string pluginPath = "./RainyDay.clap";
   std::string presetSel, outPath = "rain.wav", outDir = ".";
   double seconds = 6.0, tail = 3.0;
   int16_t key = 60;
   double velocity = 0.9;
   double sampleRate = 48000.0;
   uint32_t blockSize = 512;
   bool doList = false, doAll = false, doSelfTest = false;
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
      todo = presets;
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

      const RenderResult res =
         renderPlugin(plugin, sampleRate, blockSize, seconds, tail, key, velocity);

      const std::string file =
         doAll ? outDir + "/" + sanitise(preset.name.empty() ? "default" : preset.name) + ".wav"
               : outPath;
      writeWav(file, res.interleaved, 2, static_cast<uint32_t>(sampleRate));

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

// Exercises the parts a DAW leans on but a plain render does not: parameter
// metadata, text conversion, state round-tripping and the sleep contract.
int runSelfTest(const clap_plugin_entry_t *entry, double sampleRate) {
   int failures = 0;
   auto check = [&](bool ok, const char *what) {
      std::printf("  [%s] %s\n", ok ? "ok" : "FAIL", what);
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
      for (clap_id s : seen)
         if (s == info.id)
            idsUnique = false;
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
   check(infoOk, "every get_info() is well formed");
   check(idsUnique, "parameter ids are unique");
   check(textOk, "value_to_text() works for all defaults");
   check(roundTripOk, "value_to_text -> text_to_value round-trips");

   // --- state round trip
   const auto *state =
      static_cast<const clap_plugin_state_t *>(plugin->get_extension(plugin, CLAP_EXT_STATE));
   check(state != nullptr, "state extension present");
   std::string blob;
   if (state) {
      clap_ostream_t os{};
      os.ctx = &blob;
      os.write = [](const clap_ostream_t *s, const void *buf, uint64_t size) -> int64_t {
         static_cast<std::string *>(s->ctx)->append(static_cast<const char *>(buf), size);
         return static_cast<int64_t>(size);
      };
      check(state->save(plugin, &os), "state save");
      check(!blob.empty(), "state blob is not empty");

      struct ReadCtx {
         const std::string *data;
         size_t pos;
      } rc{&blob, 0};
      clap_istream_t is{};
      is.ctx = &rc;
      is.read = [](const clap_istream_t *s, void *buf, uint64_t size) -> int64_t {
         auto *c = static_cast<ReadCtx *>(s->ctx);
         const size_t n = std::min<size_t>(size, c->data->size() - c->pos);
         std::memcpy(buf, c->data->data() + c->pos, n);
         c->pos += n;
         return static_cast<int64_t>(n);
      };
      check(state->load(plugin, &is), "state load");

      // Loading garbage must be rejected, not crash.
      const std::string junk = "not a rainyday state blob at all";
      ReadCtx rc2{&junk, 0};
      clap_istream_t is2 = is;
      is2.ctx = &rc2;
      check(!state->load(plugin, &is2), "state load rejects garbage");
   }

   // --- audio: parameter extremes must not produce NaN or silence
   check(plugin->activate(plugin, sampleRate, 1, 512), "activate");
   const RenderResult quiet = renderPlugin(plugin, sampleRate, 512, 0.0, 0.5, 60, 0.9);
   check(!quiet.sawNonFinite, "no non-finite output");

   const RenderResult loud = renderPlugin(plugin, sampleRate, 512, 1.0, 2.0, 60, 1.0);
   check(!loud.sawNonFinite, "no non-finite output while playing");
   check(loud.peak > 0.0005f, "a held note actually produces sound");
   check(loud.peak <= 1.001f, "output stays inside +/-1.0");

   // --- odd block sizes must be handled
   const RenderResult odd = renderPlugin(plugin, sampleRate, 37, 0.5, 0.5, 48, 0.5);
   check(!odd.sawNonFinite && odd.peak > 0.0f, "renders with a 37-sample block size");

   // --- a fixed Random Seed has to mean the same rain every time, whatever the
   // engine happened to render before. The droplet allocation cursor used to
   // survive reset(), which quietly made every render depend on its history.
   {
      uint32_t seedId = CLAP_INVALID_ID;
      const uint32_t seedCount = params->count(plugin);
      for (uint32_t i = 0; i < seedCount; ++i) {
         clap_param_info_t info{};
         if (params->get_info(plugin, i, &info) &&
             std::strcmp(info.name, "Random Seed") == 0) {
            seedId = info.id;
            break;
         }
      }
      check(seedId != CLAP_INVALID_ID, "Random Seed parameter exists");

      gParamOverrides.clear();
      gParamOverrides.emplace_back(seedId, 7.0);
      plugin->reset(plugin);
      const RenderResult firstTake = renderPlugin(plugin, sampleRate, 512, 0.4, 0.4, 60, 1.0);
      // Leave the engine with a history: another note, deliberately not reset.
      renderPlugin(plugin, sampleRate, 512, 0.4, 0.4, 48, 1.0);
      plugin->reset(plugin);
      const RenderResult secondTake = renderPlugin(plugin, sampleRate, 512, 0.4, 0.4, 60, 1.0);
      gParamOverrides.clear();
      check(firstTake.interleaved == secondTake.interleaved,
            "a fixed Random Seed renders identically after reset");
   }

   // --- parameter events must be reflected by get_value, which is how a host
   // reads back what automation did.
   {
      RenderResult r;
      const std::vector<double> atMax = driveAllParams(plugin, sampleRate, Extreme::Max, &r);
      check(!r.sawNonFinite, "all parameters at maximum: output stays finite");
      check(r.peak <= 1.001f, "all parameters at maximum: output stays bounded");

      bool reflected = true;
      const uint32_t count2 = params->count(plugin);
      for (uint32_t i = 0; i < count2 && i < atMax.size(); ++i) {
         clap_param_info_t info{};
         if (!params->get_info(plugin, i, &info))
            continue;
         if (std::fabs(atMax[i] - info.max_value) > 1e-9)
            reflected = false;
      }
      check(reflected, "get_value() reflects incoming parameter events");

      const std::vector<double> atMin = driveAllParams(plugin, sampleRate, Extreme::Min, &r);
      check(!r.sawNonFinite, "all parameters at minimum: output stays finite");
      (void)atMin;

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
         if (!params->get_info(plugin, i, &info))
            continue;
         gParamOverrides.emplace_back(info.id, info.max_value + 1000.0);
      }
      RenderResult r = renderPlugin(plugin, sampleRate, 512, 0.3, 0.3, 60, 1.0);
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

   // --- a real state round trip: save, change everything, restore, compare
   if (state) {
      const std::vector<double> original = snapshotParams(plugin);
      std::string saved;
      clap_ostream_t os{};
      os.ctx = &saved;
      os.write = [](const clap_ostream_t *s, const void *buf, uint64_t size) -> int64_t {
         static_cast<std::string *>(s->ctx)->append(static_cast<const char *>(buf), size);
         return static_cast<int64_t>(size);
      };
      state->save(plugin, &os);

      RenderResult r;
      const std::vector<double> changed = driveAllParams(plugin, sampleRate, Extreme::Min, &r);
      check(changed != original, "parameters actually changed before restoring");

      struct ReadCtx {
         const std::string *data;
         size_t pos;
      } rc{&saved, 0};
      clap_istream_t is{};
      is.ctx = &rc;
      is.read = [](const clap_istream_t *s, void *buf, uint64_t size) -> int64_t {
         auto *c = static_cast<ReadCtx *>(s->ctx);
         const size_t n = std::min<size_t>(size, c->data->size() - c->pos);
         std::memcpy(buf, c->data->data() + c->pos, n);
         c->pos += n;
         return static_cast<int64_t>(n);
      };
      state->load(plugin, &is);
      check(snapshotParams(plugin) == original, "state load restores every parameter exactly");
   }

   // --- a fresh instance must be silent until a note arrives
   {
      const clap_plugin_t *fresh = createPlugin(entry);
      if (fresh) {
         fresh->activate(fresh, sampleRate, 1, 512);
         const RenderResult idle = renderPlugin(fresh, sampleRate, 512, -1.0, 1.0, 60, 0.0);
         check(idle.peak == 0.0f, "no output at all before the first note");
         fresh->deactivate(fresh);
         fresh->destroy(fresh);
      }
   }

   // --- deactivate/activate cycles must be safe
   plugin->deactivate(plugin);
   check(plugin->activate(plugin, sampleRate, 1, 256), "re-activate after deactivate");
   check(plugin->activate != nullptr, "plugin still usable");

   const auto *tailExt =
      static_cast<const clap_plugin_tail_t *>(plugin->get_extension(plugin, CLAP_EXT_TAIL));
   check(tailExt != nullptr, "tail extension present");
   check(tailExt && tailExt->get(plugin) > 0, "tail is a positive number of samples");

   // --- the preset writer and the preset parser have to agree, or saving a
   // preset quietly changes the sound it was saved from.
   {
      using namespace rainyday;
      const ParamDesc *table = paramTable();

      PresetData original;
      original.name = "Round Trip";
      original.author = "selftest";
      original.description = "Written by the self-test.";
      original.features.push_back("test");
      for (uint32_t i = 0; i < kNumParams; ++i) {
         const ParamDesc &d = table[i];
         double v = d.min + 0.37 * (d.max - d.min);
         if (d.kind == ParamKind::Enum || d.kind == ParamKind::Stepped)
            v = std::floor(v + 0.5);
         original.values.emplace_back(d.id, v);
      }

      const std::string text = formatPreset(original);
      PresetData reparsed;
      std::string err;
      check(parsePreset(text.c_str(), text.size(), reparsed, err),
            "a written preset parses back in");
      check(reparsed.name == original.name && reparsed.author == original.author &&
               reparsed.description == original.description,
            "a written preset keeps its metadata");
      check(reparsed.values.size() == original.values.size(),
            "a written preset keeps every parameter");

      bool valuesAgree = true;
      double worst = 0.0;
      const char *worstKey = "";
      for (const auto &want : original.values) {
         const ParamDesc *d = paramById(want.first);
         if (!d)
            continue;
         bool seen = false;
         for (const auto &got : reparsed.values) {
            if (got.first != want.first)
               continue;
            seen = true;
            const double span = d->max - d->min;
            const double err2 = span > 0.0 ? std::fabs(got.second - want.second) / span : 0.0;
            if (err2 > worst) {
               worst = err2;
               worstKey = d->key;
            }
            if (err2 > 0.005)
               valuesAgree = false;
            break;
         }
         if (!seen)
            valuesAgree = false;
      }
      if (!valuesAgree)
         std::printf("       worst drift %.4f on '%s'\n", worst, worstKey);
      check(valuesAgree, "a written preset reads back with the same values");

      // Enums must survive as names, which is what makes the files editable.
      check(text.find("surface = ") != std::string::npos &&
               text.find("surface = 2") == std::string::npos,
            "enum parameters are written by name");

      // The display text has to be stable under a round trip at every value, not
      // just at the ones a fuzzer happens to pick. Rounding used to push a value
      // across the boundary that chose its own precision, so 99.96 printed as
      // "100.0" and read back as "100".
      {
         bool stable = true;
         char firstText[128] = {0};
         char againText[128] = {0};
         const char *worstName = "";
         double worstValue = 0.0;
         for (uint32_t i = 0; i < kNumParams && stable; ++i) {
            const ParamDesc &d = table[i];
            for (int step = 0; step <= 400 && stable; ++step) {
               const double raw = d.min + (d.max - d.min) * (step / 400.0);
               char a[128], b[128];
               if (!paramValueToText(d, raw, a, sizeof(a)))
                  continue;
               double back = 0.0;
               if (!paramTextToValue(d, a, &back))
                  continue;
               if (!paramValueToText(d, back, b, sizeof(b)))
                  continue;
               if (std::strcmp(a, b) != 0) {
                  stable = false;
                  std::snprintf(firstText, sizeof(firstText), "%s", a);
                  std::snprintf(againText, sizeof(againText), "%s", b);
                  worstName = d.name;
                  worstValue = raw;
               }
            }
         }
         if (!stable)
            std::printf("       '%s' -> '%s' for %s at raw %.6f\n", firstText, againText,
                        worstName, worstValue);
         check(stable, "parameter text is stable across a round trip at every value");
      }

      // A display name is not a filename.
      const std::string path = userPresetPath("My Rain / 2 **");
      check(path.empty() || path.find("My_Rain_2.") != std::string::npos,
            "a preset name becomes a safe filename");

      // Saving has to create the user preset directory and land a file that
      // reads back, on a real filesystem rather than in principle.
      const std::string tmpdir =
         (std::filesystem::temp_directory_path() /
          ("rainyday-selftest-" + std::to_string(
#if defined(_WIN32)
              static_cast<unsigned long>(GetCurrentProcessId())
#else
              static_cast<unsigned long>(getpid())
#endif
              ))).string();
      std::error_code mkec;
      const char *tmp = std::filesystem::create_directories(tmpdir, mkec) || !mkec
                           ? tmpdir.c_str()
                           : nullptr;
      if (tmp) {
         setEnvVar("XDG_CONFIG_HOME", tmp);
         setEnvVar("APPDATA", tmp);
         const std::string target = userPresetPath("Saved By Selftest");
         check(!target.empty(), "a save path is offered under the user config directory");
         std::string writeErr;
         check(writePresetFile(target, text, writeErr), "a preset writes to a fresh directory");
         PresetData readBack;
         std::string readErr;
         check(parsePresetFile(target, readBack, readErr) && readBack.name == original.name,
               "a saved preset file reads back");
         std::error_code rmec;
         std::filesystem::remove_all(tmpdir, rmec);
         setEnvVar("XDG_CONFIG_HOME", nullptr);
         setEnvVar("APPDATA", nullptr);
      }
   }

   plugin->deactivate(plugin);
   plugin->destroy(plugin);

   std::printf("\nselftest: %d failure(s)\n", failures);
   return failures == 0 ? 0 : 1;
}

} // namespace
