#pragma once

#include "nativesharp/Basic/SourceLocation.h"
#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"

#include <optional>
#include <string>

namespace nativesharp::diagnostics
{
class Diagnostic final
{
public:
    Diagnostic(
        DiagnosticId id,
        DiagnosticSeverity severity,
        SourceLocation location,
        std::optional<SourceRange> range,
        std::string message);

    DiagnosticId id() const noexcept;
    DiagnosticSeverity severity() const noexcept;
    SourceLocation location() const noexcept;
    const std::optional<SourceRange>& range() const noexcept;
    const std::string& message() const noexcept;

private:
    DiagnosticId id_;
    DiagnosticSeverity severity_;
    SourceLocation location_;
    std::optional<SourceRange> range_;
    std::string message_;
};
}
