#include "LexerTestSupport.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace nsc::suite
{
namespace
{
using lexer_test::LexerFixture;
using nativesharp::diagnostics::DiagnosticId;
using nativesharp::lexer::TokenKind;

// Registers one ordinary-string case with CL-LEX-009 metadata. Complexity is O(1).
void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CL-LEX-009", "Host.Lexer", "component", "PR",
        {"lexer", "string", "utf8"}, std::move(function)});
}

// Wraps arbitrary bytes in an ordinary source-string delimiter without interpreting them. Complexity is O(n).
std::string quoted(std::string bytes, bool close = true)
{
    std::string source("\"");
    source += bytes;
    if (close) source.push_back('"');
    return source;
}

// Verifies the shared closed-string rejection contract and exact generic diagnostic. Complexity is O(n).
void expectInvalidClosedString(TestContext& context, const std::string& source, std::string_view caseId)
{
    const LexerFixture fixture(source);
    context.expect(fixture.tokens().size() == 2 && fixture.tokens()[0].kind() == TokenKind::Invalid,
        std::string(caseId) + " malformed string must emit Invalid then EOF");
    const auto& diagnostics = fixture.diagnostics().diagnostics();
    context.expect(diagnostics.size() == 1, std::string(caseId) + " must report one string diagnostic");
    if (diagnostics.empty()) return;
    lexer_test::expectDiagnostic(context, diagnostics[0], DiagnosticId::InvalidStringLiteral,
        "invalid or unterminated string literal", 0, true, 0,
        static_cast<std::uint32_t>(source.size()), caseId);
}

// Verifies empty and printable ASCII strings. Complexity is O(1).
void registerAsciiCase(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-065", [](TestContext& context) {
        const LexerFixture fixture("\"\" \"ASCII\"");
        context.expect(fixture.tokens().size() == 3, "TL-LEX-065 must emit two strings and EOF");
        if (fixture.tokens().size() != 3) return;
        lexer_test::expectToken(context, fixture.tokens()[0], TokenKind::StringLiteral, 0, 2, "\"\"",
            "TL-LEX-065");
        lexer_test::expectStringValue(context, fixture.tokens()[0], "", "TL-LEX-065");
        lexer_test::expectToken(context, fixture.tokens()[1], TokenKind::StringLiteral, 3, 10, "\"ASCII\"",
            "TL-LEX-065");
        lexer_test::expectStringValue(context, fixture.tokens()[1], "ASCII", "TL-LEX-065");
    });
}

// Verifies all simple escapes and hexadecimal escape decoding in one exact byte sequence. Complexity is O(1).
void registerEscapesCase(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-066", [](TestContext& context) {
        const std::string source = R"NS("\0\a\b\e\f\n\r\t\v\\\'\"\x41\u00E9\U0001F600")NS";
        std::string expected;
        for (const unsigned byte : {0U, 7U, 8U, 27U, 12U, 10U, 13U, 9U, 11U, 92U, 39U, 34U, 65U,
                0xC3U, 0xA9U, 0xF0U, 0x9FU, 0x98U, 0x80U})
            expected.push_back(static_cast<char>(byte));
        const LexerFixture fixture(source);
        lexer_test::expectToken(context, fixture.tokens()[0], TokenKind::StringLiteral, 0,
            static_cast<std::uint32_t>(source.size()), source, "TL-LEX-066");
        lexer_test::expectStringValue(context, fixture.tokens()[0], expected, "TL-LEX-066");
        context.expect(fixture.diagnostics().diagnostics().empty(),
            "TL-LEX-066 supported escapes must not report diagnostics");
    });
}

// Verifies valid two-, three-, and four-byte UTF-8 sequences are preserved byte-for-byte. Complexity is O(1).
void registerValidUtf8Case(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-067", [](TestContext& context) {
        std::string bytes;
        for (const unsigned byte : {0xc2U, 0xa2U, 0xe2U, 0x82U, 0xacU, 0xf0U, 0x9fU, 0x98U, 0x80U})
            bytes.push_back(static_cast<char>(byte));
        const std::string source = quoted(bytes);
        const LexerFixture fixture(source);
        lexer_test::expectToken(context, fixture.tokens()[0], TokenKind::StringLiteral, 0,
            static_cast<std::uint32_t>(source.size()), source, "TL-LEX-067");
        lexer_test::expectStringValue(context, fixture.tokens()[0], bytes, "TL-LEX-067");
        context.expect(fixture.diagnostics().diagnostics().empty(),
            "TL-LEX-067 well-formed UTF-8 must not report diagnostics");
    });
}

// Verifies both forbidden overlong lead bytes. Complexity is O(1).
void registerOverlongUtf8Case(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-068", [](TestContext& context) {
        for (const unsigned lead : {0xc0U, 0xc1U})
        {
            std::string bytes;
            bytes.push_back(static_cast<char>(lead));
            bytes.push_back(static_cast<char>(0x80));
            expectInvalidClosedString(context, quoted(bytes), "TL-LEX-068");
        }
    });
}

