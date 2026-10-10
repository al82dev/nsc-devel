#include "ParserFoundationTestSupport.h"

#include "nativesharp/AST/AstNodeKind.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace parser_support;
using nativesharp::ast::AstNode;
using nativesharp::ast::AstNodeKind;

// Returns a top-level child without assuming successful recovery. Complexity is O(1).
AstNode* childAt(AstNode* root, std::size_t index)
{
    return root != nullptr && index < root->children().size() ? root->children()[index] : nullptr;
}

// Registers one case from the global-declaration slice. Complexity is O(1).
void addCase(TestRegistry& registry, std::string id, TestFunction function, std::string profile = "PR")
{
    registry.add(TestCase{std::move(id), "CP-PAR-014", "Host.Parser", "component",
        std::move(profile), {"parser", "global-declaration"}, std::move(function)});
}

// Verifies that independent declaration modifiers reach their dedicated AST carriers. Complexity is O(n).
void testIsolatedModifiers(TestContext& context)
{
    ParserFixture fixture(
        "public struct A {} partial class B {} abstract class C {} sealed class D {} "
        "static class E {} readonly struct F {}");
    AstNode* root = fixture.parse();
    AstNode* publicNode = childAt(root, 0);
    AstNode* partialNode = childAt(root, 1);
    AstNode* abstractNode = childAt(root, 2);
    AstNode* sealedNode = childAt(root, 3);
    AstNode* staticNode = childAt(root, 4);
    AstNode* readonlyNode = childAt(root, 5);
    context.expect(publicNode != nullptr && publicNode->tokenKind() == TokenKind::KwPublic,
        "TP-PAR-128 public must set top-level accessibility");
    context.expect(partialNode != nullptr && partialNode->flag(),
        "TP-PAR-128 partial must set the class partial flag");
    context.expect(abstractNode != nullptr && abstractNode->hasModifier(TokenKind::KwAbstract),
        "TP-PAR-128 abstract must remain an AST modifier");
    context.expect(sealedNode != nullptr && sealedNode->hasModifier(TokenKind::KwSealed),
        "TP-PAR-128 sealed must remain an AST modifier");
    context.expect(staticNode != nullptr && staticNode->hasModifier(TokenKind::KwStatic),
        "TP-PAR-128 static must remain an AST modifier");
    context.expect(readonlyNode != nullptr && readonlyNode->hasModifier(TokenKind::KwReadonly),
        "TP-PAR-128 readonly must remain an AST modifier");
}

// Verifies a valid nominal modifier combination without conflating its carriers. Complexity is O(n).
void testValidModifierCombination(TestContext& context)
{
    ParserFixture fixture("public partial abstract class Combined {}");
    AstNode* node = childAt(fixture.parse(), 0);
    context.expect(node != nullptr && node->kind() == AstNodeKind::ClassDeclaration &&
            node->tokenKind() == TokenKind::KwPublic && node->flag() &&
            node->hasModifier(TokenKind::KwAbstract),
        "TP-PAR-129 public, partial, and abstract must all survive parsing");
    context.expect(fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-129 valid modifier combination must not diagnose");
}

// Verifies duplicate modifier diagnosis while retaining the declaration. Complexity is O(n).
void testDuplicateModifier(TestContext& context)
{
    ParserFixture fixture("partial partial class Repeated {}");
    AstNode* node = childAt(fixture.parse(), 0);
    context.expect(node != nullptr && node->flag(),
        "TP-PAR-130 duplicate partial must retain one effective partial flag");
    context.expect(diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedDeclaration) == 1,
        "TP-PAR-130 duplicate partial must diagnose exactly once");
}

// Verifies conflicting accessibility and its exact second-token range. Complexity is O(n).
void testConflictingAccessibility(TestContext& context)
{
    ParserFixture fixture("public internal class Conflict {}");
    AstNode* node = childAt(fixture.parse(), 0);
    const auto& diagnostics = fixture.diagnostics().diagnostics();
    const bool exactRange = diagnostics.size() == 1 && diagnostics[0].range().has_value() &&
        diagnostics[0].range()->begin() == fixture.tokens()[1].range().begin() &&
        diagnostics[0].range()->end() == fixture.tokens()[1].range().end();
    context.expect(node != nullptr && node->tokenKind() == TokenKind::KwPublic,
        "TP-PAR-131 first accessibility must remain authoritative");
    context.expect(diagnostics.size() == 1 &&
            diagnostics[0].id() == DiagnosticId::ExpectedDeclaration && exactRange,
        "TP-PAR-131 conflict must diagnose on the exact second modifier range");
}

