#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Basic/FileId.h"
#include "nativesharp/Basic/SourceLocation.h"
#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/Lexer/Token.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace nsc::suite
{
namespace
{
using nativesharp::FileId;
using nativesharp::SourceLocation;
using nativesharp::SourceRange;
using nativesharp::lexer::Token;
using nativesharp::lexer::TokenKind;

// Builds a valid physical source range whose file and offsets can be asserted independently.
SourceRange makeRange(std::uint32_t file, std::uint32_t begin, std::uint32_t end)
{
    const FileId fileId{file};
    return SourceRange{SourceLocation{fileId, begin}, SourceLocation{fileId, end}};
}

// Registers one CL-LEX-002 case without repeating component metadata.
void addTokenValueCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CL-LEX-002", "Host.Lexer", "component", "PR",
        {"lexer", "token", "value"}, std::move(function)});
}

// Proves the default TokenValue alternative and every constructor field exactly.
void testDefaultTokenValue(TestContext& context)
{
    const SourceRange expectedRange = makeRange(17, 3, 8);
    const Token token{TokenKind::Identifier, expectedRange, "alpha"};
    context.expect(token.kind() == TokenKind::Identifier,
        "TL-LEX-009 token kind must preserve the constructor argument");
    context.expect(token.range().begin() == expectedRange.begin() &&
            token.range().end() == expectedRange.end(),
        "TL-LEX-009 token range must preserve file and offsets");
    context.expect(token.lexeme() == "alpha",
        "TL-LEX-009 token lexeme must preserve its source view");
    context.expect(std::holds_alternative<std::monostate>(token.value()),
        "TL-LEX-009 omitted token value must select monostate");
}

// Proves all four non-empty TokenValue alternatives and their exact payloads.
void testTokenValueAlternatives(TestContext& context)
{
    const Token integer{TokenKind::IntegerLiteral, makeRange(1, 0, 2), "42", std::uint64_t{42}};
    const Token floating{TokenKind::FloatingLiteral, makeRange(1, 3, 7), "1.25", 1.25};
    const Token character{TokenKind::CharacterLiteral, makeRange(1, 8, 11), "'A'", std::uint8_t{'A'}};
    const Token string{TokenKind::StringLiteral, makeRange(1, 12, 18), "\"text\"", std::string{"text"}};

    const auto* integerValue = std::get_if<std::uint64_t>(&integer.value());
    const auto* floatingValue = std::get_if<double>(&floating.value());
    const auto* characterValue = std::get_if<std::uint8_t>(&character.value());
    const auto* stringValue = std::get_if<std::string>(&string.value());
    context.expect(integerValue != nullptr && *integerValue == 42,
        "TL-LEX-010 integer value must preserve its uint64 alternative and payload");
    context.expect(floatingValue != nullptr && *floatingValue == 1.25,
        "TL-LEX-010 floating value must preserve its double alternative and payload");
    context.expect(characterValue != nullptr && *characterValue == static_cast<std::uint8_t>('A'),
        "TL-LEX-010 character value must preserve its uint8 alternative and payload");
    context.expect(stringValue != nullptr && *stringValue == "text",
        "TL-LEX-010 string value must preserve its owning string alternative and payload");
}

// Returns a token whose value originates in a temporary string that dies before the assertion.
Token makeTemporaryStringValueToken()
{
    return Token{TokenKind::StringLiteral, makeRange(5, 0, 7), "\"owned\"", std::string{"owned"}};
}

// Proves that the decoded string value is owned by Token after the input temporary dies.
void testStringValueOwnership(TestContext& context)
{
    const Token token = makeTemporaryStringValueToken();
    const auto* value = std::get_if<std::string>(&token.value());
    context.expect(value != nullptr && *value == "owned",
        "TL-LEX-011 Token must own a string value after the source temporary dies");
}

// Proves that lexeme is deliberately a non-owning view into a live source buffer.
void testLexemeViewAndRange(TestContext& context)
{
    const std::string source = "prefix-token-suffix";
    const std::string_view expectedLexeme{source.data() + 7, 5};
    const SourceRange expectedRange = makeRange(31, 7, 12);
    const Token token{TokenKind::Identifier, expectedRange, expectedLexeme};

    context.expect(token.lexeme() == "token" && token.lexeme().data() == source.data() + 7,
        "TL-LEX-012 lexeme must remain the exact view into the live source buffer");
    context.expect(token.range().begin() == expectedRange.begin() &&
            token.range().end() == expectedRange.end(),
        "TL-LEX-012 range must preserve the exact file and byte offsets");
}
}

// Adds all CL-LEX-002 Token, TokenValue, ownership, and lifetime cases to the host registry.
void registerLexerTokenValueModule(TestRegistry& registry)
{
    addTokenValueCase(registry, "TL-LEX-009", testDefaultTokenValue);
    addTokenValueCase(registry, "TL-LEX-010", testTokenValueAlternatives);
    addTokenValueCase(registry, "TL-LEX-011", testStringValueOwnership);
    addTokenValueCase(registry, "TL-LEX-012", testLexemeViewAndRange);
}
}
