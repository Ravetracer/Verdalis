#include "verdalis/preset_library.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

namespace verdalis {

const char *const kFactoryFolder = "Factory Presets";

namespace {

std::string readWholeFile(const std::string &path) {
   std::ifstream file(path, std::ios::binary);
   if (!file)
      return {};
   std::ostringstream buf;
   buf << file.rdbuf();
   return buf.str();
}

} // namespace

std::vector<GuiPreset> scanPresetLibrary(const PresetLibrarySpec &spec) {
   std::vector<GuiPreset> out;

   for (unsigned i = 0; i < spec.builtinCount; ++i) {
      PresetData data;
      std::string error;
      if (!parsePreset(spec.ctx, spec.builtins[i].text, std::strlen(spec.builtins[i].text), data,
                       error))
         continue;
      GuiPreset entry;
      entry.name = data.name.empty() ? spec.builtins[i].loadKey : data.name;
      entry.description = data.description;
      entry.loadKey = spec.builtins[i].loadKey;
      entry.folder = kFactoryFolder;
      out.push_back(entry);
   }

   const std::string dir = userPresetDir(spec.ctx);
   std::error_code ec;
   if (dir.empty() || !std::filesystem::is_directory(dir, ec))
      return out;

   std::vector<GuiPreset> user;
   const std::string suffix = std::string(".") + spec.ctx.presetExtension;
   // The library's root and one level of folders under it. One level, because a
   // preset library is a shelf rather than a filesystem and a tree deep enough
   // to get lost in is one somebody will get lost in.
   auto scan = [&](const std::filesystem::path &from, const std::string &folder) {
      std::error_code dirEc;
      for (const auto &entry : std::filesystem::directory_iterator(from, dirEc)) {
         if (!entry.is_regular_file())
            continue;
         const std::string name = entry.path().filename().string();
         if (name.size() <= suffix.size() ||
             name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0)
            continue;
         const std::string path = entry.path().string();
         PresetData data;
         std::string error;
         if (!parsePresetFile(spec.ctx, path, data, error))
            continue;
         GuiPreset item;
         item.name = data.name.empty() ? name.substr(0, name.size() - suffix.size()) : data.name;
         item.description = data.description;
         item.path = path;
         item.userContent = true;
         item.folder = folder;
         user.push_back(item);
      }
   };

   scan(dir, std::string());
   std::vector<std::string> folders;
   for (const auto &entry : std::filesystem::directory_iterator(dir, ec))
      if (entry.is_directory())
         folders.push_back(entry.path().filename().string());
   std::sort(folders.begin(), folders.end());
   for (const auto &folder : folders)
      scan(std::filesystem::path(dir) / folder, folder);

   // Sorted by folder first so the browser's own grouping and the order the
   // arrow buttons walk in are the same order -- and with the root, which the
   // browser shows as Unfiled at the bottom of its shelves, last here too.
   std::sort(user.begin(), user.end(), [](const GuiPreset &a, const GuiPreset &b) {
      if (a.folder == b.folder)
         return a.name < b.name;
      if (a.folder.empty() || b.folder.empty())
         return b.folder.empty();
      return a.folder < b.folder;
   });
   out.insert(out.end(), user.begin(), user.end());
   return out;
}

void splitPresetFolder(const std::string &input, std::string &folder, std::string &leaf) {
   const size_t cut = input.find_last_of("/\\");
   if (cut == std::string::npos) {
      folder.clear();
      leaf = input;
      return;
   }
   folder = input.substr(0, cut);
   leaf = input.substr(cut + 1);
   // Only one level: "a/b/c" is folder "a_b", preset "c".
   for (char &c : folder)
      if (c == '/' || c == '\\')
         c = '_';
   auto trim = [](std::string &s) {
      while (!s.empty() && s.front() == ' ')
         s.erase(s.begin());
      while (!s.empty() && s.back() == ' ')
         s.pop_back();
   };
   trim(folder);
   trim(leaf);
}

std::string presetPackPathFor(const PresetLibrarySpec &spec, const std::string &folder) {
   const std::string dir = presetPackDir(spec.ctx);
   if (dir.empty())
      return {};
   std::string stem = presetFileStem(folder.empty() ? std::string("presets") : folder);
   if (stem.empty())
      stem = "presets";
   return dir + "/" + stem + "." + presetPackExtension(spec.ctx);
}

std::vector<std::string> presetPackFiles(const PresetLibrarySpec &spec) {
   std::vector<std::string> out;
   const std::string dir = presetPackDir(spec.ctx);
   std::error_code ec;
   if (dir.empty() || !std::filesystem::is_directory(dir, ec))
      return out;
   const std::string suffix = "." + presetPackExtension(spec.ctx);
   for (const auto &entry : std::filesystem::directory_iterator(dir, ec)) {
      if (!entry.is_regular_file())
         continue;
      const std::string name = entry.path().filename().string();
      if (name.size() > suffix.size() &&
          name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
         out.push_back(entry.path().string());
   }
   std::sort(out.begin(), out.end());
   return out;
}

bool exportPresetPack(const PresetLibrarySpec &spec, const std::vector<GuiPreset> &presets,
                      const std::string &folder, const std::string &path, std::string &error) {
   std::vector<PresetPackEntry> entries;
   for (const auto &preset : presets) {
      if (preset.folder != folder)
         continue;
      PresetPackEntry e;
      e.name = preset.name;
      if (!preset.loadKey.empty()) {
         // A factory preset lives in the binary rather than on disk.
         for (unsigned i = 0; i < spec.builtinCount; ++i)
            if (preset.loadKey == spec.builtins[i].loadKey)
               e.text = spec.builtins[i].text;
      } else {
         e.text = readWholeFile(preset.path);
      }
      if (!e.text.empty())
         entries.push_back(e);
   }
   if (entries.empty()) {
      error = "That folder has no presets in it.";
      return false;
   }
   const std::string text =
      formatPresetPack(spec.ctx, folder.empty() ? std::string("Presets") : folder, entries);
   return writePresetFile(path, text, error);
}

bool importPresetPack(const PresetLibrarySpec &spec, const std::string &path, std::string &folder,
                      std::string &error) {
   std::string packName;
   std::vector<PresetPackEntry> entries;
   if (!parsePresetPackFile(spec.ctx, path, packName, entries, error))
      return false;
   if (packName.empty())
      packName = std::filesystem::path(path).stem().string();

   const std::string dir = userPresetDir(spec.ctx);
   if (dir.empty()) {
      error = "No user preset directory: neither XDG_CONFIG_HOME nor HOME is set.";
      return false;
   }

   std::string stem = presetFileStem(packName);
   if (stem.empty())
      stem = "pack";
   std::string unique = stem;
   std::error_code ec;
   for (int n = 2; n < 100 && std::filesystem::exists(std::filesystem::path(dir) / unique, ec); ++n)
      unique = stem + "_" + std::to_string(n);

   int written = 0;
   for (const auto &entry : entries) {
      const std::string file = userPresetPathIn(spec.ctx, unique, entry.name);
      if (file.empty())
         continue;
      std::string werr;
      if (writePresetFile(file, entry.text, werr))
         ++written;
      else
         error = werr;
   }
   if (written == 0) {
      if (error.empty())
         error = "Nothing in the pack could be written.";
      return false;
   }

   folder = unique;
   return true;
}

// ------------------------------------------------------------- collections

namespace {

namespace fs = std::filesystem;

std::string trimmed(std::string s) {
   for (char &c : s)
      if (c == '\n' || c == '\r' || c == '\t')
         c = ' ';
   while (!s.empty() && s.front() == ' ')
      s.erase(s.begin());
   while (!s.empty() && s.back() == ' ')
      s.pop_back();
   return s;
}

// What a typed collection name becomes as a directory, or an error saying why
// it cannot be one.
bool collectionStem(const std::string &name, std::string &stem, std::string &error) {
   stem = presetFileStem(trimmed(name));
   if (stem.empty()) {
      error = "Give the collection a name.";
      return false;
   }
   // The factory shelf is not a directory, but a collection whose name reads
   // the same would look like it on the browser's shelves.
   if (stem == presetFileStem(kFactoryFolder)) {
      error = "That name belongs to the factory presets.";
      return false;
   }
   return true;
}

bool isPresetFile(const fs::path &p, const PresetLibrarySpec &spec) {
   const std::string suffix = std::string(".") + spec.ctx.presetExtension;
   const std::string name = p.filename().string();
   return name.size() > suffix.size() &&
          name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Moves a file, falling back to copy-and-remove where a rename cannot cross
// from one filesystem to another.
bool moveFile(const fs::path &from, const fs::path &to, std::string &error) {
   std::error_code ec;
   fs::rename(from, to, ec);
   if (!ec)
      return true;
   fs::copy_file(from, to, ec);
   if (ec) {
      error = "Could not move the preset: " + ec.message();
      return false;
   }
   fs::remove(from, ec);
   return true;
}

bool userPreset(const GuiPreset &preset, std::string &error) {
   if (preset.path.empty() || !preset.userContent) {
      error = "Factory presets are locked.";
      return false;
   }
   return true;
}

} // namespace

std::string presetCollectionDir(const PresetLibrarySpec &spec, const std::string &folder) {
   const std::string dir = userPresetDir(spec.ctx);
   if (dir.empty())
      return {};
   return folder.empty() ? dir : (fs::path(dir) / folder).string();
}

std::vector<std::string> presetCollections(const PresetLibrarySpec &spec) {
   std::vector<std::string> out;
   const std::string dir = userPresetDir(spec.ctx);
   std::error_code ec;
   if (dir.empty() || !fs::is_directory(dir, ec))
      return out;
   for (const auto &entry : fs::directory_iterator(dir, ec))
      if (entry.is_directory())
         out.push_back(entry.path().filename().string());
   std::sort(out.begin(), out.end());
   return out;
}

bool createPresetCollection(const PresetLibrarySpec &spec, const std::string &name,
                            std::string &folder, std::string &error) {
   std::string stem;
   if (!collectionStem(name, stem, error))
      return false;
   const std::string dir = presetCollectionDir(spec, stem);
   if (dir.empty()) {
      error = "No user preset directory: neither XDG_CONFIG_HOME nor HOME is set.";
      return false;
   }
   std::error_code ec;
   if (fs::exists(dir, ec)) {
      error = "There is already a collection called \"" + stem + "\".";
      return false;
   }
   fs::create_directories(dir, ec);
   if (ec) {
      error = "Could not make the collection: " + ec.message();
      return false;
   }
   folder = stem;
   return true;
}

bool renamePresetCollection(const PresetLibrarySpec &spec, const std::string &folder,
                            const std::string &name, std::string &renamed, std::string &error) {
   if (folder.empty()) {
      error = "Unfiled is not a collection and cannot be renamed.";
      return false;
   }
   std::string stem;
   if (!collectionStem(name, stem, error))
      return false;
   const std::string from = presetCollectionDir(spec, folder);
   const std::string to = presetCollectionDir(spec, stem);
   std::error_code ec;
   if (from.empty() || !fs::is_directory(from, ec)) {
      error = "That collection is not there any more.";
      return false;
   }
   if (stem == folder) {
      renamed = stem;
      return true;
   }
   // A rename that only changes case is a different name to the user and the
   // same directory to a case-insensitive filesystem, which exists() cannot
   // tell apart from a clash.
   const bool caseOnly = fs::exists(to, ec) && fs::equivalent(from, to, ec);
   if (fs::exists(to, ec) && !caseOnly) {
      error = "There is already a collection called \"" + stem + "\".";
      return false;
   }
   fs::rename(from, to, ec);
   if (ec) {
      error = "Could not rename the collection: " + ec.message();
      return false;
   }
   renamed = stem;
   return true;
}

bool deletePresetCollection(const PresetLibrarySpec &spec, const std::string &folder,
                            std::string &error) {
   if (folder.empty()) {
      error = "Unfiled is not a collection and cannot be deleted.";
      return false;
   }
   const std::string dir = presetCollectionDir(spec, folder);
   std::error_code ec;
   if (dir.empty() || !fs::is_directory(dir, ec)) {
      error = "That collection is not there any more.";
      return false;
   }
   // The presets, and then the directory only if that left it empty: anything
   // else somebody keeps in there is not ours to delete.
   for (const auto &entry : fs::directory_iterator(dir, ec))
      if (entry.is_regular_file() && isPresetFile(entry.path(), spec))
         fs::remove(entry.path(), ec);
   if (!fs::remove(dir, ec)) {
      error = "The presets are deleted, but the folder holds other files and was left in place.";
      return false;
   }
   return true;
}

bool movePresetToCollection(const PresetLibrarySpec &spec, const GuiPreset &preset,
                            const std::string &folder, std::string &moved, std::string &error) {
   if (!userPreset(preset, error))
      return false;
   const std::string dir = presetCollectionDir(spec, folder);
   if (dir.empty()) {
      error = "No user preset directory: neither XDG_CONFIG_HOME nor HOME is set.";
      return false;
   }
   const fs::path from(preset.path);
   const fs::path to = fs::path(dir) / from.filename();
   std::error_code ec;
   if (fs::equivalent(from.parent_path(), dir, ec)) {
      moved = preset.path;
      return true;
   }
   if (fs::exists(to, ec)) {
      error = "\"" + (folder.empty() ? std::string("Unfiled") : folder) +
              "\" already has a preset called \"" + preset.name + "\".";
      return false;
   }
   fs::create_directories(dir, ec);
   if (!moveFile(from, to, error))
      return false;
   moved = to.string();
   return true;
}

std::string withPresetHeader(const std::string &text, const std::string &name,
                             const std::string &description) {
   std::vector<std::string> lines;
   std::string line;
   std::istringstream in(text);
   while (std::getline(in, line)) {
      if (!line.empty() && line.back() == '\r')
         line.pop_back();
      lines.push_back(line);
   }
   auto keyOf = [](const std::string &l) {
      const size_t eq = l.find('=');
      return eq == std::string::npos || (!l.empty() && l[0] == '#') ? std::string()
                                                                     : trimmed(l.substr(0, eq));
   };
   const std::string desc = trimmed(description);
   int nameAt = -1, descAt = -1;
   for (size_t i = 0; i < lines.size(); ++i) {
      const std::string key = keyOf(lines[i]);
      if (key == "name" && nameAt < 0)
         nameAt = static_cast<int>(i);
      else if (key == "description" && descAt < 0)
         descAt = static_cast<int>(i);
   }
   if (nameAt >= 0) {
      lines[static_cast<size_t>(nameAt)] = "name = " + trimmed(name);
   } else {
      // After the header comment and the format line, where a preset written
      // by the plugin has it.
      size_t at = 0;
      while (at < lines.size() && (lines[at].empty() || lines[at][0] == '#' ||
                                   keyOf(lines[at]) == "format"))
         ++at;
      lines.insert(lines.begin() + static_cast<long>(at), "name = " + trimmed(name));
      nameAt = static_cast<int>(at);
      if (descAt >= nameAt)
         ++descAt;
   }
   if (descAt >= 0) {
      if (desc.empty())
         lines.erase(lines.begin() + descAt);
      else
         lines[static_cast<size_t>(descAt)] = "description = " + desc;
   } else if (!desc.empty()) {
      // After the author when there is one directly below the name, as every
      // factory preset has it; otherwise straight after the name.
      size_t at = static_cast<size_t>(nameAt) + 1;
      if (at < lines.size() && keyOf(lines[at]) == "author")
         ++at;
      lines.insert(lines.begin() + static_cast<long>(at), "description = " + desc);
   }
   std::string out;
   for (const auto &l : lines)
      out += l + "\n";
   return out;
}

bool editPreset(const PresetLibrarySpec &spec, const GuiPreset &preset, const std::string &name,
                const std::string &description, std::string &edited, std::string &error) {
   if (!userPreset(preset, error))
      return false;
   const std::string clean = trimmed(name);
   if (presetFileStem(clean).empty()) {
      error = "Give the preset a name.";
      return false;
   }
   const std::string text = readWholeFile(preset.path);
   if (text.empty()) {
      error = "The preset's file could not be read.";
      return false;
   }
   const std::string next = withPresetHeader(text, clean, description);
   // Checked before anything is written: an edit that fails half way must not
   // leave a preset in two places, or in none.
   PresetData check;
   if (!parsePreset(spec.ctx, next.c_str(), next.size(), check, error))
      return false;

   const std::string to = userPresetPathIn(spec.ctx, preset.folder, clean);
   std::error_code ec;
   const bool sameFile = to == preset.path || (fs::exists(to, ec) && fs::equivalent(to, preset.path, ec));
   if (!sameFile && fs::exists(to, ec)) {
      error = "There is already a preset called \"" + clean + "\" in this collection.";
      return false;
   }
   if (!writePresetFile(sameFile ? preset.path : to, next, error))
      return false;
   if (!sameFile)
      fs::remove(preset.path, ec);
   edited = sameFile ? preset.path : to;
   return true;
}

bool deletePreset(const PresetLibrarySpec &spec, const GuiPreset &preset, std::string &error) {
   (void)spec;
   if (!userPreset(preset, error))
      return false;
   std::error_code ec;
   if (!fs::remove(preset.path, ec)) {
      error = ec ? "Could not delete the preset: " + ec.message()
                 : std::string("The preset's file is not there any more.");
      return false;
   }
   return true;
}

int rescanFollowing(const PresetLibrarySpec &spec, std::vector<GuiPreset> &presets, int current,
                    const std::string &from, const std::string &to) {
   std::string key, path;
   if (current >= 0 && current < static_cast<int>(presets.size())) {
      key = presets[static_cast<size_t>(current)].loadKey;
      path = presets[static_cast<size_t>(current)].path;
   }
   // A file, or anything under a directory: "from" itself, or "from/...".
   if (!path.empty() && !from.empty() &&
       (path == from || (path.size() > from.size() && path.compare(0, from.size(), from) == 0 &&
                         (path[from.size()] == '/' || path[from.size()] == '\\'))))
      path = to.empty() ? std::string() : to + path.substr(from.size());
   presets = scanPresetLibrary(spec);
   for (size_t i = 0; i < presets.size(); ++i)
      if ((!key.empty() && presets[i].loadKey == key) || (!path.empty() && presets[i].path == path))
         return static_cast<int>(i);
   return -1;
}

} // namespace verdalis
