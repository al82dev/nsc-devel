#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Basic/FileId.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Lexer/Lexer.h"
#include "nativesharp/Lexer/Token.h"
#include "nativesharp/Lexer/TokenKind.h"
#include "nativesharp/Source/SourceFile.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace nsc::suite
{
namespace
{
using nativesharp::FileId;
using nativesharp::diagnostics::DiagnosticEngine;
using nativesharp::lexer::Lexer;
using nativesharp::lexer::Token;
using nativesharp::lexer::TokenKind;
using nativesharp::source::SourceFile;

constexpr FileId kLexerFileId{401};

// Verifies one token's public identity, source view, and physical half-open byte range. Complexity is O(1).
void expectToken(
    TestContext& context,
    const Token& token,
    TokenKind kind,
    std::string_view lexeme,
    std::uint32_t begin,
    std::uint32_t end,
    std::string_view caseId)
{
    const std::string prefix = std::string(caseId) + " token";
    context.expect(token.kind() == kind, prefix + " kind must match");
    context.expect(token.lexeme() == lexeme, prefix + " lexeme must preserve source bytes");
    context.expect(token.range().begin().fileId() == kLexerFileId, prefix + " begin file must match");
    context.expect(token.range().end().fileId() == kLexerFileId, prefix + " end file must match");
    context.expect(token.range().begin().offset() == begin, prefix + " begin offset must match");
    context.expect(token.range().end().offset() == end, prefix + " end offset must match");
}

// Registers the empty-source contract: one stable EOF at the zero-width physical range. Complexity is O(1).
void registerEmptySourceCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-017", "CL-LEX-004", "Host.Lexer", "component", "PR", {"lexer", "cursor", "eof"},
        [](TestContext& context) {
            SourceFile source(kLexerFileId, std::filesystem::path("TL-LEX-017.ns"), "");
            DiagnosticEngine diagnostics;
            Lexer lexer(source, diagnostics);
            const std::vector<Token> tokens = lexer.lexAll();
            context.expect(tokens.size() == 1, "TL-LEX-017 lexAll must emit exactly one token");
            if (tokens.size() == 1)
            {
                expectToken(context, tokens[0], TokenKind::EndOfFile, {}, 0, 0, "TL-LEX-017");
            }
            context.expect(diagnostics.diagnostics().empty(), "TL-LEX-017 must not emit diagnostics");
        },
    });
}

// Registers the initial UTF-8 BOM contract and keeps EOF offsets in physical source bytes. Complexity is O(1).
void registerInitialBomCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-018", "CL-LEX-004", "Host.Lexer", "component", "PR", {"lexer", "cursor", "bom"},
        [](TestContext& context) {
            const std::string bytes{static_cast<char>(0xef), static_cast<char>(0xbb), static_cast<char>(0xbf)};
            SourceFile source(kLexerFileId, std::filesystem::path("TL-LEX-018.ns"), bytes);
            DiagnosticEngine diagnostics;
            Lexer lexer(source, diagnostics);
            const std::vector<Token> tokens = lexer.lexAll();
            context.expect(tokens.size() == 1, "TL-LEX-018 lexAll must emit exactly one EOF");
            if (tokens.size() == 1)
            {
                expectToken(context, tokens[0], TokenKind::EndOfFile, {}, 3, 3, "TL-LEX-018");
            }
            context.expect(diagnostics.diagnostics().empty(), "TL-LEX-018 must not emit diagnostics");
        },
    });
}

