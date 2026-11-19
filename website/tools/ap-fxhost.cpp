// ap-fxhost -- a minimal offline CLAP host for rendering an effect plugin over
// an input WAV. Used by render-ap-demos.sh for Aurum and Shapeau, whose own
// renderers do not take input audio.
//
//   ap-fxhost --plugin X.clap --in in.wav --out out.wav
//             [--id plugin.id] [--sidechain sc.wav]
//             [--state file]      load a CLAP state document (CLAP_EXT_STATE)
//             [--preset file]     load a preset file (CLAP_EXT_PRESET_LOAD)
//             [--param Name=val]  set a parameter by its display name
//             [--at sec:Name=val] set a parameter at a time (automation)
//             [--bpm 120] [--tail 4] [--rate 48000] [--block 256]
//             [--list-params]
//
// The input is read as 16/24/32-bit PCM or 32-bit float, mono or stereo, and
// must be at --rate. The output is 32-bit float stereo, input length + tail.
// The transport runs from beat 0 at --bpm for the whole render.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <clap/clap.h>

namespace {

struct Wav {
   uint32_t rate = 0;
   std::vector<float> l, r;
};

bool readWav(const std::string &path, Wav &w) {
   std::ifstream f(path, std::ios::binary);
   if (!f)
      return false;
   std::vector<char> d((std::istreambuf_iterator<char>(f)), {});
   if (d.size() < 12 || std::memcmp(d.data(), "RIFF", 4) || std::memcmp(d.data() + 8, "WAVE", 4))
      return false;
   uint16_t fmt = 0, ch = 0, bits = 0;
   size_t pos = 12;
   const char *data = nullptr;
   uint32_t dataLen = 0;
   while (pos + 8 <= d.size()) {
      uint32_t len;
      std::memcpy(&len, d.data() + pos + 4, 4);
      const char *body = d.data() + pos + 8;
      if (!std::memcmp(d.data() + pos, "fmt ", 4)) {
         std::memcpy(&fmt, body, 2);
         std::memcpy(&ch, body + 2, 2);
         std::memcpy(&w.rate, body + 4, 4);
         std::memcpy(&bits, body + 14, 2);
         if (fmt == 0xFFFE && len >= 26)
            std::memcpy(&fmt, body + 24, 2);
      } else if (!std::memcmp(d.data() + pos, "data", 4)) {
         data = body;
         dataLen = std::min<uint32_t>(len, static_cast<uint32_t>(d.size() - pos - 8));
         break;
      }
      pos += 8 + len + (len & 1);
   }
   if (!data || !ch)
      return false;
   const size_t bps = bits / 8, frames = dataLen / (bps * ch);
   w.l.resize(frames);
   w.r.resize(frames);
   for (size_t i = 0; i < frames; ++i)
      for (uint16_t c = 0; c < std::min<uint16_t>(ch, 2); ++c) {
         const char *p = data + (i * ch + c) * bps;
         float v = 0;
         if (fmt == 3 && bits == 32)
            std::memcpy(&v, p, 4);
         else if (bits == 16) {
            int16_t s;
            std::memcpy(&s, p, 2);
            v = s / 32768.0f;
         } else if (bits == 24) {
            int32_t s = (static_cast<uint8_t>(p[0]) << 8) | (static_cast<uint8_t>(p[1]) << 16) |
                        (static_cast<uint8_t>(p[2]) << 24);
            v = static_cast<float>(s / 2147483648.0);
         } else if (bits == 32) {
            int32_t s;
            std::memcpy(&s, p, 4);
            v = static_cast<float>(s / 2147483648.0);
         } else
            return false;
         (c == 0 ? w.l : w.r)[i] = v;
         if (ch == 1)
            w.r[i] = v;
      }
   return true;
}

bool writeWav(const std::string &path, const std::vector<float> &l, const std::vector<float> &r,
              uint32_t rate) {
   std::ofstream f(path, std::ios::binary);
   if (!f)
      return false;
   const uint32_t frames = static_cast<uint32_t>(l.size()), dataLen = frames * 8;
   auto u32 = [&](uint32_t v) { f.write(reinterpret_cast<const char *>(&v), 4); };
   auto u16 = [&](uint16_t v) { f.write(reinterpret_cast<const char *>(&v), 2); };
   f.write("RIFF", 4);
   u32(36 + dataLen);
   f.write("WAVEfmt ", 8);
   u32(16);
   u16(3);
   u16(2);
   u32(rate);
   u32(rate * 8);
   u16(8);
   u16(32);
   f.write("data", 4);
   u32(dataLen);
   for (uint32_t i = 0; i < frames; ++i) {
      f.write(reinterpret_cast<const char *>(&l[i]), 4);
      f.write(reinterpret_cast<const char *>(&r[i]), 4);
   }
   return static_cast<bool>(f);
}

// ------------------------------------------------------------------ host side

bool gCallbackRequested = false;

const void *hostGetExtension(const clap_host_t *, const char *) { return nullptr; }
void hostRequestRestart(const clap_host_t *) {}
void hostRequestProcess(const clap_host_t *) {}
void hostRequestCallback(const clap_host_t *) { gCallbackRequested = true; }

const clap_host_t gHost = {
   CLAP_VERSION_INIT, nullptr, "ap-fxhost", "local", "", "1.0",
   hostGetExtension, hostRequestRestart, hostRequestProcess, hostRequestCallback};

struct EventList {
   std::vector<std::vector<uint8_t>> events;
   void addParam(uint32_t time, clap_id id, double value) {
      clap_event_param_value_t v{};
      v.header.size = sizeof(v);
      v.header.time = time;
      v.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
      v.header.type = CLAP_EVENT_PARAM_VALUE;
      v.param_id = id;
      v.note_id = -1;
      v.port_index = -1;
      v.channel = -1;
      v.key = -1;
      v.value = value;
      std::vector<uint8_t> b(sizeof(v));
      std::memcpy(b.data(), &v, sizeof(v));
      events.push_back(std::move(b));
   }
   static uint32_t size(const clap_input_events_t *list) {
      return static_cast<uint32_t>(static_cast<EventList *>(list->ctx)->events.size());
   }
   static const clap_event_header_t *get(const clap_input_events_t *list, uint32_t i) {
      return reinterpret_cast<const clap_event_header_t *>(
         static_cast<EventList *>(list->ctx)->events[i].data());
   }
};

bool outPush(const clap_output_events_t *, const clap_event_header_t *) { return true; }

struct IStream {
   std::string data;
   size_t pos = 0;
   static int64_t read(const clap_istream_t *s, void *buf, uint64_t size) {
      auto *self = static_cast<IStream *>(s->ctx);
      const size_t n = std::min<size_t>(size, self->data.size() - self->pos);
      std::memcpy(buf, self->data.data() + self->pos, n);
      self->pos += n;
      return static_cast<int64_t>(n);
   }
};

std::string lower(std::string s) {
   for (auto &c : s)
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
   return s;
}

struct ParamSet {
   double atSec;
   clap_id id;
   double value;
};

bool resolveParam(const clap_plugin_t *pl, const clap_plugin_params_t *params,
                  const std::string &spec, double atSec, std::vector<ParamSet> &out) {
   const size_t eq = spec.find('=');
   if (eq == std::string::npos)
      return false;
   const std::string name = lower(spec.substr(0, eq)), val = spec.substr(eq + 1);
   for (uint32_t i = 0; i < params->count(pl); ++i) {
      clap_param_info_t info{};
      params->get_info(pl, i, &info);
      const std::string full = lower(std::string(info.module) + "/" + info.name);
      if (lower(info.name) != name && full != name)
         continue;
      double v = 0;
      if (!params->text_to_value || !params->text_to_value(pl, info.id, val.c_str(), &v)) {
         char *end = nullptr;
         v = std::strtod(val.c_str(), &end);
         if (end == val.c_str()) {
            std::fprintf(stderr, "parameter '%s' does not take '%s'\n", info.name, val.c_str());
            return false;
         }
      }
      out.push_back({atSec, info.id, v});
      return true;
   }
   std::fprintf(stderr, "no parameter named '%s'\n", spec.substr(0, eq).c_str());
   return false;
}

} // namespace

