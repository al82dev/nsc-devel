#include "ParserFoundationTestSupport.h"

#include "nativesharp/Language/GenericSemantics.h"

#include <string>
#include <utility>

namespace nsc::suite
{
namespace
{
using namespace parser_support;
using namespace nativesharp::ast;
using nativesharp::diagnostics::DiagnosticId;

void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CP-PAR-009", "Host.Parser", "component", "PR",
        {"parser", "generic", "constraint"}, std::move(function)});
}

AstNode makeDeclaration()
{
    return AstNode(AstNodeKind::ClassDeclaration, {});
}

bool parseParametersAndConstraints(ParserFixture& fixture, AstNode& declaration)
{
    return fixture.parser().parseGenericParameterList(&declaration) &&
        fixture.parser().parseGenericConstraintClauses(&declaration);
}

void testClassConstraint(TestContext& context)
{
    ParserFixture fixture("<T> where T : class");
    AstNode declaration = makeDeclaration();
    const bool parsed = parseParametersAndConstraints(fixture, declaration);
    const auto& clauses = declaration.genericConstraints();
    context.expect(parsed && clauses.size() == 1 && clauses[0].parameterName == "T" &&
            clauses[0].requiresClass && clauses[0].typeConstraints.empty() &&
            fixture.parser().current().kind() == TokenKind::EndOfFile &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-080 class constraint must map to its contextual flag and exact target");
}

void testCapabilityConstraintFlags(TestContext& context)
{
    ParserFixture fixture("<T> where T : struct, new(), copy, move, lifetime, unmanaged, notnull");
    AstNode declaration = makeDeclaration();
    const bool parsed = parseParametersAndConstraints(fixture, declaration);
    const auto& clauses = declaration.genericConstraints();
    context.expect(parsed && clauses.size() == 1 && clauses[0].requiresValueType &&
            clauses[0].requiresDefaultConstructor && clauses[0].requiresCopy &&
            clauses[0].requiresMove && clauses[0].requiresTrackedLifetime &&
            clauses[0].requiresUnmanaged && clauses[0].requiresNotNull &&
            clauses[0].typeConstraints.empty() && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-081 every capability spelling must set only its dedicated constraint flag");
}

void testOrderedNominalConstraints(TestContext& context)
{
    ParserFixture fixture("<T> where T : Base, IFoo");
    AstNode declaration = makeDeclaration();
    const bool parsed = parseParametersAndConstraints(fixture, declaration);
    const auto& clauses = declaration.genericConstraints();
    context.expect(parsed && clauses.size() == 1 && clauses[0].typeConstraints.size() == 2 &&
            clauses[0].typeConstraints[0]->baseName() == "Base" &&
            clauses[0].typeConstraints[1]->baseName() == "IFoo" &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-082 multiple nominal constraints must retain source order exactly");
}

void testTypeEqualityConstraint(TestContext& context)
{
    ParserFixture fixture("<T,U> where T == U");
    AstNode declaration = makeDeclaration();
    const bool parsed = parseParametersAndConstraints(fixture, declaration);
    const auto& clauses = declaration.genericConstraints();
    context.expect(parsed && clauses.size() == 1 && clauses[0].parameterName == "T" &&
            clauses[0].equalityConstraint != nullptr &&
            clauses[0].equalityConstraint->baseName() == "U" &&
            clauses[0].associatedProjection == nullptr &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-083 type equality must retain its target and equality syntax independently");
}

void testConstEqualityConstraint(TestContext& context)
{
    ParserFixture fixture("<const int N> where N == 4");
    AstNode declaration = makeDeclaration();
    const bool parsed = parseParametersAndConstraints(fixture, declaration);
    const auto& clauses = declaration.genericConstraints();
    std::string typeKey;
    std::string value;
    const bool decoded = clauses.size() == 1 && clauses[0].equalityConstraint != nullptr &&
        nativesharp::language::parseConstGenericArgumentMarker(
            clauses[0].equalityConstraint->baseName(), typeKey, value);
    context.expect(parsed && decoded && typeKey == "builtin:int" && value == "4" &&
            clauses[0].equalityConstraint->range().begin().offset() == 25 &&
            clauses[0].equalityConstraint->range().end().offset() == 26,
        "TP-PAR-084 const equality must retain canonical type/value and literal range");
}

void testAssociatedProjectionConstraint(TestContext& context)
{
    ParserFixture fixture("<T> where T.Item == int");
    AstNode declaration = makeDeclaration();
    const bool parsed = parseParametersAndConstraints(fixture, declaration);
    const auto& clauses = declaration.genericConstraints();
    context.expect(parsed && clauses.size() == 1 && clauses[0].parameterName == "T.Item" &&
            clauses[0].associatedProjection != nullptr &&
            clauses[0].associatedProjection->baseName() == "T.Item" &&
            clauses[0].equalityConstraint != nullptr &&
            clauses[0].equalityConstraint->baseName() == "int" &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-085 associated projection must retain the dotted left side and equality target");
}

void testParserDoesNotInventGenericSubstitution(TestContext& context)
{
    // substitutedArgument belongs to a cloned specialization, not to source parsing. The parser
    // preserves the equality input and leaves the later-stage carrier empty.
    ParserFixture fixture("<T,U> where T == U");
    AstNode declaration = makeDeclaration();
    const bool parsed = parseParametersAndConstraints(fixture, declaration);
    const auto& clauses = declaration.genericConstraints();
    context.expect(parsed && clauses.size() == 1 && clauses[0].equalityConstraint != nullptr &&
            clauses[0].equalityConstraint->baseName() == "U" &&
            clauses[0].substitutedArgument == nullptr,
        "TP-PAR-086 source parser must leave specialization substitution to the generic cloner");
}

void testLifetimeOutlivesConstraints(TestContext& context)
{
    ParserFixture fixture("<lifetime a,lifetime b> where a : b, static");
    AstNode declaration = makeDeclaration();
    const bool parsed = parseParametersAndConstraints(fixture, declaration);
    const auto& constraints = declaration.lifetimeOutlivesConstraints();
    context.expect(parsed && constraints.size() == 2 && constraints[0].longer == "a" &&
            constraints[0].shorter == "b" && constraints[1].longer == "a" &&
            constraints[1].shorter == "static" &&
            declaration.genericConstraints().empty() && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-087 lifetime outlives pairs must preserve exact order and spelling");
}

void testUndeclaredConstraintTarget(TestContext& context)
{
    ParserFixture fixture("<T> where U : class");
    AstNode declaration = makeDeclaration();
    const bool parsed = parseParametersAndConstraints(fixture, declaration);
    context.expect(!parsed && declaration.genericConstraints().empty() &&
            diagnosticCount(fixture.diagnostics(), DiagnosticId::UnknownIdentifier) == 1 &&
            fixture.parser().current().kind() == TokenKind::EndOfFile,
        "TP-PAR-088 undeclared constraint target must diagnose and consume the finite clause");
}

void testRepeatedNominalConstraint(TestContext& context)
{
    ParserFixture fixture("<T> where T : Base, Base");
    AstNode declaration = makeDeclaration();
    const bool parsed = parseParametersAndConstraints(fixture, declaration);
    const auto& clauses = declaration.genericConstraints();
    context.expect(!parsed && clauses.size() == 1 && clauses[0].typeConstraints.size() == 2 &&
            diagnosticCount(fixture.diagnostics(), DiagnosticId::DuplicateSymbol) == 1 &&
            fixture.parser().current().kind() == TokenKind::EndOfFile,
        "TP-PAR-089 repeated nominal constraint must remain observable and diagnose exactly once");
}

void testMissingConstraintColon(TestContext& context)
{
    ParserFixture fixture("<T> where T class");
    AstNode declaration = makeDeclaration();
    const bool parametersParsed = fixture.parser().parseGenericParameterList(&declaration);
    const bool constraintsParsed = fixture.parser().parseGenericConstraintClauses(&declaration);
    context.expect(parametersParsed && !constraintsParsed &&
            fixture.parser().current().kind() == TokenKind::KwClass &&
            diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedToken) == 1,
        "TP-PAR-090 missing constraint colon must fail without consuming the recoverable token");
}

void testMultipleWhereClauses(TestContext& context)
{
    ParserFixture fixture("<T,U> where T : class where U : copy");
    AstNode declaration = makeDeclaration();
    const bool parsed = parseParametersAndConstraints(fixture, declaration);
    const auto& clauses = declaration.genericConstraints();
    context.expect(parsed && clauses.size() == 2 && clauses[0].parameterName == "T" &&
            clauses[0].requiresClass && clauses[1].parameterName == "U" &&
            clauses[1].requiresCopy && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-091 consecutive where clauses must retain all clauses in source order");
}

AstNode* firstDeclaration(ParserFixture& fixture)
{
    AstNode* const root = fixture.parse();
    return root != nullptr && !root->children().empty() ? root->children()[0] : nullptr;
}

void testConstraintRepresentationAcrossDeclarationContexts(TestContext& context)
{
    ParserFixture classFixture("class C<T> where T : class {}");
    ParserFixture structFixture("struct S<T> where T : class {}");
    ParserFixture delegateFixture("delegate void D<T>() where T : class;");
    ParserFixture methodFixture("class Host { void M<T>() where T : class {} }");
    AstNode* const classNode = firstDeclaration(classFixture);
    AstNode* const structNode = firstDeclaration(structFixture);
    AstNode* const delegateNode = firstDeclaration(delegateFixture);
    AstNode* const hostNode = firstDeclaration(methodFixture);
    AstNode* methodNode = nullptr;
    if (hostNode != nullptr)
    {
        for (AstNode* child : hostNode->children())
        {
            if (child != nullptr && child->kind() == AstNodeKind::MethodDeclaration)
            {
                methodNode = child;
                break;
            }
        }
    }
    const auto hasClassConstraint = [](const AstNode* node)
    {
        return node != nullptr && node->genericConstraints().size() == 1 &&
            node->genericConstraints()[0].parameterName == "T" &&
            node->genericConstraints()[0].requiresClass;
    };
    context.expect(hasClassConstraint(classNode) && hasClassConstraint(structNode) &&
            hasClassConstraint(delegateNode) && hasClassConstraint(methodNode),
        "TP-PAR-092 class, struct, delegate, and method must share one constraint representation");
}

void testPackConstraintContract(TestContext& context)
{
    ParserFixture validDeclaration("<pack Ts>");
    ParserFixture invalidConstraint("<pack Ts> where Ts : class");
    AstNode valid = makeDeclaration();
    AstNode invalid = makeDeclaration();
    const bool validParsed = parseParametersAndConstraints(validDeclaration, valid);
    const bool invalidParsed = parseParametersAndConstraints(invalidConstraint, invalid);
    context.expect(validParsed && valid.genericParameterSyntaxes().size() == 1 &&
            valid.genericParameterSyntaxes()[0].kind == GenericParameterSyntaxKind::TypePack &&
            valid.genericConstraints().empty(),
        "TP-PAR-093 a type pack declaration without an unsupported clause must remain valid");
    context.expect(!invalidParsed && invalid.genericConstraints().size() == 1 &&
            invalid.genericConstraints()[0].requiresClass &&
            diagnosticCount(invalidConstraint.diagnostics(), DiagnosticId::ExpectedDeclaration) == 1,
        "TP-PAR-093 capability clause on a pack must preserve syntax and diagnose the current rejection");
}
}

void registerParserGenericConstraintContinuationModule(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-080", testClassConstraint);
    addCase(registry, "TP-PAR-081", testCapabilityConstraintFlags);
    addCase(registry, "TP-PAR-082", testOrderedNominalConstraints);
    addCase(registry, "TP-PAR-083", testTypeEqualityConstraint);
    addCase(registry, "TP-PAR-084", testConstEqualityConstraint);
    addCase(registry, "TP-PAR-085", testAssociatedProjectionConstraint);
    addCase(registry, "TP-PAR-086", testParserDoesNotInventGenericSubstitution);
    addCase(registry, "TP-PAR-087", testLifetimeOutlivesConstraints);
    addCase(registry, "TP-PAR-088", testUndeclaredConstraintTarget);
    addCase(registry, "TP-PAR-089", testRepeatedNominalConstraint);
    addCase(registry, "TP-PAR-090", testMissingConstraintColon);
    addCase(registry, "TP-PAR-091", testMultipleWhereClauses);
    addCase(registry, "TP-PAR-092", testConstraintRepresentationAcrossDeclarationContexts);
    addCase(registry, "TP-PAR-093", testPackConstraintContract);
}
}