// Registers the noninitial-BOM path, where every high-bit byte remains ordinary invalid input. Complexity is O(1).
void registerNoninitialBomCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-019", "CL-LEX-004", "Host.Lexer", "component", "PR", {"lexer", "cursor", "bom"},
        [](TestContext& context) {
            const std::string bytes{'x', static_cast<char>(0xef), static_cast<char>(0xbb),
                static_cast<char>(0xbf)};
            SourceFile source(kLexerFileId, std::filesystem::path("TL-LEX-019.ns"), bytes);
            DiagnosticEngine diagnostics;
            Lexer lexer(source, diagnostics);
            const std::vector<Token> tokens = lexer.lexAll();
            context.expect(tokens.size() == 5, "TL-LEX-019 must emit one identifier, three invalid bytes, and EOF");
            if (tokens.size() == 5)
            {
                expectToken(context, tokens[0], TokenKind::Identifier, "x", 0, 1, "TL-LEX-019");
                expectToken(context, tokens[1], TokenKind::Invalid, std::string_view(bytes).substr(1, 1), 1, 2,
                    "TL-LEX-019");
                expectToken(context, tokens[2], TokenKind::Invalid, std::string_view(bytes).substr(2, 1), 2, 3,
                    "TL-LEX-019");
                expectToken(context, tokens[3], TokenKind::Invalid, std::string_view(bytes).substr(3, 1), 3, 4,
                    "TL-LEX-019");
                expectToken(context, tokens[4], TokenKind::EndOfFile, {}, 4, 4, "TL-LEX-019");
            }
            context.expect(diagnostics.diagnostics().size() == 3,
                "TL-LEX-019 must diagnose each noninitial BOM byte independently");
        },
    });
}

// Registers every normal whitespace byte and verifies the next token's physical offset. Complexity is O(n).
void registerWhitespaceCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-020", "CL-LEX-004", "Host.Lexer", "component", "PR", {"lexer", "trivia", "whitespace"},
        [](TestContext& context) {
            const std::string bytes = " \t\v\f\r\nx";
            SourceFile source(kLexerFileId, std::filesystem::path("TL-LEX-020.ns"), bytes);
            DiagnosticEngine diagnostics;
            Lexer lexer(source, diagnostics);
            const std::vector<Token> tokens = lexer.lexAll();
            context.expect(tokens.size() == 2, "TL-LEX-020 must omit every whitespace byte");
            if (tokens.size() == 2)
            {
                expectToken(context, tokens[0], TokenKind::Identifier, "x", 6, 7, "TL-LEX-020");
                expectToken(context, tokens[1], TokenKind::EndOfFile, {}, 7, 7, "TL-LEX-020");
            }
            context.expect(diagnostics.diagnostics().empty(), "TL-LEX-020 must not emit diagnostics");
        },
    });
}

// Registers a line comment terminated by LF and verifies that both comment and LF are trivia. Complexity is O(n).
void registerLfLineCommentCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-021", "CL-LEX-004", "Host.Lexer", "component", "PR", {"lexer", "trivia", "comment"},
        [](TestContext& context) {
            const std::string bytes = "//text\nx";
            SourceFile source(kLexerFileId, std::filesystem::path("TL-LEX-021.ns"), bytes);
            DiagnosticEngine diagnostics;
            Lexer lexer(source, diagnostics);
            const std::vector<Token> tokens = lexer.lexAll();
            context.expect(tokens.size() == 2, "TL-LEX-021 must omit the comment and LF");
            if (tokens.size() == 2)
            {
                expectToken(context, tokens[0], TokenKind::Identifier, "x", 7, 8, "TL-LEX-021");
                expectToken(context, tokens[1], TokenKind::EndOfFile, {}, 8, 8, "TL-LEX-021");
            }
            context.expect(diagnostics.diagnostics().empty(), "TL-LEX-021 must not emit diagnostics");
        },
    });
}

