#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Basic/FileId.h"
#include "nativesharp/Basic/SourceLocation.h"
#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/Lexer/Token.h"
#include "nativesharp/Lexer/TokenDumper.h"

#include <array>
#include <cstdint>
#include <sstream>
#include <span>
#include <string>
#include <utility>

namespace nsc::suite
{
namespace
{
using nativesharp::FileId;
using nativesharp::SourceLocation;
using nativesharp::SourceRange;
using nativesharp::lexer::Token;
using nativesharp::lexer::TokenDumper;
using nativesharp::lexer::TokenKind;

// Builds ranges for dump tests while keeping FileId independent from printed byte offsets.
SourceRange makeDumpRange(std::uint32_t file, std::uint32_t begin, std::uint32_t end)
{
    const FileId fileId{file};
    return SourceRange{SourceLocation{fileId, begin}, SourceLocation{fileId, end}};
}

// Registers one CL-LEX-003 case without repeating component metadata.
void addTokenDumperCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CL-LEX-003", "Host.Lexer", "component", "PR",
        {"lexer", "token-dump"}, std::move(function)});
}

// Dumps a contiguous token range and returns the complete byte-for-byte text oracle input.
template<std::size_t Size>
std::string dumpTokens(const std::array<Token, Size>& tokens)
{
    std::ostringstream output;
    TokenDumper{}.dump(std::span<const Token>{tokens}, output);
    return output.str();
}

// Validates the stable kind, quoted lexeme, physical offsets, spacing, and line endings.
void testSimpleTokenDump(TestContext& context)
{
    constexpr std::string_view ifLexeme = "if";
    constexpr std::string_view leftParenLexeme = "(";
    constexpr std::string_view eofLexeme;
    const std::array tokens = {
        Token{TokenKind::KwIf, makeDumpRange(1, 0, 2), ifLexeme},
        Token{TokenKind::LeftParen, makeDumpRange(1, 2, 3), leftParenLexeme},
        Token{TokenKind::EndOfFile, makeDumpRange(1, 3, 3), eofLexeme},
    };
    const std::string expected =
        "KwIf \"if\" @0:2\n"
        "LeftParen \"(\" @2:3\n"
        "EndOfFile \"\" @3:3\n";
    context.expect(dumpTokens(tokens) == expected,
        "TL-LEX-013 simple token dump must match the exact stable format");
}

// Validates every named escape emitted by the dumper.
void testNamedEscapeDump(TestContext& context)
{
    const std::string lexeme{"\n\r\t\\\"", 5};
    const std::array tokens = {
        Token{TokenKind::Identifier, makeDumpRange(3, 10, 15), lexeme},
    };
    const std::string expected = R"(Identifier "\n\r\t\\\"" @10:15)" "\n";
    context.expect(dumpTokens(tokens) == expected,
        "TL-LEX-014 LF, CR, tab, slash, and quote escapes must be byte exact");
}

// Validates lowercase two-digit hexadecimal escapes for control and high bytes.
void testHexadecimalEscapeDump(TestContext& context)
{
    const std::string lexeme{
        static_cast<char>(0x00),
        static_cast<char>(0x1f),
        static_cast<char>(0x7f),
        static_cast<char>(0x80),
        static_cast<char>(0xff),
    };
    const std::array tokens = {
        Token{TokenKind::Identifier, makeDumpRange(8, 0, 5), lexeme},
    };
    const std::string expected = R"(Identifier "\x00\x1f\x7f\x80\xff" @0:5)" "\n";
    context.expect(dumpTokens(tokens) == expected,
        "TL-LEX-015 non-printable bytes must use lowercase two-digit hexadecimal escapes");
}

// Proves ordering while showing that FileId and TokenValue are intentionally absent.
void testDumpOrderingAndOmissions(TestContext& context)
{
    constexpr std::string_view integerLexeme = "42";
    constexpr std::string_view stringLexeme = "\"x\"";
    const std::array tokens = {
        Token{TokenKind::IntegerLiteral, makeDumpRange(7, 4, 6), integerLexeme, std::uint64_t{42}},
        Token{TokenKind::StringLiteral, makeDumpRange(99, 1, 4), stringLexeme, std::string{"x"}},
    };
    const std::string expected =
        "IntegerLiteral \"42\" @4:6\n"
        "StringLiteral \"\\\"x\\\"\" @1:4\n";
    context.expect(dumpTokens(tokens) == expected,
        "TL-LEX-016 dump must preserve order and omit FileId and TokenValue");
}
}

// Adds all CL-LEX-003 byte-stable token dump cases to the consolidated host registry.
void registerLexerTokenDumperModule(TestRegistry& registry)
{
    addTokenDumperCase(registry, "TL-LEX-013", testSimpleTokenDump);
    addTokenDumperCase(registry, "TL-LEX-014", testNamedEscapeDump);
    addTokenDumperCase(registry, "TL-LEX-015", testHexadecimalEscapeDump);
    addTokenDumperCase(registry, "TL-LEX-016", testDumpOrderingAndOmissions);
}
}
