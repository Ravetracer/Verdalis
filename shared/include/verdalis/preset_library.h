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

// ------------------------------------------------------------- collections
//
// A collection is a folder one level under the user preset directory, which
// is all a folder ever was here; what this adds is managing them directly
// rather than only by typing "Folder/Name" into the save field. `folder` is
// always the directory's own name, and "" is the library's root, which the
// browser calls Unfiled. The factory presets are in the binary, not in a
// folder, so nothing below can touch them.

// Every collection, empty ones included, sorted. An empty collection exists
// as a directory -- one just made, or one whose presets were all moved out --
// so it is listed from the disk rather than from the presets in it.
std::vector<std::string> presetCollections(const PresetLibrarySpec &spec);

// Makes a collection called `name`, whose directory name comes back in
// `folder`. A name is turned into a safe directory name the way a preset's is.
bool createPresetCollection(const PresetLibrarySpec &spec, const std::string &name,
                            std::string &folder, std::string &error);

// Renames collection `folder` to `name`; the new directory name comes back in
// `renamed`. Never merges into a collection that is already there.
bool renamePresetCollection(const PresetLibrarySpec &spec, const std::string &folder,
                            const std::string &name, std::string &renamed, std::string &error);

// Deletes collection `folder` and every preset in it.
bool deletePresetCollection(const PresetLibrarySpec &spec, const std::string &folder,
                            std::string &error);

// Moves one of the user's presets into collection `folder` ("" is the root),
// keeping its name. Its new path comes back in `moved`. A collection that
// already has a preset of that name is left alone rather than overwritten.
bool movePresetToCollection(const PresetLibrarySpec &spec, const GuiPreset &preset,
                            const std::string &folder, std::string &moved, std::string &error);

// Renames one of the user's presets and sets its description, in place in its
// collection. Only the preset's `name` and `description` lines change, so a
// file holding lines the shared format knows nothing about -- a VerdaliScene
// scene's layers -- keeps them. Its new path comes back in `edited`.
bool editPreset(const PresetLibrarySpec &spec, const GuiPreset &preset, const std::string &name,
                const std::string &description, std::string &edited, std::string &error);

// Deletes one of the user's presets.
bool deletePreset(const PresetLibrarySpec &spec, const GuiPreset &preset, std::string &error);

// A preset's text with its `name` and `description` lines replaced, the
// description added after the name if there was none and removed if it is
// empty. Both are single lines.
std::string withPresetHeader(const std::string &text, const std::string &name,
                             const std::string &description);

// Rescans `presets` after one of the changes above and returns where the
// preset at `current` is now. A preset whose path started with `from` is
// looked for under `to` instead -- a file or a whole collection's directory --
// and one that is not found any more, because it was deleted, gives -1. A
// factory preset is found again by its load key.
int rescanFollowing(const PresetLibrarySpec &spec, std::vector<GuiPreset> &presets, int current,
                    const std::string &from, const std::string &to);

// The directory a collection lives in, or the library's root for "". Empty
// when there is no user preset directory at all.
std::string presetCollectionDir(const PresetLibrarySpec &spec, const std::string &folder);

} // namespace verdalis
