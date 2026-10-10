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

// Registers one character-literal case with the CL-LEX-008 traceability metadata. Complexity is O(1).
void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CL-LEX-008", "Host.Lexer", "component", "PR",
        {"lexer", "character", "escape"}, std::move(function)});
}

// Verifies direct printable ASCII bytes and their physical ranges. Complexity is O(1).
void registerDirectAsciiCase(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-056", [](TestContext& context) {
        const LexerFixture fixture("'A' ' ' '~'");
        const auto& tokens = fixture.tokens();
        context.expect(tokens.size() == 4, "TL-LEX-056 must emit three characters and EOF");
        if (tokens.size() != 4) return;
        lexer_test::expectToken(context, tokens[0], TokenKind::CharacterLiteral, 0, 3, "'A'", "TL-LEX-056");
        lexer_test::expectToken(context, tokens[1], TokenKind::CharacterLiteral, 4, 7, "' '", "TL-LEX-056");
        lexer_test::expectToken(context, tokens[2], TokenKind::CharacterLiteral, 8, 11, "'~'", "TL-LEX-056");
        lexer_test::expectByteValue(context, tokens[0], 'A', "TL-LEX-056");
        lexer_test::expectByteValue(context, tokens[1], ' ', "TL-LEX-056");
        lexer_test::expectByteValue(context, tokens[2], '~', "TL-LEX-056");
    });
}

// Verifies every simple escape accepted by decodeEscapeByte. Complexity is O(k), k = 11 fixed samples.
void registerSimpleEscapesCase(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-057", [](TestContext& context) {
        const std::array<std::pair<std::string_view, std::uint8_t>, 11> samples = {{
            {"'\\0'", 0}, {"'\\a'", 7}, {"'\\b'", 8}, {"'\\f'", 12}, {"'\\n'", 10},
            {"'\\r'", 13}, {"'\\t'", 9}, {"'\\v'", 11}, {"'\\\\'", '\\'},
            {"'\\''", '\''}, {"'\\\"'", '"'}}};
        std::string source;
        for (const auto& sample : samples)
        {
            if (!source.empty()) source.push_back(' ');
            source.append(sample.first);
        }
        const LexerFixture fixture(source);
        context.expect(fixture.tokens().size() == samples.size() + 1,
            "TL-LEX-057 every simple escape must emit one token plus EOF");
        std::uint32_t begin = 0;
        for (std::size_t index = 0; index < samples.size() && index < fixture.tokens().size(); ++index)
        {
            const std::uint32_t end = begin + static_cast<std::uint32_t>(samples[index].first.size());
            lexer_test::expectToken(context, fixture.tokens()[index], TokenKind::CharacterLiteral,
                begin, end, samples[index].first, "TL-LEX-057");
            lexer_test::expectByteValue(context, fixture.tokens()[index], samples[index].second, "TL-LEX-057");
            begin = end + 1;
        }
        context.expect(fixture.diagnostics().diagnostics().empty(),
            "TL-LEX-057 valid simple escapes must not report diagnostics");
    });
}

// Verifies two-digit hexadecimal decoding at byte boundaries. Complexity is O(1).
void registerHexEscapesCase(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-058", [](TestContext& context) {
        const LexerFixture fixture("'\\x00' '\\x7f' '\\xFF'");
        const std::array<std::uint8_t, 3> expected = {0x00, 0x7f, 0xff};
        const std::array<std::uint32_t, 3> begins = {0, 7, 14};
        context.expect(fixture.tokens().size() == 4, "TL-LEX-058 must emit three characters and EOF");
        for (std::size_t index = 0; index < expected.size() && index < fixture.tokens().size(); ++index)
        {
            lexer_test::expectToken(context, fixture.tokens()[index], TokenKind::CharacterLiteral,
                begins[index], begins[index] + 6, fixture.source().buffer().substr(begins[index], 6),
                "TL-LEX-058");
            lexer_test::expectByteValue(context, fixture.tokens()[index], expected[index], "TL-LEX-058");
        }
    });
}

// Verifies incomplete and non-hex \x recovery plus the two exact diagnostics. Complexity is O(1).
void registerInvalidHexEscapeCase(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-059", [](TestContext& context) {
        for (const std::string& source : {std::string("'\\x0'"), std::string("'\\xGG'")})
        {
            const LexerFixture fixture(source);
            context.expect(fixture.tokens().size() == 2 && fixture.tokens()[0].kind() == TokenKind::Invalid,
                "TL-LEX-059 each malformed hex escape must emit Invalid then EOF");
            const auto& diagnostics = fixture.diagnostics().diagnostics();
            context.expect(diagnostics.size() == 2,
                "TL-LEX-059 malformed hex must report escape and character diagnostics");
            if (diagnostics.size() != 2) continue;
            lexer_test::expectDiagnostic(context, diagnostics[0], DiagnosticId::InvalidEscapeSequence,
                "\\x escape requires exactly two hexadecimal digits", 0, false, 0, 0, "TL-LEX-059");
            lexer_test::expectDiagnostic(context, diagnostics[1], DiagnosticId::InvalidCharacterLiteral,
                "invalid character literal; exactly one ASCII char or supported escape is required",
                0, true, 0, static_cast<std::uint32_t>(source.size()), "TL-LEX-059");
        }
    });
}

