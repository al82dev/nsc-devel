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

// Returns a top-level declaration at the requested ordinal. Complexity is O(1).
AstNode* declaration(ParserFixture& fixture, std::size_t ordinal = 0)
{
    AstNode* root = fixture.parse();
    return root != nullptr && root->children().size() > ordinal ? root->children()[ordinal] : nullptr;
}

// Returns a direct member of a parsed top-level type. Complexity is O(1).
AstNode* member(ParserFixture& fixture, std::size_t ordinal = 0)
{
    AstNode* type = declaration(fixture);
    return type != nullptr && type->children().size() > ordinal ? type->children()[ordinal] : nullptr;
}

// Registers one dispatch case while retaining its canonical contract. Complexity is O(1).
void addCase(TestRegistry& registry, std::string id, std::string contract, TestFunction function)
{
    registry.add(TestCase{std::move(id), std::move(contract), "Host.Parser", "component", "PR",
        {"parser", "members"}, std::move(function)});
}

// Verifies nested layout modifiers survive as exact aggregate metadata. Complexity is O(n).
void testNestedLayout(TestContext& context)
{
    ParserFixture fixture("class Outer { pack(1) align(8) size(16) struct Inner { int value; } }");
    AstNode* nested = member(fixture);
    context.expect(nested != nullptr && nested->kind() == AstNodeKind::StructDeclaration &&
            nested->auxiliaryText().find("pack=1") != std::string::npos &&
            nested->auxiliaryText().find("align=8") != std::string::npos &&
            nested->auxiliaryText().find("size=16") != std::string::npos,
        "TP-PAR-201 nested layout metadata must preserve pack, align, and size exactly");
}

// Verifies copy and borrowed nested structs retain both independent markers. Complexity is O(n).
void testNestedCopyBorrowed(TestContext& context)
{
    ParserFixture fixture("class Outer { borrowed copy struct View { int* data; } }");
    AstNode* nested = member(fixture);
    context.expect(nested != nullptr && nested->kind() == AstNodeKind::StructDeclaration &&
            nested->auxiliaryText().find("copy") != std::string::npos &&
            nested->auxiliaryText().find("borrowed") != std::string::npos,
        "TP-PAR-202 nested struct must retain copy and borrowed markers independently");
}

// Verifies an ordinary field token is not classified as a nested type opener. Complexity is O(1).
void testNestedLookaheadFalse(TestContext& context)
{
    ParserFixture fixture("int value;");
    context.expect(!fixture.parser().atNestedTypeDeclaration(),
        "TP-PAR-203 an ordinary type token must not open a nested declaration");
}

// Verifies malformed nested syntax diagnoses and reaches the following member. Complexity is O(n).
void testNestedRecovery(TestContext& context)
{
    ParserFixture fixture("class Outer { volatile struct Bad { int x; } int kept; }");
    AstNode* root = fixture.parse();
    AstNode* type = root != nullptr && !root->children().empty() ? root->children()[0] : nullptr;
    AstNode* kept = type != nullptr && type->children().size() > 1 ? type->children()[1] : nullptr;
    context.expect(diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedDeclaration) >= 1 &&
            kept != nullptr && kept->kind() == AstNodeKind::FieldDeclaration && kept->text() == "kept",
        "TP-PAR-204 nested declaration must leave the following member intact");
}

// Verifies the current field grammar: one declaration per semicolon with exact initializer. Complexity is O(n).
void testFields(TestContext& context)
{
    ParserFixture fixture("class C { int first = 1; int second = 2; }");
    AstNode* root = fixture.parse();
    AstNode* type = root != nullptr && !root->children().empty() ? root->children()[0] : nullptr;
    AstNode* first = type != nullptr && !type->children().empty() ? type->children()[0] : nullptr;
    AstNode* second = type != nullptr && type->children().size() > 1 ? type->children()[1] : nullptr;
    context.expect(first != nullptr && second != nullptr && first->kind() == AstNodeKind::FieldDeclaration &&
            second->kind() == AstNodeKind::FieldDeclaration && first->text() == "first" &&
            second->text() == "second" && first->first() != nullptr && second->first() != nullptr,
        "TP-PAR-205 consecutive fields must preserve types, names, and initializers");
    ParserFixture comma("class C { int first = 1, second = 2; }");
    comma.parse();
    context.expect(diagnosticCount(comma.diagnostics(), DiagnosticId::ExpectedToken) >= 1,
        "TP-PAR-205 comma-separated field declarators are outside the current member grammar");
}

