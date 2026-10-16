#include "ParserFoundationTestSupport.h"

#include "nativesharp/AST/AstNodeKind.h"

#include <string>
#include <utility>

namespace nsc::suite
{
namespace
{
using namespace parser_support;
using nativesharp::ast::AstNode;
using nativesharp::ast::AstNodeKind;

// Returns a direct member of the first parsed top-level type. Complexity is O(1).
AstNode* member(ParserFixture& fixture, std::size_t ordinal = 0)
{
    AstNode* root = fixture.parse();
    AstNode* type = root != nullptr && !root->children().empty() ? root->children()[0] : nullptr;
    return type != nullptr && type->children().size() > ordinal ? type->children()[ordinal] : nullptr;
}

// Finds the first child with the requested kind and optional auxiliary prefix. Complexity is O(n).
AstNode* child(AstNode* parent, AstNodeKind kind, std::string_view auxiliaryPrefix = {})
{
    if (parent == nullptr) return nullptr;
    for (AstNode* candidate : parent->children())
    {
        if (candidate != nullptr && candidate->kind() == kind &&
            (auxiliaryPrefix.empty() || candidate->auxiliaryText().starts_with(auxiliaryPrefix))) return candidate;
    }
    return nullptr;
}

// Registers one property/indexer parser case. Complexity is O(1).
void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CP-PAR-023", "Host.Parser", "component", "PR",
        {"parser", "properties", "indexers"}, std::move(function)});
}

// Verifies automatic getter synthesis and backing storage. Complexity is O(n).
void testAutoGetter(TestContext& context)
{
    ParserFixture fixture("class C { int Value { get; } }");
    AstNode* property = member(fixture);
    AstNode* backing = child(property, AstNodeKind::FieldDeclaration);
    AstNode* getter = child(property, AstNodeKind::MethodDeclaration, "property:get:");
    context.expect(property != nullptr && property->kind() == AstNodeKind::PropertyDeclaration &&
            backing != nullptr && getter != nullptr && getter->first() != nullptr,
        "TP-PAR-229 auto getter must synthesize getter body and backing field");
}

// Verifies automatic get/set expansion and the setter assignment tree. Complexity is O(n).
void testAutoGetSet(TestContext& context)
{
    ParserFixture fixture("class C { int Value { get; set; } }");
    AstNode* property = member(fixture);
    AstNode* setter = child(property, AstNodeKind::MethodDeclaration, "property:set:");
    AstNode* block = setter != nullptr ? setter->first() : nullptr;
    AstNode* statement = block != nullptr && !block->children().empty() ? block->children()[0] : nullptr;
    AstNode* assignment = statement != nullptr ? statement->first() : nullptr;
    context.expect(property != nullptr && property->children().size() == 3 && setter != nullptr &&
            setter->children().size() == 1 && setter->children()[0]->text() == "value" &&
            assignment != nullptr && assignment->kind() == AstNodeKind::AssignmentExpression,
        "TP-PAR-230 auto get/set must synthesize two accessors and one setter assignment");
}

// Verifies explicit accessor bodies do not synthesize automatic bodies or storage. Complexity is O(n).
void testExplicitProperty(TestContext& context)
{
    ParserFixture fixture("class C { int Value { get { return 1; } set { } } }");
    AstNode* property = member(fixture);
    AstNode* getter = child(property, AstNodeKind::MethodDeclaration, "property:get:");
    AstNode* setter = child(property, AstNodeKind::MethodDeclaration, "property:set:");
    context.expect(property != nullptr && child(property, AstNodeKind::FieldDeclaration) == nullptr &&
            getter != nullptr && getter->first() != nullptr && setter != nullptr && setter->first() != nullptr,
        "TP-PAR-231 explicit property must retain source bodies without automatic backing storage");
}

// Verifies expression-bodied accessors are rejected by the current block-or-semicolon grammar. Complexity is O(n).
void testExpressionAccessorRejected(TestContext& context)
{
    ParserFixture fixture("class C { int Value { get => 1; } int kept; }");
    fixture.parse();
    context.expect(diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedToken) >= 1 ||
            diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedDeclaration) >= 1,
        "TP-PAR-232 expression-bodied accessor must not be accepted outside the current grammar");
}

// Verifies a narrower accessor accessibility attaches to that accessor only. Complexity is O(n).
void testAccessorAccessibility(TestContext& context)
{
    ParserFixture fixture("class C { public int Value { get; private set; } }");
    AstNode* property = member(fixture);
    AstNode* getter = child(property, AstNodeKind::MethodDeclaration, "property:get:");
    AstNode* setter = child(property, AstNodeKind::MethodDeclaration, "property:set:");
    context.expect(getter != nullptr && getter->hasModifier(TokenKind::KwPublic) && setter != nullptr &&
            setter->hasModifier(TokenKind::KwPrivate),
        "TP-PAR-233 private setter must override public property accessibility only for the setter");
}

