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
#include <cstddef>
#include <cstdint>
#include <filesystem>
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

constexpr FileId kLexerFileId{403};

// Verifies an integer token's exact source identity, range, and uint64 alternative. Complexity is O(1).
void expectInteger(
    TestContext& context,
    const Token& token,
    std::string_view lexeme,
    std::uint32_t begin,
    std::uint64_t value,
    std::string_view caseId)
{
    const std::string prefix = std::string(caseId) + " integer '" + std::string(lexeme) + "'";
    context.expect(token.kind() == TokenKind::IntegerLiteral, prefix + " kind must match");
    context.expect(token.lexeme() == lexeme, prefix + " lexeme must match");
    context.expect(token.range().begin().fileId() == kLexerFileId, prefix + " file must match");
    context.expect(token.range().begin().offset() == begin, prefix + " begin offset must match");
    context.expect(token.range().end().offset() == begin + lexeme.size(), prefix + " end offset must match");
    const auto* actualValue = std::get_if<std::uint64_t>(&token.value());
    context.expect(actualValue != nullptr, prefix + " value alternative must be uint64");
    context.expect(actualValue != nullptr && *actualValue == value, prefix + " decoded value must match");
}

// Verifies one InvalidNumericLiteral diagnostic including range and exact stable text. Complexity is O(1).
void expectNumericDiagnostic(
    TestContext& context,
    const Diagnostic& diagnostic,
    std::uint32_t begin,
    std::uint32_t end,
    std::string_view message,
    std::string_view caseId)
{
    const std::string prefix = std::string(caseId) + " diagnostic";
    context.expect(diagnostic.id() == DiagnosticId::InvalidNumericLiteral, prefix + " id must match");
    context.expect(diagnostic.message() == message, prefix + " message must match exactly");
    context.expect(diagnostic.range().has_value(), prefix + " must carry a source range");
    if (diagnostic.range().has_value())
    {
        context.expect(diagnostic.range()->begin().offset() == begin, prefix + " begin offset must match");
        context.expect(diagnostic.range()->end().offset() == end, prefix + " end offset must match");
    }
}

// Registers decimal zero, one, and the maximum unsigned 64-bit value. Complexity is O(n).
void registerDecimalBoundaryCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-036", "CL-LEX-006", "Host.Lexer", "component", "PR", {"lexer", "integer", "decimal"},
        [](TestContext& context) {
            const std::string bytes = "0 1 18446744073709551615";
            SourceFile source(kLexerFileId, std::filesystem::path("TL-LEX-036.ns"), bytes);
            DiagnosticEngine diagnostics;
            Lexer lexer(source, diagnostics);
            const std::vector<Token> tokens = lexer.lexAll();
            context.expect(tokens.size() == 4, "TL-LEX-036 must emit three integers and EOF");
            if (tokens.size() == 4)
            {
                expectInteger(context, tokens[0], "0", 0, 0, "TL-LEX-036");
                expectInteger(context, tokens[1], "1", 2, 1, "TL-LEX-036");
                expectInteger(context, tokens[2], "18446744073709551615", 4, UINT64_MAX, "TL-LEX-036");
                context.expect(tokens[3].kind() == TokenKind::EndOfFile,
                    "TL-LEX-036 must terminate with EOF");
            }
            context.expect(diagnostics.diagnostics().empty(), "TL-LEX-036 must not emit diagnostics");
        },
    });
}

// Registers the first decimal value outside uint64 and its exact overflow diagnostic. Complexity is O(n).
void registerDecimalOverflowCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-037", "CL-LEX-006", "Host.Lexer", "component", "PR", {"lexer", "integer", "overflow"},
        [](TestContext& context) {
            const std::string bytes = "18446744073709551616";
            SourceFile source(kLexerFileId, std::filesystem::path("TL-LEX-037.ns"), bytes);
            DiagnosticEngine diagnostics;
            Lexer lexer(source, diagnostics);
            const std::vector<Token> tokens = lexer.lexAll();
            context.expect(tokens.size() == 2, "TL-LEX-037 must emit Invalid and EOF");
            if (tokens.size() == 2)
            {
                context.expect(tokens[0].kind() == TokenKind::Invalid && tokens[0].lexeme() == bytes,
                    "TL-LEX-037 must preserve the complete overflow literal");
                context.expect(tokens[1].kind() == TokenKind::EndOfFile,
                    "TL-LEX-037 must terminate with EOF");
            }
            context.expect(diagnostics.diagnostics().size() == 1,
                "TL-LEX-037 must emit one overflow diagnostic");
            if (diagnostics.diagnostics().size() == 1)
            {
                expectNumericDiagnostic(context, diagnostics.diagnostics()[0], 0, bytes.size(),
                    "integer literal is not representable as an unsigned 64-bit value", "TL-LEX-037");
            }
        },
    });
}

