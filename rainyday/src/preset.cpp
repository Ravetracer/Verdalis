#include "rainyday.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>

#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>

#include "params.h"

namespace rainyday {

namespace {

void trim(std::string &s) {
   size_t b = 0;
   while (b < s.size() && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r' || s[b] == '\n'))
      ++b;
   size_t e = s.size();
   while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' || s[e - 1] == '\n'))
      --e;
   s = s.substr(b, e - b);
}

bool isDirectory(const std::string &path) {
   struct stat st{};
   return !path.empty() && stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

// Anchor symbol: its address is inside this shared object, which lets dladdr
// report the path the host actually loaded.
void dsoAnchor() {}

} // namespace

std::string factoryPresetDir() {
   Dl_info info{};
   if (dladdr(reinterpret_cast<const void *>(&dsoAnchor), &info) == 0 || !info.dli_fname)
      return {};

   std::string path(info.dli_fname);
   const size_t slash = path.rfind('/');
   if (slash == std::string::npos)
      return {};
   const std::string dir = path.substr(0, slash);

   // Installed layout is <dir>/RainyDay.clap + <dir>/presets, but a bundle-like
   // layout is also checked so a build tree works too.
   const std::string candidates[] = {dir + "/presets", dir + "/../presets",
                                     dir + "/RainyDay.clap/presets"};
   for (const auto &c : candidates) {
      if (isDirectory(c))
         return c;
   }
   return {};
}

std::string userPresetDir() {
   const char *xdg = std::getenv("XDG_CONFIG_HOME");
   if (xdg && xdg[0] == '/')
      return std::string(xdg) + "/RainyDay/presets";
   const char *home = std::getenv("HOME");
   if (home && home[0] == '/')
      return std::string(home) + "/.config/RainyDay/presets";
   return {};
}

bool parsePreset(const char *text, size_t length, PresetData &out, std::string &error) {
   if (!text) {
      error = "no preset data";
      return false;
   }

   out = PresetData{};
   size_t pos = 0;
   uint32_t lineNo = 0;
   bool sawAnything = false;

   while (pos < length) {
      size_t end = pos;
      while (end < length && text[end] != '\n')
         ++end;
      std::string line(text + pos, end - pos);
      pos = end + 1;
      ++lineNo;

      trim(line);
      if (line.empty() || line[0] == '#' || line[0] == ';')
         continue;

      const size_t eq = line.find('=');
      if (eq == std::string::npos) {
         char buf[128];
         std::snprintf(buf, sizeof(buf), "line %u: expected 'key = value'", lineNo);
         error = buf;
         return false;
      }

      std::string key = line.substr(0, eq);
      std::string value = line.substr(eq + 1);
      trim(key);
      trim(value);
      if (key.empty())
         continue;

      sawAnything = true;

      if (key == "name") {
         out.name = value;
      } else if (key == "author" || key == "creator") {
         out.author = value;
      } else if (key == "description") {
         out.description = value;
      } else if (key == "features" || key == "tags") {
         size_t start = 0;
         while (start <= value.size()) {
            const size_t comma = value.find(',', start);
            std::string f = value.substr(start, comma == std::string::npos ? std::string::npos
                                                                           : comma - start);
            trim(f);
            if (!f.empty())
               out.features.push_back(f);
            if (comma == std::string::npos)
               break;
            start = comma + 1;
         }
      } else if (key == "format" || key == "rainyday_version") {
         // Reserved for future format revisions; version 1 is the only one.
      } else {
         const ParamDesc *desc = paramByKey(key.c_str());
         if (!desc)
            continue; // Unknown keys are ignored so newer presets stay loadable.

         double raw = 0.0;
         bool parsed = false;
         if (desc->kind == ParamKind::Enum) {
            for (uint32_t i = 0; i < desc->enumCount; ++i) {
               if (strcasecmp(value.c_str(), desc->enumNames[i]) == 0) {
                  raw = static_cast<double>(i);
                  parsed = true;
                  break;
               }
            }
         }
         if (!parsed) {
            char *endp = nullptr;
            const double v = std::strtod(value.c_str(), &endp);
            if (endp == value.c_str())
               continue; // not a number and not a known enum name
            // Preset files store real-world units (Hz, ms, dB, 0..1 ratios).
            raw = realToParam(*desc, v);
         }
         out.values.emplace_back(desc->id, raw);
      }
   }

   if (!sawAnything) {
      error = "preset is empty";
      return false;
   }
   return true;
}

bool parsePresetFile(const std::string &path, PresetData &out, std::string &error) {
   FILE *f = std::fopen(path.c_str(), "rb");
   if (!f) {
      error = std::string("cannot open '") + path + "': " + std::strerror(errno);
      return false;
   }
   std::string buffer;
   char chunk[4096];
   size_t n;
   while ((n = std::fread(chunk, 1, sizeof(chunk), f)) > 0)
      buffer.append(chunk, n);
   const bool readError = std::ferror(f) != 0;
   std::fclose(f);
   if (readError) {
      error = std::string("read error on '") + path + "'";
      return false;
   }
   if (!parsePreset(buffer.data(), buffer.size(), out, error)) {
      error = path + ": " + error;
      return false;
   }
   if (out.name.empty()) {
      // Fall back to the file name without extension.
      const size_t slash = path.rfind('/');
      const std::string base = slash == std::string::npos ? path : path.substr(slash + 1);
      const size_t dot = base.rfind('.');
      out.name = dot == std::string::npos ? base : base.substr(0, dot);
   }
   return true;
}

} // namespace rainyday