// Verifies malformed duplicate accessor accessibility remains a visible diagnostic. Complexity is O(n).
void testInvalidAccessorAccessibility(TestContext& context)
{
    ParserFixture fixture("class C { public int Value { private public get; } }");
    fixture.parse();
    context.expect(diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedDeclaration) >= 1,
        "TP-PAR-234 conflicting accessor accessibility must diagnose");
}

// Verifies duplicate accessor identity is diagnosed exactly. Complexity is O(n).
void testDuplicateAccessor(TestContext& context)
{
    ParserFixture fixture("class C { int Value { get; get; } }");
    fixture.parse();
    context.expect(diagnosticCount(fixture.diagnostics(), DiagnosticId::DuplicateSymbol) == 1,
        "TP-PAR-235 duplicate getter must report exactly one DuplicateSymbol");
}

// Verifies abstract property accessors remain bodyless and have no backing storage. Complexity is O(n).
void testAbstractProperty(TestContext& context)
{
    ParserFixture fixture("abstract class C { public abstract int Value { get; set; } }");
    AstNode* property = member(fixture);
    AstNode* getter = child(property, AstNodeKind::MethodDeclaration, "property:get:");
    context.expect(property != nullptr && property->hasModifier(TokenKind::KwAbstract) &&
            child(property, AstNodeKind::FieldDeclaration) == nullptr && getter != nullptr &&
            getter->first() == nullptr && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-236 abstract property must be bodyless and must not synthesize storage");
}

// Verifies concrete accessors without a semicolon or block diagnose. Complexity is O(n).
void testMissingAccessorBody(TestContext& context)
{
    ParserFixture fixture("class C { int Value { get } }");
    fixture.parse();
    context.expect(diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedToken) >= 1,
        "TP-PAR-237 concrete accessor without ';' or body must diagnose");
}

// Verifies representative property-level modifiers propagate to property and accessors. Complexity is O(n).
void testPropertyModifiers(TestContext& context)
{
    ParserFixture fixture("abstract class C { public static new int S { get; } "
                          "public virtual int V { get; } public abstract int A { get; } "
                          "public sealed override int O { get; } }");
    AstNode* root = fixture.parse();
    AstNode* type = root != nullptr && !root->children().empty() ? root->children()[0] : nullptr;
    AstNode* s = type != nullptr && !type->children().empty() ? type->children()[0] : nullptr;
    AstNode* v = type != nullptr && type->children().size() > 1 ? type->children()[1] : nullptr;
    AstNode* a = type != nullptr && type->children().size() > 2 ? type->children()[2] : nullptr;
    AstNode* o = type != nullptr && type->children().size() > 3 ? type->children()[3] : nullptr;
    context.expect(s != nullptr && s->hasModifier(TokenKind::KwStatic) && s->hasModifier(TokenKind::KwNew) &&
            v != nullptr && v->hasModifier(TokenKind::KwVirtual) && a != nullptr &&
            a->hasModifier(TokenKind::KwAbstract) && o != nullptr &&
            o->hasModifier(TokenKind::KwOverride) && o->hasModifier(TokenKind::KwSealed),
        "TP-PAR-238 property modifiers must propagate without collapsing independent flags");
}

