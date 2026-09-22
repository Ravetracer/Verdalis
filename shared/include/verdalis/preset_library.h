#pragma once

// The preset library as the window's browser sees it: the factory presets that
// are compiled into the plugin, followed by the user's own directory and one
// level of folders under it, plus the pack machinery that writes a whole folder
// out as one file and reads one back in.
//
// This is the plugin side of the browser's folder support, and it is here
// rather than in each plugin because none of it is per-plugin: all it needs to
// know is the preset context and where the embedded factory presets are. Every
// plugin's GuiDelegate implementation is a handful of calls into this.

#include <string>
#include <vector>

#include "verdalis/gui/gui.h"
#include "verdalis/preset.h"

namespace verdalis {

// Everything the library machinery needs from a plugin.
struct PresetLibrarySpec {
   PresetContext ctx;
   // The table embed_presets.cmake generated, and its length.
   const BuiltinPreset *builtins = nullptr;
   unsigned builtinCount = 0;
};

// What the browser calls the presets that are compiled into the binary. They
// are not files and have no directory, so the name is ours to choose;
// everything else on the shelf is named after the folder it was found in.
extern const char *const kFactoryFolder;

// The whole library, in the order the browser lists it and the preset arrows
// walk it: the factory shelf, then the user's presets sorted by folder and then
// by name, so the browser's grouping and the arrows agree.
std::vector<GuiPreset> scanPresetLibrary(const PresetLibrarySpec &spec);

// "Folder/Name" is a save into a folder, creating it if it is not there; a name
// with no slash in it saves into the library's root, which is what every save
// did before folders existed. Typing the folder is the whole of the "make a new
// folder" gesture -- there is no second dialog, and a folder with nothing in it
// cannot be made, which is the right answer for something whose only purpose is
// to hold presets.
void splitPresetFolder(const std::string &input, std::string &folder, std::string &leaf);

// Where a pack of `folder` goes by default: the plugin's own packs directory.
std::string presetPackPathFor(const PresetLibrarySpec &spec, const std::string &folder);

// The packs already in that directory, sorted, as full paths.
std::vector<std::string> presetPackFiles(const PresetLibrarySpec &spec);

// Writes every preset in `presets` whose folder is `folder` to one pack at
// `path`. A factory preset is taken from the binary, a user preset from its
// file, and either way it is the preset's *text* that goes in -- so a line the
// shared format knows nothing about survives a round trip.
bool exportPresetPack(const PresetLibrarySpec &spec, const std::vector<GuiPreset> &presets,
                      const std::string &folder, const std::string &path, std::string &error);

// Reads a pack in as a new folder in the user library, whose name comes back in
// `folder`. Nothing is overwritten: a pack whose folder already exists is
// imported beside it under a numbered name, so importing the same pack twice
// gives two folders rather than a mixture of both versions in one.
bool importPresetPack(const PresetLibrarySpec &spec, const std::string &path, std::string &folder,
                      std::string &error);

} // namespace verdalis