// Registers a line comment terminated by CRLF and preserves both physical terminator bytes. Complexity is O(n).
void registerCrLfLineCommentCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-022", "CL-LEX-004", "Host.Lexer", "component", "PR", {"lexer", "trivia", "comment"},
        [](TestContext& context) {
            const std::string bytes = "//text\r\nx";
            SourceFile source(kLexerFileId, std::filesystem::path("TL-LEX-022.ns"), bytes);
            DiagnosticEngine diagnostics;
            Lexer lexer(source, diagnostics);
            const std::vector<Token> tokens = lexer.lexAll();
            context.expect(tokens.size() == 2, "TL-LEX-022 must omit the comment and CRLF");
            if (tokens.size() == 2)
            {
                expectToken(context, tokens[0], TokenKind::Identifier, "x", 8, 9, "TL-LEX-022");
                expectToken(context, tokens[1], TokenKind::EndOfFile, {}, 9, 9, "TL-LEX-022");
            }
            context.expect(diagnostics.diagnostics().empty(), "TL-LEX-022 must not emit diagnostics");
        },
    });
}

// Registers a closed block comment and verifies that no comment token escapes trivia. Complexity is O(n).
void registerBlockCommentCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-023", "CL-LEX-004", "Host.Lexer", "component", "PR", {"lexer", "trivia", "comment"},
        [](TestContext& context) {
            const std::string bytes = "/*text*/x";
            SourceFile source(kLexerFileId, std::filesystem::path("TL-LEX-023.ns"), bytes);
            DiagnosticEngine diagnostics;
            Lexer lexer(source, diagnostics);
            const std::vector<Token> tokens = lexer.lexAll();
            context.expect(tokens.size() == 2, "TL-LEX-023 must omit the closed block comment");
            if (tokens.size() == 2)
            {
                expectToken(context, tokens[0], TokenKind::Identifier, "x", 8, 9, "TL-LEX-023");
                expectToken(context, tokens[1], TokenKind::EndOfFile, {}, 9, 9, "TL-LEX-023");
            }
            context.expect(diagnostics.diagnostics().empty(), "TL-LEX-023 must not emit diagnostics");
        },
    });
}

// Registers a nested-looking block comment and proves that the first closing pair terminates it. Complexity is O(n).
void registerNonNestedBlockCommentCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-024", "CL-LEX-004", "Host.Lexer", "component", "PR", {"lexer", "trivia", "comment"},
        [](TestContext& context) {
            const std::string bytes = "/* outer /* inner */ x";
            const auto xOffset = static_cast<std::uint32_t>(bytes.find_last_of('x'));
            SourceFile source(kLexerFileId, std::filesystem::path("TL-LEX-024.ns"), bytes);
            DiagnosticEngine diagnostics;
            Lexer lexer(source, diagnostics);
            const std::vector<Token> tokens = lexer.lexAll();
            context.expect(tokens.size() == 2, "TL-LEX-024 must stop the comment at its first closing pair");
            if (tokens.size() == 2)
            {
                expectToken(context, tokens[0], TokenKind::Identifier, "x", xOffset, xOffset + 1,
                    "TL-LEX-024");
                expectToken(context, tokens[1], TokenKind::EndOfFile, {}, xOffset + 1, xOffset + 1,
                    "TL-LEX-024");
            }
            context.expect(diagnostics.diagnostics().empty(), "TL-LEX-024 must not emit diagnostics");
        },
    });
}

// Registers alternating trivia forms and verifies that only real tokens remain in order. Complexity is O(n).
void registerAlternatingTriviaCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-025", "CL-LEX-004", "Host.Lexer", "component", "PR", {"lexer", "trivia", "sequence"},
        [](TestContext& context) {
            const std::string bytes = " \tfoo/*c*/\r\nbar//x\nbaz ";
            SourceFile source(kLexerFileId, std::filesystem::path("TL-LEX-025.ns"), bytes);
            DiagnosticEngine diagnostics;
            Lexer lexer(source, diagnostics);
            const std::vector<Token> tokens = lexer.lexAll();
            context.expect(tokens.size() == 4, "TL-LEX-025 must emit three identifiers and one EOF");
            if (tokens.size() == 4)
            {
                const auto foo = static_cast<std::uint32_t>(bytes.find("foo"));
                const auto bar = static_cast<std::uint32_t>(bytes.find("bar"));
                const auto baz = static_cast<std::uint32_t>(bytes.find("baz"));
                expectToken(context, tokens[0], TokenKind::Identifier, "foo", foo, foo + 3, "TL-LEX-025");
                expectToken(context, tokens[1], TokenKind::Identifier, "bar", bar, bar + 3, "TL-LEX-025");
                expectToken(context, tokens[2], TokenKind::Identifier, "baz", baz, baz + 3, "TL-LEX-025");
                expectToken(context, tokens[3], TokenKind::EndOfFile, {}, bytes.size(), bytes.size(),
                    "TL-LEX-025");
            }
            context.expect(diagnostics.diagnostics().empty(), "TL-LEX-025 must not emit diagnostics");
        },
    });
}