int main(int argc, char **argv) {
   std::string pluginPath, id, inPath, scPath, outPath, statePath, presetPath;
   std::vector<std::string> paramSpecs, atSpecs;
   double bpm = 120, tail = 4, rate = 48000;
   uint32_t block = 256;
   bool listParams = false;
   for (int i = 1; i < argc; ++i) {
      const std::string a = argv[i];
      auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
      if (a == "--plugin") pluginPath = next();
      else if (a == "--id") id = next();
      else if (a == "--in") inPath = next();
      else if (a == "--sidechain") scPath = next();
      else if (a == "--out") outPath = next();
      else if (a == "--state") statePath = next();
      else if (a == "--preset") presetPath = next();
      else if (a == "--param") paramSpecs.push_back(next());
      else if (a == "--at") atSpecs.push_back(next());
      else if (a == "--bpm") bpm = std::atof(next().c_str());
      else if (a == "--tail") tail = std::atof(next().c_str());
      else if (a == "--rate") rate = std::atof(next().c_str());
      else if (a == "--block") block = static_cast<uint32_t>(std::atoi(next().c_str()));
      else if (a == "--list-params") listParams = true;
      else {
         std::fprintf(stderr, "unknown argument: %s\n", a.c_str());
         return 2;
      }
   }

   void *dso = dlopen(pluginPath.c_str(), RTLD_NOW | RTLD_LOCAL);
   if (!dso) {
      std::fprintf(stderr, "dlopen: %s\n", dlerror());
      return 1;
   }
   auto *entry = static_cast<const clap_plugin_entry_t *>(dlsym(dso, "clap_entry"));
   if (!entry || !entry->init(pluginPath.c_str())) {
      std::fprintf(stderr, "no usable clap_entry\n");
      return 1;
   }
   auto *factory = static_cast<const clap_plugin_factory_t *>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
   if (id.empty())
      id = factory->get_plugin_descriptor(factory, 0)->id;
   const clap_plugin_t *pl = factory->create_plugin(factory, &gHost, id.c_str());
   if (!pl || !pl->init(pl)) {
      std::fprintf(stderr, "cannot create %s\n", id.c_str());
      return 1;
   }
   auto *params = static_cast<const clap_plugin_params_t *>(pl->get_extension(pl, CLAP_EXT_PARAMS));

   if (listParams) {
      for (uint32_t i = 0; params && i < params->count(pl); ++i) {
         clap_param_info_t info{};
         params->get_info(pl, i, &info);
         char text[128] = "";
         params->value_to_text(pl, info.id, info.default_value, text, sizeof text);
         std::printf("%-6u %-28s %-20s [%g..%g] def %s\n", info.id, info.name, info.module,
                     info.min_value, info.max_value, text);
      }
      return 0;
   }

   if (!statePath.empty()) {
      auto *state = static_cast<const clap_plugin_state_t *>(pl->get_extension(pl, CLAP_EXT_STATE));
      std::ifstream f(statePath, std::ios::binary);
      IStream is;
      is.data.assign((std::istreambuf_iterator<char>(f)), {});
      clap_istream_t s{&is, IStream::read};
      if (!state || !state->load(pl, &s)) {
         std::fprintf(stderr, "state load failed: %s\n", statePath.c_str());
         return 1;
      }
   }
   if (!presetPath.empty()) {
      auto *pload = static_cast<const clap_plugin_preset_load_t *>(
         pl->get_extension(pl, CLAP_EXT_PRESET_LOAD));
      if (!pload || !pload->from_location(pl, CLAP_PRESET_DISCOVERY_LOCATION_FILE,
                                          presetPath.c_str(), nullptr)) {
         std::fprintf(stderr, "preset load failed: %s\n", presetPath.c_str());
         return 1;
      }
   }
   if (gCallbackRequested) {
      gCallbackRequested = false;
      pl->on_main_thread(pl);
   }

   std::vector<ParamSet> sets;
   for (const auto &s : paramSpecs)
      if (!params || !resolveParam(pl, params, s, 0.0, sets))
         return 1;
   for (const auto &s : atSpecs) {
      const size_t colon = s.find(':');
      if (colon == std::string::npos || !params ||
          !resolveParam(pl, params, s.substr(colon + 1), std::atof(s.substr(0, colon).c_str()), sets))
         return 1;
   }
   std::stable_sort(sets.begin(), sets.end(),
                    [](const ParamSet &a, const ParamSet &b) { return a.atSec < b.atSec; });

   Wav in, sc;
   if (!readWav(inPath, in) || in.rate != static_cast<uint32_t>(rate)) {
      std::fprintf(stderr, "cannot read %s at %g Hz\n", inPath.c_str(), rate);
      return 1;
   }
   const bool haveSc = !scPath.empty();
   if (haveSc && (!readWav(scPath, sc) || sc.rate != in.rate)) {
      std::fprintf(stderr, "cannot read sidechain %s\n", scPath.c_str());
      return 1;
   }

   if (!pl->activate(pl, rate, 1, block) || !pl->start_processing(pl)) {
      std::fprintf(stderr, "activate failed\n");
      return 1;
   }

   const size_t total = in.l.size() + static_cast<size_t>(tail * rate);
   std::vector<float> outL(total), outR(total), bl(block), br(block), sl(block), sr(block);
   EventList events;
   clap_input_events_t inEvents{&events, EventList::size, EventList::get};
   clap_output_events_t outEvents{nullptr, outPush};
   clap_event_transport_t tr{};
   tr.header.size = sizeof(tr);
   tr.header.type = CLAP_EVENT_TRANSPORT;
   tr.flags = CLAP_TRANSPORT_HAS_TEMPO | CLAP_TRANSPORT_HAS_BEATS_TIMELINE |
              CLAP_TRANSPORT_HAS_SECONDS_TIMELINE | CLAP_TRANSPORT_HAS_TIME_SIGNATURE |
              CLAP_TRANSPORT_IS_PLAYING;
   tr.tempo = bpm;
   tr.tsig_num = 4;
   tr.tsig_denom = 4;
   size_t nextSet = 0;

   for (size_t pos = 0; pos < total; pos += block) {
      const uint32_t n = static_cast<uint32_t>(std::min<size_t>(block, total - pos));
      for (uint32_t i = 0; i < n; ++i) {
         const size_t k = pos + i;
         bl[i] = k < in.l.size() ? in.l[k] : 0.0f;
         br[i] = k < in.r.size() ? in.r[k] : 0.0f;
         sl[i] = haveSc && k < sc.l.size() ? sc.l[k] : 0.0f;
         sr[i] = haveSc && k < sc.r.size() ? sc.r[k] : 0.0f;
      }
      events.events.clear();
      while (nextSet < sets.size() && sets[nextSet].atSec * rate < static_cast<double>(pos + n)) {
         const double at = sets[nextSet].atSec * rate - static_cast<double>(pos);
         events.addParam(static_cast<uint32_t>(std::max(0.0, at)), sets[nextSet].id, sets[nextSet].value);
         ++nextSet;
      }
      const double beats = static_cast<double>(pos) / rate * bpm / 60.0;
      tr.song_pos_beats = static_cast<clap_beattime>(std::llround(beats * CLAP_BEATTIME_FACTOR));
      tr.song_pos_seconds = static_cast<clap_sectime>(std::llround(pos / rate * CLAP_SECTIME_FACTOR));
      tr.bar_number = static_cast<int32_t>(beats / 4.0);
      tr.bar_start = static_cast<clap_beattime>(tr.bar_number * 4LL * CLAP_BEATTIME_FACTOR);

      float *inPtr[2] = {bl.data(), br.data()}, *scPtr[2] = {sl.data(), sr.data()};
      float *outPtr[2] = {outL.data() + pos, outR.data() + pos};
      clap_audio_buffer_t ins[2]{};
      ins[0].data32 = inPtr;
      ins[0].channel_count = 2;
      ins[1].data32 = scPtr;
      ins[1].channel_count = 2;
      clap_audio_buffer_t out{};
      out.data32 = outPtr;
      out.channel_count = 2;
      clap_process_t proc{};
      proc.steady_time = static_cast<int64_t>(pos);
      proc.frames_count = n;
      proc.transport = &tr;
      proc.audio_inputs = ins;
      proc.audio_inputs_count = haveSc ? 2 : 1;
      proc.audio_outputs = &out;
      proc.audio_outputs_count = 1;
      proc.in_events = &inEvents;
      proc.out_events = &outEvents;
      pl->process(pl, &proc);
      if (gCallbackRequested) {
         gCallbackRequested = false;
         pl->on_main_thread(pl);
      }
   }
   pl->stop_processing(pl);
   pl->deactivate(pl);
   pl->destroy(pl);
   entry->deinit();

   if (!writeWav(outPath, outL, outR, static_cast<uint32_t>(rate))) {
      std::fprintf(stderr, "cannot write %s\n", outPath.c_str());
      return 1;
   }
   return 0;
}
