#pragma once

#include "nativesharp/Frontend/ComptimeDiagnosticSeverity.h"

#include <cstddef>
#include <string>
#include <vector>

namespace nativesharp::frontend
{
struct ComptimeAuthoredDiagnostic final
{
    ComptimeDiagnosticSeverity severity = ComptimeDiagnosticSeverity::Error;
    std::string code;
    std::string message;
    std::size_t offset = 0;
    std::size_t length = 0;
    std::vector<std::string> notes;
};
}
