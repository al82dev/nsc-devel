#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Basic/FileId.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Lexer/Lexer.h"
#include "nativesharp/Lexer/Token.h"
#include "nativesharp/Lexer/TokenKind.h"
#include "nativesharp/Source/SourceFile.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
using nativesharp::FileId;
using nativesharp::diagnostics::DiagnosticEngine;
using nativesharp::diagnostics::DiagnosticId;
using nativesharp::lexer::Lexer;
using nativesharp::lexer::Token;
using nativesharp::lexer::TokenKind;
using nativesharp::source::SourceFile;

constexpr FileId kLexerFileId{402};
using KeywordExpectation = std::pair<std::string_view, TokenKind>;

constexpr std::array<KeywordExpectation, 97> kKeywords = {{
    {"void", TokenKind::KwVoid}, {"bool", TokenKind::KwBool},
    {"sbyte", TokenKind::KwSByte}, {"byte", TokenKind::KwByte},
    {"char", TokenKind::KwChar}, {"short", TokenKind::KwShort},
    {"ushort", TokenKind::KwUShort}, {"int", TokenKind::KwInt},
    {"uint", TokenKind::KwUInt}, {"long", TokenKind::KwLong},
    {"ulong", TokenKind::KwULong}, {"nint", TokenKind::KwNInt},
    {"nuint", TokenKind::KwNUInt}, {"i8", TokenKind::KwSByte},
    {"u8", TokenKind::KwByte}, {"i16", TokenKind::KwShort},
    {"u16", TokenKind::KwUShort}, {"i32", TokenKind::KwInt},
    {"u32", TokenKind::KwUInt}, {"i64", TokenKind::KwLong},
    {"u64", TokenKind::KwULong}, {"isize", TokenKind::KwNInt},
    {"usize", TokenKind::KwNUInt}, {"i128", TokenKind::KwInt128},
    {"u128", TokenKind::KwUInt128}, {"float", TokenKind::KwFloat},
    {"double", TokenKind::KwDouble}, {"string", TokenKind::KwString},
    {"const", TokenKind::KwConst}, {"static", TokenKind::KwStatic},
    {"readonly", TokenKind::KwReadonly}, {"volatile", TokenKind::KwVolatile},
    {"inline", TokenKind::KwInline}, {"forceinline", TokenKind::KwForceInline},
    {"noinline", TokenKind::KwNoInline}, {"noheap", TokenKind::KwNoHeap},
    {"public", TokenKind::KwPublic}, {"internal", TokenKind::KwInternal},
    {"protected", TokenKind::KwProtected}, {"private", TokenKind::KwPrivate},
    {"if", TokenKind::KwIf}, {"else", TokenKind::KwElse},
    {"while", TokenKind::KwWhile}, {"do", TokenKind::KwDo},
    {"for", TokenKind::KwFor}, {"switch", TokenKind::KwSwitch},
    {"case", TokenKind::KwCase}, {"break", TokenKind::KwBreak},
    {"continue", TokenKind::KwContinue}, {"return", TokenKind::KwReturn},
    {"try", TokenKind::KwTry}, {"catch", TokenKind::KwCatch},
    {"finally", TokenKind::KwFinally}, {"throw", TokenKind::KwThrow},
    {"struct", TokenKind::KwStruct}, {"class", TokenKind::KwClass},
    {"interface", TokenKind::KwInterface}, {"enum", TokenKind::KwEnum},
    {"namespace", TokenKind::KwNamespace}, {"using", TokenKind::KwUsing},
    {"new", TokenKind::KwNew}, {"delete", TokenKind::KwDelete},
    {"destroy", TokenKind::KwDestroy}, {"stackalloc", TokenKind::KwStackalloc},
    {"sizeof", TokenKind::KwSizeof}, {"alignof", TokenKind::KwAlignof},
    {"offsetof", TokenKind::KwOffsetof}, {"static_assert", TokenKind::KwStaticAssert},
    {"default", TokenKind::KwDefault}, {"extern", TokenKind::KwExtern},
    {"export", TokenKind::KwExport}, {"vectorcall", TokenKind::KwVectorCall},
    {"true", TokenKind::KwTrue}, {"false", TokenKind::KwFalse},
    {"null", TokenKind::KwNull}, {"this", TokenKind::KwThis},
    {"base", TokenKind::KwBase}, {"virtual", TokenKind::KwVirtual},
    {"override", TokenKind::KwOverride}, {"abstract", TokenKind::KwAbstract},
    {"sealed", TokenKind::KwSealed}, {"get", TokenKind::KwGet},
    {"set", TokenKind::KwSet}, {"operator", TokenKind::KwOperator},
    {"implicit", TokenKind::KwImplicit}, {"explicit", TokenKind::KwExplicit},
    {"is", TokenKind::KwIs}, {"as", TokenKind::KwAs},
    {"delegate", TokenKind::KwDelegate}, {"event", TokenKind::KwEvent},
    {"raii", TokenKind::KwRaii}, {"move", TokenKind::KwMove},
    {"owned", TokenKind::KwOwned}, {"borrowed", TokenKind::KwBorrowed},
    {"var", TokenKind::KwVar}, {"foreach", TokenKind::KwForeach},
    {"in", TokenKind::KwIn},
}};