// Verifies valid representation metadata and current invalid-value diagnosis. Complexity is O(n).
void testRepresentation(TestContext& context)
{
    ParserFixture valid("repr(C) struct Native {}");
    ParserFixture invalid("repr(other) struct Rejected {}");
    AstNode* validNode = childAt(valid.parse(), 0);
    AstNode* invalidNode = childAt(invalid.parse(), 0);
    context.expect(validNode != nullptr && validNode->auxiliaryText() == "repr=C" &&
            valid.diagnostics().diagnostics().empty(),
        "TP-PAR-132 repr(C) must retain exact metadata without diagnostics");
    context.expect(invalidNode != nullptr && invalidNode->auxiliaryText().empty() &&
            diagnosticCount(invalid.diagnostics(), DiagnosticId::ExpectedDeclaration) == 1,
        "TP-PAR-132 invalid repr value must not invent metadata and must diagnose once");
}

// Verifies exact layout integers in their canonical aggregate metadata. Complexity is O(n).
void testLayoutValues(TestContext& context)
{
    ParserFixture fixture("pack(1) align(8) size(16) struct Layout {}");
    AstNode* node = childAt(fixture.parse(), 0);
    context.expect(node != nullptr && node->auxiliaryText() == "pack=1,align=8,size=16",
        "TP-PAR-133 pack, align, and size must retain exact integer values and order");
    context.expect(fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-133 valid layout values must not diagnose");
}

// Verifies nonliteral and overflowing layout operands remain finite and recoverable. Complexity is O(n).
void testInvalidLayoutValues(TestContext& context)
{
    ParserFixture nonliteral("pack(value) struct Bad {} int after;");
    ParserFixture overflow("size(18446744073709551616) struct Huge {} int after;");
    AstNode* nonliteralRoot = nonliteral.parse();
    AstNode* overflowRoot = overflow.parse();
    context.expect(diagnosticCount(nonliteral.diagnostics(), DiagnosticId::ExpectedToken) >= 1 &&
            nonliteralRoot != nullptr && !nonliteralRoot->children().empty(),
        "TP-PAR-134 nonliteral layout value must diagnose and make finite progress");
    context.expect(diagnosticCount(overflow.diagnostics(), DiagnosticId::InvalidNumericLiteral) >= 1 &&
            overflowRoot != nullptr && !overflowRoot->children().empty(),
        "TP-PAR-134 overflowing layout value must retain lexical diagnosis and finite progress");
}

// Verifies export and every inlining policy dispatch to namespace functions. Complexity is O(n).
void testFunctionPolicies(TestContext& context)
{
    ParserFixture fixture(
        "export extern C inline int A() { return 0; } "
        "forceinline int B() { return 0; } noinline int C() { return 0; }");
    AstNode* root = fixture.parse();
    AstNode* first = childAt(root, 0);
    AstNode* second = childAt(root, 1);
    AstNode* third = childAt(root, 2);
    context.expect(first != nullptr && first->kind() == AstNodeKind::FunctionDeclaration &&
            first->hasModifier(TokenKind::KwExport) && first->hasModifier(TokenKind::KwExtern) &&
            first->hasModifier(TokenKind::KwInline),
        "TP-PAR-135 export extern C and inline must dispatch to one function with exact flags");
    context.expect(second != nullptr && second->hasModifier(TokenKind::KwForceInline) &&
            third != nullptr && third->hasModifier(TokenKind::KwNoInline),
        "TP-PAR-135 forceinline and noinline must retain distinct policies");
}

// Verifies record, copy, and borrowed aggregate routes and markers. Complexity is O(n).
void testAggregateModifiers(TestContext& context)
{
    ParserFixture fixture("copy record struct Value {} borrowed struct View {}");
    AstNode* root = fixture.parse();
    AstNode* value = childAt(root, 0);
    AstNode* view = childAt(root, 1);
    context.expect(value != nullptr && value->kind() == AstNodeKind::StructDeclaration &&
            value->auxiliaryText() == "record,copy",
        "TP-PAR-136 record struct and copy must use aggregate route with exact markers");
    context.expect(view != nullptr && view->kind() == AstNodeKind::StructDeclaration &&
            view->auxiliaryText() == "borrowedview",
        "TP-PAR-136 borrowed struct must use aggregate route with view marker");
}

// Verifies union, variant, and opaque dispatch while preserving their discriminating metadata. Complexity is O(n).
void testValueTypeFamilies(TestContext& context)
{
    ParserFixture fixture("union U {} variant V { None } copy opaque type O = int;");
    AstNode* root = fixture.parse();
    AstNode* unionNode = childAt(root, 0);
    AstNode* variantNode = childAt(root, 1);
    AstNode* opaqueNode = childAt(root, 2);
    context.expect(unionNode != nullptr && unionNode->kind() == AstNodeKind::StructDeclaration &&
            unionNode->auxiliaryText() == "union",
        "TP-PAR-137 union must dispatch to aggregate parser with union marker");
    context.expect(variantNode != nullptr && variantNode->kind() == AstNodeKind::StructDeclaration &&
            variantNode->auxiliaryText().find("variant") != std::string::npos,
        "TP-PAR-137 variant must dispatch to its generated aggregate route");
    context.expect(opaqueNode != nullptr && opaqueNode->kind() == AstNodeKind::StructDeclaration &&
            opaqueNode->auxiliaryText().find("opaque") != std::string::npos,
        "TP-PAR-137 opaque type must dispatch to its generated aggregate route");
}

// Verifies reference-type, trait, implementation, and delegate dispatch families. Complexity is O(n).
void testNominalFamilies(TestContext& context)
{
    ParserFixture fixture(
        "class C {} interface I {} trait T {} impl T for C {} delegate void D();", {"T", "C"});
    AstNode* root = fixture.parse();
    context.expect(childAt(root, 0) != nullptr && childAt(root, 0)->kind() == AstNodeKind::ClassDeclaration,
        "TP-PAR-138 class must dispatch to ClassDeclaration");
    context.expect(childAt(root, 1) != nullptr && childAt(root, 1)->kind() == AstNodeKind::InterfaceDeclaration,
        "TP-PAR-138 interface must dispatch to InterfaceDeclaration");
    context.expect(childAt(root, 2) != nullptr && childAt(root, 2)->auxiliaryText() == "ns3-trait",
        "TP-PAR-138 trait must retain its specialized interface marker");
    context.expect(childAt(root, 3) != nullptr && childAt(root, 3)->auxiliaryText() == "ns3-trait-impl",
        "TP-PAR-138 impl must retain its specialized class marker");
    context.expect(childAt(root, 4) != nullptr && childAt(root, 4)->kind() == AstNodeKind::DelegateDeclaration,
        "TP-PAR-138 delegate must dispatch to DelegateDeclaration");
}

// Verifies enum and extern-block dispatch. Complexity is O(n).
void testEnumAndExternFamilies(TestContext& context)
{
    ParserFixture fixture("enum E { A } extern C { int native(); }");
    AstNode* root = fixture.parse();
    AstNode* enumeration = childAt(root, 0);
    AstNode* external = childAt(root, 1);
    context.expect(enumeration != nullptr && enumeration->kind() == AstNodeKind::EnumDeclaration,
        "TP-PAR-139 enum must dispatch to EnumDeclaration");
    context.expect(external != nullptr && external->kind() == AstNodeKind::ExternCDeclaration &&
            external->children().size() == 1,
        "TP-PAR-139 extern C block must retain its external function child");
}

// Verifies name-following lookahead separates functions from global variables. Complexity is O(n).
void testFunctionVariableSplit(TestContext& context)
{
    ParserFixture fixture("int function() { return 0; } int variable;");
    AstNode* root = fixture.parse();
    context.expect(childAt(root, 0) != nullptr &&
            childAt(root, 0)->kind() == AstNodeKind::FunctionDeclaration,
        "TP-PAR-140 parenthesis after name must select function route");
    context.expect(childAt(root, 1) != nullptr &&
            childAt(root, 1)->kind() == AstNodeKind::GlobalVariableDeclaration,
        "TP-PAR-140 semicolon after name must select global-variable route");
}

// Verifies a meaningless modifier reports the current declaration diagnostic. Complexity is O(n).
void testMeaninglessModifier(TestContext& context)
{
    ParserFixture fixture("readonly class Invalid {}");
    AstNode* node = childAt(fixture.parse(), 0);
    context.expect(node != nullptr && node->kind() == AstNodeKind::ClassDeclaration,
        "TP-PAR-141 invalid modifier must not destroy the structurally parsed declaration");
    context.expect(diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedDeclaration) == 1,
        "TP-PAR-141 readonly class must report the current ExpectedDeclaration diagnostic");
}

