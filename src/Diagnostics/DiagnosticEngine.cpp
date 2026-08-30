#include "nativesharp/Diagnostics/DiagnosticEngine.h"

#include <utility>

namespace nativesharp::diagnostics
{
void DiagnosticEngine::report(
    DiagnosticId id,
    DiagnosticSeverity severity,
    SourceLocation location,
    std::string message)
{
    diagnostics_.emplace_back(id, severity, location, std::nullopt, std::move(message));

    if (severity == DiagnosticSeverity::Error)
    {
        ++errorCount_;
    }
    else if (severity == DiagnosticSeverity::Warning)
    {
        ++warningCount_;
    }
}

void DiagnosticEngine::report(
    DiagnosticId id,
    DiagnosticSeverity severity,
    SourceRange range,
    std::string message)
{
    diagnostics_.emplace_back(id, severity, range.begin(), range, std::move(message));

    if (severity == DiagnosticSeverity::Error)
    {
        ++errorCount_;
    }
    else if (severity == DiagnosticSeverity::Warning)
    {
        ++warningCount_;
    }
}

const std::vector<Diagnostic>& DiagnosticEngine::diagnostics() const noexcept
{
    return diagnostics_;
}

std::size_t DiagnosticEngine::errorCount() const noexcept
{
    return errorCount_;
}

std::size_t DiagnosticEngine::warningCount() const noexcept
{
    return warningCount_;
}

bool DiagnosticEngine::hasErrors() const noexcept
{
    return errorCount_ != 0;
}

void DiagnosticEngine::clear()
{
    diagnostics_.clear();
    errorCount_ = 0;
    warningCount_ = 0;
}
}
