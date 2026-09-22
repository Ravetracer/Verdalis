#include "verdalis/gui/filedialog.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
#   include <windows.h>
#   include <commdlg.h>
#   include <shellapi.h>
#else
#   include <unistd.h>
#endif

namespace verdalis {

#if defined(_WIN32)

namespace {

// The common dialog wants a double-terminated list of "label\0pattern\0"
// pairs, which is not something std::string hands over conveniently.
std::wstring widen(const std::string &s) {
   if (s.empty())
      return std::wstring();
   const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
   std::wstring out(static_cast<size_t>(n), L'\0');
   MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), &out[0], n);
   return out;
}

std::string narrow(const wchar_t *s) {
   if (!s || !s[0])
      return std::string();
   const int n = WideCharToMultiByte(CP_UTF8, 0, s, -1, nullptr, 0, nullptr, nullptr);
   std::string out(static_cast<size_t>(n > 0 ? n - 1 : 0), '\0');
   if (n > 1)
      WideCharToMultiByte(CP_UTF8, 0, s, -1, &out[0], n, nullptr, nullptr);
   return out;
}

std::wstring filterBlock(const std::string &filterName, const std::string &extension) {
   const std::wstring label = widen(filterName + " (*." + extension + ")");
   const std::wstring pattern = widen("*." + extension);
   std::wstring block;
   block.append(label);
   block.push_back(L'\0');
   block.append(pattern);
   block.push_back(L'\0');
   block.append(widen("All files (*.*)"));
   block.push_back(L'\0');
   block.append(L"*.*");
   block.push_back(L'\0');
   block.push_back(L'\0');
   return block;
}

} // namespace

bool fileDialogAvailable() { return true; }

bool fileRevealAvailable() { return true; }

bool revealInFileBrowser(const std::string &path) {
   if (path.empty())
      return false;
   // /select, highlights the file itself rather than only opening the folder it
   // is in. ShellExecuteW rather than one of the COM file dialogs: it needs no
   // apartment, which a plugin does not own the host's thread to make.
   const std::wstring args = L"/select,\"" + widen(path) + L"\"";
   const HINSTANCE rc =
      ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
   return reinterpret_cast<intptr_t>(rc) > 32;
}

std::string openFileDialog(const std::string &title, const std::string &filterName,
                           const std::string &extension) {
   wchar_t file[MAX_PATH] = {0};
   const std::wstring filter = filterBlock(filterName, extension);
   const std::wstring wtitle = widen(title);
   OPENFILENAMEW ofn{};
   ofn.lStructSize = sizeof(ofn);
   ofn.lpstrFilter = filter.c_str();
   ofn.lpstrFile = file;
   ofn.nMaxFile = MAX_PATH;
   ofn.lpstrTitle = wtitle.c_str();
   ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
   return GetOpenFileNameW(&ofn) ? narrow(file) : std::string();
}

std::string saveFileDialog(const std::string &title, const std::string &suggestedPath,
                           const std::string &filterName, const std::string &extension) {
   wchar_t file[MAX_PATH] = {0};
   const std::wstring suggested = widen(suggestedPath);
   if (!suggested.empty() && suggested.size() < MAX_PATH)
      std::wmemcpy(file, suggested.c_str(), suggested.size() + 1);
   const std::wstring filter = filterBlock(filterName, extension);
   const std::wstring wtitle = widen(title);
   const std::wstring wext = widen(extension);
   OPENFILENAMEW ofn{};
   ofn.lStructSize = sizeof(ofn);
   ofn.lpstrFilter = filter.c_str();
   ofn.lpstrFile = file;
   ofn.nMaxFile = MAX_PATH;
   ofn.lpstrTitle = wtitle.c_str();
   ofn.lpstrDefExt = wext.c_str();
   ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
   return GetSaveFileNameW(&ofn) ? narrow(file) : std::string();
}

#else

