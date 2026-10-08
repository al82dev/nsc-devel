#include "nsc_test/Modules.h"

#include "nsc_test/TestContext.h"
#include "nsc_test/TestRegistry.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Lexer/Lexer.h"
#include "nativesharp/Lexer/TokenKind.h"
#include "nativesharp/Source/SourceManager.h"

#include <cstdint>
#include <string>
#include <vector>

namespace nsc::test
{
namespace
{
std::vector<nativesharp::lexer::Token> lex(
    std::string sourceText,
    nativesharp::diagnostics::DiagnosticEngine& diagnostics,
    nativesharp::source::SourceManager& sources)
{
    const nativesharp::FileId id = sources.addBuffer("lexer.ns", std::move(sourceText));
    const nativesharp::source::SourceFile* file = sources.getFile(id);
    nativesharp::lexer::Lexer lexer(*file, diagnostics);
    return lexer.lexAll();
}

void verifyLexer(TestContext& context)
{
    using namespace nativesharp;
    using namespace nativesharp::lexer;

    {
        diagnostics::DiagnosticEngine diagnostics;
        source::SourceManager sources;
        const std::string sourceText =
            "\xEF\xBB\xBFint Main() { // line\n"
            "  /* block */ uint x = 0xFFu; long y = 0b1010L;\n"
            "  float f = 2f; double d = 1.25e+2; char c = '\\x41';\n"
            "  char* s = \"A\\nB\"; x <<= 1; return x >= 1 && true || false; }";
        const std::vector<Token> tokens = lex(sourceText, diagnostics, sources);
        context.expect(!diagnostics.hasErrors(), "valid lexical corpus has no diagnostics");
        context.expect(
            !tokens.empty() && tokens.front().kind() == TokenKind::KwInt,
            "UTF-8 BOM is skipped");
        context.expect(
            tokens.back().kind() == TokenKind::EndOfFile,
            "lexer always emits EOF");

        bool sawHex = false;
        bool sawBinary = false;
        bool sawFloat = false;
        bool sawString = false;
        bool sawShiftAssign = false;
        bool sawLogicalOr = false;
        for (const Token& token : tokens)
        {
            if (token.kind() == TokenKind::IntegerLiteral && token.lexeme() == "0xFFu") sawHex = true;
            if (token.kind() == TokenKind::IntegerLiteral && token.lexeme() == "0b1010L") sawBinary = true;
            if (token.kind() == TokenKind::FloatingLiteral && token.lexeme() == "2f") sawFloat = true;
            if (token.kind() == TokenKind::StringLiteral) sawString = true;
            if (token.kind() == TokenKind::LessLessEqual) sawShiftAssign = true;
            if (token.kind() == TokenKind::PipePipe) sawLogicalOr = true;
        }
        context.expect(sawHex, "hexadecimal integer literal is one token");
        context.expect(sawBinary, "binary integer literal is one token");
        context.expect(sawFloat, "digits plus f suffix forms floating literal");
        context.expect(sawString, "string literal is recognized");
        context.expect(sawShiftAssign, "maximal munch recognizes <<=");
        context.expect(sawLogicalOr, "maximal munch recognizes ||");
    }

    {
        diagnostics::DiagnosticEngine diagnostics;
        source::SourceManager sources;
        const std::vector<Token> tokens = lex("1.X 1.0f ++ -- -> >>=", diagnostics, sources);
        context.expect(!diagnostics.hasErrors(), "operator maximal-munch sample is valid");
        context.expect(tokens.size() == 9, "operator sample produces expected token stream length");
        context.expect(tokens[0].kind() == TokenKind::IntegerLiteral, "1.X begins with integer literal");
        context.expect(tokens[1].kind() == TokenKind::Dot, "1.X keeps dot separate");
        context.expect(tokens[2].kind() == TokenKind::Identifier, "1.X ends with identifier");
        context.expect(tokens[3].kind() == TokenKind::FloatingLiteral, "1.0f is one floating token");
        context.expect(tokens[4].kind() == TokenKind::PlusPlus, "++ is one token");
        context.expect(tokens[5].kind() == TokenKind::MinusMinus, "-- is one token");
        context.expect(tokens[6].kind() == TokenKind::Arrow, "-> is one token");
        context.expect(tokens[7].kind() == TokenKind::GreaterGreaterEqual, ">>= is one token");
    }

    {
        diagnostics::DiagnosticEngine diagnostics;
        source::SourceManager sources;
        const std::vector<Token> tokens =
            lex("'A' '\\n' '\\xFF' \"x\\t\\x41\"", diagnostics, sources);
        context.expect(
            !diagnostics.hasErrors(),
            "valid character and string escapes lex successfully");
        context.expect(tokens[0].kind() == TokenKind::CharacterLiteral, "character literal token kind");
        context.expect(std::get<std::uint8_t>(tokens[0].value()) == 65, "character literal payload is decoded");
        context.expect(std::get<std::uint8_t>(tokens[1].value()) == 10, "simple escape payload is decoded");
        context.expect(std::get<std::uint8_t>(tokens[2].value()) == 255, "hex escape payload is decoded");
        context.expect(
            std::get<std::string>(tokens[3].value()) == std::string("x\tA"),
            "string escape payload is decoded");
    }

    {
        diagnostics::DiagnosticEngine diagnostics;
        source::SourceManager sources;
        (void)lex("/* unterminated", diagnostics, sources);
        context.expect(diagnostics.hasErrors(), "unterminated block comment is diagnosed");
        context.expect(
            diagnostics.diagnostics().front().id() ==
                diagnostics::DiagnosticId::UnterminatedBlockComment,
            "unterminated block comment uses dedicated diagnostic");
    }

    {
        diagnostics::DiagnosticEngine diagnostics;
        source::SourceManager sources;
        (void)lex("\"bad\\q\" @", diagnostics, sources);
        context.expect(
            diagnostics.errorCount() >= 2,
            "invalid escape and unexpected character are both diagnosed");
    }

    {
        diagnostics::DiagnosticEngine diagnostics;
        source::SourceManager sources;
        (void)lex("0x 0b 1e+ 42uuu", diagnostics, sources);
        context.expect(
            diagnostics.errorCount() == 4,
            "four malformed numeric literals produce four diagnostics");
        bool allNumeric = true;
        for (const diagnostics::Diagnostic& diagnostic : diagnostics.diagnostics())
        {
            allNumeric = allNumeric &&
                         diagnostic.id() == diagnostics::DiagnosticId::InvalidNumericLiteral;
        }
        context.expect(
            allNumeric,
            "malformed numeric literals use InvalidNumericLiteral diagnostic IDs");
    }

    {
        diagnostics::DiagnosticEngine diagnostics;
        source::SourceManager sources;
        const std::vector<Token> tokens =
            lex("if If extern C c class partial", diagnostics, sources);
        context.expect(
            !diagnostics.hasErrors(),
            "case-sensitivity keyword sample lexes without errors");
        context.expect(tokens[0].kind() == TokenKind::KwIf, "lowercase if is a keyword");
        context.expect(
            tokens[1].kind() == TokenKind::Identifier,
            "If is an identifier because keywords are case-sensitive");
        context.expect(
            tokens[2].kind() == TokenKind::KwExtern &&
                tokens[3].kind() == TokenKind::Identifier && tokens[3].lexeme() == "C",
            "C remains an identifier for contextual extern C parsing");
        context.expect(tokens[4].kind() == TokenKind::Identifier, "lowercase c is not the C linkage keyword");
        context.expect(tokens[5].kind() == TokenKind::KwClass, "class remains a reserved type keyword");
        context.expect(
            tokens[6].kind() == TokenKind::Identifier,
            "partial remains lexical Identifier because it is a contextual keyword");
    }

    {
        diagnostics::DiagnosticEngine diagnostics;
        source::SourceManager sources;
        const std::string allKeywords =
            "void bool byte char short ushort int uint long ulong nint nuint float double const public internal protected private "
            "if else while for break continue return struct class enum namespace using new delete destroy stackalloc sizeof alignof offsetof static_assert "
            "extern true false null this";
        const std::vector<Token> tokens = lex(allKeywords, diagnostics, sources);
        const std::vector<TokenKind> expected = {
            TokenKind::KwVoid, TokenKind::KwBool, TokenKind::KwByte, TokenKind::KwChar,
            TokenKind::KwShort, TokenKind::KwUShort, TokenKind::KwInt, TokenKind::KwUInt,
            TokenKind::KwLong, TokenKind::KwULong, TokenKind::KwNInt, TokenKind::KwNUInt,
            TokenKind::KwFloat, TokenKind::KwDouble, TokenKind::KwConst, TokenKind::KwPublic,
            TokenKind::KwInternal, TokenKind::KwProtected, TokenKind::KwPrivate, TokenKind::KwIf,
            TokenKind::KwElse, TokenKind::KwWhile, TokenKind::KwFor, TokenKind::KwBreak,
            TokenKind::KwContinue, TokenKind::KwReturn, TokenKind::KwStruct, TokenKind::KwClass,
            TokenKind::KwEnum, TokenKind::KwNamespace, TokenKind::KwUsing, TokenKind::KwNew,
            TokenKind::KwDelete, TokenKind::KwDestroy, TokenKind::KwStackalloc,
            TokenKind::KwSizeof, TokenKind::KwAlignof, TokenKind::KwOffsetof,
            TokenKind::KwStaticAssert, TokenKind::KwExtern, TokenKind::KwTrue,
            TokenKind::KwFalse, TokenKind::KwNull, TokenKind::KwThis, TokenKind::EndOfFile};
        bool matches = tokens.size() == expected.size();
        for (std::size_t index = 0; matches && index < expected.size(); ++index)
        {
            matches = tokens[index].kind() == expected[index];
        }
        context.expect(
            !diagnostics.hasErrors() && matches,
            "complete reserved-keyword set maps to exact token kinds");
    }

    {
        diagnostics::DiagnosticEngine diagnostics;
        source::SourceManager sources;
        const std::string punctuationAndOperators =
            "( ) { } [ ] ; , . -> + - * / % ++ -- == != < <= > >= ! && || & | ^ ~ << >> "
            "= += -= *= /= %= &= |= ^= <<= >>=";
        const std::vector<Token> tokens = lex(punctuationAndOperators, diagnostics, sources);
        const std::vector<TokenKind> expected = {
            TokenKind::LeftParen, TokenKind::RightParen, TokenKind::LeftBrace,
            TokenKind::RightBrace, TokenKind::LeftBracket, TokenKind::RightBracket,
            TokenKind::Semicolon, TokenKind::Comma, TokenKind::Dot, TokenKind::Arrow,
            TokenKind::Plus, TokenKind::Minus, TokenKind::Star, TokenKind::Slash,
            TokenKind::Percent, TokenKind::PlusPlus, TokenKind::MinusMinus,
            TokenKind::EqualEqual, TokenKind::BangEqual, TokenKind::Less,
            TokenKind::LessEqual, TokenKind::Greater, TokenKind::GreaterEqual,
            TokenKind::Bang, TokenKind::AmpAmp, TokenKind::PipePipe, TokenKind::Amp,
            TokenKind::Pipe, TokenKind::Caret, TokenKind::Tilde, TokenKind::LessLess,
            TokenKind::GreaterGreater, TokenKind::Equal, TokenKind::PlusEqual,
            TokenKind::MinusEqual, TokenKind::StarEqual, TokenKind::SlashEqual,
            TokenKind::PercentEqual, TokenKind::AmpEqual, TokenKind::PipeEqual,
            TokenKind::CaretEqual, TokenKind::LessLessEqual,
            TokenKind::GreaterGreaterEqual, TokenKind::EndOfFile};
        bool matches = tokens.size() == expected.size();
        for (std::size_t index = 0; matches && index < expected.size(); ++index)
        {
            matches = tokens[index].kind() == expected[index];
        }
        context.expect(
            !diagnostics.hasErrors() && matches,
            "complete punctuation/operator set maps to exact token kinds");
    }
}
}

void registerLexerUnitTests(TestRegistry& registry)
{
    registry.add(TestCase{
        "nsc.lexer",
        "lexer",
        "lexer tokens and diagnostics",
        {"tokens", "literals", "operators", "diagnostics"},
        std::chrono::milliseconds(1000),
        verifyLexer,
    });
}
}
