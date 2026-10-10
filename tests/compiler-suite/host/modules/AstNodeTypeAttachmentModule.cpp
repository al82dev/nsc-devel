#include "nsc_suite/TestRegistry.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/TypeSyntax.h"

#include <array>
#include <string>
#include <utility>

namespace nsc::suite
{
namespace
{
using nativesharp::ast::AstNode;
using nativesharp::ast::AstNodeKind;
using nativesharp::ast::TypeSyntax;
using nativesharp::lexer::TokenKind;

// Constructs a minimal type object whose address is used as a non-owning identity. Complexity is O(1).
TypeSyntax makeType(std::string name)
{
    return TypeSyntax({}, TokenKind::Identifier, std::move(name));
}

// Registers one modifier or attached-type case with canonical metadata. Complexity is O(1).
void addAttachmentCase(TestRegistry& registry, std::string id, std::string profile, TestFunction function)
{
    registry.add(TestCase{
        std::move(id), "CA-AST-006", "Host.AST", "unit", std::move(profile),
        {"ast", "node", "attachment"}, std::move(function),
    });
}

// Registers the empty modifier collection and negative membership query. Complexity is O(1).
void registerModifierDefaultsCase(TestRegistry& registry)
{
    addAttachmentCase(registry, "TA-AST-051", "PR", [](TestContext& context) {
        const AstNode node(AstNodeKind::CompilationUnit, {});
        context.expect(node.modifiers().empty(), "TA-AST-051 modifiers must default empty");
        context.expect(!node.hasModifier(TokenKind::KwPublic), "TA-AST-051 missing modifier must return false");
    });
}

// Registers insertion order and duplicate preservation for modifiers. Complexity is O(1).
void registerModifierAppendCase(TestRegistry& registry)
{
    addAttachmentCase(registry, "TA-AST-052", "PR", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        node.addModifier(TokenKind::KwPublic);
        node.addModifier(TokenKind::KwStatic);
        node.addModifier(TokenKind::KwPublic);
        const auto& values = node.modifiers();
        context.expect(values.size() == 3, "TA-AST-052 modifier count must include duplicates");
        context.expect(values[0] == TokenKind::KwPublic && values[1] == TokenKind::KwStatic,
            "TA-AST-052 modifier order must be exact");
        context.expect(values[2] == TokenKind::KwPublic, "TA-AST-052 duplicate modifier must be preserved");
    });
}

// Registers positive and negative linear membership results. Complexity is O(n).
void registerHasModifierCase(TestRegistry& registry)
{
    addAttachmentCase(registry, "TA-AST-053", "PR", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        node.addModifier(TokenKind::KwReadonly);
        context.expect(node.hasModifier(TokenKind::KwReadonly), "TA-AST-053 present modifier must return true");
        context.expect(!node.hasModifier(TokenKind::KwPrivate), "TA-AST-053 absent modifier must return false");
    });
}

// Registers default, assigned, and reset identity of the ordinary type slot. Complexity is O(1).
void registerTypeSlotCase(TestRegistry& registry)
{
    addAttachmentCase(registry, "TA-AST-054", "PR", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        TypeSyntax type = makeType("T");
        context.expect(node.typeSyntax() == nullptr, "TA-AST-054 ordinary type must default null");
        node.setTypeSyntax(&type);
        context.expect(node.typeSyntax() == &type, "TA-AST-054 ordinary type identity must be exact");
        node.setTypeSyntax(nullptr);
        context.expect(node.typeSyntax() == nullptr, "TA-AST-054 ordinary type must reset to null");
    });
}

// Registers independence between ordinary and throws type slots. Complexity is O(1).
void registerThrowsSlotCase(TestRegistry& registry)
{
    addAttachmentCase(registry, "TA-AST-055", "PR", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        TypeSyntax ordinary = makeType("Result");
        TypeSyntax error = makeType("Error");
        node.setTypeSyntax(&ordinary);
        node.setThrowsTypeSyntax(&error);
        context.expect(node.typeSyntax() == &ordinary, "TA-AST-055 ordinary type must remain exact");
        context.expect(node.throwsTypeSyntax() == &error, "TA-AST-055 throws type must remain exact");
    });
}

