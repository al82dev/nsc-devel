#include "LexerTestSupport.h"

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

// Registers one interpolated-string case with CL-LEX-011 metadata. Complexity is O(1).
void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CL-LEX-011", "Host.Lexer", "component", "PR",
        {"lexer", "interpolated-string", "brace-depth"}, std::move(function)});
}

// Builds one interpolated source spelling without interpreting its payload bytes. Complexity is O(n).
std::string interpolated(std::string content, bool close = true)
{
    std::string source = "$\"";
    source += content;
    if (close) source.push_back('"');
    return source;
}

// Checks the shared invalid-or-unterminated interpolated diagnostic. Complexity is O(1).
void expectInvalidDiagnostic(
    TestContext& context,
    const LexerFixture& fixture,
    std::uint32_t end,
    std::string_view caseId)
{
    const auto& diagnostics = fixture.diagnostics().diagnostics();
    context.expect(diagnostics.size() == 1, std::string(caseId) + " must report one string diagnostic");
    if (diagnostics.empty()) return;
    lexer_test::expectDiagnostic(context, diagnostics[0], DiagnosticId::InvalidStringLiteral,
        "invalid or unterminated interpolated string literal", 0, true, 0, end, caseId);
}

// Verifies a text-only template is emitted as one owning interpolated token. Complexity is O(1).
void registerTextCase(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-083", [](TestContext& context) {
        const std::string source = interpolated("text");
        const LexerFixture fixture(source);
        lexer_test::expectToken(context, fixture.tokens()[0], TokenKind::InterpolatedStringLiteral,
            0, 7, source, "TL-LEX-083");
        lexer_test::expectStringValue(context, fixture.tokens()[0], "text", "TL-LEX-083");
    });
}

// Verifies simple and nested hole braces remain in the single template token. Complexity is O(1).
void registerHoleDepthCase(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-084", [](TestContext& context) {
        for (const std::string& content : {std::string("{value}"), std::string("{outer{inner}}")})
        {
            const LexerFixture fixture(interpolated(content));
            context.expect(fixture.tokens()[0].kind() == TokenKind::InterpolatedStringLiteral,
                "TL-LEX-084 balanced simple and nested holes must remain valid");
            lexer_test::expectStringValue(context, fixture.tokens()[0], content, "TL-LEX-084");
        }
    });
}

// Verifies doubled braces outside holes remain doubled and do not alter depth. Complexity is O(1).
void registerDoubledBracesCase(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-085", [](TestContext& context) {
        const std::string content = "{{left}} {{}}";
        const LexerFixture fixture(interpolated(content));
        lexer_test::expectStringValue(context, fixture.tokens()[0], content, "TL-LEX-085");
        context.expect(fixture.diagnostics().diagnostics().empty(),
            "TL-LEX-085 doubled braces must not report diagnostics");
    });
}

// Verifies escapes outside a hole are decoded using the ordinary byte escape table. Complexity is O(1).
void registerOuterEscapeCase(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-086", [](TestContext& context) {
        const LexerFixture fixture(interpolated("a\\n\\x41"));
        lexer_test::expectStringValue(context, fixture.tokens()[0], "a\nA", "TL-LEX-086");
        context.expect(fixture.diagnostics().diagnostics().empty(),
            "TL-LEX-086 supported outer escapes must not report diagnostics");
    });
}

// Verifies slash and the following byte inside a hole are preserved for the later expression lexer. Complexity O(1).
void registerHoleEscapePreservationCase(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-087", [](TestContext& context) {
        const std::string content = "{\\n}";
        const LexerFixture fixture(interpolated(content));
        lexer_test::expectStringValue(context, fixture.tokens()[0], content, "TL-LEX-087");
        context.expect(fixture.diagnostics().diagnostics().empty(),
            "TL-LEX-087 a slash-byte pair inside a balanced hole must remain valid");
    });
}

// Verifies a loose closing brace invalidates the token without inventing a generic diagnostic. Complexity is O(1).
void registerLooseClosingBraceCase(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-088", [](TestContext& context) {
        const std::string source = interpolated("oops}");
        const LexerFixture fixture(source);
        lexer_test::expectToken(context, fixture.tokens()[0], TokenKind::Invalid,
            0, static_cast<std::uint32_t>(source.size()), source, "TL-LEX-088");
        context.expect(fixture.diagnostics().diagnostics().empty(),
            "TL-LEX-088 the current loose-brace path must not synthesize a diagnostic");
    });
}

