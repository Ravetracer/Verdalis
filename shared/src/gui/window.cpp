// The Verdalis plugin window: see verdalis/gui/plugin_window.h, which holds the
// whole class so that a plugin can extend it. What is left here is the part
// that must exist exactly once per binary.

#include "verdalis/gui/plugin_window.h"

namespace verdalis {

#if defined(_WIN32)
// Windows hands the object back through the window's user data; the pointer is
// planted when CreateWindowEx delivers WM_NCCREATE.
LRESULT CALLBACK PluginWindow::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
   if (msg == WM_NCCREATE) {
      auto *cs = reinterpret_cast<CREATESTRUCTW *>(lp);
      SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                        reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
      auto *self = static_cast<PluginWindow *>(cs->lpCreateParams);
      if (self)
         self->mWindow = hwnd;
      return DefWindowProcW(hwnd, msg, wp, lp);
   }
   auto *self = reinterpret_cast<PluginWindow *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
   if (!self || !self->mWindow)
      return DefWindowProcW(hwnd, msg, wp, lp);
   return self->handleMessage(msg, wp, lp);
}
#endif

Gui *createWindow(GuiDelegate &delegate, const WindowSpec &spec) {
   auto *gui = new PluginWindow(delegate, spec);
   if (!gui->open()) {
      delete gui;
      return nullptr;
   }
   return gui;
}

} // namespace verdalis