// Verifies const, static, and readonly remain exact field modifiers. Complexity is O(n).
void testFieldModifiers(TestContext& context)
{
    ParserFixture fixture("class C { const int A = 1; static int B; readonly int Cc; }");
    AstNode* root = fixture.parse();
    AstNode* type = root != nullptr && !root->children().empty() ? root->children()[0] : nullptr;
    AstNode* a = type != nullptr && !type->children().empty() ? type->children()[0] : nullptr;
    AstNode* b = type != nullptr && type->children().size() > 1 ? type->children()[1] : nullptr;
    AstNode* c = type != nullptr && type->children().size() > 2 ? type->children()[2] : nullptr;
    context.expect(a != nullptr && a->hasModifier(TokenKind::KwConst) && b != nullptr &&
            b->hasModifier(TokenKind::KwStatic) && c != nullptr && c->hasModifier(TokenKind::KwReadonly),
        "TP-PAR-206 const, static, and readonly modifiers must attach to their own fields");
}

// Verifies explicit field offset is retained in member metadata. Complexity is O(n).
void testFieldOffset(TestContext& context)
{
    ParserFixture fixture("class C { at(8) long value; }");
    AstNode* field = member(fixture);
    context.expect(field != nullptr && field->auxiliaryText() == "at:8",
        "TP-PAR-207 explicit field offset must be encoded exactly");
}

// Verifies bit-field width and assignment rejection are represented independently. Complexity is O(n).
void testBitFields(TestContext& context)
{
    ParserFixture valid("class C { uint bits : 3; }");
    ParserFixture invalid("class C { uint bits : value = 3; }");
    AstNode* field = member(valid);
    invalid.parse();
    context.expect(field != nullptr && field->auxiliaryText() == "bitfield" && field->second() != nullptr &&
            field->second()->kind() == AstNodeKind::IntegerLiteralExpression && field->second()->integerValue() == 3,
        "TP-PAR-208 valid bit-field must preserve width expression and metadata");
    context.expect(diagnosticCount(invalid.diagnostics(), DiagnosticId::ExpectedDeclaration) >= 1,
        "TP-PAR-208 assignment in bit-field width must diagnose");
}

// Verifies anonymous aggregate declarations retain aggregate and carrier identities. Complexity is O(n).
void testAnonymousAggregates(TestContext& context)
{
    ParserFixture fixture("class C { pack(1) struct { int x; } s; union { int i; float f; } u; }");
    AstNode* root = fixture.parse();
    AstNode* type = root != nullptr && !root->children().empty() ? root->children()[0] : nullptr;
    AstNode* structure = type != nullptr && !type->children().empty() ? type->children()[0] : nullptr;
    AstNode* unionNode = type != nullptr && type->children().size() > 1 ? type->children()[1] : nullptr;
    context.expect(structure != nullptr && structure->kind() == AstNodeKind::StructDeclaration &&
            structure->first() != nullptr && structure->first()->text() == "s" &&
            structure->auxiliaryText().find("pack=1") != std::string::npos,
        "TP-PAR-209 anonymous struct must retain layout and carrier field");
    context.expect(unionNode != nullptr && unionNode->first() != nullptr && unionNode->first()->text() == "u" &&
            unionNode->auxiliaryText().find("anonymous-union") != std::string::npos,
        "TP-PAR-209 anonymous union must retain aggregate identity and carrier field");
}

// Verifies a member matching the containing type name selects constructor parsing. Complexity is O(n).
void testConstructorDispatch(TestContext& context)
{
    ParserFixture fixture("class C { C() { } }");
    AstNode* node = member(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::ConstructorDeclaration,
        "TP-PAR-210 containing type name followed by '(' must dispatch to constructor parsing");
}

// Verifies tilde dispatch selects destructor parsing. Complexity is O(n).
void testDestructorDispatch(TestContext& context)
{
    ParserFixture fixture("class C { ~C() { } }");
    AstNode* node = member(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::DestructorDeclaration,
        "TP-PAR-211 tilde member must dispatch to destructor parsing");
}

// Verifies the event keyword dispatches before ordinary type/member parsing. Complexity is O(n).
void testEventDispatch(TestContext& context)
{
    ParserFixture fixture("delegate void D(); class C { event D Changed; }");
    AstNode* type = declaration(fixture, 1);
    AstNode* node = type != nullptr && !type->children().empty() ? type->children()[0] : nullptr;
    context.expect(node != nullptr && node->kind() == AstNodeKind::EventDeclaration,
        "TP-PAR-212 event keyword must select event parsing");
}