// Verifies unknown-escape range attribution and enclosing literal failure. Complexity is O(1).
void registerUnknownEscapeCase(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-060", [](TestContext& context) {
        const LexerFixture fixture("'\\q'");
        const auto& diagnostics = fixture.diagnostics().diagnostics();
        context.expect(fixture.tokens().size() == 2 && fixture.tokens()[0].kind() == TokenKind::Invalid,
            "TL-LEX-060 an unsupported escape must emit Invalid then EOF");
        context.expect(diagnostics.size() == 2,
            "TL-LEX-060 an unsupported escape must report two diagnostics");
        if (diagnostics.size() != 2) return;
        lexer_test::expectDiagnostic(context, diagnostics[0], DiagnosticId::InvalidEscapeSequence,
            "unsupported escape sequence", 2, true, 2, 3, "TL-LEX-060");
        lexer_test::expectDiagnostic(context, diagnostics[1], DiagnosticId::InvalidCharacterLiteral,
            "invalid character literal; exactly one ASCII char or supported escape is required",
            0, true, 0, 4, "TL-LEX-060");
    });
}

// Verifies the empty-literal rejection contract. Complexity is O(1).
void registerEmptyCharacterCase(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-061", [](TestContext& context) {
        const LexerFixture fixture("''");
        lexer_test::expectToken(context, fixture.tokens()[0], TokenKind::Invalid, 0, 2, "''", "TL-LEX-061");
        const auto& diagnostics = fixture.diagnostics().diagnostics();
        context.expect(diagnostics.size() == 1, "TL-LEX-061 must report exactly one diagnostic");
        if (diagnostics.empty()) return;
        lexer_test::expectDiagnostic(context, diagnostics[0], DiagnosticId::InvalidCharacterLiteral,
            "invalid character literal; exactly one ASCII char or supported escape is required",
            0, true, 0, 2, "TL-LEX-061");
    });
}

// Verifies recovery through the closing quote of a multi-byte character spelling. Complexity is O(1).
void registerMultipleCharactersCase(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-062", [](TestContext& context) {
        const LexerFixture fixture("'ab'");
        lexer_test::expectToken(context, fixture.tokens()[0], TokenKind::Invalid, 0, 4, "'ab'", "TL-LEX-062");
        context.expect(fixture.tokens().size() == 2 && fixture.tokens()[1].kind() == TokenKind::EndOfFile,
            "TL-LEX-062 recovery must resume after the closing quote");
    });
}

// Verifies that unterminated character recovery stops before the physical newline. Complexity is O(1).
void registerNewLineRecoveryCase(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-063", [](TestContext& context) {
        const LexerFixture fixture("'a\nnext");
        context.expect(fixture.tokens().size() == 3, "TL-LEX-063 must emit Invalid, Identifier, and EOF");
        if (fixture.tokens().size() != 3) return;
        lexer_test::expectToken(context, fixture.tokens()[0], TokenKind::Invalid, 0, 2, "'a", "TL-LEX-063");
        lexer_test::expectToken(context, fixture.tokens()[1], TokenKind::Identifier, 3, 7, "next", "TL-LEX-063");
    });
}

// Verifies direct control and non-ASCII bytes are never accepted as a character value. Complexity is O(1).
void registerDirectInvalidByteCase(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-064", [](TestContext& context) {
        for (const unsigned byte : {0x1fU, 0x80U})
        {
            std::string source("'");
            source.push_back(static_cast<char>(byte));
            source.push_back('\'');
            const LexerFixture fixture(source);
            context.expect(fixture.tokens().size() == 2 && fixture.tokens()[0].kind() == TokenKind::Invalid,
                "TL-LEX-064 a direct control or non-ASCII byte must emit Invalid then EOF");
            context.expect(fixture.diagnostics().diagnostics().size() == 1,
                "TL-LEX-064 a direct invalid byte must report one character diagnostic");
        }
    });
}
}

// Registers CL-LEX-008 cases TL-LEX-056 through TL-LEX-064 in deterministic ID order.
void registerLexerCharacterLiteralModule(TestRegistry& registry)
{
    registerDirectAsciiCase(registry);
    registerSimpleEscapesCase(registry);
    registerHexEscapesCase(registry);
    registerInvalidHexEscapeCase(registry);
    registerUnknownEscapeCase(registry);
    registerEmptyCharacterCase(registry);
    registerMultipleCharactersCase(registry);
    registerNewLineRecoveryCase(registry);
    registerDirectInvalidByteCase(registry);
}
}
