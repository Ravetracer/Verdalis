#pragma once

// The desktop's own file chooser, for the two places the window needs one:
// writing a preset pack somewhere other than the plugin's own packs folder, and
// reading one back from wherever it was downloaded to.
//
// There is no toolkit here -- the window is raw X11 and Cairo, which is what
// keeps the plugin a single file with no runtime dependencies -- so there is no
// file chooser to call. On Windows there is one in the operating system. On
// Linux there is whatever the desktop installed, so this asks zenity or
// kdialog and reports honestly when it finds neither: the browser then falls
// back to its own list of packs, which is where everything it writes goes and
// is enough on its own.

#include <string>

namespace verdalis {

// Whether there is a chooser to open at all. The window hides its "Other..."
// entries when there is not, rather than offering a button that does nothing.
bool fileDialogAvailable();

// Both return the chosen path, or an empty string if the user cancelled or
// there was no chooser. Both block until the dialog closes, which on X11 means
// the plugin window stops repainting for as long as it is open -- the same deal
// a drag makes, and what every host-side file dialog does too.
std::string openFileDialog(const std::string &title, const std::string &filterName,
                           const std::string &extension);
std::string saveFileDialog(const std::string &title, const std::string &suggestedPath,
                           const std::string &filterName, const std::string &extension);

// Shows `path` in the desktop's file manager, so somebody who has just written
// a preset pack can get at the file itself without being told where it went.
// Returns false when there is no file manager to ask, which is the same answer
// fileDialogAvailable() gives for the same kind of desktop.
bool fileRevealAvailable();
bool revealInFileBrowser(const std::string &path);

} // namespace verdalis
