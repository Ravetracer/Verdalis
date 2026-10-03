#include "scene_preset.h"

#include <cstdlib>
#include <cstring>
#include <strings.h>

#include "verdalis/preset_library.h"

#include "verdaliscene.h"

namespace verdaliscene {

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

// A value as a preset writes it: an enum's name, or a number in the
// parameter's real-world unit.
bool parseValue(const ParamDesc &d, const std::string &value, double &raw) {
   if (d.kind == ParamKind::Enum) {
      for (uint32_t i = 0; i < d.enumCount; ++i) {
         if (strcasecmp(value.c_str(), d.enumNames[i]) == 0) {
            raw = static_cast<double>(i);
            return true;
         }
      }
   }
   char *end = nullptr;
   const double v = std::strtod(value.c_str(), &end);
   if (end == value.c_str())
      return false;
   raw = realToParam(d, v);
   return true;
}

int typeByName(const std::string &name) {
   for (int t = 0; t < kNumLayerTypes; ++t) {
      const LayerType &lt = layerType(t);
      if (strcasecmp(name.c_str(), lt.pluginName) == 0 || strcasecmp(name.c_str(), lt.label) == 0)
         return t;
   }
   return -1;
}

// formatPreset() of a context, minus the two header lines it always opens
// with: what is left is the module-grouped body.
std::string presetBody(const PresetContext &ctx, const PresetData &data) {
   std::string text = verdalis::formatPreset(ctx, data);
   for (int line = 0; line < 2; ++line) {
      const size_t nl = text.find('\n');
      text = nl == std::string::npos ? std::string() : text.substr(nl + 1);
   }
   // The body opens with the blank line before its first heading.
   while (!text.empty() && text[0] == '\n')
      text.erase(text.begin());
   return text;
}

// A section of the file still being read.
struct PendingLayer {
   int type = -1;
   std::string from;
   std::string preset;
   std::string body; // the plugin's own lines
   std::vector<std::pair<uint32_t, double>> mix;
};

void finishLayer(PendingLayer &p, Scene &out) {
   if (p.type < 0)
      return;
   SceneLayer layer = defaultLayer(p.type);
   const LayerType &t = layerType(p.type);
   std::string error;
   if (!p.from.empty()) {
      const std::string text = findLayerPresetText(p.type, p.from);
      if (text.empty())
         out.warnings.push_back(std::string(t.pluginName) + " has no preset called \"" + p.from +
                                "\"");
      else if (!applyLayerPreset(p.type, text, layer, error))
         out.warnings.push_back(std::string(t.pluginName) + " \"" + p.from + "\": " + error);
      layer.presetName = p.from;
   }
   if (!p.body.empty()) {
      PresetData data;
      // The shared parser wants at least one line it recognises; the name is
      // one, and it is the layer's anyway.
      const std::string text = "name = x\n" + p.body;
      if (verdalis::parsePreset(t.library().ctx, text.c_str(), text.size(), data, error)) {
         for (const auto &kv : data.values)
            if (kv.first < layer.values.size())
               layer.values[kv.first] = kv.second;
      } else {
         out.warnings.push_back(std::string(t.pluginName) + " section: " + error);
      }
   }
   for (const auto &kv : p.mix)
      layer.mix[kv.first] = kv.second;
   if (!p.preset.empty())
      layer.presetName = p.preset;
   out.layers.push_back(std::move(layer));
   p = PendingLayer{};
}

} // namespace

Scene emptyScene() {
   Scene s;
   const ParamDesc *table = sceneParamTable();
   for (uint32_t i = 0; i < kNumSceneParams; ++i)
      s.values[i] = table[i].def;
   return s;
}

SceneLayer defaultLayer(int type) {
   SceneLayer layer;
   layer.type = type;
   const LayerType &t = layerType(type);
   const ParamDesc *table = t.paramTable();
   layer.values.resize(t.paramCount);
   for (uint32_t i = 0; i < t.paramCount; ++i)
      layer.values[i] = table[i].def;
   const ParamDesc *mix = slotParamTable();
   for (uint32_t p = 0; p < kNumSlotParams; ++p)
      layer.mix[p] = mix[p].def;
   return layer;
}

SceneLayer freshLayer(int type) {
   SceneLayer layer = defaultLayer(type);
   const LayerType &t = layerType(type);
   if (t.startPreset && t.startPreset[0]) {
      const std::string text = findLayerPresetText(type, t.startPreset);
      std::string error;
      if (!text.empty() && applyLayerPreset(type, text, layer, error))
         layer.presetName = t.startPreset;
   }
   for (int i = 0; i < t.sceneDefaultCount; ++i)
      if (t.sceneDefaults[i].id < layer.values.size())
         layer.values[t.sceneDefaults[i].id] = t.sceneDefaults[i].raw;
   layer.mix[kSlotLevel] = t.startLevelDb;
   return layer;
}