// Verifies operator and conversion prefixes select specialized method routes. Complexity is O(n).
void testOperatorDispatch(TestContext& context)
{
    ParserFixture fixture("class C { public static C operator +(C a, C b) { return a; } "
                          "public static implicit operator int(C value) { return 0; } }");
    AstNode* root = fixture.parse();
    AstNode* type = root != nullptr && !root->children().empty() ? root->children()[0] : nullptr;
    AstNode* operation = type != nullptr && !type->children().empty() ? type->children()[0] : nullptr;
    AstNode* conversion = type != nullptr && type->children().size() > 1 ? type->children()[1] : nullptr;
    context.expect(operation != nullptr && operation->text() == "op_Addition" &&
            operation->auxiliaryText().starts_with("operator:"),
        "TP-PAR-213 operator must use specialized canonical route");
    context.expect(conversion != nullptr && conversion->text() == "op_Implicit" &&
            conversion->auxiliaryText() == "conversion:implicit",
        "TP-PAR-213 conversion must use specialized conversion route");
}

// Verifies braces and this-brackets disambiguate properties and indexers. Complexity is O(n).
void testPropertyIndexerDispatch(TestContext& context)
{
    ParserFixture fixture("class C { int Value { get; } int this[int i] { get { return i; } } }");
    AstNode* root = fixture.parse();
    AstNode* type = root != nullptr && !root->children().empty() ? root->children()[0] : nullptr;
    AstNode* property = type != nullptr && !type->children().empty() ? type->children()[0] : nullptr;
    AstNode* indexer = type != nullptr && type->children().size() > 1 ? type->children()[1] : nullptr;
    context.expect(property != nullptr && property->kind() == AstNodeKind::PropertyDeclaration &&
            indexer != nullptr && indexer->kind() == AstNodeKind::IndexerDeclaration,
        "TP-PAR-214 member lookahead must distinguish property and indexer routes");
}

// Verifies parenthesis after a name distinguishes method from field. Complexity is O(n).
void testMethodFieldDispatch(TestContext& context)
{
    ParserFixture fixture("class C { int Read() { return 1; } int value; }");
    AstNode* root = fixture.parse();
    AstNode* type = root != nullptr && !root->children().empty() ? root->children()[0] : nullptr;
    AstNode* method = type != nullptr && !type->children().empty() ? type->children()[0] : nullptr;
    AstNode* field = type != nullptr && type->children().size() > 1 ? type->children()[1] : nullptr;
    context.expect(method != nullptr && method->kind() == AstNodeKind::MethodDeclaration &&
            field != nullptr && field->kind() == AstNodeKind::FieldDeclaration,
        "TP-PAR-215 parenthesis after name must choose method without consuming the following field");
}

// Verifies class-member dispatch delegates nested types to the nested-type route. Complexity is O(n).
void testNestedDispatch(TestContext& context)
{
    ParserFixture fixture("class C { struct Item { int value; } }");
    AstNode* node = member(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::StructDeclaration && node->text() == "Item",
        "TP-PAR-216 nested type must use CP-PAR-020 dispatch");
}

// Verifies an extern C member block retains imported functions in source order. Complexity is O(n).
void testExternBlock(TestContext& context)
{
    ParserFixture fixture("class C { extern C { int first(int x); void second(); } }");
    AstNode* block = member(fixture);
    context.expect(block != nullptr && block->kind() == AstNodeKind::ExternCDeclaration &&
            block->children().size() == 2 && block->children()[0]->text() == "first" &&
            block->children()[1]->text() == "second",
        "TP-PAR-217 extern C block must preserve imported children and order");
}

// Verifies duplicate and conflicting modifiers produce exact declaration diagnostics. Complexity is O(n).
void testModifierConflicts(TestContext& context)
{
    ParserFixture duplicate("class C { public public int value; }");
    ParserFixture conflict("class C { public private int value; }");
    duplicate.parse();
    conflict.parse();
    context.expect(diagnosticCount(duplicate.diagnostics(), DiagnosticId::ExpectedDeclaration) >= 1 &&
            diagnosticCount(conflict.diagnostics(), DiagnosticId::ExpectedDeclaration) >= 1,
        "TP-PAR-218 duplicate and conflicting member modifiers must diagnose");
}

// Verifies unresolved names have one current parser behavior independent of the dead policy flag. Complexity is O(n).
void testUnresolvedNamedType(TestContext& context)
{
    ParserFixture allowed("Unknown allowed;");
    ParserFixture rejected("Unknown rejected;");
    AstNode* allowedNode = allowed.parser().parseClassMember("C", true, false);
    AstNode* rejectedNode = rejected.parser().parseClassMember("C", false, false);
    context.expect(allowedNode != nullptr && rejectedNode != nullptr &&
            allowedNode->kind() == AstNodeKind::FieldDeclaration &&
            rejectedNode->kind() == AstNodeKind::FieldDeclaration &&
            allowedNode->typeSyntax()->baseName() == "Unknown" &&
            rejectedNode->typeSyntax()->baseName() == "Unknown",
        "TP-PAR-219 unresolved named types are retained regardless of the currently inert policy argument");
}

