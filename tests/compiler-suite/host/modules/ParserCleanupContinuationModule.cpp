#include "ParserFoundationTestSupport.h"

#include <string>
#include <utility>

namespace nsc::suite
{
namespace
{
using namespace parser_support;
using namespace nativesharp::ast;
using nativesharp::diagnostics::DiagnosticId;

void addCase(TestRegistry& registry, std::string id, TestFunction function,
    TestDisposition disposition = TestDisposition::Required)
{
    registry.add(TestCase{std::move(id), "CP-PAR-035", "Host.Parser", "component", "PR",
        {"parser", "statement", "cleanup"}, std::move(function), disposition});
}

AstNode* parseStatement(ParserFixture& fixture)
{
    return fixture.parser().parseStatement();
}

const AstNode* loweredDefer(const AstNode* registration)
{
    if (registration == nullptr) return nullptr;
    for (const AstNode* child : registration->children())
        if (child != nullptr && child->kind() == AstNodeKind::TryStatement) return child;
    return nullptr;
}

void testReturnForms(TestContext& context)
{
    ParserFixture empty("return;");
    ParserFixture value("return result;");
    AstNode* const emptyNode = parseStatement(empty);
    AstNode* const valueNode = parseStatement(value);
    context.expect(emptyNode != nullptr && emptyNode->kind() == AstNodeKind::ReturnStatement &&
            emptyNode->first() == nullptr && valueNode != nullptr &&
            valueNode->kind() == AstNodeKind::ReturnStatement && valueNode->first() != nullptr &&
            valueNode->first()->kind() == AstNodeKind::IdentifierExpression &&
            valueNode->first()->text() == "result" && empty.diagnostics().diagnostics().empty() &&
            value.diagnostics().diagnostics().empty(),
        "TP-PAR-411 empty and valued returns must differ only by their optional value child");
}

void testCapturelessDefer(TestContext& context)
{
    ParserFixture fixture("defer [] { cleanup(); }");
    AstNode* const registration = parseStatement(fixture);
    const AstNode* const lowered = loweredDefer(registration);
    context.expect(registration != nullptr && registration->kind() == AstNodeKind::BlockStatement &&
            registration->auxiliaryText() == "defer-registration" && lowered != nullptr &&
            lowered->auxiliaryText() == "defer-lowered" && lowered->first() != nullptr &&
            lowered->first()->kind() == AstNodeKind::BlockStatement && lowered->second() != nullptr &&
            lowered->second()->kind() == AstNodeKind::BlockStatement &&
            lowered->second()->children().size() == 1 &&
            lowered->second()->children()[0]->auxiliaryText() == "defer-capture-boundary" &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-412 captureless defer must lower to protected try plus a finally-style cleanup block");
}

void testDeferCaptureModes(TestContext& context)
{
    ParserFixture reference("defer [ref observed] { use(observed); }");
    ParserFixture moved("defer [move snapshot] { use(snapshot); }");
    AstNode* const refRegistration = parseStatement(reference);
    AstNode* const moveRegistration = parseStatement(moved);
    const AstNode* const refTry = loweredDefer(refRegistration);
    const AstNode* const moveTry = loweredDefer(moveRegistration);
    const AstNode* const refLocal = refTry != nullptr && refTry->second() != nullptr &&
        !refTry->second()->children().empty() ? refTry->second()->children()[0] : nullptr;
    const AstNode* const moveOuter = moveRegistration != nullptr &&
        !moveRegistration->children().empty() ? moveRegistration->children()[0] : nullptr;
    const AstNode* const moveInner = moveTry != nullptr && moveTry->second() != nullptr &&
        !moveTry->second()->children().empty() ? moveTry->second()->children()[0] : nullptr;
    context.expect(refLocal != nullptr && refLocal->text() == "observed" &&
            refLocal->auxiliaryText() == "local:ref" && moveOuter != nullptr &&
            moveOuter->kind() == AstNodeKind::VariableDeclarationStatement &&
            moveOuter->auxiliaryText() == "local:defer-value-capture" && moveInner != nullptr &&
            moveInner->text() == "snapshot" && moveInner->auxiliaryText() == "local:defer-value-capture" &&
            reference.diagnostics().diagnostics().empty() && moved.diagnostics().diagnostics().empty(),
        "TP-PAR-413 ref and move defer captures must lower to distinct local ownership modes");
}

void testMultipleCaptureOrder(TestContext& context)
{
    ParserFixture fixture("defer [ref first, move second, ref third] { use(first); }");
    AstNode* const registration = parseStatement(fixture);
    const AstNode* const lowered = loweredDefer(registration);
    const AstNode* const cleanup = lowered != nullptr ? lowered->second() : nullptr;
    context.expect(cleanup != nullptr && cleanup->children().size() == 4 &&
            cleanup->children()[0]->text() == "first" && cleanup->children()[1]->text() == "second" &&
            cleanup->children()[2]->text() == "third" &&
            cleanup->children()[3]->text() == "first,second,third" &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-414 multiple defer captures must retain source order and exact allowed-name boundary");
}

void testMultipleDeferLifoNesting(TestContext& context)
{
    ParserFixture fixture("{ defer [] { first(); } defer [] { second(); } work(); }");
    AstNode* const block = parseStatement(fixture);
    const AstNode* const outerTry = block != nullptr && block->children().size() == 1
        ? block->children()[0] : nullptr;
    const AstNode* const outerProtected = outerTry != nullptr ? outerTry->first() : nullptr;
    const AstNode* const innerTry = outerProtected != nullptr && outerProtected->children().size() == 1
        ? outerProtected->children()[0] : nullptr;
    context.expect(outerTry != nullptr && outerTry->kind() == AstNodeKind::TryStatement &&
            innerTry != nullptr && innerTry->kind() == AstNodeKind::TryStatement &&
            innerTry->first() != nullptr && innerTry->first()->children().size() == 1 &&
            innerTry->first()->children()[0]->kind() == AstNodeKind::ExpressionStatement &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-415 consecutive defer statements must nest protected regions in LIFO cleanup order");
}

void testNestedDeferScope(TestContext& context)
{
    ParserFixture fixture("{ { defer [] { inner(); } work(); } after(); }");
    AstNode* const outer = parseStatement(fixture);
    const AstNode* const innerBlock = outer != nullptr && outer->children().size() == 2
        ? outer->children()[0] : nullptr;
    context.expect(innerBlock != nullptr && innerBlock->kind() == AstNodeKind::BlockStatement &&
            innerBlock->children().size() == 1 && innerBlock->children()[0]->kind() == AstNodeKind::TryStatement &&
            outer->children()[1]->kind() == AstNodeKind::ExpressionStatement &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-416 nested defer must protect only statements in its lexical block");
}

void testMalformedDeferRecovery(TestContext& context)
{
    ParserFixture fixture("{ defer [ref] { cleanup(); } return 7; }");
    AstNode* const block = parseStatement(fixture);
    const AstNode* const tryNode = block != nullptr && !block->children().empty()
        ? block->children()[0] : nullptr;
    const AstNode* const protectedBlock = tryNode != nullptr ? tryNode->first() : nullptr;
    const AstNode* const following = protectedBlock != nullptr && !protectedBlock->children().empty()
        ? protectedBlock->children().back() : nullptr;
    context.expect(block != nullptr && fixture.diagnostics().hasErrors() && tryNode != nullptr &&
            tryNode->kind() == AstNodeKind::TryStatement && following != nullptr &&
            following->kind() == AstNodeKind::ReturnStatement &&
            fixture.parser().current().kind() == TokenKind::EndOfFile,
        "TP-PAR-417 malformed defer capture must diagnose without corrupting cleanup scope or progress");
}

void testTryCatch(TestContext& context)
{
    ParserFixture fixture("try { work(); } catch (Error problem) { recover(); }");
    AstNode* const node = parseStatement(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::TryStatement &&
            node->first() != nullptr && node->children().size() == 1 &&
            node->children()[0]->kind() == AstNodeKind::CatchClause &&
            node->children()[0]->first() != nullptr && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-418 try/catch must retain protected body and one catch clause");
}

void testCatchOrder(TestContext& context)
{
    ParserFixture fixture("try {} catch (First one) {} catch (Second two) {}", {"First", "Second"});
    AstNode* const node = parseStatement(fixture);
    context.expect(node != nullptr && node->children().size() == 2 &&
            node->children()[0]->typeSyntax() != nullptr &&
            node->children()[0]->typeSyntax()->baseName() == "First" &&
            node->children()[0]->text() == "one" && node->children()[1]->typeSyntax() != nullptr &&
            node->children()[1]->typeSyntax()->baseName() == "Second" &&
            node->children()[1]->text() == "two" && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-419 multiple catches must retain exact source order");
}

void testCatchDeclarationAndFilter(TestContext& context)
{
    ParserFixture fixture(
        "try {} catch (Error problem) when (problem != null) { handle(); }", {"Error"});
    AstNode* const node = parseStatement(fixture);
    const AstNode* const clause = node != nullptr && node->children().size() == 1
        ? node->children()[0] : nullptr;
    context.expect(clause != nullptr && clause->typeSyntax() != nullptr &&
            clause->typeSyntax()->baseName() == "Error" && clause->text() == "problem" &&
            clause->second() != nullptr && clause->second()->kind() == AstNodeKind::BinaryExpression &&
            clause->second()->tokenKind() == TokenKind::BangEqual && clause->first() != nullptr &&
            clause->first()->kind() == AstNodeKind::BlockStatement && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-420 catch must retain type, optional name, filter, and handler in separate fields");
}

void testTryFinally(TestContext& context)
{
    ParserFixture fixture("try { work(); } finally { cleanup(); }");
    AstNode* const node = parseStatement(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::TryStatement &&
            node->first() != nullptr && node->first()->kind() == AstNodeKind::BlockStatement &&
            node->second() != nullptr && node->second()->kind() == AstNodeKind::BlockStatement &&
            node->children().empty() && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-421 try/finally must store protected and finally blocks in their contractual slots");
}

void testTryRequiresHandler(TestContext& context)
{
    ParserFixture fixture("try { work(); }");
    AstNode* const node = parseStatement(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::TryStatement &&
            fixture.diagnostics().hasErrors(),
        "TP-PAR-422 try without catch or finally must diagnose an incomplete cleanup construct");
}

void testThrowExpression(TestContext& context)
{
    ParserFixture fixture("throw problem;");
    AstNode* const node = parseStatement(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::ThrowStatement &&
            node->first() != nullptr && node->first()->kind() == AstNodeKind::IdentifierExpression &&
            node->first()->text() == "problem" && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-423 throw expression must retain its value child");
}

void testRethrow(TestContext& context)
{
    ParserFixture fixture("throw;");
    AstNode* const node = parseStatement(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::ThrowStatement &&
            node->first() == nullptr && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-424 rethrow must use ThrowStatement with a null value child");
}

void testUsingStatement(TestContext& context)
{
    ParserFixture fixture("using (Resource resource = Open()) { consume(resource); }", {"Resource"});
    AstNode* const node = parseStatement(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::UsingStatement &&
            node->typeSyntax() != nullptr && node->typeSyntax()->baseName() == "Resource" &&
            node->text() == "resource" && node->first() != nullptr &&
            node->first()->kind() == AstNodeKind::CallExpression && node->second() != nullptr &&
            node->second()->kind() == AstNodeKind::BlockStatement &&
            node->second()->children().size() == 1 && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-425 using must retain one initializer evaluation and its complete body");
}

void testMalformedCleanupRecovery(TestContext& context)
{
    ParserFixture fixture("{ using (Resource resource Open()) {} return 3; }");
    AstNode* const block = parseStatement(fixture);
    const AstNode* const following = block != nullptr && !block->children().empty()
        ? block->children().back() : nullptr;
    context.expect(block != nullptr && fixture.diagnostics().hasErrors() && following != nullptr &&
            following->kind() == AstNodeKind::ReturnStatement &&
            fixture.parser().current().kind() == TokenKind::EndOfFile,
        "TP-PAR-426 malformed cleanup construct must preserve a following return statement");
}
}

void registerParserCleanupContinuationModule(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-411", testReturnForms);
    addCase(registry, "TP-PAR-412", testCapturelessDefer);
    addCase(registry, "TP-PAR-413", testDeferCaptureModes);
    addCase(registry, "TP-PAR-414", testMultipleCaptureOrder);
    addCase(registry, "TP-PAR-415", testMultipleDeferLifoNesting);
    addCase(registry, "TP-PAR-416", testNestedDeferScope);
    addCase(registry, "TP-PAR-417", testMalformedDeferRecovery);
    addCase(registry, "TP-PAR-418", testTryCatch);
    addCase(registry, "TP-PAR-419", testCatchOrder);
    addCase(registry, "TP-PAR-420", testCatchDeclarationAndFilter);
    addCase(registry, "TP-PAR-421", testTryFinally);
    addCase(registry, "TP-PAR-422", testTryRequiresHandler, TestDisposition::KnownProductDefect);
    addCase(registry, "TP-PAR-423", testThrowExpression);
    addCase(registry, "TP-PAR-424", testRethrow);
    addCase(registry, "TP-PAR-425", testUsingStatement);
    addCase(registry, "TP-PAR-426", testMalformedCleanupRecovery);
}
}
