#include "TypeSyntaxTestSupport.h"

#include <array>
#include <string>

namespace nsc::suite
{
namespace
{
using namespace nativesharp::ast;
using type_syntax_test::addCase;
using type_syntax_test::type;
}

void registerTypeSyntaxOwnershipModule(TestRegistry& registry)
{
    addCase(registry, "TA-AST-112", "CA-AST-012", {"ast", "type-syntax", "ownership"},
        [](TestContext& context) {
            const auto value = type();
            context.expect(value.ownershipQualifier() == OwnershipQualifierSyntaxKind::None,
                "TA-AST-112 ownership must default to None");
            context.expect(ownershipQualifierSyntaxKindName(value.ownershipQualifier()) == "none",
                "TA-AST-112 None spelling must be exact");
        });

    addCase(registry, "TA-AST-113", "CA-AST-012", {"ast", "type-syntax", "ownership"},
        [](TestContext& context) {
            auto value = type();
            value.setOwnershipQualifier(OwnershipQualifierSyntaxKind::Owned);
            context.expect(value.ownershipQualifier() == OwnershipQualifierSyntaxKind::Owned &&
                    ownershipQualifierSyntaxKindName(value.ownershipQualifier()) == "owned",
                "TA-AST-113 Owned must round-trip with its exact spelling");
            value.setOwnershipQualifier(OwnershipQualifierSyntaxKind::Borrowed);
            context.expect(value.ownershipQualifier() == OwnershipQualifierSyntaxKind::Borrowed &&
                    ownershipQualifierSyntaxKindName(value.ownershipQualifier()) == "borrowed",
                "TA-AST-113 Borrowed must round-trip with its exact spelling");
        });

    addCase(registry, "TA-AST-114", "CA-AST-012", {"ast", "type-syntax", "fallback"},
        [](TestContext& context) {
            const auto unknown = static_cast<OwnershipQualifierSyntaxKind>(-1);
            context.expect(ownershipQualifierSyntaxKindName(unknown) == "none",
                "TA-AST-114 unknown ownership must use the documented none fallback");
        });

    addCase(registry, "TA-AST-115", "CA-AST-012", {"ast", "type-syntax", "lifetime"},
        [](TestContext& context) {
            auto value = type();
            context.expect(value.lifetimeName().empty(), "TA-AST-115 lifetime must start empty");
            value.setLifetimeName("arena");
            context.expect(value.lifetimeName() == "arena", "TA-AST-115 non-empty lifetime must round-trip");
            value.setLifetimeName({});
            context.expect(value.lifetimeName().empty(), "TA-AST-115 empty lifetime must round-trip");
        });

    addCase(registry, "TA-AST-116", "CA-AST-012", {"ast", "type-syntax", "dynamic"},
        [](TestContext& context) {
            auto value = type();
            context.expect(!value.isDynamicTrait(), "TA-AST-116 dynamic trait must default false");
            value.setDynamicTrait();
            context.expect(value.isDynamicTrait(), "TA-AST-116 default setter argument must enable dynamic trait");
            value.setDynamicTrait(false);
            context.expect(!value.isDynamicTrait(), "TA-AST-116 explicit false must reset dynamic trait");
        });

    addCase(registry, "TA-AST-117", "CA-AST-012", {"ast", "type-syntax", "opaque"},
        [](TestContext& context) {
            auto value = type();
            value.setDynamicTrait();
            value.setOpaqueResult();
            context.expect(value.isOpaqueResult() && value.isDynamicTrait(),
                "TA-AST-117 opaque and dynamic markers must be independent");
            value.setOpaqueResult(false);
            context.expect(!value.isOpaqueResult() && value.isDynamicTrait(),
                "TA-AST-117 clearing opaque must preserve dynamic");
        });

    addCase(registry, "TA-AST-118", "CA-AST-012", {"ast", "type-syntax", "opaque-name"},
        [](TestContext& context) {
            auto value = type();
            { std::string temporary = "DisplayTrait"; value.setOpaqueResultTraitName(std::move(temporary)); }
            context.expect(value.opaqueResultTraitName() == "DisplayTrait",
                "TA-AST-118 opaque trait name must own temporary input");
        });

    addCase(registry, "TA-AST-119", "CA-AST-012", {"ast", "type-syntax", "trait-impl"},
        [](TestContext& context) {
            auto value = type();
            value.setExplicitTraitImplementation();
            context.expect(value.isExplicitTraitImplementation() && !value.isDynamicTrait() && !value.isOpaqueResult(),
                "TA-AST-119 explicit implementation must be an independent marker");
        });

    addCase(registry, "TA-AST-120", "CA-AST-012", {"ast", "type-syntax", "const"},
        [](TestContext& context) {
            auto value = type();
            value.setPointeeConst();
            context.expect(value.isPointeeConst() && !value.isDynamicTrait(),
                "TA-AST-120 pointee const must be an independent marker");
        });

    addCase(registry, "TA-AST-121", "CA-AST-012", {"ast", "type-syntax", "orthogonal"},
        [](TestContext& context) {
            auto value = type("Original");
            TypeModifierSyntax pointer;
            value.addModifier(pointer);
            value.setOwnershipQualifier(OwnershipQualifierSyntaxKind::Borrowed);
            value.setLifetimeName("a");
            value.setDynamicTrait();
            value.setOpaqueResult();
            value.setOpaqueResultTraitName("Trait");
            value.setExplicitTraitImplementation();
            value.setPointeeConst();
            context.expect(value.baseName() == "Original" && value.modifiers().size() == 1,
                "TA-AST-121 marker combination must not alter base fields or modifiers");
            context.expect(value.isDynamicTrait() && value.isOpaqueResult() &&
                    value.isExplicitTraitImplementation() && value.isPointeeConst(),
                "TA-AST-121 all independent markers must coexist");
        });

    addCase(registry, "TA-AST-122", "CA-AST-012", {"ast", "type-syntax", "default-argument"},
        [](TestContext& context) {
            auto value = type();
            value.setDynamicTrait(); value.setOpaqueResult();
            value.setExplicitTraitImplementation(); value.setPointeeConst();
            context.expect(value.isDynamicTrait() && value.isOpaqueResult() &&
                    value.isExplicitTraitImplementation() && value.isPointeeConst(),
                "TA-AST-122 every bool setter default argument must be true");
        });

    addCase(registry, "TA-AST-123", "CA-AST-012", {"ast", "type-syntax", "independent-reset"},
        [](TestContext& context) {
            auto value = type();
            value.setDynamicTrait(); value.setOpaqueResult();
            value.setExplicitTraitImplementation(); value.setPointeeConst();
            value.setOpaqueResult(false);
            context.expect(value.isDynamicTrait() && !value.isOpaqueResult() &&
                    value.isExplicitTraitImplementation() && value.isPointeeConst(),
                "TA-AST-123 explicit false must clear only the selected bit");
        });
}
}
