#pragma once

#include "nativesharp/Basic/SourceLocation.h"
#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"

#include <optional>
#include <string>
#include <vector>

namespace nativesharp::diagnostics
{
struct DiagnosticLabel final
{
    SourceRange range;
    std::string message;
};

class Diagnostic final
{
public:
    Diagnostic(
        DiagnosticId id,
        DiagnosticSeverity severity,
        SourceLocation location,
        std::optional<SourceRange> range,
        std::string message,
        std::vector<DiagnosticLabel> secondaryLabels = {},
        std::vector<std::string> notes = {});

    DiagnosticId id() const noexcept;
    DiagnosticSeverity severity() const noexcept;
    SourceLocation location() const noexcept;
    const std::optional<SourceRange>& range() const noexcept;
    const std::string& message() const noexcept;
    const std::vector<DiagnosticLabel>& secondaryLabels() const noexcept;
    const std::vector<std::string>& notes() const noexcept;

private:
    DiagnosticId id_;
    DiagnosticSeverity severity_;
    SourceLocation location_;
    std::optional<SourceRange> range_;
    std::string message_;
    std::vector<DiagnosticLabel> secondaryLabels_;
    std::vector<std::string> notes_;
};
}
