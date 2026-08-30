#pragma once

#include <string_view>

namespace nativesharp::diagnostics
{
enum class DiagnosticSeverity
{
    Note,
    Warning,
    Error
};

constexpr std::string_view diagnosticSeverityName(DiagnosticSeverity severity) noexcept
{
    switch (severity)
    {
        case DiagnosticSeverity::Note:
            return "note";
        case DiagnosticSeverity::Warning:
            return "warning";
        case DiagnosticSeverity::Error:
            return "error";
    }

    return "error";
}
}