bool parseScene(const char *text, size_t length, Scene &out, std::string &error) {
   if (!text) {
      error = "no preset data";
      return false;
   }
   out = emptyScene();
   PendingLayer pending;
   bool inSection = false; // past the first `layer =`, known or not
   bool sawAnything = false;
   size_t pos = 0;
   uint32_t lineNo = 0;

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
         error = "line " + std::to_string(lineNo) + ": expected 'key = value'";
         return false;
      }
      std::string key = line.substr(0, eq);
      std::string value = line.substr(eq + 1);
      trim(key);
      trim(value);
      if (key.empty())
         continue;
      sawAnything = true;

      if (key == "layer") {
         finishLayer(pending, out);
         inSection = true;
         pending.type = typeByName(value);
         if (pending.type < 0)
            out.warnings.push_back("line " + std::to_string(lineNo) + ": no layer called \"" +
                                   value + "\"");
         continue;
      }

      if (pending.type >= 0) {
         if (key == "layer_from") {
            pending.from = value;
         } else if (key == "layer_preset") {
            pending.preset = value;
         } else if (const ParamDesc *d = slotParamByKey(key.c_str())) {
            double raw = 0.0;
            if (d->id != kSlotActive && parseValue(*d, value, raw))
               pending.mix.emplace_back(d->id, raw);
         } else {
            pending.body += line;
            pending.body += '\n';
         }
         continue;
      }
      // A section whose layer is unknown swallows its lines rather than
      // letting them land on the scene's parameters.
      if (inSection)
         continue;

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
      } else if (const ParamDesc *d = sceneParamByKey(key.c_str())) {
         double raw = 0.0;
         if (parseValue(*d, value, raw))
            out.values[d->id] = raw;
      }
      // Anything else -- "format", a key from a later version -- is ignored,
      // so newer scenes stay loadable.
   }
   finishLayer(pending, out);

   if (!sawAnything) {
      error = "preset is empty";
      return false;
   }
   return true;
}

std::string formatScene(const Scene &scene) {
   PresetData head;
   head.name = scene.name;
   head.author = scene.author;
   head.description = scene.description;
   head.features = scene.features;
   for (uint32_t i = 0; i < kNumSceneParams; ++i)
      head.values.emplace_back(i, scene.values[i]);
   std::string out = verdalis::formatPreset(presetContext(), head);

   const PresetContext mixCtx{kPluginName, kPresetExtension, slotParamTable(), kNumSlotParams};
   int count[kNumLayerTypes] = {};
   for (const SceneLayer &layer : scene.layers) {
      const LayerType &t = layerType(layer.type);
      out += "\n# ";
      out += t.label;
      out += " ";
      out += std::to_string(++count[layer.type]);
      out += "\nlayer = ";
      out += t.pluginName;
      out += "\n";
      if (!layer.presetName.empty())
         out += "layer_preset = " + layer.presetName + "\n";

      PresetData mix;
      for (uint32_t p = kSlotLevel; p < kNumSlotParams; ++p) {
         if (p == kSlotShotRate && t.shotLevelParam == kNoLayerParam)
            continue;
         mix.values.emplace_back(p, layer.mix[p]);
      }
      std::string mixBody = presetBody(mixCtx, mix);
      // The placement keys sit directly under `layer =`, without a heading of
      // their own: they belong to the section line above them.
      const size_t nl = mixBody.find('\n');
      if (nl != std::string::npos && mixBody[0] == '#')
         mixBody = mixBody.substr(nl + 1);
      out += mixBody;

      PresetData own;
      for (uint32_t i = 0; i < layer.values.size(); ++i)
         own.values.emplace_back(i, layer.values[i]);
      out += "\n";
      out += presetBody(t.library().ctx, own);
   }
   return out;
}

bool applyLayerPreset(int type, const std::string &text, SceneLayer &layer, std::string &error) {
   const LayerType &t = layerType(type);
   PresetData data;
   if (!verdalis::parsePreset(t.library().ctx, text.c_str(), text.size(), data, error))
      return false;
   const ParamDesc *table = t.paramTable();
   layer.values.assign(t.paramCount, 0.0);
   for (uint32_t i = 0; i < t.paramCount; ++i)
      layer.values[i] = table[i].def;
   for (const auto &kv : data.values)
      if (kv.first < layer.values.size())
         layer.values[kv.first] = kv.second;
   if (!data.name.empty())
      layer.presetName = data.name;
   return true;
}

std::string findLayerPresetText(int type, const std::string &name) {
   const LayerType &t = layerType(type);
   const verdalis::PresetLibrarySpec lib = t.library();
   for (unsigned i = 0; i < lib.builtinCount; ++i) {
      PresetData data;
      std::string error;
      const char *text = lib.builtins[i].text;
      if (verdalis::parsePreset(lib.ctx, text, std::strlen(text), data, error) &&
          strcasecmp(data.name.c_str(), name.c_str()) == 0)
         return text;
   }
   for (const GuiPreset &p : verdalis::scanPresetLibrary(lib)) {
      if (p.path.empty() || strcasecmp(p.name.c_str(), name.c_str()) != 0)
         continue;
      PresetData data;
      std::string error;
      if (!verdalis::parsePresetFile(lib.ctx, p.path, data, error))
         continue;
      std::string text = verdalis::formatPreset(lib.ctx, data);
      return text;
   }
   return {};
}

std::string formatLayerPreset(const SceneLayer &layer, const std::string &name) {
   const LayerType &t = layerType(layer.type);
   PresetData data;
   data.name = name;
   for (uint32_t i = 0; i < layer.values.size(); ++i)
      data.values.emplace_back(i, layer.values[i]);
   return verdalis::formatPreset(t.library().ctx, data);
}

} // namespace verdaliscene
