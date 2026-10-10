#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Basic/FileId.h"
#include "nativesharp/Diagnostics/Diagnostic.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Lexer/Lexer.h"
#include "nativesharp/Lexer/Token.h"
#include "nativesharp/Lexer/TokenKind.h"
#include "nativesharp/Source/SourceFile.h"

#include <array>
#include <clocale>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <locale.h>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace nsc::suite
{
namespace
{
using nativesharp::FileId;
using nativesharp::diagnostics::Diagnostic;
using nativesharp::diagnostics::DiagnosticEngine;
using nativesharp::diagnostics::DiagnosticId;
using nativesharp::lexer::Lexer;
using nativesharp::lexer::Token;
using nativesharp::lexer::TokenKind;
using nativesharp::source::SourceFile;

constexpr FileId kLexerFileId{404};

// Verifies one floating token's kind, source range, double alternative, and value tolerance. Complexity is O(1).
void expectFloating(
    TestContext& context,
    const Token& token,
    std::string_view lexeme,
    std::uint32_t begin,
    double value,
    double tolerance,
    std::string_view caseId)
{
    const std::string prefix = std::string(caseId) + " floating '" + std::string(lexeme) + "'";
    context.expect(token.kind() == TokenKind::FloatingLiteral, prefix + " kind must match");
    context.expect(token.lexeme() == lexeme, prefix + " lexeme must include every consumed byte");
    context.expect(token.range().begin().fileId() == kLexerFileId, prefix + " file must match");
    context.expect(token.range().begin().offset() == begin, prefix + " begin offset must match");
    context.expect(token.range().end().offset() == begin + lexeme.size(), prefix + " end offset must match");
    const auto* actualValue = std::get_if<double>(&token.value());
    context.expect(actualValue != nullptr, prefix + " value alternative must be double");
    context.expect(actualValue != nullptr && std::abs(*actualValue - value) <= tolerance,
        prefix + " decoded value must match");
}

// Verifies one malformed exponent diagnostic including its exact range and text. Complexity is O(1).
void expectExponentDiagnostic(
    TestContext& context,
    const Diagnostic& diagnostic,
    std::uint32_t begin,
    std::uint32_t end,
    std::string_view caseId)
{
    const std::string prefix = std::string(caseId) + " diagnostic";
    context.expect(diagnostic.id() == DiagnosticId::InvalidNumericLiteral, prefix + " id must match");
    context.expect(diagnostic.message() == "floating literal exponent requires decimal digits",
        prefix + " message must match exactly");
    context.expect(diagnostic.range().has_value(), prefix + " must carry a source range");
    if (diagnostic.range().has_value())
    {
        context.expect(diagnostic.range()->begin().offset() == begin, prefix + " begin offset must match");
        context.expect(diagnostic.range()->end().offset() == end, prefix + " end offset must match");
    }
}

// Registers ordinary decimal fractions whose expected values are exactly representable. Complexity is O(n).
void registerDecimalFractionCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-048", "CL-LEX-007", "Host.Lexer", "component", "PR", {"lexer", "floating", "fraction"},
        [](TestContext& context) {
            SourceFile source(kLexerFileId, std::filesystem::path("TL-LEX-048.ns"), "1.0 0.5 12.25");
            DiagnosticEngine diagnostics;
            Lexer lexer(source, diagnostics);
            const std::vector<Token> tokens = lexer.lexAll();
            context.expect(tokens.size() == 4, "TL-LEX-048 must emit three floating tokens and EOF");
            if (tokens.size() == 4)
            {
                expectFloating(context, tokens[0], "1.0", 0, 1.0, 0.0, "TL-LEX-048");
                expectFloating(context, tokens[1], "0.5", 4, 0.5, 0.0, "TL-LEX-048");
                expectFloating(context, tokens[2], "12.25", 8, 12.25, 0.0, "TL-LEX-048");
                context.expect(tokens[3].kind() == TokenKind::EndOfFile,
                    "TL-LEX-048 must terminate with EOF");
            }
            context.expect(diagnostics.diagnostics().empty(), "TL-LEX-048 must not emit diagnostics");
        },
    });
}

