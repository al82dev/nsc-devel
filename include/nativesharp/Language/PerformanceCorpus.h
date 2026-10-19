#pragma once

namespace nativesharp::language
{
enum class PerformanceCorpus
{
    GiantFile,
    GenericHeavy,
    MacroHeavy,
    ModuleFanout,
    AsyncFrames,
    PathologicalDiagnostics
};
}
