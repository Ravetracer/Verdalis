// The CLAP entry point, split in two so that more than one plugin format can be
// built from one implementation.
//
// Everything CrackleBlaze is lives in a static library, which exports the three
// functions below instead of a clap_plugin_entry. Each format's module then
// compiles the one small translation unit that builds the entry structure from
// them: entry.cpp for the .clap, and the clap-wrapper's own export shim for the
// .vst3. Neither format duplicates a line of the plugin itself.

#pragma once

#include <clap/clap.h>

extern "C" {

bool crackleblazeEntryInit(const char *pluginPath);
void crackleblazeEntryDeinit();
const void *crackleblazeEntryGetFactory(const char *factoryId);
}
