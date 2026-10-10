#include "ParserFoundationTestSupport.h"

#include "nativesharp/AST/AstNodeKind.h"

#include <vector>

namespace nsc::suite
{
namespace
{
using namespace parser_support;
using nativesharp::ast::AstNodeKind;
using nativesharp::ast::TypeSyntax;

// Registers the finite EOF failure at the end of an unterminated type-argument list. Complexity is O(n).
void registerUnterminatedArgumentsCase(TestRegistry& registry)
{
    addParserCase(registry, "TP-PAR-101", "CP-PAR-010", "component", "PR", [](TestContext& context) {
        ParserFixture fixture("<T", {"T"});
        std::vector<TypeSyntax*> arguments;
        const bool parsed = fixture.parser().parseTypeArgumentList(arguments);
        context.expect(!parsed, "TP-PAR-101 unterminated generic arguments must fail");
        context.expect(fixture.parser().at(TokenKind::EndOfFile), "TP-PAR-101 parser must reach EOF finitely");
        context.expect(diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedToken) == 1,
            "TP-PAR-101 must emit one ExpectedToken diagnostic");
    });
}

// Registers a generic call and its explicit type and value arguments. Complexity is O(n).
void registerGenericCallCase(TestRegistry& registry)
{
    addParserCase(registry, "TP-PAR-102", "CP-PAR-011", "unit", "PR", [](TestContext& context) {
        ParserFixture fixture("f<T>(x)", {"T"});
        auto* call = fixture.parser().parsePostfixExpression();
        context.expect(call != nullptr && call->kind() == AstNodeKind::CallExpression,
            "TP-PAR-102 expression must be a call");
        if (call != nullptr)
        {
            context.expect(call->flag() && call->typeSyntaxes().size() == 1,
                "TP-PAR-102 call must retain one explicit type argument");
            context.expect(call->children().size() == 1, "TP-PAR-102 call must retain one value argument");
        }
        context.expect(fixture.diagnostics().diagnostics().empty(), "TP-PAR-102 must parse without diagnostics");
    });
}

// Registers that relational syntax is not reclassified as a generic call. Complexity is O(n).
void registerRelationalExpressionCase(TestRegistry& registry)
{
    registry.add(TestCase{"TP-PAR-103", "CP-PAR-011", "Host.Parser", "unit", "PR",
        {"parser", "generic-lookahead"}, [](TestContext& context) {
        ParserFixture fixture("a < b > (c)");
        auto* expression = fixture.parser().parseExpression();
        context.expect(expression != nullptr && expression->kind() == AstNodeKind::BinaryExpression,
            "TP-PAR-103 relational chain must remain a binary expression");
        context.expect(fixture.diagnostics().diagnostics().empty(), "TP-PAR-103 must parse without diagnostics");
    }, TestDisposition::KnownProductDefect});
}

// Registers a closed generic type used as a static member receiver. Complexity is O(n).
void registerClosedStaticReceiverCase(TestRegistry& registry)
{
    addParserCase(registry, "TP-PAR-104", "CP-PAR-011", "unit", "PR", [](TestContext& context) {
        ParserFixture fixture("Type<T>.Member", {"Type", "T"});
        auto* expression = fixture.parser().parsePostfixExpression();
        context.expect(expression != nullptr && expression->kind() == AstNodeKind::MemberExpression,
            "TP-PAR-104 closed generic receiver must produce member access");
        context.expect(fixture.diagnostics().diagnostics().empty(), "TP-PAR-104 must parse without diagnostics");
    });
}

// Registers nested generic closing before static member access. Complexity is O(n).
void registerNestedStaticReceiverCase(TestRegistry& registry)
{
    addParserCase(registry, "TP-PAR-105", "CP-PAR-011", "unit", "PR", [](TestContext& context) {
        ParserFixture fixture("Type<A<B>>.M", {"Type", "A", "B"});
        auto* expression = fixture.parser().parsePostfixExpression();
        context.expect(expression != nullptr && expression->kind() == AstNodeKind::MemberExpression,
            "TP-PAR-105 nested closed generic receiver must produce member access");
        context.expect(fixture.parser().at(TokenKind::EndOfFile), "TP-PAR-105 nested close must consume exactly");
        context.expect(fixture.diagnostics().diagnostics().empty(), "TP-PAR-105 must parse without diagnostics");
    });
}

// Registers failure and cursor stability for an unterminated generic-call lookahead. Complexity is O(n).
void registerUnclosedLookaheadCase(TestRegistry& registry)
{
    addParserCase(registry, "TP-PAR-106", "CP-PAR-011", "unit", "PR", [](TestContext& context) {
        ParserFixture fixture("<T", {"T"});
        const std::size_t before = fixture.parser().index_;
        context.expect(!fixture.parser().looksLikeGenericCallTypeArguments(),
            "TP-PAR-106 unclosed lookahead must return false");
        context.expect(fixture.parser().index_ == before, "TP-PAR-106 lookahead must not advance cursor");
    });
}

// Registers rejection of a forbidden token inside generic-call lookahead. Complexity is O(n).
void registerForbiddenLookaheadTokenCase(TestRegistry& registry)
{
    addParserCase(registry, "TP-PAR-107", "CP-PAR-011", "unit", "PR", [](TestContext& context) {
        ParserFixture fixture("<T;>(x)", {"T"});
        context.expect(!fixture.parser().looksLikeGenericCallTypeArguments(),
            "TP-PAR-107 forbidden semicolon must reject generic-call lookahead");
        context.expect(fixture.parser().index_ == 0, "TP-PAR-107 rejected lookahead must preserve cursor");
    });
}

// Registers that a generic call leaves its following postfix chain available and correctly parsed. Complexity O(n).
void registerPostfixChainCase(TestRegistry& registry)
{
    addParserCase(registry, "TP-PAR-108", "CP-PAR-011", "unit", "PR", [](TestContext& context) {
        ParserFixture fixture("f<T>(x).M(y)", {"T"});
        auto* expression = fixture.parser().parsePostfixExpression();
        context.expect(expression != nullptr && expression->kind() == AstNodeKind::CallExpression,
            "TP-PAR-108 outer postfix must remain a call");
        context.expect(fixture.parser().at(TokenKind::EndOfFile),
            "TP-PAR-108 postfix chain must consume through its final call only");
        context.expect(fixture.diagnostics().diagnostics().empty(), "TP-PAR-108 must parse without diagnostics");
    });
}

// Registers deterministic, side-effect-free repeated lookahead. Complexity is O(n).
void registerRepeatedLookaheadCase(TestRegistry& registry)
{
    addParserCase(registry, "TP-PAR-109", "CP-PAR-011", "unit", "PR", [](TestContext& context) {
        ParserFixture fixture("<T>(x)", {"T"});
        const std::size_t before = fixture.parser().index_;
        const bool first = fixture.parser().looksLikeGenericCallTypeArguments();
        const bool second = fixture.parser().looksLikeGenericCallTypeArguments();
        context.expect(first && second, "TP-PAR-109 repeated lookaheads must agree true");
        context.expect(fixture.parser().index_ == before, "TP-PAR-109 repeated lookaheads must preserve cursor");
    });
}
}

// Registers the exclusive TP-PAR-101..109 continuation of generic parsing and lookahead.
void registerParserGenericLookaheadContinuationModule(TestRegistry& registry)
{
    registerUnterminatedArgumentsCase(registry);
    registerGenericCallCase(registry);
    registerRelationalExpressionCase(registry);
    registerClosedStaticReceiverCase(registry);
    registerNestedStaticReceiverCase(registry);
    registerUnclosedLookaheadCase(registry);
    registerForbiddenLookaheadTokenCase(registry);
    registerPostfixChainCase(registry);
    registerRepeatedLookaheadCase(registry);
}
}
