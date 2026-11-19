// Writes Aurum's factory presets out as CLAP state documents, one file per
// preset, so ap-fxhost can load them with --state. Built against Aurum's own
// FactoryPresets.cpp / StateIO.cpp / Params.cpp by render-ap-demos.sh.
//
//   aurum-preset-dump <outdir>     -> <outdir>/<Preset Name>.aurumstate
#include <cstdio>
#include <fstream>
#include <string>

#include "state/PresetManager.h"
#include "state/StateIO.h"

int main(int argc, char **argv) {
   if (argc < 2) {
      std::fprintf(stderr, "usage: aurum-preset-dump <outdir>\n");
      return 2;
   }
   for (const auto &p : aurum::factoryPresets()) {
      std::string name = p.first.substr(p.first.rfind('/') + 1);
      std::ofstream(std::string(argv[1]) + "/" + name + ".aurumstate") << aurum::serializeState(p.second);
      std::printf("%s\n", p.first.c_str());
   }
   return 0;
}
