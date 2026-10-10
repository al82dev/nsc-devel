#include "ParserFinalTestSupport.h"

#include "nativesharp/Lexer/Lexer.h"
#include "nativesharp/Source/SourceManager.h"

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace parser_final_support;
using namespace parser_support;
using namespace nativesharp::ast;
using nativesharp::lexer::TokenKind;

void testParserBorrowsCollaborators(TestContext& context)
{
    nativesharp::source::SourceManager sources;
    nativesharp::diagnostics::DiagnosticEngine diagnostics;
    const nativesharp::FileId id = sources.addBuffer("borrowed.ns", "value");
    const nativesharp::source::SourceFile* file = sources.getFile(id);
    nativesharp::lexer::Lexer lexer(*file, diagnostics);
    std::vector<nativesharp::lexer::Token> tokens = lexer.lexAll();
    AstArena arena;
    Parser parser(tokens, arena, diagnostics);
    AstNode* expression = parser.parseExpression();
    context.expect(parser.tokens_.data() == tokens.data() && &parser.arena_ == &arena &&
            &parser.diagnostics_ == &diagnostics && expression != nullptr && expression->text() == "value",
        "TP-PAR-525 Parser must borrow live tokens/arena/engine without copying or owning them");
}

void testIndependentArenasOverSharedTokens(TestContext& context)
{
    nativesharp::source::SourceManager sources;
    nativesharp::diagnostics::DiagnosticEngine diagnosticsA;
    nativesharp::diagnostics::DiagnosticEngine diagnosticsB;
    const nativesharp::FileId id = sources.addBuffer("shared.ns", "left + right");
    const nativesharp::source::SourceFile* file = sources.getFile(id);
    nativesharp::lexer::Lexer lexer(*file, diagnosticsA);
    const std::vector<nativesharp::lexer::Token> tokens = lexer.lexAll();
    AstArena arenaA;
    AstArena arenaB;
    Parser first(tokens, arenaA, diagnosticsA);
    Parser second(tokens, arenaB, diagnosticsB);
    AstNode* left = first.parseExpression();
    AstNode* right = second.parseExpression();
    context.expect(left != nullptr && right != nullptr && left != right && left->kind() == right->kind() &&
            left->tokenKind() == right->tokenKind() && left->first() != right->first() &&
            left->first()->text() == right->first()->text() && left->second()->text() == right->second()->text(),
        "TP-PAR-526 parsers over shared tokens must build equivalent trees in distinct arenas without aliases");
}

void testMatchCounter(TestContext& context)
{
    ParserFixture fixture(
        "match (first) { _ => 1 } match (second) { _ => 2 }");
    AstNode* first = fixture.parser().parseExpression();
    AstNode* second = fixture.parser().parseExpression();
    context.expect(first != nullptr && second != nullptr && first->text() == "$__nsc_match_expr_0" &&
            second->text() == "$__nsc_match_expr_1" && fixture.parser().nextMatchTemporary_ == 2,
        "TP-PAR-527 match temporaries must be monotonic and deterministic within one parser");
}

void testDeferCounterAndLifo(TestContext& context)
{
    ParserFixture fixture(
        "{ int first = 1; int second = 2; "
        "defer [move first] { consume(first); } "
        "defer [move second] { consume(second); } work(); }");
    AstNode* block = fixture.parser().parseBlock();
    const AstNode* firstHidden = firstText(block, "__nsc_defer_capture_0_first");
    const AstNode* secondHidden = firstText(block, "__nsc_defer_capture_1_second");
    const auto nodes = flatten(block);
    std::size_t lowered = 0;
    for (const AstNode* node : nodes) lowered += node->auxiliaryText() == "defer-lowered" ? 1U : 0U;
    context.expect(firstHidden != nullptr && secondHidden != nullptr && lowered == 2 &&
            fixture.parser().nextDeferTemporary_ == 2 &&
            firstKind(block, AstNodeKind::TryStatement) != nullptr,
        "TP-PAR-528 defer temporaries must be monotonic and generated try regions must nest for LIFO cleanup");
}

