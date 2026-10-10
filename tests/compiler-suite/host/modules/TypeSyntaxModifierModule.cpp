#include "TypeSyntaxTestSupport.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/AstNodeKind.h"

#include <array>
#include <limits>

namespace nsc::suite
{
namespace
{
using namespace nativesharp::ast;
using type_syntax_test::addCase;
using type_syntax_test::expectRange;
using type_syntax_test::range;
using type_syntax_test::type;
}

void registerTypeSyntaxModifierModule(TestRegistry& registry)
{
    addCase(registry, "TA-AST-156", "CA-AST-016", {"ast", "type-syntax", "modifier-default"},
        [](TestContext& context) {
            const TypeModifierSyntax value;
            context.expect(value.kind == TypeModifierSyntaxKind::Pointer && value.arrayCount == nullptr &&
                    value.arrayRank == 0 && value.pointerNullability == PointerNullabilitySyntaxKind::LegacyNullable &&
                    value.pointerHasProvenance && value.pointerKnownAlignment == 1 && value.pointerAddressSpace == 0,
                "TA-AST-156 modifier defaults must match the complete record contract");
            context.expect(!value.range.isValid(), "TA-AST-156 default modifier range must be invalid");
        });

    addCase(registry, "TA-AST-157", "CA-AST-016", {"ast", "type-syntax", "modifier-kind"},
        [](TestContext& context) {
            constexpr std::array kinds{TypeModifierSyntaxKind::Pointer, TypeModifierSyntaxKind::Array,
                TypeModifierSyntaxKind::DynamicArray, TypeModifierSyntaxKind::RangeArray,
                TypeModifierSyntaxKind::FlexibleArray};
            for (std::size_t index = 0; index < kinds.size(); ++index)
            {
                TypeModifierSyntax value; value.kind = kinds[index];
                context.expect(value.kind == kinds[index],
                    "TA-AST-157 each of the five modifier kinds must round-trip");
                for (std::size_t other = index + 1; other < kinds.size(); ++other)
                    context.expect(kinds[index] != kinds[other],
                        "TA-AST-157 modifier kind values must be distinct");
            }
        });

    addCase(registry, "TA-AST-158", "CA-AST-016", {"ast", "type-syntax", "array-count"},
        [](TestContext& context) {
            AstNode expression(AstNodeKind::ExpressionStatement, range(4, 5));
            TypeModifierSyntax value;
            context.expect(value.arrayCount == nullptr, "TA-AST-158 array count must default null");
            value.arrayCount = &expression;
            context.expect(value.arrayCount == &expression, "TA-AST-158 array count pointer identity must round-trip");
        });

    addCase(registry, "TA-AST-159", "CA-AST-016", {"ast", "type-syntax", "rank"},
        [](TestContext& context) {
            TypeModifierSyntax value;
            for (const std::size_t rank : {std::size_t{0}, std::size_t{1}, std::size_t{17}})
            {
                value.arrayRank = rank;
                context.expect(value.arrayRank == rank, "TA-AST-159 ranks 0, 1, and N must round-trip");
            }
        });

    addCase(registry, "TA-AST-160", "CA-AST-016", {"ast", "type-syntax", "nullability"},
        [](TestContext& context) {
            TypeModifierSyntax value;
            constexpr std::array kinds{PointerNullabilitySyntaxKind::LegacyNullable,
                PointerNullabilitySyntaxKind::Nullable, PointerNullabilitySyntaxKind::NonNull};
            for (const auto kind : kinds)
            {
                value.pointerNullability = kind;
                context.expect(value.pointerNullability == kind,
                    "TA-AST-160 every pointer nullability must round-trip");
            }
        });

    addCase(registry, "TA-AST-161", "CA-AST-016", {"ast", "type-syntax", "provenance"},
        [](TestContext& context) {
            TypeModifierSyntax value; value.pointerHasProvenance = false;
            context.expect(!value.pointerHasProvenance, "TA-AST-161 false provenance must persist");
            value.pointerHasProvenance = true;
            context.expect(value.pointerHasProvenance, "TA-AST-161 true provenance must persist");
        });

    addCase(registry, "TA-AST-162", "CA-AST-016", {"ast", "type-syntax", "alignment"},
        [](TestContext& context) {
            TypeModifierSyntax value;
            for (const std::size_t alignment : {std::size_t{1}, std::size_t{16}, std::size_t{3}, std::size_t{0}})
            {
                value.pointerKnownAlignment = alignment;
                context.expect(value.pointerKnownAlignment == alignment,
                    "TA-AST-162 alignment storage must not validate powers or zero");
            }
        });

    addCase(registry, "TA-AST-163", "CA-AST-016", {"ast", "type-syntax", "address-space"},
        [](TestContext& context) {
            TypeModifierSyntax value; value.pointerAddressSpace = 0;
            context.expect(value.pointerAddressSpace == 0, "TA-AST-163 address space zero must persist");
            value.pointerAddressSpace = std::numeric_limits<unsigned>::max();
            context.expect(value.pointerAddressSpace == std::numeric_limits<unsigned>::max(),
                "TA-AST-163 maximum address space must persist");
        });

    addCase(registry, "TA-AST-164", "CA-AST-016", {"ast", "type-syntax", "range"},
        [](TestContext& context) {
            TypeModifierSyntax value; const auto expected = range(11, 19, 8202); value.range = expected;
            expectRange(context, value.range, expected, "TA-AST-164");
        });

    addCase(registry, "TA-AST-165", "CA-AST-016", {"ast", "type-syntax", "order"},
        [](TestContext& context) {
            auto value = type();
            for (const auto kind : {TypeModifierSyntaxKind::Pointer, TypeModifierSyntaxKind::Array,
                     TypeModifierSyntaxKind::FlexibleArray})
            {
                TypeModifierSyntax modifier; modifier.kind = kind; value.addModifier(modifier);
            }
            context.expect(value.modifiers().size() == 3 &&
                    value.modifiers()[0].kind == TypeModifierSyntaxKind::Pointer &&
                    value.modifiers()[1].kind == TypeModifierSyntaxKind::Array &&
                    value.modifiers()[2].kind == TypeModifierSyntaxKind::FlexibleArray,
                "TA-AST-165 mixed modifiers must preserve insertion order");
        });

    addCase(registry, "TA-AST-166", "CA-AST-016", {"ast", "type-syntax", "duplicates"},
        [](TestContext& context) {
            auto value = type(); TypeModifierSyntax duplicate; duplicate.kind = TypeModifierSyntaxKind::Array;
            value.addModifier(duplicate); value.addModifier(duplicate);
            context.expect(value.modifiers().size() == 2 &&
                    value.modifiers()[0].kind == TypeModifierSyntaxKind::Array &&
                    value.modifiers()[1].kind == TypeModifierSyntaxKind::Array,
                "TA-AST-166 duplicate modifier records must remain present");
        });

    addCase(registry, "TA-AST-167", "CA-AST-016", {"ast", "type-syntax", "no-validation"},
        [](TestContext& context) {
            TypeModifierSyntax value; value.kind = TypeModifierSyntaxKind::Array;
            value.pointerNullability = PointerNullabilitySyntaxKind::NonNull;
            value.pointerHasProvenance = false; value.pointerKnownAlignment = 32; value.pointerAddressSpace = 7;
            context.expect(value.kind == TypeModifierSyntaxKind::Array &&
                    value.pointerNullability == PointerNullabilitySyntaxKind::NonNull &&
                    !value.pointerHasProvenance && value.pointerKnownAlignment == 32 && value.pointerAddressSpace == 7,
                "TA-AST-167 pointer-only fields must survive on an incongruent array record");
        });

    addCase(registry, "TA-AST-168", "CA-AST-016", {"ast", "type-syntax", "non-owning"},
        [](TestContext& context) {
            AstNode externalExpression(AstNodeKind::ExpressionStatement, range(40, 41, 8203));
            TypeModifierSyntax value; value.arrayCount = &externalExpression;
            context.expect(value.arrayCount == &externalExpression,
                "TA-AST-168 array count from another owner must remain the same non-owning pointer");
        });

    addCase(registry, "TA-AST-169", "CA-AST-016", {"ast", "type-syntax", "many-modifiers"},
        [](TestContext& context) {
            auto value = type();
            constexpr std::size_t count = 4096;
            for (std::size_t index = 0; index < count; ++index)
            {
                TypeModifierSyntax modifier; modifier.kind = static_cast<TypeModifierSyntaxKind>(index % 5);
                modifier.pointerKnownAlignment = index + 1; value.addModifier(modifier);
            }
            context.expect(value.modifiers().size() == count &&
                    value.modifiers().front().pointerKnownAlignment == 1 &&
                    value.modifiers().back().pointerKnownAlignment == count,
                "TA-AST-169 large modifier vector must retain every record and endpoint payload");
        }, "NIGHTLY");
}
}
