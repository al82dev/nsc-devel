#include "nsc_suite/TestRegistry.h"

#include "nativesharp/AST/AstDumper.h"
#include "nativesharp/AST/TypeSyntax.h"

#include <sstream>
#include <string>
#include <utility>

namespace nsc::suite
{
namespace
{
using namespace nativesharp::ast;

// Dumps one isolated node to an owned byte string for an exact-output oracle.
std::string dumpNode(const AstNode& node)
{
    std::ostringstream output;
    AstDumper{}.dump(&node, output);
    return output.str();
}

// Builds a source-location-independent node fixture.
AstNode makeNode(AstNodeKind kind = AstNodeKind::IdentifierExpression)
{
    return AstNode(kind, {});
}

// Builds a nominal type fixture used only as dumper input.
TypeSyntax makeType(std::string name)
{
    return TypeSyntax({}, nativesharp::lexer::TokenKind::Invalid, std::move(name));
}

// Registers one CA-AST-018 case with common component metadata.
void addNodeDumpCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CA-AST-018", "Host.AST", "component", "PR",
        {"ast", "dumper", "node"}, std::move(function)});
}

// Freezes the minimal one-line node representation including its final newline.
void testMinimalNode(TestContext& context)
{
    const AstNode node = makeNode(AstNodeKind::CompilationUnit);
    context.expect(dumpNode(node) == "CompilationUnit\n",
        "TA-AST-178 minimal node dump must contain only kind and newline");
}

// Freezes scalar string channel order, quoting, and lack of escaping.
void testTextAuxiliaryAndName(TestContext& context)
{
    AstNode node = makeNode();
    node.setText("text");
    node.setAuxiliaryText("aux");
    node.setArgumentName("argument");
    context.expect(dumpNode(node) ==
            "IdentifierExpression \"text\" aux=\"aux\" name=\"argument\"\n",
        "TA-AST-179 text, auxiliary text, and argument name order must be exact");
}

// Covers absent, single, and multiple generic parameter spellings.
void testGenericParameterLists(TestContext& context)
{
    AstNode empty = makeNode();
    AstNode single = makeNode();
    AstNode multiple = makeNode();
    single.addGenericParameter("T");
    multiple.addGenericParameter("T");
    multiple.addGenericParameter("U");
    multiple.addGenericParameter("V");
    context.expect(dumpNode(empty) == "IdentifierExpression\n" &&
            dumpNode(single) == "IdentifierExpression generic=<T>\n" &&
            dumpNode(multiple) == "IdentifierExpression generic=<T,U,V>\n",
        "TA-AST-180 generic parameter angle list and commas must be exact");
}

// Covers lifetime parameter list spelling and declaration order.
void testLifetimeParameterLists(TestContext& context)
{
    AstNode node = makeNode();
    node.addLifetimeParameter(LifetimeParameterSyntax{"a", {}});
    node.addLifetimeParameter(LifetimeParameterSyntax{"long", {}});
    context.expect(dumpNode(node) == "IdentifierExpression lifetime=<a,long>\n",
        "TA-AST-181 lifetime parameter list must preserve spelling and order");
}

// Verifies one outlives sequence is emitted for every stored record.
void testOutlivesRecords(TestContext& context)
{
    AstNode node = makeNode();
    node.addLifetimeOutlivesConstraint(LifetimeOutlivesConstraint{"a", "b", {}});
    node.addLifetimeOutlivesConstraint(LifetimeOutlivesConstraint{"b", "c", {}});
    context.expect(dumpNode(node) == "IdentifierExpression outlives=a:b outlives=b:c\n",
        "TA-AST-182 every outlives record must be emitted in storage order");
}

// Freezes the independent lifetime-name scalar channel.
void testLifetimeName(TestContext& context)
{
    AstNode node = makeNode();
    node.setLifetimeName("result");
    context.expect(dumpNode(node) == "IdentifierExpression lifetime-name=result\n",
        "TA-AST-183 lifetime name marker must be exact");
}

