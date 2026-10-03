#pragma once

// The window is the suite's; see shared/include/verdalis/gui/gui.h for the
// interface and window.h for what a plugin describes about itself. This only
// brings those names into the plugin's namespace and declares the entry point.

#include "verdalis/gui/gui.h"

namespace verdalis {
struct WindowSpec;
class HeaderOrnament;
} // namespace verdalis

namespace skyhowl {

using verdalis::Gui;
using verdalis::GuiDelegate;
using verdalis::GuiPreset;

// Returns nullptr if no X display could be opened.
Gui *createGui(GuiDelegate &delegate);

// The window's description without its ornament, and a fresh ornament of the
// kind this window draws. createGui() is these two and createWindow(); they are
// separate so that VerdaliScene can show this plugin's panels as one of its
// layer pages.
verdalis::WindowSpec windowSpec();
verdalis::HeaderOrnament *createOrnament();

} // namespace skyhowl
