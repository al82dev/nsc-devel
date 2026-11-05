#pragma once

#include "nativesharp/Basic/SourceLocation.h"
#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/Diagnostics/DiagnosticLabel.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"

#include <optional>
#include <filesystem>
#include <cstdint>
#include <string>
#include <vector>

namespace nativesharp::diagnostics
{
// Coordinates supplied by an external compiler, retained independently of
// NativeSharp's SourceManager. Columns are the external compiler's units.
struct ExternalSourceLocation final
{
    std::filesystem::path file;
    std::uint32_t line = 0;
    std::uint32_t column = 0;
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
        std::vector<std::string> notes = {},
        std::optional<ExternalSourceLocation> externalLocation = std::nullopt);

    DiagnosticId id() const noexcept;
    DiagnosticSeverity severity() const noexcept;
    SourceLocation location() const noexcept;
    const std::optional<SourceRange>& range() const noexcept;
    const std::string& message() const noexcept;
    const std::vector<DiagnosticLabel>& secondaryLabels() const noexcept;
    const std::vector<std::string>& notes() const noexcept;
    const std::optional<ExternalSourceLocation>& externalLocation() const noexcept;

private:
    DiagnosticId id_;
    DiagnosticSeverity severity_;
    SourceLocation location_;
    std::optional<SourceRange> range_;
    std::string message_;
    std::vector<DiagnosticLabel> secondaryLabels_;
    std::vector<std::string> notes_;
    std::optional<ExternalSourceLocation> externalLocation_;
};
}