// Registers a dot without a following digit and verifies integer-plus-punctuation tokenization. Complexity O(n).
void registerTrailingDotCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-049", "CL-LEX-007", "Host.Lexer", "component", "PR", {"lexer", "floating", "boundary"},
        [](TestContext& context) {
            SourceFile source(kLexerFileId, std::filesystem::path("TL-LEX-049.ns"), "1.");
            DiagnosticEngine diagnostics;
            Lexer lexer(source, diagnostics);
            const std::vector<Token> tokens = lexer.lexAll();
            context.expect(tokens.size() == 3, "TL-LEX-049 must emit an integer, Dot, and EOF");
            if (tokens.size() == 3)
            {
                context.expect(tokens[0].kind() == TokenKind::IntegerLiteral && tokens[0].lexeme() == "1",
                    "TL-LEX-049 must keep 1 as an integer");
                context.expect(tokens[0].range().begin().offset() == 0 && tokens[0].range().end().offset() == 1,
                    "TL-LEX-049 integer range must be 0:1");
                context.expect(tokens[1].kind() == TokenKind::Dot && tokens[1].lexeme() == ".",
                    "TL-LEX-049 must leave the dot as punctuation");
                context.expect(tokens[1].range().begin().offset() == 1 && tokens[1].range().end().offset() == 2,
                    "TL-LEX-049 dot range must be 1:2");
                context.expect(tokens[2].kind() == TokenKind::EndOfFile,
                    "TL-LEX-049 must terminate with EOF");
            }
            context.expect(diagnostics.diagnostics().empty(), "TL-LEX-049 must not emit diagnostics");
        },
    });
}

// Registers unsigned, negative-sign, and positive-sign decimal exponents. Complexity is O(n).
void registerExponentValuesCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-050", "CL-LEX-007", "Host.Lexer", "component", "PR", {"lexer", "floating", "exponent"},
        [](TestContext& context) {
            SourceFile source(kLexerFileId, std::filesystem::path("TL-LEX-050.ns"), "1e2 1E-2 1e+2");
            DiagnosticEngine diagnostics;
            Lexer lexer(source, diagnostics);
            const std::vector<Token> tokens = lexer.lexAll();
            context.expect(tokens.size() == 4, "TL-LEX-050 must emit three floating tokens and EOF");
            if (tokens.size() == 4)
            {
                expectFloating(context, tokens[0], "1e2", 0, 100.0, 0.0, "TL-LEX-050");
                expectFloating(context, tokens[1], "1E-2", 4, 0.01, 1.0e-15, "TL-LEX-050");
                expectFloating(context, tokens[2], "1e+2", 9, 100.0, 0.0, "TL-LEX-050");
                context.expect(tokens[3].kind() == TokenKind::EndOfFile,
                    "TL-LEX-050 must terminate with EOF");
            }
            context.expect(diagnostics.diagnostics().empty(), "TL-LEX-050 must not emit diagnostics");
        },
    });
}

// Registers exponents without digits, including a consumed sign, and exact diagnostics. Complexity is O(n).
void registerMissingExponentDigitCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-051", "CL-LEX-007", "Host.Lexer", "component", "PR", {"lexer", "floating", "exponent"},
        [](TestContext& context) {
            SourceFile source(kLexerFileId, std::filesystem::path("TL-LEX-051.ns"), "1e 1e+");
            DiagnosticEngine diagnostics;
            Lexer lexer(source, diagnostics);
            const std::vector<Token> tokens = lexer.lexAll();
            context.expect(tokens.size() == 3, "TL-LEX-051 must emit two Invalid tokens and EOF");
            if (tokens.size() == 3)
            {
                context.expect(tokens[0].kind() == TokenKind::Invalid && tokens[0].lexeme() == "1e",
                    "TL-LEX-051 must preserve the first malformed exponent");
                context.expect(tokens[1].kind() == TokenKind::Invalid && tokens[1].lexeme() == "1e+",
                    "TL-LEX-051 must preserve the signed malformed exponent");
                context.expect(tokens[2].kind() == TokenKind::EndOfFile,
                    "TL-LEX-051 must terminate with EOF");
            }
            context.expect(diagnostics.diagnostics().size() == 2,
                "TL-LEX-051 must diagnose both malformed exponents");
            if (diagnostics.diagnostics().size() == 2)
            {
                expectExponentDiagnostic(context, diagnostics.diagnostics()[0], 0, 2, "TL-LEX-051");
                expectExponentDiagnostic(context, diagnostics.diagnostics()[1], 3, 6, "TL-LEX-051");
            }
        },
    });
}

