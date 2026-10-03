#pragma once

// How the parameters reach the engine: the real-world value of every parameter
// turned into the engine's own parameter block. Kept out of plugin.cpp so that
// anything else driving a NightEngine -- VerdaliScene runs one per layer -- turns
// the same parameters into the same sound.

#include "dsp/night_engine.h"
#include "params.h"

namespace nightlife {

// `real` holds paramToReal() of every parameter's host value, modulation
// included, indexed by ParamId.
EngineParams engineParams(const double *real);

} // namespace nightlife
