#pragma once

#include "nativesharp/Compilation/CompilationInputManifest.h"

namespace nativesharp::driver
{
compilation::CompilationInputManifest createCompilationInputManifest(
    compilation::ProjectGraph graph,
    compilation::ProjectLoadOptions loadOptions);
}
