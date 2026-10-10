#include "ParserFoundationTestSupport.h"

#include "nativesharp/Language/GenericSemantics.h"

#include <string>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace parser_support;
using namespace nativesharp::ast;
using nativesharp::diagnostics::DiagnosticId;

void addCase(
    TestRegistry& registry,
    std::string id,
    TestFunction function,
    TestDisposition disposition = TestDisposition::Required)
{
    registry.add(TestCase{std::move(id), "CP-PAR-010", "Host.Parser", "component", "PR",
        {"parser", "generic", "argument"}, std::move(function), disposition});
}

void testSingleTypeArgument(TestContext& context)
{
    ParserFixture fixture("<T>");
    std::vector<TypeSyntax*> arguments;
    const bool parsed = fixture.parser().parseTypeArgumentList(arguments);
    context.expect(parsed && arguments.size() == 1 && arguments[0]->baseName() == "T" &&
            fixture.parser().pendingGenericGreater_ == 0 &&
            fixture.parser().current().kind() == TokenKind::EndOfFile &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-094 one generic type argument must be consumed and represented exactly");
}

void testMixedTypeConstAndPackArguments(TestContext& context)
{
    ParserFixture fixture("<T,4,Ts...>");
    std::vector<TypeSyntax*> arguments;
    const bool parsed = fixture.parser().parseTypeArgumentList(arguments);
    std::string constType;
    std::string constValue;
    const bool decoded = arguments.size() == 3 &&
        nativesharp::language::parseConstGenericArgumentMarker(
            arguments[1]->baseName(), constType, constValue);
    context.expect(parsed && arguments.size() == 3 && arguments[0]->baseName() == "T" &&
            decoded && constType == "builtin:int" && constValue == "4" &&
            arguments[2]->baseName() == "Ts" && arguments[2]->isPackExpansion() &&
            fixture.parser().current().kind() == TokenKind::EndOfFile,
        "TP-PAR-095 type, const, and pack-expansion arguments must retain class and order");
}

void testNestedPlainGreaterClosures(TestContext& context)
{
    ParserFixture fixture("<Box<int> >");
    std::vector<TypeSyntax*> arguments;
    const bool parsed = fixture.parser().parseTypeArgumentList(arguments);
    context.expect(parsed && arguments.size() == 1 && arguments[0]->baseName() == "Box" &&
            arguments[0]->typeArguments().size() == 1 &&
            arguments[0]->typeArguments()[0]->baseName() == "int" &&
            fixture.parser().pendingGenericGreater_ == 0 &&
            fixture.parser().current().kind() == TokenKind::EndOfFile,
        "TP-PAR-096 separate greater tokens must close nested and outer argument lists exactly");
}

void testGreaterGreaterLeavesOnePendingClose(TestContext& context)
{
    ParserFixture fixture("<T>>");
    std::vector<TypeSyntax*> arguments;
    const bool parsed = fixture.parser().parseTypeArgumentList(arguments);
    context.expect(parsed && arguments.size() == 1 && arguments[0]->baseName() == "T" &&
            fixture.parser().pendingGenericGreater_ == 1 &&
            fixture.parser().current().kind() == TokenKind::EndOfFile,
        "TP-PAR-097 >> must satisfy one list close and preserve one pending generic close");
}

void testGreaterGreaterEqualPreservesAssignment(TestContext& context)
{
    ParserFixture fixture("<Box<T>>=value");
    std::vector<TypeSyntax*> arguments;
    const bool parsed = fixture.parser().parseTypeArgumentList(arguments);
    context.expect(parsed && arguments.size() == 1 && arguments[0]->baseName() == "Box" &&
            arguments[0]->typeArguments().size() == 1 &&
            arguments[0]->typeArguments()[0]->baseName() == "T" &&
            fixture.parser().pendingGenericGreater_ == 0 &&
            fixture.parser().current().kind() == TokenKind::Equal,
        "TP-PAR-098 >>= must close both generic levels while preserving assignment '='");
}

void testEmptyAndTrailingCommaArgumentLists(TestContext& context)
{
    ParserFixture empty("<>");
    ParserFixture trailing("<T,>");
    std::vector<TypeSyntax*> emptyArguments;
    std::vector<TypeSyntax*> trailingArguments;
    const bool emptyParsed = empty.parser().parseTypeArgumentList(emptyArguments);
    const bool trailingParsed = trailing.parser().parseTypeArgumentList(trailingArguments);
    context.expect(emptyParsed && emptyArguments.empty() &&
            empty.parser().current().kind() == TokenKind::EndOfFile &&
            empty.diagnostics().diagnostics().empty(),
        "TP-PAR-099 an explicitly empty argument list must follow its current acceptance contract");
    context.expect(!trailingParsed && trailingArguments.size() == 1 &&
            trailing.parser().current().kind() == TokenKind::Greater &&
            diagnosticCount(trailing.diagnostics(), DiagnosticId::ExpectedType) == 1,
        "TP-PAR-099 trailing comma must fail at the closing delimiter without consuming it");
}

void testForbiddenArgumentToken(TestContext& context)
{
    ParserFixture fixture("<;>");
    std::vector<TypeSyntax*> arguments;
    const bool parsed = fixture.parser().parseTypeArgumentList(arguments);
    context.expect(!parsed && arguments.empty() &&
            fixture.parser().current().kind() == TokenKind::Semicolon &&
            fixture.parser().pendingGenericGreater_ == 0 &&
            diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedType) == 1,
        "TP-PAR-100 forbidden argument token must fail without consuming or overflowing its delimiter");
}
}

void registerParserTypeArgumentContinuationModule(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-094", testSingleTypeArgument);
    addCase(registry, "TP-PAR-095", testMixedTypeConstAndPackArguments);
    addCase(registry, "TP-PAR-096", testNestedPlainGreaterClosures);
    addCase(registry, "TP-PAR-097", testGreaterGreaterLeavesOnePendingClose);
    addCase(registry, "TP-PAR-098", testGreaterGreaterEqualPreservesAssignment,
        TestDisposition::KnownProductDefect);
    addCase(registry, "TP-PAR-099", testEmptyAndTrailingCommaArgumentLists);
    addCase(registry, "TP-PAR-100", testForbiddenArgumentToken);
}
}
