#pragma once

// The preset library's collection and editing operations, checked end to end
// against a plugin's own preset context and factory presets. Shared so every
// plugin's offline self-test runs the same checks: the operations are shared
// code, and a plugin's part in them is only its context and its binary.
//
// It points XDG_CONFIG_HOME at a fresh temporary directory for its duration,
// so it never touches the real user library, and puts the old value back.
// Linux only, like the self-tests that call it.

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

#include "verdalis/preset.h"
#include "verdalis/preset_library.h"

namespace verdalis {
namespace testing {

inline void checkPresetLibrary(const PresetLibrarySpec &spec,
                               const std::function<void(bool, const std::string &)> &check) {
#if !defined(_WIN32)
   namespace fs = std::filesystem;
   if (spec.builtinCount == 0) {
      check(false, "collections: the plugin has a factory preset to test with");
      return;
   }
   char templ[] = "/tmp/verdalis-library-XXXXXX";
   const char *made = mkdtemp(templ);
   if (!made) {
      check(false, "collections: a temporary library could be made");
      return;
   }
   const std::string tmp = made;
   const char *was = std::getenv("XDG_CONFIG_HOME");
   const std::string old = was ? was : "";
   setenv("XDG_CONFIG_HOME", tmp.c_str(), 1);

   auto read = [](const std::string &path) {
      std::ifstream in(path, std::ios::binary);
      std::ostringstream buf;
      buf << in.rdbuf();
      return buf.str();
   };
   auto find = [](const std::vector<GuiPreset> &list, const std::string &name) {
      for (size_t i = 0; i < list.size(); ++i)
         if (list[i].userContent && list[i].name == name)
            return static_cast<int>(i);
      return -1;
   };
   std::string error, folder;

   // A collection made, refused twice, and listed even while empty.
   check(createPresetCollection(spec, "My Pads", folder, error) && folder == "My_Pads",
         "collections: one is made, named safely for the filesystem");
   check(!createPresetCollection(spec, "My Pads", folder, error),
         "collections: a second of the same name is refused");
   check(!createPresetCollection(spec, kFactoryFolder, folder, error),
         "collections: the factory shelf's name is refused");
   check(presetCollections(spec) == std::vector<std::string>{"My_Pads"},
         "collections: an empty one is listed");

   // A preset of the user's own, made from a factory preset's text, in the root.
   const std::string factory = spec.builtins[0].text;
   const std::string one = userPresetPathIn(spec.ctx, "", "Test One");
   writePresetFile(one, withPresetHeader(factory, "Test One", "first"), error);
   std::vector<GuiPreset> list = scanPresetLibrary(spec);
   int cur = find(list, "Test One");
   check(cur >= 0 && list[static_cast<size_t>(cur)].folder.empty() &&
            list[static_cast<size_t>(cur)].description == "first",
         "collections: a saved preset is in the root, with its description");

   // Moved into the collection, and the current preset follows it.
   std::string moved;
   const GuiPreset p1 = list[static_cast<size_t>(std::max(cur, 0))];
   check(movePresetToCollection(spec, p1, "My_Pads", moved, error) && !fs::exists(one) &&
            fs::exists(moved),
         "collections: a preset moves into a collection");
   cur = rescanFollowing(spec, list, cur, p1.path, moved);
   check(cur >= 0 && list[static_cast<size_t>(cur)].folder == "My_Pads" &&
            list[static_cast<size_t>(cur)].name == "Test One",
         "collections: the current preset follows a move");

   // Renamed and described, every other line of it untouched.
   const GuiPreset p2 = list[static_cast<size_t>(std::max(cur, 0))];
   std::string edited;
   check(editPreset(spec, p2, "Renamed", "A pad for the evening", edited, error) &&
            !fs::exists(p2.path) && fs::exists(edited),
         "editing: a preset is renamed in place in its collection");
   PresetData before, after;
   std::string e1, e2;
   const std::string afterText = read(edited);
   parsePreset(spec.ctx, factory.c_str(), factory.size(), before, e1);
   parsePreset(spec.ctx, afterText.c_str(), afterText.size(), after, e2);
   check(after.name == "Renamed" && after.description == "A pad for the evening" &&
            after.values == before.values,
         "editing: the name and description change and every value stays");
   cur = rescanFollowing(spec, list, cur, p2.path, edited);
   check(cur >= 0 && list[static_cast<size_t>(cur)].name == "Renamed",
         "editing: the current preset follows a rename");

   // Factory presets are locked.
   const GuiPreset fac = list[0];
   check(!fac.userContent && !movePresetToCollection(spec, fac, "My_Pads", moved, error) &&
            !editPreset(spec, fac, "X", "", edited, error) && !deletePreset(spec, fac, error),
         "collections: a factory preset cannot be moved, edited or deleted");

   // Nothing is overwritten by a move.
   const std::string two = userPresetPathIn(spec.ctx, "", "Renamed");
   writePresetFile(two, withPresetHeader(factory, "Renamed", ""), error);
   list = scanPresetLibrary(spec);
   int clash = -1;
   for (size_t i = 0; i < list.size(); ++i)
      if (list[i].path == two)
         clash = static_cast<int>(i);
   check(clash >= 0 &&
            !movePresetToCollection(spec, list[static_cast<size_t>(clash)], "My_Pads", moved,
                                    error) &&
            fs::exists(two),
         "collections: a move onto a preset of the same name is refused");
   fs::remove(two);

   // The collection renamed, and the current preset follows it there.
   list = scanPresetLibrary(spec);
   cur = find(list, "Renamed");
   std::string renamed;
   check(renamePresetCollection(spec, "My_Pads", "Ambient Pads", renamed, error) &&
            renamed == "Ambient_Pads" && presetCollections(spec) == std::vector<std::string>{"Ambient_Pads"},
         "collections: one is renamed");
   cur = rescanFollowing(spec, list, cur, presetCollectionDir(spec, "My_Pads"),
                         presetCollectionDir(spec, "Ambient_Pads"));
   check(cur >= 0 && list[static_cast<size_t>(cur)].folder == "Ambient_Pads",
         "collections: the current preset follows its collection's rename");

   // An export holds that collection and nothing else.
   const std::string loose = userPresetPathIn(spec.ctx, "", "Loose");
   writePresetFile(loose, withPresetHeader(factory, "Loose", ""), error);
   list = scanPresetLibrary(spec);
   const std::string packPath = tmp + "/out.pack";
   std::string packName;
   std::vector<PresetPackEntry> entries;
   check(exportPresetPack(spec, list, "Ambient_Pads", packPath, error) &&
            parsePresetPackFile(spec.ctx, packPath, packName, entries, error) &&
            entries.size() == 1 && entries[0].name == "Renamed",
         "collections: an export holds only the collection on screen");

   // Deleted: a preset, then the collection and what is in it.
   cur = find(list, "Loose");
   const GuiPreset p3 = list[static_cast<size_t>(std::max(cur, 0))];
   check(deletePreset(spec, p3, error) && !fs::exists(loose),
         "editing: a preset is deleted");
   check(rescanFollowing(spec, list, cur, p3.path, {}) < 0,
         "editing: a deleted current preset is no longer current");
   check(deletePresetCollection(spec, "Ambient_Pads", error) && presetCollections(spec).empty() &&
            find(scanPresetLibrary(spec), "Renamed") < 0,
         "collections: one is deleted with its presets");

   // The header edit itself: a description goes after the author, and an
   // empty one removes the line.
   const std::string head = "# x\nformat = 1\nname = A\nauthor = B\n\nkey = 1\n";
   check(withPresetHeader(head, "C", "D") ==
            "# x\nformat = 1\nname = C\nauthor = B\ndescription = D\n\nkey = 1\n",
         "editing: a description is added after the author");
   check(withPresetHeader(withPresetHeader(head, "C", "D"), "C", "") ==
            "# x\nformat = 1\nname = C\nauthor = B\n\nkey = 1\n",
         "editing: an empty description removes the line");

   std::error_code ec;
   fs::remove_all(tmp, ec);
   if (old.empty())
      unsetenv("XDG_CONFIG_HOME");
   else
      setenv("XDG_CONFIG_HOME", old.c_str(), 1);
#else
   (void)spec;
   (void)check;
#endif
}

} // namespace testing
} // namespace verdalis