namespace {

// Which chooser this desktop has, worked out once and remembered. Looked up in
// PATH by hand rather than by running a shell, because the answer is wanted
// before anything is run and running a shell to ask is the expensive way.
// Wanted by two callers now, so the PATH walk stops being chooser()'s own.
const char *inPath(const char *name) {
   const char *path = std::getenv("PATH");
   if (!path)
      return nullptr;
   const char *start = path;
   while (*start) {
      const char *end = std::strchr(start, ':');
      const size_t len = end ? static_cast<size_t>(end - start) : std::strlen(start);
      if (len > 0) {
         std::string candidate(start, len);
         candidate += "/";
         candidate += name;
         if (access(candidate.c_str(), X_OK) == 0)
            return name;
      }
      if (!end)
         break;
      start = end + 1;
   }
   return nullptr;
}

const char *chooser() {
   static const char *found = []() -> const char * {
      static const char *const kNames[] = {"zenity", "kdialog"};
      for (const char *name : kNames)
         if (const char *hit = inPath(name))
            return hit;
      return nullptr;
   }();
   return found;
}

// One argument, quoted for /bin/sh. Everything that reaches here is a path or
// a title this plugin composed, but a preset folder is named by the user and
// ends up in the suggested filename, so it is quoted properly rather than
// hopefully.
std::string quote(const std::string &s) {
   std::string out = "'";
   for (const char c : s) {
      if (c == '\'')
         out += "'\\''";
      else
         out.push_back(c);
   }
   out += "'";
   return out;
}

std::string runChooser(const std::string &command) {
   FILE *pipe = popen(command.c_str(), "r");
   if (!pipe)
      return std::string();
   std::string out;
   char buf[512];
   while (std::fgets(buf, sizeof(buf), pipe))
      out += buf;
   pclose(pipe);
   while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
      out.pop_back();
   return out;
}

} // namespace

bool fileDialogAvailable() { return chooser() != nullptr; }

bool fileRevealAvailable() { return inPath("xdg-open") != nullptr; }

bool revealInFileBrowser(const std::string &path) {
   if (path.empty() || !inPath("xdg-open"))
      return false;
   // The folder rather than the file: xdg-open on a preset pack would hand it
   // to whatever claims the extension, which is not what "reveal" means.
   // Backgrounded, because a file manager started this way runs for as long as
   // the user keeps it open and nothing here is waiting for it.
   std::string dir = path;
   const size_t cut = dir.find_last_of('/');
   if (cut != std::string::npos)
      dir.erase(cut);
   if (dir.empty())
      dir = "/";
   const std::string command = "xdg-open " + quote(dir) + " >/dev/null 2>&1 &";
   return std::system(command.c_str()) == 0;
}

std::string openFileDialog(const std::string &title, const std::string &filterName,
                           const std::string &extension) {
   const char *tool = chooser();
   if (!tool)
      return std::string();
   if (std::strcmp(tool, "zenity") == 0)
      return runChooser("zenity --file-selection --title=" + quote(title) + " --file-filter=" +
                        quote(filterName + " | *." + extension) + " --file-filter=" +
                        quote("All files | *") + " 2>/dev/null");
   return runChooser("kdialog --title " + quote(title) + " --getopenfilename . " +
                     quote("*." + extension + "|" + filterName) + " 2>/dev/null");
}

std::string saveFileDialog(const std::string &title, const std::string &suggestedPath,
                           const std::string &filterName, const std::string &extension) {
   const char *tool = chooser();
   if (!tool)
      return std::string();
   if (std::strcmp(tool, "zenity") == 0)
      return runChooser("zenity --file-selection --save --confirm-overwrite --title=" +
                        quote(title) + " --filename=" + quote(suggestedPath) + " --file-filter=" +
                        quote(filterName + " | *." + extension) + " 2>/dev/null");
   return runChooser("kdialog --title " + quote(title) + " --getsavefilename " +
                     quote(suggestedPath) + " " + quote("*." + extension + "|" + filterName) +
                     " 2>/dev/null");
}

#endif

} // namespace verdalis
