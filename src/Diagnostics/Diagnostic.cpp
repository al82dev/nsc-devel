#include "nativesharp/Diagnostics/Diagnostic.h"

#include <utility>

namespace nativesharp::diagnostics
{
Diagnostic::Diagnostic(
    DiagnosticId id,
    DiagnosticSeverity severity,
    SourceLocation location,
    std::optional<SourceRange> range,
    std::string message,
    std::vector<DiagnosticLabel> secondaryLabels,
    std::vector<std::string> notes)
    : id_(id),
      severity_(severity),
      location_(location),
      range_(range),
      message_(std::move(message)),
      secondaryLabels_(std::move(secondaryLabels)),
      notes_(std::move(notes))
{
}

DiagnosticId Diagnostic::id() const noexcept
{
    return id_;
}

DiagnosticSeverity Diagnostic::severity() const noexcept
{
    return severity_;
}

SourceLocation Diagnostic::location() const noexcept
{
    return location_;
}

const std::optional<SourceRange>& Diagnostic::range() const noexcept
{
    return range_;
}

const std::string& Diagnostic::message() const noexcept
{
    return message_;
}

const std::vector<DiagnosticLabel>& Diagnostic::secondaryLabels() const noexcept
{
    return secondaryLabels_;
}

const std::vector<std::string>& Diagnostic::notes() const noexcept
{
    return notes_;
}
}