// Registers lowercase/uppercase hexadecimal prefixes and case-insensitive digits. Complexity is O(n).
void registerHexadecimalValuesCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-038", "CL-LEX-006", "Host.Lexer", "component", "PR", {"lexer", "integer", "hex"},
        [](TestContext& context) {
            SourceFile source(kLexerFileId, std::filesystem::path("TL-LEX-038.ns"), "0x0 0Xff 0xABC");
            DiagnosticEngine diagnostics;
            Lexer lexer(source, diagnostics);
            const std::vector<Token> tokens = lexer.lexAll();
            context.expect(tokens.size() == 4, "TL-LEX-038 must emit three integers and EOF");
            if (tokens.size() == 4)
            {
                expectInteger(context, tokens[0], "0x0", 0, 0, "TL-LEX-038");
                expectInteger(context, tokens[1], "0Xff", 4, 255, "TL-LEX-038");
                expectInteger(context, tokens[2], "0xABC", 9, 2748, "TL-LEX-038");
                context.expect(tokens[3].kind() == TokenKind::EndOfFile,
                    "TL-LEX-038 must terminate with EOF");
            }
            context.expect(diagnostics.diagnostics().empty(), "TL-LEX-038 must not emit diagnostics");
        },
    });
}

// Registers a hexadecimal prefix without digits and its exact recovery boundary. Complexity is O(1).
void registerMissingHexDigitCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-039", "CL-LEX-006", "Host.Lexer", "component", "PR", {"lexer", "integer", "hex"},
        [](TestContext& context) {
            SourceFile source(kLexerFileId, std::filesystem::path("TL-LEX-039.ns"), "0x");
            DiagnosticEngine diagnostics;
            Lexer lexer(source, diagnostics);
            const std::vector<Token> tokens = lexer.lexAll();
            context.expect(tokens.size() == 2 && tokens[0].kind() == TokenKind::Invalid,
                "TL-LEX-039 must emit Invalid and EOF");
            context.expect(diagnostics.diagnostics().size() == 1,
                "TL-LEX-039 must emit one missing-digit diagnostic");
            if (diagnostics.diagnostics().size() == 1)
            {
                expectNumericDiagnostic(context, diagnostics.diagnostics()[0], 0, 2,
                    "hexadecimal literal requires at least one hexadecimal digit", "TL-LEX-039");
            }
        },
    });
}

// Registers lowercase/uppercase binary prefixes and exact decoded values. Complexity is O(n).
void registerBinaryValuesCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-040", "CL-LEX-006", "Host.Lexer", "component", "PR", {"lexer", "integer", "binary"},
        [](TestContext& context) {
            SourceFile source(kLexerFileId, std::filesystem::path("TL-LEX-040.ns"), "0b0 0B1 0b1010");
            DiagnosticEngine diagnostics;
            Lexer lexer(source, diagnostics);
            const std::vector<Token> tokens = lexer.lexAll();
            context.expect(tokens.size() == 4, "TL-LEX-040 must emit three integers and EOF");
            if (tokens.size() == 4)
            {
                expectInteger(context, tokens[0], "0b0", 0, 0, "TL-LEX-040");
                expectInteger(context, tokens[1], "0B1", 4, 1, "TL-LEX-040");
                expectInteger(context, tokens[2], "0b1010", 8, 10, "TL-LEX-040");
                context.expect(tokens[3].kind() == TokenKind::EndOfFile,
                    "TL-LEX-040 must terminate with EOF");
            }
            context.expect(diagnostics.diagnostics().empty(), "TL-LEX-040 must not emit diagnostics");
        },
    });
}

// Registers a binary prefix without bits and its exact recovery boundary. Complexity is O(1).
void registerMissingBinaryDigitCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-041", "CL-LEX-006", "Host.Lexer", "component", "PR", {"lexer", "integer", "binary"},
        [](TestContext& context) {
            SourceFile source(kLexerFileId, std::filesystem::path("TL-LEX-041.ns"), "0b");
            DiagnosticEngine diagnostics;
            Lexer lexer(source, diagnostics);
            const std::vector<Token> tokens = lexer.lexAll();
            context.expect(tokens.size() == 2 && tokens[0].kind() == TokenKind::Invalid,
                "TL-LEX-041 must emit Invalid and EOF");
            context.expect(diagnostics.diagnostics().size() == 1,
                "TL-LEX-041 must emit one missing-bit diagnostic");
            if (diagnostics.diagnostics().size() == 1)
            {
                expectNumericDiagnostic(context, diagnostics.diagnostics()[0], 0, 2,
                    "binary literal requires at least one binary digit", "TL-LEX-041");
            }
        },
    });
}

