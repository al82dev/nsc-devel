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

// Registers one raw-string case with CL-LEX-010 metadata. Complexity is O(1).
void addCase(TestRegistry& registry,
             std::string id,
             TestFunction function,
             TestDisposition disposition = TestDisposition::Required)
{
    registry.add(TestCase{std::move(id), "CL-LEX-010", "Host.Lexer", "component", "PR",
        {"lexer", "raw-string", "utf8"}, std::move(function), disposition});
}

// Builds a raw-string spelling with independently controlled opening and closing widths. Complexity is O(n).
std::string rawString(std::size_t openingWidth, std::string content, std::size_t closingWidth)
{
    std::string source(openingWidth, '"');
    source += content;
    source.append(closingWidth, '"');
    return source;
}

// Checks the raw-string-specific failure diagnostic after a closing delimiter is found. Complexity is O(n).
void expectInvalidClosedRaw(TestContext& context, const std::string& source, std::string_view caseId)
{
    const LexerFixture fixture(source);
    context.expect(fixture.tokens().size() == 2 && fixture.tokens()[0].kind() == TokenKind::Invalid,
        std::string(caseId) + " invalid raw string must emit Invalid then EOF");
    const auto& diagnostics = fixture.diagnostics().diagnostics();
    context.expect(diagnostics.size() == 1, std::string(caseId) + " must report one raw-string diagnostic");
    if (diagnostics.empty()) return;
    lexer_test::expectDiagnostic(context, diagnostics[0], DiagnosticId::InvalidStringLiteral,
        "raw string literal contains an invalid control byte or UTF-8 sequence",
        0, true, 0, static_cast<std::uint32_t>(source.size()), caseId);
}

// Verifies the empty triple-quoted spelling. Complexity is O(1).
void registerEmptyCase(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-075", [](TestContext& context) {
        const std::string source = rawString(3, {}, 3);
        const LexerFixture fixture(source);
        lexer_test::expectToken(context, fixture.tokens()[0], TokenKind::StringLiteral,
            0, 6, source, "TL-LEX-075");
        lexer_test::expectStringValue(context, fixture.tokens()[0], "", "TL-LEX-075");
    }, TestDisposition::KnownProductDefect);
}

// Verifies that opening runs of three, four, and five quotes fix the required close width. Complexity is O(1).
void registerDelimiterWidthCase(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-076", [](TestContext& context) {
        for (const std::size_t width : {3U, 4U, 5U})
        {
            const std::string source = rawString(width, "body", width);
            const LexerFixture fixture(source);
            lexer_test::expectToken(context, fixture.tokens()[0], TokenKind::StringLiteral,
                0, static_cast<std::uint32_t>(source.size()), source, "TL-LEX-076");
            lexer_test::expectStringValue(context, fixture.tokens()[0], "body", "TL-LEX-076");
        }
    });
}

// Verifies slash, backslash, and a quote run below the delimiter width remain literal. Complexity is O(1).
void registerLiteralContentCase(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-077", [](TestContext& context) {
        std::string content = "/\\";
        content.append(3, '"');
        content.push_back('x');
        const std::string source = rawString(4, content, 4);
        const LexerFixture fixture(source);
        lexer_test::expectStringValue(context, fixture.tokens()[0], content, "TL-LEX-077");
        context.expect(fixture.diagnostics().diagnostics().empty(),
            "TL-LEX-077 literal raw content must not report diagnostics");
    });
}

// Verifies tab, CR, LF, and CRLF bytes are accepted and preserved. Complexity is O(1).
void registerAcceptedControlsCase(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-078", [](TestContext& context) {
        const std::string content = "a\tb\rc\r\nd\ne";
        const std::string source = rawString(3, content, 3);
        const LexerFixture fixture(source);
        lexer_test::expectStringValue(context, fixture.tokens()[0], content, "TL-LEX-078");
        context.expect(fixture.tokens()[0].range().end().offset() == source.size(),
            "TL-LEX-078 accepted physical line bytes must remain inside one raw token");
    });
}