// Proves equality uses == without a colon and traversal continues to later clauses.
void testEqualityConstraint(TestContext& context)
{
    TypeSyntax intType = makeType("int");
    GenericConstraintClause equality;
    equality.parameterName = "T";
    equality.equalityConstraint = &intType;
    GenericConstraintClause following;
    following.parameterName = "U";
    following.requiresClass = true;
    AstNode node = makeNode();
    node.addGenericConstraint(std::move(equality));
    node.addGenericConstraint(std::move(following));
    context.expect(dumpNode(node) == "IdentifierExpression where T==int where U:class\n",
        "TA-AST-184 equality constraint must continue with subsequent clauses");
}

// Freezes the canonical order and spelling of all boolean generic capabilities.
void testEveryConstraintCapability(TestContext& context)
{
    GenericConstraintClause clause;
    clause.parameterName = "T";
    clause.requiresClass = true;
    clause.requiresValueType = true;
    clause.requiresDefaultConstructor = true;
    clause.requiresCopy = true;
    clause.requiresMove = true;
    clause.requiresTrackedLifetime = true;
    clause.requiresUnmanaged = true;
    clause.requiresNotNull = true;
    AstNode node = makeNode();
    node.addGenericConstraint(std::move(clause));
    context.expect(dumpNode(node) ==
            "IdentifierExpression where T:class,struct,new(),copy,move,lifetime,unmanaged,notnull\n",
        "TA-AST-185 capability spelling and fixed order must be exact");
}

// Combines capability and type constraints to freeze comma placement.
void testMixedConstraintList(TestContext& context)
{
    TypeSyntax base = makeType("Base");
    TypeSyntax interfaceType = makeType("IFoo");
    GenericConstraintClause clause;
    clause.parameterName = "T";
    clause.requiresClass = true;
    clause.requiresCopy = true;
    clause.typeConstraints = {&base, &interfaceType};
    AstNode node = makeNode();
    node.addGenericConstraint(std::move(clause));
    context.expect(dumpNode(node) == "IdentifierExpression where T:class,copy,Base,IFoo\n",
        "TA-AST-186 mixed constraints must use exact commas without spaces");
}

// Distinguishes the omitted Invalid default from a valid operator token.
void testTokenKindMarker(TestContext& context)
{
    AstNode invalid = makeNode();
    AstNode valid = makeNode();
    valid.setTokenKind(nativesharp::lexer::TokenKind::Plus);
    context.expect(dumpNode(invalid) == "IdentifierExpression\n" &&
            dumpNode(valid) == "IdentifierExpression op=Plus\n",
        "TA-AST-187 valid token kind must print and Invalid must be omitted");
}

// Freezes ordinary type before throws type on a callable-like node.
void testTypeThenThrows(TestContext& context)
{
    TypeSyntax result = makeType("Result");
    TypeSyntax error = makeType("Error");
    AstNode node = makeNode();
    node.setTypeSyntax(&result);
    node.setThrowsTypeSyntax(&error);
    context.expect(dumpNode(node) == "IdentifierExpression type=Result throws=Error\n",
        "TA-AST-188 type marker must precede throws marker");
}

// Checks that flag, ref, and variadic markers appear only for true values.
void testBooleanMarkers(TestContext& context)
{
    AstNode allFalse = makeNode();
    AstNode allTrue = makeNode();
    allTrue.setFlag(true);
    allTrue.setReference(true);
    allTrue.setVariadic(true);
    context.expect(dumpNode(allFalse) == "IdentifierExpression\n" &&
            dumpNode(allTrue) == "IdentifierExpression flag=true ref=true variadic=true\n",
        "TA-AST-189 boolean markers must appear only when true and in fixed order");
}

