#pragma once

#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/Interop/CppSessionDiagnostic.h"

#include <functional>
#include <optional>
#include <span>

namespace nativesharp::diagnostics { class DiagnosticEngine; }

namespace nativesharp::frontend
{
// A resolver must only return a range for the same source content Clang saw.
using CppDiagnosticLocationResolver =
    std::function<std::optional<SourceRange>(const interop::CppSourcePoint&)>;

void appendCppDiagnostics(
    std::span<const interop::CppSessionDiagnostic> source,
    diagnostics::DiagnosticEngine& destination,
    const CppDiagnosticLocationResolver& resolve = {});
}
