#pragma once

#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Lexer/Lexer.h"
#include "nativesharp/Source/SourceFile.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace nsc::suite::lexer_test
{
// Owns the source bytes for the entire lifetime of the non-owning token lexemes. Construction lexes the
// source exactly once with a fresh diagnostic engine, so every case starts from isolated cursor state.
class LexerFixture final
{
public:
    explicit LexerFixture(
        std::string source,
        std::size_t errorLimit = nativesharp::diagnostics::DiagnosticEngine::kDefaultErrorLimit)
        : source_(nativesharp::FileId(7001), std::filesystem::path("phase2-lexer.ns"), std::move(source)),
          diagnostics_(errorLimit)
    {
        nativesharp::lexer::Lexer lexer(source_, diagnostics_);
        tokens_ = lexer.lexAll();
    }

    // Returns the immutable source retained by this fixture. Complexity is O(1).
    [[nodiscard]] const nativesharp::source::SourceFile& source() const noexcept
    {
        return source_;
    }

    // Returns the complete token sequence, including its single terminal EOF. Complexity is O(1).
    [[nodiscard]] const std::vector<nativesharp::lexer::Token>& tokens() const noexcept
    {
        return tokens_;
    }

    // Returns the diagnostics accumulated during the isolated lexical pass. Complexity is O(1).
    [[nodiscard]] const nativesharp::diagnostics::DiagnosticEngine& diagnostics() const noexcept
    {
        return diagnostics_;
    }

private:
    nativesharp::source::SourceFile source_;
    nativesharp::diagnostics::DiagnosticEngine diagnostics_;
    std::vector<nativesharp::lexer::Token> tokens_;
};

// Checks the stable observable token fields without inspecting private lexer state. Complexity is O(1).
inline void expectToken(
    TestContext& context,
    const nativesharp::lexer::Token& token,
    nativesharp::lexer::TokenKind kind,
    std::uint32_t begin,
    std::uint32_t end,
    std::string_view lexeme,
    std::string_view caseId)
{
    context.expect(token.kind() == kind, std::string(caseId) + " token kind must match");
    context.expect(token.range().begin().fileId() == nativesharp::FileId(7001) &&
            token.range().end().fileId() == nativesharp::FileId(7001),
        std::string(caseId) + " token range must retain the source FileId");
    context.expect(token.range().begin().offset() == begin && token.range().end().offset() == end,
        std::string(caseId) + " token range must be the exact half-open byte interval");
    context.expect(token.lexeme() == lexeme, std::string(caseId) + " token lexeme must preserve source bytes");
}

// Checks one byte-valued token alternative. Complexity is O(1).
inline void expectByteValue(
    TestContext& context,
    const nativesharp::lexer::Token& token,
    std::uint8_t expected,
    std::string_view caseId)
{
    const auto* value = std::get_if<std::uint8_t>(&token.value());
    context.expect(value != nullptr && *value == expected,
        std::string(caseId) + " token must retain the exact byte value");
}

// Checks one owning string-valued token alternative, including embedded NUL bytes. Complexity is O(n).
inline void expectStringValue(
    TestContext& context,
    const nativesharp::lexer::Token& token,
    std::string_view expected,
    std::string_view caseId)
{
    const auto* value = std::get_if<std::string>(&token.value());
    context.expect(value != nullptr && std::string_view(*value) == expected,
        std::string(caseId) + " token must retain the exact decoded byte sequence");
}

// Checks one diagnostic's identity, severity, message, location, and optional half-open range. Complexity is O(1).
inline void expectDiagnostic(
    TestContext& context,
    const nativesharp::diagnostics::Diagnostic& diagnostic,
    nativesharp::diagnostics::DiagnosticId id,
    std::string_view message,
    std::uint32_t location,
    bool hasRange,
    std::uint32_t begin,
    std::uint32_t end,
    std::string_view caseId)
{
    using nativesharp::diagnostics::DiagnosticSeverity;
    context.expect(diagnostic.id() == id, std::string(caseId) + " diagnostic ID must match");
    context.expect(diagnostic.severity() == DiagnosticSeverity::Error,
        std::string(caseId) + " diagnostic severity must be Error");
    context.expect(diagnostic.location() == nativesharp::SourceLocation(nativesharp::FileId(7001), location),
        std::string(caseId) + " diagnostic location must match");
    context.expect(diagnostic.message() == message, std::string(caseId) + " diagnostic message must match");
    context.expect(diagnostic.range().has_value() == hasRange,
        std::string(caseId) + " diagnostic range presence must match");
    if (hasRange && diagnostic.range())
    {
        context.expect(diagnostic.range()->begin().offset() == begin &&
                diagnostic.range()->end().offset() == end,
            std::string(caseId) + " diagnostic range must match");
    }
}
}
