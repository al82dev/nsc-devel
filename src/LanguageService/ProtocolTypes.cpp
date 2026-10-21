#include "nativesharp/LanguageService/ProtocolTypes.h"

#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "Internal/DecodedCodePoint.h"

#include <algorithm>

namespace nativesharp::language_service
{
bool MemberCompletionResult::isIncomplete() const noexcept
{
    return status != MemberCompletionStatus::Complete;
}

namespace
{
DecodedCodePoint decodeUtf8(std::string_view text, std::size_t offset) noexcept
{
    const auto byte = [&](std::size_t index) {
        return static_cast<unsigned char>(text[index]);
    };
    const unsigned char first = byte(offset);
    if (first < 0x80U) return {first, 1};

    std::size_t width = 0;
    std::uint32_t value = 0;
    std::uint32_t minimum = 0;
    if ((first & 0xE0U) == 0xC0U)
    {
        width = 2;
        value = first & 0x1FU;
        minimum = 0x80U;
    }
    else if ((first & 0xF0U) == 0xE0U)
    {
        width = 3;
        value = first & 0x0FU;
        minimum = 0x800U;
    }
    else if ((first & 0xF8U) == 0xF0U)
    {
        width = 4;
        value = first & 0x07U;
        minimum = 0x10000U;
    }
    else
    {
        return {};
    }
    if (offset + width > text.size()) return {};
    for (std::size_t index = 1; index < width; ++index)
    {
        const unsigned char continuation = byte(offset + index);
        if ((continuation & 0xC0U) != 0x80U) return {};
        value = (value << 6U) | (continuation & 0x3FU);
    }
    if (value < minimum || value > 0x10FFFFU || (value >= 0xD800U && value <= 0xDFFFU))
    {
        return {};
    }
    return {value, width};
}

std::uint32_t utf16Width(std::uint32_t codePoint) noexcept
{
    return codePoint > 0xFFFFU ? 2U : 1U;
}
}

std::optional<TextPosition> utf16PositionForOffset(
    std::string_view utf8,
    std::size_t byteOffset) noexcept
{
    if (byteOffset > utf8.size()) return std::nullopt;
    TextPosition result;
    std::size_t offset = 0;
    while (offset < byteOffset)
    {
        if (utf8[offset] == '\r')
        {
            ++offset;
            if (offset < byteOffset && offset < utf8.size() && utf8[offset] == '\n') ++offset;
            ++result.line;
            result.character = 0;
            continue;
        }
        if (utf8[offset] == '\n')
        {
            ++offset;
            ++result.line;
            result.character = 0;
            continue;
        }
        const DecodedCodePoint decoded = decodeUtf8(utf8, offset);
        if (offset + decoded.bytes > byteOffset) return std::nullopt;
        offset += decoded.bytes;
        result.character += utf16Width(decoded.value);
    }
    return result;
}

std::optional<std::size_t> byteOffsetForUtf16Position(
    std::string_view utf8,
    TextPosition position) noexcept
{
    TextPosition current;
    std::size_t offset = 0;
    while (true)
    {
        if (current.line == position.line && current.character == position.character)
        {
            return offset;
        }
        if (offset >= utf8.size()) return std::nullopt;
        if (utf8[offset] == '\r' || utf8[offset] == '\n')
        {
            if (current.line == position.line) return std::nullopt;
            if (utf8[offset] == '\r' && offset + 1 < utf8.size() && utf8[offset + 1] == '\n')
            {
                offset += 2;
            }
            else
            {
                ++offset;
            }
            ++current.line;
            current.character = 0;
            continue;
        }
        if (current.line > position.line) return std::nullopt;
        const DecodedCodePoint decoded = decodeUtf8(utf8, offset);
        const std::uint32_t width = utf16Width(decoded.value);
        if (current.line == position.line && current.character + width > position.character)
        {
            return std::nullopt;
        }
        offset += decoded.bytes;
        current.character += width;
    }
}

std::optional<LanguageDiagnostic> makeLanguageDiagnostic(
    const diagnostics::Diagnostic& diagnostic,
    const source::SourceManager& sourceManager)
{
    const source::SourceFile* file = sourceManager.getFile(diagnostic.location().fileId());
    if (file == nullptr) return std::nullopt;

    const SourceRange nativeRange = diagnostic.range().value_or(
        SourceRange(diagnostic.location(), diagnostic.location()));
    const auto start = utf16PositionForOffset(file->buffer(), nativeRange.begin().offset());
    const auto end = utf16PositionForOffset(file->buffer(), nativeRange.end().offset());
    if (!start.has_value() || !end.has_value()) return std::nullopt;

    LanguageDiagnosticSeverity severity = LanguageDiagnosticSeverity::Error;
    if (diagnostic.severity() == diagnostics::DiagnosticSeverity::Note)
        severity = LanguageDiagnosticSeverity::Note;
    else if (diagnostic.severity() == diagnostics::DiagnosticSeverity::Warning)
        severity = LanguageDiagnosticSeverity::Warning;

    return LanguageDiagnostic{
        file->path(),
        TextRange{*start, *end},
        severity,
        std::string(diagnostics::diagnosticIdName(diagnostic.id())),
        diagnostic.message()};
}
}