void testComprehensionCounterAndContext(TestContext& context)
{
    ParserFixture fixture(
        "comprehend(first) { yield return 1; } comprehend(second) { yield return 2; }");
    AstNode* first = fixture.parser().parseExpression();
    context.expect(fixture.parser().comprehensionContexts_.empty(),
        "TP-PAR-529 first comprehension must pop its temporary parse context");
    AstNode* second = fixture.parser().parseExpression();
    context.expect(firstText(first, "$__comprehension_builder_0") != nullptr &&
            firstText(second, "$__comprehension_builder_1") != nullptr &&
            fixture.parser().nextComprehensionTemporary_ == 2 &&
            fixture.parser().comprehensionContexts_.empty(),
        "TP-PAR-529 comprehension names must be exact and every pushed context must be popped");
}

void testExtensionFlagRestoredAfterError(TestContext& context)
{
    ParserFixture fixture("extension int { 123; } int after;");
    AstNode* root = fixture.parse();
    context.expect(root != nullptr && fixture.diagnostics().hasErrors() &&
            !fixture.parser().parsingExtensionBody_ && firstText(root, "after") != nullptr,
        "TP-PAR-530 an erroneous extension body must restore extension context before later declarations");
}

void testPendingGenericGreaterDoesNotLeak(TestContext& context)
{
    ParserFixture fixture("class Broken<T>> { } int after;");
    AstNode* root = fixture.parse();
    context.expect(root != nullptr && firstText(root, "after") != nullptr &&
            fixture.parser().pendingGenericGreater_ == 0 && fixture.parser().current().kind() == TokenKind::EndOfFile,
        "TP-PAR-531 generic-close recovery must not contaminate the following declaration");
}

void testPracticalNestingLimit(TestContext& context)
{
    std::string unary(256, '!');
    unary += "true";
    ParserFixture unaryFixture(unary);
    AstNode* unaryRoot = unaryFixture.parser().parseExpression();
    std::size_t unaryCount = 0;
    for (const AstNode* node : flatten(unaryRoot))
        unaryCount += node->kind() == AstNodeKind::UnaryExpression ? 1U : 0U;

    std::string type = "int";
    for (std::size_t index = 0; index < 64; ++index) type = "Box<" + type + ">";
    ParserFixture typeFixture(type);
    TypeSyntax* parsedType = typeFixture.parser().parseType();

    std::string pattern;
    pattern.append(64, '(');
    pattern += "0";
    for (std::size_t index = 0; index < 64; ++index) pattern += ",)";
    ParserFixture patternFixture(pattern);
    const Parser::Pattern parsedPattern = patternFixture.parser().parsePattern(false);

    context.expect(unaryRoot != nullptr && unaryCount == 256 && parsedType != nullptr &&
            parsedPattern.kind == Parser::PatternKind::Tuple,
        "TP-PAR-532 type, pattern, and unary nesting must terminate deterministically at the practical depth");
}

void testDeepCloneAndFloatingMode(TestContext& context)
{
    std::string nested(128, '!');
    nested += "flag";
    ParserFixture cloneFixture(
        "match (source) { Left(var value) | Right(var value) => " + nested + ", _ => false }");
    AstNode* cloned = cloneFixture.parser().parseExpression();

    ParserFixture fpFixture("double F() fp(strict) { return " + nested + "; }");
    AstNode* fpRoot = fpFixture.parse();
    const AstNode* function = firstKind(fpRoot, AstNodeKind::FunctionDeclaration);
    bool fpApplied = function != nullptr && function->first() != nullptr;
    for (const AstNode* node : flatten(function != nullptr ? function->first() : nullptr))
        fpApplied = fpApplied && node->floatingPointMode() == FloatingPointModeSyntaxKind::Strict;
    context.expect(cloned != nullptr && fpApplied,
        "TP-PAR-533 deep clone and FP propagation must complete at the characterized practical depth of 128");
}

void testDelimiterOnlyProgress(TestContext& context)
{
    ParserFixture fixture("))))]]]]}}}}{{{{[[[((;;,,");
    AstNode* root = fixture.parse();
    context.expect(root != nullptr && fixture.parser().current().kind() == TokenKind::EndOfFile &&
            fixture.diagnostics().hasErrors(),
        "TP-PAR-534 delimiter-only adversarial input must make finite progress to EOF");
}

