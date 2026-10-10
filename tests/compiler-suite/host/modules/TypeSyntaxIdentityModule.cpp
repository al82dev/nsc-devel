#include "TypeSyntaxTestSupport.h"

#include <array>
#include <string>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace nativesharp::ast;
using nativesharp::lexer::TokenKind;
using type_syntax_test::addCase;
using type_syntax_test::expectRange;
using type_syntax_test::range;
using type_syntax_test::type;

TypeModifierSyntax modifier(TypeModifierSyntaxKind kind, std::size_t alignment)
{
    TypeModifierSyntax result;
    result.kind = kind;
    result.pointerKnownAlignment = alignment;
    return result;
}
}

void registerTypeSyntaxIdentityModule(TestRegistry& registry)
{
    addCase(registry, "TA-AST-102", "CA-AST-011", {"ast", "type-syntax", "identity"},
        [](TestContext& context) {
            const auto expectedRange = range(3, 9);
            const TypeSyntax value(expectedRange, TokenKind::KwInt, "int");
            expectRange(context, value.range(), expectedRange, "TA-AST-102");
            context.expect(value.baseTokenKind() == TokenKind::KwInt, "TA-AST-102 token kind must match");
            context.expect(value.baseName() == "int", "TA-AST-102 base name must match");
            context.expect(value.modifiers().empty() && value.typeArguments().empty(),
                "TA-AST-102 vectors must start empty");
            context.expect(value.ownershipQualifier() == OwnershipQualifierSyntaxKind::None &&
                    !value.isTuple() && !value.isFunction() && value.constExpression() == nullptr &&
                    !value.isPackExpansion() && !value.originProjectOrdinal() && !value.originNamespace(),
                "TA-AST-102 remaining channels must retain documented defaults");
        });

    addCase(registry, "TA-AST-103", "CA-AST-011", {"ast", "type-syntax", "range"},
        [](TestContext& context) {
            auto value = type();
            const auto replacement = range(10, 20, 8102);
            value.setRange(replacement);
            expectRange(context, value.range(), replacement, "TA-AST-103");
        });

    addCase(registry, "TA-AST-104", "CA-AST-011", {"ast", "type-syntax", "token"},
        [](TestContext& context) {
            auto value = type();
            value.setBaseTokenKind(TokenKind::KwUInt);
            context.expect(value.baseTokenKind() == TokenKind::KwUInt,
                "TA-AST-104 setBaseTokenKind must replace the token");
        });

    addCase(registry, "TA-AST-105", "CA-AST-011", {"ast", "type-syntax", "ownership"},
        [](TestContext& context) {
            auto value = type();
            { std::string temporary = "Owned.Name"; value.setBaseName(std::move(temporary)); }
            context.expect(value.baseName() == "Owned.Name",
                "TA-AST-105 base name must own temporary input bytes");
        });

    addCase(registry, "TA-AST-106", "CA-AST-011", {"ast", "type-syntax", "modifier"},
        [](TestContext& context) {
            auto value = type();
            value.addModifier(modifier(TypeModifierSyntaxKind::Pointer, 4));
            const auto* originalData = value.modifiers().data();
            value.prependModifiers({});
            context.expect(value.modifiers().size() == 1 && value.modifiers().data() == originalData,
                "TA-AST-106 empty prepend must be a true no-op");
        });

    addCase(registry, "TA-AST-107", "CA-AST-011", {"ast", "type-syntax", "modifier-order"},
        [](TestContext& context) {
            auto value = type();
            value.addModifier(modifier(TypeModifierSyntaxKind::DynamicArray, 3));
            value.addModifier(modifier(TypeModifierSyntaxKind::RangeArray, 4));
            value.prependModifiers({modifier(TypeModifierSyntaxKind::Pointer, 1),
                modifier(TypeModifierSyntaxKind::Array, 2)});
            constexpr std::array expected{TypeModifierSyntaxKind::Pointer, TypeModifierSyntaxKind::Array,
                TypeModifierSyntaxKind::DynamicArray, TypeModifierSyntaxKind::RangeArray};
            context.expect(value.modifiers().size() == expected.size(), "TA-AST-107 modifier count must match");
            for (std::size_t index = 0; index < expected.size() && index < value.modifiers().size(); ++index)
                context.expect(value.modifiers()[index].kind == expected[index],
                    "TA-AST-107 prepend must preserve both internal orders");
        });

    addCase(registry, "TA-AST-108", "CA-AST-011", {"ast", "type-syntax", "duplicates"},
        [](TestContext& context) {
            auto value = type();
            value.addModifier(modifier(TypeModifierSyntaxKind::Pointer, 7));
            const auto duplicate = modifier(TypeModifierSyntaxKind::Pointer, 7);
            value.prependModifiers({duplicate, duplicate});
            context.expect(value.modifiers().size() == 3,
                "TA-AST-108 prepend must retain duplicate modifier records");
            context.expect(value.modifiers()[0].pointerKnownAlignment == 7 &&
                    value.modifiers()[1].pointerKnownAlignment == 7,
                "TA-AST-108 duplicate payloads must remain exact");
        });

    addCase(registry, "TA-AST-109", "CA-AST-011", {"ast", "type-syntax", "alias"},
        [](TestContext& context) {
            auto value = type("AliasUse");
            value.addModifier(modifier(TypeModifierSyntaxKind::Pointer, 8));
            value.prependModifiers({modifier(TypeModifierSyntaxKind::Pointer, 4)});
            context.expect(value.modifiers().size() == 2 &&
                    value.modifiers()[0].pointerKnownAlignment == 4 &&
                    value.modifiers()[1].pointerKnownAlignment == 8,
                "TA-AST-109 alias pointer must precede the use-site pointer");
        });

    addCase(registry, "TA-AST-110", "CA-AST-011", {"ast", "type-syntax", "invalidation"},
        [](TestContext& context) {
            auto value = type();
            value.addModifier(modifier(TypeModifierSyntaxKind::Pointer, 1));
            const auto oldCapacity = value.modifiers().capacity();
            std::vector<TypeModifierSyntax> prefix(oldCapacity + 1);
            value.prependModifiers(prefix);
            context.expect(value.modifiers().size() > oldCapacity,
                "TA-AST-110 prepend must exceed prior capacity and therefore invalidate prior references");
        });

    addCase(registry, "TA-AST-111", "CA-AST-011", {"ast", "type-syntax", "no-validation"},
        [](TestContext& context) {
            TypeSyntax value(range(1, 3), TokenKind::KwInt, "DefinitelyNotInt");
            context.expect(value.baseTokenKind() == TokenKind::KwInt && value.baseName() == "DefinitelyNotInt",
                "TA-AST-111 storage must not force token/name coherence");
        });
}
}
