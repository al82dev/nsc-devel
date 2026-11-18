#pragma once

#include <string>

namespace nativesharp::driver
{
namespace
{
struct CompilationTimeTracePhase final
{
    std::string name;
    long long durationMicroseconds = 0;
    // EN: -1 when the timing line carried no memory measurement (written as JSON null).
    // ES: -1 cuando la linea de tiempos no traia medicion de memoria (se escribe null en JSON).
    long long peakWorkingSetBytes = -1;
};
} // namespace
} // namespace nativesharp::driver
