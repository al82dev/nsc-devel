#include "LexerTestSupport.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Lexer/Lexer.h"
#include "nativesharp/Source/SourceFile.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

namespace nsc::suite
{
namespace
{
using lexer_test::LexerFixture;
using nativesharp::FileId;
using nativesharp::diagnostics::DiagnosticEngine;
using nativesharp::lexer::Lexer;
using nativesharp::lexer::TokenKind;
using nativesharp::source::SourceFile;

// Registers one contextual-assembly case with CL-LEX-013 traceability metadata. Complexity is O(1).
void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CL-LEX-013", "Host.Lexer", "component", "PR",
        {"lexer", "assembly", "contextual"}, std::move(function)});
}

// Checks one token from an already bounded sequence. Complexity is O(1).
void expectToken(TestContext& context,
                 const LexerFixture& fixture,
                 std::size_t index,
                 TokenKind kind,
                 std::uint32_t begin,
                 std::uint32_t end,
                 std::string_view lexeme,
                 std::string_view caseId)
{
    lexer_test::expectToken(context, fixture.tokens()[index], kind, begin, end, lexeme, caseId);
}
}

void registerLexerAssemblyModule(TestRegistry& registry)
{
    addCase(registry, "TL-LEX-101", [](TestContext& context) {
        const LexerFixture fixture("asm {x}");
        const auto& tokens = fixture.tokens();
        context.expect(tokens.size() == 5, "TL-LEX-101 must emit asm, brace, raw text, brace, and EOF");
        if (tokens.size() != 5) return;
        expectToken(context, fixture, 0, TokenKind::Identifier, 0, 3, "asm", "TL-LEX-101");
        expectToken(context, fixture, 1, TokenKind::LeftBrace, 4, 5, "{", "TL-LEX-101");
        expectToken(context, fixture, 2, TokenKind::AssemblyText, 5, 6, "x", "TL-LEX-101");
        expectToken(context, fixture, 3, TokenKind::RightBrace, 6, 7, "}", "TL-LEX-101");
        expectToken(context, fixture, 4, TokenKind::EndOfFile, 7, 7, "", "TL-LEX-101");
    });

    addCase(registry, "TL-LEX-102", [](TestContext& context) {
        const LexerFixture fixture("asm /* gap */ \t {x}");
        const auto& tokens = fixture.tokens();
        context.expect(tokens.size() == 5,
            "TL-LEX-102 trivia and comments between asm and brace must preserve assembly activation");
        if (tokens.size() != 5) return;
        expectToken(context, fixture, 0, TokenKind::Identifier, 0, 3, "asm", "TL-LEX-102");
        expectToken(context, fixture, 1, TokenKind::LeftBrace, 16, 17, "{", "TL-LEX-102");
        expectToken(context, fixture, 2, TokenKind::AssemblyText, 17, 18, "x", "TL-LEX-102");
        expectToken(context, fixture, 3, TokenKind::RightBrace, 18, 19, "}", "TL-LEX-102");
        expectToken(context, fixture, 4, TokenKind::EndOfFile, 19, 19, "", "TL-LEX-102");
    });

    addCase(registry, "TL-LEX-103", [](TestContext& context) {
        const LexerFixture fixture("asm value {x}");
        const auto& tokens = fixture.tokens();
        context.expect(tokens.size() == 6, "TL-LEX-103 a real token must cancel assembly activation");
        if (tokens.size() != 6) return;
        expectToken(context, fixture, 0, TokenKind::Identifier, 0, 3, "asm", "TL-LEX-103");
        expectToken(context, fixture, 1, TokenKind::Identifier, 4, 9, "value", "TL-LEX-103");
        expectToken(context, fixture, 2, TokenKind::LeftBrace, 10, 11, "{", "TL-LEX-103");
        expectToken(context, fixture, 3, TokenKind::Identifier, 11, 12, "x", "TL-LEX-103");
        expectToken(context, fixture, 4, TokenKind::RightBrace, 12, 13, "}", "TL-LEX-103");
        expectToken(context, fixture, 5, TokenKind::EndOfFile, 13, 13, "", "TL-LEX-103");
    });

    addCase(registry, "TL-LEX-104", [](TestContext& context) {
        const LexerFixture fixture("asm");
        const auto& tokens = fixture.tokens();
        context.expect(tokens.size() == 2, "TL-LEX-104 asm at EOF must terminate normally");
        if (tokens.size() != 2) return;
        expectToken(context, fixture, 0, TokenKind::Identifier, 0, 3, "asm", "TL-LEX-104");
        expectToken(context, fixture, 1, TokenKind::EndOfFile, 3, 3, "", "TL-LEX-104");
        context.expect(fixture.diagnostics().diagnostics().empty(), "TL-LEX-104 must not diagnose missing brace");
    });

    addCase(registry, "TL-LEX-105", [](TestContext& context) {
        const LexerFixture fixture("asm {a\nb\rc\r\nd}");
        const auto& tokens = fixture.tokens();
        context.expect(tokens.size() == 11, "TL-LEX-105 must preserve LF, CR, and CRLF as distinct records");
        if (tokens.size() != 11) return;
        expectToken(context, fixture, 2, TokenKind::AssemblyText, 5, 6, "a", "TL-LEX-105");
        expectToken(context, fixture, 3, TokenKind::AssemblyNewLine, 6, 7, "\n", "TL-LEX-105");
        expectToken(context, fixture, 4, TokenKind::AssemblyText, 7, 8, "b", "TL-LEX-105");
        expectToken(context, fixture, 5, TokenKind::AssemblyNewLine, 8, 9, "\r", "TL-LEX-105");
        expectToken(context, fixture, 6, TokenKind::AssemblyText, 9, 10, "c", "TL-LEX-105");
        expectToken(context, fixture, 7, TokenKind::AssemblyNewLine, 10, 12, "\r\n", "TL-LEX-105");
        expectToken(context, fixture, 8, TokenKind::AssemblyText, 12, 13, "d", "TL-LEX-105");
        expectToken(context, fixture, 9, TokenKind::RightBrace, 13, 14, "}", "TL-LEX-105");
        expectToken(context, fixture, 10, TokenKind::EndOfFile, 14, 14, "", "TL-LEX-105");
    });

    addCase(registry, "TL-LEX-106", [](TestContext& context) {
        constexpr std::string_view body = "  mov rax, 1 // raw \"quote\"  ";
        const std::string source = std::string("asm {") + std::string(body) + "}";
        const LexerFixture fixture(source);
        const auto& tokens = fixture.tokens();
        context.expect(tokens.size() == 5, "TL-LEX-106 must preserve one raw physical-line fragment");
        if (tokens.size() != 5) return;
        expectToken(context, fixture, 2, TokenKind::AssemblyText, 5,
            static_cast<std::uint32_t>(5 + body.size()), body, "TL-LEX-106");
        context.expect(fixture.diagnostics().diagnostics().empty(),
            "TL-LEX-106 assembly comments and quotes must not be lexed as language trivia");
    });

    addCase(registry, "TL-LEX-107", [](TestContext& context) {
        const LexerFixture fixture("asm {outer {inner} tail}");
        const auto& tokens = fixture.tokens();
        context.expect(tokens.size() == 5, "TL-LEX-107 nested braces must remain in one raw fragment");
        if (tokens.size() != 5) return;
        expectToken(context, fixture, 2, TokenKind::AssemblyText, 5, 23,
            "outer {inner} tail", "TL-LEX-107");
        expectToken(context, fixture, 3, TokenKind::RightBrace, 23, 24, "}", "TL-LEX-107");
    });

    addCase(registry, "TL-LEX-108", [](TestContext& context) {
        const LexerFixture quotedFixture("asm {\"{\" } tail}");
        const auto& quotedTokens = quotedFixture.tokens();
        context.expect(quotedTokens.size() == 5,
            "TL-LEX-108 a brace in an assembly quote must affect raw brace depth");
        if (quotedTokens.size() == 5)
        {
            lexer_test::expectToken(context, quotedTokens[2], TokenKind::AssemblyText,
                5, 15, "\"{\" } tail", "TL-LEX-108");
        }

        const LexerFixture commentFixture("asm {//{ } tail}");
        const auto& commentTokens = commentFixture.tokens();
        context.expect(commentTokens.size() == 5,
            "TL-LEX-108 a brace in an assembly comment must affect raw brace depth");
        if (commentTokens.size() == 5)
        {
            lexer_test::expectToken(context, commentTokens[2], TokenKind::AssemblyText,
                5, 15, "//{ } tail", "TL-LEX-108");
        }
    });

    addCase(registry, "TL-LEX-109", [](TestContext& context) {
        SourceFile source(FileId(7001), std::filesystem::path("phase2-lexer.ns"), "asm {fragment}");
        DiagnosticEngine diagnostics;
        Lexer lexer(source, diagnostics);
        const auto asmToken = lexer.nextToken();
        const auto leftBrace = lexer.nextToken();
        const auto text = lexer.nextToken();
        const auto rightBrace = lexer.nextToken();
        lexer_test::expectToken(context, asmToken, TokenKind::Identifier, 0, 3, "asm", "TL-LEX-109");
        lexer_test::expectToken(context, leftBrace, TokenKind::LeftBrace, 4, 5, "{", "TL-LEX-109");
        lexer_test::expectToken(context, text, TokenKind::AssemblyText, 5, 13, "fragment", "TL-LEX-109");
        lexer_test::expectToken(context, rightBrace, TokenKind::RightBrace, 13, 14, "}", "TL-LEX-109");
    });

    addCase(registry, "TL-LEX-110", [](TestContext& context) {
        const LexerFixture fixture("asm {unterminated");
        const auto& tokens = fixture.tokens();
        context.expect(tokens.size() == 4, "TL-LEX-110 must emit raw text and EOF inside an open body");
        if (tokens.size() != 4) return;
        expectToken(context, fixture, 2, TokenKind::AssemblyText, 5, 17,
            "unterminated", "TL-LEX-110");
        expectToken(context, fixture, 3, TokenKind::EndOfFile, 17, 17, "", "TL-LEX-110");
        context.expect(fixture.diagnostics().diagnostics().empty(),
            "TL-LEX-110 must not invent an assembly-specific EOF diagnostic");
    });
}
}