// Registers a nonbinary decimal byte after a valid binary prefix and verifies token separation. Complexity O(n).
void registerBinaryBoundaryCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-042", "CL-LEX-006", "Host.Lexer", "component", "PR", {"lexer", "integer", "boundary"},
        [](TestContext& context) {
            SourceFile source(kLexerFileId, std::filesystem::path("TL-LEX-042.ns"), "0b102");
            DiagnosticEngine diagnostics;
            Lexer lexer(source, diagnostics);
            const std::vector<Token> tokens = lexer.lexAll();
            context.expect(tokens.size() == 3, "TL-LEX-042 must emit two integers and EOF");
            if (tokens.size() == 3)
            {
                expectInteger(context, tokens[0], "0b10", 0, 2, "TL-LEX-042");
                expectInteger(context, tokens[1], "2", 4, 2, "TL-LEX-042");
                context.expect(tokens[2].kind() == TokenKind::EndOfFile,
                    "TL-LEX-042 must terminate with EOF");
            }
            context.expect(diagnostics.diagnostics().empty(), "TL-LEX-042 must not emit diagnostics");
        },
    });
}

// Registers a nonhex identifier byte after a valid hexadecimal literal. Complexity is O(n).
void registerHexadecimalBoundaryCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-043", "CL-LEX-006", "Host.Lexer", "component", "PR", {"lexer", "integer", "boundary"},
        [](TestContext& context) {
            SourceFile source(kLexerFileId, std::filesystem::path("TL-LEX-043.ns"), "0x1g");
            DiagnosticEngine diagnostics;
            Lexer lexer(source, diagnostics);
            const std::vector<Token> tokens = lexer.lexAll();
            context.expect(tokens.size() == 3, "TL-LEX-043 must emit an integer, identifier, and EOF");
            if (tokens.size() == 3)
            {
                expectInteger(context, tokens[0], "0x1", 0, 1, "TL-LEX-043");
                context.expect(tokens[1].kind() == TokenKind::Identifier && tokens[1].lexeme() == "g",
                    "TL-LEX-043 must leave g for identifier scanning");
                context.expect(tokens[1].range().begin().offset() == 3 && tokens[1].range().end().offset() == 4,
                    "TL-LEX-043 identifier range must be 3:4");
                context.expect(tokens[2].kind() == TokenKind::EndOfFile,
                    "TL-LEX-043 must terminate with EOF");
            }
            context.expect(diagnostics.diagnostics().empty(), "TL-LEX-043 must not emit diagnostics");
        },
    });
}

// Registers each accepted single-letter integer suffix without changing the decoded value. Complexity is O(n).
void registerSingleSuffixCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-044", "CL-LEX-006", "Host.Lexer", "component", "PR", {"lexer", "integer", "suffix"},
        [](TestContext& context) {
            SourceFile source(kLexerFileId, std::filesystem::path("TL-LEX-044.ns"), "1u 2U 3l 4L");
            DiagnosticEngine diagnostics;
            Lexer lexer(source, diagnostics);
            const std::vector<Token> tokens = lexer.lexAll();
            constexpr std::array<std::string_view, 4> lexemes = {"1u", "2U", "3l", "4L"};
            constexpr std::array<std::uint32_t, 4> begins = {0, 3, 6, 9};
            context.expect(tokens.size() == 5, "TL-LEX-044 must emit four integers and EOF");
            if (tokens.size() == 5)
            {
                for (std::size_t index = 0; index < lexemes.size(); ++index)
                {
                    expectInteger(context, tokens[index], lexemes[index], begins[index], index + 1,
                        "TL-LEX-044");
                }
            }
            context.expect(diagnostics.diagnostics().empty(), "TL-LEX-044 must not emit diagnostics");
        },
    });
}

// Registers all accepted two-letter u/l orders and case combinations. Complexity is O(n).
void registerCombinedSuffixCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-045", "CL-LEX-006", "Host.Lexer", "component", "PR", {"lexer", "integer", "suffix"},
        [](TestContext& context) {
            const std::string bytes = "1ul 2uL 3Ul 4UL 5lu 6lU 7Lu 8LU";
            constexpr std::array<std::string_view, 8> lexemes = {
                "1ul", "2uL", "3Ul", "4UL", "5lu", "6lU", "7Lu", "8LU"};
            SourceFile source(kLexerFileId, std::filesystem::path("TL-LEX-045.ns"), bytes);
            DiagnosticEngine diagnostics;
            Lexer lexer(source, diagnostics);
            const std::vector<Token> tokens = lexer.lexAll();
            context.expect(tokens.size() == 9, "TL-LEX-045 must emit eight integers and EOF");
            if (tokens.size() == 9)
            {
                std::size_t searchFrom = 0;
                for (std::size_t index = 0; index < lexemes.size(); ++index)
                {
                    const auto begin = static_cast<std::uint32_t>(bytes.find(lexemes[index], searchFrom));
                    expectInteger(context, tokens[index], lexemes[index], begin, index + 1, "TL-LEX-045");
                    searchFrom = begin + lexemes[index].size();
                }
            }
            context.expect(diagnostics.diagnostics().empty(), "TL-LEX-045 must not emit diagnostics");
        },
    });
}