// Verifies an open hole survives a quote and fails only at newline or EOF. Complexity is O(1).
void registerUnclosedHoleCase(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-089", [](TestContext& context) {
        const std::string eofSource = interpolated("{x");
        const LexerFixture eofFixture(eofSource);
        lexer_test::expectToken(context, eofFixture.tokens()[0], TokenKind::Invalid,
            0, static_cast<std::uint32_t>(eofSource.size()), eofSource, "TL-LEX-089");
        expectInvalidDiagnostic(context, eofFixture, static_cast<std::uint32_t>(eofSource.size()), "TL-LEX-089");
        const LexerFixture newlineFixture(interpolated("{x", false) + "\nnext");
        context.expect(newlineFixture.tokens().size() == 3 &&
                newlineFixture.tokens()[1].kind() == TokenKind::Identifier,
            "TL-LEX-089 newline recovery must resume at the following identifier");
        expectInvalidDiagnostic(context, newlineFixture, 4, "TL-LEX-089");
    });
}

// Verifies text without a closing quote fails at EOF or before newline and then progresses. Complexity is O(1).
void registerUnterminatedTemplateCase(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-090", [](TestContext& context) {
        const std::string eofSource = interpolated("text", false);
        const LexerFixture eofFixture(eofSource);
        lexer_test::expectToken(context, eofFixture.tokens()[0], TokenKind::Invalid,
            0, static_cast<std::uint32_t>(eofSource.size()), eofSource, "TL-LEX-090");
        expectInvalidDiagnostic(context, eofFixture, static_cast<std::uint32_t>(eofSource.size()), "TL-LEX-090");
        const LexerFixture newlineFixture(interpolated("text", false) + "\nnext");
        context.expect(newlineFixture.tokens().size() == 3 &&
                newlineFixture.tokens()[1].kind() == TokenKind::Identifier,
            "TL-LEX-090 newline must remain available to trivia before the next token");
        expectInvalidDiagnostic(context, newlineFixture, 6, "TL-LEX-090");
    });
}

// Characterizes the current byte-preserving template path, which performs no UTF-8/control validation. Complexity O(1).
void registerUnvalidatedTemplateBytesCase(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-091", [](TestContext& context) {
        std::string content;
        for (const unsigned byte : {0xc0U, 0x80U, 0U, 0x1fU, 0xf5U})
            content.push_back(static_cast<char>(byte));
        const LexerFixture fixture(interpolated(content));
        context.expect(fixture.tokens()[0].kind() == TokenKind::InterpolatedStringLiteral,
            "TL-LEX-091 current interpolated strings must preserve unvalidated template bytes");
        lexer_test::expectStringValue(context, fixture.tokens()[0], content, "TL-LEX-091");
        context.expect(fixture.diagnostics().diagnostics().empty(),
            "TL-LEX-091 byte preservation must not invent UTF-8 or control diagnostics");
    });
}

// Builds a raw interpolated spelling `$"""..."""` (or more quotes) without interpreting bytes.
// Complexity is O(n).
std::string rawInterpolated(std::string content, std::uint32_t quotes = 3)
{
    std::string source = "$";
    source.append(quotes, '"');
    source += content;
    source.append(quotes, '"');
    return source;
}

// Verifies `$"""..."""` emits one interpolated token with raw (unescaped) content. Complexity is O(1).
void registerRawInterpolatedCase(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-111", [](TestContext& context) {
        const LexerFixture fixture(rawInterpolated("a\\n{hole}b"));
        context.expect(fixture.tokens().size() == 2 &&
                fixture.tokens()[0].kind() == TokenKind::InterpolatedStringLiteral,
            "TL-LEX-111 raw interpolated must emit one interpolated token plus EOF");
        const std::string expected = "a\\n{hole}b";
        lexer_test::expectStringValue(context, fixture.tokens()[0], expected, "TL-LEX-111");
        context.expect(fixture.diagnostics().diagnostics().empty(),
            "TL-LEX-111 raw interpolated content must not decode escapes");
    });
}
}

// Registers CL-LEX-011 cases TL-LEX-083 through TL-LEX-091 plus TL-LEX-111 in deterministic order.
void registerLexerInterpolatedStringModule(TestRegistry& registry)
{
    registerTextCase(registry);
    registerHoleDepthCase(registry);
    registerDoubledBracesCase(registry);
    registerOuterEscapeCase(registry);
    registerHoleEscapePreservationCase(registry);
    registerLooseClosingBraceCase(registry);
    registerUnclosedHoleCase(registry);
    registerUnterminatedTemplateCase(registry);
    registerUnvalidatedTemplateBytesCase(registry);
    registerRawInterpolatedCase(registry);
}
}