constexpr std::array<KeywordExpectation, 10> kIntegerAliases = {{
    {"i8", TokenKind::KwSByte}, {"u8", TokenKind::KwByte},
    {"i16", TokenKind::KwShort}, {"u16", TokenKind::KwUShort},
    {"i32", TokenKind::KwInt}, {"u32", TokenKind::KwUInt},
    {"i64", TokenKind::KwLong}, {"u64", TokenKind::KwULong},
    {"isize", TokenKind::KwNInt}, {"usize", TokenKind::KwNUInt},
}};

// Verifies one exact keyword or identifier spelling through the public scanner. Complexity is O(n).
void expectSingleToken(
    TestContext& context,
    std::string_view caseId,
    std::string_view spelling,
    TokenKind expectedKind)
{
    SourceFile source(kLexerFileId, std::filesystem::path(std::string(caseId) + ".ns"), std::string(spelling));
    DiagnosticEngine diagnostics;
    Lexer lexer(source, diagnostics);
    const std::vector<Token> tokens = lexer.lexAll();
    const std::string prefix = std::string(caseId) + " spelling '" + std::string(spelling) + "'";
    context.expect(tokens.size() == 2, prefix + " must emit one token and EOF");
    if (tokens.size() == 2)
    {
        context.expect(tokens[0].kind() == expectedKind, prefix + " kind must match");
        context.expect(tokens[0].lexeme() == spelling, prefix + " lexeme must match");
        context.expect(tokens[0].range().begin().offset() == 0, prefix + " begin offset must be zero");
        context.expect(tokens[0].range().end().offset() == spelling.size(), prefix + " end offset must match");
        context.expect(tokens[1].kind() == TokenKind::EndOfFile, prefix + " must terminate with EOF");
    }
    context.expect(diagnostics.diagnostics().empty(), prefix + " must not emit diagnostics");
}