// Verifies synchronization retains a valid declaration after an invalid global. Complexity is O(n).
void testRecoveryAfterInvalidGlobal(TestContext& context)
{
    ParserFixture fixture("partial struct Invalid {} int preserved;");
    AstNode* root = fixture.parse();
    AstNode* preserved = root != nullptr && !root->children().empty() ? root->children().back() : nullptr;
    context.expect(diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedDeclaration) >= 1,
        "TP-PAR-142 invalid partial global must diagnose");
    context.expect(preserved != nullptr && preserved->kind() == AstNodeKind::GlobalVariableDeclaration &&
            preserved->text() == "preserved",
        "TP-PAR-142 synchronization must retain the following valid declaration");
}

// Verifies heterogeneous global dispatch order and finite ranges in one nightly sequence. Complexity is O(n).
void testHeterogeneousSequence(TestContext& context)
{
    ParserFixture fixture(
        "struct S {} class C {} interface I {} trait T {} impl T for C {} union U {} "
        "variant V { None } opaque type O = int; delegate void D(); enum E { A } "
        "int F() { return 0; } int G; extern C { int N(); }",
        {"T", "C"});
    AstNode* root = fixture.parse();
    const std::array kinds{AstNodeKind::StructDeclaration, AstNodeKind::ClassDeclaration,
        AstNodeKind::InterfaceDeclaration, AstNodeKind::InterfaceDeclaration,
        AstNodeKind::ClassDeclaration, AstNodeKind::StructDeclaration,
        AstNodeKind::StructDeclaration, AstNodeKind::StructDeclaration,
        AstNodeKind::DelegateDeclaration, AstNodeKind::EnumDeclaration,
        AstNodeKind::FunctionDeclaration, AstNodeKind::GlobalVariableDeclaration,
        AstNodeKind::ExternCDeclaration};
    bool exact = root != nullptr && root->children().size() == kinds.size();
    for (std::size_t index = 0; exact && index < kinds.size(); ++index)
    {
        exact = root->children()[index] != nullptr && root->children()[index]->kind() == kinds[index] &&
            root->children()[index]->range().isValid();
    }
    context.expect(exact, "TP-PAR-143 all global families must retain exact source order and finite ranges");
    context.expect(fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-143 valid heterogeneous sequence must not diagnose");
}
}

