// Opens RainyDay's editor in a plain X11 window so the interface can be driven
// and photographed without a DAW. The plugin's GUI is embedded (non-floating),
// so a host has to supply the parent window; that is all this does.
//
//   ./rainyday-guihost [plugin.clap] [preset.rainyday] [seconds]
//
// It processes audio on a timer thread as well, because the editor reads its
// parameter values back through the same path a host would.

#include <clap/clap.h>

#include <X11/Xlib.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <string>
#include <thread>
#include <vector>

namespace {

const clap_host_t *gHostPtr = nullptr;
const clap_plugin_t *gPlugin = nullptr;
std::atomic<bool> gRunning{true};

const void *hostGetExtension(const clap_host_t *, const char *) { return nullptr; }
void hostRequestRestart(const clap_host_t *) {}
void hostRequestProcess(const clap_host_t *) {}
void hostRequestCallback(const clap_host_t *) {}

} // namespace

int main(int argc, char **argv) {
   const std::string pluginPath = argc > 1 ? argv[1] : "./RainyDay.clap";
   const char *presetPath = (argc > 2 && argv[2][0]) ? argv[2] : nullptr;
   const int liveSeconds = argc > 3 ? std::atoi(argv[3]) : 600;

   void *lib = dlopen(pluginPath.c_str(), RTLD_NOW | RTLD_LOCAL);
   if (!lib) {
      std::fprintf(stderr, "dlopen failed: %s\n", dlerror());
      return 1;
   }
   auto *entry = static_cast<const clap_plugin_entry_t *>(dlsym(lib, "clap_entry"));
   if (!entry || !entry->init(pluginPath.c_str())) {
      std::fprintf(stderr, "no usable clap_entry\n");
      return 1;
   }

   auto *fac = static_cast<const clap_plugin_factory_t *>(
      entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
   const clap_plugin_descriptor_t *desc = fac->get_plugin_descriptor(fac, 0);

   clap_host_t host{};
   host.clap_version = CLAP_VERSION;
   host.name = "rainyday-guihost";
   host.vendor = "RainyDay";
   host.url = "";
   host.version = "1.0";
   host.get_extension = hostGetExtension;
   host.request_restart = hostRequestRestart;
   host.request_process = hostRequestProcess;
   host.request_callback = hostRequestCallback;
   gHostPtr = &host;

   const clap_plugin_t *plug = fac->create_plugin(fac, &host, desc->id);
   if (!plug || !plug->init(plug)) {
      std::fprintf(stderr, "plugin init failed\n");
      return 1;
   }
   gPlugin = plug;

   const double sr = 48000.0;
   const uint32_t block = 512;
   plug->activate(plug, sr, block, block);
   plug->start_processing(plug);

   if (presetPath) {
      auto *pl = static_cast<const clap_plugin_preset_load_t *>(
         plug->get_extension(plug, CLAP_EXT_PRESET_LOAD));
      if (pl && !pl->from_location(plug, CLAP_PRESET_DISCOVERY_LOCATION_FILE, presetPath, ""))
         std::fprintf(stderr, "preset load failed: %s\n", presetPath);
   }

   auto *gui = static_cast<const clap_plugin_gui_t *>(plug->get_extension(plug, CLAP_EXT_GUI));
   if (!gui || !gui->is_api_supported(plug, CLAP_WINDOW_API_X11, false)) {
      std::fprintf(stderr, "plugin has no embedded X11 GUI\n");
      return 1;
   }
   if (!gui->create(plug, CLAP_WINDOW_API_X11, false)) {
      std::fprintf(stderr, "gui create failed\n");
      return 1;
   }

   uint32_t w = 900, h = 648;
   gui->get_size(plug, &w, &h);

   Display *dpy = XOpenDisplay(nullptr);
   if (!dpy) {
      std::fprintf(stderr, "no X display\n");
      return 1;
   }
   const int screen = DefaultScreen(dpy);
   Window win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen), 0, 0, w, h, 0,
                                    BlackPixel(dpy, screen), BlackPixel(dpy, screen));
   XStoreName(dpy, win, "RainyDay");
   XSelectInput(dpy, win, StructureNotifyMask);
   XMapWindow(dpy, win);
   XFlush(dpy);

   clap_window_t parent{};
   parent.api = CLAP_WINDOW_API_X11;
   parent.x11 = static_cast<clap_xwnd>(win);
   if (!gui->set_parent(plug, &parent)) {
      std::fprintf(stderr, "set_parent failed\n");
      return 1;
   }
   gui->show(plug);
   XFlush(dpy);

   // The editor publishes its parameter edits through process(), so the audio
   // thread has to keep turning for the interface to behave as it does in a host.
   std::thread audio([&] {
      std::vector<float> l(block), r(block);
      float *chans[2] = {l.data(), r.data()};
      clap_audio_buffer_t ab{};
      ab.data32 = chans;
      ab.channel_count = 2;
      clap_input_events_t in{};
      in.size = [](const clap_input_events_t *) -> uint32_t { return 0; };
      in.get = [](const clap_input_events_t *, uint32_t) -> const clap_event_header_t * {
         return nullptr;
      };
      clap_output_events_t out{};
      out.try_push = [](const clap_output_events_t *, const clap_event_header_t *) { return true; };
      while (gRunning.load()) {
         clap_process_t pr{};
         pr.frames_count = block;
         pr.audio_outputs = &ab;
         pr.audio_outputs_count = 1;
         pr.in_events = &in;
         pr.out_events = &out;
         plug->process(plug, &pr);
         std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
   });

   std::printf("window 0x%lx  %ux%u\n", win, w, h);
   std::fflush(stdout);

   const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(liveSeconds);
   while (std::chrono::steady_clock::now() < deadline) {
      while (XPending(dpy)) {
         XEvent ev;
         XNextEvent(dpy, &ev);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(16));
   }

   gRunning = false;
   audio.join();
   plug->stop_processing(plug);
   gui->destroy(plug);
   plug->deactivate(plug);
   plug->destroy(plug);
   entry->deinit();
   XCloseDisplay(dpy);
   return 0;
}