// Verifies a non-whitelisted control byte invalidates the token at the close delimiter. Complexity is O(1).
void registerRejectedControlCase(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-079", [](TestContext& context) {
        std::string content("a");
        content.push_back(static_cast<char>(0x1f));
        content.push_back('b');
        expectInvalidClosedRaw(context, rawString(3, content, 3), "TL-LEX-079");
    });
}

// Verifies valid UTF-8 preservation and all encoded-scalar boundary rejections. Complexity is O(1).
void registerUtf8MatrixCase(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-080", [](TestContext& context) {
        std::string valid;
        for (const unsigned byte : {0xc2U, 0xa2U, 0xe2U, 0x82U, 0xacU, 0xf0U, 0x9fU, 0x98U, 0x80U})
            valid.push_back(static_cast<char>(byte));
        const LexerFixture validFixture(rawString(3, valid, 3));
        lexer_test::expectStringValue(context, validFixture.tokens()[0], valid, "TL-LEX-080");
        const std::array<std::array<unsigned, 4>, 4> invalid = {{
            {0xc0, 0x80, 0x20, 0x20}, {0xed, 0xa0, 0x80, 0x20},
            {0xf4, 0x90, 0x80, 0x80}, {0xe2, 0x41, 0x82, 0x20}}};
        for (const auto& bytes : invalid)
        {
            std::string content;
            for (const unsigned byte : bytes) content.push_back(static_cast<char>(byte));
            expectInvalidClosedRaw(context, rawString(3, content, 3), "TL-LEX-080");
        }
    });
}

// Verifies a closing run consumes exactly the opening width and leaves excess quotes to lex later. Complexity O(1).
void registerExcessClosingQuotesCase(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-081", [](TestContext& context) {
        const std::string source = rawString(3, "x", 4);
        const LexerFixture fixture(source);
        context.expect(fixture.tokens().size() == 3,
            "TL-LEX-081 one excess quote must be tokenized after the raw string before EOF");
        if (fixture.tokens().size() != 3) return;
        lexer_test::expectToken(context, fixture.tokens()[0], TokenKind::StringLiteral,
            0, 7, std::string_view(source).substr(0, 7), "TL-LEX-081");
        lexer_test::expectStringValue(context, fixture.tokens()[0], "x", "TL-LEX-081");
        lexer_test::expectToken(context, fixture.tokens()[1], TokenKind::Invalid, 7, 8, "\"", "TL-LEX-081");
    });
}

// Verifies EOF before a matching raw delimiter reports the exact unterminated contract. Complexity is O(1).
void registerUnterminatedCase(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-082", [](TestContext& context) {
        const std::string source = rawString(3, "body", 0);
        const LexerFixture fixture(source);
        lexer_test::expectToken(context, fixture.tokens()[0], TokenKind::Invalid,
            0, static_cast<std::uint32_t>(source.size()), source, "TL-LEX-082");
        const auto& diagnostics = fixture.diagnostics().diagnostics();
        context.expect(diagnostics.size() == 1, "TL-LEX-082 must report one unterminated diagnostic");
        if (diagnostics.empty()) return;
        lexer_test::expectDiagnostic(context, diagnostics[0], DiagnosticId::InvalidStringLiteral,
            "invalid or unterminated raw string literal", 0, true, 0,
            static_cast<std::uint32_t>(source.size()), "TL-LEX-082");
    });
}
}

// Registers CL-LEX-010 cases TL-LEX-075 through TL-LEX-082 in deterministic ID order.
void registerLexerRawStringModule(TestRegistry& registry)
{
    registerEmptyCase(registry);
    registerDelimiterWidthCase(registry);
    registerLiteralContentCase(registry);
    registerAcceptedControlsCase(registry);
    registerRejectedControlCase(registry);
    registerUtf8MatrixCase(registry);
    registerExcessClosingQuotesCase(registry);
    registerUnterminatedCase(registry);
}
}
