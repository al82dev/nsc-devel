#include "nativesharp/LanguageService/ProtocolTypes.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

namespace
{
int failures = 0;

void expect(bool condition, std::string_view message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}
}

int main()
{
    using namespace nativesharp;
    using namespace nativesharp::language_service;
    const std::string text = "a\r\n\xC3\xA9\xE6\x97\xA5\xF0\x9F\x98\x80z";

    expect(utf16PositionForOffset(text, 0) == TextPosition{0, 0}, "file start");
    expect(utf16PositionForOffset(text, 3) == TextPosition{1, 0}, "CRLF starts next line");
    expect(utf16PositionForOffset(text, 5) == TextPosition{1, 1}, "accent is one UTF-16 unit");
    expect(utf16PositionForOffset(text, 8) == TextPosition{1, 2}, "Japanese character is one unit");
    expect(utf16PositionForOffset(text, 12) == TextPosition{1, 4}, "emoji is a surrogate pair");
    expect(utf16PositionForOffset(text, 13) == TextPosition{1, 5}, "ASCII follows surrogate pair");
    expect(!utf16PositionForOffset(text, 4).has_value(), "middle of UTF-8 sequence is rejected");

    expect(byteOffsetForUtf16Position(text, {1, 0}) == 3, "line start converts to byte offset");
    expect(byteOffsetForUtf16Position(text, {1, 2}) == 8, "Unicode position converts to offset");
    expect(byteOffsetForUtf16Position(text, {1, 4}) == 12, "surrogate pair converts to offset");
    expect(!byteOffsetForUtf16Position(text, {1, 3}).has_value(), "split surrogate is rejected");

    source::SourceManager sources;
    const FileId file = sources.addBuffer(std::filesystem::path("Unicode.ns"), text);
    diagnostics::Diagnostic diagnostic(
        diagnostics::DiagnosticId::UnknownIdentifier,
        diagnostics::DiagnosticSeverity::Error,
        SourceLocation(file, 3),
        SourceRange(SourceLocation(file, 3), SourceLocation(file, 12)),
        "unknown identifier");
    const auto converted = makeLanguageDiagnostic(diagnostic, sources);
    expect(converted.has_value(), "native diagnostic converts");
    expect(converted.has_value() && converted->range == TextRange{{1, 0}, {1, 4}},
        "diagnostic range uses UTF-16");
    expect(converted.has_value() && converted->code == "UnknownIdentifier",
        "diagnostic publishes stable code");
    expect(converted.has_value() && converted->severity == LanguageDiagnosticSeverity::Error,
        "diagnostic severity is preserved");

    if (failures == 0)
    {
        std::cout << "PASS: UTF-16 positions and structured diagnostics\n";
    }
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
