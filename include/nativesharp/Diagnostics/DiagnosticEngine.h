#pragma once

#include "nativesharp/Basic/SourceLocation.h"
#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/Diagnostics/Diagnostic.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace nativesharp::diagnostics
{
class DiagnosticEngine final
{
public:
    void report(
        DiagnosticId id,
        DiagnosticSeverity severity,
        SourceLocation location,
        std::string message);

    void report(
        DiagnosticId id,
        DiagnosticSeverity severity,
        SourceRange range,
        std::string message);

    const std::vector<Diagnostic>& diagnostics() const noexcept;
    std::size_t errorCount() const noexcept;
    std::size_t warningCount() const noexcept;
    bool hasErrors() const noexcept;
    void clear();

private:
    std::vector<Diagnostic> diagnostics_;
    std::size_t errorCount_ = 0;
    std::size_t warningCount_ = 0;
};
}