// Verifies extension parsing scopes its implicit receiver to the extension body only. Complexity is O(n).
void testExtensionMember(TestContext& context)
{
    ParserFixture fixture("extension int { int Twice() { return this + this; } } int after;");
    AstNode* root = fixture.parse();
    AstNode* extension = root != nullptr && !root->children().empty() ? root->children()[0] : nullptr;
    AstNode* after = root != nullptr && root->children().size() > 1 ? root->children()[1] : nullptr;
    AstNode* function = extension != nullptr && !extension->children().empty() ? extension->children()[0] : nullptr;
    context.expect(extension != nullptr && extension->kind() == AstNodeKind::ExtensionDeclaration &&
            function != nullptr && !function->children().empty() &&
            function->children()[0]->text() == "__nsc_extension_this",
        "TP-PAR-220 extension member must receive exactly one scoped receiver");
    context.expect(after != nullptr && after->kind() == AstNodeKind::GlobalVariableDeclaration,
        "TP-PAR-220 extension context must end before the following declaration");
}

// Verifies an invalid class member diagnoses and finite recovery retains the next member. Complexity is O(n).
void testInvalidMemberRecovery(TestContext& context)
{
    ParserFixture fixture("class C { return; int kept; }");
    AstNode* type = declaration(fixture);
    AstNode* kept = type != nullptr && !type->children().empty() ? type->children().back() : nullptr;
    context.expect(diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedDeclaration) >= 1 &&
            kept != nullptr && kept->kind() == AstNodeKind::FieldDeclaration && kept->text() == "kept",
        "TP-PAR-221 invalid member must diagnose and preserve the following declaration");
}

// Verifies the enclosing right brace is consumed by its owner, never as a member. Complexity is O(n).
void testTypeClose(TestContext& context)
{
    ParserFixture fixture("class C { int value; } int after;");
    AstNode* root = fixture.parse();
    AstNode* type = root != nullptr && !root->children().empty() ? root->children()[0] : nullptr;
    AstNode* after = root != nullptr && root->children().size() > 1 ? root->children()[1] : nullptr;
    context.expect(type != nullptr && type->children().size() == 1 && after != nullptr &&
            after->kind() == AstNodeKind::GlobalVariableDeclaration && after->text() == "after",
        "TP-PAR-222 type close must remain owned by the class and preserve the following global");
}
}

// Registers TP-PAR-201..222 for nested types and class-member dispatch.
void registerParserNestedMemberDispatchModule(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-201", "CP-PAR-020", testNestedLayout);
    addCase(registry, "TP-PAR-202", "CP-PAR-020", testNestedCopyBorrowed);
    addCase(registry, "TP-PAR-203", "CP-PAR-020", testNestedLookaheadFalse);
    addCase(registry, "TP-PAR-204", "CP-PAR-020", testNestedRecovery);
    addCase(registry, "TP-PAR-205", "CP-PAR-021", testFields);
    addCase(registry, "TP-PAR-206", "CP-PAR-021", testFieldModifiers);
    addCase(registry, "TP-PAR-207", "CP-PAR-021", testFieldOffset);
    addCase(registry, "TP-PAR-208", "CP-PAR-021", testBitFields);
    addCase(registry, "TP-PAR-209", "CP-PAR-021", testAnonymousAggregates);
    addCase(registry, "TP-PAR-210", "CP-PAR-021", testConstructorDispatch);
    addCase(registry, "TP-PAR-211", "CP-PAR-021", testDestructorDispatch);
    addCase(registry, "TP-PAR-212", "CP-PAR-021", testEventDispatch);
    addCase(registry, "TP-PAR-213", "CP-PAR-021", testOperatorDispatch);
    addCase(registry, "TP-PAR-214", "CP-PAR-021", testPropertyIndexerDispatch);
    addCase(registry, "TP-PAR-215", "CP-PAR-021", testMethodFieldDispatch);
    addCase(registry, "TP-PAR-216", "CP-PAR-021", testNestedDispatch);
    addCase(registry, "TP-PAR-217", "CP-PAR-021", testExternBlock);
    addCase(registry, "TP-PAR-218", "CP-PAR-021", testModifierConflicts);
    addCase(registry, "TP-PAR-219", "CP-PAR-021", testUnresolvedNamedType);
    addCase(registry, "TP-PAR-220", "CP-PAR-021", testExtensionMember);
    addCase(registry, "TP-PAR-221", "CP-PAR-021", testInvalidMemberRecovery);
    addCase(registry, "TP-PAR-222", "CP-PAR-021", testTypeClose);
}
}
