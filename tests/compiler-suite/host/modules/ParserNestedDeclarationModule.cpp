#include "ParserDeclarationTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using namespace parser_declaration_support;

void testNestedTypeKinds(TestContext& context)
{
    DeclarationFixture fixture(
        "class Outer { class C { } struct S { } interface I { } enum E { A } delegate void D(); }");
    AstNode* const outer = declaration(fixture);
    constexpr std::array expected = {
        AstNodeKind::ClassDeclaration,
        AstNodeKind::StructDeclaration,
        AstNodeKind::InterfaceDeclaration,
        AstNodeKind::EnumDeclaration,
        AstNodeKind::DelegateDeclaration};
    bool exact = outer != nullptr && outer->children().size() == expected.size();
    for (std::size_t index = 0; exact && index < expected.size(); ++index)
    {
        exact = outer->children()[index]->kind() == expected[index];
    }
    context.expect(exact,
        "TP-PAR-197 nested class/struct/interface/enum/delegate must reuse exact child kinds");
}

void testNestedAccessibility(TestContext& context)
{
    DeclarationFixture fixture(
        "class Outer { public class PublicType { } private struct PrivateType { } "
        "internal interface InternalType { } protected enum ProtectedType { A } }");
    AstNode* const outer = declaration(fixture);
    context.expect(outer != nullptr && outer->children().size() == 4 &&
            outer->children()[0]->tokenKind() == TokenKind::KwPublic &&
            outer->children()[1]->tokenKind() == TokenKind::KwPrivate &&
            outer->children()[2]->tokenKind() == TokenKind::KwInternal &&
            outer->children()[3]->tokenKind() == TokenKind::KwProtected,
        "TP-PAR-198 nested accessibility must be applied to each declaration node");
}

void testNestedPartialRules(TestContext& context)
{
    DeclarationFixture accepted("class Outer { partial class Part { } }");
    DeclarationFixture rejected("class Outer { partial struct Bad { } }");
    AstNode* const outer = declaration(accepted);
    context.expect(outer != nullptr && outer->children().size() == 1 &&
            outer->children()[0]->flag() && !rejected.diagnostics().empty(),
        "TP-PAR-199 partial must be accepted only by nested declaration kinds that support it");
}

void testNestedModifierContracts(TestContext& context)
{
    DeclarationFixture newType("class Outer { new class Bad { } }");
    DeclarationFixture staticType("class Outer { static struct Bad { } }");
    DeclarationFixture readonlyType("class Outer { readonly interface Bad { } }");
    DeclarationFixture constType("class Outer { const enum Bad { A } }");

    AstNode* const nestedNewType = childNamed(declaration(newType), "Bad");
    context.expect(nestedNewType != nullptr &&
            nestedNewType->kind() == AstNodeKind::ClassDeclaration &&
            nestedNewType->hasModifier(TokenKind::KwNew) &&
            newType.diagnostics().empty(),
        "TP-PAR-200 new on a nested class must be preserved without a diagnostic");
    context.expect(!staticType.diagnostics().empty(),
        "TP-PAR-200 static on a non-class nested type must diagnose the modifier");
    context.expect(!readonlyType.diagnostics().empty(),
        "TP-PAR-200 readonly on a non-struct nested type must diagnose the modifier");
    context.expect(!constType.diagnostics().empty(),
        "TP-PAR-200 const on a nested type must diagnose the modifier");
}
}

void registerParserNestedDeclarationModule(TestRegistry& registry)
{
    addDeclarationCase(registry, "TP-PAR-197", "CP-PAR-020", "PR", testNestedTypeKinds);
    addDeclarationCase(registry, "TP-PAR-198", "CP-PAR-020", "PR", testNestedAccessibility);
    addDeclarationCase(registry, "TP-PAR-199", "CP-PAR-020", "PR", testNestedPartialRules);
    addDeclarationCase(registry, "TP-PAR-200", "CP-PAR-020", "PR", testNestedModifierContracts);
}
}
