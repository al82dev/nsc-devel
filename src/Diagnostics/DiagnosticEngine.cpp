#include "nativesharp/Diagnostics/DiagnosticEngine.h"

#include <algorithm>
#include <cstdint>
#include <tuple>
#include <utility>
#include <vector>

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

void DiagnosticEngine::append(const Diagnostic& diagnostic)
{
    if (diagnostic.range().has_value())
    {
        report(
            diagnostic.id(),
            diagnostic.severity(),
            *diagnostic.range(),
            diagnostic.message());
        return;
    }
    report(
        diagnostic.id(),
        diagnostic.severity(),
        diagnostic.location(),
        diagnostic.message());
}

void DiagnosticEngine::mergeDeterministically(std::span<const DiagnosticEngine> shards)
{
    struct Entry final
    {
        const Diagnostic* diagnostic = nullptr;
    };

    std::vector<Entry> entries;
    std::size_t suppressedByShards = 0;
    for (const DiagnosticEngine& shard : shards)
    {
        entries.reserve(entries.size() + shard.diagnostics().size());
        for (const Diagnostic& diagnostic : shard.diagnostics())
            entries.push_back({&diagnostic});
        suppressedByShards += shard.suppressedDiagnosticCount();
    }

    const auto severityOrder = [](DiagnosticSeverity severity) noexcept {
        switch (severity)
        {
            case DiagnosticSeverity::Error: return 0;
            case DiagnosticSeverity::Warning: return 1;
            case DiagnosticSeverity::Note: return 2;
        }
        return 3;
    };
    std::sort(entries.begin(), entries.end(), [&](const Entry& left, const Entry& right) {
        const Diagnostic& a = *left.diagnostic;
        const Diagnostic& b = *right.diagnostic;
        const std::uint32_t aFile = a.location().fileId().value();
        const std::uint32_t bFile = b.location().fileId().value();
        const std::uint32_t aEnd = a.range().has_value()
            ? a.range()->end().offset()
            : a.location().offset();
        const std::uint32_t bEnd = b.range().has_value()
            ? b.range()->end().offset()
            : b.location().offset();
        return std::tuple(
                   a.location().isValid() ? 0 : 1,
                   aFile,
                   a.location().offset(),
                   aEnd,
                   severityOrder(a.severity()),
                   static_cast<int>(a.id()),
                   std::string_view(a.message())) <
            std::tuple(
                   b.location().isValid() ? 0 : 1,
                   bFile,
                   b.location().offset(),
                   bEnd,
                   severityOrder(b.severity()),
                   static_cast<int>(b.id()),
                   std::string_view(b.message()));
    });
    for (const Entry& entry : entries) append(*entry.diagnostic);
    suppressedDiagnosticCount_ += suppressedByShards;
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