// Registers the intentional ignore-null rule for the repeated type list. Complexity is O(1).
void registerIgnoreNullTypeCase(TestRegistry& registry)
{
    addAttachmentCase(registry, "TA-AST-056", "PR", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        node.addTypeSyntax(nullptr);
        context.expect(node.typeSyntaxes().empty(), "TA-AST-056 null repeated type must be ignored");
    });
}

// Registers insertion order and duplicate identity in the repeated type list. Complexity is O(1).
void registerRepeatedTypesCase(TestRegistry& registry)
{
    addAttachmentCase(registry, "TA-AST-057", "PR", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        TypeSyntax first = makeType("A");
        TypeSyntax second = makeType("B");
        node.addTypeSyntax(&first);
        node.addTypeSyntax(&second);
        node.addTypeSyntax(&first);
        const auto& values = node.typeSyntaxes();
        context.expect(values.size() == 3, "TA-AST-057 repeated type count must include duplicates");
        context.expect(values[0] == &first && values[1] == &second && values[2] == &first,
            "TA-AST-057 repeated type order and identity must be exact");
    });
}

// Registers that clearing the vector leaves both single type slots intact. Complexity is O(n).
void registerClearTypesCase(TestRegistry& registry)
{
    addAttachmentCase(registry, "TA-AST-058", "PR", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        TypeSyntax ordinary = makeType("Result");
        TypeSyntax error = makeType("Error");
        node.setTypeSyntax(&ordinary);
        node.setThrowsTypeSyntax(&error);
        node.addTypeSyntax(&ordinary);
        node.clearTypeSyntaxes();
        context.expect(node.typeSyntaxes().empty(), "TA-AST-058 repeated type list must clear");
        context.expect(node.typeSyntax() == &ordinary, "TA-AST-058 ordinary slot must survive clear");
        context.expect(node.throwsTypeSyntax() == &error, "TA-AST-058 throws slot must survive clear");
    });
}

// Registers that a stable vector reference observes append operations that do not rebind the vector. Complexity O(1).
void registerVectorReferenceCase(TestRegistry& registry)
{
    addAttachmentCase(registry, "TA-AST-059", "PR", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        TypeSyntax first = makeType("A");
        TypeSyntax second = makeType("B");
        const auto& reference = node.typeSyntaxes();
        node.addTypeSyntax(&first);
        node.addTypeSyntax(&second);
        context.expect(&reference == &node.typeSyntaxes(), "TA-AST-059 vector object identity must remain stable");
        context.expect(reference.size() == 2 && reference[0] == &first && reference[1] == &second,
            "TA-AST-059 prior vector reference must observe later appends");
    });
}

// Registers membership at both ends of a large modifier sequence. Complexity is O(n).
void registerModifierScaleCase(TestRegistry& registry)
{
    addAttachmentCase(registry, "TA-AST-060", "NIGHTLY", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        constexpr std::array cycle{TokenKind::KwPublic, TokenKind::KwStatic, TokenKind::KwReadonly,
            TokenKind::KwPrivate};
        for (std::size_t index = 0; index < 4096; ++index) node.addModifier(cycle[index % cycle.size()]);
        node.addModifier(TokenKind::Plus);
        context.expect(node.hasModifier(TokenKind::KwPublic), "TA-AST-060 first-cycle modifier must be found");
        context.expect(node.hasModifier(TokenKind::Plus), "TA-AST-060 final modifier must be found");
        context.expect(!node.hasModifier(TokenKind::Minus), "TA-AST-060 absent modifier must not be found");
    });
}
}

// Registers CA-AST-006 modifier and attached-type contracts.
void registerAstNodeTypeAttachmentModule(TestRegistry& registry)
{
    registerModifierDefaultsCase(registry);
    registerModifierAppendCase(registry);
    registerHasModifierCase(registry);
    registerTypeSlotCase(registry);
    registerThrowsSlotCase(registry);
    registerIgnoreNullTypeCase(registry);
    registerRepeatedTypesCase(registry);
    registerClearTypesCase(registry);
    registerVectorReferenceCase(registry);
    registerModifierScaleCase(registry);
}
}