// Registers the complete ASCII identifier-start and continuation sample with physical ranges. Complexity is O(n).
void registerIdentifierGrammarCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-028", "CL-LEX-005", "Host.Lexer", "component", "PR", {"lexer", "identifier", "ascii"},
        [](TestContext& context) {
            const std::string bytes = "_ A z a0 _9";
            SourceFile source(kLexerFileId, std::filesystem::path("TL-LEX-028.ns"), bytes);
            DiagnosticEngine diagnostics;
            Lexer lexer(source, diagnostics);
            const std::vector<Token> tokens = lexer.lexAll();
            constexpr std::array<std::string_view, 5> lexemes = {"_", "A", "z", "a0", "_9"};
            constexpr std::array<std::uint32_t, 5> begins = {0, 2, 4, 6, 9};
            context.expect(tokens.size() == 6, "TL-LEX-028 must emit five identifiers and EOF");
            if (tokens.size() == 6)
            {
                for (std::size_t index = 0; index < lexemes.size(); ++index)
                {
                    context.expect(tokens[index].kind() == TokenKind::Identifier,
                        "TL-LEX-028 every spelling must be an Identifier");
                    context.expect(tokens[index].lexeme() == lexemes[index],
                        "TL-LEX-028 every identifier lexeme must match");
                    context.expect(tokens[index].range().begin().offset() == begins[index] &&
                            tokens[index].range().end().offset() == begins[index] + lexemes[index].size(),
                        "TL-LEX-028 every identifier range must match physical bytes");
                }
                context.expect(tokens.back().kind() == TokenKind::EndOfFile,
                    "TL-LEX-028 must terminate with EOF");
            }
            context.expect(diagnostics.diagnostics().empty(), "TL-LEX-028 must not emit diagnostics");
        },
    });
}

// Registers a digit-led spelling and proves numeric scanning ends before identifier bytes. Complexity is O(n).
void registerDigitInitialCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-029", "CL-LEX-005", "Host.Lexer", "component", "PR", {"lexer", "identifier", "boundary"},
        [](TestContext& context) {
            SourceFile source(kLexerFileId, std::filesystem::path("TL-LEX-029.ns"), "1abc");
            DiagnosticEngine diagnostics;
            Lexer lexer(source, diagnostics);
            const std::vector<Token> tokens = lexer.lexAll();
            context.expect(tokens.size() == 3, "TL-LEX-029 must emit an integer, an identifier, and EOF");
            if (tokens.size() == 3)
            {
                context.expect(tokens[0].kind() == TokenKind::IntegerLiteral && tokens[0].lexeme() == "1",
                    "TL-LEX-029 the digit prefix must be an integer token");
                context.expect(tokens[0].range().begin().offset() == 0 && tokens[0].range().end().offset() == 1,
                    "TL-LEX-029 the integer range must be 0:1");
                context.expect(tokens[1].kind() == TokenKind::Identifier && tokens[1].lexeme() == "abc",
                    "TL-LEX-029 trailing ASCII letters must form an identifier");
                context.expect(tokens[1].range().begin().offset() == 1 && tokens[1].range().end().offset() == 4,
                    "TL-LEX-029 the identifier range must be 1:4");
                context.expect(tokens[2].kind() == TokenKind::EndOfFile,
                    "TL-LEX-029 must terminate with EOF");
            }
            context.expect(diagnostics.diagnostics().empty(), "TL-LEX-029 must not emit diagnostics");
        },
    });
}

// Registers a two-byte UTF-8 spelling and proves identifier grammar remains byte-oriented ASCII. Complexity is O(n).
void registerNonAsciiInitialCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-030", "CL-LEX-005", "Host.Lexer", "component", "PR", {"lexer", "identifier", "ascii"},
        [](TestContext& context) {
            const std::string bytes{static_cast<char>(0xc3), static_cast<char>(0xa9)};
            SourceFile source(kLexerFileId, std::filesystem::path("TL-LEX-030.ns"), bytes);
            DiagnosticEngine diagnostics;
            Lexer lexer(source, diagnostics);
            const std::vector<Token> tokens = lexer.lexAll();
            context.expect(tokens.size() == 3, "TL-LEX-030 must emit two Invalid bytes and EOF");
            if (tokens.size() == 3)
            {
                context.expect(tokens[0].kind() == TokenKind::Invalid && tokens[1].kind() == TokenKind::Invalid,
                    "TL-LEX-030 must not recognize a Unicode identifier");
                context.expect(tokens[0].lexeme() == std::string_view(bytes).substr(0, 1) &&
                        tokens[1].lexeme() == std::string_view(bytes).substr(1, 1),
                    "TL-LEX-030 must preserve each invalid source byte");
                context.expect(tokens[2].kind() == TokenKind::EndOfFile,
                    "TL-LEX-030 must terminate with EOF");
            }
            context.expect(diagnostics.diagnostics().size() == 2,
                "TL-LEX-030 must diagnose both unexpected bytes");
            for (std::size_t index = 0; index < diagnostics.diagnostics().size(); ++index)
            {
                const auto& diagnostic = diagnostics.diagnostics()[index];
                context.expect(diagnostic.id() == DiagnosticId::UnexpectedCharacter,
                    "TL-LEX-030 diagnostics must identify unexpected characters");
                context.expect(diagnostic.message() == "unexpected character in source",
                    "TL-LEX-030 diagnostics must preserve exact text");
            }
        },
    });
}