// Registers f/F suffixes on integer-shaped, fraction, and exponent spellings. Complexity is O(n).
void registerFloatingSuffixCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-052", "CL-LEX-007", "Host.Lexer", "component", "PR", {"lexer", "floating", "suffix"},
        [](TestContext& context) {
            SourceFile source(kLexerFileId, std::filesystem::path("TL-LEX-052.ns"), "1f 1F 1.5f 1e2f");
            DiagnosticEngine diagnostics;
            Lexer lexer(source, diagnostics);
            const std::vector<Token> tokens = lexer.lexAll();
            context.expect(tokens.size() == 5, "TL-LEX-052 must emit four floating tokens and EOF");
            if (tokens.size() == 5)
            {
                expectFloating(context, tokens[0], "1f", 0, 1.0, 0.0, "TL-LEX-052");
                expectFloating(context, tokens[1], "1F", 3, 1.0, 0.0, "TL-LEX-052");
                expectFloating(context, tokens[2], "1.5f", 6, 1.5, 0.0, "TL-LEX-052");
                expectFloating(context, tokens[3], "1e2f", 11, 100.0, 0.0, "TL-LEX-052");
                context.expect(tokens[4].kind() == TokenKind::EndOfFile,
                    "TL-LEX-052 must terminate with EOF");
            }
            context.expect(diagnostics.diagnostics().empty(), "TL-LEX-052 must not emit diagnostics");
        },
    });
}

// Registers a leading dot and verifies that floating scanning requires a digit-led entry. Complexity is O(n).
void registerLeadingDotCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-053", "CL-LEX-007", "Host.Lexer", "component", "PR", {"lexer", "floating", "boundary"},
        [](TestContext& context) {
            SourceFile source(kLexerFileId, std::filesystem::path("TL-LEX-053.ns"), ".5");
            DiagnosticEngine diagnostics;
            Lexer lexer(source, diagnostics);
            const std::vector<Token> tokens = lexer.lexAll();
            context.expect(tokens.size() == 3, "TL-LEX-053 must emit Dot, integer, and EOF");
            if (tokens.size() == 3)
            {
                context.expect(tokens[0].kind() == TokenKind::Dot && tokens[0].lexeme() == ".",
                    "TL-LEX-053 must emit the leading dot independently");
                context.expect(tokens[0].range().begin().offset() == 0 && tokens[0].range().end().offset() == 1,
                    "TL-LEX-053 dot range must be 0:1");
                context.expect(tokens[1].kind() == TokenKind::IntegerLiteral && tokens[1].lexeme() == "5",
                    "TL-LEX-053 must emit the trailing digit as an integer");
                context.expect(tokens[1].range().begin().offset() == 1 && tokens[1].range().end().offset() == 2,
                    "TL-LEX-053 integer range must be 1:2");
                context.expect(tokens[2].kind() == TokenKind::EndOfFile,
                    "TL-LEX-053 must terminate with EOF");
            }
            context.expect(diagnostics.diagnostics().empty(), "TL-LEX-053 must not emit diagnostics");
        },
    });
}

