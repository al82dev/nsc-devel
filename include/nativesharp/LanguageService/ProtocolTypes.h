#pragma once

#include "nativesharp/Diagnostics/Diagnostic.h"
#include "nativesharp/Source/SourceManager.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace nativesharp::language_service
{
struct TextPosition final
{
    std::uint32_t line = 0;
    std::uint32_t character = 0;

    friend constexpr bool operator==(TextPosition, TextPosition) noexcept = default;
};

struct TextRange final
{
    TextPosition start;
    TextPosition end;

    friend constexpr bool operator==(TextRange, TextRange) noexcept = default;
};

enum class LanguageDiagnosticSeverity
{
    Note,
    Warning,
    Error
};

struct LanguageDiagnostic final
{
    std::filesystem::path path;
    TextRange range;
    LanguageDiagnosticSeverity severity = LanguageDiagnosticSeverity::Error;
    std::string code;
    std::string message;
};

std::optional<TextPosition> utf16PositionForOffset(
    std::string_view utf8,
    std::size_t byteOffset) noexcept;
std::optional<std::size_t> byteOffsetForUtf16Position(
    std::string_view utf8,
    TextPosition position) noexcept;
std::optional<LanguageDiagnostic> makeLanguageDiagnostic(
    const diagnostics::Diagnostic& diagnostic,
    const source::SourceManager& sourceManager);
}