// Verifies the lower and upper UTF-8 surrogate encodings are rejected. Complexity is O(1).
void registerSurrogateUtf8Case(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-069", [](TestContext& context) {
        for (const std::array<unsigned, 3> bytes :
             {std::array<unsigned, 3>{0xed, 0xa0, 0x80}, std::array<unsigned, 3>{0xed, 0xbf, 0xbf}})
        {
            std::string payload;
            for (const unsigned byte : bytes) payload.push_back(static_cast<char>(byte));
            expectInvalidClosedString(context, quoted(payload), "TL-LEX-069");
        }
    });
}

// Verifies encodings above U+10FFFF and illegal five-plane lead bytes are rejected. Complexity is O(1).
void registerAboveUnicodeLimitCase(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-070", [](TestContext& context) {
        for (const std::array<unsigned, 4> bytes :
             {std::array<unsigned, 4>{0xf4, 0x90, 0x80, 0x80},
              std::array<unsigned, 4>{0xf5, 0x80, 0x80, 0x80}})
        {
            std::string payload;
            for (const unsigned byte : bytes) payload.push_back(static_cast<char>(byte));
            expectInvalidClosedString(context, quoted(payload), "TL-LEX-070");
        }
    });
}

// Verifies missing and malformed continuation bytes terminate without losing cursor progress. Complexity is O(1).
void registerContinuationFailureCase(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-071", [](TestContext& context) {
        std::string missing;
        missing.push_back(static_cast<char>(0xc2));
        const LexerFixture missingFixture(quoted(missing, false));
        context.expect(missingFixture.tokens().size() == 2 &&
                missingFixture.tokens()[0].kind() == TokenKind::Invalid,
            "TL-LEX-071 a missing continuation must emit Invalid then EOF");
        std::string malformed;
        malformed.push_back(static_cast<char>(0xe2));
        malformed.push_back('A');
        malformed.push_back(static_cast<char>(0x82));
        const LexerFixture malformedFixture(quoted(malformed));
        context.expect(malformedFixture.tokens().size() == 2 &&
                malformedFixture.tokens()[0].range().end().offset() == malformedFixture.source().size(),
            "TL-LEX-071 malformed continuation recovery must still reach the closing quote");
    });
}

// Verifies direct NUL and control bytes invalidate an ordinary string. Complexity is O(1).
void registerControlByteCase(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-072", [](TestContext& context) {
        for (const unsigned byte : {0U, 0x1fU})
        {
            std::string payload("a");
            payload.push_back(static_cast<char>(byte));
            payload.push_back('b');
            expectInvalidClosedString(context, quoted(payload), "TL-LEX-072");
        }
    });
}

// Verifies newline and EOF termination, including that newline remains available to trivia. Complexity is O(1).
void registerUnterminatedCase(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-073", [](TestContext& context) {
        const LexerFixture newlineFixture("\"abc\nnext");
        context.expect(newlineFixture.tokens().size() == 3,
            "TL-LEX-073 newline recovery must emit Invalid, Identifier, and EOF");
        if (newlineFixture.tokens().size() == 3)
        {
            lexer_test::expectToken(context, newlineFixture.tokens()[0], TokenKind::Invalid,
                0, 4, "\"abc", "TL-LEX-073");
            lexer_test::expectToken(context, newlineFixture.tokens()[1], TokenKind::Identifier,
                5, 9, "next", "TL-LEX-073");
        }
        const LexerFixture eofFixture("\"abc");
        lexer_test::expectToken(context, eofFixture.tokens()[0], TokenKind::Invalid,
            0, 4, "\"abc", "TL-LEX-073");
    });
}

// Verifies escape and enclosing-string diagnostics remain separately observable. Complexity is O(1).
void registerUnknownEscapeCase(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-074", [](TestContext& context) {
        const std::string source = R"NS("a\qz")NS";
        const LexerFixture fixture(source);
        const auto& diagnostics = fixture.diagnostics().diagnostics();
        context.expect(fixture.tokens()[0].kind() == TokenKind::Invalid,
            "TL-LEX-074 an unsupported escape must invalidate the string");
        context.expect(diagnostics.size() == 2,
            "TL-LEX-074 must retain both escape and string diagnostics");
        if (diagnostics.size() != 2) return;
        lexer_test::expectDiagnostic(context, diagnostics[0], DiagnosticId::InvalidEscapeSequence,
            "unsupported escape sequence", 3, true, 3, 4, "TL-LEX-074");
        lexer_test::expectDiagnostic(context, diagnostics[1], DiagnosticId::InvalidStringLiteral,
            "invalid or unterminated string literal", 0, true, 0, 6, "TL-LEX-074");
    });
}
}

// Registers CL-LEX-009 cases TL-LEX-065 through TL-LEX-074 in deterministic ID order.
void registerLexerStringLiteralModule(TestRegistry& registry)
{
    registerAsciiCase(registry);
    registerEscapesCase(registry);
    registerValidUtf8Case(registry);
    registerOverlongUtf8Case(registry);
    registerSurrogateUtf8Case(registry);
    registerAboveUnicodeLimitCase(registry);
    registerContinuationFailureCase(registry);
    registerControlByteCase(registry);
    registerUnterminatedCase(registry);
    registerUnknownEscapeCase(registry);
}
}