// Registers repeated nextToken calls after exhaustion and proves EOF is stable without cursor advance. Complexity O(1).
void registerRepeatedEofCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-026", "CL-LEX-004", "Host.Lexer", "component", "PR", {"lexer", "cursor", "eof"},
        [](TestContext& context) {
            SourceFile source(kLexerFileId, std::filesystem::path("TL-LEX-026.ns"), "x");
            DiagnosticEngine diagnostics;
            Lexer lexer(source, diagnostics);
            const Token identifier = lexer.nextToken();
            const Token firstEof = lexer.nextToken();
            const Token secondEof = lexer.nextToken();
            expectToken(context, identifier, TokenKind::Identifier, "x", 0, 1, "TL-LEX-026");
            expectToken(context, firstEof, TokenKind::EndOfFile, {}, 1, 1, "TL-LEX-026");
            expectToken(context, secondEof, TokenKind::EndOfFile, {}, 1, 1, "TL-LEX-026");
            context.expect(diagnostics.diagnostics().empty(), "TL-LEX-026 must not emit diagnostics");
        },
    });
}

// Registers lexAll ordering and its exactly-one-terminal-EOF invariant. Complexity is O(n).
void registerLexAllOrderCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TL-LEX-027", "CL-LEX-004", "Host.Lexer", "component", "PR", {"lexer", "cursor", "sequence"},
        [](TestContext& context) {
            SourceFile source(kLexerFileId, std::filesystem::path("TL-LEX-027.ns"), "alpha + 7");
            DiagnosticEngine diagnostics;
            Lexer lexer(source, diagnostics);
            const std::vector<Token> tokens = lexer.lexAll();
            context.expect(tokens.size() == 4, "TL-LEX-027 must emit three tokens and one EOF");
            if (tokens.size() == 4)
            {
                expectToken(context, tokens[0], TokenKind::Identifier, "alpha", 0, 5, "TL-LEX-027");
                expectToken(context, tokens[1], TokenKind::Plus, "+", 6, 7, "TL-LEX-027");
                expectToken(context, tokens[2], TokenKind::IntegerLiteral, "7", 8, 9, "TL-LEX-027");
                expectToken(context, tokens[3], TokenKind::EndOfFile, {}, 9, 9, "TL-LEX-027");
            }
            context.expect(diagnostics.diagnostics().empty(), "TL-LEX-027 must not emit diagnostics");
        },
    });
}
}

// Registers CL-LEX-004 without exposing private cursor helpers or altering lexer behavior.
void registerLexerCursorTriviaModule(TestRegistry& registry)
{
    registerEmptySourceCase(registry);
    registerInitialBomCase(registry);
    registerNoninitialBomCase(registry);
    registerWhitespaceCase(registry);
    registerLfLineCommentCase(registry);
    registerCrLfLineCommentCase(registry);
    registerBlockCommentCase(registry);
    registerNonNestedBlockCommentCase(registry);
    registerAlternatingTriviaCase(registry);
    registerRepeatedEofCase(registry);
    registerLexAllOrderCase(registry);
}
}