void testExhaustedDiagnosticBudgetProgress(TestContext& context)
{
    nativesharp::source::SourceManager sources;
    nativesharp::diagnostics::DiagnosticEngine diagnostics(1);
    const std::string source(256, ';');
    const nativesharp::FileId id = sources.addBuffer("budget.ns", source);
    const nativesharp::source::SourceFile* file = sources.getFile(id);
    nativesharp::lexer::Lexer lexer(*file, diagnostics);
    const std::vector<nativesharp::lexer::Token> tokens = lexer.lexAll();
    AstArena arena;
    Parser parser(tokens, arena, diagnostics);
    AstNode* root = parser.parseCompilationUnit();
    context.expect(root != nullptr && parser.current().kind() == TokenKind::EndOfFile &&
            diagnostics.errorLimitReached() && diagnostics.suppressedDiagnosticCount() > 0,
        "TP-PAR-535 Parser must terminate after the diagnostic engine exhausts its message budget");
}

void testDeterministicDelimiterCorpus(TestContext& context)
{
    static constexpr std::array<std::string_view, 16> corpus = {
        "()", "{}", "[]", "(]", "{)", "[}", "(((())))", "{{{{}}}}",
        "([{}])", ")]}", "<<<>>>", "class C {{{", "int f( {", "match ((( {", ";;;;", "{[()]}}"};
    bool allFinite = true;
    for (const std::string_view input : corpus)
    {
        ParserFixture fixture{std::string(input)};
        AstNode* root = fixture.parse();
        allFinite = allFinite && root != nullptr && fixture.parser().current().kind() == TokenKind::EndOfFile;
        for (const AstNode* node : flatten(root))
        {
            allFinite = allFinite && node->range().begin().offset() <= input.size() &&
                node->range().end().offset() <= input.size();
        }
        for (const auto& diagnostic : fixture.diagnostics().diagnostics())
        {
            if (!diagnostic.range().has_value()) continue;
            allFinite = allFinite && diagnostic.range()->begin().offset() <= input.size() &&
                diagnostic.range()->end().offset() <= input.size();
        }
    }
    context.expect(allFinite,
        "TP-PAR-536 deterministic balanced/unbalanced delimiter corpus must terminate with bounded ranges");
}
}

void registerParserStateRobustnessFinalModule(TestRegistry& registry)
{
    addFinalParserCase(registry, "TP-PAR-525", "CP-PAR-042", testParserBorrowsCollaborators);
    addFinalParserCase(registry, "TP-PAR-526", "CP-PAR-042", testIndependentArenasOverSharedTokens);
    addFinalParserCase(registry, "TP-PAR-527", "CP-PAR-042", testMatchCounter);
    addFinalParserCase(registry, "TP-PAR-528", "CP-PAR-042", testDeferCounterAndLifo);
    addFinalParserCase(registry, "TP-PAR-529", "CP-PAR-042", testComprehensionCounterAndContext);
    addFinalParserCase(registry, "TP-PAR-530", "CP-PAR-042", testExtensionFlagRestoredAfterError);
    addFinalParserCase(registry, "TP-PAR-531", "CP-PAR-042", testPendingGenericGreaterDoesNotLeak);
    addFinalParserCase(registry, "TP-PAR-532", "CP-PAR-042", testPracticalNestingLimit, "NIGHTLY");
    addFinalParserCase(registry, "TP-PAR-533", "CP-PAR-042", testDeepCloneAndFloatingMode, "NIGHTLY");
    addFinalParserCase(registry, "TP-PAR-534", "CP-PAR-042", testDelimiterOnlyProgress);
    addFinalParserCase(registry, "TP-PAR-535", "CP-PAR-042", testExhaustedDiagnosticBudgetProgress);
    addFinalParserCase(registry, "TP-PAR-536", "CP-PAR-042", testDeterministicDelimiterCorpus, "NIGHTLY");
}
}