// Registers strtod overflow and characterizes the current positive-infinity result. Complexity is O(n).
void registerFloatingOverflowCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-054", "CL-LEX-007", "Host.Lexer", "component", "PR", {"lexer", "floating", "overflow"},
        [](TestContext& context) {
            SourceFile source(kLexerFileId, std::filesystem::path("TL-LEX-054.ns"), "1e309");
            DiagnosticEngine diagnostics;
            Lexer lexer(source, diagnostics);
            const std::vector<Token> tokens = lexer.lexAll();
            context.expect(tokens.size() == 2, "TL-LEX-054 must emit one floating token and EOF");
            if (tokens.size() == 2)
            {
                context.expect(tokens[0].kind() == TokenKind::FloatingLiteral && tokens[0].lexeme() == "1e309",
                    "TL-LEX-054 must preserve the overflow spelling as a floating token");
                const auto* value = std::get_if<double>(&tokens[0].value());
                context.expect(value != nullptr, "TL-LEX-054 value alternative must be double");
                context.expect(value != nullptr && std::isinf(*value) && *value > 0.0,
                    "TL-LEX-054 current strtod contract must yield positive infinity");
                context.expect(tokens[1].kind() == TokenKind::EndOfFile,
                    "TL-LEX-054 must terminate with EOF");
            }
            context.expect(diagnostics.diagnostics().empty(), "TL-LEX-054 must not emit diagnostics");
        },
    });
}

// Registers the process-locale sensitivity of strtod using thread-local UCRT locale state. Complexity is O(k+n).
void registerDecimalLocaleCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-055", "CL-LEX-007", "Host.Lexer", "component", "NIGHTLY", {"lexer", "floating", "locale"},
        [](TestContext& context) {
            const int previousMode = _configthreadlocale(_ENABLE_PER_THREAD_LOCALE);
            const char* previousNamePointer = std::setlocale(LC_NUMERIC, nullptr);
            const std::string previousName = previousNamePointer == nullptr ? "C" : previousNamePointer;
            constexpr std::array<const char*, 3> candidates = {
                "de-DE", "German_Germany.1252", "deu_deu.1252"};
            const char* selected = nullptr;
            for (const char* candidate : candidates)
            {
                if (std::setlocale(LC_NUMERIC, candidate) != nullptr)
                {
                    selected = candidate;
                    break;
                }
            }
            context.expect(selected != nullptr,
                "TL-LEX-055 requires one deterministic comma-decimal UCRT locale");
            if (selected != nullptr)
            {
                SourceFile source(kLexerFileId, std::filesystem::path("TL-LEX-055.ns"), "1.5");
                DiagnosticEngine diagnostics;
                Lexer lexer(source, diagnostics);
                const std::vector<Token> tokens = lexer.lexAll();
                context.expect(tokens.size() == 2 && tokens[0].kind() == TokenKind::FloatingLiteral,
                    "TL-LEX-055 scanner shape must remain one floating token and EOF");
                if (tokens.size() == 2)
                {
                    const auto* value = std::get_if<double>(&tokens[0].value());
                    context.expect(value != nullptr && *value == 1.0,
                        "TL-LEX-055 comma-decimal strtod must currently stop at the source dot");
                    context.expect(tokens[0].lexeme() == "1.5",
                        "TL-LEX-055 lexer must still consume the complete source spelling");
                }
                context.expect(diagnostics.diagnostics().empty(),
                    "TL-LEX-055 current locale-sensitive conversion must not emit diagnostics");
            }
            std::setlocale(LC_NUMERIC, previousName.c_str());
            _configthreadlocale(previousMode);
        },
    });
}
}

// Registers CL-LEX-007 through exact scanner outputs while isolating locale mutation to the current thread.
void registerLexerFloatingModule(TestRegistry& registry)
{
    registerDecimalFractionCase(registry);
    registerTrailingDotCase(registry);
    registerExponentValuesCase(registry);
    registerMissingExponentDigitCase(registry);
    registerFloatingSuffixCase(registry);
    registerLeadingDotCase(registry);
    registerFloatingOverflowCase(registry);
    registerDecimalLocaleCase(registry);
}
}