// Registers the independently curated complete keyword table and validates every exact spelling. Complexity O(k*n).
void registerKeywordTableCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-031", "CL-LEX-005", "Host.Lexer", "component", "PR", {"lexer", "keyword", "table"},
        [](TestContext& context) {
            for (const auto& [spelling, expectedKind] : kKeywords)
            {
                expectSingleToken(context, "TL-LEX-031", spelling, expectedKind);
            }
        },
    });
}

// Registers all integer aliases and proves they map to their canonical builtin token classes. Complexity O(k*n).
void registerIntegerAliasCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-032", "CL-LEX-005", "Host.Lexer", "component", "PR", {"lexer", "keyword", "alias"},
        [](TestContext& context) {
            for (const auto& [spelling, expectedKind] : kIntegerAliases)
            {
                expectSingleToken(context, "TL-LEX-032", spelling, expectedKind);
            }
        },
    });
}

// Registers case-changed keyword-like spellings and verifies exact case sensitivity. Complexity is O(k*n).
void registerKeywordCaseSensitivityCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-033", "CL-LEX-005", "Host.Lexer", "component", "PR", {"lexer", "keyword", "case"},
        [](TestContext& context) {
            constexpr std::array<std::string_view, 7> changed = {
                "Void", "BOOL", "Int", "PUBLIC", "Owned", "Foreach", "U8"};
            for (const std::string_view spelling : changed)
            {
                expectSingleToken(context, "TL-LEX-033", spelling, TokenKind::Identifier);
            }
        },
    });
}

// Registers contextual `C` and prevents it from becoming a reserved keyword. Complexity is O(1).
void registerContextualCCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-034", "CL-LEX-005", "Host.Lexer", "component", "PR", {"lexer", "keyword", "contextual"},
        [](TestContext& context) {
            expectSingleToken(context, "TL-LEX-034", "C", TokenKind::Identifier);
        },
    });
}

// Registers keyword prefixes and suffixes and verifies maximal identifier consumption. Complexity is O(k*n).
void registerKeywordAffixCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-035", "CL-LEX-005", "Host.Lexer", "component", "PR", {"lexer", "keyword", "affix"},
        [](TestContext& context) {
            constexpr std::array<std::string_view, 7> spellings = {
                "intx", "xint", "_int", "int_", "public2", "switchable", "borrowedValue"};
            for (const std::string_view spelling : spellings)
            {
                expectSingleToken(context, "TL-LEX-035", spelling, TokenKind::Identifier);
            }
        },
    });
}
}

// Registers CL-LEX-005 through public lexer outputs without duplicating private scanner entry points.
void registerLexerIdentifierModule(TestRegistry& registry)
{
    registerIdentifierGrammarCase(registry);
    registerDigitInitialCase(registry);
    registerNonAsciiInitialCase(registry);
    registerKeywordTableCase(registry);
    registerIntegerAliasCase(registry);
    registerKeywordCaseSensitivityCase(registry);
    registerContextualCCase(registry);
    registerKeywordAffixCase(registry);
}
}
