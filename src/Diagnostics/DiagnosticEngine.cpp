#include "nativesharp/Diagnostics/DiagnosticEngine.h"

#include <utility>

namespace nativesharp::diagnostics
{
DiagnosticEngine::DiagnosticEngine(std::size_t errorLimit) noexcept
    : errorLimit_(errorLimit == 0 ? 1 : errorLimit)
{
}

void DiagnosticEngine::report(
    DiagnosticId id,
    DiagnosticSeverity severity,
    SourceLocation location,
    std::string message)
{
    if (suppress(severity))
    {
        return;
    }

    diagnostics_.emplace_back(id, severity, location, std::nullopt, std::move(message));

    if (severity == DiagnosticSeverity::Error)
    {
        ++errorCount_;
        errorLimitReached_ = errorCount_ >= errorLimit_;
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
    if (suppress(severity))
    {
        return;
    }

    diagnostics_.emplace_back(id, severity, range.begin(), range, std::move(message));

    if (severity == DiagnosticSeverity::Error)
    {
        ++errorCount_;
        errorLimitReached_ = errorCount_ >= errorLimit_;
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

std::size_t DiagnosticEngine::errorLimit() const noexcept
{
    return errorLimit_;
}

bool DiagnosticEngine::errorLimitReached() const noexcept
{
    return errorLimitReached_;
}

std::size_t DiagnosticEngine::suppressedDiagnosticCount() const noexcept
{
    return suppressedDiagnosticCount_;
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
    suppressedDiagnosticCount_ = 0;
    errorLimitReached_ = false;
}

bool DiagnosticEngine::suppress(DiagnosticSeverity) noexcept
{
    if (!errorLimitReached_)
    {
        return false;
    }

    ++suppressedDiagnosticCount_;
    return true;
}
}
