#include "ParserDeclarationTestSupport.h"

namespace nsc::suite
{
namespace
{
using namespace parser_declaration_support;

void testMinimalOpaqueType(TestContext& context)
{
    DeclarationFixture fixture("opaque type Meters = int;");
    AstNode* const node = declaration(fixture);
    AstNode* const field = childNamed(node, "__nsc_opaque_value");
    AstNode* const from = childNamed(node, "FromRepresentation");
    AstNode* const to = childNamed(node, "ToRepresentation");
    context.expect(node != nullptr && node->kind() == AstNodeKind::StructDeclaration &&
            hasMarker(node, "opaque-type") && node->children().size() == 3 && field != nullptr &&
            field->tokenKind() == TokenKind::KwPrivate && field->typeSyntax() != nullptr &&
            field->typeSyntax()->baseName() == "int" && from != nullptr && to != nullptr,
        "TP-PAR-175 minimal opaque type must synthesize private storage and both boundary methods");
}

void testCopyOpaqueType(TestContext& context)
{
    DeclarationFixture fixture("copy opaque type Meters = int;");
    AstNode* const node = declaration(fixture);
    AstNode* const from = childNamed(node, "FromRepresentation");
    context.expect(hasMarker(node, "copy") && from != nullptr && from->first() != nullptr &&
            !from->first()->children().empty() &&
            from->first()->children().back()->kind() == AstNodeKind::ReturnStatement &&
            from->first()->children().back()->first() != nullptr &&
            from->first()->children().back()->first()->kind() == AstNodeKind::IdentifierExpression,
        "TP-PAR-176 copy opaque marker must propagate to its non-moving boundary return");
}

void testMoveOnlyOpaqueType(TestContext& context)
{
    DeclarationFixture fixture("opaque type Handle = nint;");
    AstNode* const node = declaration(fixture);
    AstNode* const from = childNamed(node, "FromRepresentation");
    AstNode* const result = from == nullptr || from->first() == nullptr || from->first()->children().empty()
        ? nullptr
        : from->first()->children().back()->first();
    context.expect(node != nullptr && !hasMarker(node, "copy") && result != nullptr &&
            result->kind() == AstNodeKind::MoveExpression,
        "TP-PAR-177 non-copy opaque type must not invent copy semantics");
}

void testComplexOpaqueRepresentation(TestContext& context)
{
    DeclarationFixture fixture("opaque type Handle = int*?;");
    AstNode* const field = childNamed(declaration(fixture), "__nsc_opaque_value");
    const auto* const type = field == nullptr ? nullptr : field->typeSyntax();
    context.expect(type != nullptr && type->baseName() == "int" && type->modifiers().size() == 1 &&
            type->modifiers()[0].kind == nativesharp::ast::TypeModifierSyntaxKind::Pointer &&
            type->modifiers()[0].pointerNullability ==
                nativesharp::ast::PointerNullabilitySyntaxKind::Nullable,
        "TP-PAR-178 complex underlying type must be preserved exactly on opaque storage");
}

void testGenericOpaqueType(TestContext& context)
{
    DeclarationFixture fixture("opaque type Handle<T> = T* where T : class;");
    AstNode* const node = declaration(fixture);
    AstNode* const from = childNamed(node, "FromRepresentation");
    context.expect(node != nullptr && node->genericParameters() == std::vector<std::string>{"T"} &&
            node->genericConstraints().size() == 1 && node->genericConstraints()[0].requiresClass &&
            from != nullptr && from->typeSyntax() != nullptr && from->typeSyntax()->baseName() == "Handle" &&
            from->typeSyntax()->typeArguments().size() == 1 &&
            from->typeSyntax()->typeArguments()[0]->baseName() == "T",
        "TP-PAR-179 generic opaque metadata and synthesized self type must remain exact");
}

void testMalformedOpaqueDeclarations(TestContext& context)
{
    DeclarationFixture missingName("opaque type = int;");
    DeclarationFixture missingType("opaque type Missing = ; int Tail;");
    DeclarationFixture missingTerminator("opaque type Open = int int Tail;");
    context.expect(!missingName.diagnostics().empty() && !missingType.diagnostics().empty() &&
            declaration(missingType, 0) != nullptr && declaration(missingType, 1) != nullptr &&
            !missingTerminator.diagnostics().empty(),
        "TP-PAR-180 missing opaque name/type/terminator must diagnose and keep global progress");
}

void testOpaqueModifierRejection(TestContext& context)
{
    DeclarationFixture fixture("readonly opaque type Frozen = int;");
    context.expect(!fixture.diagnostics().empty(),
        "TP-PAR-181 incompatible opaque modifiers must retain the current rejection");
}

void testConsecutiveOpaqueHygiene(TestContext& context)
{
    DeclarationFixture fixture("opaque type First = int; opaque type Second = long;");
    AstNode* const first = declaration(fixture);
    AstNode* const second = declaration(fixture, 1);
    AstNode* const firstFrom = childNamed(first, "FromRepresentation");
    AstNode* const secondFrom = childNamed(second, "FromRepresentation");
    context.expect(first != nullptr && second != nullptr && first != second && first->text() == "First" &&
            second->text() == "Second" && firstFrom != nullptr && secondFrom != nullptr &&
            firstFrom->typeSyntax()->baseName() == "First" &&
            secondFrom->typeSyntax()->baseName() == "Second" &&
            childNamed(first, "__nsc_opaque_value") != childNamed(second, "__nsc_opaque_value"),
        "TP-PAR-182 consecutive opaque declarations must synthesize independent hygienic trees");
}
}

void registerParserOpaqueDeclarationModule(TestRegistry& registry)
{
    addDeclarationCase(registry, "TP-PAR-175", "CP-PAR-018", "PR", testMinimalOpaqueType);
    addDeclarationCase(registry, "TP-PAR-176", "CP-PAR-018", "PR", testCopyOpaqueType);
    addDeclarationCase(registry, "TP-PAR-177", "CP-PAR-018", "PR", testMoveOnlyOpaqueType);
    addDeclarationCase(registry, "TP-PAR-178", "CP-PAR-018", "PR", testComplexOpaqueRepresentation);
    addDeclarationCase(registry, "TP-PAR-179", "CP-PAR-018", "PR", testGenericOpaqueType);
    addDeclarationCase(registry, "TP-PAR-180", "CP-PAR-018", "PR", testMalformedOpaqueDeclarations);
    addDeclarationCase(registry, "TP-PAR-181", "CP-PAR-018", "PR", testOpaqueModifierRejection);
    addDeclarationCase(registry, "TP-PAR-182", "CP-PAR-018", "PR", testConsecutiveOpaqueHygiene);
}
}