// Verifies a ref-return property retains its exact type and marks the getter. Complexity is O(n).
void testReferencePropertyAccepted(TestContext& context)
{
    ParserFixture fixture("class C { ref int Value { get { return 1; } } }");
    AstNode* property = member(fixture);
    AstNode* getter = child(property, AstNodeKind::MethodDeclaration, "property:get:");
    context.expect(property != nullptr && property->kind() == AstNodeKind::PropertyDeclaration &&
            property->typeSyntax() != nullptr && property->typeSyntax()->baseName() == "int" &&
            getter != nullptr && getter->isReference() && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-239 ref-return property must preserve its type and mark the getter without diagnostics");
}

// Verifies one- and multi-parameter indexers retain exact accessor parameter order. Complexity is O(n).
void testIndexerParameters(TestContext& context)
{
    ParserFixture fixture("class C { int this[int i] { get { return i; } } "
                          "int this[int row, int column] { get { return row; } } }");
    AstNode* root = fixture.parse();
    AstNode* type = root != nullptr && !root->children().empty() ? root->children()[0] : nullptr;
    AstNode* one = type != nullptr && !type->children().empty() ? type->children()[0] : nullptr;
    AstNode* two = type != nullptr && type->children().size() > 1 ? type->children()[1] : nullptr;
    AstNode* oneGet = child(one, AstNodeKind::MethodDeclaration, "indexer:get:");
    AstNode* twoGet = child(two, AstNodeKind::MethodDeclaration, "indexer:get:");
    context.expect(one != nullptr && one->kind() == AstNodeKind::IndexerDeclaration && oneGet != nullptr &&
            oneGet->children().size() == 1 && oneGet->children()[0]->text() == "i",
        "TP-PAR-240 one-parameter indexer must retain its parameter");
    context.expect(twoGet != nullptr && twoGet->children().size() == 2 &&
            twoGet->children()[0]->text() == "row" && twoGet->children()[1]->text() == "column",
        "TP-PAR-240 multi-parameter indexer must retain parameter order");
}

// Verifies a reference-returning indexer and getter carry the independent reference flag. Complexity is O(n).
void testReferenceIndexer(TestContext& context)
{
    ParserFixture fixture("class C { ref int this[int i] { get { return ref i; } } }");
    AstNode* indexer = member(fixture);
    AstNode* getter = child(indexer, AstNodeKind::MethodDeclaration, "indexer:get:");
    context.expect(indexer != nullptr && indexer->isReference() && getter != nullptr && getter->isReference(),
        "TP-PAR-241 reference-return indexer must mark both declaration and getter");
}

// Verifies malformed indexer punctuation diagnoses and makes finite progress. Complexity is O(n).
void testMalformedIndexer(TestContext& context)
{
    ParserFixture missingBracket("class C { int this[int i { get { return i; } } int kept; }");
    ParserFixture missingThis("class C { int [int i] { get { return i; } } int kept; }");
    missingBracket.parse();
    missingThis.parse();
    context.expect(diagnosticCount(missingBracket.diagnostics(), DiagnosticId::ExpectedToken) >= 1,
        "TP-PAR-242 indexer missing closing bracket must diagnose");
    context.expect(diagnosticCount(missingThis.diagnostics(), DiagnosticId::ExpectedToken) >= 1 ||
            diagnosticCount(missingThis.diagnostics(), DiagnosticId::ExpectedDeclaration) >= 1,
        "TP-PAR-242 indexer missing this keyword must diagnose and terminate");
}

// Verifies owned automatic setters move while ordinary setters assign directly. Complexity is O(n).
void testOwnedAutoSetter(TestContext& context)
{
    ParserFixture fixture("class Item { } class C { owned Item Owned { get; set; } Item Plain { get; set; } }");
    AstNode* root = fixture.parse();
    AstNode* type = root != nullptr && root->children().size() > 1 ? root->children()[1] : nullptr;
    AstNode* owned = type != nullptr && !type->children().empty() ? type->children()[0] : nullptr;
    AstNode* plain = type != nullptr && type->children().size() > 1 ? type->children()[1] : nullptr;
    AstNode* ownedSetter = child(owned, AstNodeKind::MethodDeclaration, "property:set:");
    AstNode* plainSetter = child(plain, AstNodeKind::MethodDeclaration, "property:set:");
    AstNode* ownedStatement = ownedSetter->first()->children()[0];
    AstNode* plainStatement = plainSetter->first()->children()[0];
    context.expect(ownedStatement->first()->second()->kind() == AstNodeKind::MoveExpression &&
            plainStatement->first()->second()->kind() == AstNodeKind::IdentifierExpression,
        "TP-PAR-243 owned auto setter must move while non-owned setter assigns directly");
}

// Verifies compiler-generated backing names are unique and deterministic per property. Complexity is O(n).
void testBackingNames(TestContext& context)
{
    ParserFixture fixture("class C { int Left { get; } int Right { get; } }");
    AstNode* root = fixture.parse();
    AstNode* type = root != nullptr && !root->children().empty() ? root->children()[0] : nullptr;
    AstNode* first = type != nullptr && !type->children().empty() ? type->children()[0] : nullptr;
    AstNode* second = type != nullptr && type->children().size() > 1 ? type->children()[1] : nullptr;
    AstNode* left = child(first, AstNodeKind::FieldDeclaration);
    AstNode* right = child(second, AstNodeKind::FieldDeclaration);
    context.expect(left != nullptr && right != nullptr && left->text() == "__nsc_backing_Left$stage38" &&
            right->text() == "__nsc_backing_Right$stage38" && left->text() != right->text(),
        "TP-PAR-244 backing names must be deterministic and unique by property name");
}
}

// Registers TP-PAR-229..244 for properties and indexers.
void registerParserPropertyIndexerModule(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-229", testAutoGetter);
    addCase(registry, "TP-PAR-230", testAutoGetSet);
    addCase(registry, "TP-PAR-231", testExplicitProperty);
    addCase(registry, "TP-PAR-232", testExpressionAccessorRejected);
    addCase(registry, "TP-PAR-233", testAccessorAccessibility);
    addCase(registry, "TP-PAR-234", testInvalidAccessorAccessibility);
    addCase(registry, "TP-PAR-235", testDuplicateAccessor);
    addCase(registry, "TP-PAR-236", testAbstractProperty);
    addCase(registry, "TP-PAR-237", testMissingAccessorBody);
    addCase(registry, "TP-PAR-238", testPropertyModifiers);
    addCase(registry, "TP-PAR-239", testReferencePropertyAccepted);
    addCase(registry, "TP-PAR-240", testIndexerParameters);
    addCase(registry, "TP-PAR-241", testReferenceIndexer);
    addCase(registry, "TP-PAR-242", testMalformedIndexer);
    addCase(registry, "TP-PAR-243", testOwnedAutoSetter);
    addCase(registry, "TP-PAR-244", testBackingNames);
}
}
