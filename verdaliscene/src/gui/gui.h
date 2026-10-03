#pragma once

// VerdaliScene's window: the suite's window, extended with a tab per layer
// and a scene mixer. See gui.cpp.

#include "verdalis/gui/gui.h"

#include "scene_delegate.h"

namespace verdaliscene {

using verdalis::Gui;

// Returns nullptr if no X display could be opened.
Gui *createGui(SceneDelegate &delegate);

} // namespace verdaliscene
