#pragma once

#include "nativesharp/Basic/SourceRange.h"

#include <string>

namespace nativesharp::diagnostics
{
struct DiagnosticLabel final
{
    SourceRange range;
    std::string message;
};
}
