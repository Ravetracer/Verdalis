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
   // arrow buttons walk in are the same order.
   std::sort(user.begin(), user.end(), [](const GuiPreset &a, const GuiPreset &b) {
      return a.folder == b.folder ? a.name < b.name : a.folder < b.folder;
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

} // namespace verdalis