// Registers duplicate and overlong integer suffix runs and verifies complete invalid-run consumption. Complexity O(n).
void registerInvalidSuffixCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-046", "CL-LEX-006", "Host.Lexer", "component", "PR", {"lexer", "integer", "suffix"},
        [](TestContext& context) {
            const std::string bytes = "1uu 2ll 3uul 4lLuU";
            constexpr std::array<std::string_view, 4> lexemes = {"1uu", "2ll", "3uul", "4lLuU"};
            SourceFile source(kLexerFileId, std::filesystem::path("TL-LEX-046.ns"), bytes);
            DiagnosticEngine diagnostics;
            Lexer lexer(source, diagnostics);
            const std::vector<Token> tokens = lexer.lexAll();
            context.expect(tokens.size() == 5, "TL-LEX-046 must emit four Invalid tokens and EOF");
            context.expect(diagnostics.diagnostics().size() == 4,
                "TL-LEX-046 must diagnose every invalid suffix run");
            std::size_t searchFrom = 0;
            for (std::size_t index = 0; index < lexemes.size(); ++index)
            {
                const auto begin = static_cast<std::uint32_t>(bytes.find(lexemes[index], searchFrom));
                const auto end = static_cast<std::uint32_t>(begin + lexemes[index].size());
                if (tokens.size() == 5)
                {
                    context.expect(tokens[index].kind() == TokenKind::Invalid &&
                            tokens[index].lexeme() == lexemes[index],
                        "TL-LEX-046 must preserve each complete invalid suffix run");
                }
                if (diagnostics.diagnostics().size() == 4)
                {
                    expectNumericDiagnostic(context, diagnostics.diagnostics()[index], begin, end,
                        "invalid integer literal suffix", "TL-LEX-046");
                }
                searchFrom = end;
            }
        },
    });
}

// Registers an underscore boundary and verifies that separators are not part of integer grammar. Complexity O(n).
void registerUnderscoreBoundaryCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-047", "CL-LEX-006", "Host.Lexer", "component", "PR", {"lexer", "integer", "separator"},
        [](TestContext& context) {
            SourceFile source(kLexerFileId, std::filesystem::path("TL-LEX-047.ns"), "1_000");
            DiagnosticEngine diagnostics;
            Lexer lexer(source, diagnostics);
            const std::vector<Token> tokens = lexer.lexAll();
            context.expect(tokens.size() == 3, "TL-LEX-047 must emit an integer, identifier, and EOF");
            if (tokens.size() == 3)
            {
                expectInteger(context, tokens[0], "1", 0, 1, "TL-LEX-047");
                context.expect(tokens[1].kind() == TokenKind::Identifier && tokens[1].lexeme() == "_000",
                    "TL-LEX-047 must leave the underscore run for identifier scanning");
                context.expect(tokens[1].range().begin().offset() == 1 && tokens[1].range().end().offset() == 5,
                    "TL-LEX-047 identifier range must be 1:5");
                context.expect(tokens[2].kind() == TokenKind::EndOfFile,
                    "TL-LEX-047 must terminate with EOF");
            }
            context.expect(diagnostics.diagnostics().empty(), "TL-LEX-047 must not emit diagnostics");
        },
    });
}
}

// Registers CL-LEX-006 through exact token, value, diagnostic, and recovery observations.
void registerLexerIntegerModule(TestRegistry& registry)
{
    registerDecimalBoundaryCase(registry);
    registerDecimalOverflowCase(registry);
    registerHexadecimalValuesCase(registry);
    registerMissingHexDigitCase(registry);
    registerBinaryValuesCase(registry);
    registerMissingBinaryDigitCase(registry);
    registerBinaryBoundaryCase(registry);
    registerHexadecimalBoundaryCase(registry);
    registerSingleSuffixCase(registry);
    registerCombinedSuffixCase(registry);
    registerInvalidSuffixCase(registry);
    registerUnderscoreBoundaryCase(registry);
}
}