// Registers TP-PAR-128..143 for global modifiers, dispatch, and recovery.
void registerParserGlobalDeclarationModule(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-128", testIsolatedModifiers);
    addCase(registry, "TP-PAR-129", testValidModifierCombination);
    addCase(registry, "TP-PAR-130", testDuplicateModifier);
    addCase(registry, "TP-PAR-131", testConflictingAccessibility);
    addCase(registry, "TP-PAR-132", testRepresentation);
    addCase(registry, "TP-PAR-133", testLayoutValues);
    addCase(registry, "TP-PAR-134", testInvalidLayoutValues);
    addCase(registry, "TP-PAR-135", testFunctionPolicies);
    addCase(registry, "TP-PAR-136", testAggregateModifiers);
    addCase(registry, "TP-PAR-137", testValueTypeFamilies);
    addCase(registry, "TP-PAR-138", testNominalFamilies);
    addCase(registry, "TP-PAR-139", testEnumAndExternFamilies);
    addCase(registry, "TP-PAR-140", testFunctionVariableSplit);
    addCase(registry, "TP-PAR-141", testMeaninglessModifier);
    addCase(registry, "TP-PAR-142", testRecoveryAfterInvalidGlobal);
    addCase(registry, "TP-PAR-143", testHeterogeneousSequence, "NIGHTLY");
}
}