// Covers omission of inherit and exact spelling of every explicit FP mode.
void testFloatingPointModes(TestContext& context)
{
    AstNode inherit = makeNode();
    AstNode strict = makeNode();
    AstNode precise = makeNode();
    AstNode fast = makeNode();
    strict.setFloatingPointMode(FloatingPointModeSyntaxKind::Strict);
    precise.setFloatingPointMode(FloatingPointModeSyntaxKind::Precise);
    fast.setFloatingPointMode(FloatingPointModeSyntaxKind::Fast);
    context.expect(dumpNode(inherit) == "IdentifierExpression\n" &&
            dumpNode(strict) == "IdentifierExpression fp=strict\n" &&
            dumpNode(precise) == "IdentifierExpression fp=precise\n" &&
            dumpNode(fast) == "IdentifierExpression fp=fast\n",
        "TA-AST-190 floating-point mode spellings must be exact");
}

// Characterizes node channels intentionally omitted from the diagnostic representation.
void testOmittedNodeChannels(TestContext& context)
{
    AstNode baseline = makeNode();
    AstNode variant = makeNode();
    variant.setUnsafe(true);
    variant.setFfiUnwindPolicy(FfiUnwindSyntaxKind::Abort);
    variant.setIntegerValue(42);
    variant.setFloatingValue(3.5);
    variant.addModifier(nativesharp::lexer::TokenKind::KwPublic);
    context.expect(dumpNode(variant) == dumpNode(baseline),
        "TA-AST-191 omitted unsafe, unwind, numeric, and modifier channels must not alter dump");
}

// Freezes raw quote and newline bytes; AstDumper deliberately performs no escaping.
void testUnescapedStringBytes(TestContext& context)
{
    AstNode node = makeNode();
    node.setText("a\"b\nc");
    node.setAuxiliaryText("x\"y\nz");
    node.setArgumentName("m\"n\no");
    context.expect(dumpNode(node) ==
            "IdentifierExpression \"a\"b\nc\" aux=\"x\"y\nz\" name=\"m\"n\no\"\n",
        "TA-AST-192 quote and newline bytes must be emitted without additional escaping");
}

// Characterizes writes to a stream already in a hard failure state.
void testFailedOutputStream(TestContext& context)
{
    AstNode node = makeNode();
    std::ostringstream output;
    output.setstate(std::ios::badbit);
    AstDumper{}.dump(&node, output);
    context.expect(output.bad() && output.str().empty(),
        "TA-AST-193 failed ostream must remain failed and accept no output bytes");
}
}

// Adds every executable CA-AST-018 case to the consolidated host registry.
void registerAstDumperNodeModule(TestRegistry& registry)
{
    addNodeDumpCase(registry, "TA-AST-178", testMinimalNode);
    addNodeDumpCase(registry, "TA-AST-179", testTextAuxiliaryAndName);
    addNodeDumpCase(registry, "TA-AST-180", testGenericParameterLists);
    addNodeDumpCase(registry, "TA-AST-181", testLifetimeParameterLists);
    addNodeDumpCase(registry, "TA-AST-182", testOutlivesRecords);
    addNodeDumpCase(registry, "TA-AST-183", testLifetimeName);
    addNodeDumpCase(registry, "TA-AST-184", testEqualityConstraint);
    addNodeDumpCase(registry, "TA-AST-185", testEveryConstraintCapability);
    addNodeDumpCase(registry, "TA-AST-186", testMixedConstraintList);
    addNodeDumpCase(registry, "TA-AST-187", testTokenKindMarker);
    addNodeDumpCase(registry, "TA-AST-188", testTypeThenThrows);
    addNodeDumpCase(registry, "TA-AST-189", testBooleanMarkers);
    addNodeDumpCase(registry, "TA-AST-190", testFloatingPointModes);
    addNodeDumpCase(registry, "TA-AST-191", testOmittedNodeChannels);
    addNodeDumpCase(registry, "TA-AST-192", testUnescapedStringBytes);
    addNodeDumpCase(registry, "TA-AST-193", testFailedOutputStream);
}
}
